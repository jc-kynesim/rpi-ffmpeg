/*
 * V4L2 context helper functions.
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
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include "libavutil/avassert.h"
#include "libavcodec/avcodec.h"
#include "decode.h"
#include "v4l2_buffers.h"
#include "v4l2_fmt.h"
#include "v4l2_m2m.h"
#include "weak_link.h"

struct v4l2_format_update {
    uint32_t v4l2_fmt;
    int update_v4l2;

    enum AVPixelFormat av_fmt;
    int update_avfmt;
};

static inline V4L2m2mContext *ctx_to_m2mctx(const V4L2Context *ctx)
{
    return V4L2_TYPE_IS_OUTPUT(ctx->type) ?
        container_of(ctx, V4L2m2mContext, output) :
        container_of(ctx, V4L2m2mContext, capture);
}

static inline AVCodecContext *logger(const V4L2Context *ctx)
{
    return ctx_to_m2mctx(ctx)->avctx;
}

static AVRational v4l2_get_sar(V4L2Context *ctx)
{
    struct AVRational sar = { 0, 1 };
    struct v4l2_cropcap cropcap;
    int ret;

    memset(&cropcap, 0, sizeof(cropcap));
    cropcap.type = ctx->type;

    ret = ioctl(ctx_to_m2mctx(ctx)->fd, VIDIOC_CROPCAP, &cropcap);
    if (ret)
        return sar;

    sar.num = cropcap.pixelaspect.numerator;
    sar.den = cropcap.pixelaspect.denominator;
    return sar;
}

static inline unsigned int v4l2_resolution_changed(V4L2Context *ctx, struct v4l2_format *fmt2)
{
    struct v4l2_format *fmt1 = &ctx->format;
    int ret =  V4L2_TYPE_IS_MULTIPLANAR(ctx->type) ?
        fmt1->fmt.pix_mp.width != fmt2->fmt.pix_mp.width ||
        fmt1->fmt.pix_mp.height != fmt2->fmt.pix_mp.height
        :
        fmt1->fmt.pix.width != fmt2->fmt.pix.width ||
        fmt1->fmt.pix.height != fmt2->fmt.pix.height;

    if (ret)
        av_log(logger(ctx), AV_LOG_DEBUG, "%s changed (%dx%d) -> (%dx%d)\n",
            ctx->name,
            ff_v4l2_get_format_width(fmt1), ff_v4l2_get_format_height(fmt1),
            ff_v4l2_get_format_width(fmt2), ff_v4l2_get_format_height(fmt2));

    return ret;
}

static inline int v4l2_type_supported(V4L2Context *ctx)
{
    return ctx->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE ||
        ctx->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE ||
        ctx->type == V4L2_BUF_TYPE_VIDEO_CAPTURE ||
        ctx->type == V4L2_BUF_TYPE_VIDEO_OUTPUT;
}

static inline int v4l2_get_framesize_compressed(V4L2Context* ctx, int width, int height)
{
    V4L2m2mContext *s = ctx_to_m2mctx(ctx);
    const int SZ_4K = 0x1000;
    int size;

    if (s->avctx && av_codec_is_decoder(s->avctx->codec))
        return ((width * height * 3 / 2) / 2) + 128;

    /* encoder */
    size = FFALIGN(height, 32) * FFALIGN(width, 32) * 3 / 2 / 2;
    return FFALIGN(size, SZ_4K);
}

static inline void v4l2_save_to_context(V4L2Context* ctx, struct v4l2_format_update *fmt)
{
    ctx->format.type = ctx->type;

    if (fmt->update_avfmt)
        ctx->av_pix_fmt = fmt->av_fmt;

    if (V4L2_TYPE_IS_MULTIPLANAR(ctx->type)) {
        /* update the sizes to handle the reconfiguration of the capture stream at runtime */
        ctx->format.fmt.pix_mp.height = ctx->height;
        ctx->format.fmt.pix_mp.width = ctx->width;
        if (fmt->update_v4l2) {
            ctx->format.fmt.pix_mp.pixelformat = fmt->v4l2_fmt;

            /* s5p-mfc requires the user to specify a buffer size */
            ctx->format.fmt.pix_mp.plane_fmt[0].sizeimage =
                v4l2_get_framesize_compressed(ctx, ctx->width, ctx->height);
        }
    } else {
        ctx->format.fmt.pix.height = ctx->height;
        ctx->format.fmt.pix.width = ctx->width;
        if (fmt->update_v4l2) {
            ctx->format.fmt.pix.pixelformat = fmt->v4l2_fmt;

            /* s5p-mfc requires the user to specify a buffer size */
            ctx->format.fmt.pix.sizeimage =
                v4l2_get_framesize_compressed(ctx, ctx->width, ctx->height);
        }
    }
}

static int get_default_selection(V4L2Context * const ctx, struct v4l2_rect *r)
{
    V4L2m2mContext * const s = ctx_to_m2mctx(ctx);
    struct v4l2_selection selection = {
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .target = V4L2_SEL_TGT_COMPOSE
    };

    memset(r, 0, sizeof(*r));
    if (ioctl(s->fd, VIDIOC_G_SELECTION, &selection))
        return AVERROR(errno);

    *r = selection.r;
    return 0;
}

static int do_source_change(V4L2m2mContext * const s)
{
    AVCodecContext *const avctx = s->avctx;

    int ret;
    int reinit;
    int full_reinit;
    struct v4l2_format cap_fmt = s->capture.format;

    s->resize_pending = 0;
    s->capture.done = 0;

    ret = ioctl(s->fd, VIDIOC_G_FMT, &cap_fmt);
    if (ret) {
        av_log(avctx, AV_LOG_ERROR, "%s VIDIOC_G_FMT failed\n", s->capture.name);
        return 0;
    }

    s->output.sample_aspect_ratio = v4l2_get_sar(&s->output);

    get_default_selection(&s->capture, &s->capture.selection);

    reinit = v4l2_resolution_changed(&s->capture, &cap_fmt);
    if (reinit) {
        s->capture.height = ff_v4l2_get_format_height(&cap_fmt);
        s->capture.width = ff_v4l2_get_format_width(&cap_fmt);
    }
    s->capture.sample_aspect_ratio = v4l2_get_sar(&s->capture);

    av_log(avctx, AV_LOG_DEBUG, "Source change: SAR: %d/%d, crop %dx%d @ %d,%d\n",
           s->capture.sample_aspect_ratio.num, s->capture.sample_aspect_ratio.den,
           s->capture.selection.width, s->capture.selection.height,
           s->capture.selection.left, s->capture.selection.top);

    s->reinit = 1;

    if (reinit) {
        if (avctx)
            ret = ff_set_dimensions(s->avctx, s->capture.width, s->capture.height);
        if (ret < 0)
            av_log(avctx, AV_LOG_WARNING, "update avcodec height and width failed\n");

        ret = ff_v4l2_m2m_codec_reinit(s);
        if (ret) {
            av_log(avctx, AV_LOG_ERROR, "v4l2_m2m_codec_reinit failed\n");
            return AVERROR(EINVAL);
        }
        goto reinit_run;
    }

    /* Buffers are OK so just stream off to ack */
    av_log(avctx, AV_LOG_DEBUG, "%s: Parameters only\n", __func__);

    ret = ff_v4l2_context_set_status(&s->capture, VIDIOC_STREAMOFF);
    if (ret)
        av_log(avctx, AV_LOG_ERROR, "capture VIDIOC_STREAMOFF failed\n");
    s->draining = 0;

    /* reinit executed */
reinit_run:
    ret = ff_v4l2_context_set_status(&s->capture, VIDIOC_STREAMON);
    return 1;
}

static int ctx_done(V4L2Context * const ctx)
{
    int rv = 0;
    V4L2m2mContext * const s = ctx_to_m2mctx(ctx);

    ctx->done = 1;

    if (s->resize_pending && !V4L2_TYPE_IS_OUTPUT(ctx->type))
        rv = do_source_change(s);

    return rv;
}

/**
 * handle resolution change event and end of stream event
 * returns 1 if reinit was successful, negative if it failed
 * returns 0 if reinit was not executed
 */
static int v4l2_handle_event(V4L2Context *ctx)
{
    V4L2m2mContext * const s = ctx_to_m2mctx(ctx);
    struct v4l2_event evt = { 0 };
    int ret;

    ret = ioctl(s->fd, VIDIOC_DQEVENT, &evt);
    if (ret < 0) {
        av_log(logger(ctx), AV_LOG_ERROR, "%s VIDIOC_DQEVENT\n", ctx->name);
        return 0;
    }

    av_log(logger(ctx), AV_LOG_INFO, "Dq event %d\n", evt.type);

    if (evt.type == V4L2_EVENT_EOS) {
//        ctx->done = 1;
        av_log(logger(ctx), AV_LOG_TRACE, "%s VIDIOC_EVENT_EOS\n", ctx->name);
        return 0;
    }

    if (evt.type != V4L2_EVENT_SOURCE_CHANGE)
        return 0;

    s->resize_pending = 1;
    if (!ctx->done)
        return 0;

    return do_source_change(s);
}

static int v4l2_stop_decode(V4L2Context *ctx)
{
    struct v4l2_decoder_cmd cmd = {
        .cmd = V4L2_DEC_CMD_STOP,
        .flags = 0,
    };
    int ret;

    ret = ioctl(ctx_to_m2mctx(ctx)->fd, VIDIOC_DECODER_CMD, &cmd);
    if (ret) {
        /* DECODER_CMD is optional */
        if (errno == ENOTTY)
            return ff_v4l2_context_set_status(ctx, VIDIOC_STREAMOFF);
        else
            return AVERROR(errno);
    }

    return 0;
}

static int v4l2_stop_encode(V4L2Context *ctx)
{
    struct v4l2_encoder_cmd cmd = {
        .cmd = V4L2_ENC_CMD_STOP,
        .flags = 0,
    };
    int ret;

    ret = ioctl(ctx_to_m2mctx(ctx)->fd, VIDIOC_ENCODER_CMD, &cmd);
    if (ret) {
        /* ENCODER_CMD is optional */
        if (errno == ENOTTY)
            return ff_v4l2_context_set_status(ctx, VIDIOC_STREAMOFF);
        else
            return AVERROR(errno);
    }

    return 0;
}

static int count_in_driver(const V4L2Context * const ctx)
{
    int i;
    int n = 0;

    if (!ctx->bufrefs)
        return -1;

    for (i = 0; i < ctx->num_buffers; ++i) {
        V4L2Buffer *const avbuf = (V4L2Buffer *)ctx->bufrefs[i]->data;
        if (avbuf->status == V4L2BUF_IN_DRIVER)
            ++n;
    }
    return n;
}

static V4L2Buffer* v4l2_dequeue_v4l2buf(V4L2Context *ctx, int timeout)
{
    V4L2m2mContext * const s = ctx_to_m2mctx(ctx);
    const int is_capture = !V4L2_TYPE_IS_OUTPUT(ctx->type);
    struct v4l2_plane planes[VIDEO_MAX_PLANES];
    struct v4l2_buffer buf = { 0 };
    V4L2Buffer *avbuf;
    struct pollfd pfd = {
        .events =  POLLIN | POLLRDNORM | POLLPRI | POLLOUT | POLLWRNORM, /* default blocking capture */
        .fd = ctx_to_m2mctx(ctx)->fd,
    };
    int i, ret;
    int no_rx_means_done = 0;

    if (is_capture && ctx->bufrefs) {
        for (i = 0; i < ctx->num_buffers; i++) {
            avbuf = (V4L2Buffer *)ctx->bufrefs[i]->data;
            if (avbuf->status == V4L2BUF_IN_DRIVER)
                break;
        }
        if (i == ctx->num_buffers)
            av_log(logger(ctx), AV_LOG_WARNING, "All capture buffers (%d) returned to "
                                                "userspace. Increase num_capture_buffers "
                                                "to prevent device deadlock or dropped "
                                                "packets/frames.\n", i);
    }

#if 0
    // I think this is true but pointless
    // we will get some other form of EOF signal

    /* if we are draining and there are no more capture buffers queued in the driver we are done */
    if (is_capture && ctx_to_m2mctx(ctx)->draining) {
        for (i = 0; i < ctx->num_buffers; i++) {
            /* capture buffer initialization happens during decode hence
             * detection happens at runtime
             */
            if (!ctx->bufrefs)
                break;

            avbuf = (V4L2Buffer *)ctx->bufrefs[i]->data;
            if (avbuf->status == V4L2BUF_IN_DRIVER)
                goto start;
        }
        ctx->done = 1;
        return NULL;
    }
#endif

start:
    if (is_capture) {
        /* no need to listen to requests for more input while draining */
        if (ctx_to_m2mctx(ctx)->draining || timeout > 0)
            pfd.events =  POLLIN | POLLRDNORM | POLLPRI;
    } else {
        pfd.events =  POLLOUT | POLLWRNORM;
    }
    no_rx_means_done = s->resize_pending && is_capture;

    for (;;) {
        // If we have a resize pending then all buffers should be Qed
        // With a resize pending we should be in drain but evidence suggests
        // that not all decoders do this so poll to clear
        int t2 = no_rx_means_done ? 0 : timeout < 0 ? 3000 : timeout;
        const int e = pfd.events;

        ret = poll(&pfd, 1, t2);

        if (ret > 0)
            break;

        if (ret < 0) {
            int err = errno;
            if (err == EINTR)
                continue;
            av_log(logger(ctx), AV_LOG_ERROR, "=== poll error %d (%s): events=%#x, cap buffers=%d\n",
                   err, strerror(err),
                   e, count_in_driver(ctx));
            return NULL;
        }

        // ret == 0 (timeout)
        if (no_rx_means_done) {
            av_log(logger(ctx), AV_LOG_DEBUG, "Ctx done on timeout\n");
            ret = ctx_done(ctx);
            if (ret > 0)
                goto start;
        }
        if (timeout == -1)
            av_log(logger(ctx), AV_LOG_ERROR, "=== poll unexpected TIMEOUT: events=%#x, cap buffers=%d\n", e, count_in_driver(ctx));;
        return NULL;
    }

    /* 0. handle errors */
    if (pfd.revents & POLLERR) {
        /* if we are trying to get free buffers but none have been queued yet
           no need to raise a warning */
        if (timeout == 0) {
            for (i = 0; i < ctx->num_buffers; i++) {
                avbuf = (V4L2Buffer *)ctx->bufrefs[i]->data;
                if (avbuf->status != V4L2BUF_AVAILABLE)
                    av_log(logger(ctx), AV_LOG_WARNING, "%s POLLERR\n", ctx->name);
            }
        }
        else
            av_log(logger(ctx), AV_LOG_WARNING, "%s POLLERR\n", ctx->name);

        return NULL;
    }

    /* 1. handle resolution changes */
    if (pfd.revents & POLLPRI) {
        ret = v4l2_handle_event(ctx);
        if (ret < 0) {
            /* if re-init failed, abort */
            ctx->done = 1;
            return NULL;
        }
        if (ret > 0)
            goto start;
    }

    /* 2. dequeue the buffer */
    if (pfd.revents & (POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM)) {

        if (is_capture) {
            /* there is a capture buffer ready */
            if (pfd.revents & (POLLIN | POLLRDNORM))
                goto dequeue;

            // CAPTURE Q drained
            if (no_rx_means_done) {
                if (ctx_done(ctx) > 0)
                    goto start;
                return NULL;
            }

            /* the driver is ready to accept more input; instead of waiting for the capture
             * buffer to complete we return NULL so input can proceed (we are single threaded)
             */
            if (pfd.revents & (POLLOUT | POLLWRNORM))
                return NULL;
        }

dequeue:
        memset(&buf, 0, sizeof(buf));
        buf.memory = V4L2_MEMORY_MMAP;
        buf.type = ctx->type;
        if (V4L2_TYPE_IS_MULTIPLANAR(ctx->type)) {
            memset(planes, 0, sizeof(planes));
            buf.length = VIDEO_MAX_PLANES;
            buf.m.planes = planes;
        }

        while ((ret = ioctl(ctx_to_m2mctx(ctx)->fd, VIDIOC_DQBUF, &buf)) == -1) {
            const int err = errno;
            if (err == EINTR)
                continue;
            if (err != EAGAIN) {
                // EPIPE on CAPTURE can be used instead of BUF_FLAG_LAST
                if (err != EPIPE || !is_capture)
                    av_log(logger(ctx), AV_LOG_DEBUG, "%s VIDIOC_DQBUF, errno (%s)\n",
                        ctx->name, av_err2str(AVERROR(err)));
                if (ctx_done(ctx) > 0)
                    goto start;
            }
            return NULL;
        }
        --ctx->q_count;
        av_log(logger(ctx), AV_LOG_DEBUG, "--- %s VIDIOC_DQBUF OK: index=%d, ts=%ld.%06ld, count=%d, dq=%d field=%d\n",
               ctx->name, buf.index,
               buf.timestamp.tv_sec, buf.timestamp.tv_usec,
               ctx->q_count, ++ctx->dq_count, buf.field);

        avbuf = (V4L2Buffer *)ctx->bufrefs[buf.index]->data;
        avbuf->status = V4L2BUF_AVAILABLE;
        avbuf->buf = buf;
        if (V4L2_TYPE_IS_MULTIPLANAR(ctx->type)) {
            memcpy(avbuf->planes, planes, sizeof(planes));
            avbuf->buf.m.planes = avbuf->planes;
        }

        if (ctx_to_m2mctx(ctx)->draining && is_capture) {
            int bytesused = V4L2_TYPE_IS_MULTIPLANAR(buf.type) ?
                            buf.m.planes[0].bytesused : buf.bytesused;
            if (bytesused == 0) {
                av_log(logger(ctx), AV_LOG_DEBUG, "Buffer empty - reQ\n");

                // Must reQ so we don't leak
                // May not matter if the next thing we do is release all the
                // buffers but better to be tidy.
                ff_v4l2_buffer_enqueue(avbuf);

                if (ctx_done(ctx) > 0)
                    goto start;
                return NULL;
            }
#ifdef V4L2_BUF_FLAG_LAST
            if (buf.flags & V4L2_BUF_FLAG_LAST) {
                av_log(logger(ctx), AV_LOG_TRACE, "FLAG_LAST set\n");
                avbuf->status = V4L2BUF_IN_USE;  // Avoid flushing this buffer
                ctx_done(ctx);
            }
#endif
        }

        return avbuf;
    }

    return NULL;
}

static V4L2Buffer* v4l2_getfree_v4l2buf(V4L2Context *ctx)
{
    int timeout = 0; /* return when no more buffers to dequeue */
    int i;

    /* get back as many output buffers as possible */
    if (V4L2_TYPE_IS_OUTPUT(ctx->type)) {
          do {
          } while (v4l2_dequeue_v4l2buf(ctx, timeout));
    }

    for (i = 0; i < ctx->num_buffers; i++) {
        V4L2Buffer * const avbuf = (V4L2Buffer *)ctx->bufrefs[i]->data;
        if (avbuf->status == V4L2BUF_AVAILABLE)
            return avbuf;
    }

    return NULL;
}

static int v4l2_release_buffers(V4L2Context* ctx)
{
    int i;
    int ret = 0;
    const int fd = ctx_to_m2mctx(ctx)->fd;

    // Orphan any buffers in the wild
    ff_weak_link_break(&ctx->wl_master);

    if (ctx->bufrefs) {
        for (i = 0; i < ctx->num_buffers; i++)
            av_buffer_unref(ctx->bufrefs + i);
    }

    if (fd != -1) {
        struct v4l2_requestbuffers req = {
            .memory = V4L2_MEMORY_MMAP,
            .type = ctx->type,
            .count = 0, /* 0 -> unmap all buffers from the driver */
        };

        while ((ret = ioctl(fd, VIDIOC_REQBUFS, &req)) == -1) {
            if (errno == EINTR)
                continue;

            ret = AVERROR(errno);

            av_log(logger(ctx), AV_LOG_ERROR, "release all %s buffers (%s)\n",
                ctx->name, av_err2str(AVERROR(errno)));

            if (ctx_to_m2mctx(ctx)->output_drm)
                av_log(logger(ctx), AV_LOG_ERROR,
                    "Make sure the DRM client releases all FB/GEM objects before closing the codec (ie):\n"
                    "for all buffers: \n"
                    "  1. drmModeRmFB(..)\n"
                    "  2. drmIoctl(.., DRM_IOCTL_GEM_CLOSE,... )\n");
        }
    }
    ctx->q_count = 0;

    return ret;
}

static inline int v4l2_try_raw_format(V4L2Context* ctx, enum AVPixelFormat pixfmt)
{
    struct v4l2_format *fmt = &ctx->format;
    uint32_t v4l2_fmt;
    int ret;

    v4l2_fmt = ff_v4l2_format_avfmt_to_v4l2(pixfmt);
    if (!v4l2_fmt)
        return AVERROR(EINVAL);

    if (V4L2_TYPE_IS_MULTIPLANAR(ctx->type))
        fmt->fmt.pix_mp.pixelformat = v4l2_fmt;
    else
        fmt->fmt.pix.pixelformat = v4l2_fmt;

    fmt->type = ctx->type;

    ret = ioctl(ctx_to_m2mctx(ctx)->fd, VIDIOC_TRY_FMT, fmt);
    if (ret)
        return AVERROR(EINVAL);

    return 0;
}

static int v4l2_get_raw_format(V4L2Context* ctx, enum AVPixelFormat *p)
{
    V4L2m2mContext* s = ctx_to_m2mctx(ctx);
    V4L2m2mPriv *priv = s->avctx->priv_data;
    enum AVPixelFormat pixfmt = ctx->av_pix_fmt;
    struct v4l2_fmtdesc fdesc;
    int ret;

    memset(&fdesc, 0, sizeof(fdesc));
    fdesc.type = ctx->type;

    if (pixfmt != AV_PIX_FMT_NONE) {
        ret = v4l2_try_raw_format(ctx, pixfmt);
        if (!ret)
            return 0;
    }

    for (;;) {
        ret = ioctl(ctx_to_m2mctx(ctx)->fd, VIDIOC_ENUM_FMT, &fdesc);
        if (ret)
            return AVERROR(EINVAL);

        if (priv->pix_fmt != AV_PIX_FMT_NONE) {
            if (fdesc.pixelformat != ff_v4l2_format_avfmt_to_v4l2(priv->pix_fmt)) {
                fdesc.index++;
                continue;
            }
        }

        pixfmt = ff_v4l2_format_v4l2_to_avfmt(fdesc.pixelformat, AV_CODEC_ID_RAWVIDEO);
        ret = v4l2_try_raw_format(ctx, pixfmt);
        if (ret){
            fdesc.index++;
            continue;
        }

        *p = pixfmt;

        return 0;
    }

    return AVERROR(EINVAL);
}

static int v4l2_get_coded_format(V4L2Context* ctx, uint32_t *p)
{
    struct v4l2_fmtdesc fdesc;
    uint32_t v4l2_fmt;
    int ret;

    /* translate to a valid v4l2 format */
    v4l2_fmt = ff_v4l2_format_avcodec_to_v4l2(ctx->av_codec_id);
    if (!v4l2_fmt)
        return AVERROR(EINVAL);

    /* check if the driver supports this format */
    memset(&fdesc, 0, sizeof(fdesc));
    fdesc.type = ctx->type;

    for (;;) {
        ret = ioctl(ctx_to_m2mctx(ctx)->fd, VIDIOC_ENUM_FMT, &fdesc);
        if (ret)
            return AVERROR(EINVAL);

        if (fdesc.pixelformat == v4l2_fmt)
            break;

        fdesc.index++;
    }

    *p = v4l2_fmt;

    return 0;
}

 /*****************************************************************************
  *
  *             V4L2 Context Interface
  *
  *****************************************************************************/


static void flush_all_buffers_status(V4L2Context* const ctx)
{
    int i;

    if (!ctx->bufrefs)
        return;

    for (i = 0; i < ctx->num_buffers; ++i) {
        struct V4L2Buffer * const buf = (struct V4L2Buffer *)ctx->bufrefs[i]->data;
        if (buf->status == V4L2BUF_IN_DRIVER)
            buf->status = V4L2BUF_AVAILABLE;
    }
    ctx->q_count = 0;
}

static int stuff_all_buffers(AVCodecContext * avctx, V4L2Context* ctx)
{
    int i;
    int rv;

    if (!ctx->bufrefs) {
        rv = ff_v4l2_context_init(ctx);
        if (rv) {
            av_log(avctx, AV_LOG_ERROR, "can't request capture buffers\n");
            return rv;
        }
    }

    for (i = 0; i < ctx->num_buffers; ++i) {
        struct V4L2Buffer * const buf = (struct V4L2Buffer *)ctx->bufrefs[i]->data;
        if (buf->status == V4L2BUF_AVAILABLE) {
            rv = ff_v4l2_buffer_enqueue(buf);
            if (rv < 0)
                return rv;
        }
    }
    return 0;
}

int ff_v4l2_context_set_status(V4L2Context* ctx, uint32_t cmd)
{
    int type = ctx->type;
    int ret;
    AVCodecContext * const avctx = logger(ctx);

    ff_mutex_lock(&ctx->lock);

    if (cmd == VIDIOC_STREAMON && !V4L2_TYPE_IS_OUTPUT(ctx->type))
        stuff_all_buffers(avctx, ctx);

    ret = ioctl(ctx_to_m2mctx(ctx)->fd, cmd, &type);
    if (ret < 0) {
        const int err = errno;
        av_log(avctx, AV_LOG_ERROR, "%s set status %d (%s) failed: err=%d\n", ctx->name,
               cmd, (cmd == VIDIOC_STREAMON) ? "ON" : "OFF", err);
        ret = AVERROR(err);
    }
    else
    {
        if (cmd == VIDIOC_STREAMOFF)
            flush_all_buffers_status(ctx);

        ctx->streamon = (cmd == VIDIOC_STREAMON);
        av_log(avctx, AV_LOG_DEBUG, "%s set status %d (%s) OK\n", ctx->name,
               cmd, (cmd == VIDIOC_STREAMON) ? "ON" : "OFF");
    }

    ff_mutex_unlock(&ctx->lock);

    return ret;
}

int ff_v4l2_context_enqueue_frame(V4L2Context* ctx, const AVFrame* frame)
{
    V4L2m2mContext *s = ctx_to_m2mctx(ctx);
    V4L2Buffer* avbuf;
    int ret;

    if (!frame) {
        ret = v4l2_stop_encode(ctx);
        if (ret)
            av_log(logger(ctx), AV_LOG_ERROR, "%s stop_encode\n", ctx->name);
        s->draining= 1;
        return 0;
    }

    avbuf = v4l2_getfree_v4l2buf(ctx);
    if (!avbuf)
        return AVERROR(EAGAIN);

    ret = ff_v4l2_buffer_avframe_to_buf(frame, avbuf);
    if (ret)
        return ret;

    return ff_v4l2_buffer_enqueue(avbuf);
}

int ff_v4l2_context_enqueue_packet(V4L2Context* ctx, const AVPacket* pkt,
                                   const void * extdata, size_t extlen)
{
    V4L2m2mContext *s = ctx_to_m2mctx(ctx);
    V4L2Buffer* avbuf;
    int ret;

    if (!pkt->size) {
        ret = v4l2_stop_decode(ctx);
        // Log but otherwise ignore stop failure
        if (ret)
            av_log(logger(ctx), AV_LOG_ERROR, "%s stop_decode failed: err=%d\n", ctx->name, ret);
        s->draining = 1;
        return 0;
    }

    avbuf = v4l2_getfree_v4l2buf(ctx);
    if (!avbuf)
        return AVERROR(EAGAIN);

    ret = ff_v4l2_buffer_avpkt_to_buf_ext(pkt, avbuf, extdata, extlen);
    if (ret == AVERROR(ENOMEM))
        av_log(logger(ctx), AV_LOG_ERROR, "Buffer overflow in %s: pkt->size=%d > buf->length=%d\n",
               __func__, pkt->size, avbuf->planes[0].length);
    else if (ret)
        return ret;

    return ff_v4l2_buffer_enqueue(avbuf);
}

int ff_v4l2_context_dequeue_frame(V4L2Context* ctx, AVFrame* frame, int timeout)
{
    V4L2Buffer *avbuf;

    /*
     * timeout=-1 blocks until:
     *  1. decoded frame available
     *  2. an input buffer is ready to be dequeued
     */
    avbuf = v4l2_dequeue_v4l2buf(ctx, timeout);
    if (!avbuf) {
        if (ctx->done)
            return AVERROR_EOF;

        return AVERROR(EAGAIN);
    }

    return ff_v4l2_buffer_buf_to_avframe(frame, avbuf);
}

int ff_v4l2_context_dequeue_packet(V4L2Context* ctx, AVPacket* pkt)
{
    V4L2Buffer *avbuf;

    /*
     * blocks until:
     *  1. encoded packet available
     *  2. an input buffer ready to be dequeued
     */
    avbuf = v4l2_dequeue_v4l2buf(ctx, -1);
    if (!avbuf) {
        if (ctx->done)
            return AVERROR_EOF;

        return AVERROR(EAGAIN);
    }

    return ff_v4l2_buffer_buf_to_avpkt(pkt, avbuf);
}

int ff_v4l2_context_get_format(V4L2Context* ctx, int probe)
{
    struct v4l2_format_update fmt = { 0 };
    int ret;

    if  (ctx->av_codec_id == AV_CODEC_ID_RAWVIDEO) {
        ret = v4l2_get_raw_format(ctx, &fmt.av_fmt);
        if (ret)
            return ret;

        fmt.update_avfmt = !probe;
        v4l2_save_to_context(ctx, &fmt);

        /* format has been tried already */
        return ret;
    }

    ret = v4l2_get_coded_format(ctx, &fmt.v4l2_fmt);
    if (ret)
        return ret;

    fmt.update_v4l2 = 1;
    v4l2_save_to_context(ctx, &fmt);

    return ioctl(ctx_to_m2mctx(ctx)->fd, VIDIOC_TRY_FMT, &ctx->format);
}

int ff_v4l2_context_set_format(V4L2Context* ctx)
{
    int ret;

    ret = ioctl(ctx_to_m2mctx(ctx)->fd, VIDIOC_S_FMT, &ctx->format);
    if (ret != 0)
        return ret;

    // Check returned size against min size and if smaller have another go
    // Only worry about plane[0] as this is meant to enforce limits for
    // encoded streams where we might know a bit more about the shape
    // than the driver
    if (V4L2_TYPE_IS_MULTIPLANAR(ctx->format.type)) {
        if (ctx->min_buf_size <= ctx->format.fmt.pix_mp.plane_fmt[0].sizeimage)
            return 0;
        ctx->format.fmt.pix_mp.plane_fmt[0].sizeimage = ctx->min_buf_size;
    }
    else {
        if (ctx->min_buf_size <= ctx->format.fmt.pix.sizeimage)
            return 0;
        ctx->format.fmt.pix.sizeimage = ctx->min_buf_size;
    }

    ret = ioctl(ctx_to_m2mctx(ctx)->fd, VIDIOC_S_FMT, &ctx->format);
    return ret;
}

void ff_v4l2_context_release(V4L2Context* ctx)
{
    int ret;

    if (!ctx->bufrefs)
        return;

    ret = v4l2_release_buffers(ctx);
    if (ret)
        av_log(logger(ctx), AV_LOG_WARNING, "V4L2 failed to unmap the %s buffers\n", ctx->name);

    av_freep(&ctx->bufrefs);
    av_buffer_unref(&ctx->frames_ref);

    ff_mutex_destroy(&ctx->lock);
}


static int create_buffers(V4L2Context* const ctx, const unsigned int req_buffers)
{
    V4L2m2mContext * const s = ctx_to_m2mctx(ctx);
    struct v4l2_requestbuffers req;
    int ret;
    int i;

    memset(&req, 0, sizeof(req));
    req.count = req_buffers;
    req.memory = V4L2_MEMORY_MMAP;
    req.type = ctx->type;
    while ((ret = ioctl(s->fd, VIDIOC_REQBUFS, &req)) == -1) {
        if (errno != EINTR) {
            ret = AVERROR(errno);
            av_log(logger(ctx), AV_LOG_ERROR, "%s VIDIOC_REQBUFS failed: %s\n", ctx->name, av_err2str(ret));
            return ret;
        }
    }

    ctx->num_buffers = req.count;
    ctx->bufrefs = av_mallocz(ctx->num_buffers * sizeof(*ctx->bufrefs));
    if (!ctx->bufrefs) {
        av_log(logger(ctx), AV_LOG_ERROR, "%s malloc enomem\n", ctx->name);
        goto fail_release;
    }

    ctx->wl_master = ff_weak_link_new(ctx);
    if (!ctx->wl_master) {
        ret = AVERROR(ENOMEM);
        goto fail_release;
    }

    for (i = 0; i < ctx->num_buffers; i++) {
        ret = ff_v4l2_buffer_initialize(&ctx->bufrefs[i], i, ctx);
        if (ret) {
            av_log(logger(ctx), AV_LOG_ERROR, "%s buffer[%d] initialization (%s)\n", ctx->name, i, av_err2str(ret));
            goto fail_release;
        }
    }

    av_log(logger(ctx), AV_LOG_DEBUG, "%s: %s %02d buffers initialized: %04ux%04u, sizeimage %08u, bytesperline %08u\n", ctx->name,
        V4L2_TYPE_IS_MULTIPLANAR(ctx->type) ? av_fourcc2str(ctx->format.fmt.pix_mp.pixelformat) : av_fourcc2str(ctx->format.fmt.pix.pixelformat),
        req.count,
        ff_v4l2_get_format_width(&ctx->format),
        ff_v4l2_get_format_height(&ctx->format),
        V4L2_TYPE_IS_MULTIPLANAR(ctx->type) ? ctx->format.fmt.pix_mp.plane_fmt[0].sizeimage : ctx->format.fmt.pix.sizeimage,
        V4L2_TYPE_IS_MULTIPLANAR(ctx->type) ? ctx->format.fmt.pix_mp.plane_fmt[0].bytesperline : ctx->format.fmt.pix.bytesperline);

    return 0;

fail_release:
    v4l2_release_buffers(ctx);
    av_freep(&ctx->bufrefs);
    return ret;
}

int ff_v4l2_context_init(V4L2Context* ctx)
{
    V4L2m2mContext * const s = ctx_to_m2mctx(ctx);
    int ret;

    // It is not valid to reinit a context without a previous release
    av_assert0(ctx->bufrefs == NULL);

    if (!v4l2_type_supported(ctx)) {
        av_log(logger(ctx), AV_LOG_ERROR, "type %i not supported\n", ctx->type);
        return AVERROR_PATCHWELCOME;
    }

    ff_mutex_init(&ctx->lock, NULL);

    if (s->output_drm) {
        AVHWFramesContext *hwframes;

        ctx->frames_ref = av_hwframe_ctx_alloc(s->device_ref);
        if (!ctx->frames_ref) {
            ret = AVERROR(ENOMEM);
            goto fail_unlock;
        }

        hwframes = (AVHWFramesContext*)ctx->frames_ref->data;
        hwframes->format = AV_PIX_FMT_DRM_PRIME;
        hwframes->sw_format = ctx->av_pix_fmt;
        hwframes->width = ctx->width;
        hwframes->height = ctx->height;
        ret = av_hwframe_ctx_init(ctx->frames_ref);
        if (ret < 0)
            goto fail_unref_hwframes;
    }

    ret = ioctl(s->fd, VIDIOC_G_FMT, &ctx->format);
    if (ret) {
        ret = AVERROR(errno);
        av_log(logger(ctx), AV_LOG_ERROR, "%s VIDIOC_G_FMT failed: %s\n", ctx->name, av_err2str(ret));
        goto fail_unref_hwframes;
    }

    ret = create_buffers(ctx, ctx->num_buffers);
    if (ret < 0)
        goto fail_unref_hwframes;

    return 0;

fail_unref_hwframes:
    av_buffer_unref(&ctx->frames_ref);
fail_unlock:
    ff_mutex_destroy(&ctx->lock);
    return ret;
}
