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
#include "v4l2_req_dmabufs.h"

// Pick 64 for max last count - that is >1sec at 60fps
#define STATS_LAST_COUNT_MAX 64
#define STATS_INTERVAL_MAX (1 << 30)

#ifndef FF_API_BUFFER_SIZE_T
#define FF_API_BUFFER_SIZE_T 1
#endif

#define DUMP_FAILED_EXTRADATA 0

#if DUMP_FAILED_EXTRADATA
static inline char hex1(unsigned int x)
{
    x &= 0xf;
    return x <= 9 ? '0' + x : 'a' + x - 10;
}

static inline char * hex2(char * s, unsigned int x)
{
    *s++ = hex1(x >> 4);
    *s++ = hex1(x);
    return s;
}

static inline char * hex4(char * s, unsigned int x)
{
    s = hex2(s, x >> 8);
    s = hex2(s, x);
    return s;
}

static inline char * dash2(char * s)
{
    *s++ = '-';
    *s++ = '-';
    return s;
}

static void
data16(char * s, const unsigned int offset, const uint8_t * m, const size_t len)
{
    size_t i;
    s = hex4(s, offset);
    m += offset;
    for (i = 0; i != 8; ++i) {
        *s++ = ' ';
        s = len > i + offset ? hex2(s, *m++) : dash2(s);
    }
    *s++ = ' ';
    *s++ = ':';
    for (; i != 16; ++i) {
        *s++ = ' ';
        s = len > i + offset ? hex2(s, *m++) : dash2(s);
    }
    *s++ = 0;
}

static void
log_dump(void * logctx, int lvl, const void * const data, const size_t len)
{
    size_t i;
    for (i = 0; i < len; i += 16) {
        char buf[80];
        data16(buf, i, data, len);
        av_log(logctx, lvl, "%s\n", buf);
    }
}
#endif

static int64_t pts_stats_guess(const pts_stats_t * const stats)
{
    if (stats->last_count <= 1)
        return stats->last_pts;
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

// If abdata == NULL then this just counts space required
// Unpacks avcC if detected
static int
h264_xd_copy(const uint8_t * const extradata, const int extrasize, uint8_t * abdata)
{
    const uint8_t * const xdend = extradata + extrasize;
    const uint8_t * p = extradata;
    uint8_t * d = abdata;
    unsigned int n;
    unsigned int len;
    const unsigned int hdrlen = 4;
    unsigned int need_pps = 1;

    if (extrasize < 8)
        return AVERROR(EINVAL);

    if (p[0] == 0 && p[1] == 0) {
        // Assume a couple of leading zeros are good enough to indicate NAL
        if (abdata)
            memcpy(d, p, extrasize);
        return extrasize;
    }

    // avcC starts with a 1
    if (p[0] != 1)
        return AVERROR(EINVAL);

    p += 5;
    n = *p++ & 0x1f;

doxps:
    while (n--) {
        if (xdend - p < 2)
            return AVERROR(EINVAL);
        len = (p[0] << 8) | p[1];
        p += 2;
        if (xdend - p < (ptrdiff_t)len)
            return AVERROR(EINVAL);
        if (abdata) {
            d[0] = 0;
            d[1] = 0;
            d[2] = 0;
            d[3] = 1;
            memcpy(d + 4, p, len);
        }
        d += len + hdrlen;
        p += len;
    }
    if (need_pps) {
        need_pps = 0;
        if (p >= xdend)
            return AVERROR(EINVAL);
        n = *p++;
        goto doxps;
    }

    return d - abdata;
}

static int
copy_extradata(AVCodecContext * const avctx,
               const void * const src_data, const int src_len,
               void ** const pdst_data, size_t * const pdst_len)
{
    int len;

    *pdst_len = 0;
    av_freep(pdst_data);

    if (avctx->codec_id == AV_CODEC_ID_H264)
        len = h264_xd_copy(src_data, src_len, NULL);
    else
        len = src_len < 0 ? AVERROR(EINVAL) : src_len;

    // Zero length is OK but we want to stop - -ve is error val
    if (len <= 0)
        return len;

    if ((*pdst_data = av_malloc(len + AV_INPUT_BUFFER_PADDING_SIZE)) == NULL)
        return AVERROR(ENOMEM);

    if (avctx->codec_id == AV_CODEC_ID_H264)
        h264_xd_copy(src_data, src_len, *pdst_data);
    else
        memcpy(*pdst_data, src_data, len);
    *pdst_len = len;

    return 0;
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
    if (ret != 0) {
        av_log(avctx, AV_LOG_ERROR, "VIDIOC_STREAMON on output context: %s\n", av_err2str(ret));
        return ret;
    }

    // STREAMON should do implicit START so this just for those that don't.
    // It is optional so don't worry if it fails
    if (ioctl(s->fd, VIDIOC_DECODER_CMD, &cmd) < 0) {
        ret = AVERROR(errno);
        av_log(avctx, AV_LOG_WARNING, "VIDIOC_DECODER_CMD start error: %s\n", av_err2str(ret));
    }
    else {
        av_log(avctx, AV_LOG_TRACE, "VIDIOC_DECODER_CMD start OK\n");
    }
    return 0;
}

static int v4l2_try_start(AVCodecContext *avctx)
{
    V4L2m2mContext * const s = ((V4L2m2mPriv*)avctx->priv_data)->context;
    int ret;

    /* 1. start the output process */
    if ((ret = check_output_streamon(avctx, s)) != 0)
        return ret;
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

static void
set_best_effort_pts(AVCodecContext *const avctx,
             pts_stats_t * const ps,
             AVFrame *const frame)
{
    pts_stats_add(ps, frame->pts);

    frame->best_effort_timestamp = pts_stats_guess(ps);
    // If we can't guess from just PTS - try DTS
    if (frame->best_effort_timestamp == AV_NOPTS_VALUE)
        frame->best_effort_timestamp = frame->pkt_dts;

    // We can't emulate what s/w does in a useful manner and using the
    // "correct" answer seems to just confuse things.
    frame->pkt_dts               = frame->pts;
    av_log(avctx, AV_LOG_TRACE, "Out PTS=%" PRId64 "/%"PRId64", DTS=%" PRId64 "\n",
           frame->pts, frame->best_effort_timestamp, frame->pkt_dts);
}

static void
xlat_flush(xlat_track_t * const x)
{
    unsigned int i;
    // Do not reset track_no - this ensures that any frames left in the decoder
    // that turn up later get discarded.

    x->last_pts = AV_NOPTS_VALUE;
    x->last_opaque = 0;
    for (i = 0; i != FF_V4L2_M2M_TRACK_SIZE; ++i) {
        x->track_els[i].pending = 0;
        x->track_els[i].discard = 1;
    }
}

static void
xlat_init(xlat_track_t * const x)
{
    memset(x, 0, sizeof(*x));
    xlat_flush(x);
}

static int
xlat_pending(const xlat_track_t * const x)
{
    unsigned int n = x->track_no % FF_V4L2_M2M_TRACK_SIZE;
    int i;
    const int64_t now = x->last_pts;

    for (i = 0; i < FF_V4L2_M2M_TRACK_SIZE; ++i, n = (n - 1) & (FF_V4L2_M2M_TRACK_SIZE - 1)) {
        const V4L2m2mTrackEl * const t = x->track_els + n;

        // Discard only set on never-set or flushed entries
        // So if we get here we've never successfully decoded a frame so allow
        // more frames into the buffer before stalling
        if (t->discard)
            return i - 16;

        // If we've got this frame out then everything before this point
        // must have entered the decoder
        if (!t->pending)
            break;

        // If we've never seen a pts all we can do is count frames
        if (now == AV_NOPTS_VALUE)
            continue;

        if (t->dts != AV_NOPTS_VALUE && now >= t->dts)
            break;
    }

    return i;
}

static inline int stream_started(const V4L2m2mContext * const s) {
    return s->output.streamon;
}

#define NQ_OK        0
#define NQ_Q_FULL    1
#define NQ_SRC_EMPTY 2
#define NQ_NONE      3
#define NQ_DRAINING  4
#define NQ_DEAD      5

#define TRY_DQ(nq_status) ((nq_status) >= NQ_OK && (nq_status) <= NQ_DRAINING)
#define RETRY_NQ(nq_status) ((nq_status) == NQ_Q_FULL || (nq_status) == NQ_NONE)

// do_not_get      If true then no new packet will be got but status will
//                  be set appropriately

// AVERROR_EOF     Flushing an already flushed stream
// -ve             Error (all errors except EOF are unexpected)
// NQ_OK (0)       OK
// NQ_Q_FULL       Dst full (retry if we think V4L2 Q has space now)
// NQ_SRC_EMPTY    Src empty (do not retry)
// NQ_NONE         Enqueue not attempted
// NQ_DRAINING     At EOS, dQ dest until EOS there too
// NQ_DEAD         Not running (do not retry, do not attempt capture dQ)

static int try_enqueue_src(AVCodecContext * const avctx, V4L2m2mContext * const s, const int do_not_get)
{
    int ret;

    // If we don't already have a coded packet - get a new one
    // We will already have a coded pkt if the output Q was full last time we
    // tried to Q it
    if (!s->buf_pkt.size && !do_not_get) {
        unsigned int i;

        for (i = 0; i < 256; ++i) {
            uint8_t * side_data;
            size_t side_size;

            ret = ff_decode_get_packet(avctx, &s->buf_pkt);
            if (ret != 0)
                break;

            // New extradata is the only side-data we undertand
            side_data = av_packet_get_side_data(&s->buf_pkt, AV_PKT_DATA_NEW_EXTRADATA, &side_size);
            if (side_data) {
                av_log(avctx, AV_LOG_DEBUG, "New extradata\n");
                if ((ret = copy_extradata(avctx, side_data, (int)side_size, &s->extdata_data, &s->extdata_size)) < 0)
                    av_log(avctx, AV_LOG_WARNING, "Failed to copy new extra data: %s\n", av_err2str(ret));
                s->extdata_sent = 0;
            }

            if (s->buf_pkt.size != 0)
                break;

            if (s->buf_pkt.side_data_elems == 0) {
                av_log(avctx, AV_LOG_WARNING, "Empty pkt from ff_decode_get_packet - treating as EOF\n");
                ret = AVERROR_EOF;
                break;
            }

            // Retry a side-data only pkt
        }
        // If i >= 256 something has gone wrong
        if (i >= 256) {
            av_log(avctx, AV_LOG_ERROR, "Too many side-data only packets\n");
            return AVERROR(EIO);
        }

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
    }

    if (s->draining) {
        if (s->buf_pkt.size) {
            av_log(avctx, AV_LOG_WARNING, "Unexpected input whilst draining\n");
            av_packet_unref(&s->buf_pkt);
        }
        return NQ_DRAINING;
    }

    if (!s->buf_pkt.size)
        return NQ_NONE;

    if ((ret = check_output_streamon(avctx, s)) != 0)
        return ret;

    if (s->extdata_sent)
        ret = ff_v4l2_context_enqueue_packet(&s->output, &s->buf_pkt, NULL, 0);
    else
        ret = ff_v4l2_context_enqueue_packet(&s->output, &s->buf_pkt, s->extdata_data, s->extdata_size);

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

static int qbuf_wait(AVCodecContext * const avctx, V4L2Context * const ctx)
{
    int rv = 0;

    ff_mutex_lock(&ctx->lock);

    while (atomic_load(&ctx->q_count) == 0 && ctx->streamon) {
        if (pthread_cond_wait(&ctx->cond, &ctx->lock) != 0) {
            rv = AVERROR(errno);
            av_log(avctx, AV_LOG_ERROR, "Cond wait failure: %s\n", av_err2str(rv));
            break;
        }
    }

    ff_mutex_unlock(&ctx->lock);
    return rv;
}

static int v4l2_receive_frame(AVCodecContext *avctx, AVFrame *frame)
{
    V4L2m2mContext *const s = ((V4L2m2mPriv*)avctx->priv_data)->context;
    int src_rv = NQ_OK;
    int dst_rv = 1;  // Non-zero (done), non-negative (error) number
    unsigned int i = 0;

    do {
        const int pending = xlat_pending(&s->xlat);
        const int prefer_dq = (pending > 3);
        const int last_src_rv = src_rv;

        av_log(avctx, AV_LOG_TRACE, "Pending=%d, src_rv=%d, req_pkt=%d\n", pending, src_rv, s->req_pkt);

        // Enqueue another pkt for decode if
        // (a) We don't have a lot of stuff in the buffer already OR
        // (b) ... we (think we) do but we've failed to get a frame already OR
        // (c) We've dequeued a lot of frames without asking for input
        src_rv = try_enqueue_src(avctx, s, !(!prefer_dq || i != 0 || s->req_pkt > 2));

        // If we got a frame last time or we've already tried to get a frame and
        // we have nothing to enqueue then return now. rv will be AVERROR(EAGAIN)
        // indicating that we want more input.
        // This should mean that once decode starts we enter a stable state where
        // we alternately ask for input and produce output
        if ((i != 0 || s->req_pkt) && src_rv == NQ_SRC_EMPTY)
            break;

        if (src_rv == NQ_Q_FULL && last_src_rv == NQ_Q_FULL) {
            av_log(avctx, AV_LOG_WARNING, "Poll thinks src Q has space; none found\n");
            break;
        }

        // Try to get a new frame if
        // (a) we haven't already got one AND
        // (b) enqueue returned a status indicating that decode should be attempted
        if (dst_rv != 0 && TRY_DQ(src_rv)) {
            // Pick a timeout depending on state
            const int t =
                src_rv == NQ_Q_FULL ? -1 :
                src_rv == NQ_DRAINING ? 300 :
                prefer_dq ? 5 : 0;

            // Dequeue frame will unref any previous contents of frame
            // if it returns success so we don't need an explicit unref
            // when discarding
            // This returns AVERROR(EAGAIN) on timeout or if
            // there is room in the input Q and timeout == -1
            dst_rv = ff_v4l2_context_dequeue_frame(&s->capture, frame, t);

            // Failure due to no buffer in Q?
            if (dst_rv == AVERROR(ENOSPC)) {
                // Wait & retry
                if ((dst_rv = qbuf_wait(avctx, &s->capture)) == 0) {
                    dst_rv = ff_v4l2_context_dequeue_frame(&s->capture, frame, t);
                }
            }

            if (dst_rv == 0)
                set_best_effort_pts(avctx, &s->pts_stat, frame);

            if (dst_rv == AVERROR(EAGAIN) && src_rv == NQ_DRAINING) {
                av_log(avctx, AV_LOG_WARNING, "Timeout in drain - assume EOF");
                dst_rv = AVERROR_EOF;
                s->capture.done = 1;
            }
            else if (dst_rv == AVERROR_EOF && (s->draining || s->capture.done))
                av_log(avctx, AV_LOG_DEBUG, "Dequeue EOF: draining=%d, cap.done=%d\n",
                       s->draining, s->capture.done);
            else if (dst_rv && dst_rv != AVERROR(EAGAIN))
                av_log(avctx, AV_LOG_ERROR, "Packet dequeue failure: draining=%d, cap.done=%d, err=%d\n",
                       s->draining, s->capture.done, dst_rv);
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

static int
check_size(AVCodecContext * const avctx, V4L2m2mContext * const s)
{
    unsigned int i;
    const uint32_t fcc = ff_v4l2_get_format_pixelformat(&s->capture.format);
    const uint32_t w = avctx->coded_width;
    const uint32_t h = avctx->coded_height;

    if (w == 0 || h == 0 || fcc == 0) {
        av_log(avctx, AV_LOG_TRACE, "%s: Size %dx%d or fcc %s empty\n", __func__, w, h, av_fourcc2str(fcc));
        return 0;
    }
    if ((s->quirks & FF_V4L2_QUIRK_ENUM_FRAMESIZES_BROKEN) != 0) {
        av_log(avctx, AV_LOG_TRACE, "%s: Skipped (quirk): Size %dx%d, fcc %s\n", __func__, w, h, av_fourcc2str(fcc));
        return 0;
    }

    for (i = 0;; ++i) {
        struct v4l2_frmsizeenum fs = {
            .index = i,
            .pixel_format = fcc,
        };

        while (ioctl(s->fd, VIDIOC_ENUM_FRAMESIZES, &fs) != 0) {
            const int err = AVERROR(errno);
            if (err == AVERROR(EINTR))
                continue;
            if (i == 0 && err == AVERROR(ENOTTY)) {
                av_log(avctx, AV_LOG_DEBUG, "Framesize enum not supported\n");
                return 0;
            }
            if (err != AVERROR(EINVAL)) {
                av_log(avctx, AV_LOG_ERROR, "Failed to enum framesizes: %s", av_err2str(err));
                return err;
            }
            av_log(avctx, AV_LOG_WARNING, "Failed to find Size=%dx%d, fmt=%s in %u frame size enums\n",
                   w, h, av_fourcc2str(fcc), i);
            return err;
        }

        switch (fs.type) {
            case V4L2_FRMSIZE_TYPE_DISCRETE:
                av_log(avctx, AV_LOG_TRACE, "%s[%d]: Discrete: %dx%d\n", __func__, i,
                       fs.discrete.width,fs.discrete.height);
                if (w == fs.discrete.width && h == fs.discrete.height)
                    return 0;
                break;
            case V4L2_FRMSIZE_TYPE_STEPWISE:
                av_log(avctx, AV_LOG_TRACE, "%s[%d]: Stepwise: Min: %dx%d Max: %dx%d, Step: %dx%d\n", __func__, i,
                       fs.stepwise.min_width, fs.stepwise.min_height,
                       fs.stepwise.max_width, fs.stepwise.max_height,
                       fs.stepwise.step_width,fs.stepwise.step_height);
                if (w >= fs.stepwise.min_width && w <= fs.stepwise.max_width &&
                    h >= fs.stepwise.min_height && h <= fs.stepwise.max_height &&
                    (w - fs.stepwise.min_width) % fs.stepwise.step_width == 0 &&
                    (h - fs.stepwise.min_height) % fs.stepwise.step_height == 0)
                    return 0;
                break;
            case V4L2_FRMSIZE_TYPE_CONTINUOUS:
                av_log(avctx, AV_LOG_TRACE, "%s[%d]: Continuous: Min: %dx%d Max: %dx%d, Step: %dx%d\n", __func__, i,
                       fs.stepwise.min_width, fs.stepwise.min_height,
                       fs.stepwise.max_width, fs.stepwise.max_height,
                       fs.stepwise.step_width,fs.stepwise.step_height);
                if (w >= fs.stepwise.min_width && w <= fs.stepwise.max_width &&
                    h >= fs.stepwise.min_height && h <= fs.stepwise.max_height)
                    return 0;
                break;
            default:
                av_log(avctx, AV_LOG_ERROR, "Unexpected framesize enum: %d", fs.type);
                return AVERROR(EINVAL);
        }
    }
}

static int
get_quirks(AVCodecContext * const avctx, V4L2m2mContext * const s)
{
    struct v4l2_capability cap;

    memset(&cap, 0, sizeof(cap));
    while (ioctl(s->fd, VIDIOC_QUERYCAP, &cap) != 0) {
        int err = errno;
        if (err == EINTR)
            continue;
        av_log(avctx, AV_LOG_ERROR, "V4L2: Failed to get capabilities: %s\n", strerror(err));
        return AVERROR(err);
    }

    // Could be made table driven if we have a few more but right now there
    // seems no point

    // Meson (amlogic) always gives a resolution changed event after output
    // streamon and userspace must (re)allocate capture buffers and streamon
    // capture to clear the event even if the capture buffers were the right
    // size in the first place.
    if (strcmp(cap.driver, "meson-vdec") == 0)
        s->quirks |= FF_V4L2_QUIRK_REINIT_ALWAYS | FF_V4L2_QUIRK_ENUM_FRAMESIZES_BROKEN;

    av_log(avctx, AV_LOG_DEBUG, "Driver '%s': Quirks=%#x\n", cap.driver, s->quirks);
    return 0;
}

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
//    output->height = capture->height = avctx->coded_height;
//    output->width = capture->width = avctx->coded_width;
    output->height = capture->height = 0;
    output->width = capture->width = 0;

    output->av_codec_id = avctx->codec_id;
    output->av_pix_fmt  = AV_PIX_FMT_NONE;
    output->min_buf_size = max_coded_size(avctx);

    capture->av_codec_id = AV_CODEC_ID_RAWVIDEO;
    capture->av_pix_fmt = avctx->pix_fmt;
    capture->min_buf_size = 0;

    /* the client requests the codec to generate DRM frames:
     *   - data[0] will therefore point to the returned AVDRMFrameDescriptor
     *       check the ff_v4l2_buffer_to_avframe conversion function.
     *   - the DRM frame format is passed in the DRM frame descriptor layer.
     *       check the v4l2_get_drm_frame function.
     */

    avctx->sw_pix_fmt = avctx->pix_fmt;
    gf_pix_fmt = ff_get_format(avctx, avctx->codec->pix_fmts);
    av_log(avctx, AV_LOG_DEBUG, "avctx requested=%d (%s) %dx%d; get_format requested=%d (%s)\n",
           avctx->pix_fmt, av_get_pix_fmt_name(avctx->pix_fmt),
           avctx->coded_width, avctx->coded_height,
           gf_pix_fmt, av_get_pix_fmt_name(gf_pix_fmt));

    if (gf_pix_fmt == AV_PIX_FMT_DRM_PRIME || avctx->pix_fmt == AV_PIX_FMT_DRM_PRIME) {
        avctx->pix_fmt = AV_PIX_FMT_DRM_PRIME;
        s->output_drm = 1;
    }
    else {
        capture->av_pix_fmt = gf_pix_fmt;
        s->output_drm = 0;
    }

    s->db_ctl = NULL;
    if (priv->dmabuf_alloc != NULL && strcmp(priv->dmabuf_alloc, "v4l2") != 0) {
        if (strcmp(priv->dmabuf_alloc, "cma") == 0)
            s->db_ctl = dmabufs_ctl_new();
        else {
            av_log(avctx, AV_LOG_ERROR, "Unknown dmabuf alloc method: '%s'\n", priv->dmabuf_alloc);
            return AVERROR(EINVAL);
        }
        if (!s->db_ctl) {
            av_log(avctx, AV_LOG_ERROR, "Can't open dmabuf provider '%s'\n", priv->dmabuf_alloc);
            return AVERROR(ENOMEM);
        }
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

    if (avctx->extradata &&
        (ret = copy_extradata(avctx, avctx->extradata, avctx->extradata_size, &s->extdata_data, &s->extdata_size)) != 0) {
        av_log(avctx, AV_LOG_ERROR, "Failed to copy extradata from context: %s\n", av_err2str(ret));
#if DUMP_FAILED_EXTRADATA
        log_dump(avctx, AV_LOG_INFO, avctx->extradata, avctx->extradata_size);
#endif
        return ret;
    }

    if ((ret = v4l2_prepare_decoder(s)) < 0)
        return ret;

    if ((ret = get_quirks(avctx, s)) != 0)
        return ret;

    if ((ret = check_size(avctx, s)) != 0)
        return ret;

    return 0;
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

    av_log(avctx, AV_LOG_TRACE, "<<< %s: streamon=%d\n", __func__, output->streamon);

    // Reflushing everything is benign, quick and avoids having to worry about
    // states like EOS processing so don't try to optimize out (having got it
    // wrong once)

    ff_v4l2_context_set_status(output, VIDIOC_STREAMOFF);

    // Clear any buffered input packet
    av_packet_unref(&s->buf_pkt);

    // Clear a pending EOS
    if (ff_v4l2_ctx_eos(capture)) {
        // Arguably we could delay this but this is easy and doesn't require
        // thought or extra vars
        ff_v4l2_context_set_status(capture, VIDIOC_STREAMOFF);
        ff_v4l2_context_set_status(capture, VIDIOC_STREAMON);
    }

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
    { "dmabuf_alloc", "Dmabuf alloc method", OFFSET(dmabuf_alloc), AV_OPT_TYPE_STRING, {.str = NULL}, 0, 0, FLAGS },
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
