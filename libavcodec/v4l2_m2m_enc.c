/*
 * V4L2 mem2mem encoders
 *
 * Copyright (C) 2017 Alexis Ballier <aballier@gentoo.org>
 * Copyright (C) 2017 Jorge Ramirez <jorge.ramirez-ortiz@linaro.org>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <search.h>
#include <drm_fourcc.h>

#include "encode.h"
#include "libavcodec/avcodec.h"
#include "libavutil/pixdesc.h"
#include "libavutil/pixfmt.h"
#include "libavutil/opt.h"
#include "codec_internal.h"
#include "profiles.h"
#include "v4l2_context.h"
#include "v4l2_m2m.h"
#include "v4l2_fmt.h"

#define MPEG_CID(x) V4L2_CID_MPEG_VIDEO_##x
#define MPEG_VIDEO(x) V4L2_MPEG_VIDEO_##x

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

#ifndef V4L2_CID_CODEC_BASE
#define V4L2_CID_CODEC_BASE V4L2_CID_MPEG_BASE
#endif

// V4L2_PIX_FMT_NV12_10_COL128 and V4L2_PIX_FMT_NV12_COL128 should be defined
// in videodev2.h hopefully will be sometime in the future but until then...
#ifndef V4L2_PIX_FMT_NV12_10_COL128
#define V4L2_PIX_FMT_NV12_10_COL128 v4l2_fourcc('N', 'C', '3', '0')
#endif

#ifndef V4L2_PIX_FMT_NV12_COL128
#define V4L2_PIX_FMT_NV12_COL128 v4l2_fourcc('N', 'C', '1', '2') /* 12  Y/CbCr 4:2:0 128 pixel wide column */
#endif

static inline void v4l2_set_timeperframe(V4L2m2mContext *s, unsigned int num, unsigned int den)
{
    struct v4l2_streamparm parm = { 0 };

    parm.type = V4L2_TYPE_IS_MULTIPLANAR(s->output.type) ? V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT;
    parm.parm.output.timeperframe.denominator = den;
    parm.parm.output.timeperframe.numerator = num;

    if (ioctl(s->fd, VIDIOC_S_PARM, &parm) < 0)
        av_log(s->avctx, AV_LOG_WARNING, "Failed to set timeperframe");
}

static inline void v4l2_set_ext_ctrl(V4L2m2mContext *s, unsigned int id, signed int value, const char *name, int log_warning)
{
    struct v4l2_ext_controls ctrls = { { 0 } };
    struct v4l2_ext_control ctrl = { 0 };

    /* set ctrls */
    ctrls.ctrl_class = V4L2_CTRL_CLASS_MPEG;
    ctrls.controls = &ctrl;
    ctrls.count = 1;

    /* set ctrl*/
    ctrl.value = value;
    ctrl.id = id;

    if (ioctl(s->fd, VIDIOC_S_EXT_CTRLS, &ctrls) < 0)
        av_log(s->avctx, log_warning || errno != EINVAL ? AV_LOG_WARNING : AV_LOG_DEBUG,
               "Failed to set %s: %s\n", name, strerror(errno));
    else
        av_log(s->avctx, AV_LOG_DEBUG, "Encoder: %s = %d\n", name, value);
}

static inline int v4l2_get_ext_ctrl(V4L2m2mContext *s, unsigned int id, signed int *value, const char *name, int log_warning)
{
    struct v4l2_ext_controls ctrls = { { 0 } };
    struct v4l2_ext_control ctrl = { 0 };
    int ret;

    /* set ctrls */
    ctrls.ctrl_class = V4L2_CTRL_CLASS_MPEG;
    ctrls.controls = &ctrl;
    ctrls.count = 1;

    /* set ctrl*/
    ctrl.id = id ;

    ret = ioctl(s->fd, VIDIOC_G_EXT_CTRLS, &ctrls);
    if (ret < 0) {
        av_log(s->avctx, log_warning || errno != EINVAL ? AV_LOG_WARNING : AV_LOG_DEBUG,
               "Failed to get %s\n", name);
        return ret;
    }

    *value = ctrl.value;

    return 0;
}

static inline unsigned int v4l2_h264_profile_from_ff(int p)
{
    static const struct h264_profile  {
        unsigned int ffmpeg_val;
        unsigned int v4l2_val;
    } profile[] = {
        { FF_PROFILE_H264_CONSTRAINED_BASELINE, MPEG_VIDEO(H264_PROFILE_CONSTRAINED_BASELINE) },
        { FF_PROFILE_H264_HIGH_444_PREDICTIVE, MPEG_VIDEO(H264_PROFILE_HIGH_444_PREDICTIVE) },
        { FF_PROFILE_H264_HIGH_422_INTRA, MPEG_VIDEO(H264_PROFILE_HIGH_422_INTRA) },
        { FF_PROFILE_H264_HIGH_444_INTRA, MPEG_VIDEO(H264_PROFILE_HIGH_444_INTRA) },
        { FF_PROFILE_H264_HIGH_10_INTRA, MPEG_VIDEO(H264_PROFILE_HIGH_10_INTRA) },
        { FF_PROFILE_H264_HIGH_422, MPEG_VIDEO(H264_PROFILE_HIGH_422) },
        { FF_PROFILE_H264_BASELINE, MPEG_VIDEO(H264_PROFILE_BASELINE) },
        { FF_PROFILE_H264_EXTENDED, MPEG_VIDEO(H264_PROFILE_EXTENDED) },
        { FF_PROFILE_H264_HIGH_10, MPEG_VIDEO(H264_PROFILE_HIGH_10) },
        { FF_PROFILE_H264_MAIN, MPEG_VIDEO(H264_PROFILE_MAIN) },
        { FF_PROFILE_H264_HIGH, MPEG_VIDEO(H264_PROFILE_HIGH) },
    };
    int i;

    for (i = 0; i < FF_ARRAY_ELEMS(profile); i++) {
        if (profile[i].ffmpeg_val == p)
            return profile[i].v4l2_val;
    }
    return AVERROR(ENOENT);
}

static inline int v4l2_mpeg4_profile_from_ff(int p)
{
    static const struct mpeg4_profile {
        unsigned int ffmpeg_val;
        unsigned int v4l2_val;
    } profile[] = {
        { FF_PROFILE_MPEG4_ADVANCED_CODING, MPEG_VIDEO(MPEG4_PROFILE_ADVANCED_CODING_EFFICIENCY) },
        { FF_PROFILE_MPEG4_ADVANCED_SIMPLE, MPEG_VIDEO(MPEG4_PROFILE_ADVANCED_SIMPLE) },
        { FF_PROFILE_MPEG4_SIMPLE_SCALABLE, MPEG_VIDEO(MPEG4_PROFILE_SIMPLE_SCALABLE) },
        { FF_PROFILE_MPEG4_SIMPLE, MPEG_VIDEO(MPEG4_PROFILE_SIMPLE) },
        { FF_PROFILE_MPEG4_CORE, MPEG_VIDEO(MPEG4_PROFILE_CORE) },
    };
    int i;

    for (i = 0; i < FF_ARRAY_ELEMS(profile); i++) {
        if (profile[i].ffmpeg_val == p)
            return profile[i].v4l2_val;
    }
    return AVERROR(ENOENT);
}

static int v4l2_check_b_frame_support(V4L2m2mContext *s)
{
    if (s->avctx->max_b_frames)
        av_log(s->avctx, AV_LOG_WARNING, "Encoder does not support %d b-frames yet\n", s->avctx->max_b_frames);

    v4l2_set_ext_ctrl(s, MPEG_CID(B_FRAMES), s->avctx->max_b_frames, "number of B-frames", 1);
    v4l2_get_ext_ctrl(s, MPEG_CID(B_FRAMES), &s->avctx->max_b_frames, "number of B-frames", 0);
    if (s->avctx->max_b_frames == 0)
        return 0;

    avpriv_report_missing_feature(s->avctx, "DTS/PTS calculation for V4L2 encoding");
    return AVERROR_PATCHWELCOME;
}

static inline void v4l2_subscribe_eos_event(V4L2m2mContext *s)
{
    struct v4l2_event_subscription sub;

    memset(&sub, 0, sizeof(sub));
    sub.type = V4L2_EVENT_EOS;
    if (ioctl(s->fd, VIDIOC_SUBSCRIBE_EVENT, &sub) < 0)
        av_log(s->avctx, AV_LOG_WARNING,
               "the v4l2 driver does not support end of stream VIDIOC_SUBSCRIBE_EVENT\n");
}

static int v4l2_prepare_encoder(V4L2m2mContext *s)
{
    AVCodecContext *avctx = s->avctx;
    int qmin_cid, qmax_cid, qmin, qmax;
    int ret, val;

    /**
     * requirements
     */
    v4l2_subscribe_eos_event(s);

    ret = v4l2_check_b_frame_support(s);
    if (ret)
        return ret;

    /**
     * settingss
     */
    if (avctx->framerate.num || avctx->framerate.den)
        v4l2_set_timeperframe(s, avctx->framerate.den, avctx->framerate.num);

    /* set ext ctrls */
    v4l2_set_ext_ctrl(s, MPEG_CID(HEADER_MODE), MPEG_VIDEO(HEADER_MODE_SEPARATE), "header mode", 0);
    v4l2_set_ext_ctrl(s, MPEG_CID(BITRATE) , avctx->bit_rate, "bit rate", 1);
    v4l2_set_ext_ctrl(s, MPEG_CID(FRAME_RC_ENABLE), 1, "frame level rate control", 0);
    v4l2_set_ext_ctrl(s, MPEG_CID(GOP_SIZE), avctx->gop_size,"gop size", 1);

    av_log(avctx, AV_LOG_DEBUG,
        "Encoder Context: id (%d), profile (%d), frame rate(%d/%d), number b-frames (%d), "
        "gop size (%d), bit rate (%"PRId64"), qmin (%d), qmax (%d)\n",
        avctx->codec_id, avctx->profile, avctx->framerate.num, avctx->framerate.den,
        avctx->max_b_frames, avctx->gop_size, avctx->bit_rate, avctx->qmin, avctx->qmax);

    switch (avctx->codec_id) {
    case AV_CODEC_ID_H264:
        if (avctx->profile != FF_PROFILE_UNKNOWN) {
            val = v4l2_h264_profile_from_ff(avctx->profile);
            if (val < 0)
                av_log(avctx, AV_LOG_WARNING, "h264 profile not found\n");
            else
                v4l2_set_ext_ctrl(s, MPEG_CID(H264_PROFILE), val, "h264 profile", 1);
        }
        qmin_cid = MPEG_CID(H264_MIN_QP);
        qmax_cid = MPEG_CID(H264_MAX_QP);
        qmin = 0;
        qmax = 51;
        break;
    case AV_CODEC_ID_MPEG4:
        if (avctx->profile != FF_PROFILE_UNKNOWN) {
            val = v4l2_mpeg4_profile_from_ff(avctx->profile);
            if (val < 0)
                av_log(avctx, AV_LOG_WARNING, "mpeg4 profile not found\n");
            else
                v4l2_set_ext_ctrl(s, MPEG_CID(MPEG4_PROFILE), val, "mpeg4 profile", 1);
        }
        qmin_cid = MPEG_CID(MPEG4_MIN_QP);
        qmax_cid = MPEG_CID(MPEG4_MAX_QP);
        if (avctx->flags & AV_CODEC_FLAG_QPEL)
            v4l2_set_ext_ctrl(s, MPEG_CID(MPEG4_QPEL), 1, "qpel", 1);
        qmin = 1;
        qmax = 31;
        break;
    case AV_CODEC_ID_H263:
        qmin_cid = MPEG_CID(H263_MIN_QP);
        qmax_cid = MPEG_CID(H263_MAX_QP);
        qmin = 1;
        qmax = 31;
        break;
    case AV_CODEC_ID_VP8:
        qmin_cid = MPEG_CID(VPX_MIN_QP);
        qmax_cid = MPEG_CID(VPX_MAX_QP);
        qmin = 0;
        qmax = 127;
        break;
    case AV_CODEC_ID_VP9:
        qmin_cid = MPEG_CID(VPX_MIN_QP);
        qmax_cid = MPEG_CID(VPX_MAX_QP);
        qmin = 0;
        qmax = 255;
        break;
    default:
        return 0;
    }

    if (avctx->qmin >= 0 && avctx->qmax >= 0 && avctx->qmin > avctx->qmax) {
        av_log(avctx, AV_LOG_WARNING, "Invalid qmin:%d qmax:%d. qmin should not "
                                      "exceed qmax\n", avctx->qmin, avctx->qmax);
    } else {
        qmin = avctx->qmin >= 0 ? avctx->qmin : qmin;
        qmax = avctx->qmax >= 0 ? avctx->qmax : qmax;
    }

    v4l2_set_ext_ctrl(s, qmin_cid, qmin, "minimum video quantizer scale",
                      avctx->qmin >= 0);
    v4l2_set_ext_ctrl(s, qmax_cid, qmax, "maximum video quantizer scale",
                      avctx->qmax >= 0);

    return 0;
}

static int avdrm_to_v4l2(struct v4l2_format * const format, const AVFrame * const frame)
{
    const AVDRMFrameDescriptor *const src = (const AVDRMFrameDescriptor *)frame->data[0];

    const uint32_t drm_fmt = src->layers[0].format;
    // Treat INVALID as LINEAR
    const uint64_t mod = src->objects[0].format_modifier == DRM_FORMAT_MOD_INVALID ?
        DRM_FORMAT_MOD_LINEAR : src->objects[0].format_modifier;
    uint32_t pix_fmt = 0;
    uint32_t w = 0;
    uint32_t h = 0;
    uint32_t bpl = src->layers[0].planes[0].pitch;

    // We really don't expect multiple layers
    // All formats that we currently cope with are single object

    if (src->nb_layers != 1 || src->nb_objects != 1)
        return AVERROR(EINVAL);

    switch (drm_fmt) {
        case DRM_FORMAT_YUV420:
            if (mod == DRM_FORMAT_MOD_LINEAR) {
                if (src->layers[0].nb_planes != 3)
                    break;
                pix_fmt = V4L2_PIX_FMT_YUV420;
                h = src->layers[0].planes[1].offset / bpl;
                w = bpl;
            }
            break;

        case DRM_FORMAT_NV12:
            if (mod == DRM_FORMAT_MOD_LINEAR) {
                if (src->layers[0].nb_planes != 2)
                    break;
                pix_fmt = V4L2_PIX_FMT_NV12;
                h = src->layers[0].planes[1].offset / bpl;
                w = bpl;
            }
            else if (fourcc_mod_broadcom_mod(mod) == DRM_FORMAT_MOD_BROADCOM_SAND128) {
                if (src->layers[0].nb_planes != 2)
                    break;
                pix_fmt = V4L2_PIX_FMT_NV12_COL128;
                w = bpl;
                h = src->layers[0].planes[1].offset / 128;
                bpl = fourcc_mod_broadcom_param(mod);
            }
            break;

        case DRM_FORMAT_P030:
            if (fourcc_mod_broadcom_mod(mod) == DRM_FORMAT_MOD_BROADCOM_SAND128) {
                if (src->layers[0].nb_planes != 2)
                    break;
                pix_fmt =  V4L2_PIX_FMT_NV12_10_COL128;
                w = bpl / 2;  // Matching lie to how we construct this
                h = src->layers[0].planes[1].offset / 128;
                bpl = fourcc_mod_broadcom_param(mod);
            }
            break;

        default:
            break;
    }

    if (!pix_fmt)
        return AVERROR(EINVAL);

    if (V4L2_TYPE_IS_MULTIPLANAR(format->type)) {
        struct v4l2_pix_format_mplane *const pix = &format->fmt.pix_mp;

        pix->width = w;
        pix->height = h;
        pix->pixelformat = pix_fmt;
        pix->plane_fmt[0].bytesperline = bpl;
        pix->num_planes = 1;
    }
    else {
        struct v4l2_pix_format *const pix = &format->fmt.pix;

        pix->width = w;
        pix->height = h;
        pix->pixelformat = pix_fmt;
        pix->bytesperline = bpl;
    }

    return 0;
}

// Do we have similar enough formats to be usable?
static int fmt_eq(const struct v4l2_format * const a, const struct v4l2_format * const b)
{
    if (a->type != b->type)
        return 0;

    if (V4L2_TYPE_IS_MULTIPLANAR(a->type)) {
        const struct v4l2_pix_format_mplane *const pa = &a->fmt.pix_mp;
        const struct v4l2_pix_format_mplane *const pb = &b->fmt.pix_mp;
        unsigned int i;
        if (pa->pixelformat != pb->pixelformat ||
            pa->num_planes != pb->num_planes)
            return 0;
        for (i = 0; i != pa->num_planes; ++i) {
            if (pa->plane_fmt[i].bytesperline != pb->plane_fmt[i].bytesperline)
                return 0;
        }
    }
    else {
        const struct v4l2_pix_format *const pa = &a->fmt.pix;
        const struct v4l2_pix_format *const pb = &b->fmt.pix;
        if (pa->pixelformat != pb->pixelformat ||
            pa->bytesperline != pb->bytesperline)
            return 0;
    }
    return 1;
}


static int v4l2_send_frame(AVCodecContext *avctx, const AVFrame *frame)
{
    V4L2m2mContext *s = ((V4L2m2mPriv*)avctx->priv_data)->context;
    V4L2Context *const output = &s->output;

    // Signal EOF if needed
    if (!frame) {
        return ff_v4l2_context_enqueue_frame(output, frame);
    }

    if (s->input_drm && !output->streamon) {
        int rv;
        struct v4l2_format req_format = {.type = output->format.type};

        // Set format when we first get a buffer
        if ((rv = avdrm_to_v4l2(&req_format, frame)) != 0) {
            av_log(avctx, AV_LOG_ERROR, "Failed to get V4L2 format from DRM_PRIME frame\n");
            return rv;
        }

        ff_v4l2_context_release(output);

        output->format = req_format;

        if ((rv = ff_v4l2_context_set_format(output)) != 0) {
            av_log(avctx, AV_LOG_ERROR, "Failed to set V4L2 format\n");
            return rv;
        }

        if (!fmt_eq(&req_format, &output->format)) {
            av_log(avctx, AV_LOG_ERROR, "Format mismatch after setup\n");
            return AVERROR(EINVAL);
        }

        output->selection.top = frame->crop_top;
        output->selection.left = frame->crop_left;
        output->selection.width = av_frame_cropped_width(frame);
        output->selection.height = av_frame_cropped_height(frame);

        if ((rv = ff_v4l2_context_init(output)) != 0) {
            av_log(avctx, AV_LOG_ERROR, "Failed to (re)init context\n");
            return rv;
        }

        {
            struct v4l2_selection selection = {
                .type = V4L2_BUF_TYPE_VIDEO_OUTPUT,
                .target = V4L2_SEL_TGT_CROP,
                .r = output->selection
            };
            if (ioctl(s->fd, VIDIOC_S_SELECTION, &selection) != 0) {
                av_log(avctx, AV_LOG_WARNING, "S_SELECTION (CROP) %dx%d @ %d,%d failed: %s\n",
                       selection.r.width, selection.r.height, selection.r.left, selection.r.top,
                       av_err2str(AVERROR(errno)));
            }
            av_log(avctx, AV_LOG_TRACE, "S_SELECTION (CROP) %dx%d @ %d,%d OK\n",
                   selection.r.width, selection.r.height, selection.r.left, selection.r.top);
        }
    }

#ifdef V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME
    if (frame->pict_type == AV_PICTURE_TYPE_I)
        v4l2_set_ext_ctrl(s, MPEG_CID(FORCE_KEY_FRAME), 0, "force key frame", 1);
#endif

    return ff_v4l2_context_enqueue_frame(output, frame);
}

static int v4l2_receive_packet(AVCodecContext *avctx, AVPacket *avpkt)
{
    V4L2m2mContext *s = ((V4L2m2mPriv*)avctx->priv_data)->context;
    V4L2Context *const capture = &s->capture;
    V4L2Context *const output = &s->output;
    AVFrame *frame = s->frame;
    int ret;

    if (s->draining)
        goto dequeue;

    if (!frame->buf[0]) {
        ret = ff_encode_get_frame(avctx, frame);
        if (ret < 0 && ret != AVERROR_EOF)
            return ret;

        if (ret == AVERROR_EOF)
            frame = NULL;
    }

    ret = v4l2_send_frame(avctx, frame);
    if (ret != AVERROR(EAGAIN))
        av_frame_unref(frame);

    if (ret < 0 && ret != AVERROR(EAGAIN))
        return ret;

    if (!output->streamon) {
        ret = ff_v4l2_context_set_status(output, VIDIOC_STREAMON);
        if (ret) {
            av_log(avctx, AV_LOG_ERROR, "VIDIOC_STREAMON failed on output context\n");
            return ret;
        }
    }

    if (!capture->streamon) {
        ret = ff_v4l2_context_set_status(capture, VIDIOC_STREAMON);
        if (ret) {
            av_log(avctx, AV_LOG_ERROR, "VIDIOC_STREAMON failed on capture context\n");
            return ret;
        }
    }

dequeue:
    if ((ret = ff_v4l2_context_dequeue_packet(capture, avpkt)) != 0)
        return ret;

    if (capture->first_buf == 1) {
        uint8_t * data;
        const int len = avpkt->size;

        // 1st buffer after streamon should be SPS/PPS
        capture->first_buf = 2;

        // Clear both possible stores so there is no chance of confusion
        av_freep(&s->extdata_data);
        s->extdata_size = 0;
        av_freep(&avctx->extradata);
        avctx->extradata_size = 0;

        if ((data = av_malloc(len + AV_INPUT_BUFFER_PADDING_SIZE)) == NULL)
            goto fail_no_mem;

        memcpy(data, avpkt->data, len);
        av_packet_unref(avpkt);

        // We need to copy the header, but keep local if not global
        if ((avctx->flags & AV_CODEC_FLAG_GLOBAL_HEADER) != 0) {
            avctx->extradata = data;
            avctx->extradata_size = len;
        }
        else {
            s->extdata_data = data;
            s->extdata_size = len;
        }

        if ((ret = ff_v4l2_context_dequeue_packet(capture, avpkt)) != 0)
            return ret;
    }

    // First frame must be key so mark as such even if encoder forgot
    if (capture->first_buf == 2) {
        avpkt->flags |= AV_PKT_FLAG_KEY;

        // Add any extradata to the 1st packet we emit as we cannot create it at init
        if (avctx->extradata_size > 0 && avctx->extradata) {
            void * const side = av_packet_new_side_data(avpkt,
                                           AV_PKT_DATA_NEW_EXTRADATA,
                                           avctx->extradata_size);
            if (!side)
                goto fail_no_mem;

            memcpy(side, avctx->extradata, avctx->extradata_size);
        }
    }

    // Add SPS/PPS to the start of every key frame if non-global headers
    if ((avpkt->flags & AV_PKT_FLAG_KEY) != 0 && s->extdata_size != 0) {
        const size_t newlen = s->extdata_size + avpkt->size;
        AVBufferRef * const buf = av_buffer_alloc(newlen + AV_INPUT_BUFFER_PADDING_SIZE);

        if (buf == NULL)
            goto fail_no_mem;

        memcpy(buf->data, s->extdata_data, s->extdata_size);
        memcpy(buf->data + s->extdata_size, avpkt->data, avpkt->size);

        av_buffer_unref(&avpkt->buf);
        avpkt->buf = buf;
        avpkt->data = buf->data;
        avpkt->size = newlen;
    }

//    av_log(avctx, AV_LOG_INFO, "%s: PTS out=%"PRId64", size=%d, ret=%d\n", __func__, avpkt->pts, avpkt->size, ret);
    capture->first_buf = 0;
    return 0;

fail_no_mem:
    ret = AVERROR(ENOMEM);
    av_packet_unref(avpkt);
    return ret;
}

static av_cold int v4l2_encode_init(AVCodecContext *avctx)
{
    V4L2Context *capture, *output;
    V4L2m2mContext *s;
    V4L2m2mPriv *priv = avctx->priv_data;
    enum AVPixelFormat pix_fmt_output;
    uint32_t v4l2_fmt_output;
    int ret;

    av_log(avctx, AV_LOG_INFO, " <<< %s: fmt=%d/%d\n", __func__, avctx->pix_fmt, avctx->sw_pix_fmt);

    ret = ff_v4l2_m2m_create_context(priv, &s);
    if (ret < 0)
        return ret;

    capture = &s->capture;
    output  = &s->output;

    s->input_drm = (avctx->pix_fmt == AV_PIX_FMT_DRM_PRIME);

    /* common settings output/capture */
    output->height = capture->height = avctx->height;
    output->width = capture->width = avctx->width;

    /* output context */
    output->av_codec_id = AV_CODEC_ID_RAWVIDEO;
    output->av_pix_fmt = !s->input_drm ? avctx->pix_fmt :
            avctx->sw_pix_fmt != AV_PIX_FMT_NONE ? avctx->sw_pix_fmt :
            AV_PIX_FMT_YUV420P;

    /* capture context */
    capture->av_codec_id = avctx->codec_id;
    capture->av_pix_fmt = AV_PIX_FMT_NONE;

    s->avctx = avctx;
    ret = ff_v4l2_m2m_codec_init(priv);
    if (ret) {
        av_log(avctx, AV_LOG_ERROR, "can't configure encoder\n");
        return ret;
    }

    if (V4L2_TYPE_IS_MULTIPLANAR(output->type))
        v4l2_fmt_output = output->format.fmt.pix_mp.pixelformat;
    else
        v4l2_fmt_output = output->format.fmt.pix.pixelformat;

    pix_fmt_output = ff_v4l2_format_v4l2_to_avfmt(v4l2_fmt_output, AV_CODEC_ID_RAWVIDEO);
    if (!s->input_drm && pix_fmt_output != avctx->pix_fmt) {
        const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(pix_fmt_output);
        av_log(avctx, AV_LOG_ERROR, "Encoder requires %s pixel format.\n", desc->name);
        return AVERROR(EINVAL);
    }

    return v4l2_prepare_encoder(s);
}

static av_cold int v4l2_encode_close(AVCodecContext *avctx)
{
    return ff_v4l2_m2m_codec_end(avctx->priv_data);
}

#define OFFSET(x) offsetof(V4L2m2mPriv, x)
#define FLAGS AV_OPT_FLAG_VIDEO_PARAM | AV_OPT_FLAG_ENCODING_PARAM

#define V4L_M2M_CAPTURE_OPTS \
    { "num_output_buffers", "Number of buffers in the output context",\
        OFFSET(num_output_buffers), AV_OPT_TYPE_INT, { .i64 = 0 }, 0, INT_MAX, FLAGS },\
    { "num_capture_buffers", "Number of buffers in the capture context", \
        OFFSET(num_capture_buffers), AV_OPT_TYPE_INT, {.i64 = 8 }, 8, INT_MAX, FLAGS }

static const AVOption mpeg4_options[] = {
    V4L_M2M_CAPTURE_OPTS,
    FF_MPEG4_PROFILE_OPTS
    { NULL },
};

static const AVOption options[] = {
    V4L_M2M_CAPTURE_OPTS,
    { NULL },
};

static const FFCodecDefault v4l2_m2m_defaults[] = {
    { "qmin", "-1" },
    { "qmax", "-1" },
    { NULL },
};

#define M2MENC_CLASS(NAME, OPTIONS_NAME) \
    static const AVClass v4l2_m2m_ ## NAME ## _enc_class = { \
        .class_name = #NAME "_v4l2m2m_encoder", \
        .item_name  = av_default_item_name, \
        .option     = OPTIONS_NAME, \
        .version    = LIBAVUTIL_VERSION_INT, \
    };

#define M2MENC(NAME, LONGNAME, OPTIONS_NAME, CODEC) \
    M2MENC_CLASS(NAME, OPTIONS_NAME) \
    const FFCodec ff_ ## NAME ## _v4l2m2m_encoder = { \
        .p.name         = #NAME "_v4l2m2m" , \
        CODEC_LONG_NAME("V4L2 mem2mem " LONGNAME " encoder wrapper"), \
        .p.type         = AVMEDIA_TYPE_VIDEO, \
        .p.id           = CODEC , \
        .priv_data_size = sizeof(V4L2m2mPriv), \
        .p.priv_class   = &v4l2_m2m_ ## NAME ##_enc_class, \
        .init           = v4l2_encode_init, \
        FF_CODEC_RECEIVE_PACKET_CB(v4l2_receive_packet), \
        .close          = v4l2_encode_close, \
        .defaults       = v4l2_m2m_defaults, \
        .p.capabilities = AV_CODEC_CAP_HARDWARE | AV_CODEC_CAP_DELAY, \
        .caps_internal  = FF_CODEC_CAP_NOT_INIT_THREADSAFE | \
                          FF_CODEC_CAP_INIT_CLEANUP, \
        .p.wrapper_name = "v4l2m2m", \
    }

M2MENC(mpeg4,"MPEG4", mpeg4_options, AV_CODEC_ID_MPEG4);
M2MENC(h263, "H.263", options,       AV_CODEC_ID_H263);
M2MENC(h264, "H.264", options,       AV_CODEC_ID_H264);
M2MENC(hevc, "HEVC",  options,       AV_CODEC_ID_HEVC);
M2MENC(vp8,  "VP8",   options,       AV_CODEC_ID_VP8);
