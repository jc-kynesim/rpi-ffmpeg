/*
    Copyright (C) 2024  John Cox john.cox@raspberrypi.com

    Permission is hereby granted, free of charge, to any person
    obtaining a copy of this software and associated documentation
    files (the "Software"), to deal in the Software without
    restriction, including without limitation the rights to use, copy,
    modify, merge, publish, distribute, sublicense, and/or sell copies
    of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be
    included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
    HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
    WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
 */

#ifndef AVCODEC_V4L2_REQUEST_HEVC_H
#define AVCODEC_V4L2_REQUEST_HEVC_H

#include <stdint.h>
#include "refstruct.h"
#include "libavutil/fourcc_drm.h"
#include "v4l2_req_decode_q.h"

#include "libavutil/fourcc_v4l2.h"
#include <linux/videodev2.h>
#ifndef V4L2_CID_CODEC_BASE
#define V4L2_CID_CODEC_BASE V4L2_CID_MPEG_BASE
#endif

#ifndef V4L2_CTRL_FLAG_DYNAMIC_ARRAY
#define V4L2_CTRL_FLAG_DYNAMIC_ARRAY	0x0800
#endif

#define VCAT(name, version) name##_v##version
#define V2(n,v) VCAT(n, v)
#define V(n) V2(n, HEVC_CTRLS_VERSION)

#define S2(x) #x
#define STR(x) S2(x)

// 1 per decoder
struct v4l2_req_decode_fns;

typedef struct V4L2RequestContextHEVC {
//    V4L2RequestContext base;
    const struct v4l2_req_decode_fns * fns;

    unsigned int timestamp;  // ?? maybe uint64_t

    int decode_mode;
    int start_code;
    unsigned int max_slices;    // 0 => not wanted (frame mode)
    unsigned int max_offsets;   // 0 => not wanted

    req_decode_q decode_q;

    struct devscan *devscan;
    struct dmabufs_ctl *dbufs;
    struct pollqueue *pq;
    struct media_pool * mpool;
    struct mediabufs_ctl *mbufs;

    void * decode_ctx;
} V4L2RequestContextHEVC;

typedef struct V4L2RequestPrivHEVC {
    V4L2RequestContextHEVC * cctx;  // Common context
    AVBufferRef * cctx_buf;         // Buf for cctx
    int bit_depth;
} V4L2RequestPrivHEVC;

typedef struct v4l2_req_decode_fns {
    int src_pix_fmt_v4l2;
    const char * name;

    // Init setup
    int (*probe)(AVCodecContext * const avctx, V4L2RequestContextHEVC * const ctx);
    // Set controls & any other init (e.g. decode_ctx)
    int (*set_controls)(AVCodecContext * const avctx, V4L2RequestContextHEVC * const ctx);

    // Called on shutdown - avctx may not exist
    void (*uninit)(V4L2RequestContextHEVC * const ctx);

    // Passthrough of hwaccel fns
    int (*start_frame)(AVCodecContext *avctx, V4L2RequestContextHEVC *const ctx, const uint8_t *buf, uint32_t buf_size);
    int (*decode_slice)(AVCodecContext *avctx, V4L2RequestContextHEVC *const ctx, const uint8_t *buf, uint32_t buf_size);
    int (*end_frame)(AVCodecContext *avctx, V4L2RequestContextHEVC *const ctx);
    void (*abort_frame)(AVCodecContext *avctx, V4L2RequestContextHEVC *const ctx);
    int (*frame_params)(AVCodecContext *avctx, V4L2RequestContextHEVC *const ctx, AVBufferRef *hw_frames_ctx);
    int (*alloc_frame)(AVCodecContext * avctx, V4L2RequestContextHEVC *const ctx, AVFrame *frame);
} v4l2_req_decode_fns;

int ff_v4l2_request_start_frame(AVCodecContext *avctx, const uint8_t *buffer, uint32_t size);
int ff_v4l2_request_decode_slice(AVCodecContext *avctx, const uint8_t *buffer, uint32_t size);
int ff_v4l2_request_end_frame(AVCodecContext *avctx);
void ff_v4l2_request_abort_frame(AVCodecContext * const avctx);
int ff_v4l2_request_frame_params(AVCodecContext *avctx, AVBufferRef *hw_frames_ctx);
int ff_v4l2_request_alloc_frame(AVCodecContext * avctx, AVFrame *frame);
int ff_v4l2_request_init(AVCodecContext *avctx,
                         const struct v4l2_req_decode_fns * const * const try_fns,
                         const int width, const int height, const int bit_depth,
                         const int dst_buffers);
int ff_v4l2_request_uninit(AVCodecContext *avctx);
int ff_v4l2_request_update_thread_context(AVCodecContext *dst, const AVCodecContext *src);
void ff_v4l2_request_free_frame_priv(FFRefStructOpaque hwctx, void *data);

extern const v4l2_req_decode_fns V2(ff_v4l2_req_hevc, 1);
extern const v4l2_req_decode_fns V2(ff_v4l2_req_hevc, 2);
extern const v4l2_req_decode_fns V2(ff_v4l2_req_hevc, 3);
extern const v4l2_req_decode_fns V2(ff_v4l2_req_hevc, 4);

#endif
