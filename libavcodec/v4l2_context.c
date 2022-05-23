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

static inline int ctx_buffers_alloced(const V4L2Context * const ctx)
{
    return ctx->bufrefs != NULL;
}

// Width/Height changed or we don't have an alloc in the first place?
static int ctx_resolution_changed(const V4L2Context *ctx, const struct v4l2_format *fmt2)
{
    const struct v4l2_format *fmt1 = &ctx->format;
    int ret = !ctx_buffers_alloced(ctx) ||
        (V4L2_TYPE_IS_MULTIPLANAR(ctx->type) ?
            fmt1->fmt.pix_mp.width != fmt2->fmt.pix_mp.width ||
            fmt1->fmt.pix_mp.height != fmt2->fmt.pix_mp.height
            :
            fmt1->fmt.pix.width != fmt2->fmt.pix.width ||
            fmt1->fmt.pix.height != fmt2->fmt.pix.height);

    if (ret)
        av_log(logger(ctx), AV_LOG_DEBUG, "V4L2 %s changed: alloc=%d (%dx%d) -> (%dx%d)\n",
            ctx->name,
            ctx_buffers_alloced(ctx),
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
    struct v4l2_format cap_fmt = s->capture.format;

    s->capture.done = 0;

    ret = ioctl(s->fd, VIDIOC_G_FMT, &cap_fmt);
    if (ret) {
        av_log(avctx, AV_LOG_ERROR, "%s VIDIOC_G_FMT failed\n", s->capture.name);
        return 0;
    }

    get_default_selection(&s->capture, &s->capture.selection);

    reinit = ctx_resolution_changed(&s->capture, &cap_fmt);
    if ((s->quirks & FF_V4L2_QUIRK_REINIT_ALWAYS) != 0)
        reinit = 1;

    s->capture.format = cap_fmt;
    if (reinit) {
        s->capture.height = ff_v4l2_get_format_height(&cap_fmt);
        s->capture.width = ff_v4l2_get_format_width(&cap_fmt);
    }

    // If we don't support selection (or it is bust) and we obviously have HD then kludge
    if ((s->capture.selection.width == 0 || s->capture.selection.height == 0) &&
        (s->capture.height == 1088 && s->capture.width == 1920)) {
        s->capture.selection = (struct v4l2_rect){.width = 1920, .height = 1080};
    }

    s->capture.sample_aspect_ratio = v4l2_get_sar(&s->capture);

    av_log(avctx, AV_LOG_DEBUG, "Source change: SAR: %d/%d, wxh %dx%d crop %dx%d @ %d,%d, reinit=%d\n",
           s->capture.sample_aspect_ratio.num, s->capture.sample_aspect_ratio.den,
           s->capture.width, s->capture.height,
           s->capture.selection.width, s->capture.selection.height,
           s->capture.selection.left, s->capture.selection.top, reinit);

    if (reinit) {
        if (avctx)
            ret = ff_set_dimensions(s->avctx,
                                    s->capture.selection.width != 0 ? s->capture.selection.width : s->capture.width,
                                    s->capture.selection.height != 0 ? s->capture.selection.height : s->capture.height);
        if (ret < 0)
            av_log(avctx, AV_LOG_WARNING, "update avcodec height and width failed\n");

        ret = ff_v4l2_m2m_codec_reinit(s);
        if (ret) {
            av_log(avctx, AV_LOG_ERROR, "v4l2_m2m_codec_reinit failed\n");
            return AVERROR(EINVAL);
        }

        if (s->capture.width > ff_v4l2_get_format_width(&s->capture.format) ||
            s->capture.height > ff_v4l2_get_format_height(&s->capture.format)) {
            av_log(avctx, AV_LOG_ERROR, "Format post reinit too small: wanted %dx%d > got %dx%d\n",
                   s->capture.width, s->capture.height,
                   ff_v4l2_get_format_width(&s->capture.format), ff_v4l2_get_format_height(&s->capture.format));
            return AVERROR(EINVAL);
        }

        // Update pixel format - should only actually do something on initial change
        s->capture.av_pix_fmt =
            ff_v4l2_format_v4l2_to_avfmt(ff_v4l2_get_format_pixelformat(&s->capture.format), AV_CODEC_ID_RAWVIDEO);
        if (s->output_drm) {
            avctx->pix_fmt = AV_PIX_FMT_DRM_PRIME;
            avctx->sw_pix_fmt = s->capture.av_pix_fmt;
        }
        else
            avctx->pix_fmt = s->capture.av_pix_fmt;

        goto reinit_run;
    }

    /* Buffers are OK so just stream off to ack */
    av_log(avctx, AV_LOG_DEBUG, "%s: Parameters only - restart decode\n", __func__);

    ret = ff_v4l2_context_set_status(&s->capture, VIDIOC_STREAMOFF);
    if (ret)
        av_log(avctx, AV_LOG_ERROR, "capture VIDIOC_STREAMOFF failed\n");
    s->draining = 0;

    /* reinit executed */
reinit_run:
    ret = ff_v4l2_context_set_status(&s->capture, VIDIOC_STREAMON);
    return 1;
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

// DQ a buffer
// Amalgamates all the various ways there are of signalling EOS/Event to
// generate a consistant EPIPE.
//
// Sets ctx->flag_last if next dq would produce EPIPE (i.e. stream has stopped)
//
// Returns:
//  0               Success
//  AVERROR(EPIPE)  Nothing more to read
//  AVERROR(ENOSPC) No buffers in Q to put result in
//  *               AVERROR(..)

 static int
dq_buf(V4L2Context * const ctx, V4L2Buffer ** const ppavbuf)
{
    V4L2m2mContext * const m = ctx_to_m2mctx(ctx);
    AVCodecContext * const avctx = m->avctx;
    V4L2Buffer * avbuf;
    const int is_mp = V4L2_TYPE_IS_MULTIPLANAR(ctx->type);

    struct v4l2_plane planes[VIDEO_MAX_PLANES] = {{0}};

    struct v4l2_buffer buf = {
        .type = ctx->type,
        .memory = V4L2_MEMORY_MMAP,
    };

    *ppavbuf = NULL;

    if (ctx->flag_last)
        return AVERROR(EPIPE);

    if (is_mp) {
        buf.length = VIDEO_MAX_PLANES;
        buf.m.planes = planes;
    }

    while (ioctl(m->fd, VIDIOC_DQBUF, &buf) != 0) {
        const int err = errno;
        av_assert0(AVERROR(err) < 0);
        if (err != EINTR) {
            av_log(avctx, AV_LOG_DEBUG, "%s VIDIOC_DQBUF, errno (%s)\n",
                ctx->name, av_err2str(AVERROR(err)));

            if (err == EPIPE)
                ctx->flag_last = 1;

            return AVERROR(err);
        }
    }
    atomic_fetch_sub(&ctx->q_count, 1);

    avbuf = (V4L2Buffer *)ctx->bufrefs[buf.index]->data;
    avbuf->status = V4L2BUF_AVAILABLE;
    avbuf->buf = buf;
    if (is_mp) {
        memcpy(avbuf->planes, planes, sizeof(planes));
        avbuf->buf.m.planes = avbuf->planes;
    }

    if (V4L2_TYPE_IS_CAPTURE(ctx->type)) {
        // Zero length cap buffer return == EOS
        if ((is_mp ? buf.m.planes[0].bytesused : buf.bytesused) == 0) {
            av_log(avctx, AV_LOG_DEBUG, "Buffer empty - reQ\n");

            // Must reQ so we don't leak
            // May not matter if the next thing we do is release all the
            // buffers but better to be tidy.
            ff_v4l2_buffer_enqueue(avbuf);

            ctx->flag_last = 1;
            return AVERROR(EPIPE);
        }

#ifdef V4L2_BUF_FLAG_LAST
        // If flag_last set then this contains data but is the last frame
        // so remember that but return OK
        if ((buf.flags & V4L2_BUF_FLAG_LAST) != 0)
            ctx->flag_last = 1;
#endif
    }

    *ppavbuf = avbuf;
    return 0;
}

/**
 * handle resolution change event and end of stream event
 * Expects to be called after the stream has stopped
 *
 * returns 1 if reinit was successful, negative if it failed
 * returns 0 if reinit was not executed
 */
static int
get_event(V4L2m2mContext * const m)
{
    AVCodecContext * const avctx = m->avctx;
    struct v4l2_event evt = { 0 };

    while (ioctl(m->fd, VIDIOC_DQEVENT, &evt) != 0) {
        const int rv = AVERROR(errno);
        if (rv == AVERROR(EINTR))
            continue;
        if (rv == AVERROR(EAGAIN)) {
            av_log(avctx, AV_LOG_WARNING, "V4L2 failed to get expected event - assume EOS\n");
            return AVERROR_EOF;
        }
        av_log(avctx, AV_LOG_ERROR, "V4L2 VIDIOC_DQEVENT: %s\n", av_err2str(rv));
        return rv;
    }

    av_log(avctx, AV_LOG_DEBUG, "Dq event %d\n", evt.type);

    if (evt.type == V4L2_EVENT_EOS) {
        av_log(avctx, AV_LOG_TRACE, "V4L2 VIDIOC_EVENT_EOS\n");
        return AVERROR_EOF;
    }

    if (evt.type == V4L2_EVENT_SOURCE_CHANGE)
        return do_source_change(m);

    return 0;
}


// Get a buffer
// If output then just gets the buffer in the expected way
// If capture then runs the capture state m/c to deal with res change etc.
// If return value == 0 then *ppavbuf != NULL

static int
get_qbuf(V4L2Context * const ctx, V4L2Buffer ** const ppavbuf, const int timeout)
{
    V4L2m2mContext * const m = ctx_to_m2mctx(ctx);
    AVCodecContext * const avctx = m->avctx;
    const int is_cap = V4L2_TYPE_IS_CAPTURE(ctx->type);

    const unsigned int poll_cap = (POLLIN | POLLRDNORM);
    const unsigned int poll_out = (POLLOUT | POLLWRNORM);
    const unsigned int poll_event = POLLPRI;

    *ppavbuf = NULL;

    for (;;) {
        struct pollfd pfd = {
            .fd = m->fd,
            // If capture && stream not started then assume we are waiting for the initial event
            .events = !is_cap ? poll_out :
                !ff_v4l2_ctx_eos(ctx) && ctx->streamon ? poll_cap :
                    poll_event,
        };
        int ret;

        if (ctx->done) {
            av_log(avctx, AV_LOG_TRACE, "V4L2 %s already done\n", ctx->name);
            return AVERROR_EOF;
        }

        // If capture && timeout == -1 then also wait for rx buffer free
        if (is_cap && timeout == -1 && m->output.streamon && !m->draining)
            pfd.events |= poll_out;

        // If nothing Qed all we will get is POLLERR - avoid that
        if ((pfd.events == poll_out && atomic_load(&m->output.q_count) == 0) ||
            (pfd.events == poll_cap && atomic_load(&m->capture.q_count) == 0) ||
            (pfd.events == (poll_cap | poll_out) && atomic_load(&m->capture.q_count) == 0 && atomic_load(&m->output.q_count) == 0)) {
            av_log(avctx, AV_LOG_TRACE, "V4L2 poll %s empty\n", ctx->name);
            return AVERROR(ENOSPC);
        }

        // Timeout kludged s.t. "forever" eventually gives up & produces logging
        // If waiting for an event when we have seen a last_frame then we expect
        //   it to be ready already so force a short timeout
        ret = poll(&pfd, 1,
                   ff_v4l2_ctx_eos(ctx) ? 10 :
                   timeout == -1 ? 3000 : timeout);
        if (ret < 0) {
            ret = AVERROR(errno);  // Remember errno before logging etc.
            av_assert0(ret < 0);
        }

        av_log(avctx, AV_LOG_TRACE, "V4L2 poll %s ret=%d, timeout=%d, events=%#x, revents=%#x\n",
               ctx->name, ret, timeout, pfd.events, pfd.revents);

        if (ret < 0) {
            if (ret == AVERROR(EINTR))
                continue;
            av_log(avctx, AV_LOG_ERROR, "V4L2 %s poll error %d (%s)\n", ctx->name, AVUNERROR(ret), av_err2str(ret));
            return ret;
        }

        if (ret == 0) {
            if (timeout == -1)
                av_log(avctx, AV_LOG_ERROR, "V4L2 %s poll unexpected timeout: events=%#x\n", ctx->name, pfd.events);
            if (ff_v4l2_ctx_eos(ctx)) {
                av_log(avctx, AV_LOG_WARNING, "V4L2 %s poll event timeout\n", ctx->name);
                ret = get_event(m);
                if (ret < 0) {
                    ctx->done = 1;
                    return ret;
                }
            }
            return AVERROR(EAGAIN);
        }

        if ((pfd.revents & POLLERR) != 0) {
            av_log(avctx, AV_LOG_WARNING, "V4L2 %s POLLERR\n", ctx->name);
            return AVERROR_UNKNOWN;
        }

        if ((pfd.revents & poll_event) != 0) {
            ret = get_event(m);
            if (ret < 0) {
                ctx->done = 1;
                return ret;
            }
            continue;
        }

        if ((pfd.revents & poll_cap) != 0) {
            ret = dq_buf(ctx, ppavbuf);
            if (ret == AVERROR(EPIPE))
                continue;
            return ret;
        }

        if ((pfd.revents & poll_out) != 0) {
            if (is_cap)
                return AVERROR(EAGAIN);
            return dq_buf(ctx, ppavbuf);
        }

        av_log(avctx, AV_LOG_ERROR, "V4L2 poll unexpected events=%#x, revents=%#x\n", pfd.events, pfd.revents);
        return AVERROR_UNKNOWN;
    }
}

// Clear out flags and timestamps that should should be set by the user
// Returns the passed avbuf
static V4L2Buffer *
clean_v4l2_buffer(V4L2Buffer * const avbuf)
{
    struct v4l2_buffer *const buf = &avbuf->buf;

    buf->flags = 0;
    buf->field = V4L2_FIELD_ANY;
    buf->timestamp = (struct timeval){0};
    buf->timecode = (struct v4l2_timecode){0};
    buf->sequence = 0;

    return avbuf;
}

static V4L2Buffer* v4l2_getfree_v4l2buf(V4L2Context *ctx)
{
    int i;

    /* get back as many output buffers as possible */
    if (V4L2_TYPE_IS_OUTPUT(ctx->type)) {
        V4L2Buffer * avbuf;
        do {
            get_qbuf(ctx, &avbuf, 0);
        } while (avbuf);
    }

    for (i = 0; i < ctx->num_buffers; i++) {
        V4L2Buffer * const avbuf = (V4L2Buffer *)ctx->bufrefs[i]->data;
        if (avbuf->status == V4L2BUF_AVAILABLE)
            return clean_v4l2_buffer(avbuf);
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
    atomic_store(&ctx->q_count, 0);

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
    atomic_store(&ctx->q_count, 0);
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
    int ret = 0;
    AVCodecContext * const avctx = logger(ctx);

    // Avoid doing anything if there is nothing we can do
    if (cmd == VIDIOC_STREAMOFF && !ctx_buffers_alloced(ctx) && !ctx->streamon)
        return 0;

    ff_mutex_lock(&ctx->lock);

    if (cmd == VIDIOC_STREAMON && !V4L2_TYPE_IS_OUTPUT(ctx->type))
        stuff_all_buffers(avctx, ctx);

    if (ioctl(ctx_to_m2mctx(ctx)->fd, cmd, &type) < 0) {
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

    // Both stream off & on effectively clear flag_last
    ctx->flag_last = 0;

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
    int rv;

    if ((rv = get_qbuf(ctx, &avbuf, timeout)) != 0)
        return rv;

    return ff_v4l2_buffer_buf_to_avframe(frame, avbuf);
}

int ff_v4l2_context_dequeue_packet(V4L2Context* ctx, AVPacket* pkt)
{
    V4L2Buffer *avbuf;
    int rv;

    if ((rv = get_qbuf(ctx, &avbuf, -1)) != 0)
        return rv == AVERROR(ENOSPC) ? AVERROR(EAGAIN) : rv;  // Caller not currently expecting ENOSPC

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
    pthread_cond_destroy(&ctx->cond);
}


static int create_buffers(V4L2Context* const ctx, const unsigned int req_buffers)
{
    V4L2m2mContext * const s = ctx_to_m2mctx(ctx);
    struct v4l2_requestbuffers req;
    int ret;
    int i;

    av_assert0(ctx->bufrefs == NULL);

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
    pthread_cond_init(&ctx->cond, NULL);
    atomic_init(&ctx->q_count, 0);

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
        hwframes->width = ctx->width != 0 ? ctx->width : s->avctx->width;
        hwframes->height = ctx->height != 0 ? ctx->height : s->avctx->height;
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
