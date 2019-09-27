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

#define NUM_BUFFERS 4
#define RPI_DISPLAY_ALL 0

typedef struct rpi_display_env_s
{
    AVClass *class;

    MMAL_COMPONENT_T* display;
    MMAL_COMPONENT_T* isp;
    MMAL_PORT_T * port_in;  // Input port of either isp or display depending on pipe setup
    MMAL_CONNECTION_T * conn;

    MMAL_POOL_T *rpi_pool;
    volatile int rpi_display_count;
    enum AVPixelFormat avfmt;

    AVZcEnvPtr zc;
} rpi_display_env_t;


static MMAL_POOL_T* display_alloc_pool(MMAL_PORT_T* port)
{
    MMAL_POOL_T* pool;
    mmal_port_parameter_set_boolean(port, MMAL_PARAMETER_ZERO_COPY, MMAL_TRUE); // Does this mark that the buffer contains a vc_handle?  Would have expected a vc_image?
    pool = mmal_port_pool_create(port, NUM_BUFFERS, 0);
    assert(pool);

    return pool;
}

static void display_cb_input(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) {
    rpi_display_env_t *const de = (rpi_display_env_t *)port->userdata;
    av_rpi_zc_unref(buffer->user_data);
    atomic_fetch_add(&de->rpi_display_count, -1);
    mmal_buffer_header_release(buffer);
}

static void display_cb_control(MMAL_PORT_T *port,MMAL_BUFFER_HEADER_T *buffer) {
    mmal_buffer_header_release(buffer);
}

#define DISPLAY_PORT_DEPTH 4

static void display_frame(AVFormatContext * const s, rpi_display_env_t * const de, const AVFrame* const fr)
{
    MMAL_BUFFER_HEADER_T* buf;

    if (de == NULL)
        return;

    if (atomic_load(&de->rpi_display_count) >= DISPLAY_PORT_DEPTH - 1) {
        av_log(s, AV_LOG_VERBOSE, "Frame dropped\n");
        return;
    }

    buf = mmal_queue_get(de->rpi_pool->queue);
    if (!buf) {
        // Running too fast so drop the frame
        printf("Q alloc failure\n");
        return;
    }
    assert(buf);
    buf->cmd = 0;
    buf->offset = 0; // Offset to valid data
    buf->flags = 0;
    {
        const AVRpiZcRefPtr fr_buf = av_rpi_zc_ref(s, de->zc, fr, de->avfmt, 1);
        if (fr_buf == NULL) {
            mmal_buffer_header_release(buf);
            return;
        }

        buf->user_data = fr_buf;
        buf->data = (uint8_t *)av_rpi_zc_vc_handle(fr_buf);  // Cast our handle to a pointer for mmal
        buf->offset = av_rpi_zc_offset(fr_buf);
        buf->length = av_rpi_zc_length(fr_buf);
        buf->alloc_size = av_rpi_zc_numbytes(fr_buf);
        atomic_fetch_add(&de->rpi_display_count, 1);
    }
#if RPI_DISPLAY_ALL
    while (atomic_load(&de->rpi_display_count) >= DISPLAY_PORT_DEPTH - 1) {
        usleep(5000);
    }
#endif

    if (mmal_port_send_buffer(de->port_in, buf) != MMAL_SUCCESS)
    {
        av_log(s, AV_LOG_ERROR, "mmal_port_send_buffer failed: depth=%d\n", de->rpi_display_count);
        display_cb_input(de->port_in, buf);
    }
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

    if (de->conn != NULL) {
        mmal_connection_destroy(de->conn);
        de->conn = NULL;
    }
    if (de->rpi_pool != NULL) {
        mmal_port_pool_destroy(de->display->input[0], de->rpi_pool);
        de->rpi_pool = NULL;
    }
    if (de->isp != NULL) {
        mmal_component_destroy(de->isp);
        de->isp = NULL;
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
    const unsigned int w = par->width;
    const unsigned int h = par->height;
    const unsigned int x = 0;
    const unsigned int y = 0;
    const enum AVPixelFormat req_fmt = par->format;

#if TRACE_ALL
    av_log(s, AV_LOG_INFO, "%s\n", __func__);
#endif
    if (   s->nb_streams > 1
        || par->codec_type != AVMEDIA_TYPE_VIDEO
        || par->codec_id   != AV_CODEC_ID_WRAPPED_AVFRAME) {
        av_log(s, AV_LOG_ERROR, "Only supports one wrapped avframe stream\n");
        return AVERROR(EINVAL);
    }

    {
        MMAL_STATUS_T err;
        MMAL_DISPLAYREGION_T region =
        {
            .hdr = {MMAL_PARAMETER_DISPLAYREGION, sizeof(region)},
            .set = MMAL_DISPLAY_SET_LAYER | MMAL_DISPLAY_SET_FULLSCREEN | MMAL_DISPLAY_SET_DEST_RECT,
            .layer = 2,
            .fullscreen = 0,
            .dest_rect = {x, y, w, h}
        };
#if RPI_ZC_SAND_8_IN_10_BUF
        const enum AVPixelFormat fmt = (req_fmt == AV_PIX_FMT_YUV420P10 || av_rpi_is_sand_format(req_fmt)) ? AV_PIX_FMT_SAND128 : req_fmt;
#else
        const enum AVPixelFormat fmt = (req_fmt == AV_PIX_FMT_YUV420P10) ? AV_PIX_FMT_SAND128 : req_fmt;
#endif
        const AVRpiZcFrameGeometry geo = av_rpi_zc_frame_geometry(fmt, w, h);
        int isp_req = (fmt == AV_PIX_FMT_SAND64_10);

        bcm_host_init();  // Needs to be done by someone...

        mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_RENDERER, &de->display);
        av_assert0(de->display);
        de->port_in = de->display->input[0];

        if (isp_req)
        {
            mmal_component_create("vc.ril.isp", &de->isp);
            de->port_in = de->isp->input[0];
        }

        mmal_port_parameter_set(de->display->input[0], &region.hdr);

        {
            MMAL_PORT_T * const port = de->port_in;
            MMAL_ES_FORMAT_T* const format = port->format;
            port->userdata = (struct MMAL_PORT_USERDATA_T *)de;
            port->buffer_num = DISPLAY_PORT_DEPTH;
            format->encoding =
                fmt == AV_PIX_FMT_SAND128 ? MMAL_ENCODING_YUVUV128 :
                fmt == AV_PIX_FMT_RPI4_8  ? MMAL_ENCODING_YUVUV128 :
                fmt == AV_PIX_FMT_RPI4_10 ? MMAL_ENCODING_YUV10_COL :
                fmt == AV_PIX_FMT_SAND64_10 ? MMAL_ENCODING_YUVUV64_16 :
                    MMAL_ENCODING_I420;
            format->es->video.width = geo.stride_y;
            format->es->video.height = (fmt == AV_PIX_FMT_SAND128 ||
                                        fmt == AV_PIX_FMT_RPI4_8 ||
                                        fmt == AV_PIX_FMT_RPI4_10 ||
                                        fmt == AV_PIX_FMT_SAND64_10) ?
                                          (h + 15) & ~15 : geo.height_y;  // Magic
            format->es->video.crop.x = 0;
            format->es->video.crop.y = 0;
            format->es->video.crop.width = w;
            format->es->video.crop.height = h;
            mmal_port_format_commit(port);
        }

        de->rpi_pool = display_alloc_pool(de->port_in);
        mmal_port_enable(de->port_in,display_cb_input);

        if (isp_req) {
            MMAL_PORT_T * const port_out = de->isp->output[0];
            mmal_log_dump_port(de->port_in);
            mmal_format_copy(port_out->format, de->port_in->format);
            if (fmt == AV_PIX_FMT_SAND64_10) {
                if ((err = mmal_port_parameter_set_int32(de->port_in, MMAL_PARAMETER_CCM_SHIFT, 5)) != MMAL_SUCCESS ||
                    (err = mmal_port_parameter_set_int32(port_out, MMAL_PARAMETER_OUTPUT_SHIFT, 1)) != MMAL_SUCCESS)
                {
                    av_log(NULL, AV_LOG_WARNING, "Failed to set ISP output port shift\n");
                }
                else
                    av_log(NULL, AV_LOG_WARNING, "Set ISP output port shift OK\n");

            }
            port_out->format->encoding = MMAL_ENCODING_I420;
            mmal_log_dump_port(port_out);
            if ((err = mmal_port_format_commit(port_out)) != MMAL_SUCCESS)
            {
                av_log(NULL, AV_LOG_ERROR, "Failed to set ISP output port format\n");
                goto fail;
            }
            if ((err = mmal_connection_create(&de->conn, port_out, de->display->input[0], MMAL_CONNECTION_FLAG_TUNNELLING)) != MMAL_SUCCESS) {
                av_log(NULL, AV_LOG_ERROR, "Failed to create connection\n");
                goto fail;
            }
            if ((err = mmal_connection_enable(de->conn)) != MMAL_SUCCESS) {
                av_log(NULL, AV_LOG_ERROR, "Failed to enable connection\n");
                goto fail;
            }
            mmal_port_enable(de->isp->control,display_cb_control);
            mmal_component_enable(de->isp);
        }

        mmal_component_enable(de->display);
        mmal_port_enable(de->display->control,display_cb_control);
        de->avfmt = fmt;
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
#if 0
    { "display_name", "set display name",       OFFSET(display_name), AV_OPT_TYPE_STRING, {.str = NULL }, 0, 0, AV_OPT_FLAG_ENCODING_PARAM },
    { "window_id",    "set existing window id", OFFSET(window_id),    AV_OPT_TYPE_INT64,  {.i64 = 0 }, 0, INT64_MAX, AV_OPT_FLAG_ENCODING_PARAM },
    { "window_size",  "set window forced size", OFFSET(window_width), AV_OPT_TYPE_IMAGE_SIZE, {.str = NULL}, 0, 0, AV_OPT_FLAG_ENCODING_PARAM },
    { "window_title", "set window title",       OFFSET(window_title), AV_OPT_TYPE_STRING, {.str = NULL }, 0, 0, AV_OPT_FLAG_ENCODING_PARAM },
    { "window_x",     "set window x offset",    OFFSET(window_x),     AV_OPT_TYPE_INT,    {.i64 = 0 }, -INT_MAX, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM },
    { "window_y",     "set window y offset",    OFFSET(window_y),     AV_OPT_TYPE_INT,    {.i64 = 0 }, -INT_MAX, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM },
#endif
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
