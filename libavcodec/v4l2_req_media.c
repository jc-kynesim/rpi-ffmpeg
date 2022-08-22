/*
 * Copyright (C) 2018 Paul Kocialkowski <paul.kocialkowski@bootlin.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/media.h>
#include <linux/mman.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <linux/videodev2.h>

#include "v4l2_req_dmabufs.h"
#include "v4l2_req_media.h"
#include "v4l2_req_pollqueue.h"
#include "v4l2_req_utils.h"
#include "weak_link.h"


/* floor(log2(x)) */
static unsigned int log2_size(size_t x)
{
    unsigned int n = 0;

    if (x & ~0xffff) {
        n += 16;
        x >>= 16;
    }
    if (x & ~0xff) {
        n += 8;
        x >>= 8;
    }
    if (x & ~0xf) {
        n += 4;
        x >>= 4;
    }
    if (x & ~3) {
        n += 2;
        x >>= 2;
    }
    return (x & ~1) ? n + 1 : n;
}

static size_t round_up_size(const size_t x)
{
    /* Admit no size < 256 */
    const unsigned int n = x < 256 ? 8 : log2_size(x) - 1;

    return x >= (3 << n) ? 4 << n : (3 << n);
}

struct media_request;

struct media_pool {
    int fd;
    sem_t sem;
    pthread_mutex_t lock;
    struct media_request * free_reqs;
    struct pollqueue * pq;
};

struct media_request {
    struct media_request * next;
    struct media_pool * mp;
    int fd;
    struct polltask * pt;
};

static inline enum v4l2_memory
mediabufs_memory_to_v4l2(const enum mediabufs_memory m)
{
    return (enum v4l2_memory)m;
}

const char *
mediabufs_memory_name(const enum mediabufs_memory m)
{
    switch (m) {
    case MEDIABUFS_MEMORY_UNSET:
        return "Unset";
    case MEDIABUFS_MEMORY_MMAP:
        return "MMap";
    case MEDIABUFS_MEMORY_USERPTR:
        return "UserPtr";
    case MEDIABUFS_MEMORY_OVERLAY:
        return "Overlay";
    case MEDIABUFS_MEMORY_DMABUF:
        return "DMABuf";
    default:
        break;
    }
    return "Unknown";
}


static inline int do_trywait(sem_t *const sem)
{
    while (sem_trywait(sem)) {
        if (errno != EINTR)
            return -errno;
    }
    return 0;
}

static inline int do_wait(sem_t *const sem)
{
    while (sem_wait(sem)) {
        if (errno != EINTR)
            return -errno;
    }
    return 0;
}

static int request_buffers(int video_fd, unsigned int type,
                           enum mediabufs_memory memory, unsigned int buffers_count)
{
    struct v4l2_requestbuffers buffers;
    int rc;

    memset(&buffers, 0, sizeof(buffers));
    buffers.type = type;
    buffers.memory = mediabufs_memory_to_v4l2(memory);
    buffers.count = buffers_count;

    rc = ioctl(video_fd, VIDIOC_REQBUFS, &buffers);
    if (rc < 0) {
        rc = -errno;
        request_log("Unable to request %d type %d buffers: %s\n", buffers_count, type, strerror(-rc));
        return rc;
    }

    return 0;
}


static int set_stream(int video_fd, unsigned int type, bool enable)
{
    enum v4l2_buf_type buf_type = type;
    int rc;

    rc = ioctl(video_fd, enable ? VIDIOC_STREAMON : VIDIOC_STREAMOFF,
           &buf_type);
    if (rc < 0) {
        rc = -errno;
        request_log("Unable to %sable stream: %s\n",
                enable ? "en" : "dis", strerror(-rc));
        return rc;
    }

    return 0;
}



struct media_request * media_request_get(struct media_pool * const mp)
{
    struct media_request *req = NULL;

    /* Timeout handled by poll code */
    if (do_wait(&mp->sem))
        return NULL;

    pthread_mutex_lock(&mp->lock);
    req = mp->free_reqs;
    if (req) {
        mp->free_reqs = req->next;
        req->next = NULL;
    }
    pthread_mutex_unlock(&mp->lock);
    return req;
}

int media_request_fd(const struct media_request * const req)
{
    return req->fd;
}

int media_request_start(struct media_request * const req)
{
    while (ioctl(req->fd, MEDIA_REQUEST_IOC_QUEUE, NULL) == -1)
    {
        const int err = errno;
        if (err == EINTR)
            continue;
        request_log("%s: Failed to Q media: (%d) %s\n", __func__, err, strerror(err));
        return -err;
    }

    pollqueue_add_task(req->pt, 2000);
    return 0;
}

static void media_request_done(void *v, short revents)
{
    struct media_request *const req = v;
    struct media_pool *const mp = req->mp;

    /* ** Not sure what to do about timeout */

    if (ioctl(req->fd, MEDIA_REQUEST_IOC_REINIT, NULL) < 0)
        request_log("Unable to reinit media request: %s\n",
                strerror(errno));

    pthread_mutex_lock(&mp->lock);
    req->next = mp->free_reqs;
    mp->free_reqs = req;
    pthread_mutex_unlock(&mp->lock);
    sem_post(&mp->sem);
}

int media_request_abort(struct media_request ** const preq)
{
    struct media_request * const req = *preq;

    if (req == NULL)
        return 0;
    *preq = NULL;

    media_request_done(req, 0);
    return 0;
}

static void delete_req_chain(struct media_request * const chain)
{
    struct media_request * next = chain;
    while (next) {
        struct media_request * const req = next;
        next = req->next;
        if (req->pt)
            polltask_delete(&req->pt);
        if (req->fd != -1)
            close(req->fd);
        free(req);
    }
}

struct media_pool * media_pool_new(const char * const media_path,
                   struct pollqueue * const pq,
                   const unsigned int n)
{
    struct media_pool * const mp = calloc(1, sizeof(*mp));
    unsigned int i;

    if (!mp)
        goto fail0;

    mp->pq = pq;
    pthread_mutex_init(&mp->lock, NULL);
    mp->fd = open(media_path, O_RDWR | O_NONBLOCK);
    if (mp->fd == -1) {
        request_log("Failed to open '%s': %s\n", media_path, strerror(errno));
        goto fail1;
    }

    for (i = 0; i != n; ++i) {
        struct media_request * req = malloc(sizeof(*req));
        if (!req)
            goto fail4;

        *req = (struct media_request){
            .next = mp->free_reqs,
            .mp = mp,
            .fd = -1
        };
        mp->free_reqs = req;

        if (ioctl(mp->fd, MEDIA_IOC_REQUEST_ALLOC, &req->fd) == -1) {
            request_log("Failed to alloc request %d: %s\n", i, strerror(errno));
            goto fail4;
        }

        req->pt = polltask_new(pq, req->fd, POLLPRI, media_request_done, req);
        if (!req->pt)
            goto fail4;
    }

    sem_init(&mp->sem, 0, n);

    return mp;

fail4:
    delete_req_chain(mp->free_reqs);
    close(mp->fd);
    pthread_mutex_destroy(&mp->lock);
fail1:
    free(mp);
fail0:
    return NULL;
}

void media_pool_delete(struct media_pool ** pMp)
{
    struct media_pool * const mp = *pMp;

    if (!mp)
        return;
    *pMp = NULL;

    delete_req_chain(mp->free_reqs);
    close(mp->fd);
    sem_destroy(&mp->sem);
    pthread_mutex_destroy(&mp->lock);
    free(mp);
}


#define INDEX_UNSET (~(uint32_t)0)

enum qent_status {
    QENT_NEW = 0,       // Initial state - shouldn't last
    QENT_FREE,          // On free chain
    QENT_PENDING,       // User has ent
    QENT_WAITING,       // On inuse
    QENT_DONE,          // Frame rx
    QENT_ERROR,         // Error
    QENT_IMPORT
};

struct qent_base {
    atomic_int ref_count;
    struct qent_base *next;
    struct qent_base *prev;
    enum qent_status status;
    enum mediabufs_memory memtype;
    uint32_t index;
    struct dmabuf_h *dh[VIDEO_MAX_PLANES];
    struct timeval timestamp;
};

struct qent_src {
    struct qent_base base;
    int fixed_size;
};

struct qent_dst {
    struct qent_base base;
    bool waiting;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    struct ff_weak_link_client * mbc_wl;
};

struct qe_list_head {
    struct qent_base *head;
    struct qent_base *tail;
};

struct buf_pool {
    enum mediabufs_memory memtype;
    pthread_mutex_t lock;
    sem_t free_sem;
    struct qe_list_head free;
    struct qe_list_head inuse;
};


static inline struct qent_dst *base_to_dst(struct qent_base *be)
{
    return (struct qent_dst *)be;
}

static inline struct qent_src *base_to_src(struct qent_base *be)
{
    return (struct qent_src *)be;
}


#define QENT_BASE_INITIALIZER(mtype) {\
    .ref_count = ATOMIC_VAR_INIT(0),\
    .status = QENT_NEW,\
    .memtype = (mtype),\
    .index  = INDEX_UNSET\
}

static void qe_base_uninit(struct qent_base *const be)
{
    unsigned int i;
    for (i = 0; i != VIDEO_MAX_PLANES; ++i) {
        dmabuf_free(be->dh[i]);
        be->dh[i] = NULL;
    }
}

static void qe_src_free(struct qent_src *const be_src)
{
    if (!be_src)
        return;
    qe_base_uninit(&be_src->base);
    free(be_src);
}

static struct qent_src * qe_src_new(enum mediabufs_memory mtype)
{
    struct qent_src *const be_src = malloc(sizeof(*be_src));
    if (!be_src)
        return NULL;
    *be_src = (struct qent_src){
        .base = QENT_BASE_INITIALIZER(mtype)
    };
    return be_src;
}

static void qe_dst_free(struct qent_dst *const be_dst)
{
    if (!be_dst)
        return;

    ff_weak_link_unref(&be_dst->mbc_wl);
    pthread_cond_destroy(&be_dst->cond);
    pthread_mutex_destroy(&be_dst->lock);
    qe_base_uninit(&be_dst->base);
    free(be_dst);
}

static struct qent_dst* qe_dst_new(struct ff_weak_link_master * const wl, const enum mediabufs_memory memtype)
{
    struct qent_dst *const be_dst = malloc(sizeof(*be_dst));
    if (!be_dst)
        return NULL;
    *be_dst = (struct qent_dst){
        .base = QENT_BASE_INITIALIZER(memtype),
        .lock = PTHREAD_MUTEX_INITIALIZER,
        .cond = PTHREAD_COND_INITIALIZER,
        .mbc_wl = ff_weak_link_ref(wl)
    };
    return be_dst;
}

static void ql_add_tail(struct qe_list_head * const ql, struct qent_base * be)
{
    if (ql->tail)
        ql->tail->next = be;
    else
        ql->head = be;
    be->prev = ql->tail;
    be->next = NULL;
    ql->tail = be;
}

static struct qent_base * ql_extract(struct qe_list_head * const ql, struct qent_base * be)
{
    if (!be)
        return NULL;

    if (be->next)
        be->next->prev = be->prev;
    else
        ql->tail = be->prev;
    if (be->prev)
        be->prev->next = be->next;
    else
        ql->head = be->next;
    be->next = NULL;
    be->prev = NULL;
    return be;
}


static void bq_put_free(struct buf_pool *const bp, struct qent_base * be)
{
    ql_add_tail(&bp->free, be);
}

static struct qent_base * bq_get_free(struct buf_pool *const bp)
{
    return ql_extract(&bp->free, bp->free.head);
}

static struct qent_base * bq_extract_inuse(struct buf_pool *const bp, struct qent_base *const be)
{
    return ql_extract(&bp->inuse, be);
}

static struct qent_base * bq_get_inuse(struct buf_pool *const bp)
{
    return ql_extract(&bp->inuse, bp->inuse.head);
}

static void bq_free_all_free_src(struct buf_pool *const bp)
{
    struct qent_base *be;
    while ((be = bq_get_free(bp)) != NULL)
        qe_src_free(base_to_src(be));
}

static void bq_free_all_inuse_src(struct buf_pool *const bp)
{
    struct qent_base *be;
    while ((be = bq_get_inuse(bp)) != NULL)
        qe_src_free(base_to_src(be));
}

static void bq_free_all_free_dst(struct buf_pool *const bp)
{
    struct qent_base *be;
    while ((be = bq_get_free(bp)) != NULL)
        qe_dst_free(base_to_dst(be));
}

static void queue_put_free(struct buf_pool *const bp, struct qent_base *be)
{
    unsigned int i;

    pthread_mutex_lock(&bp->lock);
    /* Clear out state vars */
    be->timestamp.tv_sec = 0;
    be->timestamp.tv_usec = 0;
    be->status = QENT_FREE;
    for (i = 0; i < VIDEO_MAX_PLANES && be->dh[i]; ++i)
        dmabuf_len_set(be->dh[i], 0);
    bq_put_free(bp, be);
    pthread_mutex_unlock(&bp->lock);
    sem_post(&bp->free_sem);
}

static bool queue_is_inuse(const struct buf_pool *const bp)
{
    return bp->inuse.tail != NULL;
}

static void queue_put_inuse(struct buf_pool *const bp, struct qent_base *be)
{
    if (!be)
        return;
    pthread_mutex_lock(&bp->lock);
    ql_add_tail(&bp->inuse, be);
    be->status = QENT_WAITING;
    pthread_mutex_unlock(&bp->lock);
}

static struct qent_base *queue_get_free(struct buf_pool *const bp)
{
    struct qent_base *buf;

    if (do_wait(&bp->free_sem))
        return NULL;
    pthread_mutex_lock(&bp->lock);
    buf = bq_get_free(bp);
    pthread_mutex_unlock(&bp->lock);
    return buf;
}

static struct qent_base *queue_tryget_free(struct buf_pool *const bp)
{
    struct qent_base *buf;

    if (do_trywait(&bp->free_sem))
        return NULL;
    pthread_mutex_lock(&bp->lock);
    buf = bq_get_free(bp);
    pthread_mutex_unlock(&bp->lock);
    return buf;
}

static struct qent_base * queue_find_extract_index(struct buf_pool *const bp, const unsigned int index)
{
    struct qent_base *be;

    pthread_mutex_lock(&bp->lock);
    /* Expect 1st in Q, but allow anywhere */
    for (be = bp->inuse.head; be; be = be->next) {
        if (be->index == index) {
            bq_extract_inuse(bp, be);
            break;
        }
    }
    pthread_mutex_unlock(&bp->lock);

    return be;
}

static void queue_delete(struct buf_pool *const bp)
{
    sem_destroy(&bp->free_sem);
    pthread_mutex_destroy(&bp->lock);
    free(bp);
}

static struct buf_pool* queue_new(const int vfd)
{
    struct buf_pool *bp = calloc(1, sizeof(*bp));
    if (!bp)
        return NULL;
    pthread_mutex_init(&bp->lock, NULL);
    sem_init(&bp->free_sem, 0, 0);
    return bp;
}


struct mediabufs_ctl {
    atomic_int ref_count;  /* 0 is single ref for easier atomics */
    void * dc;
    int vfd;
    bool stream_on;
    bool polling;
    bool dst_fixed;             // Dst Q is fixed size
    pthread_mutex_t lock;
    struct buf_pool * src;
    struct buf_pool * dst;
    struct polltask * pt;
    struct pollqueue * pq;
    struct ff_weak_link_master * this_wlm;

    enum mediabufs_memory src_memtype;
    enum mediabufs_memory dst_memtype;
    struct v4l2_format src_fmt;
    struct v4l2_format dst_fmt;
    struct v4l2_capability capability;
};

static int qe_v4l2_queue(struct qent_base *const be,
               const int vfd, struct media_request *const mreq,
               const struct v4l2_format *const fmt,
               const bool is_dst, const bool hold_flag)
{
    struct v4l2_buffer buffer = {
        .type = fmt->type,
        .memory = mediabufs_memory_to_v4l2(be->memtype),
        .index = be->index
    };
    struct v4l2_plane planes[VIDEO_MAX_PLANES] = {{0}};

    if (V4L2_TYPE_IS_MULTIPLANAR(fmt->type)) {
        unsigned int i;
        for (i = 0; i < VIDEO_MAX_PLANES && be->dh[i]; ++i) {
            if (is_dst)
                dmabuf_len_set(be->dh[i], 0);

            /* *** Really need a pixdesc rather than a format so we can fill in data_offset */
            planes[i].length = dmabuf_size(be->dh[i]);
            planes[i].bytesused = dmabuf_len(be->dh[i]);
            if (be->memtype == MEDIABUFS_MEMORY_DMABUF)
                planes[i].m.fd = dmabuf_fd(be->dh[i]);
            else
                planes[i].m.mem_offset = 0;
        }
        buffer.m.planes = planes;
        buffer.length = i;
    }
    else {
        if (is_dst)
            dmabuf_len_set(be->dh[0], 0);

        buffer.bytesused = dmabuf_len(be->dh[0]);
        buffer.length = dmabuf_size(be->dh[0]);
        if (be->memtype == MEDIABUFS_MEMORY_DMABUF)
            buffer.m.fd = dmabuf_fd(be->dh[0]);
        else
            buffer.m.offset = 0;
    }

    if (!is_dst && mreq) {
        buffer.flags |= V4L2_BUF_FLAG_REQUEST_FD;
        buffer.request_fd = media_request_fd(mreq);
        if (hold_flag)
            buffer.flags |= V4L2_BUF_FLAG_M2M_HOLD_CAPTURE_BUF;
    }

    if (is_dst)
        be->timestamp = (struct timeval){0,0};

    buffer.timestamp = be->timestamp;

    while (ioctl(vfd, VIDIOC_QBUF, &buffer)) {
        const int err = errno;
        if (err != EINTR) {
            request_log("%s: Failed to Q buffer: err=%d (%s)\n", __func__, err, strerror(err));
            return -err;
        }
    }
    return 0;
}

static struct qent_base * qe_dequeue(struct buf_pool *const bp,
                     const int vfd,
                     const struct v4l2_format * const f)
{
    struct qent_base *be;
    int rc;
    const bool mp = V4L2_TYPE_IS_MULTIPLANAR(f->type);
    struct v4l2_plane planes[VIDEO_MAX_PLANES] = {{0}};
    struct v4l2_buffer buffer = {
        .type =  f->type,
        .memory = mediabufs_memory_to_v4l2(bp->memtype)
    };
    if (mp) {
        buffer.length = f->fmt.pix_mp.num_planes;
        buffer.m.planes = planes;
    }

    while ((rc = ioctl(vfd, VIDIOC_DQBUF, &buffer)) != 0 &&
           errno == EINTR)
        /* Loop */;
    if (rc) {
        request_log("Error DQing buffer type %d: %s\n", f->type, strerror(errno));
        return NULL;
    }

    be = queue_find_extract_index(bp, buffer.index);
    if (!be) {
        request_log("Failed to find index %d in Q\n", buffer.index);
        return NULL;
    }

    if (mp) {
        unsigned int i;
        for (i = 0; i != buffer.length; ++i)
            dmabuf_len_set(be->dh[i], V4L2_TYPE_IS_CAPTURE(f->type) ? planes[i].bytesused : 0);
    }
    else
        dmabuf_len_set(be->dh[0], V4L2_TYPE_IS_CAPTURE(f->type) ? buffer.length : 0);

    be->timestamp = buffer.timestamp;
    be->status = (buffer.flags & V4L2_BUF_FLAG_ERROR) ? QENT_ERROR : QENT_DONE;
    return be;
}

static void qe_dst_done(struct qent_dst * dst_be)
{
    pthread_mutex_lock(&dst_be->lock);
    dst_be->waiting = false;
    pthread_cond_broadcast(&dst_be->cond);
    pthread_mutex_unlock(&dst_be->lock);

    qent_dst_unref(&dst_be);
}

static bool qe_dst_waiting(struct qent_dst *const dst_be)
{
    bool waiting;
    pthread_mutex_lock(&dst_be->lock);
    waiting = dst_be->waiting;
    dst_be->waiting = true;
    pthread_mutex_unlock(&dst_be->lock);
    return waiting;
}


static bool mediabufs_wants_poll(const struct mediabufs_ctl *const mbc)
{
    return queue_is_inuse(mbc->src) || queue_is_inuse(mbc->dst);
}

static void mediabufs_poll_cb(void * v, short revents)
{
    struct mediabufs_ctl *mbc = v;
    struct qent_src *src_be = NULL;
    struct qent_dst *dst_be = NULL;

    if (!revents)
        request_err(mbc->dc, "%s: Timeout\n", __func__);

    pthread_mutex_lock(&mbc->lock);
    mbc->polling = false;

    if ((revents & POLLOUT) != 0)
        src_be = base_to_src(qe_dequeue(mbc->src, mbc->vfd, &mbc->src_fmt));
    if ((revents & POLLIN) != 0)
        dst_be = base_to_dst(qe_dequeue(mbc->dst, mbc->vfd, &mbc->dst_fmt));

    /* Reschedule */
    if (mediabufs_wants_poll(mbc)) {
        mbc->polling = true;
        pollqueue_add_task(mbc->pt, 2000);
    }
    pthread_mutex_unlock(&mbc->lock);

    if (src_be)
        queue_put_free(mbc->src, &src_be->base);
    if (dst_be)
        qe_dst_done(dst_be);
}

int qent_src_params_set(struct qent_src *const be_src, const struct timeval * timestamp)
{
    struct qent_base *const be = &be_src->base;

    be->timestamp = *timestamp;
    return 0;
}

struct timeval qent_dst_timestamp_get(const struct qent_dst *const be_dst)
{
    return be_dst->base.timestamp;
}

static int qent_base_realloc(struct qent_base *const be, const size_t len, struct dmabufs_ctl * dbsc)
{
    if (!be->dh[0] || len > dmabuf_size(be->dh[0])) {
        size_t newsize = round_up_size(len);
        request_log("%s: Overrun %zd > %zd; trying %zd\n", __func__, len, dmabuf_size(be->dh[0]), newsize);
        if (!dbsc) {
            request_log("%s: No dmbabuf_ctrl for realloc\n", __func__);
            return -ENOMEM;
        }
        if ((be->dh[0] = dmabuf_realloc(dbsc, be->dh[0], newsize)) == NULL) {
            request_log("%s: Realloc %zd failed\n", __func__, newsize);
            return -ENOMEM;
        }
    }
    return 0;
}

int qent_src_alloc(struct qent_src *const be_src, const size_t len, struct dmabufs_ctl * dbsc)
{
    struct qent_base *const be = &be_src->base;
    return qent_base_realloc(be, len, dbsc);
}


int qent_src_data_copy(struct qent_src *const be_src, const size_t offset, const void *const src, const size_t len, struct dmabufs_ctl * dbsc)
{
    void * dst;
    struct qent_base *const be = &be_src->base;
    int rv;

    // Realloc doesn't copy so don't alloc if offset != 0
    if ((rv = qent_base_realloc(be, offset + len,
                                be_src->fixed_size || offset ? NULL : dbsc)) != 0)
        return rv;

    dmabuf_write_start(be->dh[0]);
    dst = dmabuf_map(be->dh[0]);
    if (!dst)
        return -1;
    memcpy((char*)dst + offset, src, len);
    dmabuf_len_set(be->dh[0], len);
    dmabuf_write_end(be->dh[0]);
    return 0;
}

const struct dmabuf_h * qent_dst_dmabuf(const struct qent_dst *const be_dst, unsigned int plane)
{
    const struct qent_base *const be = &be_dst->base;

    return (plane >= sizeof(be->dh)/sizeof(be->dh[0])) ? NULL : be->dh[plane];
}

int qent_dst_dup_fd(const struct qent_dst *const be_dst, unsigned int plane)
{
    return dup(dmabuf_fd(qent_dst_dmabuf(be_dst, plane)));
}

MediaBufsStatus mediabufs_start_request(struct mediabufs_ctl *const mbc,
                struct media_request **const pmreq,
                struct qent_src **const psrc_be,
                struct qent_dst *const dst_be,
                const bool is_final)
{
    struct media_request * mreq = *pmreq;
    struct qent_src *const src_be = *psrc_be;

    // Req & src are always both "consumed"
    *pmreq = NULL;
    *psrc_be = NULL;

    pthread_mutex_lock(&mbc->lock);

    if (!src_be)
        goto fail1;

    if (dst_be) {
        if (qe_dst_waiting(dst_be)) {
            request_info(mbc->dc, "Request buffer already waiting on start\n");
            goto fail1;
        }
        dst_be->base.timestamp = (struct timeval){0,0};
        if (qe_v4l2_queue(&dst_be->base, mbc->vfd, NULL, &mbc->dst_fmt, true, false))
            goto fail1;

        qent_dst_ref(dst_be);
        queue_put_inuse(mbc->dst, &dst_be->base);
    }

    if (qe_v4l2_queue(&src_be->base, mbc->vfd, mreq, &mbc->src_fmt, false, !is_final))
        goto fail1;
    queue_put_inuse(mbc->src, &src_be->base);

    if (!mbc->polling && mediabufs_wants_poll(mbc)) {
        mbc->polling = true;
        pollqueue_add_task(mbc->pt, 2000);
    }
    pthread_mutex_unlock(&mbc->lock);

    if (media_request_start(mreq))
        return MEDIABUFS_ERROR_OPERATION_FAILED;

    return MEDIABUFS_STATUS_SUCCESS;

fail1:
    media_request_abort(&mreq);
    if (src_be)
        queue_put_free(mbc->src, &src_be->base);

// *** TODO: If src Q fails this doesnt unwind properly - separate dst Q from src Q
    if (dst_be) {
        dst_be->base.status = QENT_ERROR;
        qe_dst_done(dst_be);
    }
    pthread_mutex_unlock(&mbc->lock);
    return MEDIABUFS_ERROR_OPERATION_FAILED;
}


static int qe_alloc_from_fmt(struct qent_base *const be,
                   struct dmabufs_ctl *const dbsc,
                   const struct v4l2_format *const fmt)
{
    if (V4L2_TYPE_IS_MULTIPLANAR(fmt->type)) {
        unsigned int i;
        for (i = 0; i != fmt->fmt.pix_mp.num_planes; ++i) {
            be->dh[i] = dmabuf_realloc(dbsc, be->dh[i],
                fmt->fmt.pix_mp.plane_fmt[i].sizeimage);
            /* On failure tidy up and die */
            if (!be->dh[i]) {
                while (i--) {
                    dmabuf_free(be->dh[i]);
                    be->dh[i] = NULL;
                }
                return -1;
            }
        }
    }
    else {
//      be->dh[0] = dmabuf_alloc(dbsc, fmt->fmt.pix.sizeimage);
        size_t size = fmt->fmt.pix.sizeimage;
        be->dh[0] = dmabuf_realloc(dbsc, be->dh[0], size);
        if (!be->dh[0])
            return -1;
    }
    return 0;
}

static MediaBufsStatus fmt_set(struct v4l2_format *const fmt, const int fd,
            const enum v4l2_buf_type buftype,
            uint32_t pixfmt,
            const unsigned int width, const unsigned int height,
                               const size_t bufsize)
{
    *fmt = (struct v4l2_format){.type = buftype};

    if (V4L2_TYPE_IS_MULTIPLANAR(buftype)) {
        fmt->fmt.pix_mp.width = width;
        fmt->fmt.pix_mp.height = height;
        fmt->fmt.pix_mp.pixelformat = pixfmt;
        if (bufsize) {
            fmt->fmt.pix_mp.num_planes = 1;
            fmt->fmt.pix_mp.plane_fmt[0].sizeimage = bufsize;
        }
    }
    else {
        fmt->fmt.pix.width = width;
        fmt->fmt.pix.height = height;
        fmt->fmt.pix.pixelformat = pixfmt;
        fmt->fmt.pix.sizeimage = bufsize;
    }

    while (ioctl(fd, VIDIOC_S_FMT, fmt))
        if (errno != EINTR)
            return MEDIABUFS_ERROR_OPERATION_FAILED;

    // Treat anything where we don't get at least what we asked for as a fail
    if (V4L2_TYPE_IS_MULTIPLANAR(buftype)) {
        if (fmt->fmt.pix_mp.width < width ||
            fmt->fmt.pix_mp.height < height ||
            fmt->fmt.pix_mp.pixelformat != pixfmt) {
            return MEDIABUFS_ERROR_UNSUPPORTED_BUFFERTYPE;
        }
    }
    else {
        if (fmt->fmt.pix.width < width ||
            fmt->fmt.pix.height < height ||
            fmt->fmt.pix.pixelformat != pixfmt) {
            return MEDIABUFS_ERROR_UNSUPPORTED_BUFFERTYPE;
        }
    }

    return MEDIABUFS_STATUS_SUCCESS;
}

static MediaBufsStatus find_fmt_flags(struct v4l2_format *const fmt,
                   const int fd,
                   const unsigned int type_v4l2,
                   const uint32_t flags_must,
                   const uint32_t flags_not,
                   const unsigned int width,
                   const unsigned int height,
                   mediabufs_dst_fmt_accept_fn *const accept_fn,
                   void *const accept_v)
{
    unsigned int i;

    for (i = 0;; ++i) {
        struct v4l2_fmtdesc fmtdesc = {
            .index = i,
            .type = type_v4l2
        };
        while (ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc)) {
            if (errno != EINTR)
                return MEDIABUFS_ERROR_UNSUPPORTED_BUFFERTYPE;
        }
        if ((fmtdesc.flags & flags_must) != flags_must ||
            (fmtdesc.flags & flags_not))
            continue;
        if (!accept_fn(accept_v, &fmtdesc))
            continue;

        if (fmt_set(fmt, fd, fmtdesc.type, fmtdesc.pixelformat,
                width, height, 0) == MEDIABUFS_STATUS_SUCCESS)
            return MEDIABUFS_STATUS_SUCCESS;
    }
    return 0;
}


/* Wait for qent done */

MediaBufsStatus qent_dst_wait(struct qent_dst *const be_dst)
{
    struct qent_base *const be = &be_dst->base;
    enum qent_status estat;

    pthread_mutex_lock(&be_dst->lock);
    while (be_dst->waiting &&
           !pthread_cond_wait(&be_dst->cond, &be_dst->lock))
        /* Loop */;
    estat = be->status;
    pthread_mutex_unlock(&be_dst->lock);

    return estat == QENT_DONE ? MEDIABUFS_STATUS_SUCCESS :
        estat == QENT_ERROR ? MEDIABUFS_ERROR_DECODING_ERROR :
            MEDIABUFS_ERROR_OPERATION_FAILED;
}

const uint8_t * qent_dst_data(struct qent_dst *const be_dst, unsigned int buf_no)
{
    struct qent_base *const be = &be_dst->base;
    return dmabuf_map(be->dh[buf_no]);
}

MediaBufsStatus qent_dst_read_start(struct qent_dst *const be_dst)
{
    struct qent_base *const be = &be_dst->base;
    unsigned int i;
    for (i = 0; i != VIDEO_MAX_PLANES && be->dh[i]; ++i) {
        if (dmabuf_read_start(be->dh[i])) {
            while (i--)
                dmabuf_read_end(be->dh[i]);
            return MEDIABUFS_ERROR_ALLOCATION_FAILED;
        }
    }
    return MEDIABUFS_STATUS_SUCCESS;
}

MediaBufsStatus qent_dst_read_stop(struct qent_dst *const be_dst)
{
    struct qent_base *const be = &be_dst->base;
    unsigned int i;
    MediaBufsStatus status = MEDIABUFS_STATUS_SUCCESS;

    for (i = 0; i != VIDEO_MAX_PLANES && be->dh[i]; ++i) {
        if (dmabuf_read_end(be->dh[i]))
            status = MEDIABUFS_ERROR_OPERATION_FAILED;
    }
    return status;
}

struct qent_dst * qent_dst_ref(struct qent_dst * const be_dst)
{
    if (be_dst)
        atomic_fetch_add(&be_dst->base.ref_count, 1);
    return be_dst;
}

void qent_dst_unref(struct qent_dst ** const pbe_dst)
{
    struct qent_dst * const be_dst = *pbe_dst;
    struct mediabufs_ctl * mbc;
    if (!be_dst)
        return;
    *pbe_dst = NULL;

    if (atomic_fetch_sub(&be_dst->base.ref_count, 1) != 0)
        return;

    if ((mbc = ff_weak_link_lock(&be_dst->mbc_wl)) != NULL) {
        queue_put_free(mbc->dst, &be_dst->base);
        ff_weak_link_unlock(be_dst->mbc_wl);
    }
    else {
        qe_dst_free(be_dst);
    }
}

MediaBufsStatus qent_dst_import_fd(struct qent_dst *const be_dst,
                unsigned int plane,
                int fd, size_t size)
{
    struct qent_base *const be = &be_dst->base;
    struct dmabuf_h * dh;

    if (be->status != QENT_IMPORT || be->dh[plane])
        return MEDIABUFS_ERROR_OPERATION_FAILED;

    dh = dmabuf_import(fd, size);
    if (!dh)
        return MEDIABUFS_ERROR_ALLOCATION_FAILED;

    be->dh[plane] = dh;
    return MEDIABUFS_STATUS_SUCCESS;
}

// Returns noof buffers created, -ve for error
static int create_dst_bufs(struct mediabufs_ctl *const mbc, unsigned int n, struct qent_dst * const qes[])
{
    unsigned int i;

    struct v4l2_create_buffers cbuf = {
        .count = n,
        .memory = mediabufs_memory_to_v4l2(mbc->dst->memtype),
        .format = mbc->dst_fmt,
    };

    while (ioctl(mbc->vfd, VIDIOC_CREATE_BUFS, &cbuf)) {
        const int err = -errno;
        if (err != EINTR) {
            request_err(mbc->dc, "%s: Failed to create V4L2 buffer\n", __func__);
            return -err;
        }
    }

    if (cbuf.count != n)
        request_warn(mbc->dc, "%s: Created %d of %d V4L2 buffers requested\n", __func__, cbuf.count, n);

    for (i = 0; i != cbuf.count; ++i)
        qes[i]->base.index = cbuf.index + i;

    return cbuf.count;
}

static MediaBufsStatus
qe_import_from_buf(struct mediabufs_ctl *const mbc, struct qent_base * const be, const struct v4l2_format *const fmt,
                   const unsigned int n, const bool x_dmabuf)
{
    struct v4l2_buffer buf = {
        .index = n,
        .type = fmt->type,
    };
    struct v4l2_plane planes[VIDEO_MAX_PLANES];
    int ret;

    if (be->dh[0])
        return 0;

    if (V4L2_TYPE_IS_MULTIPLANAR(fmt->type)) {
        memset(planes, 0, sizeof(planes));
        buf.m.planes = planes;
        buf.length = VIDEO_MAX_PLANES;
    }

    if ((ret = ioctl(mbc->vfd, VIDIOC_QUERYBUF, &buf)) != 0) {
        request_err(mbc->dc, "VIDIOC_QUERYBUF failed");
        return MEDIABUFS_ERROR_OPERATION_FAILED;
    }

    if (V4L2_TYPE_IS_MULTIPLANAR(fmt->type))
    {
        unsigned int i;
        for (i = 0; i != buf.length; ++i) {
            if (x_dmabuf) {
                struct v4l2_exportbuffer xbuf = {
                    .type = buf.type,
                    .index = buf.index,
                    .plane = i,
                    .flags = O_RDWR, // *** Arguably O_RDONLY would be fine
                };
                if (ioctl(mbc->vfd, VIDIOC_EXPBUF, &xbuf) == 0)
                    be->dh[i] = dmabuf_import(xbuf.fd, planes[i].length);
            }
            else {
                be->dh[i] = dmabuf_import_mmap(
                    mmap(NULL, planes[i].length,
                        PROT_READ | PROT_WRITE,
                        MAP_SHARED | MAP_POPULATE,
                        mbc->vfd, planes[i].m.mem_offset),
                    planes[i].length);
            }
            /* On failure tidy up and die */
            if (!be->dh[i]) {
                while (i--) {
                    dmabuf_free(be->dh[i]);
                    be->dh[i] = NULL;
                }
                return MEDIABUFS_ERROR_OPERATION_FAILED;
            }
        }
    }
    else
    {
        if (x_dmabuf) {
            struct v4l2_exportbuffer xbuf = {
                .type = buf.type,
                .index = buf.index,
                .flags = O_RDWR, // *** Arguably O_RDONLY would be fine
            };
            if (ioctl(mbc->vfd, VIDIOC_EXPBUF, &xbuf) == 0)
                be->dh[0] = dmabuf_import(xbuf.fd, buf.length);
        }
        else {
            be->dh[0] = dmabuf_import_mmap(
                mmap(NULL, buf.length,
                    PROT_READ | PROT_WRITE,
                    MAP_SHARED | MAP_POPULATE,
                    mbc->vfd, buf.m.offset),
                buf.length);
        }
        /* On failure tidy up and die */
        if (!be->dh[0]) {
            return MEDIABUFS_ERROR_OPERATION_FAILED;
        }
    }

    return 0;
}

struct qent_dst* mediabufs_dst_qent_alloc(struct mediabufs_ctl *const mbc, struct dmabufs_ctl *const dbsc)
{
    struct qent_dst * be_dst;

    if (mbc == NULL) {
        be_dst = qe_dst_new(NULL, MEDIABUFS_MEMORY_DMABUF);
        if (be_dst)
            be_dst->base.status = QENT_IMPORT;
        return be_dst;
    }

    if (mbc->dst_fixed) {
        be_dst = base_to_dst(queue_get_free(mbc->dst));
        if (!be_dst)
            return NULL;
    }
    else {
        be_dst = base_to_dst(queue_tryget_free(mbc->dst));
        if (!be_dst) {
            be_dst = qe_dst_new(mbc->this_wlm, mbc->dst->memtype);
            if (!be_dst)
                return NULL;

            if (create_dst_bufs(mbc, 1, &be_dst) != 1) {
                qe_dst_free(be_dst);
                return NULL;
            }
        }
    }

    if (mbc->dst->memtype == MEDIABUFS_MEMORY_MMAP) {
        if (qe_import_from_buf(mbc, &be_dst->base, &mbc->dst_fmt, be_dst->base.index, true)) {
            request_err(mbc->dc, "Failed to export as dmabuf\n");
            queue_put_free(mbc->dst, &be_dst->base);
            return NULL;
        }
    }
    else {
        if (qe_alloc_from_fmt(&be_dst->base, dbsc, &mbc->dst_fmt)) {
            /* Given  how create buf works we can't uncreate it on alloc failure
             * all we can do is put it on the free Q
            */
            queue_put_free(mbc->dst, &be_dst->base);
            return NULL;
        }
    }

    be_dst->base.status = QENT_PENDING;
    atomic_store(&be_dst->base.ref_count, 0);
    return be_dst;
}

const struct v4l2_format *mediabufs_dst_fmt(struct mediabufs_ctl *const mbc)
{
    return &mbc->dst_fmt;
}

MediaBufsStatus mediabufs_dst_fmt_set(struct mediabufs_ctl *const mbc,
               const unsigned int width,
               const unsigned int height,
               mediabufs_dst_fmt_accept_fn *const accept_fn,
               void *const accept_v)
{
    MediaBufsStatus status;
    unsigned int i;
    const enum v4l2_buf_type buf_type = mbc->dst_fmt.type;
    static const struct {
        unsigned int flags_must;
        unsigned int flags_not;
    } trys[] = {
        {0, V4L2_FMT_FLAG_EMULATED},
        {V4L2_FMT_FLAG_EMULATED, 0},
    };
    for (i = 0; i != sizeof(trys)/sizeof(trys[0]); ++i) {
        status = find_fmt_flags(&mbc->dst_fmt, mbc->vfd,
                                buf_type,
                                trys[i].flags_must,
                                trys[i].flags_not,
                                width, height, accept_fn, accept_v);
        if (status != MEDIABUFS_ERROR_UNSUPPORTED_BUFFERTYPE)
            return status;
    }

    if (status != MEDIABUFS_STATUS_SUCCESS)
        return status;

    /* Try to create a buffer - don't alloc */
    return status;
}

// ** This is a mess if we get partial alloc but without any way to remove
//    individual V4L2 Q members we are somewhat stuffed
MediaBufsStatus mediabufs_dst_slots_create(struct mediabufs_ctl *const mbc, const unsigned int n, const bool fixed, const enum mediabufs_memory memtype)
{
    unsigned int i;
    int a = 0;
    unsigned int qc;
    struct qent_dst * qes[32];

    if (n > 32)
        return MEDIABUFS_ERROR_ALLOCATION_FAILED;

    mbc->dst->memtype = memtype;

    // Create qents first as it is hard to get rid of the V4L2 buffers on error
    for (qc = 0; qc != n; ++qc)
    {
        if ((qes[qc] = qe_dst_new(mbc->this_wlm, mbc->dst->memtype)) == NULL)
            goto fail;
    }

    if ((a = create_dst_bufs(mbc, n, qes)) < 0)
        goto fail;

    for (i = 0; i != a; ++i)
        queue_put_free(mbc->dst, &qes[i]->base);

    if (a != n)
        goto fail;

    mbc->dst_fixed = fixed;
    return MEDIABUFS_STATUS_SUCCESS;

fail:
    for (i = (a < 0 ? 0 : a); i != qc; ++i)
        qe_dst_free(qes[i]);

    return MEDIABUFS_ERROR_ALLOCATION_FAILED;
}

struct qent_src *mediabufs_src_qent_get(struct mediabufs_ctl *const mbc)
{
    struct qent_base * buf = queue_get_free(mbc->src);
    buf->status = QENT_PENDING;
    return base_to_src(buf);
}

void mediabufs_src_qent_abort(struct mediabufs_ctl *const mbc, struct qent_src **const pqe_src)
{
    struct qent_src *const qe_src = *pqe_src;
    if (!qe_src)
        return;
    *pqe_src = NULL;
    queue_put_free(mbc->src, &qe_src->base);
}

static MediaBufsStatus
chk_memory_type(struct mediabufs_ctl *const mbc,
    const struct v4l2_format * const f,
    const enum mediabufs_memory m)
{
    struct v4l2_create_buffers cbuf = {
        .count = 0,
        .memory = V4L2_MEMORY_MMAP,
        .format = *f
    };

    if (ioctl(mbc->vfd, VIDIOC_CREATE_BUFS, &cbuf) != 0)
        return MEDIABUFS_ERROR_OPERATION_FAILED;

    switch (m) {
    case MEDIABUFS_MEMORY_DMABUF:
        // 0 = Unknown but assume not in that case
        if ((cbuf.capabilities & V4L2_BUF_CAP_SUPPORTS_DMABUF) == 0)
            return MEDIABUFS_ERROR_UNSUPPORTED_MEMORY;
        break;
    case MEDIABUFS_MEMORY_MMAP:
        break;
    default:
        return MEDIABUFS_ERROR_UNSUPPORTED_MEMORY;
    }

    return MEDIABUFS_STATUS_SUCCESS;
}

MediaBufsStatus
mediabufs_src_chk_memtype(struct mediabufs_ctl *const mbc, const enum mediabufs_memory memtype)
{
    return chk_memory_type(mbc, &mbc->src_fmt, memtype);
}

MediaBufsStatus
mediabufs_dst_chk_memtype(struct mediabufs_ctl *const mbc, const enum mediabufs_memory memtype)
{
    return chk_memory_type(mbc, &mbc->dst_fmt, memtype);
}

/* src format must have been set up before this */
MediaBufsStatus mediabufs_src_pool_create(struct mediabufs_ctl *const mbc,
                  struct dmabufs_ctl * const dbsc,
                  unsigned int n, const enum mediabufs_memory memtype)
{
    unsigned int i;
    struct v4l2_requestbuffers req = {
        .count = n,
        .type = mbc->src_fmt.type,
        .memory = mediabufs_memory_to_v4l2(memtype)
    };

    bq_free_all_free_src(mbc->src);

    while (ioctl(mbc->vfd, VIDIOC_REQBUFS, &req) == -1) {
        if (errno != EINTR) {
            request_err(mbc->dc, "%s: Failed to request src bufs\n", __func__);
            return MEDIABUFS_ERROR_OPERATION_FAILED;
        }
    }

    if (n > req.count) {
        request_info(mbc->dc, "Only allocated %d of %d src buffers requested\n", req.count, n);
        n = req.count;
    }

    for (i = 0; i != n; ++i) {
        struct qent_src *const be_src = qe_src_new(memtype);
        if (!be_src) {
            request_err(mbc->dc, "Failed to create src be %d\n", i);
            goto fail;
        }
        switch (memtype) {
        case MEDIABUFS_MEMORY_MMAP:
            if (qe_import_from_buf(mbc, &be_src->base, &mbc->src_fmt, i, false)) {
                qe_src_free(be_src);
                goto fail;
            }
            be_src->fixed_size = 1;
            break;
        case MEDIABUFS_MEMORY_DMABUF:
            if (qe_alloc_from_fmt(&be_src->base, dbsc, &mbc->src_fmt)) {
                qe_src_free(be_src);
                goto fail;
            }
            be_src->fixed_size = !mediabufs_src_resizable(mbc);
            break;
        default:
            request_err(mbc->dc, "Unexpected memorty type\n");
            goto fail;
        }
        be_src->base.index = i;

        queue_put_free(mbc->src, &be_src->base);
    }

    mbc->src->memtype = memtype;
    return MEDIABUFS_STATUS_SUCCESS;

fail:
    bq_free_all_free_src(mbc->src);
    req.count = 0;
    while (ioctl(mbc->vfd, VIDIOC_REQBUFS, &req) == -1 &&
           errno == EINTR)
        /* Loop */;

    return MEDIABUFS_ERROR_OPERATION_FAILED;
}



/*
 * Set stuff order:
 *  Set src fmt
 *  Set parameters (sps) on vfd
 *  Negotiate dst format (dst_fmt_set)
 *  Create src buffers
 *  Alloc a dst buffer or Create dst slots
*/
MediaBufsStatus mediabufs_stream_on(struct mediabufs_ctl *const mbc)
{
    if (mbc->stream_on)
        return MEDIABUFS_STATUS_SUCCESS;

    if (set_stream(mbc->vfd, mbc->src_fmt.type, true) < 0) {
        request_log("Failed to set stream on src type %d\n", mbc->src_fmt.type);
        return MEDIABUFS_ERROR_OPERATION_FAILED;
    }

    if (set_stream(mbc->vfd, mbc->dst_fmt.type, true) < 0) {
        request_log("Failed to set stream on dst type %d\n", mbc->dst_fmt.type);
        set_stream(mbc->vfd, mbc->src_fmt.type, false);
        return MEDIABUFS_ERROR_OPERATION_FAILED;
    }

    mbc->stream_on = true;
    return MEDIABUFS_STATUS_SUCCESS;
}

MediaBufsStatus mediabufs_stream_off(struct mediabufs_ctl *const mbc)
{
    MediaBufsStatus status = MEDIABUFS_STATUS_SUCCESS;

    if (!mbc->stream_on)
        return MEDIABUFS_STATUS_SUCCESS;

    if (set_stream(mbc->vfd, mbc->dst_fmt.type, false) < 0) {
        request_log("Failed to set stream off dst type %d\n", mbc->dst_fmt.type);
        status = MEDIABUFS_ERROR_OPERATION_FAILED;
    }

    if (set_stream(mbc->vfd, mbc->src_fmt.type, false) < 0) {
        request_log("Failed to set stream off src type %d\n", mbc->src_fmt.type);
        status = MEDIABUFS_ERROR_OPERATION_FAILED;
    }

    mbc->stream_on = false;
    return status;
}

int mediabufs_ctl_set_ext_ctrls(struct mediabufs_ctl * mbc, struct media_request * const mreq, struct v4l2_ext_control control_array[], unsigned int n)
{
    struct v4l2_ext_controls controls = {
        .controls = control_array,
        .count = n
    };

    if (mreq) {
        controls.which = V4L2_CTRL_WHICH_REQUEST_VAL;
        controls.request_fd = media_request_fd(mreq);
    }

    while (ioctl(mbc->vfd, VIDIOC_S_EXT_CTRLS, &controls))
    {
        const int err = errno;
        if (err != EINTR) {
            request_err(mbc->dc, "Unable to set controls: %s\n", strerror(err));
            return -err;
        }
    }

    return 0;
}

MediaBufsStatus mediabufs_set_ext_ctrl(struct mediabufs_ctl *const mbc,
                struct media_request * const mreq,
                unsigned int id, void *data,
                unsigned int size)
{
    struct v4l2_ext_control control = {
        .id = id,
        .ptr = data,
        .size = size
    };

    int rv = mediabufs_ctl_set_ext_ctrls(mbc, mreq, &control, 1);
    return !rv ? MEDIABUFS_STATUS_SUCCESS : MEDIABUFS_ERROR_OPERATION_FAILED;
}

MediaBufsStatus mediabufs_src_fmt_set(struct mediabufs_ctl *const mbc,
                                      enum v4l2_buf_type buf_type,
                   const uint32_t pixfmt,
                   const uint32_t width, const uint32_t height,
                                      const size_t bufsize)
{
    MediaBufsStatus rv = fmt_set(&mbc->src_fmt, mbc->vfd, buf_type, pixfmt, width, height, bufsize);
    if (rv != MEDIABUFS_STATUS_SUCCESS)
        request_err(mbc->dc, "Failed to set src buftype %d, format %#x %dx%d\n", buf_type, pixfmt, width, height);

    return rv;
}

int mediabufs_ctl_query_ext_ctrls(struct mediabufs_ctl * mbc, struct v4l2_query_ext_ctrl ctrls[], unsigned int n)
{
    int rv = 0;
    while (n--) {
        while (ioctl(mbc->vfd, VIDIOC_QUERY_EXT_CTRL, ctrls)) {
            const int err = errno;
            if (err != EINTR) {
                // Often used for probing - errors are to be expected
                request_debug(mbc->dc, "Failed to query ext id=%#x, err=%d\n", ctrls->id, err);
                ctrls->type = 0; // 0 is invalid
                rv = -err;
                break;
            }
        }
        ++ctrls;
    }
    return rv;
}

int mediabufs_src_resizable(const struct mediabufs_ctl *const mbc)
{
#if 1
    return 0;
#else
    // Single planar OUTPUT can only take exact size buffers
    // Multiplanar will take larger than negotiated
    return V4L2_TYPE_IS_MULTIPLANAR(mbc->src_fmt.type);
#endif
}

static void mediabufs_ctl_delete(struct mediabufs_ctl *const mbc)
{
    if (!mbc)
        return;

    // Break the weak link first
    ff_weak_link_break(&mbc->this_wlm);

    polltask_delete(&mbc->pt);

    mediabufs_stream_off(mbc);

    // Empty v4l2 buffer stash
    request_buffers(mbc->vfd, mbc->src_fmt.type, V4L2_MEMORY_MMAP, 0);
    request_buffers(mbc->vfd, mbc->dst_fmt.type, V4L2_MEMORY_MMAP, 0);

    bq_free_all_free_src(mbc->src);
    bq_free_all_inuse_src(mbc->src);
    bq_free_all_free_dst(mbc->dst);

    {
        struct qent_dst *dst_be;
        while ((dst_be = base_to_dst(bq_get_inuse(mbc->dst))) != NULL) {
            dst_be->base.timestamp = (struct timeval){0};
            dst_be->base.status = QENT_ERROR;
            qe_dst_done(dst_be);
        }
    }

    queue_delete(mbc->dst);
    queue_delete(mbc->src);
    close(mbc->vfd);
    pthread_mutex_destroy(&mbc->lock);

    free(mbc);
}

struct mediabufs_ctl * mediabufs_ctl_ref(struct mediabufs_ctl *const mbc)
{
    atomic_fetch_add(&mbc->ref_count, 1);
    return mbc;
}

void mediabufs_ctl_unref(struct mediabufs_ctl **const pmbc)
{
    struct mediabufs_ctl *const mbc = *pmbc;
    int n;

    if (!mbc)
        return;
    *pmbc = NULL;
    n = atomic_fetch_sub(&mbc->ref_count, 1);
    if (n)
        return;
    mediabufs_ctl_delete(mbc);
}

unsigned int mediabufs_ctl_driver_version(struct mediabufs_ctl *const mbc)
{
    return mbc->capability.version;
}

static int set_capabilities(struct mediabufs_ctl *const mbc)
{
    uint32_t caps;

    if (ioctl(mbc->vfd, VIDIOC_QUERYCAP, &mbc->capability)) {
        int err = errno;
        request_err(mbc->dc, "Failed to get capabilities: %s\n", strerror(err));
        return -err;
    }

    caps = (mbc->capability.capabilities & V4L2_CAP_DEVICE_CAPS) != 0 ?
            mbc->capability.device_caps :
            mbc->capability.capabilities;

    if ((caps & V4L2_CAP_VIDEO_M2M_MPLANE) != 0) {
        mbc->src_fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        mbc->dst_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    }
    else if ((caps & V4L2_CAP_VIDEO_M2M) != 0) {
        mbc->src_fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        mbc->dst_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    }
    else {
        request_err(mbc->dc, "No M2M capabilities (%#x)\n", caps);
        return -EINVAL;
    }

    return 0;
}

/* One of these per context */
struct mediabufs_ctl * mediabufs_ctl_new(void * const dc, const char * vpath, struct pollqueue *const pq)
{
    struct mediabufs_ctl *const mbc = calloc(1, sizeof(*mbc));

    if (!mbc)
        return NULL;

    mbc->dc = dc;
    // Default mono planar
    mbc->pq = pq;
    pthread_mutex_init(&mbc->lock, NULL);

    /* Pick a default  - could we scan for this? */
    if (vpath == NULL)
        vpath = "/dev/media0";

    while ((mbc->vfd = open(vpath, O_RDWR)) == -1)
    {
        const int err = errno;
        if (err != EINTR) {
            request_err(dc, "Failed to open video dev '%s': %s\n", vpath, strerror(err));
            goto fail0;
        }
    }

    if (set_capabilities(mbc)) {
        request_err(dc, "Bad capabilities for video dev '%s'\n", vpath);
        goto fail1;
    }

    mbc->src = queue_new(mbc->vfd);
    if (!mbc->src)
        goto fail1;
    mbc->dst = queue_new(mbc->vfd);
    if (!mbc->dst)
        goto fail2;
    mbc->pt = polltask_new(pq, mbc->vfd, POLLIN | POLLOUT, mediabufs_poll_cb, mbc);
    if (!mbc->pt)
        goto fail3;
    mbc->this_wlm = ff_weak_link_new(mbc);
    if (!mbc->this_wlm)
        goto fail4;

    /* Cannot add polltask now - polling with nothing pending
     * generates infinite error polls
    */
    return mbc;

fail4:
    polltask_delete(&mbc->pt);
fail3:
    queue_delete(mbc->dst);
fail2:
    queue_delete(mbc->src);
fail1:
    close(mbc->vfd);
fail0:
    free(mbc);
    request_info(dc, "%s: FAILED\n", __func__);
    return NULL;
}



