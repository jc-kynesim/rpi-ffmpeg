/*
 * Copyright (c) 2013 Jeff Moguillansky
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
 * XVideo output device
 *
 * TODO:
 * - add support to more formats
 */

#include "libavutil/opt.h"
#include "libavutil/avassert.h"
#include "libavutil/pixdesc.h"
#include "libavutil/imgutils.h"
#include "libavformat/internal.h"
#include "avdevice.h"

#include <stdatomic.h>
#include <unistd.h>

#pragma GCC diagnostic push
// Many many redundant decls in the header files
#pragma GCC diagnostic ignored "-Wredundant-decls"
#include <bcm_host.h>
#include <interface/mmal/mmal.h>
#include <interface/mmal/mmal_parameters_camera.h>
#include <interface/mmal/mmal_buffer.h>
#include <interface/mmal/mmal_port.h>
#include <interface/mmal/util/mmal_util.h>
#include <interface/mmal/util/mmal_default_components.h>
#include <interface/mmal/util/mmal_connection.h>
#include <interface/mmal/util/mmal_util_params.h>
#pragma GCC diagnostic pop
#include "libavutil/rpi_sand_fns.h"
#include "libavcodec/rpi_zc.h"

#define TRACE_ALL 0

#define DISPLAY_PORT_DEPTH 4

typedef struct rpi_display_env_s
{
    AVClass *class;

    MMAL_COMPONENT_T* display;
    MMAL_COMPONENT_T* isp;
    MMAL_PORT_T * port_in;  // Input port of either isp or display depending on pipe setup
    MMAL_CONNECTION_T * conn;

    MMAL_POOL_T *rpi_pool;
    volatile int rpi_display_count;

    MMAL_FOURCC_T req_fmt;
    MMAL_VIDEO_FORMAT_T req_vfmt;

    AVZcEnvPtr zc;

    int window_width, window_height;
    int window_x, window_y;
    int layer, fullscreen;
    int show_all;
} rpi_display_env_t;


static void display_cb_input(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) {
    mmal_buffer_header_release(buffer);
}

static void display_cb_control(MMAL_PORT_T *port,MMAL_BUFFER_HEADER_T *buffer) {
    mmal_buffer_header_release(buffer);
}


static MMAL_FOURCC_T mmfmt_from_avfmt(const enum AVPixelFormat fmt)
{
    switch (fmt) {
    case AV_PIX_FMT_SAND128:
    case AV_PIX_FMT_RPI4_8:
        return MMAL_ENCODING_YUVUV128;
    case AV_PIX_FMT_RPI4_10:
        return MMAL_ENCODING_YUV10_COL;
    case AV_PIX_FMT_SAND64_10:
        return MMAL_ENCODING_YUVUV64_10;
    case AV_PIX_FMT_SAND64_16:
        return MMAL_ENCODING_YUVUV64_16;
    case AV_PIX_FMT_YUV420P:
        return MMAL_ENCODING_I420;

    default:
        break;
    }
    return 0;
}


static void video_format_from_zc_frame(MMAL_ES_FORMAT_T* const es_fmt,
                                       const AVFrame * const frame, const AVRpiZcRefPtr fr_ref)
{
    MMAL_VIDEO_FORMAT_T *const vfmt = &es_fmt->es->video;
    const AVRpiZcFrameGeometry * geo = av_rpi_zc_geometry(fr_ref);
    if (av_rpi_is_sand_format(geo->format)) {
        // Sand formats are a bit "special"
        // stride1 implicit in format
        // width = stride2
        vfmt->width = geo->stripe_is_yc ?
            geo->height_y + geo->height_c : geo->height_y;
//        es->height = geo->video_height;  //*** When we get the FLAG this will change
        vfmt->height = geo->height_y;
        es_fmt->flags = MMAL_ES_FORMAT_FLAG_COL_FMTS_WIDTH_IS_COL_STRIDE;
    }
    else {
        vfmt->width = geo->stride_y / geo->bytes_per_pel;
        vfmt->height = geo->height_y;
        es_fmt->flags = 0;
    }

    es_fmt->type = MMAL_ES_TYPE_VIDEO;
    es_fmt->encoding = mmfmt_from_avfmt(geo->format);
    es_fmt->encoding_variant = 0;
    es_fmt->bitrate = 0;

    vfmt->crop.x = frame->crop_left;
    vfmt->crop.y = frame->crop_top;
    vfmt->crop.width = av_frame_cropped_width(frame);
    vfmt->crop.height = av_frame_cropped_height(frame);

    vfmt->frame_rate.den = 0;  // Don't think I know it here
    vfmt->frame_rate.num = 0;

    vfmt->par.den = frame->sample_aspect_ratio.den;
    vfmt->par.num = frame->sample_aspect_ratio.num;

    vfmt->color_space = 0;  // Unknown currently
}

static MMAL_BOOL_T buf_release_cb(MMAL_BUFFER_HEADER_T * buf, void *userdata)
{
    rpi_display_env_t * const de = userdata;
    if (buf->user_data != NULL) {
        av_rpi_zc_unref((AVRpiZcRefPtr)buf->user_data);
        buf->user_data = NULL;
    }
    atomic_fetch_add(&de->rpi_display_count, -1);
    return MMAL_FALSE;
}

static inline int avfmt_needs_isp(const enum AVPixelFormat avfmt)
{
    return avfmt == AV_PIX_FMT_SAND64_10;
}

static void isp_remove(AVFormatContext * const s, rpi_display_env_t * const de)
{
    if (de->isp != NULL)
    {
        if (de->isp->input[0]->is_enabled)
            mmal_port_disable(de->isp->input[0]);
        if (de->isp->control->is_enabled)
            mmal_port_disable(de->isp->control);
    }
    if (de->conn != NULL) {
        mmal_connection_destroy(de->conn);
        de->conn = NULL;
    }
    if (de->isp != NULL) {
        mmal_component_destroy(de->isp);
        de->isp = NULL;
    }
}

static void display_frame(AVFormatContext * const s, rpi_display_env_t * const de, const AVFrame* const fr)
{
    MMAL_BUFFER_HEADER_T* buf = NULL;
    AVRpiZcRefPtr fr_buf = NULL;

    if (de == NULL)
        return;

    if (atomic_load(&de->rpi_display_count) >= DISPLAY_PORT_DEPTH - 1) {
        av_log(s, AV_LOG_VERBOSE, "Frame dropped\n");
        return;
    }

    if ((fr_buf = av_rpi_zc_ref(s, de->zc, fr, fr->format, 1)) == NULL) {
        return;
    }

    buf = mmal_queue_get(de->rpi_pool->queue);
    if (!buf) {
        // Running too fast so drop the frame (unexpected)
        goto fail;
    }

    buf->cmd = 0;
    buf->offset = 0;
    buf->flags = 0;
    mmal_buffer_header_reset(buf);

    atomic_fetch_add(&de->rpi_display_count, 1);  // Deced on release
    mmal_buffer_header_pre_release_cb_set(buf, buf_release_cb, de);

    buf->user_data = fr_buf;
    buf->data = (uint8_t *)av_rpi_zc_vc_handle(fr_buf);  // Cast our handle to a pointer for mmal
    buf->offset = av_rpi_zc_offset(fr_buf);
    buf->length = av_rpi_zc_length(fr_buf);
    buf->alloc_size = av_rpi_zc_numbytes(fr_buf);

    while (de->show_all && atomic_load(&de->rpi_display_count) >= DISPLAY_PORT_DEPTH - 1) {
        usleep(5000);
    }

    {
        MMAL_ES_SPECIFIC_FORMAT_T new_ess = {.video = {0}};
        MMAL_ES_FORMAT_T new_es = {.es = &new_ess};
		MMAL_VIDEO_FORMAT_T * const new_vfmt = &new_ess.video;

        video_format_from_zc_frame(&new_es, fr, fr_buf);
        if (de->req_fmt != new_es.encoding ||
            de->req_vfmt.width       != new_vfmt->width ||
            de->req_vfmt.height      != new_vfmt->height ||
            de->req_vfmt.crop.x      != new_vfmt->crop.x ||
            de->req_vfmt.crop.y      != new_vfmt->crop.y ||
            de->req_vfmt.crop.width  != new_vfmt->crop.width ||
            de->req_vfmt.crop.height != new_vfmt->crop.height) {
            // Something has changed

            // If we have an ISP tear it down
            isp_remove(s, de);
            de->port_in = de->display->input[0];

            // If we still need an ISP create it now
            if (avfmt_needs_isp(fr->format))
            {
                if (mmal_component_create("vc.ril.isp", &de->isp) != MMAL_SUCCESS)
                {
                    av_log(s, AV_LOG_ERROR, "ISP creation failed\n");
                    goto fail;
                }
                de->port_in = de->isp->input[0];
            }

            mmal_format_copy(de->port_in->format, &new_es);

            if (mmal_port_format_commit(de->port_in)) {
                av_log(s, AV_LOG_ERROR, "Failed to commit input format\n");
                goto fail;
            }

            // If we have an ISP then we must want to use it
            if (de->isp != NULL) {
                MMAL_PORT_T * const port_out = de->isp->output[0];
                MMAL_VIDEO_FORMAT_T* vfmt_in = &de->port_in->format->es->video;
                MMAL_VIDEO_FORMAT_T* vfmt_out = &port_out->format->es->video;

                port_out->format->type = MMAL_ES_TYPE_VIDEO;
                port_out->format->encoding  = MMAL_ENCODING_YUVUV128;
                port_out->format->encoding_variant = 0;
                port_out->format->bitrate = 0;
                port_out->format->flags = 0;
                port_out->format->extradata = NULL;
                port_out->format->extradata_size = 0;

                vfmt_out->width       = (vfmt_in->crop.width + 31) & ~31;
                vfmt_out->height      = (vfmt_in->crop.height + 15) & ~15;
                vfmt_out->crop.x      = 0;
                vfmt_out->crop.y      = 0;
                vfmt_out->crop.width  = vfmt_in->crop.width;
                vfmt_out->crop.height = vfmt_in->crop.height;
                vfmt_out->frame_rate  = vfmt_in->frame_rate;
                vfmt_out->par         = vfmt_in->par;
                vfmt_out->color_space = vfmt_in->color_space;

                if (mmal_port_format_commit(port_out)) {
                    av_log(s, AV_LOG_ERROR, "Failed to commit output format\n");
                    goto fail;
                }

                if (mmal_connection_create(&de->conn, port_out, de->display->input[0], MMAL_CONNECTION_FLAG_TUNNELLING) != MMAL_SUCCESS) {
                    av_log(s, AV_LOG_ERROR, "Failed to create connection\n");
                    goto fail;
                }
                if (mmal_connection_enable(de->conn) != MMAL_SUCCESS) {
                    av_log(s, AV_LOG_ERROR, "Failed to enable connection\n");
                    goto fail;
                }
                mmal_port_enable(de->isp->control,display_cb_control);
                mmal_component_enable(de->isp);
            }

            // Number of slots in my port Q
            de->port_in->buffer_num = DISPLAY_PORT_DEPTH;
            // Size to keep it happy - isn't used for anything other than error checking
            de->port_in->buffer_size = buf->alloc_size;
            if (!de->port_in->is_enabled)
            {
                mmal_port_parameter_set_boolean(de->port_in, MMAL_PARAMETER_ZERO_COPY, MMAL_TRUE); // Does this mark that the buffer contains a vc_handle?  Would have expected a vc_image?
                if (mmal_port_enable(de->port_in, display_cb_input) != MMAL_SUCCESS) {
                    av_log(s, AV_LOG_ERROR, "Failed to enable input port\n");
                    goto fail;
                }
            }

            de->req_fmt  = new_es.encoding;
            de->req_vfmt = *new_vfmt;
        }
    }

    if (mmal_port_send_buffer(de->port_in, buf) != MMAL_SUCCESS)
    {
        av_log(s, AV_LOG_ERROR, "mmal_port_send_buffer failed: depth=%d\n", de->rpi_display_count);
        goto fail;
    }
    return;

fail:
    // If we have a buf then fr_buf is held by that
    if (buf != NULL)
        mmal_buffer_header_release(buf);
    else if (fr_buf != NULL)
        av_rpi_zc_unref(fr_buf);
}


static int xv_write_trailer(AVFormatContext *s)
{
    rpi_display_env_t * const de = s->priv_data;
#if TRACE_ALL
    av_log(s, AV_LOG_INFO, "%s\n", __func__);
#endif
    if (de->port_in != NULL && de->port_in->is_enabled) {
        mmal_port_disable(de->port_in);
    }

    // The above disable should kick out all buffers - check that
    if (atomic_load(&de->rpi_display_count) != 0) {
        av_log(s, AV_LOG_WARNING, "Exiting with display count non-zero:%d\n", atomic_load(&de->rpi_display_count));
    }

    isp_remove(s, de);
    if (de->rpi_pool != NULL) {
        mmal_pool_destroy(de->rpi_pool);
        de->rpi_pool = NULL;
    }
    if (de->display != NULL) {
        mmal_component_destroy(de->display);
        de->display = NULL;
    }

    return 0;
}

static int xv_write_header(AVFormatContext *s)
{
    rpi_display_env_t * const de = s->priv_data;
    const AVCodecParameters * const par = s->streams[0]->codecpar;
    const unsigned int w = de->window_width ? de->window_width : par->width;
    const unsigned int h = de->window_height ? de->window_height : par->height;
    const unsigned int x = de->window_x;
    const unsigned int y = de->window_y;
    const int layer = de->layer ? de->layer : 2;
    const MMAL_BOOL_T fullscreen = de->fullscreen;

#if TRACE_ALL
    av_log(s, AV_LOG_INFO, "%s: %dx%d\n", __func__, w, h);
#endif
    if (   s->nb_streams > 1
        || par->codec_type != AVMEDIA_TYPE_VIDEO
        || par->codec_id   != AV_CODEC_ID_WRAPPED_AVFRAME) {
        av_log(s, AV_LOG_ERROR, "Only supports one wrapped avframe stream\n");
        return AVERROR(EINVAL);
    }

    {
        MMAL_DISPLAYREGION_T region =
        {
            .hdr = {MMAL_PARAMETER_DISPLAYREGION, sizeof(region)},
            .set = MMAL_DISPLAY_SET_LAYER | MMAL_DISPLAY_SET_FULLSCREEN |
                MMAL_DISPLAY_SET_DEST_RECT | MMAL_DISPLAY_SET_ALPHA,
            .layer = layer,
            .fullscreen = fullscreen,
            .dest_rect = {x, y, w, h},
            .alpha = !fullscreen ? 0xff : 0xff | MMAL_DISPLAY_ALPHA_FLAGS_DISCARD_LOWER_LAYERS,
        };

        bcm_host_init();  // Needs to be done by someone...

        if (mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_RENDERER, &de->display) != MMAL_SUCCESS)
        {
            av_log(s, AV_LOG_ERROR, "Failed to create display component\n");
            goto fail;
        }
        de->port_in = de->display->input[0];

        mmal_port_parameter_set(de->display->input[0], &region.hdr);

        if (mmal_component_enable(de->display) != MMAL_SUCCESS)
        {
            av_log(s, AV_LOG_ERROR, "Failed to enable display component\n");
            goto fail;
        }
        if (mmal_port_enable(de->display->control,display_cb_control) != MMAL_SUCCESS)
        {
            av_log(s, AV_LOG_ERROR, "Failed to enable display control port\n");
            goto fail;
        }

        if ((de->rpi_pool = mmal_pool_create(DISPLAY_PORT_DEPTH, 0)) == NULL)
        {
            av_log(s, AV_LOG_ERROR, "Failed to create pool\n");
            goto fail;
        }
    }

    return 0;

fail:
    xv_write_trailer(s);
    return AVERROR_UNKNOWN;
}

static int xv_write_packet(AVFormatContext *s, AVPacket *pkt)
{
    AVFrame * const frame = (AVFrame *)pkt->data;
#if TRACE_ALL
    av_log(s, AV_LOG_INFO, "%s\n", __func__);
#endif
    display_frame(s, s->priv_data, frame);
    return 0;
}

static int xv_write_frame(AVFormatContext *s, int stream_index, AVFrame **ppframe,
                          unsigned flags)
{
#if TRACE_ALL
    av_log(s, AV_LOG_INFO, "%s: idx=%d, flags=%#x\n", __func__, stream_index, flags);
#endif

    /* xv_write_header() should have accepted only supported formats */
    if ((flags & AV_WRITE_UNCODED_FRAME_QUERY))
        return 0;
//    return write_picture(s, (*frame)->data, (*frame)->linesize);

    display_frame(s, s->priv_data, *ppframe);
    return 0;
}

static int xv_control_message(AVFormatContext *s, int type, void *data, size_t data_size)
{
#if TRACE_ALL
    av_log(s, AV_LOG_INFO, "%s: %d\n", __func__, type);
#endif
    switch(type) {
    case AV_APP_TO_DEV_WINDOW_REPAINT:
        return 0;
    default:
        break;
    }
    return AVERROR(ENOSYS);
}

// deinit is called if init fails so no need to clean up explicity here
static int rpi_vout_init(struct AVFormatContext * s)
{
    rpi_display_env_t * const de = s->priv_data;

    // Get a ZC context in case we need one - has little overhead if unused
    if ((de->zc = av_rpi_zc_int_env_alloc(s)) == NULL)
        return 1;

    return 0;
}

static void rpi_vout_deinit(struct AVFormatContext * s)
{
    rpi_display_env_t * const de = s->priv_data;

    av_rpi_zc_int_env_freep(&de->zc);
}


#define OFFSET(x) offsetof(rpi_display_env_t, x)
static const AVOption options[] = {
    { "show_all",     "show all frames",        OFFSET(show_all),     AV_OPT_TYPE_BOOL,   {.i64 = 0 }, 0, 1, AV_OPT_FLAG_ENCODING_PARAM },
    { "window_size",  "set window forced size", OFFSET(window_width), AV_OPT_TYPE_IMAGE_SIZE, {.str = NULL}, 0, 0, AV_OPT_FLAG_ENCODING_PARAM },
    { "window_x",     "set window x offset",    OFFSET(window_x),     AV_OPT_TYPE_INT,    {.i64 = 0 }, -INT_MAX, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM },
    { "window_y",     "set window y offset",    OFFSET(window_y),     AV_OPT_TYPE_INT,    {.i64 = 0 }, -INT_MAX, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM },
    { "display_layer","set display layer",      OFFSET(layer),        AV_OPT_TYPE_INT,    {.i64 = 0 }, -INT_MAX, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM },
    { "fullscreen",   "set fullscreen display", OFFSET(fullscreen),   AV_OPT_TYPE_BOOL,   {.i64 = 0 }, 0, 1, AV_OPT_FLAG_ENCODING_PARAM },
    { NULL }

};

static const AVClass xv_class = {
    .class_name = "rpi vid outdev",
    .item_name  = av_default_item_name,
    .option     = options,
    .version    = LIBAVUTIL_VERSION_INT,
    .category   = AV_CLASS_CATEGORY_DEVICE_VIDEO_OUTPUT,
};

AVOutputFormat ff_vout_rpi_muxer = {
    .name           = "vout_rpi",
    .long_name      = NULL_IF_CONFIG_SMALL("Rpi (mmal) video output device"),
    .priv_data_size = sizeof(rpi_display_env_t),
    .audio_codec    = AV_CODEC_ID_NONE,
    .video_codec    = AV_CODEC_ID_WRAPPED_AVFRAME,
    .write_header   = xv_write_header,
    .write_packet   = xv_write_packet,
    .write_uncoded_frame = xv_write_frame,
    .write_trailer  = xv_write_trailer,
    .control_message = xv_control_message,
    .flags          = AVFMT_NOFILE | AVFMT_VARIABLE_FPS | AVFMT_NOTIMESTAMPS,
    .priv_class     = &xv_class,
    .init           = rpi_vout_init,
    .deinit         = rpi_vout_deinit,
};
