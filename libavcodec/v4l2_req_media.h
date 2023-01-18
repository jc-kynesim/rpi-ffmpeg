/*
e.h
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

#ifndef _MEDIA_H_
#define _MEDIA_H_

#include <stdbool.h>
#include <stdint.h>

struct v4l2_format;
struct v4l2_fmtdesc;
struct v4l2_query_ext_ctrl;

struct pollqueue;
struct media_request;
struct media_pool;

typedef enum media_buf_status {
    MEDIABUFS_STATUS_SUCCESS = 0,
    MEDIABUFS_ERROR_OPERATION_FAILED,
    MEDIABUFS_ERROR_DECODING_ERROR,
    MEDIABUFS_ERROR_UNSUPPORTED_BUFFERTYPE,
    MEDIABUFS_ERROR_UNSUPPORTED_RT_FORMAT,
    MEDIABUFS_ERROR_ALLOCATION_FAILED,
    MEDIABUFS_ERROR_UNSUPPORTED_MEMORY,
} MediaBufsStatus;

struct media_pool * media_pool_new(const char * const media_path,
                   struct pollqueue * const pq,
                   const unsigned int n);
void media_pool_delete(struct media_pool ** pmp);

// Obtain a media request
// Will block if none availible - has a 2sec timeout
struct media_request * media_request_get(struct media_pool * const mp);
int media_request_fd(const struct media_request * const req);

// Start this request
// Request structure is returned to pool once done
int media_request_start(struct media_request * const req);

// Return an *unstarted* media_request to the pool
// May later be upgraded to allow for aborting a started req
int media_request_abort(struct media_request ** const preq);


struct mediabufs_ctl;
struct qent_src;
struct qent_dst;
struct dmabuf_h;
struct dmabufs_ctl;

// 1-1 mammping to V4L2 type - just defined separetely to avoid some include versioning difficulties
enum mediabufs_memory {
   MEDIABUFS_MEMORY_UNSET            = 0,
   MEDIABUFS_MEMORY_MMAP             = 1,
   MEDIABUFS_MEMORY_USERPTR          = 2,
   MEDIABUFS_MEMORY_OVERLAY          = 3,
   MEDIABUFS_MEMORY_DMABUF           = 4,
};

int qent_src_params_set(struct qent_src *const be, const struct timeval * timestamp);
struct timeval qent_dst_timestamp_get(const struct qent_dst *const be_dst);

// prealloc
int qent_src_alloc(struct qent_src *const be_src, const size_t len, struct dmabufs_ctl * dbsc);
// dbsc may be NULL if realloc not required
int qent_src_data_copy(struct qent_src *const be_src, const size_t offset, const void *const src, const size_t len, struct dmabufs_ctl * dbsc);
const struct dmabuf_h * qent_dst_dmabuf(const struct qent_dst *const be, unsigned int plane);
int qent_dst_dup_fd(const struct qent_dst *const be, unsigned int plane);
MediaBufsStatus qent_dst_wait(struct qent_dst *const be);
void qent_dst_delete(struct qent_dst *const be);
// Returns a qent_dst to its mbc free Q or deletes it if the mbc is dead
void qent_dst_unref(struct qent_dst ** const pbe_dst);
struct qent_dst * qent_dst_ref(struct qent_dst * const be_dst);

const uint8_t * qent_dst_data(struct qent_dst *const be, unsigned int buf_no);
MediaBufsStatus qent_dst_read_start(struct qent_dst *const be);
MediaBufsStatus qent_dst_read_stop(struct qent_dst *const be);
/* Import an fd unattached to any mediabuf */
MediaBufsStatus qent_dst_import_fd(struct qent_dst *const be_dst,
                unsigned int plane,
                int fd, size_t size);

const char * mediabufs_memory_name(const enum mediabufs_memory m);

MediaBufsStatus mediabufs_start_request(struct mediabufs_ctl *const mbc,
                struct media_request **const pmreq,
                struct qent_src **const psrc_be,
                struct qent_dst *const dst_be,
                const bool is_final);
// Get / alloc a dst buffer & associate with a slot
// If the dst pool is empty then behaviour depends on the fixed flag passed to
// dst_slots_create.  Default is !fixed = unlimited alloc
struct qent_dst* mediabufs_dst_qent_alloc(struct mediabufs_ctl *const mbc,
                           struct dmabufs_ctl *const dbsc);
// Create dst slots without alloc
// If fixed true then qent_alloc will only get slots from this pool and will
// block until a qent has been unrefed
MediaBufsStatus mediabufs_dst_slots_create(struct mediabufs_ctl *const mbc, const unsigned int n, const bool fixed, const enum mediabufs_memory memtype);

MediaBufsStatus mediabufs_stream_on(struct mediabufs_ctl *const mbc);
MediaBufsStatus mediabufs_stream_off(struct mediabufs_ctl *const mbc);
const struct v4l2_format *mediabufs_dst_fmt(struct mediabufs_ctl *const mbc);

typedef int mediabufs_dst_fmt_accept_fn(void * v, const struct v4l2_fmtdesc *fmtdesc);

MediaBufsStatus mediabufs_dst_fmt_set(struct mediabufs_ctl *const mbc,
               const unsigned int width,
               const unsigned int height,
               mediabufs_dst_fmt_accept_fn *const accept_fn,
               void *const accept_v);
struct qent_src *mediabufs_src_qent_get(struct mediabufs_ctl *const mbc);
void mediabufs_src_qent_abort(struct mediabufs_ctl *const mbc, struct qent_src **const pqe_src);

int mediabufs_ctl_set_ext_ctrls(struct mediabufs_ctl * mbc, struct media_request * const mreq,
                                struct v4l2_ext_control control_array[], unsigned int n);
MediaBufsStatus mediabufs_set_ext_ctrl(struct mediabufs_ctl *const mbc,
                struct media_request * const mreq,
                unsigned int id, void *data,
                unsigned int size);
int mediabufs_ctl_query_ext_ctrls(struct mediabufs_ctl * mbc, struct v4l2_query_ext_ctrl ctrls[], unsigned int n);

int mediabufs_src_resizable(const struct mediabufs_ctl *const mbc);

MediaBufsStatus mediabufs_src_fmt_set(struct mediabufs_ctl *const mbc,
                                      enum v4l2_buf_type buf_type,
                                      const uint32_t pixfmt,
                                      const uint32_t width, const uint32_t height,
                                      const size_t bufsize);

MediaBufsStatus mediabufs_src_pool_create(struct mediabufs_ctl *const rw,
                  struct dmabufs_ctl * const dbsc,
                  unsigned int n,
                  const enum mediabufs_memory memtype);

// Want to have appropriate formats set first
MediaBufsStatus mediabufs_src_chk_memtype(struct mediabufs_ctl *const mbc, const enum mediabufs_memory memtype);
MediaBufsStatus mediabufs_dst_chk_memtype(struct mediabufs_ctl *const mbc, const enum mediabufs_memory memtype);

#define MEDIABUFS_DRIVER_VERSION(a, b, c) (((a) << 16) | ((b) << 8) | (c))
unsigned int mediabufs_ctl_driver_version(struct mediabufs_ctl *const mbc);

struct mediabufs_ctl * mediabufs_ctl_new(void * const dc,
                     const char *vpath, struct pollqueue *const pq);
void mediabufs_ctl_unref(struct mediabufs_ctl **const pmbc);
struct mediabufs_ctl * mediabufs_ctl_ref(struct mediabufs_ctl *const mbc);


#endif
