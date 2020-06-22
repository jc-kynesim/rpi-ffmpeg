/*
 * Raw Video Encoder
 * Copyright (c) 2001 Fabrice Bellard
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
 * Raw Video Encoder
 */

#include "config.h"
#include "avcodec.h"
#include "raw.h"
#include "internal.h"
#include "libavutil/pixdesc.h"
#include "libavutil/intreadwrite.h"
#include "libavutil/imgutils.h"
#include "libavutil/internal.h"
#include "libavutil/avassert.h"
#if CONFIG_SAND
#include "libavutil/rpi_sand_fns.h"
#endif

static av_cold int raw_encode_init(AVCodecContext *avctx)
{
    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(avctx->pix_fmt);

#if FF_API_CODED_FRAME
FF_DISABLE_DEPRECATION_WARNINGS
    avctx->coded_frame->pict_type = AV_PICTURE_TYPE_I;
FF_ENABLE_DEPRECATION_WARNINGS
#endif
    avctx->bits_per_coded_sample = av_get_bits_per_pixel(desc);
    if(!avctx->codec_tag)
        avctx->codec_tag = avcodec_pix_fmt_to_codec_tag(avctx->pix_fmt);
    avctx->bit_rate = ff_guess_coded_bitrate(avctx);

    return 0;
}

#if CONFIG_SAND
static int raw_sand8_as_yuv420(AVCodecContext *avctx, AVPacket *pkt,
                      const AVFrame *frame)
{
    const int width = av_frame_cropped_width(frame);
    const int height = av_frame_cropped_height(frame);
    const int x0 = frame->crop_left;
    const int y0 = frame->crop_top;
    const int size = width * height * 3 / 2;
    uint8_t * dst;
    int ret;

    if ((ret = ff_alloc_packet2(avctx, pkt, size, size)) < 0)
        return ret;

    dst = pkt->data;

    av_rpi_sand_to_planar_y8(dst, width, frame->data[0], frame->linesize[0], frame->linesize[3], x0, y0, width, height);
    dst += width * height;
    av_rpi_sand_to_planar_c8(dst, width / 2, dst + width * height / 4, width / 2,
                          frame->data[1], frame->linesize[1], av_rpi_sand_frame_stride2(frame), x0 / 2, y0 / 2, width / 2, height / 2);
    return 0;
}

static int raw_sand16_as_yuv420(AVCodecContext *avctx, AVPacket *pkt,
                      const AVFrame *frame)
{
    const int width = av_frame_cropped_width(frame);
    const int height = av_frame_cropped_height(frame);
    const int x0 = frame->crop_left;
    const int y0 = frame->crop_top;
    const int size = width * height * 3;
    uint8_t * dst;
    int ret;

    if ((ret = ff_alloc_packet2(avctx, pkt, size, size)) < 0)
        return ret;

    dst = pkt->data;

    av_rpi_sand_to_planar_y16(dst, width * 2, frame->data[0], frame->linesize[0], frame->linesize[3], x0 * 2, y0, width * 2, height);
    dst += width * height * 2;
    av_rpi_sand_to_planar_c16(dst, width, dst + width * height / 2, width,
                          frame->data[1], frame->linesize[1], av_rpi_sand_frame_stride2(frame), x0, y0 / 2, width, height / 2);
    return 0;
}

static int raw_sand30_as_yuv420(AVCodecContext *avctx, AVPacket *pkt,
                      const AVFrame *frame)
{
    const int width = av_frame_cropped_width(frame);
    const int height = av_frame_cropped_height(frame);
    const int x0 = frame->crop_left;
    const int y0 = frame->crop_top;
    const int size = width * height * 3;
    uint8_t * dst;
    int ret;

    if ((ret = ff_alloc_packet2(avctx, pkt, size, size)) < 0)
        return ret;

    dst = pkt->data;

    av_rpi_sand30_to_planar_y16(dst, width * 2, frame->data[0], frame->linesize[0], frame->linesize[3], x0, y0, width, height);
    dst += width * height * 2;
    av_rpi_sand30_to_planar_c16(dst, width, dst + width * height / 2, width,
                          frame->data[1], frame->linesize[1], av_rpi_sand_frame_stride2(frame), x0/2, y0 / 2, width/2, height / 2);
    return 0;
}
#endif


static int raw_encode(AVCodecContext *avctx, AVPacket *pkt,
                      const AVFrame *frame, int *got_packet)
{
    int ret;

#if CONFIG_SAND
    if (av_rpi_is_sand_frame(frame)) {
        ret = av_rpi_is_sand8_frame(frame) ? raw_sand8_as_yuv420(avctx, pkt, frame) :
            av_rpi_is_sand16_frame(frame) ? raw_sand16_as_yuv420(avctx, pkt, frame) :
            av_rpi_is_sand30_frame(frame) ? raw_sand30_as_yuv420(avctx, pkt, frame) : -1;
        *got_packet = (ret == 0);
        return ret;
    }
#endif

    ret = av_image_get_buffer_size(frame->format,
                                       frame->width, frame->height, 1);
    if (ret < 0)
        return ret;

    if ((ret = ff_alloc_packet2(avctx, pkt, ret, ret)) < 0)
        return ret;
    if ((ret = av_image_copy_to_buffer(pkt->data, pkt->size,
                                       (const uint8_t **)frame->data, frame->linesize,
                                       frame->format,
                                       frame->width, frame->height, 1)) < 0)
        return ret;

    if(avctx->codec_tag == AV_RL32("yuv2") && ret > 0 &&
       frame->format   == AV_PIX_FMT_YUYV422) {
        int x;
        for(x = 1; x < frame->height*frame->width*2; x += 2)
            pkt->data[x] ^= 0x80;
    } else if (avctx->codec_tag == AV_RL32("b64a") && ret > 0 &&
        frame->format == AV_PIX_FMT_RGBA64BE) {
        uint64_t v;
        int x;
        for (x = 0; x < frame->height * frame->width; x++) {
            v = AV_RB64(&pkt->data[8 * x]);
            AV_WB64(&pkt->data[8 * x], v << 48 | v >> 16);
        }
    }
    pkt->flags |= AV_PKT_FLAG_KEY;
    *got_packet = 1;
    return 0;
}

AVCodec ff_rawvideo_encoder = {
    .name           = "rawvideo",
    .long_name      = NULL_IF_CONFIG_SMALL("raw video"),
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_RAWVIDEO,
    .init           = raw_encode_init,
    .encode2        = raw_encode,
};
