/*
 * DXVA2 HW acceleration.
 *
 * copyright (c) 2010 Laurent Aimar
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

#include <assert.h>
#include <string.h>

#include "libavutil/log.h"
#include "libavutil/time.h"

#include "avcodec.h"
#include "mpegvideo.h"
#include "dxva2_internal.h"

void *ff_dxva2_get_surface(const AVFrame *frame)
{
    return frame->data[3];
}

unsigned ff_dxva2_get_surface_index(const struct dxva_context *ctx,
                                    const AVFrame *frame)
{
    void *surface = ff_dxva2_get_surface(frame);
    unsigned i;

    for (i = 0; i < ctx->surface_count; i++)
        if (ctx->surface[i] == surface)
            return i;

    assert(0);
    return 0;
}

int ff_dxva2_commit_buffer(AVCodecContext *avctx,
                           struct dxva_context *ctx,
                           D3D11_VIDEO_DECODER_BUFFER_DESC *dsc,
                           D3D11_VIDEO_DECODER_BUFFER_TYPE type, 
                           const void *data, unsigned size,
                           unsigned mb_count)
{
    void     *dxva_data;
    unsigned dxva_size;
    int      result;
    HRESULT hr;

    hr = ID3D11VideoContext_GetDecoderBuffer(ctx->context, ctx->decoder, type,
                                             &dxva_size, &dxva_data);
    if (FAILED(hr)) {
        av_log(avctx, AV_LOG_ERROR, "Failed to get a buffer for %u: 0x%lx\n",
               type, hr);
        return -1;
    }
    if (size <= dxva_size) {
        memcpy(dxva_data, data, size);

        memset(dsc, 0, sizeof(*dsc));
        dsc->BufferType     = type;
        dsc->DataSize       = size;
        dsc->NumMBsInBuffer = mb_count;

        result = 0;
    } else {
        av_log(avctx, AV_LOG_ERROR, "Buffer for type %u was too small (dx:%u < reg:%u)\n", type, dxva_size, size);
        result = -1;
    }

    hr = ID3D11VideoContext_ReleaseDecoderBuffer(ctx->context, ctx->decoder, type);

    if (FAILED(hr)) {
        av_log(avctx, AV_LOG_ERROR,
               "Failed to release buffer type %u: 0x%lx\n",
               type, hr);
        result = -1;
    }
    return result;
}

int ff_dxva2_common_end_frame(AVCodecContext *avctx, AVFrame *frame,
                              const void *pp, unsigned pp_size,
                              const void *qm, unsigned qm_size,
                              int (*commit_bs_si)(AVCodecContext *,
                                                  D3D11_VIDEO_DECODER_BUFFER_DESC *bs,
                                                  D3D11_VIDEO_DECODER_BUFFER_DESC *slice))
{
    struct dxva_context *ctx = avctx->hwaccel_context;
    unsigned               buffer_count = 0;
    D3D11_VIDEO_DECODER_BUFFER_DESC buffer[4];
    int result, runs = 0;
    HRESULT hr;

    do {
        hr = ID3D11VideoContext_DecoderBeginFrame(ctx->context, ctx->decoder,
                                                  ff_dxva2_get_surface(frame),
                                                  0, NULL);
        if (hr == E_PENDING)
            av_usleep(2000);
    } while (hr == E_PENDING && ++runs < 50);

    if (FAILED(hr)) {
        av_log(avctx, AV_LOG_ERROR, "Failed to begin frame: 0x%lx\n", hr);
        return -1;
    }

    result = ff_dxva2_commit_buffer(avctx, ctx, &buffer[buffer_count],
                                    D3D11_VIDEO_DECODER_BUFFER_PICTURE_PARAMETERS,
                                    pp, pp_size, 0);
    if (result) {
        av_log(avctx, AV_LOG_ERROR,
               "Failed to add picture parameter buffer\n");
        goto end;
    }
    buffer_count++;

    if (qm_size > 0) {
        result = ff_dxva2_commit_buffer(avctx, ctx, &buffer[buffer_count],
                                        D3D11_VIDEO_DECODER_BUFFER_INVERSE_QUANTIZATION_MATRIX,
                                        qm, qm_size, 0);
        if (result) {
            av_log(avctx, AV_LOG_ERROR,
                   "Failed to add inverse quantization matrix buffer\n");
            goto end;
        }
        buffer_count++;
    }

    result = commit_bs_si(avctx,
                          &buffer[buffer_count + 0],
                          &buffer[buffer_count + 1]);
    if (result) {
        av_log(avctx, AV_LOG_ERROR,
               "Failed to add bitstream or slice control buffer\n");
        goto end;
    }
    buffer_count += 2;

    /* TODO Film Grain when possible */

    assert(buffer_count == 1 + (qm_size > 0) + 2);

    hr = ID3D11VideoContext_SubmitDecoderBuffers(ctx->context, ctx->decoder, buffer_count, buffer);
    if (FAILED(hr)) {
        av_log(avctx, AV_LOG_ERROR, "Failed to execute: 0x%lx\n", hr);
        result = -1;
    }

end:
    hr = ID3D11VideoContext_DecoderEndFrame(ctx->context, ctx->decoder);
    if (FAILED(hr)) {
        av_log(avctx, AV_LOG_ERROR, "Failed to end frame: 0x%lx\n", hr);
        result = -1;
    }

    return result;
}
