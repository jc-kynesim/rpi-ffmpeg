#ifndef AVCODEC_V4L2_REQUEST_HEVC_H
#define AVCODEC_V4L2_REQUEST_HEVC_H

#include <stdint.h>
#include <drm_fourcc.h>
#include "v4l2_req_decode_q.h"

#ifndef DRM_FORMAT_NV15
#define DRM_FORMAT_NV15 fourcc_code('N', 'V', '1', '5')
#endif

#ifndef DRM_FORMAT_NV20
#define DRM_FORMAT_NV20 fourcc_code('N', 'V', '2', '0')
#endif

// P030 should be defined in drm_fourcc.h and hopefully will be sometime
// in the future but until then...
#ifndef DRM_FORMAT_P030
#define DRM_FORMAT_P030 fourcc_code('P', '0', '3', '0')
#endif

#ifndef DRM_FORMAT_NV15
#define DRM_FORMAT_NV15 fourcc_code('N', 'V', '1', '5')
#endif

#ifndef DRM_FORMAT_NV20
#define DRM_FORMAT_NV20 fourcc_code('N', 'V', '2', '0')
#endif

#include <linux/videodev2.h>
#ifndef V4L2_CID_CODEC_BASE
#define V4L2_CID_CODEC_BASE V4L2_CID_MPEG_BASE
#endif

// V4L2_PIX_FMT_NV12_10_COL128 and V4L2_PIX_FMT_NV12_COL128 should be defined
// in drm_fourcc.h hopefully will be sometime in the future but until then...
#ifndef V4L2_PIX_FMT_NV12_10_COL128
#define V4L2_PIX_FMT_NV12_10_COL128 v4l2_fourcc('N', 'C', '3', '0')
#endif

#ifndef V4L2_PIX_FMT_NV12_COL128
#define V4L2_PIX_FMT_NV12_COL128 v4l2_fourcc('N', 'C', '1', '2') /* 12  Y/CbCr 4:2:0 128 pixel wide column */
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
} V4L2RequestContextHEVC;

typedef struct v4l2_req_decode_fns {
    int src_pix_fmt_v4l2;
    const char * name;

    // Init setup
    int (*probe)(AVCodecContext * const avctx, V4L2RequestContextHEVC * const ctx);
    int (*set_controls)(AVCodecContext * const avctx, V4L2RequestContextHEVC * const ctx);

    // Passthrough of hwaccel fns
    int (*start_frame)(AVCodecContext *avctx, const uint8_t *buf, uint32_t buf_size);
    int (*decode_slice)(AVCodecContext *avctx, const uint8_t *buf, uint32_t buf_size);
    int (*end_frame)(AVCodecContext *avctx);
    void (*abort_frame)(AVCodecContext *avctx);
    int (*frame_params)(AVCodecContext *avctx, AVBufferRef *hw_frames_ctx);
    int (*alloc_frame)(AVCodecContext * avctx, AVFrame *frame);
} v4l2_req_decode_fns;


extern const v4l2_req_decode_fns V2(ff_v4l2_req_hevc, 1);
extern const v4l2_req_decode_fns V2(ff_v4l2_req_hevc, 2);
extern const v4l2_req_decode_fns V2(ff_v4l2_req_hevc, 3);
extern const v4l2_req_decode_fns V2(ff_v4l2_req_hevc, 4);

#endif
