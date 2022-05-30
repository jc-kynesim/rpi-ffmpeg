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

#include "config.h"
#include <stdint.h>
#include <string.h>
#include "rpi_sand_fns.h"
#include "avassert.h"
#include "frame.h"

#if ARCH_ARM && HAVE_NEON
#include "arm/rpi_sand_neon.h"
#define HAVE_SAND_ASM 1
#elif ARCH_AARCH64 && HAVE_NEON
#include "aarch64/rpi_sand_neon.h"
#define HAVE_SAND_ASM 1
#else
#define HAVE_SAND_ASM 0
#endif

#define PW 1
#include "rpi_sand_fn_pw.h"
#undef PW

#define PW 2
#include "rpi_sand_fn_pw.h"
#undef PW

#if 1
// Simple round
static void cpy16_to_8(uint8_t * dst, const uint8_t * _src, unsigned int n, const unsigned int shr)
{
    const unsigned int rnd = (1 << shr) >> 1;
    const uint16_t * src = (const uint16_t *)_src;

    for (; n != 0; --n) {
        *dst++ = (*src++ + rnd) >> shr;
    }
}
#else
// Dithered variation
static void cpy16_to_8(uint8_t * dst, const uint8_t * _src, unsigned int n, const unsigned int shr)
{
    unsigned int rnd = (1 << shr) >> 1;
    const unsigned int mask = ((1 << shr) - 1);
    const uint16_t * src = (const uint16_t *)_src;

    for (; n != 0; --n) {
        rnd = *src++ + (rnd & mask);
        *dst++ = rnd >> shr;
    }
}
#endif

// Fetches a single patch - offscreen fixup not done here
// w <= stride1
// unclipped
// _x & _w in pixels, strides in bytes
void av_rpi_sand30_to_planar_y16(uint8_t * dst, const unsigned int dst_stride,
                             const uint8_t * src,
                             unsigned int stride1, unsigned int stride2,
                             unsigned int _x, unsigned int y,
                             unsigned int _w, unsigned int h)
{
    const unsigned int x0 = (_x / 3) * 4; // Byte offset of the word
    const unsigned int xskip0 = _x - (x0 >> 2) * 3;
    const unsigned int x1 = ((_x + _w) / 3) * 4;
    const unsigned int xrem1 = _x + _w - (x1 >> 2) * 3;
    const unsigned int mask = stride1 - 1;
    const uint8_t * p0 = src + (x0 & mask) + y * stride1 + (x0 & ~mask) * stride2;
    const unsigned int slice_inc = ((stride2 - 1) * stride1) >> 2;  // RHS of a stripe to LHS of next in words

#if HAVE_SAND_ASM
    if (_x == 0) {
        ff_rpi_sand30_lines_to_planar_y16(dst, dst_stride, src, stride1, stride2, _x, y, _w, h);
        return;
    }
#endif

    if (x0 == x1) {
        // *******************
        // Partial single word xfer
        return;
    }

    for (unsigned int i = 0; i != h; ++i, dst += dst_stride, p0 += stride1)
    {
        unsigned int x = x0;
        const uint32_t * p = (const uint32_t *)p0;
        uint16_t * d = (uint16_t *)dst;

        if (xskip0 != 0) {
            const uint32_t p3 = *p++;

            if (xskip0 == 1)
                *d++ = (p3 >> 10) & 0x3ff;
            *d++ = (p3 >> 20) & 0x3ff;

            if (((x += 4) & mask) == 0)
                p += slice_inc;
        }

        while (x != x1) {
            const uint32_t p3 = *p++;
            *d++ = p3 & 0x3ff;
            *d++ = (p3 >> 10) & 0x3ff;
            *d++ = (p3 >> 20) & 0x3ff;

            if (((x += 4) & mask) == 0)
                p += slice_inc;
        }

        if (xrem1 != 0) {
            const uint32_t p3 = *p;

            *d++ = p3 & 0x3ff;
            if (xrem1 == 2)
                *d++ = (p3 >> 10) & 0x3ff;
        }
    }
}


void av_rpi_sand30_to_planar_c16(uint8_t * dst_u, const unsigned int dst_stride_u,
                             uint8_t * dst_v, const unsigned int dst_stride_v,
                             const uint8_t * src,
                             unsigned int stride1, unsigned int stride2,
                             unsigned int _x, unsigned int y,
                             unsigned int _w, unsigned int h)
{
    const unsigned int x0 = (_x / 3) * 8; // Byte offset of the word
    const unsigned int xskip0 = _x - (x0 >> 3) * 3;
    const unsigned int x1 = ((_x + _w) / 3) * 8;
    const unsigned int xrem1 = _x + _w - (x1 >> 3) * 3;
    const unsigned int mask = stride1 - 1;
    const uint8_t * p0 = src + (x0 & mask) + y * stride1 + (x0 & ~mask) * stride2;
    const unsigned int slice_inc = ((stride2 - 1) * stride1) >> 2;  // RHS of a stripe to LHS of next in words

#if HAVE_SAND_ASM
    if (_x == 0) {
        ff_rpi_sand30_lines_to_planar_c16(dst_u, dst_stride_u, dst_v, dst_stride_v,
                                       src, stride1, stride2, _x, y, _w, h);
        return;
    }
#endif

    if (x0 == x1) {
        // *******************
        // Partial single word xfer
        return;
    }

    for (unsigned int i = 0; i != h; ++i, dst_u += dst_stride_u, dst_v += dst_stride_v, p0 += stride1)
    {
        unsigned int x = x0;
        const uint32_t * p = (const uint32_t *)p0;
        uint16_t * du = (uint16_t *)dst_u;
        uint16_t * dv = (uint16_t *)dst_v;

        if (xskip0 != 0) {
            const uint32_t p3a = *p++;
            const uint32_t p3b = *p++;

            if (xskip0 == 1)
            {
                *du++ = (p3a >> 20) & 0x3ff;
                *dv++ = (p3b >>  0) & 0x3ff;
            }
            *du++ = (p3b >> 10) & 0x3ff;
            *dv++ = (p3b >> 20) & 0x3ff;

            if (((x += 8) & mask) == 0)
                p += slice_inc;
        }

        while (x != x1) {
            const uint32_t p3a = *p++;
            const uint32_t p3b = *p++;

            *du++ = p3a & 0x3ff;
            *dv++ = (p3a >> 10) & 0x3ff;
            *du++ = (p3a >> 20) & 0x3ff;
            *dv++ = p3b & 0x3ff;
            *du++ = (p3b >> 10) & 0x3ff;
            *dv++ = (p3b >> 20) & 0x3ff;

            if (((x += 8) & mask) == 0)
                p += slice_inc;
        }

        if (xrem1 != 0) {
            const uint32_t p3a = *p++;
            const uint32_t p3b = *p++;

            *du++ = p3a & 0x3ff;
            *dv++ = (p3a >> 10) & 0x3ff;
            if (xrem1 == 2)
            {
                *du++ = (p3a >> 20) & 0x3ff;
                *dv++ = p3b & 0x3ff;
            }
        }
    }
}

// Fetches a single patch - offscreen fixup not done here
// w <= stride1
// single lose bottom 2 bits truncation
// _x & _w in pixels, strides in bytes
void av_rpi_sand30_to_planar_y8(uint8_t * dst, const unsigned int dst_stride,
                             const uint8_t * src,
                             unsigned int stride1, unsigned int stride2,
                             unsigned int _x, unsigned int y,
                             unsigned int _w, unsigned int h)
{
    const unsigned int x0 = (_x / 3) * 4; // Byte offset of the word
    const unsigned int xskip0 = _x - (x0 >> 2) * 3;
    const unsigned int x1 = ((_x + _w) / 3) * 4;
    const unsigned int xrem1 = _x + _w - (x1 >> 2) * 3;
    const unsigned int mask = stride1 - 1;
    const uint8_t * p0 = src + (x0 & mask) + y * stride1 + (x0 & ~mask) * stride2;
    const unsigned int slice_inc = ((stride2 - 1) * stride1) >> 2;  // RHS of a stripe to LHS of next in words

#if HAVE_SAND_ASM && 0
    if (_x == 0) {
        ff_rpi_sand30_lines_to_planar_y8(dst, dst_stride, src, stride1, stride2, _x, y, _w, h);
        return;
    }
#endif

    if (x0 == x1) {
        // *******************
        // Partial single word xfer
        return;
    }

    for (unsigned int i = 0; i != h; ++i, dst += dst_stride, p0 += stride1)
    {
        unsigned int x = x0;
        const uint32_t * p = (const uint32_t *)p0;
        uint8_t * d = dst;

        if (xskip0 != 0) {
            const uint32_t p3 = *p++;

            if (xskip0 == 1)
                *d++ = (p3 >> 12) & 0xff;
            *d++ = (p3 >> 22) & 0xff;

            if (((x += 4) & mask) == 0)
                p += slice_inc;
        }

        while (x != x1) {
            const uint32_t p3 = *p++;
            *d++ = (p3 >> 2) & 0xff;
            *d++ = (p3 >> 12) & 0xff;
            *d++ = (p3 >> 22) & 0xff;

            if (((x += 4) & mask) == 0)
                p += slice_inc;
        }

        if (xrem1 != 0) {
            const uint32_t p3 = *p;

            *d++ = (p3 >> 2) & 0xff;
            if (xrem1 == 2)
                *d++ = (p3 >> 12) & 0xff;
        }
    }
}



// w/h in pixels
void av_rpi_sand16_to_sand8(uint8_t * dst, const unsigned int dst_stride1, const unsigned int dst_stride2,
                         const uint8_t * src, const unsigned int src_stride1, const unsigned int src_stride2,
                         unsigned int w, unsigned int h, const unsigned int shr)
{
    const unsigned int n = dst_stride1 / 2;
    unsigned int j;

    // This is true for our current layouts
    av_assert0(dst_stride1 == src_stride1);

    // As we have the same stride1 for src & dest and src is wider than dest
    // then if we loop on src we can always write contiguously to dest
    // We make no effort to copy an exact width - round up to nearest src stripe
    // as we will always have storage in dest for that

#if ARCH_ARM && HAVE_NEON
    if (shr == 3 && src_stride1 == 128) {
        for (j = 0; j + n < w; j += dst_stride1) {
            uint8_t * d = dst + j * dst_stride2;
            const uint8_t * s1 = src + j * 2 * src_stride2;
            const uint8_t * s2 = s1 + src_stride1 * src_stride2;

            ff_rpi_sand128b_stripe_to_8_10(d, s1, s2, h);
        }
    }
    else
#endif
    {
        for (j = 0; j + n < w; j += dst_stride1) {
            uint8_t * d = dst + j * dst_stride2;
            const uint8_t * s1 = src + j * 2 * src_stride2;
            const uint8_t * s2 = s1 + src_stride1 * src_stride2;

            for (unsigned int i = 0; i != h; ++i, s1 += src_stride1, s2 += src_stride1, d += dst_stride1) {
                cpy16_to_8(d, s1, n, shr);
                cpy16_to_8(d + n, s2, n, shr);
            }
        }
    }

    // Fix up a trailing dest half stripe
    if (j < w) {
        uint8_t * d = dst + j * dst_stride2;
        const uint8_t * s1 = src + j * 2 * src_stride2;

        for (unsigned int i = 0; i != h; ++i, s1 += src_stride1, d += dst_stride1) {
            cpy16_to_8(d, s1, n, shr);
        }
    }
}

int av_rpi_sand_to_planar_frame(AVFrame * const dst, const AVFrame * const src)
{
    const int w = av_frame_cropped_width(src);
    const int h = av_frame_cropped_height(src);
    const int x = src->crop_left;
    const int y = src->crop_top;

    // We will crop as part of the conversion
    dst->crop_top = 0;
    dst->crop_left = 0;
    dst->crop_bottom = 0;
    dst->crop_right = 0;

    switch (src->format){
        case AV_PIX_FMT_SAND128:
        case AV_PIX_FMT_RPI4_8:
            switch (dst->format){
                case AV_PIX_FMT_YUV420P:
                    av_rpi_sand_to_planar_y8(dst->data[0], dst->linesize[0],
                                             src->data[0],
                                             av_rpi_sand_frame_stride1(src), av_rpi_sand_frame_stride2(src),
                                             x, y, w, h);
                    av_rpi_sand_to_planar_c8(dst->data[1], dst->linesize[1],
                                             dst->data[2], dst->linesize[2],
                                             src->data[1],
                                             av_rpi_sand_frame_stride1(src), av_rpi_sand_frame_stride2(src),
                                             x/2, y/2,  w/2, h/2);
                    break;
                case AV_PIX_FMT_NV12:
                    av_rpi_sand_to_planar_y8(dst->data[0], dst->linesize[0],
                                             src->data[0],
                                             av_rpi_sand_frame_stride1(src), av_rpi_sand_frame_stride2(src),
                                             x, y, w, h);
                    av_rpi_sand_to_planar_y8(dst->data[1], dst->linesize[1],
                                             src->data[1],
                                             av_rpi_sand_frame_stride1(src), av_rpi_sand_frame_stride2(src),
                                             x/2, y/2, w, h/2);
                    break;
                default:
                    return -1;
            }
            break;
        case AV_PIX_FMT_SAND64_10:
            switch (dst->format){
                case AV_PIX_FMT_YUV420P10:
                    av_rpi_sand_to_planar_y16(dst->data[0], dst->linesize[0],
                                             src->data[0],
                                             av_rpi_sand_frame_stride1(src), av_rpi_sand_frame_stride2(src),
                                             x*2, y, w*2, h);
                    av_rpi_sand_to_planar_c16(dst->data[1], dst->linesize[1],
                                             dst->data[2], dst->linesize[2],
                                             src->data[1],
                                             av_rpi_sand_frame_stride1(src), av_rpi_sand_frame_stride2(src),
                                             x, y/2,  w, h/2);
                    break;
                default:
                    return -1;
            }
            break;
        case AV_PIX_FMT_RPI4_10:
            switch (dst->format){
                case AV_PIX_FMT_YUV420P10:
                    av_rpi_sand30_to_planar_y16(dst->data[0], dst->linesize[0],
                                             src->data[0],
                                             av_rpi_sand_frame_stride1(src), av_rpi_sand_frame_stride2(src),
                                             x, y, w, h);
                    av_rpi_sand30_to_planar_c16(dst->data[1], dst->linesize[1],
                                             dst->data[2], dst->linesize[2],
                                             src->data[1],
                                             av_rpi_sand_frame_stride1(src), av_rpi_sand_frame_stride2(src),
                                             x/2, y/2, w/2, h/2);
                    break;
                case AV_PIX_FMT_NV12:
                    av_rpi_sand30_to_planar_y8(dst->data[0], dst->linesize[0],
                                             src->data[0],
                                             av_rpi_sand_frame_stride1(src), av_rpi_sand_frame_stride2(src),
                                             x, y, w, h);
                    av_rpi_sand30_to_planar_y8(dst->data[1], dst->linesize[1],
                                             src->data[1],
                                             av_rpi_sand_frame_stride1(src), av_rpi_sand_frame_stride2(src),
                                             x/2, y/2, w, h/2);
                    break;
                default:
                    return -1;
            }
            break;
        default:
            return -1;
    }

    return av_frame_copy_props(dst, src);
}
