/*
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

#include "config.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/ioctl.h>

/* This was introduced in version 4.6. And may not exist all without an
 * optional package. So to prevent a hard dependency on needing the Linux
 * kernel headers to compile, make this optional. */
#if HAVE_LINUX_DMA_BUF_H
#include <linux/dma-buf.h>
#include <sys/ioctl.h>
#endif

#include <drm.h>
#include <libdrm/drm_fourcc.h>
#include <xf86drm.h>

#include "avassert.h"
#include "hwcontext.h"
#include "hwcontext_drm.h"
#include "hwcontext_internal.h"
#include "imgutils.h"
#if CONFIG_SAND
#include "libavutil/rpi_sand_fns.h"
#endif

static void drm_device_free(AVHWDeviceContext *hwdev)
{
    AVDRMDeviceContext *hwctx = hwdev->hwctx;

    close(hwctx->fd);
}

static int drm_device_create(AVHWDeviceContext *hwdev, const char *device,
                             AVDictionary *opts, int flags)
{
    AVDRMDeviceContext *hwctx = hwdev->hwctx;
    drmVersionPtr version;

    if (device == NULL) {
        hwctx->fd = -1;
        return 0;
    }

    hwctx->fd = open(device, O_RDWR);
    if (hwctx->fd < 0)
        return AVERROR(errno);

    version = drmGetVersion(hwctx->fd);
    if (!version) {
        av_log(hwdev, AV_LOG_ERROR, "Failed to get version information "
               "from %s: probably not a DRM device?\n", device);
        close(hwctx->fd);
        return AVERROR(EINVAL);
    }

    av_log(hwdev, AV_LOG_VERBOSE, "Opened DRM device %s: driver %s "
           "version %d.%d.%d.\n", device, version->name,
           version->version_major, version->version_minor,
           version->version_patchlevel);

    drmFreeVersion(version);

    hwdev->free = &drm_device_free;

    return 0;
}

static int drm_get_buffer(AVHWFramesContext *hwfc, AVFrame *frame)
{
    frame->buf[0] = av_buffer_pool_get(hwfc->pool);
    if (!frame->buf[0])
        return AVERROR(ENOMEM);

    frame->data[0] = (uint8_t*)frame->buf[0]->data;

    frame->format = AV_PIX_FMT_DRM_PRIME;
    frame->width  = hwfc->width;
    frame->height = hwfc->height;

    return 0;
}

typedef struct DRMMapping {
    // Address and length of each mmap()ed region.
    int nb_regions;
    int sync_flags;
    int object[AV_DRM_MAX_PLANES];
    void *address[AV_DRM_MAX_PLANES];
    size_t length[AV_DRM_MAX_PLANES];
} DRMMapping;

static void drm_unmap_frame(AVHWFramesContext *hwfc,
                            HWMapDescriptor *hwmap)
{
    DRMMapping *map = hwmap->priv;

    for (int i = 0; i < map->nb_regions; i++) {
#if HAVE_LINUX_DMA_BUF_H
        struct dma_buf_sync sync = { .flags = DMA_BUF_SYNC_END | map->sync_flags };
        ioctl(map->object[i], DMA_BUF_IOCTL_SYNC, &sync);
#endif
        munmap(map->address[i], map->length[i]);
    }

    av_free(map);
}

static int drm_map_frame(AVHWFramesContext *hwfc,
                         AVFrame *dst, const AVFrame *src, int flags)
{
    const AVDRMFrameDescriptor *desc = (AVDRMFrameDescriptor*)src->data[0];
#if HAVE_LINUX_DMA_BUF_H
    struct dma_buf_sync sync_start = { 0 };
#endif
    DRMMapping *map;
    int err, i, p, plane;
    int mmap_prot;
    void *addr;

    map = av_mallocz(sizeof(*map));
    if (!map)
        return AVERROR(ENOMEM);

    mmap_prot = 0;
    if (flags & AV_HWFRAME_MAP_READ)
        mmap_prot |= PROT_READ;
    if (flags & AV_HWFRAME_MAP_WRITE)
        mmap_prot |= PROT_WRITE;

    if (dst->format == AV_PIX_FMT_NONE)
        dst->format = hwfc->sw_format;
#if HAVE_LINUX_DMA_BUF_H
    if (flags & AV_HWFRAME_MAP_READ)
        map->sync_flags |= DMA_BUF_SYNC_READ;
    if (flags & AV_HWFRAME_MAP_WRITE)
        map->sync_flags |= DMA_BUF_SYNC_WRITE;
    sync_start.flags = DMA_BUF_SYNC_START | map->sync_flags;
#endif

    av_assert0(desc->nb_objects <= AV_DRM_MAX_PLANES);
    for (i = 0; i < desc->nb_objects; i++) {
        addr = mmap(NULL, desc->objects[i].size, mmap_prot, MAP_SHARED,
                    desc->objects[i].fd, 0);
        if (addr == MAP_FAILED) {
            err = AVERROR(errno);
            av_log(hwfc, AV_LOG_ERROR, "Failed to map DRM object %d to "
                   "memory: %d.\n", desc->objects[i].fd, errno);
            goto fail;
        }

        map->address[i] = addr;
        map->length[i]  = desc->objects[i].size;
        map->object[i] = desc->objects[i].fd;

#if HAVE_LINUX_DMA_BUF_H
        /* We're not checking for errors here because the kernel may not
         * support the ioctl, in which case its okay to carry on */
        ioctl(desc->objects[i].fd, DMA_BUF_IOCTL_SYNC, &sync_start);
#endif
    }
    map->nb_regions = i;

    plane = 0;
    for (i = 0; i < desc->nb_layers; i++) {
        const AVDRMLayerDescriptor *layer = &desc->layers[i];
        for (p = 0; p < layer->nb_planes; p++) {
            dst->data[plane] =
                (uint8_t*)map->address[layer->planes[p].object_index] +
                                       layer->planes[p].offset;
            dst->linesize[plane] =     layer->planes[p].pitch;
            ++plane;
        }
    }
    av_assert0(plane <= AV_DRM_MAX_PLANES);

    dst->width  = src->width;
    dst->height = src->height;
    dst->crop_top    = src->crop_top;
    dst->crop_bottom = src->crop_bottom;
    dst->crop_left   = src->crop_left;
    dst->crop_right  = src->crop_right;

#if CONFIG_SAND
    // Rework for sand frames
    if (av_rpi_is_sand_frame(dst)) {
        // As it stands the sand formats hold stride2 in linesize[3]
        // linesize[0] & [1] contain stride1 which is always 128 for everything we do
        // * Arguably this should be reworked s.t. stride2 is in linesize[0] & [1]
        dst->linesize[3] = fourcc_mod_broadcom_param(desc->objects[0].format_modifier);
        dst->linesize[0] = 128;
        dst->linesize[1] = 128;
        // *** Are we sure src->height is actually what we want ???
    }
#endif

    err = ff_hwframe_map_create(src->hw_frames_ctx, dst, src,
                                &drm_unmap_frame, map);
    if (err < 0)
        goto fail;

    return 0;

fail:
    for (i = 0; i < desc->nb_objects; i++) {
        if (map->address[i])
            munmap(map->address[i], map->length[i]);
    }
    av_free(map);
    return err;
}

static int drm_transfer_get_formats(AVHWFramesContext *ctx,
                                    enum AVHWFrameTransferDirection dir,
                                    enum AVPixelFormat **formats)
{
    enum AVPixelFormat *p;

    p = *formats = av_malloc_array(3, sizeof(*p));
    if (!p)
        return AVERROR(ENOMEM);

    // **** Offer native sand too ????
    *p++ =
#if CONFIG_SAND
        ctx->sw_format == AV_PIX_FMT_RPI4_8 || ctx->sw_format == AV_PIX_FMT_SAND128 ?
            AV_PIX_FMT_YUV420P :
        ctx->sw_format == AV_PIX_FMT_RPI4_10 ?
            AV_PIX_FMT_YUV420P10LE :
#endif
            ctx->sw_format;

#if CONFIG_SAND
    if (ctx->sw_format == AV_PIX_FMT_RPI4_10 ||
        ctx->sw_format == AV_PIX_FMT_RPI4_8 || ctx->sw_format == AV_PIX_FMT_SAND128)
        *p++ = AV_PIX_FMT_NV12;
#endif

    *p = AV_PIX_FMT_NONE;
    return 0;
}

static int drm_transfer_data_from(AVHWFramesContext *hwfc,
                                  AVFrame *dst, const AVFrame *src)
{
    AVFrame *map;
    int err;

    if (dst->width > hwfc->width || dst->height > hwfc->height)
        return AVERROR(EINVAL);

    map = av_frame_alloc();
    if (!map)
        return AVERROR(ENOMEM);

    // Map to default
    map->format = AV_PIX_FMT_NONE;
    err = drm_map_frame(hwfc, map, src, AV_HWFRAME_MAP_READ);
    if (err)
        goto fail;

#if 0
    av_log(hwfc, AV_LOG_INFO, "%s: src fmt=%d (%d), dst fmt=%d (%d) s=%dx%d l=%d/%d/%d/%d, d=%dx%d l=%d/%d/%d\n", __func__,
           hwfc->sw_format, AV_PIX_FMT_RPI4_8, dst->format, AV_PIX_FMT_YUV420P10LE,
           map->width, map->height,
           map->linesize[0],
           map->linesize[1],
           map->linesize[2],
           map->linesize[3],
           dst->width, dst->height,
           dst->linesize[0],
           dst->linesize[1],
           dst->linesize[2]);
#endif
#if CONFIG_SAND
    if (av_rpi_is_sand_frame(map)) {
        // Preserve crop - later ffmpeg code assumes that we have in that it
        // overwrites any crop that we create with the old values
        const unsigned int w = FFMIN(dst->width, map->width);
        const unsigned int h = FFMIN(dst->height, map->height);

        map->crop_top = 0;
        map->crop_bottom = 0;
        map->crop_left = 0;
        map->crop_right = 0;

        if (av_rpi_sand_to_planar_frame(dst, map) != 0)
        {
            av_log(hwfc, AV_LOG_ERROR, "%s: Incompatible output pixfmt for sand\n", __func__);
            err = AVERROR(EINVAL);
            goto fail;
        }

        dst->width = w;
        dst->height = h;
    }
    else
#endif
    {
        // Kludge mapped h/w s.t. frame_copy works
        map->width  = dst->width;
        map->height = dst->height;
        err = av_frame_copy(dst, map);
    }

    if (err)
    {
        av_log(hwfc, AV_LOG_ERROR, "%s: Copy fail\n", __func__);
        goto fail;
    }

    err = 0;
fail:
    av_frame_free(&map);
    return err;
}

static int drm_transfer_data_to(AVHWFramesContext *hwfc,
                                AVFrame *dst, const AVFrame *src)
{
    AVFrame *map;
    int err;

    if (src->width > hwfc->width || src->height > hwfc->height)
    {
        av_log(hwfc, AV_LOG_ERROR, "%s: H/w mismatch: %d/%d, %d/%d\n", __func__, dst->width, hwfc->width, dst->height, hwfc->height);
        return AVERROR(EINVAL);
    }

    map = av_frame_alloc();
    if (!map)
        return AVERROR(ENOMEM);
    map->format = src->format;

    err = drm_map_frame(hwfc, map, dst, AV_HWFRAME_MAP_WRITE |
                                        AV_HWFRAME_MAP_OVERWRITE);
    if (err)
        goto fail;

    map->width  = src->width;
    map->height = src->height;

    err = av_frame_copy(map, src);
    if (err)
        goto fail;

    err = 0;
fail:
    av_frame_free(&map);
    return err;
}

static int drm_map_from(AVHWFramesContext *hwfc, AVFrame *dst,
                        const AVFrame *src, int flags)
{
    int err;

    if (hwfc->sw_format != dst->format)
        return AVERROR(ENOSYS);

    err = drm_map_frame(hwfc, dst, src, flags);
    if (err)
        return err;

    err = av_frame_copy_props(dst, src);
    if (err)
        return err;

    return 0;
}

const HWContextType ff_hwcontext_type_drm = {
    .type                   = AV_HWDEVICE_TYPE_DRM,
    .name                   = "DRM",

    .device_hwctx_size      = sizeof(AVDRMDeviceContext),

    .device_create          = &drm_device_create,

    .frames_get_buffer      = &drm_get_buffer,

    .transfer_get_formats   = &drm_transfer_get_formats,
    .transfer_data_to       = &drm_transfer_data_to,
    .transfer_data_from     = &drm_transfer_data_from,
    .map_from               = &drm_map_from,

    .pix_fmts = (const enum AVPixelFormat[]) {
        AV_PIX_FMT_DRM_PRIME,
        AV_PIX_FMT_NONE
    },
};
