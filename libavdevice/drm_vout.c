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
//     Amongst other issues it doesn't wait for the pic to be displayed before
//     returning the buffer so flikering does occur.

#include "libavutil/opt.h"
#include "libavutil/avassert.h"
#include "libavutil/pixdesc.h"
#include "libavutil/imgutils.h"
#include "libavutil/hwcontext_drm.h"
#include "libavformat/internal.h"
#include "avdevice.h"

#include "pthread.h"
#include <semaphore.h>
#include <stdatomic.h>

#include "drm_fourcc.h"
#include <drm.h>
#include <drm_mode.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "libavutil/rpi_sand_fns.h"

#define TRACE_ALL 0

#define NUM_BUFFERS 4
#define RPI_DISPLAY_ALL 0

#define DRM_MODULE "vc4"

#define ERRSTR strerror(errno)

struct drm_setup {
   int conId;
   uint32_t crtcId;
   int crtcIdx;
   uint32_t planeId;
   unsigned int out_fourcc;
   struct {
       int x, y, width, height;
   } compose;
};

typedef struct drm_aux_s {
    int fd;
    uint32_t bo_handles[4];
    unsigned int fb_handle;
} drm_aux_t;

typedef struct drm_display_env_s
{
    AVClass *class;

    int drm_fd;
    uint32_t con_id;
    struct drm_setup setup;
    enum AVPixelFormat avfmt;

    drm_aux_t aux[32];

    pthread_t q_thread;
    pthread_mutex_t q_lock;
    sem_t q_sem;
    int q_terminate;
    AVFrame * q_this;
    AVFrame * q_next;

} drm_display_env_t;


static int drm_vout_write_trailer(AVFormatContext *s)
{
#if TRACE_ALL
    av_log(s, AV_LOG_INFO, "%s\n", __func__);
#endif

    return 0;
}

static int drm_vout_write_header(AVFormatContext *s)
{
    const AVCodecParameters * const par = s->streams[0]->codecpar;

#if TRACE_ALL
    av_log(s, AV_LOG_INFO, "%s\n", __func__);
#endif
    if (   s->nb_streams > 1
        || par->codec_type != AVMEDIA_TYPE_VIDEO
        || par->codec_id   != AV_CODEC_ID_WRAPPED_AVFRAME) {
        av_log(s, AV_LOG_ERROR, "Only supports one wrapped avframe stream\n");
        return AVERROR(EINVAL);
    }

    return 0;
}


static int do_display(AVFormatContext * const s, drm_display_env_t * const de, AVFrame * const frame)
{
    int ret = 0;

    const AVDRMFrameDescriptor *desc = (AVDRMFrameDescriptor*)frame->data[0];
    drm_aux_t * da = NULL;
    unsigned int i;

#if TRACE_ALL
    av_log(s, AV_LOG_INFO, "<<< %s\n", __func__);
#endif

    for (i = 0; i != 32; ++i) {
        if (de->aux[i].fd == -1 || de->aux[i].fd == desc->objects[0].fd) {
            da = de->aux + i;
            break;
        }
    }

    if (da == NULL) {
        av_log(s, AV_LOG_INFO, "%s: Out of handles\n", __func__);
        return AVERROR(EINVAL);
    }

    if (da->fd == -1) {
        uint32_t pitches[4] = {0};
        uint32_t offsets[4] = {0};
        uint64_t modifiers[4] = {0};
        uint32_t bo_plane_handles[4] = {0};
        int i, j, n;

        for (i = 0; i < desc->nb_objects; ++i) {
            if (drmPrimeFDToHandle(de->drm_fd, desc->objects[i].fd, da->bo_handles + i) != 0) {
                av_log(s, AV_LOG_WARNING, "drmPrimeFDToHandle failed: %s\n", ERRSTR);
                return -1;
            }
        }

        n = 0;
        for (i = 0; i < desc->nb_layers; ++i) {
            for (j = 0; j < desc->layers[i].nb_planes; ++j) {
                const AVDRMPlaneDescriptor * const p = desc->layers[i].planes + j;
                const AVDRMObjectDescriptor * const obj = desc->objects + p->object_index;
                pitches[n] = p->pitch;
                offsets[n] = p->offset;
                modifiers[n] = obj->format_modifier;
                bo_plane_handles[n] = da->bo_handles[p->object_index];
                ++n;
            }
        }

#if 0
        av_log(s, AV_LOG_INFO, "%dx%d, fmt: %x, boh=%d,%d,%d,%d, pitch=%d,%d,%d,%d,"
               " offset=%d,%d,%d,%d, mod=%llx,%llx,%llx,%llx\n",
               av_frame_cropped_width(frame),
               av_frame_cropped_height(frame),
               desc->layers[0].format,
               bo_plane_handles[0],
               bo_plane_handles[1],
               bo_plane_handles[2],
               bo_plane_handles[3],
               pitches[0],
               pitches[1],
               pitches[2],
               pitches[3],
               offsets[0],
               offsets[1],
               offsets[2],
               offsets[3],
               (long long)modifiers[0],
               (long long)modifiers[1],
               (long long)modifiers[2],
               (long long)modifiers[3]
               );
#endif

        if (drmModeAddFB2WithModifiers(de->drm_fd,
                                         av_frame_cropped_width(frame),
                                         av_frame_cropped_height(frame),
                                         desc->layers[0].format, bo_plane_handles,
                                         pitches, offsets, modifiers,
                                         &da->fb_handle, DRM_MODE_FB_MODIFIERS /** 0 if no mods */) != 0) {
            av_log(s, AV_LOG_WARNING, "drmModeAddFB2WithModifiers failed: %s\n", ERRSTR);
            return -1;
        }

        da->fd = desc->objects[0].fd;
    }

    ret = drmModeSetPlane(de->drm_fd, de->setup.planeId, de->setup.crtcId,
                              da->fb_handle, 0,
                de->setup.compose.x, de->setup.compose.y,
                de->setup.compose.width,
                de->setup.compose.height,
                0, 0,
                av_frame_cropped_width(frame) << 16,
                av_frame_cropped_height(frame) << 16);

    if (ret != 0) {
        av_log(s, AV_LOG_WARNING, "drmModeSetPlane failed: %s\n", ERRSTR);
    }

    return ret;
}

static void * display_thread(void * v)
{
    AVFormatContext * const s = v;
    drm_display_env_t * const de = s->priv_data;

#if TRACE_ALL
    av_log(s, AV_LOG_INFO, "<<< %s\n", __func__);
#endif

    for (;;) {
        AVFrame * frame;

        while (sem_wait(&de->q_sem) != 0) {
            av_assert0(errno == EINTR);
        }

        if (de->q_terminate)
            break;

        pthread_mutex_lock(&de->q_lock);
        frame = de->q_next;
        de->q_next = NULL;
        pthread_mutex_unlock(&de->q_lock);

        do_display(s, de, frame);

        av_frame_free(&de->q_this);
        de->q_this = frame;
    }

#if TRACE_ALL
    av_log(s, AV_LOG_INFO, ">>> %s\n", __func__);
#endif

    return NULL;
}

static int drm_vout_write_packet(AVFormatContext *s, AVPacket *pkt)
{
    const AVFrame * const src_frame = (AVFrame *)pkt->data;
    AVFrame * frame;
    drm_display_env_t * const de = s->priv_data;

#if TRACE_ALL
    av_log(s, AV_LOG_INFO, "%s\n", __func__);
#endif

    if (src_frame->format == AV_PIX_FMT_DRM_PRIME) {
        frame = av_frame_alloc();
        av_frame_ref(frame, src_frame);
    }
    else if (src_frame->format == AV_PIX_FMT_VAAPI) {
        frame = av_frame_alloc();
        frame->format = AV_PIX_FMT_DRM_PRIME;
        if (av_hwframe_map(frame, src_frame, 0) != 0)
        {
            av_log(s, AV_LOG_WARNING, "Failed to map frame (format=%d) to DRM_PRiME\n", src_frame->format);
            av_frame_free(&frame);
            return AVERROR(EINVAL);
        }
    }
    else {
        av_log(s, AV_LOG_WARNING, "Frame (format=%d) not DRM_PRiME\n", src_frame->format);
        return AVERROR(EINVAL);
    }


    pthread_mutex_lock(&de->q_lock);
    {
        AVFrame * const t = de->q_next;
        de->q_next = frame;
        frame = t;
    }
    pthread_mutex_unlock(&de->q_lock);

    if (frame == NULL)
        sem_post(&de->q_sem);
    else
        av_frame_free(&frame);

    return 0;
}

static int drm_vout_write_frame(AVFormatContext *s, int stream_index, AVFrame **ppframe,
                          unsigned flags)
{
#if TRACE_ALL
    av_log(s, AV_LOG_INFO, "%s: idx=%d, flags=%#x\n", __func__, stream_index, flags);
#endif

    /* drm_vout_write_header() should have accepted only supported formats */
    if ((flags & AV_WRITE_UNCODED_FRAME_QUERY))
        return 0;

    return 0;
}

static int drm_vout_control_message(AVFormatContext *s, int type, void *data, size_t data_size)
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

static int find_crtc(struct AVFormatContext * const avctx, int drmfd, struct drm_setup *s, uint32_t * const pConId)
{
   int ret = -1;
   int i;
   drmModeRes *res = drmModeGetResources(drmfd);
   drmModeConnector *c;

   if(!res)
   {
      printf( "drmModeGetResources failed: %s\n", ERRSTR);
      return -1;
   }

   if (res->count_crtcs <= 0)
   {
      printf( "drm: no crts\n");
      goto fail_res;
   }

   if (!s->conId) {
      fprintf(stderr,
         "No connector ID specified.  Choosing default from list:\n");

      for (i = 0; i < res->count_connectors; i++) {
         drmModeConnector *con =
            drmModeGetConnector(drmfd, res->connectors[i]);
         drmModeEncoder *enc = NULL;
         drmModeCrtc *crtc = NULL;

         if (con->encoder_id) {
            enc = drmModeGetEncoder(drmfd, con->encoder_id);
            if (enc->crtc_id) {
               crtc = drmModeGetCrtc(drmfd, enc->crtc_id);
            }
         }

         if (!s->conId && crtc) {
            s->conId = con->connector_id;
            s->crtcId = crtc->crtc_id;
         }

         av_log(avctx, AV_LOG_INFO, "Connector %d (crtc %d): type %d, %dx%d%s\n",
                con->connector_id,
                crtc ? crtc->crtc_id : 0,
                con->connector_type,
                crtc ? crtc->width : 0,
                crtc ? crtc->height : 0,
                (s->conId == (int)con->connector_id ?
            " (chosen)" : ""));
      }

      if (!s->conId) {
         av_log(avctx, AV_LOG_ERROR,
            "No suitable enabled connector found.\n");
         return -1;;
      }
   }

   s->crtcIdx = -1;

   for (i = 0; i < res->count_crtcs; ++i) {
      if (s->crtcId == res->crtcs[i]) {
         s->crtcIdx = i;
         break;
      }
   }

   if (s->crtcIdx == -1)
   {
       av_log(avctx, AV_LOG_WARNING, "drm: CRTC %u not found\n", s->crtcId);
       goto fail_res;
   }

   if (res->count_connectors <= 0)
   {
       av_log(avctx, AV_LOG_WARNING, "drm: no connectors\n");
       goto fail_res;
   }

   c = drmModeGetConnector(drmfd, s->conId);
   if (!c)
   {
       av_log(avctx, AV_LOG_WARNING, "drmModeGetConnector failed: %s\n", ERRSTR);
       goto fail_res;
   }

   if (!c->count_modes)
   {
       av_log(avctx, AV_LOG_WARNING, "connector supports no mode\n");
       goto fail_conn;
   }

   {
      drmModeCrtc *crtc = drmModeGetCrtc(drmfd, s->crtcId);
      s->compose.x = crtc->x;
      s->compose.y = crtc->y;
      s->compose.width = crtc->width;
      s->compose.height = crtc->height;
      drmModeFreeCrtc(crtc);
   }

   if (pConId)
      *pConId = c->connector_id;
   ret = 0;

fail_conn:
   drmModeFreeConnector(c);

fail_res:
   drmModeFreeResources(res);

   return ret;
}

static int find_plane(struct AVFormatContext * const avctx, int drmfd, struct drm_setup *s)
{
   drmModePlaneResPtr planes;
   drmModePlanePtr plane;
   unsigned int i;
   unsigned int j;
   int ret = 0;

   planes = drmModeGetPlaneResources(drmfd);
   if (!planes)
   {
       av_log(avctx, AV_LOG_WARNING, "drmModeGetPlaneResources failed: %s\n", ERRSTR);
       return -1;
   }

   for (i = 0; i < planes->count_planes; ++i) {
      plane = drmModeGetPlane(drmfd, planes->planes[i]);
      if (!planes)
      {
          av_log(avctx, AV_LOG_WARNING, "drmModeGetPlane failed: %s\n", ERRSTR);
          break;
      }

      if (!(plane->possible_crtcs & (1 << s->crtcIdx))) {
         drmModeFreePlane(plane);
         continue;
      }

      for (j = 0; j < plane->count_formats; ++j) {
         if (plane->formats[j] == s->out_fourcc)
            break;
      }

      if (j == plane->count_formats) {
         drmModeFreePlane(plane);
         continue;
      }

      s->planeId = plane->plane_id;
      drmModeFreePlane(plane);
      break;
   }

   if (i == planes->count_planes)
      ret = -1;

   drmModeFreePlaneResources(planes);
   return ret;
}

// deinit is called if init fails so no need to clean up explicity here
static int drm_vout_init(struct AVFormatContext * s)
{
    drm_display_env_t * const de = s->priv_data;
    unsigned int i;

    av_log(s, AV_LOG_INFO, "<<< %s\n", __func__);

    de->drm_fd = -1;
    de->con_id = 0;
    de->setup = (struct drm_setup){0};

    de->setup.out_fourcc = DRM_FORMAT_NV12; // **** Need some sort of select

    for (i = 0; i != 32; ++i) {
        de->aux[i].fd = -1;
    }

    if ((de->drm_fd = drmOpen(DRM_MODULE, NULL)) < 0)
    {
        av_log(s, AV_LOG_ERROR, "Failed to drmOpen %s\n", DRM_MODULE);
        return -1;
    }

    if (find_crtc(s, de->drm_fd, &de->setup, &de->con_id) != 0)
    {
        av_log(s, AV_LOG_ERROR, "failed to find valid mode\n");
        return -1;
    }

    if (find_plane(s, de->drm_fd, &de->setup) != 0)
    {
        av_log(s, AV_LOG_ERROR, "failed to find compatible plane\n");
        return -1;
    }

    de->q_terminate = 0;
    pthread_mutex_init(&de->q_lock, NULL);
    sem_init(&de->q_sem, 0, 0);
    av_assert0(pthread_create(&de->q_thread, NULL, display_thread, s) == 0);

    av_log(s, AV_LOG_INFO, ">>> %s\n", __func__);

    return 0;
}

static void drm_vout_deinit(struct AVFormatContext * s)
{
    drm_display_env_t * const de = s->priv_data;

    av_log(s, AV_LOG_INFO, "<<< %s\n", __func__);

    de->q_terminate = 1;
    sem_post(&de->q_sem);
    pthread_join(de->q_thread, NULL);
    sem_destroy(&de->q_sem);
    pthread_mutex_destroy(&de->q_lock);

    av_frame_free(&de->q_next);
    av_frame_free(&de->q_this);

    if (de->drm_fd >= 0) {
        close(de->drm_fd);
        de->drm_fd = -1;
    }

    av_log(s, AV_LOG_INFO, ">>> %s\n", __func__);
}


#define OFFSET(x) offsetof(drm_display_env_t, x)
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

static const AVClass drm_vout_class = {
    .class_name = "drm vid outdev",
    .item_name  = av_default_item_name,
    .option     = options,
    .version    = LIBAVUTIL_VERSION_INT,
    .category   = AV_CLASS_CATEGORY_DEVICE_VIDEO_OUTPUT,
};

AVOutputFormat ff_vout_drm_muxer = {
    .name           = "vout_drm",
    .long_name      = NULL_IF_CONFIG_SMALL("Drm video output device"),
    .priv_data_size = sizeof(drm_display_env_t),
    .audio_codec    = AV_CODEC_ID_NONE,
    .video_codec    = AV_CODEC_ID_WRAPPED_AVFRAME,
    .write_header   = drm_vout_write_header,
    .write_packet   = drm_vout_write_packet,
    .write_uncoded_frame = drm_vout_write_frame,
    .write_trailer  = drm_vout_write_trailer,
    .control_message = drm_vout_control_message,
    .flags          = AVFMT_NOFILE | AVFMT_VARIABLE_FPS | AVFMT_NOTIMESTAMPS,
    .priv_class     = &drm_vout_class,
    .init           = drm_vout_init,
    .deinit         = drm_vout_deinit,
};
