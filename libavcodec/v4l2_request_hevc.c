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
#include "decode.h"
#include "hevcdec.h"
#include "hwconfig.h"
#include "internal.h"

#include "v4l2_request_hevc.h"

#include "libavutil/hwcontext_drm.h"

#include "v4l2_req_devscan.h"
#include "v4l2_req_dmabufs.h"
#include "v4l2_req_pollqueue.h"
#include "v4l2_req_media.h"
#include "v4l2_req_utils.h"

static size_t bit_buf_size(unsigned int w, unsigned int h, unsigned int bits_minus8)
{
    const size_t wxh = w * h;
    size_t bits_alloc;

    /* Annex A gives a min compression of 2 @ lvl 3.1
     * (wxh <= 983040) and min 4 thereafter but avoid
     * the odity of 983041 having a lower limit than
     * 983040.
     * Multiply by 3/2 for 4:2:0
     */
    bits_alloc = wxh < 983040 ? wxh * 3 / 4 :
        wxh < 983040 * 2 ? 983040 * 3 / 4 :
        wxh * 3 / 8;
    /* Allow for bit depth */
    bits_alloc += (bits_alloc * bits_minus8) / 8;
    /* Add a few bytes (16k) for overhead */
    bits_alloc += 0x4000;
    return bits_alloc;
}

static int v4l2_req_hevc_start_frame(AVCodecContext *avctx,
                                     av_unused const uint8_t *buffer,
                                     av_unused uint32_t size)
{
    const V4L2RequestContextHEVC * const ctx = avctx->internal->hwaccel_priv_data;
    return ctx->fns->start_frame(avctx, buffer, size);
}

static int v4l2_req_hevc_decode_slice(AVCodecContext *avctx, const uint8_t *buffer, uint32_t size)
{
    V4L2RequestContextHEVC * const ctx = avctx->internal->hwaccel_priv_data;
    return ctx->fns->decode_slice(avctx, buffer, size);
}

static int v4l2_req_hevc_end_frame(AVCodecContext *avctx)
{
    V4L2RequestContextHEVC *ctx = avctx->internal->hwaccel_priv_data;
    return ctx->fns->end_frame(avctx);
}

static void v4l2_req_hevc_abort_frame(AVCodecContext * const avctx)
{
    V4L2RequestContextHEVC * const ctx = avctx->internal->hwaccel_priv_data;
    ctx->fns->abort_frame(avctx);
}

static int v4l2_req_hevc_frame_params(AVCodecContext *avctx, AVBufferRef *hw_frames_ctx)
{
    V4L2RequestContextHEVC * const ctx = avctx->internal->hwaccel_priv_data;
    return ctx->fns->frame_params(avctx, hw_frames_ctx);
}

static int v4l2_req_hevc_alloc_frame(AVCodecContext * avctx, AVFrame *frame)
{
    V4L2RequestContextHEVC * const ctx = avctx->internal->hwaccel_priv_data;
    return ctx->fns->alloc_frame(avctx, frame);
}


static int v4l2_request_hevc_uninit(AVCodecContext *avctx)
{
    V4L2RequestContextHEVC * const ctx = avctx->internal->hwaccel_priv_data;

    av_log(avctx, AV_LOG_DEBUG, "<<< %s\n", __func__);

    decode_q_wait(&ctx->decode_q, NULL);  // Wait for all other threads to be out of decode

    mediabufs_ctl_unref(&ctx->mbufs);
    media_pool_delete(&ctx->mpool);
    pollqueue_unref(&ctx->pq);
    dmabufs_ctl_delete(&ctx->dbufs);
    devscan_delete(&ctx->devscan);

    decode_q_uninit(&ctx->decode_q);

//    if (avctx->hw_frames_ctx) {
//        AVHWFramesContext *hwfc = (AVHWFramesContext*)avctx->hw_frames_ctx->data;
//        av_buffer_pool_flush(hwfc->pool);
//    }
    return 0;
}

static int dst_fmt_accept_cb(void * v, const struct v4l2_fmtdesc *fmtdesc)
{
    AVCodecContext *const avctx = v;
    const HEVCContext *const h = avctx->priv_data;

    if (h->ps.sps->bit_depth == 8) {
        if (fmtdesc->pixelformat == V4L2_PIX_FMT_NV12_COL128 ||
            fmtdesc->pixelformat == V4L2_PIX_FMT_NV12) {
            return 1;
        }
    }
    else if (h->ps.sps->bit_depth == 10) {
        if (fmtdesc->pixelformat == V4L2_PIX_FMT_NV12_10_COL128) {
            return 1;
        }
    }
    return 0;
}

static int v4l2_request_hevc_init(AVCodecContext *avctx)
{
    const HEVCContext *h = avctx->priv_data;
    V4L2RequestContextHEVC * const ctx = avctx->internal->hwaccel_priv_data;
    const HEVCSPS * const sps = h->ps.sps;
    int ret;
    const struct decdev * decdev;
    const uint32_t src_pix_fmt = V2(ff_v4l2_req_hevc, 4).src_pix_fmt_v4l2;  // Assuming constant for all APIs but avoiding V4L2 includes
    size_t src_size;
    enum mediabufs_memory src_memtype;
    enum mediabufs_memory dst_memtype;

    av_log(avctx, AV_LOG_DEBUG, "<<< %s\n", __func__);

    // Give up immediately if this is something that we have no code to deal with
    if (h->ps.sps->chroma_format_idc != 1) {
        av_log(avctx, AV_LOG_WARNING, "chroma_format_idc(%d) != 1: Not implemented\n", h->ps.sps->chroma_format_idc);
        return AVERROR_PATCHWELCOME;
    }
    if (!(h->ps.sps->bit_depth == 10 || h->ps.sps->bit_depth == 8) ||
        h->ps.sps->bit_depth != h->ps.sps->bit_depth_chroma) {
        av_log(avctx, AV_LOG_WARNING, "Bit depth Y:%d C:%d: Not implemented\n", h->ps.sps->bit_depth, h->ps.sps->bit_depth_chroma);
        return AVERROR_PATCHWELCOME;
    }

    if ((ret = devscan_build(avctx, &ctx->devscan)) != 0) {
        av_log(avctx, AV_LOG_WARNING, "Failed to find any V4L2 devices\n");
        return (AVERROR(-ret));
    }
    ret = AVERROR(ENOMEM);  // Assume mem fail by default for these

    if ((decdev = devscan_find(ctx->devscan, src_pix_fmt)) == NULL)
    {
        av_log(avctx, AV_LOG_WARNING, "Failed to find a V4L2 device for H265\n");
        ret = AVERROR(ENODEV);
        goto fail0;
    }
    av_log(avctx, AV_LOG_DEBUG, "Trying V4L2 devices: %s,%s\n",
           decdev_media_path(decdev), decdev_video_path(decdev));

    if ((ctx->dbufs = dmabufs_ctl_new()) == NULL) {
        av_log(avctx, AV_LOG_DEBUG, "Unable to open dmabufs - try mmap buffers\n");
        src_memtype = MEDIABUFS_MEMORY_MMAP;
        dst_memtype = MEDIABUFS_MEMORY_MMAP;
    }
    else {
        av_log(avctx, AV_LOG_DEBUG, "Dmabufs opened - try dmabuf buffers\n");
        src_memtype = MEDIABUFS_MEMORY_DMABUF;
        dst_memtype = MEDIABUFS_MEMORY_DMABUF;
    }

    if ((ctx->pq = pollqueue_new()) == NULL) {
        av_log(avctx, AV_LOG_ERROR, "Unable to create pollqueue\n");
        goto fail1;
    }

    if ((ctx->mpool = media_pool_new(decdev_media_path(decdev), ctx->pq, 4)) == NULL) {
        av_log(avctx, AV_LOG_ERROR, "Unable to create media pool\n");
        goto fail2;
    }

    if ((ctx->mbufs = mediabufs_ctl_new(avctx, decdev_video_path(decdev), ctx->pq)) == NULL) {
        av_log(avctx, AV_LOG_ERROR, "Unable to create media controls\n");
        goto fail3;
    }

    // Ask for an initial bitbuf size of max size / 4
    // We will realloc if we need more
    // Must use sps->h/w as avctx contains cropped size
retry_src_memtype:
    src_size = bit_buf_size(sps->width, sps->height, sps->bit_depth - 8);
    if (src_memtype == MEDIABUFS_MEMORY_DMABUF && mediabufs_src_resizable(ctx->mbufs))
        src_size /= 4;
    // Kludge for conformance tests which break Annex A limits
    else if (src_size < 0x40000)
        src_size = 0x40000;

    if (mediabufs_src_fmt_set(ctx->mbufs, decdev_src_type(decdev), src_pix_fmt,
                              sps->width, sps->height, src_size)) {
        char tbuf1[5];
        av_log(avctx, AV_LOG_ERROR, "Failed to set source format: %s %dx%d\n", strfourcc(tbuf1, src_pix_fmt), sps->width, sps->height);
        goto fail4;
    }

    if (mediabufs_src_chk_memtype(ctx->mbufs, src_memtype)) {
        if (src_memtype == MEDIABUFS_MEMORY_DMABUF) {
            src_memtype = MEDIABUFS_MEMORY_MMAP;
            goto retry_src_memtype;
        }
        av_log(avctx, AV_LOG_ERROR, "Failed to get src memory type\n");
        goto fail4;
    }

    if (V2(ff_v4l2_req_hevc, 4).probe(avctx, ctx) == 0) {
        av_log(avctx, AV_LOG_DEBUG, "HEVC API version 4 probed successfully\n");
        ctx->fns = &V2(ff_v4l2_req_hevc, 4);
    }
#if CONFIG_V4L2_REQ_HEVC_VX
    else if (V2(ff_v4l2_req_hevc, 3).probe(avctx, ctx) == 0) {
        av_log(avctx, AV_LOG_DEBUG, "HEVC API version 3 probed successfully\n");
        ctx->fns = &V2(ff_v4l2_req_hevc, 3);
    }
    else if (V2(ff_v4l2_req_hevc, 2).probe(avctx, ctx) == 0) {
        av_log(avctx, AV_LOG_DEBUG, "HEVC API version 2 probed successfully\n");
        ctx->fns = &V2(ff_v4l2_req_hevc, 2);
    }
    else if (V2(ff_v4l2_req_hevc, 1).probe(avctx, ctx) == 0) {
        av_log(avctx, AV_LOG_DEBUG, "HEVC API version 1 probed successfully\n");
        ctx->fns = &V2(ff_v4l2_req_hevc, 1);
    }
#endif
    else {
        av_log(avctx, AV_LOG_ERROR, "No HEVC version probed successfully\n");
        ret = AVERROR(EINVAL);
        goto fail4;
    }

    if (mediabufs_dst_fmt_set(ctx->mbufs, sps->width, sps->height, dst_fmt_accept_cb, avctx)) {
        char tbuf1[5];
        av_log(avctx, AV_LOG_ERROR, "Failed to set destination format: %s %dx%d\n", strfourcc(tbuf1, src_pix_fmt), sps->width, sps->height);
        goto fail4;
    }

    if (mediabufs_src_pool_create(ctx->mbufs, ctx->dbufs, 6, src_memtype)) {
        av_log(avctx, AV_LOG_ERROR, "Failed to create source pool\n");
        goto fail4;
    }

    {
        unsigned int dst_slots = sps->temporal_layer[sps->max_sub_layers - 1].max_dec_pic_buffering +
            avctx->thread_count + (avctx->extra_hw_frames > 0 ? avctx->extra_hw_frames : 6);
        av_log(avctx, AV_LOG_DEBUG, "Slots=%d: Reordering=%d, threads=%d, hw+=%d\n", dst_slots,
               sps->temporal_layer[sps->max_sub_layers - 1].max_dec_pic_buffering,
               avctx->thread_count, avctx->extra_hw_frames);

        if (mediabufs_dst_chk_memtype(ctx->mbufs, dst_memtype)) {
            if (dst_memtype != MEDIABUFS_MEMORY_DMABUF) {
                av_log(avctx, AV_LOG_ERROR, "Failed to get dst memory type\n");
                goto fail4;
            }
            av_log(avctx, AV_LOG_DEBUG, "Dst DMABUF not supported - trying mmap\n");
            dst_memtype = MEDIABUFS_MEMORY_MMAP;
        }

        // extra_hw_frames is -1 if unset
        if (mediabufs_dst_slots_create(ctx->mbufs, dst_slots, (avctx->extra_hw_frames > 0), dst_memtype)) {
            av_log(avctx, AV_LOG_ERROR, "Failed to create destination slots\n");
            goto fail4;
        }
    }

    if (mediabufs_stream_on(ctx->mbufs)) {
        av_log(avctx, AV_LOG_ERROR, "Failed stream on\n");
        goto fail4;
    }

    if ((ret = ff_decode_get_hw_frames_ctx(avctx, AV_HWDEVICE_TYPE_DRM)) != 0) {
        av_log(avctx, AV_LOG_ERROR, "Failed to create frame ctx\n");
        goto fail4;
    }

    if ((ret = ctx->fns->set_controls(avctx, ctx)) != 0) {
        av_log(avctx, AV_LOG_ERROR, "Failed set controls\n");
        goto fail5;
    }

    decode_q_init(&ctx->decode_q);

    // Set our s/w format
    avctx->sw_pix_fmt = ((AVHWFramesContext *)avctx->hw_frames_ctx->data)->sw_format;

    av_log(avctx, AV_LOG_INFO, "Hwaccel %s; devices: %s,%s; buffers: src %s, dst %s\n",
           ctx->fns->name,
           decdev_media_path(decdev), decdev_video_path(decdev),
           mediabufs_memory_name(src_memtype), mediabufs_memory_name(dst_memtype));

    return 0;

fail5:
    av_buffer_unref(&avctx->hw_frames_ctx);
fail4:
    mediabufs_ctl_unref(&ctx->mbufs);
fail3:
    media_pool_delete(&ctx->mpool);
fail2:
    pollqueue_unref(&ctx->pq);
fail1:
    dmabufs_ctl_delete(&ctx->dbufs);
fail0:
    devscan_delete(&ctx->devscan);
    return ret;
}

const AVHWAccel ff_hevc_v4l2request_hwaccel = {
    .name           = "hevc_v4l2request",
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_HEVC,
    .pix_fmt        = AV_PIX_FMT_DRM_PRIME,
    .alloc_frame    = v4l2_req_hevc_alloc_frame,
    .start_frame    = v4l2_req_hevc_start_frame,
    .decode_slice   = v4l2_req_hevc_decode_slice,
    .end_frame      = v4l2_req_hevc_end_frame,
    .abort_frame    = v4l2_req_hevc_abort_frame,
    .init           = v4l2_request_hevc_init,
    .uninit         = v4l2_request_hevc_uninit,
    .priv_data_size = sizeof(V4L2RequestContextHEVC),
    .frame_params   = v4l2_req_hevc_frame_params,
    .caps_internal  = HWACCEL_CAP_ASYNC_SAFE | HWACCEL_CAP_MT_SAFE,
};
