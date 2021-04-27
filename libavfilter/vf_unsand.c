/*
 * Copyright (c) 2007 Bobby Bingham
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

/**
 * @file
 * format and noformat video filters
 */

#include <string.h>

#include "libavutil/internal.h"
#include "libavutil/mem.h"
#include "libavutil/pixdesc.h"
#include "libavutil/opt.h"
#include "libavutil/rpi_sand_fns.h"

#include "avfilter.h"
#include "formats.h"
#include "internal.h"
#include "video.h"

typedef struct UnsandContext {
    const AVClass *class;
} UnsandContext;

static av_cold void uninit(AVFilterContext *ctx)
{
//    UnsandContext *s = ctx->priv;
}

static av_cold int init(AVFilterContext *ctx)
{
//    UnsandContext *s = ctx->priv;

    return 0;
}


static int filter_frame(AVFilterLink *link, AVFrame *in)
{
    AVFilterLink * const outlink = link->dst->outputs[0];
    AVFrame *out = NULL;
    int rv = 0;

    if (outlink->format == in->format) {
        // If nothing to do then do nothing
        out = in;
    }
    else
    {
        if ((out = ff_get_video_buffer(outlink, av_frame_cropped_width(in), av_frame_cropped_height(in))) == NULL)
        {
            rv = AVERROR(ENOMEM);
            goto fail;
        }
        if (av_rpi_sand_to_planar_frame(out, in) != 0)
        {
            rv = -1;
            goto fail;
        }

        av_frame_free(&in);
    }

    return ff_filter_frame(outlink, out);

fail:
    av_frame_free(&out);
    av_frame_free(&in);
    return rv;
}

#if 0
static void dump_fmts(const AVFilterFormats * fmts)
{
    int i;
    if (fmts== NULL) {
        printf("NULL\n");
        return;
    }
    for (i = 0; i < fmts->nb_formats; ++i) {
        printf(" %d", fmts->formats[i]);
    }
    printf("\n");
}
#endif

static int query_formats(AVFilterContext *ctx)
{
//    UnsandContext *s = ctx->priv;
    int ret;

    // If we aren't connected at both ends then just do nothing
    if (ctx->inputs[0] == NULL || ctx->outputs[0] == NULL)
        return 0;

    // Our output formats depend on our input formats and we can't/don't
    // want to convert between bit depths so we need to wait for the source
    // to have an opinion before we do
    if (ctx->inputs[0]->incfg.formats == NULL)
        return AVERROR(EAGAIN);

    // Accept anything
    if (ctx->inputs[0]->outcfg.formats == NULL &&
        (ret = ff_formats_ref(ctx->inputs[0]->incfg.formats, &ctx->inputs[0]->outcfg.formats)) < 0)
        return ret;

    // Filter out sand formats

    // Generate a container if we don't already have one
    if (ctx->outputs[0]->incfg.formats == NULL)
    {
        // Somewhat rubbish way of ensuring we have a good structure
        const static enum AVPixelFormat out_fmts[] = {AV_PIX_FMT_YUV420P10, AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE};
        AVFilterFormats *formats = ff_make_format_list(out_fmts);

        if (formats == NULL)
            return AVERROR(ENOMEM);
        if ((ret = ff_formats_ref(formats, &ctx->outputs[0]->incfg.formats)) < 0)
            return ret;
    }

    // Replace old format list with new filtered list derived from what our
    // input says it can do
    {
        const AVFilterFormats * const src_ff = ctx->inputs[0]->outcfg.formats;
        AVFilterFormats * const dst_ff = ctx->outputs[0]->incfg.formats;
        enum AVPixelFormat *dst_fmts = av_malloc(sizeof(enum AVPixelFormat) * src_ff->nb_formats);
        int i;
        int n = 0;
        int seen_420p = 0;
        int seen_420p10 = 0;

        for (i = 0; i < src_ff->nb_formats; ++i) {
            const enum AVPixelFormat f = src_ff->formats[i];

            switch (f){
                case AV_PIX_FMT_YUV420P:
                case AV_PIX_FMT_SAND128:
                case AV_PIX_FMT_RPI4_8:
                    if (!seen_420p) {
                        seen_420p = 1;
                        dst_fmts[n++] = AV_PIX_FMT_YUV420P;
                    }
                    break;
                case AV_PIX_FMT_SAND64_10:
                case AV_PIX_FMT_YUV420P10:
                case AV_PIX_FMT_RPI4_10:
                    if (!seen_420p10) {
                        seen_420p10 = 1;
                        dst_fmts[n++] = AV_PIX_FMT_YUV420P10;
                    }
                    break;
                default:
                    dst_fmts[n++] = f;
                    break;
            }
        }

        av_freep(&dst_ff->formats);
        dst_ff->formats = dst_fmts;
        dst_ff->nb_formats = n;
    }

//    printf("Unsand: %s calc: ", __func__);
//    dump_fmts(ctx->outputs[0]->incfg.formats);

    return 0;
}


#define OFFSET(x) offsetof(UnsandContext, x)
static const AVOption unsand_options[] = {
    { NULL }
};


AVFILTER_DEFINE_CLASS(unsand);

static const AVFilterPad avfilter_vf_unsand_inputs[] = {
    {
        .name             = "default",
        .type             = AVMEDIA_TYPE_VIDEO,
        .filter_frame = filter_frame,
    },
    { NULL }
};

static const AVFilterPad avfilter_vf_unsand_outputs[] = {
    {
        .name = "default",
        .type = AVMEDIA_TYPE_VIDEO
    },
};

AVFilter ff_vf_unsand = {
    .name          = "unsand",
    .description   = NULL_IF_CONFIG_SMALL("Convert sand pix fmt to yuv"),

    .init          = init,
    .uninit        = uninit,

    FILTER_QUERY_FUNC(query_formats),

    .priv_size     = sizeof(UnsandContext),
    .priv_class    = &unsand_class,

    FILTER_INPUTS(avfilter_vf_unsand_inputs),
    FILTER_OUTPUTS(avfilter_vf_unsand_outputs),
};

