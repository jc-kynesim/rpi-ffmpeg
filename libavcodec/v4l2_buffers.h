/*
 * V4L2 buffer helper functions.
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

#ifndef AVCODEC_V4L2_BUFFERS_H
#define AVCODEC_V4L2_BUFFERS_H

#include <stdatomic.h>
#include <stddef.h>
#include <linux/videodev2.h>

#include "avcodec.h"
#include "libavutil/buffer.h"
#include "libavutil/frame.h"
#include "libavutil/hwcontext_drm.h"
#include "packet.h"

enum V4L2Buffer_status {
    V4L2BUF_AVAILABLE,
    V4L2BUF_IN_DRIVER,
    V4L2BUF_IN_USE,
    V4L2BUF_RET_USER,
};

/**
 * V4L2Buffer (wrapper for v4l2_buffer management)
 */
struct V4L2Context;
struct ff_weak_link_client;
struct dmabuf_h;

typedef struct V4L2Buffer {
    /* each buffer needs to have a reference to its context
     * The pointer is good enough for most operation but once the buffer has
     * been passed to the user the buffer may become orphaned so for free ops
     * the weak link must be used to ensure that the context is actually
     * there
     */
    struct V4L2Context *context;
    struct ff_weak_link_client *context_wl;

    /* DRM descriptor */
    AVDRMFrameDescriptor drm_frame;
    /* For DRM_PRIME encode - need to keep a ref to the source buffer till we
     * are done
     */
    AVBufferRef * ref_buf;

    /* keep track of the mmap address and mmap length */
    struct V4L2Plane_info {
        size_t bytesperline;
        size_t offset;
        void * mm_addr;
        size_t length;
    } plane_info[VIDEO_MAX_PLANES];

    int num_planes;

    /* the v4l2_buffer buf.m.planes pointer uses the planes[] mem */
    struct v4l2_buffer buf;
    struct v4l2_plane planes[VIDEO_MAX_PLANES];

    enum V4L2Buffer_status status;

    struct dmabuf_h * dmabuf[VIDEO_MAX_PLANES]; // If externally alloced dmabufs - stash other info here
} V4L2Buffer;

/**
 * Extracts the data from a V4L2Buffer to an AVFrame
 *
 * @param[in] frame The AVFRame to push the information to
 * @param[in] buf The V4L2Buffer to get the information from
 *
 * @returns 0 in case of success, AVERROR(EINVAL) if the number of planes is incorrect,
 * AVERROR(ENOMEM) if the AVBufferRef can't be created.
 */
int ff_v4l2_buffer_buf_to_avframe(AVFrame *frame, V4L2Buffer *buf);

/**
 * Extracts the data from a V4L2Buffer to an AVPacket
 *
 * @param[in] pkt The AVPacket to push the information to
 * @param[in] buf The V4L2Buffer to get the information from
 *
 * @returns 0 in case of success, AVERROR(EINVAL) if the number of planes is incorrect,
 * AVERROR(ENOMEM) if the AVBufferRef can't be created.
 *
 */
int ff_v4l2_buffer_buf_to_avpkt(AVPacket *pkt, V4L2Buffer *buf);

/**
 * Extracts the data from an AVPacket to a V4L2Buffer
 *
 * @param[in]  frame AVPacket to get the data from
 * @param[in]  avbuf V4L2Bfuffer to push the information to
 *
 * @returns 0 in case of success, a negative AVERROR code otherwise
 */
int ff_v4l2_buffer_avpkt_to_buf(const AVPacket *pkt, V4L2Buffer *out);

int ff_v4l2_buffer_avpkt_to_buf_ext(const AVPacket * const pkt, V4L2Buffer * const out,
                                    const void *extdata, size_t extlen,
                                    const int64_t timestamp);

/**
 * Extracts the data from an AVFrame to a V4L2Buffer
 *
 * @param[in]  frame AVFrame to get the data from
 * @param[in]  avbuf V4L2Bfuffer to push the information to
 *
 * @returns 0 in case of success, a negative AVERROR code otherwise
 */
int ff_v4l2_buffer_avframe_to_buf(const AVFrame *frame, V4L2Buffer *out, const int64_t track_ts);

/**
 * Initializes a V4L2Buffer
 *
 * @param[in]  avbuf V4L2Bfuffer to initialize
 * @param[in]  index v4l2 buffer id
 *
 * @returns 0 in case of success, a negative AVERROR code otherwise
 */
int ff_v4l2_buffer_initialize(AVBufferRef **avbuf, int index, struct V4L2Context *ctx, enum v4l2_memory mem);

/**
 * Enqueues a V4L2Buffer
 *
 * @param[in] avbuf V4L2Bfuffer to push to the driver
 *
 * @returns 0 in case of success, a negative AVERROR code otherwise
 */
int ff_v4l2_buffer_enqueue(V4L2Buffer* avbuf);

static inline void
ff_v4l2_buffer_set_avail(V4L2Buffer* const avbuf)
{
    avbuf->status = V4L2BUF_AVAILABLE;
    av_buffer_unref(&avbuf->ref_buf);
}


#endif // AVCODEC_V4L2_BUFFERS_H
