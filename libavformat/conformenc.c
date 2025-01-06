/*
 * Copyright (c) 2020 John Cox for Raspberry Pi Trading
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


// *** This module is a work in progress and its utility is strictly
//     limited to testing.

#include "libavutil/opt.h"
#include "libavutil/frame.h"
#include "libavutil/md5.h"
#include "libavutil/mem.h"
#include "libavutil/pixdesc.h"
#include "libavutil/hwcontext.h"
#include "libavutil/hwcontext_drm.h"
#include "mux.h"

#include "pthread.h"
#include <semaphore.h>
#include <unistd.h>

#define TRACE_ALL 1

#define DRM_MODULE "vc4"

// Aux size should only need to be 2, but on a few streams (Hobbit) under FKMS
// we get initial flicker probably due to dodgy drm timing
#define AUX_SIZE 3
typedef struct conform_display_env_s
{
    AVClass *class;

    void * line_buf;
    size_t line_size;

    struct AVMD5 * md5;

    int conform_file;

    unsigned long long foffset;
    unsigned int fno;
} conform_display_env_t;


static int conform_vout_write_trailer(AVFormatContext *s)
{
    conform_display_env_t * const de = s->priv_data;

#if TRACE_ALL
    av_log(s, AV_LOG_DEBUG, "%s\n", __func__);
#endif

    if (de->md5) {
        uint8_t m[16];
        av_md5_final(de->md5, m);
        avio_printf(s->pb, "MD5=%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
                    m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8], m[9], m[10], m[11], m[12], m[13], m[14], m[15]);
    }

    return 0;
}

static int conform_vout_write_header(AVFormatContext *s)
{
    conform_display_env_t * const de = s->priv_data;

#if TRACE_ALL
    av_log(s, AV_LOG_DEBUG, "%s\n", __func__);
#endif

    if (de->md5)
        av_md5_init(de->md5);
    de->fno = 1;
    de->foffset = 0;

    return 0;
}

static int conform_vout_write_packet(AVFormatContext *s, AVPacket *pkt)
{
    const AVFrame * const sf = (AVFrame *)pkt->data;
    AVFrame * cf = NULL;
    const AVFrame * f = sf;

    conform_display_env_t * const de = s->priv_data;
    const AVPixFmtDescriptor * pix_desc = av_pix_fmt_desc_get(sf->format);
    int is_hw = (pix_desc->flags & AV_PIX_FMT_FLAG_HWACCEL) != 0;
    enum AVPixelFormat fmt = is_hw ? AV_PIX_FMT_NONE : sf->format;
    unsigned int i;

    if (is_hw) {
        enum AVPixelFormat *xfmts = NULL;
        av_hwframe_transfer_get_formats(sf->hw_frames_ctx, AV_HWFRAME_TRANSFER_DIRECTION_FROM, &xfmts, 0);
        fmt = *xfmts;
        av_free(xfmts);
    }

    av_log(s, AV_LOG_DEBUG, "%s: Frame %3d: %#08llx %dx%d crop(ltrb) %zd,%zd,%zd,%zd fmt %s -> %s\n", __func__,
           de->fno, de->foffset,
           sf->width, sf->height, sf->crop_left, sf->crop_top, sf->crop_right, sf->crop_bottom,
           av_get_pix_fmt_name(sf->format), av_get_pix_fmt_name(fmt));
    av_log(s, AV_LOG_DEBUG, "%s: Data ref count=%d, data=%p\n", __func__, av_buffer_get_ref_count(sf->buf[0]), sf->data[0]);

    if ((sf->flags & AV_FRAME_FLAG_CORRUPT) != 0) {
        av_log(s, AV_LOG_WARNING, "Discard corrupt frame: fmt=%d, ts=%" PRId64 "\n", sf->format, sf->pts);
        return 0;
    }

    if (!is_hw)
        f = sf;
    else
    {
        cf = av_frame_alloc();
        cf->format = fmt;
        av_hwframe_transfer_data(cf, sf, AV_HWFRAME_TRANSFER_DIRECTION_FROM);
        pix_desc = av_pix_fmt_desc_get(cf->format);
        f = cf;
    }

    // This is fully generic - much optimisation possible
    for (i = 0; i != pix_desc->nb_components; ++i) {
        const AVComponentDescriptor * const cd = pix_desc->comp + i;
        const unsigned int srw = ((i == 1 || i == 2) ? pix_desc->log2_chroma_w : 0);
        const unsigned int rndw = (1 << srw) - 1;
        const unsigned int srh = ((i == 1 || i == 2) ? pix_desc->log2_chroma_h : 0);
        const unsigned int rndh = (1 << srh) - 1;
        const unsigned int srp = cd->shift;
        const unsigned int bpp = cd->depth > 8 ? 2 : 1;
        const unsigned int h = (f->height - (f->crop_top + f->crop_bottom) + rndh) >> srh;
        unsigned int y;
        av_log(s, AV_LOG_DEBUG, "[%d] srp=%d\n", i, srp);
        for (y = 0; y < h; ++y) {
            const void *const lstart = f->data[cd->plane] + (y + (f->crop_top >> srh)) * f->linesize[cd->plane] + cd->offset + (f->crop_left >> srw) * cd->step;
            const unsigned int w = (f->width - (f->crop_left + f->crop_right) + rndw) >> srw;
            unsigned int x;
            if (bpp == 1) {
                uint8_t *d = de->line_buf;
                const uint8_t *s = lstart;
                for (x = 0; x != w; ++x) {
                    *d++ = *s >> srp;
                    s += cd->step;
                }
            }
            else {
                uint16_t *d = de->line_buf;
                const uint8_t *s = lstart;
                for (x = 0; x != w; ++x) {
                    *d++ = *(uint16_t*)s >> srp;
                    s += cd->step;
                }
            }

            // We have one line

            if (de->md5)
                av_md5_update(de->md5, de->line_buf, w * bpp);
            else {
                avio_write(s->pb, de->line_buf, w * bpp);
                de->foffset += w * bpp;
            }
        }
    }
    ++de->fno;

    av_log(s, AV_LOG_DEBUG, "%s: Done: Data ref count=%d\n", __func__, av_buffer_get_ref_count(sf->buf[0]));

    av_frame_free(&cf);

    return 0;
}

static int conform_vout_write_frame(AVFormatContext *s, int stream_index, AVFrame **ppframe,
                          unsigned flags)
{
    av_log(s, AV_LOG_ERROR, "%s: NIF: idx=%d, flags=%#x\n", __func__, stream_index, flags);
    return AVERROR_PATCHWELCOME;
}

// deinit is called if init fails so no need to clean up explicity here
static int conform_vout_init(struct AVFormatContext * s)
{
    conform_display_env_t * const de = s->priv_data;

    av_log(s, AV_LOG_DEBUG, "<<< %s\n", __func__);

    de->line_size = (8192 * 4); // 4bpp * 8k seems plenty
    de->line_buf = av_malloc(de->line_size);

    if (!de->conform_file)
        de->md5 = av_md5_alloc();

    av_log(s, AV_LOG_DEBUG, ">>> %s\n", __func__);

    return 0;
}

static void conform_vout_deinit(struct AVFormatContext * s)
{
    conform_display_env_t * const de = s->priv_data;

    av_log(s, AV_LOG_DEBUG, "<<< %s\n", __func__);

    av_freep(&de->line_buf);
    av_freep(&de->md5);

    av_log(s, AV_LOG_DEBUG, ">>> %s\n", __func__);
}


#define OFFSET(x) offsetof(conform_display_env_t, x)
static const AVOption options[] = {
    { "conform_yuv", "Output yuv file rather than md5", OFFSET(conform_file), AV_OPT_TYPE_BOOL, { .i64 = 0 }, 0, 1, AV_OPT_FLAG_ENCODING_PARAM },
    { NULL }
};

static const AVClass conform_vid_class = {
    .class_name = "conform vid muxer",
    .item_name  = av_default_item_name,
    .option     = options,
    .version    = LIBAVUTIL_VERSION_INT,
    .category   = AV_CLASS_CATEGORY_DEVICE_VIDEO_OUTPUT,
};

const FFOutputFormat ff_conform_muxer = {
    .p = {
        .name           = "conform",
        .long_name      = NULL_IF_CONFIG_SMALL("Video out conformance test helper"),
        .audio_codec    = AV_CODEC_ID_NONE,
        .video_codec    = AV_CODEC_ID_WRAPPED_AVFRAME,
        .flags          = AVFMT_VARIABLE_FPS | AVFMT_NOTIMESTAMPS,
        .priv_class     = &conform_vid_class,
    },
    .priv_data_size = sizeof(conform_display_env_t),
    .write_header   = conform_vout_write_header,
    .write_packet   = conform_vout_write_packet,
    .write_uncoded_frame = conform_vout_write_frame,
    .write_trailer  = conform_vout_write_trailer,
    .init           = conform_vout_init,
    .deinit         = conform_vout_deinit,
};

