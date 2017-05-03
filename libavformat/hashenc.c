/*
 * Hash/MD5 encoder (for codec/format testing)
 * Copyright (c) 2009 Reimar Döffinger, based on crcenc (c) 2002 Fabrice Bellard
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

#include "libavutil/avassert.h"
#include "libavutil/avstring.h"
#include "libavutil/hash.h"
#include "libavutil/intreadwrite.h"
#include "libavutil/opt.h"
#include "avformat.h"
#include "internal.h"

#define DEBUG_WRAPPED_FRAME 1

struct HashContext {
    const AVClass *avclass;
    struct AVHashContext *hash;
    char *hash_name;
    int format_version;
};

#define OFFSET(x) offsetof(struct HashContext, x)
#define ENC AV_OPT_FLAG_ENCODING_PARAM
#if CONFIG_HASH_MUXER || CONFIG_FRAMEHASH_MUXER
static const AVOption hash_options[] = {
    { "hash", "set hash to use", OFFSET(hash_name), AV_OPT_TYPE_STRING, {.str = "sha256"}, 0, 0, ENC },
    { "format_version", "file format version", OFFSET(format_version), AV_OPT_TYPE_INT, {.i64 = 2}, 1, 2, ENC },
    { NULL },
};
#endif

#if CONFIG_MD5_MUXER || CONFIG_FRAMEMD5_MUXER
static const AVOption md5_options[] = {
    { "hash", "set hash to use", OFFSET(hash_name), AV_OPT_TYPE_STRING, {.str = "md5"}, 0, 0, ENC },
    { "format_version", "file format version", OFFSET(format_version), AV_OPT_TYPE_INT, {.i64 = 2}, 1, 2, ENC },
    { NULL },
};
#endif

#if CONFIG_HASH_MUXER || CONFIG_MD5_MUXER
static int hash_write_header(struct AVFormatContext *s)
{
    struct HashContext *c = s->priv_data;
    int res = av_hash_alloc(&c->hash, c->hash_name);
    if (res < 0)
        return res;
    av_hash_init(c->hash);
    return 0;
}

static int hash_write_packet(struct AVFormatContext *s, AVPacket *pkt)
{
    struct HashContext *c = s->priv_data;
    printf("%s: %#x @ %p n=%d\n", __func__, pkt->size, pkt->data, s->nb_streams);
    av_hash_update(c->hash, pkt->data, pkt->size);
    return 0;
}

static void update_hash_sand_c(struct HashContext *c, const AVFrame * const frame, const int c_off)
{
    for (int y = 0; y != frame->height / 2; ++y) {
        for (int x = 0; x < frame->width; x += frame->linesize[0]) {
            uint8_t cbuf[128]; // Will deal with SAND256 if needed
            const uint8_t * p = frame->data[1] + x * frame->linesize[3] + y * frame->linesize[0] + c_off;
            for (int i = 0; i < frame->linesize[0] / 2; ++i)
                cbuf[i] = p[i * 2];

            av_hash_update(c->hash,
                cbuf,
                FFMIN(frame->linesize[0], frame->width - x) / 2);
        }
    }
}

static void update_hash_2d(struct HashContext *c,
                           const uint8_t * src, const unsigned int width, const unsigned int height, const unsigned int stride)
{
    if (stride == width) {
        av_hash_update(c->hash, src, width * height);
    }
    else
    {
        for (unsigned int y = 0; y != height; ++y, src += stride) {
            av_hash_update(c->hash, src, width);
        }
    }
}

static int hash_write_packet_v(struct AVFormatContext *s, AVPacket *pkt)
{
    struct HashContext *c = s->priv_data;
    const AVFrame * const frame = (AVFrame *)pkt->data;

    switch (frame->format) {
    case AV_PIX_FMT_SAND128:
        {
            int x, y;
            // Luma is "easy"
            for (y = 0; y != frame->height; ++y) {
                for (x = 0; x < frame->width; x += frame->linesize[0]) {
                    av_hash_update(c->hash,
                        frame->data[0] + x * frame->linesize[3] + y * frame->linesize[0],
                        FFMIN(frame->linesize[0], frame->width - x));
                }
            }
            // Chroma is dull
            update_hash_sand_c(c, frame, 0);
            update_hash_sand_c(c, frame, 1);
        }
        break;

    case AV_PIX_FMT_YUV420P:
        update_hash_2d(c, frame->data[0], frame->width, frame->height, frame->linesize[0]);
        update_hash_2d(c, frame->data[1], frame->width/2, frame->height/2, frame->linesize[1]);
        update_hash_2d(c, frame->data[2], frame->width/2, frame->height/2, frame->linesize[2]);
        break;

    default:
        av_log(NULL, AV_LOG_ERROR, "MD5V can only deal with sand currently\n");
        return -1;
    }
    return 0;
}

static int hash_write_trailer(struct AVFormatContext *s)
{
    struct HashContext *c = s->priv_data;
    char buf[AV_HASH_MAX_SIZE*2+128];
    snprintf(buf, sizeof(buf) - 200, "%s=", av_hash_get_name(c->hash));

    av_hash_final_hex(c->hash, buf + strlen(buf), sizeof(buf) - strlen(buf));
    av_strlcatf(buf, sizeof(buf), "\n");
    avio_write(s->pb, buf, strlen(buf));
    avio_flush(s->pb);

    av_hash_freep(&c->hash);
    return 0;
}
#endif

#if CONFIG_HASH_MUXER
static const AVClass hashenc_class = {
    .class_name = "hash encoder class",
    .item_name  = av_default_item_name,
    .option     = hash_options,
    .version    = LIBAVUTIL_VERSION_INT,
};

AVOutputFormat ff_hash_muxer = {
    .name              = "hash",
    .long_name         = NULL_IF_CONFIG_SMALL("Hash testing"),
    .priv_data_size    = sizeof(struct HashContext),
    .audio_codec       = AV_CODEC_ID_PCM_S16LE,
    .video_codec       = AV_CODEC_ID_RAWVIDEO,
    .write_header      = hash_write_header,
    .write_packet      = hash_write_packet,
    .write_trailer     = hash_write_trailer,
    .flags             = AVFMT_VARIABLE_FPS | AVFMT_TS_NONSTRICT |
                         AVFMT_TS_NEGATIVE,
    .priv_class        = &hashenc_class,
};
#endif

#if CONFIG_MD5_MUXER
static const AVClass md5enc_class = {
    .class_name = "MD5 encoder class",
    .item_name  = av_default_item_name,
    .option     = md5_options,
    .version    = LIBAVUTIL_VERSION_INT,
};

#if !DEBUG_WRAPPED_FRAME
AVOutputFormat ff_md5_muxer = {
    .name              = "md5",
    .long_name         = NULL_IF_CONFIG_SMALL("MD5 testing"),
    .priv_data_size    = sizeof(struct HashContext),
    .audio_codec       = AV_CODEC_ID_PCM_S16LE,
    .video_codec       = AV_CODEC_ID_RAWVIDEO,
    .write_header      = hash_write_header,
    .write_packet      = hash_write_packet,
    .write_trailer     = hash_write_trailer,
    .flags             = AVFMT_VARIABLE_FPS | AVFMT_TS_NONSTRICT |
                         AVFMT_TS_NEGATIVE,
    .priv_class        = &md5enc_class,
};
#else
AVOutputFormat ff_md5_muxer = {
    .name              = "md5",
    .long_name         = NULL_IF_CONFIG_SMALL("MD5 (VFrame) testing"),
    .priv_data_size    = sizeof(struct HashContext),
    .audio_codec       = AV_CODEC_ID_PCM_S16LE,
    .video_codec       = AV_CODEC_ID_WRAPPED_AVFRAME,
    .write_header      = hash_write_header,
    .write_packet      = hash_write_packet_v,
    .write_trailer     = hash_write_trailer,
    .flags             = AVFMT_VARIABLE_FPS | AVFMT_TS_NONSTRICT |
                         AVFMT_TS_NEGATIVE,
    .priv_class        = &md5enc_class,
};
#endif

#endif

#if CONFIG_FRAMEHASH_MUXER || CONFIG_FRAMEMD5_MUXER
static void framehash_print_extradata(struct AVFormatContext *s)
{
    int i;

    for (i = 0; i < s->nb_streams; i++) {
        AVStream *st = s->streams[i];
        AVCodecParameters *par = st->codecpar;
        if (par->extradata) {
            struct HashContext *c = s->priv_data;
            char buf[AV_HASH_MAX_SIZE*2+1];

            avio_printf(s->pb, "#extradata %d, %31d, ", i, par->extradata_size);
            av_hash_init(c->hash);
            av_hash_update(c->hash, par->extradata, par->extradata_size);
            av_hash_final_hex(c->hash, buf, sizeof(buf));
            avio_write(s->pb, buf, strlen(buf));
            avio_printf(s->pb, "\n");
        }
    }
}

static int framehash_write_header(struct AVFormatContext *s)
{
    struct HashContext *c = s->priv_data;
    int res = av_hash_alloc(&c->hash, c->hash_name);
    if (res < 0)
        return res;
    avio_printf(s->pb, "#format: frame checksums\n");
    avio_printf(s->pb, "#version: %d\n", c->format_version);
    avio_printf(s->pb, "#hash: %s\n", av_hash_get_name(c->hash));
    framehash_print_extradata(s);
    ff_framehash_write_header(s);
    avio_printf(s->pb, "#stream#, dts,        pts, duration,     size, hash\n");
    return 0;
}

static int framehash_write_packet(struct AVFormatContext *s, AVPacket *pkt)
{
    struct HashContext *c = s->priv_data;
    char buf[AV_HASH_MAX_SIZE*2+128];
    int len;
    av_hash_init(c->hash);
    av_hash_update(c->hash, pkt->data, pkt->size);

    snprintf(buf, sizeof(buf) - (AV_HASH_MAX_SIZE * 2 + 1), "%d, %10"PRId64", %10"PRId64", %8"PRId64", %8d, ",
             pkt->stream_index, pkt->dts, pkt->pts, pkt->duration, pkt->size);
    len = strlen(buf);
    av_hash_final_hex(c->hash, buf + len, sizeof(buf) - len);
    avio_write(s->pb, buf, strlen(buf));

    if (c->format_version > 1 && pkt->side_data_elems) {
        int i, j;
        avio_printf(s->pb, ", S=%d", pkt->side_data_elems);
        for (i = 0; i < pkt->side_data_elems; i++) {
            av_hash_init(c->hash);
            if (HAVE_BIGENDIAN && pkt->side_data[i].type == AV_PKT_DATA_PALETTE) {
                for (j = 0; j < pkt->side_data[i].size; j += sizeof(uint32_t)) {
                    uint32_t data = AV_RL32(pkt->side_data[i].data + j);
                    av_hash_update(c->hash, (uint8_t *)&data, sizeof(uint32_t));
                }
            } else
                av_hash_update(c->hash, pkt->side_data[i].data, pkt->side_data[i].size);
            snprintf(buf, sizeof(buf) - (AV_HASH_MAX_SIZE * 2 + 1), ", %8d, ", pkt->side_data[i].size);
            len = strlen(buf);
            av_hash_final_hex(c->hash, buf + len, sizeof(buf) - len);
            avio_write(s->pb, buf, strlen(buf));
        }
    }

    avio_printf(s->pb, "\n");
    avio_flush(s->pb);
    return 0;
}

static int framehash_write_trailer(struct AVFormatContext *s)
{
    struct HashContext *c = s->priv_data;
    av_hash_freep(&c->hash);
    return 0;
}
#endif

#if CONFIG_FRAMEHASH_MUXER
static const AVClass framehash_class = {
    .class_name = "frame hash encoder class",
    .item_name  = av_default_item_name,
    .option     = hash_options,
    .version    = LIBAVUTIL_VERSION_INT,
};

AVOutputFormat ff_framehash_muxer = {
    .name              = "framehash",
    .long_name         = NULL_IF_CONFIG_SMALL("Per-frame hash testing"),
    .priv_data_size    = sizeof(struct HashContext),
    .audio_codec       = AV_CODEC_ID_PCM_S16LE,
    .video_codec       = AV_CODEC_ID_RAWVIDEO,
    .write_header      = framehash_write_header,
    .write_packet      = framehash_write_packet,
    .write_trailer     = framehash_write_trailer,
    .flags             = AVFMT_VARIABLE_FPS | AVFMT_TS_NONSTRICT |
                         AVFMT_TS_NEGATIVE,
    .priv_class        = &framehash_class,
};
#endif

#if CONFIG_FRAMEMD5_MUXER
static const AVClass framemd5_class = {
    .class_name = "frame hash encoder class",
    .item_name  = av_default_item_name,
    .option     = md5_options,
    .version    = LIBAVUTIL_VERSION_INT,
};

AVOutputFormat ff_framemd5_muxer = {
    .name              = "framemd5",
    .long_name         = NULL_IF_CONFIG_SMALL("Per-frame MD5 testing"),
    .priv_data_size    = sizeof(struct HashContext),
    .audio_codec       = AV_CODEC_ID_PCM_S16LE,
    .video_codec       = AV_CODEC_ID_RAWVIDEO,
    .write_header      = framehash_write_header,
    .write_packet      = framehash_write_packet,
    .write_trailer     = framehash_write_trailer,
    .flags             = AVFMT_VARIABLE_FPS | AVFMT_TS_NONSTRICT |
                         AVFMT_TS_NEGATIVE,
    .priv_class        = &framemd5_class,
};
#endif
