/*
 * V4L2 mem2mem decoders
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

#include "libavutil/hwcontext.h"
#include "libavutil/hwcontext_drm.h"
#include "libavutil/pixfmt.h"
#include "libavutil/pixdesc.h"
#include "libavutil/opt.h"
#include "libavcodec/avcodec.h"
#include "libavcodec/decode.h"
#include "libavcodec/internal.h"

#include "libavcodec/hwaccels.h"
#include "libavcodec/internal.h"
#include "libavcodec/hwconfig.h"

#include "v4l2_context.h"
#include "v4l2_m2m.h"
#include "v4l2_fmt.h"

static int check_output_streamon(AVCodecContext *const avctx, V4L2m2mContext *const s)
{
    int ret;
    struct v4l2_decoder_cmd cmd = {
        .cmd = V4L2_DEC_CMD_START,
        .flags = 0,
    };

    if (s->output.streamon)
        return 0;

    ret = ff_v4l2_context_set_status(&s->output, VIDIOC_STREAMON);
    if (ret < 0)
        av_log(avctx, AV_LOG_ERROR, "VIDIOC_STREAMON on output context\n");

    if (!s->capture.streamon || ret < 0)
        return ret;

    ret = ioctl(s->fd, VIDIOC_DECODER_CMD, &cmd);
    if (ret < 0)
        av_log(avctx, AV_LOG_ERROR, "VIDIOC_DECODER_CMD start error: %d\n", errno);
    return ret;
}

static int v4l2_try_start(AVCodecContext *avctx)
{
    V4L2m2mContext *s = ((V4L2m2mPriv*)avctx->priv_data)->context;
    V4L2Context *const capture = &s->capture;
    struct v4l2_selection selection = { 0 };
    int ret;

    /* 1. start the output process */
    if ((ret = check_output_streamon(avctx, s)) != 0)
        return ret;

    if (capture->streamon)
        return 0;

    /* 2. get the capture format */
    capture->format.type = capture->type;
    ret = ioctl(s->fd, VIDIOC_G_FMT, &capture->format);
    if (ret) {
        av_log(avctx, AV_LOG_WARNING, "VIDIOC_G_FMT ioctl\n");
        return ret;
    }

    /* 2.1 update the AVCodecContext */
    capture->av_pix_fmt =
        ff_v4l2_format_v4l2_to_avfmt(capture->format.fmt.pix_mp.pixelformat, AV_CODEC_ID_RAWVIDEO);
    if (s->output_drm) {
        avctx->pix_fmt = AV_PIX_FMT_DRM_PRIME;
        avctx->sw_pix_fmt = capture->av_pix_fmt;
    }
    else
        avctx->pix_fmt = capture->av_pix_fmt;

    /* 3. set the crop parameters */
    selection.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    selection.r.height = avctx->coded_height;
    selection.r.width = avctx->coded_width;
    ret = ioctl(s->fd, VIDIOC_S_SELECTION, &selection);
    if (!ret) {
        ret = ioctl(s->fd, VIDIOC_G_SELECTION, &selection);
        if (ret) {
            av_log(avctx, AV_LOG_WARNING, "VIDIOC_G_SELECTION ioctl\n");
        } else {
            av_log(avctx, AV_LOG_DEBUG, "crop output %dx%d\n", selection.r.width, selection.r.height);
            /* update the size of the resulting frame */
            capture->height = selection.r.height;
            capture->width  = selection.r.width;
        }
    }

    /* 4. init the capture context now that we have the capture format */
    if (!capture->buffers) {
        ret = ff_v4l2_context_init(capture);
        if (ret) {
            av_log(avctx, AV_LOG_ERROR, "can't request capture buffers\n");
            return AVERROR(ENOMEM);
        }
    }

    /* 5. start the capture process */
    ret = ff_v4l2_context_set_status(capture, VIDIOC_STREAMON);
    if (ret) {
        av_log(avctx, AV_LOG_DEBUG, "VIDIOC_STREAMON, on capture context\n");
        return ret;
    }

    return 0;
}

static int v4l2_prepare_decoder(V4L2m2mContext *s)
{
    struct v4l2_event_subscription sub;
    V4L2Context *output = &s->output;
    int ret;

    /**
     * requirements
     */
    memset(&sub, 0, sizeof(sub));
    sub.type = V4L2_EVENT_SOURCE_CHANGE;
    ret = ioctl(s->fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
    if ( ret < 0) {
        if (output->height == 0 || output->width == 0) {
            av_log(s->avctx, AV_LOG_ERROR,
                "the v4l2 driver does not support VIDIOC_SUBSCRIBE_EVENT\n"
                "you must provide codec_height and codec_width on input\n");
            return ret;
        }
    }

    memset(&sub, 0, sizeof(sub));
    sub.type = V4L2_EVENT_EOS;
    ret = ioctl(s->fd, VIDIOC_SUBSCRIBE_EVENT, &sub);
    if (ret < 0)
        av_log(s->avctx, AV_LOG_WARNING,
               "the v4l2 driver does not support end of stream VIDIOC_SUBSCRIBE_EVENT\n");

    return 0;
}

#define XLAT_PTS 1

static inline int64_t track_to_pts(AVCodecContext *avctx, unsigned int n)
{
    const AVRational t = avctx->pkt_timebase.num ? avctx->pkt_timebase : avctx->time_base;
    return !t.num || !t.den ? (int64_t)n * 1000000 : ((int64_t)n * t.den) / (t.num);
}

static inline unsigned int pts_to_track(AVCodecContext *avctx, const int64_t pts)
{
    const AVRational t = avctx->pkt_timebase.num ? avctx->pkt_timebase : avctx->time_base;
    return (unsigned int)(!t.num || !t.den ? pts / 1000000 : (pts * t.num) / t.den);
}

static void
xlat_pts_in(AVCodecContext *const avctx, V4L2m2mContext *const s, AVPacket *const avpkt)
{
#if XLAT_PTS
    int64_t track_pts;

    // Avoid 0
    if (++s->track_no == 0)
        s->track_no = 1;

    track_pts = track_to_pts(avctx, s->track_no);

    av_log(avctx, AV_LOG_TRACE, "In PTS=%" PRId64 ", DTS=%" PRId64 ", track=%" PRId64 ", n=%u\n", avpkt->pts, avpkt->dts, track_pts, s->track_no);
    s->last_pkt_dts = avpkt->dts;
    s->track_els[s->track_no  % FF_V4L2_M2M_TRACK_SIZE] = (V4L2m2mTrackEl){
        .discard          = 0,
        .pkt_size         = avpkt->size,
        .pts              = avpkt->pts,
        .reordered_opaque = avctx->reordered_opaque,
        .pkt_pos          = avpkt->pos,
        .pkt_duration     = avpkt->duration,
        .track_pts        = track_pts
    };
    avpkt->pts = track_pts;
#endif
}

// Returns -1 if we should discard the frame
static int
xlat_pts_out(AVCodecContext *const avctx, V4L2m2mContext *const s, AVFrame *const frame)
{
#if XLAT_PTS
    unsigned int n = pts_to_track(avctx, frame->pts) % FF_V4L2_M2M_TRACK_SIZE;
    const V4L2m2mTrackEl *const t = s->track_els + n;
    if (frame->pts == AV_NOPTS_VALUE || frame->pts != t->track_pts)
    {
        av_log(avctx, AV_LOG_INFO, "Tracking failure: pts=%" PRId64 ", track[%d]=%" PRId64 "\n", frame->pts, n, t->track_pts);
        frame->pts              = AV_NOPTS_VALUE;
        frame->pkt_dts          = s->last_pkt_dts;
        frame->reordered_opaque = s->last_opaque;
        frame->pkt_pos          = -1;
        frame->pkt_duration     = 0;
        frame->pkt_size         = -1;
    }
    else if (!t->discard)
    {
        frame->pts              = t->pts;
        frame->pkt_dts          = s->last_pkt_dts;
        frame->reordered_opaque = t->reordered_opaque;
        frame->pkt_pos          = t->pkt_pos;
        frame->pkt_duration     = t->pkt_duration;
        frame->pkt_size         = t->pkt_size;

        s->last_opaque = s->track_els[n].reordered_opaque;
        s->track_els[n].pts = AV_NOPTS_VALUE;  // If we hit this again deny accurate knowledge of PTS
    }
    else
    {
        av_log(avctx, AV_LOG_DEBUG, "Discard frame (flushed): pts=%" PRId64 ", track[%d]=%" PRId64 "\n", frame->pts, n, t->track_pts);
        return -1;
    }

#if FF_API_PKT_PTS
FF_DISABLE_DEPRECATION_WARNINGS
    frame->pkt_pts = frame->pts;
FF_ENABLE_DEPRECATION_WARNINGS
#endif
    frame->best_effort_timestamp = frame->pts;
    frame->pkt_dts               = frame->pts;  // We can't emulate what s/w does in a useful manner?
    av_log(avctx, AV_LOG_TRACE, "Out PTS=%" PRId64 ", DTS=%" PRId64 "\n", frame->pts, frame->pkt_dts);
#endif
    return 0;
}

static inline int stream_started(const V4L2m2mContext * const s) {
    return s->capture.streamon && s->output.streamon;
}


// -ve  Error
// 0    OK
// 1    Dst full (retry if we think V4L2 Q has space now)
// 2    Src empty (do not retry)
// 3    Start failure (do not retry)

static int try_enqueue_src(AVCodecContext * const avctx, V4L2m2mContext * const s)
{
    AVPacket avpkt = {0};
    int ret = 0;
    int ret2 = 0;

    if (s->buf_pkt.size) {
        av_packet_move_ref(&avpkt, &s->buf_pkt);
    } else {
        ret = ff_decode_get_packet(avctx, &avpkt);
        if (ret == AVERROR(EAGAIN))
            return 2;

        if (ret == AVERROR_EOF || avpkt.size == 0) {
            // EOF - enter drain mode
            av_log(avctx, AV_LOG_TRACE, "--- EOS req: ret=%d, size=%d, started=%d, drain=%d\n", ret, avpkt.size, stream_started(s), s->draining);
            if (stream_started(s) && !s->draining) {
                // On the offchance that get_packet left something that needs freeing in here
                av_packet_unref(&avpkt);
                // Calling enqueue with an empty pkt starts drain
                ret = ff_v4l2_context_enqueue_packet(&s->output, &avpkt, NULL, 0);
                if (ret) {
                    av_log(avctx, AV_LOG_ERROR, "Failed to start drain: ret=%d\n", ret);
                    return ret;
                }
            }
            return 2;
        }

        if (ret < 0)
            return ret;

        xlat_pts_in(avctx, s, &avpkt);
    }

    if ((ret = check_output_streamon(avctx, s)) != 0)
        return ret;

    ret = ff_v4l2_context_enqueue_packet(&s->output, &avpkt,
                                         avctx->extradata, s->extdata_sent ? 0 : avctx->extradata_size);
    s->extdata_sent = 1;

    if (ret == AVERROR(EAGAIN)) {
        // Out of input buffers - stash
        av_packet_move_ref(&s->buf_pkt, &avpkt);
        ret = 1;
    }
    else {
        // In all other cases we are done with this packet
        av_packet_unref(&avpkt);

        if (ret) {
            av_log(avctx, AV_LOG_ERROR, "Packet enqueue failure: err=%d\n", ret);
            return ret;
        }
    }

    // Start if we haven't
    ret2 = v4l2_try_start(avctx);
    if (ret2) {
        av_log(avctx, AV_LOG_DEBUG, "Start failure: err=%d\n", ret2);
        ret = (ret2 == AVERROR(ENOMEM)) ? ret2 : 3;
    }

    return ret;
}

static int v4l2_receive_frame(AVCodecContext *avctx, AVFrame *frame)
{
#if 1
    V4L2m2mContext *const s = ((V4L2m2mPriv*)avctx->priv_data)->context;
    int src_rv;
    int dst_rv = 1;

    do {
        src_rv = try_enqueue_src(avctx, s);

        if (src_rv < 0) {
            av_log(avctx, AV_LOG_ERROR, "Packet enqueue failure: err=%d\n", src_rv);
        }

        if (s->req_pkt && src_rv == 2 && !s->draining)
            break;

        if (src_rv == 1 && dst_rv == AVERROR(EAGAIN)) {
            av_log(avctx, AV_LOG_WARNING, "Poll says src Q has space but enqueue fail");
            src_rv = 2;
        }

        if (src_rv >= 0 && src_rv <= 2 && dst_rv != 0) {
            do {
                // Dequeue frame will unref any previous contents of frame
                // so we don't need an explicit unref when discarding
                dst_rv = ff_v4l2_context_dequeue_frame(&s->capture, frame, -1);

                if (dst_rv < 0) {
                    av_log(avctx, AV_LOG_ERROR, "Packet dequeue failure: draining=%d, out.done=%d, err=%d\n", s->draining, s->output.done, dst_rv);
                }

            } while (dst_rv == 0 && xlat_pts_out(avctx, s, frame));
        }
    } while (src_rv == 0 || (src_rv == 1 && dst_rv == AVERROR(EAGAIN)) );

    if (dst_rv)
        av_frame_unref(frame);

    // If we got a frame this time ask for a pkt next time
    s->req_pkt = (dst_rv == 0);

    return dst_rv == 0 ? 0 :
        src_rv < 0 ? src_rv :
        dst_rv < 0 ? dst_rv :
            AVERROR(EAGAIN);

#else
    V4L2m2mContext *s = ((V4L2m2mPriv*)avctx->priv_data)->context;
    V4L2Context *const capture = &s->capture;
    V4L2Context *const output = &s->output;
    AVPacket avpkt = {0};
    int ret = 0;

    if (s->buf_pkt.size) {
        av_packet_move_ref(&avpkt, &s->buf_pkt);
    } else {
        ret = ff_decode_get_packet(avctx, &avpkt);
        if (ret < 0 && ret != AVERROR_EOF && ret != AVERROR(EAGAIN))
            return ret;
        if (ret == 0)
            xlat_pts_in(avctx, s, &avpkt);
    }

    if (ret)
        goto dequeue;

//    av_log(avctx, AV_LOG_INFO, "Extdata len=%d, sent=%d\n", avctx->extradata_size, s->extdata_sent);
    ret = ff_v4l2_context_enqueue_packet(output, &avpkt,
                                         avctx->extradata, s->extdata_sent ? 0 : avctx->extradata_size);
    s->extdata_sent = 1;
    if (ret < 0) {
        av_log(avctx, AV_LOG_ERROR, "Packet enqueue failure: err=%d\n", ret);
        if (ret != AVERROR(EAGAIN))
           return ret;

        s->buf_pkt = avpkt;
        /* no input buffers available, continue dequeing */
    }

    if (avpkt.size) {
        ret = v4l2_try_start(avctx);
        if (ret) {
            av_packet_unref(&avpkt);

            /* cant recover */
            if (ret == AVERROR(ENOMEM))
                return ret;

            return 0;
        }
    }

dequeue:
    if (!s->buf_pkt.size)
        av_packet_unref(&avpkt);

    ret = ff_v4l2_context_dequeue_frame(capture, frame, -1);
    if (!ret)
        xlat_pts_out(avctx, s, frame);
    return ret;
#endif
}

#if 0
#include <time.h>
static int64_t us_time(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

static int v4l2_receive_frame(AVCodecContext *avctx, AVFrame *frame)
{
    int ret;
    const int64_t now = us_time();
    int64_t done;
    av_log(avctx, AV_LOG_TRACE, "<<< %s\n", __func__);
    ret = v4l2_receive_frame2(avctx, frame);
    done = us_time();
    av_log(avctx, AV_LOG_TRACE, ">>> %s: rx time=%" PRId64 ", rv=%d\n", __func__, done - now, ret);
    return ret;
}
#endif

static av_cold int v4l2_decode_init(AVCodecContext *avctx)
{
    V4L2Context *capture, *output;
    V4L2m2mContext *s;
    V4L2m2mPriv *priv = avctx->priv_data;
    int ret;

    av_log(avctx, AV_LOG_TRACE, "<<< %s\n", __func__);
    avctx->pix_fmt = AV_PIX_FMT_DRM_PRIME;

    ret = ff_v4l2_m2m_create_context(priv, &s);
    if (ret < 0)
        return ret;

    capture = &s->capture;
    output = &s->output;

    /* if these dimensions are invalid (ie, 0 or too small) an event will be raised
     * by the v4l2 driver; this event will trigger a full pipeline reconfig and
     * the proper values will be retrieved from the kernel driver.
     */
    output->height = capture->height = avctx->coded_height;
    output->width = capture->width = avctx->coded_width;

    output->av_codec_id = avctx->codec_id;
    output->av_pix_fmt  = AV_PIX_FMT_NONE;

    capture->av_codec_id = AV_CODEC_ID_RAWVIDEO;
    capture->av_pix_fmt = avctx->pix_fmt;

    /* the client requests the codec to generate DRM frames:
     *   - data[0] will therefore point to the returned AVDRMFrameDescriptor
     *       check the ff_v4l2_buffer_to_avframe conversion function.
     *   - the DRM frame format is passed in the DRM frame descriptor layer.
     *       check the v4l2_get_drm_frame function.
     */
    switch (ff_get_format(avctx, avctx->codec->pix_fmts)) {
    default:
        s->output_drm = 1;
        break;
    }

    s->device_ref = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_DRM);
    if (!s->device_ref) {
        ret = AVERROR(ENOMEM);
        return ret;
    }

    ret = av_hwdevice_ctx_init(s->device_ref);
    if (ret < 0)
        return ret;

    s->avctx = avctx;
    ret = ff_v4l2_m2m_codec_init(priv);
    if (ret) {
        av_log(avctx, AV_LOG_ERROR, "can't configure decoder\n");
        return ret;
    }

    return v4l2_prepare_decoder(s);
}

static av_cold int v4l2_decode_close(AVCodecContext *avctx)
{
    av_log(avctx, AV_LOG_TRACE, "<<< %s\n", __func__);
    return ff_v4l2_m2m_codec_end(avctx->priv_data);
    av_log(avctx, AV_LOG_TRACE, ">>> %s\n", __func__);
}

static void v4l2_decode_flush(AVCodecContext *avctx)
{

#if 0
    v4l2_decode_close(avctx);
    v4l2_decode_init(avctx);
#else
    V4L2m2mPriv *priv = avctx->priv_data;
    V4L2m2mContext* s = priv->context;
    V4L2Context* output = &s->output;
    V4L2Context* capture = &s->capture;
    int ret, i;

    av_log(avctx, AV_LOG_TRACE, "<<< %s\n", __func__);

    ret = ff_v4l2_context_set_status(output, VIDIOC_STREAMOFF);
    if (ret < 0)
        av_log(avctx, AV_LOG_ERROR, "VIDIOC_STREAMOFF %s error: %d\n", output->name, ret);

    for (i = 0; i < output->num_buffers; i++) {
        if (output->buffers[i].status == V4L2BUF_IN_DRIVER)
            output->buffers[i].status = V4L2BUF_AVAILABLE;
    }

    for (i = 0; i != FF_V4L2_M2M_TRACK_SIZE; ++i)
        s->track_els[i].discard = 1;

#if 0

    ret = ff_v4l2_context_set_status(capture, VIDIOC_STREAMOFF);
    if (ret < 0)
        av_log(avctx, AV_LOG_ERROR, "VIDIOC_STREAMOFF %s error: %d\n", capture->name, ret);


    ret = ff_v4l2_context_set_status(capture, VIDIOC_STREAMON);
    if (ret < 0)
        av_log(avctx, AV_LOG_ERROR, "VIDIOC_STREAMON %s error: %d\n", capture->name, ret);
    ret = ff_v4l2_context_set_status(output, VIDIOC_STREAMON);
    if (ret < 0)
        av_log(avctx, AV_LOG_ERROR, "VIDIOC_STREAMON %s error: %d\n", output->name, ret);

    struct v4l2_decoder_cmd cmd = {
        .cmd = V4L2_DEC_CMD_START,
        .flags = 0,
    };

    ret = ioctl(s->fd, VIDIOC_DECODER_CMD, &cmd);
    if (ret < 0)
        av_log(avctx, AV_LOG_ERROR, "VIDIOC_DECODER_CMD start error: %d\n", errno);
#endif

    s->draining = 0;
    s->extdata_sent = 0;
    output->done = 0;
    capture->done = 0;
#endif
    av_log(avctx, AV_LOG_TRACE, ">>> %s\n", __func__);
}

#define OFFSET(x) offsetof(V4L2m2mPriv, x)
#define FLAGS AV_OPT_FLAG_VIDEO_PARAM | AV_OPT_FLAG_DECODING_PARAM

static const AVOption options[] = {
    V4L_M2M_DEFAULT_OPTS,
    { "num_capture_buffers", "Number of buffers in the capture context",
        OFFSET(num_capture_buffers), AV_OPT_TYPE_INT, {.i64 = 20}, 2, INT_MAX, FLAGS },
    { "pixel_format", "Pixel format to be used by the decoder", OFFSET(pix_fmt), AV_OPT_TYPE_PIXEL_FMT, {.i64 = AV_PIX_FMT_NONE}, AV_PIX_FMT_NONE, AV_PIX_FMT_NB, FLAGS },
    { NULL},
};

static const AVCodecHWConfigInternal *v4l2_m2m_hw_configs[] = {
    HW_CONFIG_INTERNAL(DRM_PRIME),
    NULL
};

#define M2MDEC_CLASS(NAME) \
    static const AVClass v4l2_m2m_ ## NAME ## _dec_class = { \
        .class_name = #NAME "_v4l2m2m_decoder", \
        .item_name  = av_default_item_name, \
        .option     = options, \
        .version    = LIBAVUTIL_VERSION_INT, \
    };

#define M2MDEC(NAME, LONGNAME, CODEC, bsf_name) \
    M2MDEC_CLASS(NAME) \
    AVCodec ff_ ## NAME ## _v4l2m2m_decoder = { \
        .name           = #NAME "_v4l2m2m" , \
        .long_name      = NULL_IF_CONFIG_SMALL("V4L2 mem2mem " LONGNAME " decoder wrapper"), \
        .type           = AVMEDIA_TYPE_VIDEO, \
        .id             = CODEC , \
        .priv_data_size = sizeof(V4L2m2mPriv), \
        .priv_class     = &v4l2_m2m_ ## NAME ## _dec_class, \
        .init           = v4l2_decode_init, \
        .receive_frame  = v4l2_receive_frame, \
        .close          = v4l2_decode_close, \
        .flush          = v4l2_decode_flush, \
        .bsfs           = bsf_name, \
        .capabilities   = AV_CODEC_CAP_HARDWARE | AV_CODEC_CAP_DELAY | AV_CODEC_CAP_AVOID_PROBING, \
        .caps_internal  = FF_CODEC_CAP_SETS_PKT_DTS | FF_CODEC_CAP_INIT_CLEANUP, \
        .pix_fmts       = (const enum AVPixelFormat[]) { AV_PIX_FMT_DRM_PRIME, \
                                                         AV_PIX_FMT_NV12, \
                                                         AV_PIX_FMT_NONE}, \
        .hw_configs     = v4l2_m2m_hw_configs, \
        .wrapper_name   = "v4l2m2m", \
    }

M2MDEC(h264,  "H.264", AV_CODEC_ID_H264,       "h264_mp4toannexb");
M2MDEC(hevc,  "HEVC",  AV_CODEC_ID_HEVC,       "hevc_mp4toannexb");
M2MDEC(mpeg1, "MPEG1", AV_CODEC_ID_MPEG1VIDEO, NULL);
M2MDEC(mpeg2, "MPEG2", AV_CODEC_ID_MPEG2VIDEO, NULL);
M2MDEC(mpeg4, "MPEG4", AV_CODEC_ID_MPEG4,      NULL);
M2MDEC(h263,  "H.263", AV_CODEC_ID_H263,       NULL);
M2MDEC(vc1 ,  "VC1",   AV_CODEC_ID_VC1,        NULL);
M2MDEC(vp8,   "VP8",   AV_CODEC_ID_VP8,        NULL);
M2MDEC(vp9,   "VP9",   AV_CODEC_ID_VP9,        NULL);
