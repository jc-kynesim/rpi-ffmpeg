/*
Copyright (c) 2018 Raspberry Pi (Trading) Ltd.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Authors: John Cox
*/

#ifndef AVUTIL_RPI_SAND_FNS_H
#define AVUTIL_RPI_SAND_FNS_H

#include "libavutil/frame.h"

// For all these fns _x & _w are measured as coord * PW
// For the C fns coords are in chroma pels (so luma / 2)
// Strides are in bytes

void av_rpi_sand_to_planar_y8(uint8_t * dst, const unsigned int dst_stride,
                             const uint8_t * src,
                             unsigned int stride1, unsigned int stride2,
                             unsigned int _x, unsigned int y,
                             unsigned int _w, unsigned int h);
void av_rpi_sand_to_planar_y16(uint8_t * dst, const unsigned int dst_stride,
                             const uint8_t * src,
                             unsigned int stride1, unsigned int stride2,
                             unsigned int _x, unsigned int y,
                             unsigned int _w, unsigned int h);

void av_rpi_sand_to_planar_c8(uint8_t * dst_u, const unsigned int dst_stride_u,
                             uint8_t * dst_v, const unsigned int dst_stride_v,
                             const uint8_t * src,
                             unsigned int stride1, unsigned int stride2,
                             unsigned int _x, unsigned int y,
                             unsigned int _w, unsigned int h);
void av_rpi_sand_to_planar_c16(uint8_t * dst_u, const unsigned int dst_stride_u,
                             uint8_t * dst_v, const unsigned int dst_stride_v,
                             const uint8_t * src,
                             unsigned int stride1, unsigned int stride2,
                             unsigned int _x, unsigned int y,
                             unsigned int _w, unsigned int h);

void av_rpi_planar_to_sand_c8(uint8_t * dst_c,
                             unsigned int stride1, unsigned int stride2,
                             const uint8_t * src_u, const unsigned int src_stride_u,
                             const uint8_t * src_v, const unsigned int src_stride_v,
                             unsigned int _x, unsigned int y,
                             unsigned int _w, unsigned int h);
void av_rpi_planar_to_sand_c16(uint8_t * dst_c,
                             unsigned int stride1, unsigned int stride2,
                             const uint8_t * src_u, const unsigned int src_stride_u,
                             const uint8_t * src_v, const unsigned int src_stride_v,
                             unsigned int _x, unsigned int y,
                             unsigned int _w, unsigned int h);

void av_rpi_sand30_to_planar_y16(uint8_t * dst, const unsigned int dst_stride,
                             const uint8_t * src,
                             unsigned int stride1, unsigned int stride2,
                             unsigned int _x, unsigned int y,
                             unsigned int _w, unsigned int h);
void av_rpi_sand30_to_planar_c16(uint8_t * dst_u, const unsigned int dst_stride_u,
                             uint8_t * dst_v, const unsigned int dst_stride_v,
                             const uint8_t * src,
                             unsigned int stride1, unsigned int stride2,
                             unsigned int _x, unsigned int y,
                             unsigned int _w, unsigned int h);

void av_rpi_sand30_to_planar_y8(uint8_t * dst, const unsigned int dst_stride,
                             const uint8_t * src,
                             unsigned int stride1, unsigned int stride2,
                             unsigned int _x, unsigned int y,
                             unsigned int _w, unsigned int h);

// w/h in pixels
void av_rpi_sand16_to_sand8(uint8_t * dst, const unsigned int dst_stride1, const unsigned int dst_stride2,
                         const uint8_t * src, const unsigned int src_stride1, const unsigned int src_stride2,
                         unsigned int w, unsigned int h, const unsigned int shr);


// dst must contain required pixel format & allocated data buffers
// Cropping on the src buffer will be honoured and dst crop will be set to zero
int av_rpi_sand_to_planar_frame(AVFrame * const dst, const AVFrame * const src);


static inline unsigned int av_rpi_sand_frame_stride1(const AVFrame * const frame)
{
#ifdef RPI_ZC_SAND128_ONLY
    // If we are sure we only only support 128 byte sand formats replace the
    // var with a constant which should allow for better optimisation
    return 128;
#else
    return frame->linesize[0];
#endif
}

static inline unsigned int av_rpi_sand_frame_stride2_y(const AVFrame * const frame)
{
    return frame->linesize[3];
}

static inline unsigned int av_rpi_sand_frame_stride2_c(const AVFrame * const frame)
{
    return frame->linesize[4];
}

static inline int av_rpi_is_sand_format(const int format)
{
    return (format >= AV_PIX_FMT_SAND128 && format <= AV_PIX_FMT_RPI4_10);
}

static inline int av_rpi_is_sand_frame(const AVFrame * const frame)
{
    return av_rpi_is_sand_format(frame->format);
}

static inline int av_rpi_is_sand8_frame(const AVFrame * const frame)
{
    return (frame->format == AV_PIX_FMT_SAND128 || frame->format == AV_PIX_FMT_RPI4_8);
}

static inline int av_rpi_is_sand16_frame(const AVFrame * const frame)
{
    return (frame->format >= AV_PIX_FMT_SAND64_10 && frame->format <= AV_PIX_FMT_SAND64_16);
}

static inline int av_rpi_is_sand30_frame(const AVFrame * const frame)
{
    return (frame->format == AV_PIX_FMT_RPI4_10);
}

static inline int av_rpi_sand_frame_xshl(const AVFrame * const frame)
{
    return av_rpi_is_sand8_frame(frame) ? 0 : 1;
}

#endif

