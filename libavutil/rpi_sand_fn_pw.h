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

// * Included twice from rpi_sand_fn with different PW

#define STRCAT(x,y) x##y

#if PW == 1
#define pixel uint8_t
#define FUNC(f) STRCAT(f, 8)
#elif PW == 2
#define pixel uint16_t
#define FUNC(f) STRCAT(f, 16)
#else
#error Unexpected PW
#endif

// Fetches a single patch - offscreen fixup not done here
// w <= stride1
// unclipped
void FUNC(av_rpi_sand_to_planar_y)(uint8_t * dst, const unsigned int dst_stride,
                             const uint8_t * src,
                             unsigned int stride1, unsigned int stride2,
                             unsigned int _x, unsigned int y,
                             unsigned int _w, unsigned int h)
{
    const unsigned int x = _x;
    const unsigned int w = _w;
    const unsigned int mask = stride1 - 1;

#if PW == 1 && HAVE_SAND_ASM
    if (_x == 0) {
        ff_rpi_sand8_lines_to_planar_y8(dst, dst_stride,
                                     src, stride1, stride2, _x, y, _w, h);
        return;
    }
#endif

    if ((x & ~mask) == ((x + w) & ~mask)) {
        // All in one sand stripe
        const uint8_t * p = src + (x & mask) + y * stride1 + (x & ~mask) * stride2;
        for (unsigned int i = 0; i != h; ++i, dst += dst_stride, p += stride1) {
            memcpy(dst, p, w);
        }
    }
    else
    {
        // Two+ stripe
        const unsigned int sstride = stride1 * stride2;
        const uint8_t * p1 = src + (x & mask) + y * stride1 + (x & ~mask) * stride2;
        const uint8_t * p2 = p1 + sstride - (x & mask);
        const unsigned int w1 = stride1 - (x & mask);
        const unsigned int w3 = (x + w) & mask;
        const unsigned int w2 = w - (w1 + w3);

        for (unsigned int i = 0; i != h; ++i, dst += dst_stride, p1 += stride1, p2 += stride1) {
            unsigned int j;
            const uint8_t * p = p2;
            uint8_t * d = dst;
            memcpy(d, p1, w1);
            d += w1;
            for (j = 0; j < w2; j += stride1, d += stride1, p += sstride) {
                memcpy(d, p, stride1);
            }
            memcpy(d, p, w3);
        }
    }
}

// x & w in bytes but not of interleave (i.e. offset = x*2 for U&V)

void FUNC(av_rpi_sand_to_planar_c)(uint8_t * dst_u, const unsigned int dst_stride_u,
                             uint8_t * dst_v, const unsigned int dst_stride_v,
                             const uint8_t * src,
                             unsigned int stride1, unsigned int stride2,
                             unsigned int _x, unsigned int y,
                             unsigned int _w, unsigned int h)
{
    const unsigned int x = _x * 2;
    const unsigned int w = _w * 2;
    const unsigned int mask = stride1 - 1;

#if PW == 1 && HAVE_SAND_ASM
    if (_x == 0) {
        ff_rpi_sand8_lines_to_planar_c8(dst_u, dst_stride_u, dst_v, dst_stride_v,
                                     src, stride1, stride2, _x, y, _w, h);
        return;
    }
#endif

    if ((x & ~mask) == ((x + w) & ~mask)) {
        // All in one sand stripe
        const uint8_t * p1 = src + (x & mask) + y * stride1 + (x & ~mask) * stride2;
        for (unsigned int i = 0; i != h; ++i, dst_u += dst_stride_u, dst_v += dst_stride_v, p1 += stride1) {
            pixel * du = (pixel *)dst_u;
            pixel * dv = (pixel *)dst_v;
            const pixel * p = (const pixel *)p1;
            for (unsigned int k = 0; k < w; k += 2 * PW) {
                *du++ = *p++;
                *dv++ = *p++;
            }
        }
    }
    else
    {
        // Two+ stripe
        const unsigned int sstride = stride1 * stride2;
        const unsigned int sstride_p = (sstride - stride1) / PW;

        const uint8_t * p1 = src + (x & mask) + y * stride1 + (x & ~mask) * stride2;
        const uint8_t * p2 = p1 + sstride - (x & mask);
        const unsigned int w1 = stride1 - (x & mask);
        const unsigned int w3 = (x + w) & mask;
        const unsigned int w2 = w - (w1 + w3);

        for (unsigned int i = 0; i != h; ++i, dst_u += dst_stride_u, dst_v += dst_stride_v, p1 += stride1, p2 += stride1) {
            unsigned int j;
            const pixel * p = (const pixel *)p1;
            pixel * du = (pixel *)dst_u;
            pixel * dv = (pixel *)dst_v;
            for (unsigned int k = 0; k < w1; k += 2 * PW) {
                *du++ = *p++;
                *dv++ = *p++;
            }
            for (j = 0, p = (const pixel *)p2; j < w2; j += stride1, p += sstride_p) {
                for (unsigned int k = 0; k < stride1; k += 2 * PW) {
                    *du++ = *p++;
                    *dv++ = *p++;
                }
            }
            for (unsigned int k = 0; k < w3; k += 2 * PW) {
                *du++ = *p++;
                *dv++ = *p++;
            }
        }
    }
}

void FUNC(av_rpi_planar_to_sand_c)(uint8_t * dst_c,
                             unsigned int stride1, unsigned int stride2,
                             const uint8_t * src_u, const unsigned int src_stride_u,
                             const uint8_t * src_v, const unsigned int src_stride_v,
                             unsigned int _x, unsigned int y,
                             unsigned int _w, unsigned int h)
{
    const unsigned int x = _x * 2;
    const unsigned int w = _w * 2;
    const unsigned int mask = stride1 - 1;
    if ((x & ~mask) == ((x + w) & ~mask)) {
        // All in one sand stripe
        uint8_t * p1 = dst_c + (x & mask) + y * stride1 + (x & ~mask) * stride2;
        for (unsigned int i = 0; i != h; ++i, src_u += src_stride_u, src_v += src_stride_v, p1 += stride1) {
            const pixel * su = (const pixel *)src_u;
            const pixel * sv = (const pixel *)src_v;
            pixel * p = (pixel *)p1;
            for (unsigned int k = 0; k < w; k += 2 * PW) {
                *p++ = *su++;
                *p++ = *sv++;
            }
        }
    }
    else
    {
        // Two+ stripe
        const unsigned int sstride = stride1 * stride2;
        const unsigned int sstride_p = (sstride - stride1) / PW;

        const uint8_t * p1 = dst_c + (x & mask) + y * stride1 + (x & ~mask) * stride2;
        const uint8_t * p2 = p1 + sstride - (x & mask);
        const unsigned int w1 = stride1 - (x & mask);
        const unsigned int w3 = (x + w) & mask;
        const unsigned int w2 = w - (w1 + w3);

        for (unsigned int i = 0; i != h; ++i, src_u += src_stride_u, src_v += src_stride_v, p1 += stride1, p2 += stride1) {
            unsigned int j;
            const pixel * su = (const pixel *)src_u;
            const pixel * sv = (const pixel *)src_v;
            pixel * p = (pixel *)p1;
            for (unsigned int k = 0; k < w1; k += 2 * PW) {
                *p++ = *su++;
                *p++ = *sv++;
            }
            for (j = 0, p = (pixel *)p2; j < w2; j += stride1, p += sstride_p) {
                for (unsigned int k = 0; k < stride1; k += 2 * PW) {
                    *p++ = *su++;
                    *p++ = *sv++;
                }
            }
            for (unsigned int k = 0; k < w3; k += 2 * PW) {
                *p++ = *su++;
                *p++ = *sv++;
            }
        }
    }
}


#undef pixel
#undef STRCAT
#undef FUNC

