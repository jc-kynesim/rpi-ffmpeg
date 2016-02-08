#include "config.h"
#ifdef RPI
#include "rpi_qpu.h"
#include "rpi_zc.h"

#include "libavutil/buffer_internal.h"

struct ZcPoolEnt;

typedef struct ZcPool
{
    int numbytes;
    struct ZcPoolEnt * head;
    pthread_mutex_t lock;
} ZcPool;

typedef struct ZcPoolEnt
{
    // It is important that we start with gmem as other bits of code will expect to see that
    GPU_MEM_PTR_T gmem;
    struct ZcPoolEnt * next;
    struct ZcPool * pool;
} ZcPoolEnt;

static ZcPoolEnt * zc_pool_ent_alloc(ZcPool * const pool, const int size)
{
    ZcPoolEnt * const zp = av_malloc(sizeof(ZcPoolEnt));

    printf("%s: alloc: %d\n", __func__, size);

    if (zp == NULL) {
        printf("av_malloc(ZcPoolEnt) failed\n");
        goto fail0;
    }

    if (gpu_malloc_cached(size, &zp->gmem) != 0)
    {
        printf("av_gpu_malloc_cached(%d) failed\n", size);
        goto fail1;
    }

    zp->next = NULL;
    zp->pool = pool;
    return zp;

fail1:
    av_free(zp);
fail0:
    return NULL;
}

static ZcPoolEnt * zc_pool_alloc(ZcPool * const pool, const int numbytes)
{
    ZcPoolEnt * zp;
    pthread_mutex_lock(&pool->lock);
    if (numbytes == pool->numbytes && pool->head != NULL)
    {
        printf("%s: Alloc from pool\n", __func__);
        zp = pool->head;
        pool->head = zp->next;
    }
    else
    {
        printf("%s: Alloc from sys\n", __func__);
        zp = zc_pool_ent_alloc(pool, numbytes);
    }
    pool->numbytes = numbytes;
    pthread_mutex_unlock(&pool->lock);
    return zp;
}

static void zc_pool_free(ZcPoolEnt * const zp)
{
    ZcPool * const pool = zp == NULL ? NULL : zp->pool;
    if (zp != NULL)
    {
        pthread_mutex_lock(&pool->lock);
        if (pool->numbytes == zp->gmem.numbytes)
        {
            printf("%s: Free to head\n", __func__);
            zp->next = pool->head;
            pool->head = zp;
            pthread_mutex_unlock(&pool->lock);
        }
        else
        {
            printf("%s: Free to sys\n", __func__);
            pthread_mutex_unlock(&pool->lock);
            gpu_free(&zp->gmem);
            av_free(zp);
        }
    }
}

static void
zc_pool_init(ZcPool * const pool)
{
    pool->numbytes = -1;
    pool->head = NULL;
    pthread_mutex_init(&pool->lock, NULL);
}

static void
zc_pool_destroy(ZcPool * const pool)
{
    pool->numbytes = -1;
    while (pool->head != NULL)
    {
        zc_pool_free(pool->head);
    }
    pthread_mutex_destroy(&pool->lock);
}


typedef struct ZcEnv
{
    ZcPool pool;
    volatile int refs;
} ZcEnv;

// Callback when buffer unrefed to zero
static void rpi_free_display_buffer(void *opaque, uint8_t *data)
{
    ZcPoolEnt *const zp = opaque;
//    printf("%s: data=%p\n", __func__, data);
    zc_pool_free(zp);
}

static inline GPU_MEM_PTR_T * pic_gm_ptr(AVBufferRef * const buf)
{
    // Kludge where we check the free fn to check this is really
    // one of our buffers - can't think of a better way
    return buf == NULL || buf->buffer->free != rpi_free_display_buffer ? NULL :
        av_buffer_get_opaque(buf);
}

AVRpiZcFrameGeometry av_rpi_zc_frame_geometry(
    const unsigned int video_width, const unsigned int video_height)
{
    AVRpiZcFrameGeometry geo;
    geo.stride_y = (video_width + 32 + 31) & ~31;
    geo.stride_c = geo.stride_y / 2;
//    geo.height_y = (video_height + 15) & ~15;
    geo.height_y = (video_height + 32 + 31) & ~31;
    geo.height_c = geo.height_y / 2;
    return geo;
}

static AVBufferRef * rpi_buf_pool_alloc(ZcPool * const pool, int size)
{
    ZcPoolEnt *const zp = zc_pool_alloc(pool, size);
    AVBufferRef * buf;

    printf("%s: alloc: %d\n", __func__, size);

    if (zp == NULL) {
        printf("zc_pool_alloc(%d) failed\n", size);
        goto fail0;
    }

    if ((buf = av_buffer_create(zp->gmem.arm, size, rpi_free_display_buffer, zp, AV_BUFFER_FLAG_READONLY)) == NULL)
    {
        printf("av_buffer_create() failed\n");
        goto fail2;
    }

    return buf;

fail2:
    zc_pool_free(zp);
fail0:
    return NULL;
}

static int rpi_get_display_buffer(struct AVCodecContext * const s, AVFrame * const frame)
{
    ZcEnv *const zc = s->get_buffer_context;
    const AVRpiZcFrameGeometry geo = av_rpi_zc_frame_geometry(frame->width, frame->height);
    const unsigned int size_y = geo.stride_y * geo.height_y;
    const unsigned int size_c = geo.stride_c * geo.height_c;
    const unsigned int size_pic = size_y + size_c * 2;
    AVBufferRef * buf;
    unsigned int i;

//    printf("Do local alloc: format=%#x, %dx%d: %u\n", frame->format, frame->width, frame->height, size_pic);

    if ((buf = rpi_buf_pool_alloc(&zc->pool, size_pic)) == NULL)
    {
        av_log(s, AV_LOG_ERROR, "rpi_get_display_buffer: Failed to get buffer from pool\n");
        return AVERROR(ENOMEM);
    }

    for (i = 0; i < AV_NUM_DATA_POINTERS; i++) {
        frame->buf[i] = NULL;
        frame->data[i] = NULL;
        frame->linesize[i] = 0;
    }

    frame->buf[0] = buf;
    frame->linesize[0] = geo.stride_y;
    frame->linesize[1] = geo.stride_c;
    frame->linesize[2] = geo.stride_c;
    frame->data[0] = buf->data;
    frame->data[1] = frame->data[0] + size_y;
    frame->data[2] = frame->data[1] + size_c;
    frame->extended_data = frame->data;
    // Leave extended buf alone

    return 0;
}


#define RPI_GET_BUFFER2 1

int av_rpi_zc_get_buffer2(struct AVCodecContext *s, AVFrame *frame, int flags)
{
#if !RPI_GET_BUFFER2
    return avcodec_default_get_buffer2(s, frame, flags);
#else
    int rv;

    if ((s->codec->capabilities & AV_CODEC_CAP_DR1) == 0 ||
        frame->format != AV_PIX_FMT_YUV420P)
    {
//        printf("Do default alloc: format=%#x\n", frame->format);
        rv = avcodec_default_get_buffer2(s, frame, flags);
    }
    else
    {
        rv = rpi_get_display_buffer(s, frame);
    }

#if 0
    printf("%s: %dx%d lsize=%d/%d/%d data=%p/%p/%p bref=%p/%p/%p opaque[0]=%p\n", __func__,
        frame->width, frame->height,
        frame->linesize[0], frame->linesize[1], frame->linesize[2],
        frame->data[0], frame->data[1], frame->data[2],
        frame->buf[0], frame->buf[1], frame->buf[2],
        av_buffer_get_opaque(frame->buf[0]));
#endif
    return rv;
#endif
}


static AVBufferRef * zc_copy(struct AVCodecContext * const s,
    const AVFrame * const src)
{
    AVFrame dest_frame;
    AVFrame * const dest = &dest_frame;
    unsigned int i;
    uint8_t * psrc, * pdest;

    dest->width = src->width;
    dest->height = src->height;

    if (rpi_get_display_buffer(s, dest) != 0)
    {
        return NULL;
    }

    for (i = 0, psrc = src->data[0], pdest = dest->data[0];
         i != dest->height;
         ++i, psrc += src->linesize[0], pdest += dest->linesize[0])
    {
        memcpy(pdest, psrc, dest->width);
    }
    for (i = 0, psrc = src->data[1], pdest = dest->data[1];
         i != dest->height / 2;
         ++i, psrc += src->linesize[1], pdest += dest->linesize[1])
    {
        memcpy(pdest, psrc, dest->width / 2);
    }
    for (i = 0, psrc = src->data[2], pdest = dest->data[2];
         i != dest->height / 2;
         ++i, psrc += src->linesize[2], pdest += dest->linesize[2])
    {
        memcpy(pdest, psrc, dest->width / 2);
    }

    return dest->buf[0];
}


AVRpiZcRefPtr av_rpi_zc_ref(struct AVCodecContext * const s,
    const AVFrame * const frame, const int maycopy)
{
    assert(s != NULL);

    if (frame->format != AV_PIX_FMT_YUV420P)
    {
        printf("%s: *** Format not YUV420P: %d\n", __func__, frame->format);
        return NULL;
    }

    if (frame->buf[1] != NULL)
    {
        if (maycopy)
        {
            printf("%s: *** Not a single buf frame: copying\n", __func__);
            return zc_copy(s, frame);
        }
        else
        {
            printf("%s: *** Not a single buf frame: NULL\n", __func__);
            return NULL;
        }
    }

    if (pic_gm_ptr(frame->buf[0]) == NULL)
    {
        if (maycopy)
        {
            printf("%s: *** Not one of our buffers: copying\n", __func__);
            return zc_copy(s, frame);
        }
        else
        {
            printf("%s: *** Not one of our buffers: NULL\n", __func__);
            return NULL;
        }
    }

    return av_buffer_ref(frame->buf[0]);
}

int av_rpi_zc_vc_handle(const AVRpiZcRefPtr fr_ref)
{
    const GPU_MEM_PTR_T * const p = pic_gm_ptr(fr_ref);
    return p == NULL ? -1 : p->vc_handle;
}

int av_rpi_zc_numbytes(const AVRpiZcRefPtr fr_ref)
{
    const GPU_MEM_PTR_T * const p = pic_gm_ptr(fr_ref);
    return p == NULL ? 0 : p->numbytes;
}

void av_rpi_zc_unref(AVRpiZcRefPtr fr_ref)
{
    if (fr_ref != NULL)
    {
        av_buffer_unref(&fr_ref);
    }
}

int av_rpi_zc_init(struct AVCodecContext * const s)
{
    ZcEnv * const zc = av_mallocz(sizeof(ZcEnv));
    if (zc == NULL)
    {
        av_log(s, AV_LOG_ERROR, "ZC Init: Context allocation failed\n");
        return AVERROR(ENOMEM);
    }

    zc_pool_init(&zc->pool);
    zc->refs = 0;

    s->get_buffer_context = zc;
    s->get_buffer2 = av_rpi_zc_get_buffer2;
    return 0;
}

void av_rpi_zc_uninit(struct AVCodecContext * const s)
{
    if (s->get_buffer2 == av_rpi_zc_get_buffer2)
    {
        ZcEnv * const zc = s->get_buffer_context;
        s->get_buffer2 = avcodec_default_get_buffer2;
        s->get_buffer_context = NULL;

        zc_pool_destroy(&zc->pool); ;
        av_free(zc);
    }
}

#endif  // RPI

