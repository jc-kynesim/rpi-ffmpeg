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

#include "libavutil/avassert.h"
#include "libavutil/hwcontext.h"
#include "libavutil/hwcontext_drm.h"
#include "libavutil/pixfmt.h"
#include "libavutil/pixdesc.h"
#include "libavutil/opt.h"
#include "libavcodec/avcodec.h"
#include "codec_internal.h"
#include "libavcodec/decode.h"

#include "libavcodec/hwaccels.h"
#include "libavcodec/internal.h"
#include "libavcodec/hwconfig.h"

#include "v4l2_context.h"
#include "v4l2_m2m.h"
#include "v4l2_fmt.h"

// Pick 64 for max last count - that is >1sec at 60fps
#define STATS_LAST_COUNT_MAX 64
#define STATS_INTERVAL_MAX (1 << 30)

static int64_t pts_stats_guess(const pts_stats_t * const stats)
{
    if (stats->last_pts == AV_NOPTS_VALUE ||
            stats->last_interval == 0 ||
            stats->last_count >= STATS_LAST_COUNT_MAX)
        return AV_NOPTS_VALUE;
    return stats->last_pts + (int64_t)(stats->last_count - 1) * (int64_t)stats->last_interval;
}

static void pts_stats_add(pts_stats_t * const stats, int64_t pts)
{
    if (pts == AV_NOPTS_VALUE || pts == stats->last_pts) {
        if (stats->last_count < STATS_LAST_COUNT_MAX)
            ++stats->last_count;
        return;
    }

    if (stats->last_pts != AV_NOPTS_VALUE) {
        const int64_t interval = pts - stats->last_pts;

        if (interval < 0 || interval >= STATS_INTERVAL_MAX ||
            stats->last_count >= STATS_LAST_COUNT_MAX) {
            if (stats->last_interval != 0)
                av_log(stats->logctx, AV_LOG_DEBUG, "%s: %s: Bad interval: %" PRId64 "/%d\n",
                       __func__, stats->name, interval, stats->last_count);
            stats->last_interval = 0;
        }
        else {
            const int64_t frame_time = interval / (int64_t)stats->last_count;

            if (frame_time != stats->last_interval)
                av_log(stats->logctx, AV_LOG_DEBUG, "%s: %s: New interval: %u->%" PRId64 "/%d=%" PRId64 "\n",
                       __func__, stats->name, stats->last_interval, interval, stats->last_count, frame_time);
            stats->last_interval = frame_time;
        }
    }

    stats->last_pts = pts;
    stats->last_count = 1;
}

static void pts_stats_init(pts_stats_t * const stats, void * logctx, const char * name)
{
    *stats = (pts_stats_t){
        .logctx = logctx,
        .name = name,
        .last_count = 1,
        .last_interval = 0,
        .last_pts = AV_NOPTS_VALUE
    };
}

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
    else
        av_log(avctx, AV_LOG_DEBUG, "VIDIOC_DECODER_CMD start OK\n");

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
#if 1
    selection.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    selection.target = V4L2_SEL_TGT_CROP_DEFAULT;
    ret = ioctl(s->fd, VIDIOC_G_SELECTION, &selection);
    av_log(avctx, AV_LOG_INFO, "Post G selection ret=%d, err=%d %dx%d\n", ret, errno, selection.r.width, selection.r.height);
#else
    selection.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    selection.r.height = avctx->coded_height;
    selection.r.width = avctx->coded_width;
    av_log(avctx, AV_LOG_INFO, "Try selection %dx%d\n", avctx->coded_width, avctx->coded_height);
    ret = ioctl(s->fd, VIDIOC_S_SELECTION, &selection);
    av_log(avctx, AV_LOG_INFO, "Post S selection ret=%d, err=%d %dx%d\n", ret, errno, selection.r.width, selection.r.height);
    if (1) {
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
#endif

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

static inline int64_t track_to_pts(AVCodecContext *avctx, unsigned int n)
{
    return (int64_t)n;
}

static inline unsigned int pts_to_track(AVCodecContext *avctx, const int64_t pts)
{
    return (unsigned int)pts;
}

// FFmpeg requires us to propagate a number of vars from the coded pkt into
// the decoded frame. The only thing that tracks like that in V4L2 stateful
// is timestamp. PTS maps to timestamp for this decode. FFmpeg makes no
// guarantees about PTS being unique or specified for every frame so replace
// the supplied PTS with a simple incrementing number and keep a circular
// buffer of all the things we want preserved (including the original PTS)
// indexed by the tracking no.
static void
xlat_pts_in(AVCodecContext *const avctx, xlat_track_t *const x, AVPacket *const avpkt)
{
    int64_t track_pts;

    // Avoid 0
    if (++x->track_no == 0)
        x->track_no = 1;

    track_pts = track_to_pts(avctx, x->track_no);

    av_log(avctx, AV_LOG_TRACE, "In PTS=%" PRId64 ", DTS=%" PRId64 ", track=%" PRId64 ", n=%u\n", avpkt->pts, avpkt->dts, track_pts, x->track_no);
    x->last_pkt_dts = avpkt->dts;
    x->track_els[x->track_no  % FF_V4L2_M2M_TRACK_SIZE] = (V4L2m2mTrackEl){
        .discard          = 0,
        .pending          = 1,
        .pkt_size         = avpkt->size,
        .pts              = avpkt->pts,
        .dts              = avpkt->dts,
        .reordered_opaque = avctx->reordered_opaque,
        .pkt_pos          = avpkt->pos,
        .pkt_duration     = avpkt->duration,
        .track_pts        = track_pts
    };
    avpkt->pts = track_pts;
}

// Returns -1 if we should discard the frame
static int
xlat_pts_out(AVCodecContext *const avctx,
             xlat_track_t * const x,
             pts_stats_t * const ps,
             AVFrame *const frame)
{
    unsigned int n = pts_to_track(avctx, frame->pts) % FF_V4L2_M2M_TRACK_SIZE;
    V4L2m2mTrackEl *const t = x->track_els + n;
    if (frame->pts == AV_NOPTS_VALUE || frame->pts != t->track_pts)
    {
        av_log(avctx, AV_LOG_INFO, "Tracking failure: pts=%" PRId64 ", track[%d]=%" PRId64 "\n", frame->pts, n, t->track_pts);
        frame->pts              = AV_NOPTS_VALUE;
        frame->pkt_dts          = x->last_pkt_dts;
        frame->reordered_opaque = x->last_opaque;
        frame->pkt_pos          = -1;
        frame->pkt_duration     = 0;
        frame->pkt_size         = -1;
    }
    else if (!t->discard)
    {
        frame->pts              = t->pending ? t->pts : AV_NOPTS_VALUE;
        frame->pkt_dts          = x->last_pkt_dts;
        frame->reordered_opaque = t->reordered_opaque;
        frame->pkt_pos          = t->pkt_pos;
        frame->pkt_duration     = t->pkt_duration;
        frame->pkt_size         = t->pkt_size;

        x->last_opaque = x->track_els[n].reordered_opaque;
        if (frame->pts != AV_NOPTS_VALUE)
            x->last_pts = frame->pts;
        t->pending = 0;
    }
    else
    {
        av_log(avctx, AV_LOG_DEBUG, "Discard frame (flushed): pts=%" PRId64 ", track[%d]=%" PRId64 "\n", frame->pts, n, t->track_pts);
        return -1;
    }

    pts_stats_add(ps, frame->pts);

    frame->best_effort_timestamp = pts_stats_guess(ps);
    frame->pkt_dts               = frame->pts;  // We can't emulate what s/w does in a useful manner?
    av_log(avctx, AV_LOG_TRACE, "Out PTS=%" PRId64 "/%"PRId64", DTS=%" PRId64 "\n", frame->pts, frame->best_effort_timestamp, frame->pkt_dts);
    return 0;
}

static void
xlat_flush(xlat_track_t * const x)
{
    unsigned int i;
    for (i = 0; i != FF_V4L2_M2M_TRACK_SIZE; ++i) {
        x->track_els[i].pending = 0;
        x->track_els[i].discard = 1;
    }
    x->last_pts = AV_NOPTS_VALUE;
}

static void
xlat_init(xlat_track_t * const x)
{
    memset(x, 0, sizeof(*x));
    x->last_pts = AV_NOPTS_VALUE;
}

static int
xlat_pending(const xlat_track_t * const x)
{
    unsigned int n = x->track_no % FF_V4L2_M2M_TRACK_SIZE;
    unsigned int i;
    int r = 0;
    int64_t now = AV_NOPTS_VALUE;

    for (i = 0; i < 32; ++i, n = (n - 1) % FF_V4L2_M2M_TRACK_SIZE) {
        const V4L2m2mTrackEl * const t = x->track_els + n;

        if (!t->pending)
            continue;

        if (now == AV_NOPTS_VALUE)
            now = t->dts;

        if (t->pts == AV_NOPTS_VALUE ||
            ((now == AV_NOPTS_VALUE || t->pts <= now) &&
             (x->last_pts == AV_NOPTS_VALUE || t->pts > x->last_pts)))
            ++r;
    }

    // If we never get any ideas about PTS vs DTS allow a lot more buffer
    if (now == AV_NOPTS_VALUE)
        r -= 16;

    return r;
}

static inline int stream_started(const V4L2m2mContext * const s) {
    return s->capture.streamon && s->output.streamon;
}

#define NQ_OK        0
#define NQ_Q_FULL    1
#define NQ_SRC_EMPTY 2
#define NQ_NONE      3
#define NQ_DRAINING  4
#define NQ_DEAD      5

#define TRY_DQ(nq_status) ((nq_status) >= NQ_OK && (nq_status) <= NQ_DRAINING)
#define RETRY_NQ(nq_status) ((nq_status) == NQ_Q_FULL || (nq_status) == NQ_NONE)

// AVERROR_EOF     Flushing an already flushed stream
// -ve             Error (all errors except EOF are unexpected)
// NQ_OK (0)       OK
// NQ_Q_FULL       Dst full (retry if we think V4L2 Q has space now)
// NQ_SRC_EMPTY    Src empty (do not retry)
// NQ_NONE         Enqueue not attempted
// NQ_DRAINING     At EOS, dQ dest until EOS there too
// NQ_DEAD         Not running (do not retry, do not attempt capture dQ)

static int try_enqueue_src(AVCodecContext * const avctx, V4L2m2mContext * const s)
{
    int ret;

    // If we don't already have a coded packet - get a new one
    // We will already have a coded pkt if the output Q was full last time we
    // tried to Q it
    if (!s->buf_pkt.size) {
        ret = ff_decode_get_packet(avctx, &s->buf_pkt);

        if (ret == AVERROR(EAGAIN)) {
            if (!stream_started(s)) {
                av_log(avctx, AV_LOG_TRACE, "%s: receive_frame before 1st coded packet\n", __func__);
                return NQ_DEAD;
            }
            return NQ_SRC_EMPTY;
        }

        if (ret == AVERROR_EOF) {
            // EOF - enter drain mode
            av_log(avctx, AV_LOG_TRACE, "--- EOS req: ret=%d, size=%d, started=%d, drain=%d\n",
                   ret, s->buf_pkt.size, stream_started(s), s->draining);
            if (!stream_started(s)) {
                av_log(avctx, AV_LOG_DEBUG, "EOS on flushed stream\n");
                s->draining = 1;
                s->capture.done = 1;
                return AVERROR_EOF;
            }

            if (!s->draining) {
                // Calling enqueue with an empty pkt starts drain
                av_assert0(s->buf_pkt.size == 0);
                ret = ff_v4l2_context_enqueue_packet(&s->output, &s->buf_pkt, NULL, 0);
                if (ret) {
                    av_log(avctx, AV_LOG_ERROR, "Failed to start drain: ret=%d\n", ret);
                    return ret;
                }
            }
            return NQ_DRAINING;
        }

        if (ret < 0) {
            av_log(avctx, AV_LOG_ERROR, "Failed to get coded packet: err=%d\n", ret);
            return ret;
        }

        xlat_pts_in(avctx, &s->xlat, &s->buf_pkt);
    }

    if ((ret = check_output_streamon(avctx, s)) != 0)
        return ret;

    ret = ff_v4l2_context_enqueue_packet(&s->output, &s->buf_pkt,
                                         avctx->extradata, s->extdata_sent ? 0 : avctx->extradata_size);

    if (ret == AVERROR(EAGAIN)) {
        // Out of input buffers - keep packet
        ret = NQ_Q_FULL;
    }
    else {
        // In all other cases we are done with this packet
        av_packet_unref(&s->buf_pkt);
        s->extdata_sent = 1;

        if (ret) {
            av_log(avctx, AV_LOG_ERROR, "Packet enqueue failure: err=%d\n", ret);
            return ret;
        }
    }

    // Start if we haven't
    {
        const int ret2 = v4l2_try_start(avctx);
        if (ret2) {
            av_log(avctx, AV_LOG_DEBUG, "Start failure: err=%d\n", ret2);
            ret = (ret2 == AVERROR(ENOMEM)) ? ret2 : NQ_DEAD;
        }
    }

    return ret;
}

static int v4l2_receive_frame(AVCodecContext *avctx, AVFrame *frame)
{
    V4L2m2mContext *const s = ((V4L2m2mPriv*)avctx->priv_data)->context;
    int src_rv = NQ_NONE;
    int dst_rv = 1;  // Non-zero (done), non-negative (error) number
    unsigned int i = 0;

    do {
        const int pending = xlat_pending(&s->xlat);
        const int prefer_dq = (pending > 5);

        // Enqueue another pkt for decode if
        // (a) We don't have a lot of stuff in the buffer already OR
        // (b) ... we (think we) do but we've failed to get a frame already OR
        // (c) We've dequeued a lot of frames without asking for input
        if (!prefer_dq || i != 0 || s->req_pkt > 2) {
            src_rv = try_enqueue_src(avctx, s);

            // If we got a frame last time or we've already tried to get a frame and
            // we have nothing to enqueue then return now. rv will be AVERROR(EAGAIN)
            // indicating that we want more input.
            // This should mean that once decode starts we enter a stable state where
            // we alternately ask for input and produce output
            if ((i != 0 || s->req_pkt) && src_rv == NQ_SRC_EMPTY)
                break;
        }

        // Try to get a new frame if
        // (a) we haven't already got one AND
        // (b) enqueue returned a status indicating that decode should be attempted
        if (dst_rv != 0 && TRY_DQ(src_rv)) {
            do {
                // Dequeue frame will unref any previous contents of frame
                // if it returns success so we don't need an explicit unref
                // when discarding
                // This returns AVERROR(EAGAIN) on timeout or if
                // there is room in the input Q and timeout == -1
                dst_rv = ff_v4l2_context_dequeue_frame(&s->capture, frame, prefer_dq ? 5 : -1);

                if (dst_rv == AVERROR_EOF && (s->draining || s->capture.done))
                    av_log(avctx, AV_LOG_DEBUG, "Dequeue EOF: draining=%d, cap.done=%d\n",
                           s->draining, s->capture.done);
                else if (dst_rv && dst_rv != AVERROR(EAGAIN))
                    av_log(avctx, AV_LOG_ERROR, "Packet dequeue failure: draining=%d, cap.done=%d, err=%d\n",
                           s->draining, s->capture.done, dst_rv);

                // Go again if we got a frame that we need to discard
            } while (dst_rv == 0 && xlat_pts_out(avctx, &s->xlat, &s->pts_stat, frame));
        }

        ++i;
        if (i >= 256) {
            av_log(avctx, AV_LOG_ERROR, "Unexpectedly large retry count: %d\n", i);
            src_rv = AVERROR(EIO);
        }

        // Continue trying to enqueue packets if either
        // (a) we succeeded last time OR
        // (b) we didn't ret a frame and we can retry the input
    } while (src_rv == NQ_OK || (dst_rv == AVERROR(EAGAIN) && RETRY_NQ(src_rv)));

    // Ensure that the frame contains nothing if we aren't returning a frame
    // (might happen when discarding)
    if (dst_rv)
        av_frame_unref(frame);

    // If we got a frame this time ask for a pkt next time
    s->req_pkt = (dst_rv == 0) ? s->req_pkt + 1 : 0;

#if 0
    if (dst_rv == 0)
    {
        static int z = 0;
        if (++z > 50) {
            av_log(avctx, AV_LOG_ERROR, "Streamoff and die?\n");
            ff_v4l2_context_set_status(&s->capture, VIDIOC_STREAMOFF);
            return -1;
        }
    }
#endif

    return dst_rv == 0 ? 0 :
        src_rv < 0 ? src_rv :
        dst_rv < 0 ? dst_rv :
            AVERROR(EAGAIN);
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

// This heuristic is for H264 but use for everything
static uint32_t max_coded_size(const AVCodecContext * const avctx)
{
    uint32_t wxh = avctx->coded_width * avctx->coded_height;
    uint32_t size;

    size = wxh * 3 / 2;
    // H.264 Annex A table A-1 gives minCR which is either 2 or 4
    // unfortunately that doesn't yield an actually useful limit
    // and it should be noted that frame 0 is special cased to allow
    // a bigger number which really isn't helpful for us. So just pick
    // frame_size / 2
    size /= 2;
    // Add 64k to allow for any overheads and/or encoder hopefulness
    // with small WxH
    return size + (1 << 16);
}

static av_cold int v4l2_decode_init(AVCodecContext *avctx)
{
    V4L2Context *capture, *output;
    V4L2m2mContext *s;
    V4L2m2mPriv *priv = avctx->priv_data;
    int gf_pix_fmt;
    int ret;

    av_log(avctx, AV_LOG_TRACE, "<<< %s\n", __func__);

    if (avctx->codec_id == AV_CODEC_ID_H264) {
        if (avctx->ticks_per_frame == 1) {
            if(avctx->time_base.den < INT_MAX/2) {
                avctx->time_base.den *= 2;
            } else
                avctx->time_base.num /= 2;
        }
        avctx->ticks_per_frame = 2;
    }

    av_log(avctx, AV_LOG_INFO, "level=%d\n", avctx->level);
    ret = ff_v4l2_m2m_create_context(priv, &s);
    if (ret < 0)
        return ret;

    xlat_init(&s->xlat);
    pts_stats_init(&s->pts_stat, avctx, "decoder");

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
    output->min_buf_size = max_coded_size(avctx);
    output->no_pts_rescale = 1;

    capture->av_codec_id = AV_CODEC_ID_RAWVIDEO;
    capture->av_pix_fmt = avctx->pix_fmt;
    capture->min_buf_size = 0;
    capture->no_pts_rescale = 1;

    /* the client requests the codec to generate DRM frames:
     *   - data[0] will therefore point to the returned AVDRMFrameDescriptor
     *       check the ff_v4l2_buffer_to_avframe conversion function.
     *   - the DRM frame format is passed in the DRM frame descriptor layer.
     *       check the v4l2_get_drm_frame function.
     */

    gf_pix_fmt = ff_get_format(avctx, avctx->codec->pix_fmts);
    av_log(avctx, AV_LOG_DEBUG, "avctx requested=%d (%s); get_format requested=%d (%s)\n",
           avctx->pix_fmt, av_get_pix_fmt_name(avctx->pix_fmt), gf_pix_fmt, av_get_pix_fmt_name(gf_pix_fmt));

    s->output_drm = 0;
    if (gf_pix_fmt == AV_PIX_FMT_DRM_PRIME || avctx->pix_fmt == AV_PIX_FMT_DRM_PRIME) {
        avctx->pix_fmt = AV_PIX_FMT_DRM_PRIME;
        s->output_drm = 1;
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
    int rv;
    av_log(avctx, AV_LOG_TRACE, "<<< %s\n", __func__);
    rv = ff_v4l2_m2m_codec_end(avctx->priv_data);
    av_log(avctx, AV_LOG_TRACE, ">>> %s: rv=%d\n", __func__, rv);
    return rv;
}

static void v4l2_decode_flush(AVCodecContext *avctx)
{
    // An alternatve and more drastic form of flush is to simply do this:
    //    v4l2_decode_close(avctx);
    //    v4l2_decode_init(avctx);
    // The downside is that this keeps a decoder open until all the frames
    // associated with it have been returned.  This is a bit wasteful on
    // possibly limited h/w resources and fails on a Pi for this reason unless
    // more GPU mem is allocated than is the default.

    V4L2m2mPriv * const priv = avctx->priv_data;
    V4L2m2mContext * const s = priv->context;
    V4L2Context * const output = &s->output;
    V4L2Context * const capture = &s->capture;
    int ret;

    av_log(avctx, AV_LOG_TRACE, "<<< %s: streamon=%d\n", __func__, output->streamon);

    // Reflushing everything is benign, quick and avoids having to worry about
    // states like EOS processing so don't try to optimize out (having got it
    // wrong once)

    ret = ff_v4l2_context_set_status(output, VIDIOC_STREAMOFF);
    if (ret < 0)
        av_log(avctx, AV_LOG_ERROR, "VIDIOC_STREAMOFF %s error: %d\n", output->name, ret);

    // Clear any buffered input packet
    av_packet_unref(&s->buf_pkt);

    // V4L2 makes no guarantees about whether decoded frames are flushed or not
    // so mark all frames we are tracking to be discarded if they appear
    xlat_flush(&s->xlat);

    // resend extradata
    s->extdata_sent = 0;
    // clear EOS status vars
    s->draining = 0;
    output->done = 0;
    capture->done = 0;

    // Stream on will occur when we actually submit a new frame
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
    const FFCodec ff_ ## NAME ## _v4l2m2m_decoder = { \
        .p.name         = #NAME "_v4l2m2m" , \
        CODEC_LONG_NAME("V4L2 mem2mem " LONGNAME " decoder wrapper"), \
        .p.type         = AVMEDIA_TYPE_VIDEO, \
        .p.id           = CODEC , \
        .priv_data_size = sizeof(V4L2m2mPriv), \
        .p.priv_class   = &v4l2_m2m_ ## NAME ## _dec_class, \
        .init           = v4l2_decode_init, \
        FF_CODEC_RECEIVE_FRAME_CB(v4l2_receive_frame), \
        .close          = v4l2_decode_close, \
        .flush          = v4l2_decode_flush, \
        .bsfs           = bsf_name, \
        .p.capabilities = AV_CODEC_CAP_HARDWARE | AV_CODEC_CAP_DELAY | AV_CODEC_CAP_AVOID_PROBING, \
        .caps_internal  = FF_CODEC_CAP_NOT_INIT_THREADSAFE | \
                          FF_CODEC_CAP_SETS_PKT_DTS | FF_CODEC_CAP_INIT_CLEANUP, \
        .p.wrapper_name = "v4l2m2m", \
        .p.pix_fmts     = (const enum AVPixelFormat[]) { AV_PIX_FMT_DRM_PRIME, \
                                                         AV_PIX_FMT_NV12, \
                                                         AV_PIX_FMT_YUV420P, \
                                                         AV_PIX_FMT_NONE}, \
        .hw_configs     = v4l2_m2m_hw_configs, \
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
