/*
 * HEVC video decoder
 *
 * Copyright (C) 2012 - 2013 Guillaume Martres
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

#include "config.h"
#include "libavutil/pixdesc.h"
#include "libavutil/rpi_sand_fns.h"
#include "bit_depth_template.c"

#include "rpi_hevcdec.h"
#include "rpi_hevcpred.h"

#define DUMP_PRED 0

#define POS(x, y) src[(x) + stride * (y)]

// INCLUDED_ONCE defined at EOF
#ifndef INCLUDED_ONCE
typedef uint8_t (* c8_dst_ptr_t)[2];
typedef const uint8_t (* c8_src_ptr_t)[2];
typedef uint16_t (* c16_dst_ptr_t)[2];
typedef const uint16_t (* c16_src_ptr_t)[2];

// *** On ARM make these NEON registers
typedef struct pixel4_16 {
    uint16_t x[4];
} pixel4_16;
typedef struct pixel4_32 {
    uint32_t x[4];
} pixel4_32;
static inline pixel4_16 PIXEL_SPLAT_X4_16(const uint16_t x)
{
    pixel4_16 t = {{x, x, x, x}};
    return t;
}
static inline pixel4_32 PIXEL_SPLAT_X4_32(const uint32_t x)
{
    pixel4_32 t = {{x, x, x, x}};
    return t;
}
#endif

#if PRED_C
// For chroma we double pixel size so we copy pairs
#undef pixel
#undef pixel2
#undef pixel4
#undef dctcoef
#undef INIT_CLIP
#undef no_rnd_avg_pixel4
#undef rnd_avg_pixel4
#undef AV_RN2P
#undef AV_RN4P
#undef AV_RN4PA
#undef AV_WN2P
#undef AV_WN4P
#undef AV_WN4PA
#undef CLIP
#undef FUNC
#undef FUNCC
#undef av_clip_pixel
#undef PIXEL_SPLAT_X4

#if BIT_DEPTH == 8
#define pixel uint16_t
#define pixel4 pixel4_16
#define PIXEL_SPLAT_X4 PIXEL_SPLAT_X4_16
#define cpel uint8_t
#define c_src_ptr_t  c8_src_ptr_t
#define c_dst_ptr_t  c8_dst_ptr_t
#else
#define pixel uint32_t
#define pixel4 pixel4_32
#define PIXEL_SPLAT_X4 PIXEL_SPLAT_X4_32
#define cpel uint16_t
#define c_src_ptr_t c16_dst_ptr_t
#define c_dst_ptr_t c16_dst_ptr_t
#endif
#define AV_RN4P(p) (*(pixel4*)(p))
#define AV_WN4P(p,x) (*(pixel4*)(p) = (x))
#define FUNC(a) FUNC2(a, BIT_DEPTH, _c)
#endif


// Get PW prior to horrid PRED_C trickery
#if BIT_DEPTH == 8
#define PW 1
#else
#define PW 2
#endif


#if DUMP_PRED && !defined(INCLUDED_ONCE)
static void dump_pred_uv(const uint8_t * data, const unsigned int stride, const unsigned int size)
{
    for (unsigned int y = 0; y != size; y++, data += stride * 2) {
        for (unsigned int x = 0; x != size; x++) {
            printf("%4d", data[x * 2]);
        }
        printf("\n");
    }
    printf("\n");
}
#endif

#ifndef INCLUDED_ONCE
static inline void extend_8(void * ptr, const unsigned int v, unsigned int n)
{
    if ((n >>= 2) != 0) {
        uint32_t v4 = v | (v << 8);
        uint32_t * p = (uint32_t *)ptr;
        v4 = v4 | (v4 << 16);
        do {
            *p++ = v4;
        } while (--n != 0);
    }
}

static inline void extend_16(void * ptr, const unsigned int v, unsigned int n)
{
    if ((n >>= 2) != 0) {
        uint32_t v2 = v | (v << 16);
        uint32_t * p = (uint32_t *)ptr;
        do {
            *p++ = v2;
            *p++ = v2;
        } while (--n != 0);
    }
}

static inline void extend_32(void * ptr, const unsigned int v, unsigned int n)
{
    if ((n >>= 2) != 0) {
        uint32_t * p = (uint32_t *)ptr;
        do {
            *p++ = v;
            *p++ = v;
            *p++ = v;
            *p++ = v;
        } while (--n != 0);
    }
}

// Beware that this inverts the avail ordering
// For CIP it seems easier this way round
static unsigned int cip_avail_l(const uint8_t * is_intra, const int i_stride, const unsigned int i_mask,
                                const unsigned int log2_intra_bits, const unsigned int avail, unsigned int size,
                              unsigned int s0, unsigned int odd_s)
{
    const unsigned int n = 1 << log2_intra_bits;
    unsigned int fa = 0;
    unsigned int i;

    size >>= 2;   // Now in 4-pel units
    s0 >>= 2;

    if ((avail & AVAIL_DL) != 0)
        fa |= ((1 << s0) - 1) << (size - s0);
    if ((avail & AVAIL_L) != 0)
        fa |= ((1 << size) - 1) << size;
    if ((avail & AVAIL_UL) != 0)
        fa |= 1 << (size << 1);

    if (odd_s) {
        if ((fa & 1) != 0 && (*is_intra & i_mask) == 0)
            fa &= ~1;
        is_intra += i_stride;
    }

    for (i = odd_s; (fa >> i) != 0; i += n, is_intra += i_stride) {
        const unsigned int m = ((1 << n) - 1) << i;
        if ((fa & m) != 0 && (*is_intra & i_mask) == 0)
            fa &= ~m;
    }

    return fa;
}

static unsigned int cip_avail_u(const uint8_t * is_intra, unsigned int i_shift,
                                const unsigned int log2_intra_bits, const unsigned int avail, unsigned int size,
                                unsigned int s1, unsigned int odd_s)
{
    if ((avail & (AVAIL_U | AVAIL_UR)) == 0)
    {
        return 0;
    }
    else
    {
        const unsigned int n = 1 << log2_intra_bits;
        unsigned int fa = 0;
        unsigned int i;
        unsigned int im = ((is_intra[1] << 8) | (is_intra[0])) >> i_shift;

        size >>= 2;   // Now in 4-pel units
        s1 >>= 2;

        if ((avail & AVAIL_U) != 0)
            fa |= ((1 << size) - 1);
        if ((avail & AVAIL_UR) != 0)
            fa |= ((1 << s1) - 1) << size;

        if (odd_s) {
            fa &= im | ~1;
            im >>= 1;
        }

        for (i = odd_s; (fa >> i) != 0; i += n, im >>= 1) {
            const unsigned int m = ((1 << n) - 1) << i;
            if ((im & 1) == 0)
                fa &= ~m;
        }
        return fa;
    }
}



static inline unsigned int rmbd(unsigned int x)
{
#if 1
    return __builtin_ctz(x);
#else
    unsigned int n = 0;
    if ((x & 0xffff) == 0) {
        x >>= 16;
        n += 16;
    }
    if ((x & 0xff) == 0) {
        x >>= 8;
        n += 8;
    }
    if ((x & 0xf) == 0) {
        x >>= 4;
        n += 4;
    }
    if ((x & 0x3) == 0) {
        x >>= 2;
        n += 2;
    }

    return (x & 1) == 0 ? n + 1 : n;
#endif
}
#endif


static void FUNC(cip_fill)(pixel * const left, pixel * const top,
    const unsigned int avail_l, const unsigned int avail_u,
    const pixel * const src_l, const pixel * const src_u, const pixel * const src_ur,
    const unsigned int stride,
    const unsigned int size)
{
    pixel a;
    unsigned int i;

    // 1st find DL value
    if ((avail_l & 1) == 0) {
        if (avail_l != 0)
            a = src_l[((int)size * 2 - 1 - (int)rmbd(avail_l)*4) * (int)stride];
        else
        {
            // (avail_l | avail_u) != 0 so this must be good
            const unsigned int n = rmbd(avail_u)*4;
            a = (n >= size) ? src_ur[n - size] : src_u[n];
        }
    }

    // L
    {
        pixel * d = left + size * 2 - 1;
        const pixel * s = src_l + (size * 2 - 1) * stride;
        unsigned int x = avail_l;
        for (i = 0; i < size * 2; i += 4, x >>= 1)
        {
            if ((x & 1) != 0) {
                // Avail
                *d-- = *s;
                s -= stride;
                *d-- = *s;
                s -= stride;
                *d-- = *s;
                s -= stride;
                *d-- = a = *s;
                s -= stride;
            }
            else
            {
                *d-- = a;
                *d-- = a;
                *d-- = a;
                *d-- = a;
                s -= stride * 4;
            }
        }
        // UL
        *d = a = (x & 1) != 0 ? *s : a;
    }

    // U
    {
        pixel * d = top;
        const pixel * s = src_u;
        unsigned int x = avail_u;

        for (i = 0; i < size; i += 4, x >>= 1)
        {
            if ((x & 1) != 0) {
                // Avail
                *d++ = *s++;
                *d++ = *s++;
                *d++ = *s++;
                *d++ = a = *s++;
            }
            else
            {
                *d++ = a;
                *d++ = a;
                *d++ = a;
                *d++ = a;
                s += 4;
            }
        }

        // UR
        s = src_ur;
        for (i = 0; i < size; i += 4, x >>= 1)
        {
            if ((x & 1) != 0) {
                // Avail
                *d++ = *s++;
                *d++ = *s++;
                *d++ = *s++;
                *d++ = a = *s++;
            }
            else
            {
                *d++ = a;
                *d++ = a;
                *d++ = a;
                *d++ = a;
                s += 4;
            }
        }
    }
}


#if !PRED_C && PW == 1
#define EXTEND(ptr, val, len) extend_8(ptr, val, len)
#elif (!PRED_C && PW == 2) || (PRED_C && PW == 1)
#define EXTEND(ptr, val, len) extend_16(ptr, val, len)
#else
#define EXTEND(ptr, val, len) extend_32(ptr, val, len)
#endif

// Reqs:
//
// Planar:  DL[0], L, ul, U, UR[0]
// DC:         dl, L, ul, U, ur
// A2-9:       DL, L, ul, u, ur
// A10:        dl, L, ul, u, ur
// A11-17      dl, L, UL, U, ur
// A18-25      dl, L, Ul, U, ur
// A26         dl, l, ul, U, ur
// A27-34      dl, l, ul, U, UR

#ifndef INCLUDED_ONCE

intra_filter_fn_t ff_hevc_rpi_intra_filter_8_neon_8;
intra_filter_fn_t ff_hevc_rpi_intra_filter_4_neon_16;
intra_filter_fn_t ff_hevc_rpi_intra_filter_8_neon_16;

static const uint8_t req_avail_c[35] =
{
    AVAIL_DL | AVAIL_L | 0         |  AVAIL_U | AVAIL_UR,  // Planar (DL[0] & UR[0] only needed)
               AVAIL_L | 0         |  AVAIL_U,             // DC
    AVAIL_DL | AVAIL_L,                                    // 2
    AVAIL_DL | AVAIL_L,                                    // 3
    AVAIL_DL | AVAIL_L,                                    // 4
    AVAIL_DL | AVAIL_L,                                    // 5
    AVAIL_DL | AVAIL_L,                                    // 6
    AVAIL_DL | AVAIL_L,                                    // 7
    AVAIL_DL | AVAIL_L,                                    // 8
    AVAIL_DL | AVAIL_L,                                    // 9
               AVAIL_L,                                    // 10 (H)
               AVAIL_L | AVAIL_UL | AVAIL_U,               // 11
               AVAIL_L | AVAIL_UL | AVAIL_U,               // 12
               AVAIL_L | AVAIL_UL | AVAIL_U,               // 13
               AVAIL_L | AVAIL_UL | AVAIL_U,               // 14
               AVAIL_L | AVAIL_UL | AVAIL_U,               // 15
               AVAIL_L | AVAIL_UL | AVAIL_U,               // 16
               AVAIL_L | AVAIL_UL | AVAIL_U,               // 17
               AVAIL_L | AVAIL_UL | AVAIL_U,               // 18
               AVAIL_L | AVAIL_UL | AVAIL_U,               // 19
               AVAIL_L | AVAIL_UL | AVAIL_U,               // 20
               AVAIL_L | AVAIL_UL | AVAIL_U,               // 21
               AVAIL_L | AVAIL_UL | AVAIL_U,               // 22
               AVAIL_L | AVAIL_UL | AVAIL_U,               // 23
               AVAIL_L | AVAIL_UL | AVAIL_U,               // 24
               AVAIL_L | AVAIL_UL | AVAIL_U,               // 25
                                    AVAIL_U,               // 26 (V)
                                    AVAIL_U | AVAIL_UR,    // 27
                                    AVAIL_U | AVAIL_UR,    // 28
                                    AVAIL_U | AVAIL_UR,    // 29
                                    AVAIL_U | AVAIL_UR,    // 30
                                    AVAIL_U | AVAIL_UR,    // 31
                                    AVAIL_U | AVAIL_UR,    // 32
                                    AVAIL_U | AVAIL_UR,    // 33
                                    AVAIL_U | AVAIL_UR     // 34
};

static const uint8_t req_avail[4][35] = {
{
    AVAIL_DL | AVAIL_L | 0         |  AVAIL_U | AVAIL_UR,  // Planar (DL[0] & UR[0] only needed)
               AVAIL_L | 0         |  AVAIL_U,             // DC
    AVAIL_DL | AVAIL_L,                                    // 2
    AVAIL_DL | AVAIL_L,                                    // 3
    AVAIL_DL | AVAIL_L,                                    // 4
    AVAIL_DL | AVAIL_L,                                    // 5
    AVAIL_DL | AVAIL_L,                                    // 6
    AVAIL_DL | AVAIL_L,                                    // 7
    AVAIL_DL | AVAIL_L,                                    // 8
    AVAIL_DL | AVAIL_L,                                    // 9
               AVAIL_L | AVAIL_UL | AVAIL_U,               // 10 (H)
               AVAIL_L | AVAIL_UL | AVAIL_U,               // 11
               AVAIL_L | AVAIL_UL | AVAIL_U,               // 12
               AVAIL_L | AVAIL_UL | AVAIL_U,               // 13
               AVAIL_L | AVAIL_UL | AVAIL_U,               // 14
               AVAIL_L | AVAIL_UL | AVAIL_U,               // 15
               AVAIL_L | AVAIL_UL | AVAIL_U,               // 16
               AVAIL_L | AVAIL_UL | AVAIL_U,               // 17
               AVAIL_L | AVAIL_UL | AVAIL_U,               // 18
               AVAIL_L | AVAIL_UL | AVAIL_U,               // 19
               AVAIL_L | AVAIL_UL | AVAIL_U,               // 20
               AVAIL_L | AVAIL_UL | AVAIL_U,               // 21
               AVAIL_L | AVAIL_UL | AVAIL_U,               // 22
               AVAIL_L | AVAIL_UL | AVAIL_U,               // 23
               AVAIL_L | AVAIL_UL | AVAIL_U,               // 24
               AVAIL_L | AVAIL_UL | AVAIL_U,               // 25
               AVAIL_L | AVAIL_UL | AVAIL_U,               // 26 (V)
                                    AVAIL_U | AVAIL_UR,    // 27
                                    AVAIL_U | AVAIL_UR,    // 28
                                    AVAIL_U | AVAIL_UR,    // 29
                                    AVAIL_U | AVAIL_UR,    // 30
                                    AVAIL_U | AVAIL_UR,    // 31
                                    AVAIL_U | AVAIL_UR,    // 32
                                    AVAIL_U | AVAIL_UR,    // 33
                                    AVAIL_U | AVAIL_UR     // 34
},
{  // 3
    AVAIL_DL | AVAIL_L | 0        | AVAIL_U | AVAIL_UR | FILTER_LIGHT,  // Planar (DL[0] & UR[0] only needed)
               AVAIL_L | 0        | AVAIL_U,                            // DC
    AVAIL_DL | AVAIL_L                                 | FILTER_LIGHT,  // 2
    AVAIL_DL | AVAIL_L                                 | 0,             // 3
    AVAIL_DL | AVAIL_L                                 | 0,             // 4
    AVAIL_DL | AVAIL_L                                 | 0,             // 5
    AVAIL_DL | AVAIL_L                                 | 0,             // 6
    AVAIL_DL | AVAIL_L                                 | 0,             // 7
    AVAIL_DL | AVAIL_L                                 | 0,             // 8
    AVAIL_DL | AVAIL_L                                 | 0,             // 9
               AVAIL_L | AVAIL_UL | AVAIL_U            | 0,             // 10 (H)
               AVAIL_L | AVAIL_UL | AVAIL_U            | 0,             // 11
               AVAIL_L | AVAIL_UL | AVAIL_U            | 0,             // 12
               AVAIL_L | AVAIL_UL | AVAIL_U            | 0,             // 13
               AVAIL_L | AVAIL_UL | AVAIL_U            | 0,             // 14
               AVAIL_L | AVAIL_UL | AVAIL_U            | 0,             // 15
               AVAIL_L | AVAIL_UL | AVAIL_U            | 0,             // 16
               AVAIL_L | AVAIL_UL | AVAIL_U            | 0,             // 17
               AVAIL_L | AVAIL_UL | AVAIL_U            | FILTER_LIGHT,  // 18
               AVAIL_L | AVAIL_UL | AVAIL_U            | 0,             // 19
               AVAIL_L | AVAIL_UL | AVAIL_U            | 0,             // 20
               AVAIL_L | AVAIL_UL | AVAIL_U            | 0,             // 21
               AVAIL_L | AVAIL_UL | AVAIL_U            | 0,             // 22
               AVAIL_L | AVAIL_UL | AVAIL_U            | 0,             // 23
               AVAIL_L | AVAIL_UL | AVAIL_U            | 0,             // 24
               AVAIL_L | AVAIL_UL | AVAIL_U            | 0,             // 25
               AVAIL_L | AVAIL_UL | AVAIL_U            | 0,             // 26 (V)
                                    AVAIL_U | AVAIL_UR | 0,             // 27
                                    AVAIL_U | AVAIL_UR | 0,             // 28
                                    AVAIL_U | AVAIL_UR | 0,             // 29
                                    AVAIL_U | AVAIL_UR | 0,             // 30
                                    AVAIL_U | AVAIL_UR | 0,             // 31
                                    AVAIL_U | AVAIL_UR | 0,             // 32
                                    AVAIL_U | AVAIL_UR | 0,             // 33
                                    AVAIL_U | AVAIL_UR | FILTER_LIGHT   // 34
},
{  // 4
    AVAIL_DL | AVAIL_L | 0        | AVAIL_U | AVAIL_UR | FILTER_LIGHT,  // Planar (DL[0] & UR[0] only needed)
               AVAIL_L | 0        | AVAIL_U,                            // DC
    AVAIL_DL | AVAIL_L                                 | FILTER_LIGHT,  // 2
    AVAIL_DL | AVAIL_L                                 | FILTER_LIGHT,  // 3
    AVAIL_DL | AVAIL_L                                 | FILTER_LIGHT,  // 4
    AVAIL_DL | AVAIL_L                                 | FILTER_LIGHT,  // 5
    AVAIL_DL | AVAIL_L                                 | FILTER_LIGHT,  // 6
    AVAIL_DL | AVAIL_L                                 | FILTER_LIGHT,  // 7
    AVAIL_DL | AVAIL_L                                 | FILTER_LIGHT,  // 8
    AVAIL_DL | AVAIL_L                                 | 0,             // 9
               AVAIL_L | AVAIL_UL | AVAIL_U            | 0,             // 10 (H)
               AVAIL_L | AVAIL_UL | AVAIL_U            | 0,             // 11
               AVAIL_L | AVAIL_UL | AVAIL_U            | FILTER_LIGHT,  // 12
               AVAIL_L | AVAIL_UL | AVAIL_U            | FILTER_LIGHT,  // 13
               AVAIL_L | AVAIL_UL | AVAIL_U            | FILTER_LIGHT,  // 14
               AVAIL_L | AVAIL_UL | AVAIL_U            | FILTER_LIGHT,  // 15
               AVAIL_L | AVAIL_UL | AVAIL_U            | FILTER_LIGHT,  // 16
               AVAIL_L | AVAIL_UL | AVAIL_U            | FILTER_LIGHT,  // 17
               AVAIL_L | AVAIL_UL | AVAIL_U            | FILTER_LIGHT,  // 18
               AVAIL_L | AVAIL_UL | AVAIL_U            | FILTER_LIGHT,  // 19
               AVAIL_L | AVAIL_UL | AVAIL_U            | FILTER_LIGHT,  // 20
               AVAIL_L | AVAIL_UL | AVAIL_U            | FILTER_LIGHT,  // 21
               AVAIL_L | AVAIL_UL | AVAIL_U            | FILTER_LIGHT,  // 22
               AVAIL_L | AVAIL_UL | AVAIL_U            | FILTER_LIGHT,  // 23
               AVAIL_L | AVAIL_UL | AVAIL_U            | FILTER_LIGHT,  // 24
               AVAIL_L | AVAIL_UL | AVAIL_U            | 0,             // 25
               AVAIL_L | AVAIL_UL | AVAIL_U            | 0,             // 26 (V)
                                    AVAIL_U | AVAIL_UR | 0,             // 27
                                    AVAIL_U | AVAIL_UR | FILTER_LIGHT,  // 28
                                    AVAIL_U | AVAIL_UR | FILTER_LIGHT,  // 29
                                    AVAIL_U | AVAIL_UR | FILTER_LIGHT,  // 30
                                    AVAIL_U | AVAIL_UR | FILTER_LIGHT,  // 31
                                    AVAIL_U | AVAIL_UR | FILTER_LIGHT,  // 32
                                    AVAIL_U | AVAIL_UR | FILTER_LIGHT,  // 33
                                    AVAIL_U | AVAIL_UR | FILTER_LIGHT   // 34
},
{  // 5
    AVAIL_DL | AVAIL_L | 0        | AVAIL_U | AVAIL_UR | FILTER_EITHER, // Planar (DL[0] & UR[0] only needed)
               AVAIL_L | 0        | AVAIL_U,                            // DC
    AVAIL_DL | AVAIL_L                                 | FILTER_EITHER, // 2
    AVAIL_DL | AVAIL_L                                 | FILTER_EITHER, // 3
    AVAIL_DL | AVAIL_L                                 | FILTER_EITHER, // 4
    AVAIL_DL | AVAIL_L                                 | FILTER_EITHER, // 5
    AVAIL_DL | AVAIL_L                                 | FILTER_EITHER, // 6
    AVAIL_DL | AVAIL_L                                 | FILTER_EITHER, // 7
    AVAIL_DL | AVAIL_L                                 | FILTER_EITHER, // 8
    AVAIL_DL | AVAIL_L                                 | FILTER_EITHER, // 9
               AVAIL_L                                 | 0,             // 10 (H)
               AVAIL_L | AVAIL_UL | AVAIL_U            | FILTER_EITHER, // 11
               AVAIL_L | AVAIL_UL | AVAIL_U            | FILTER_EITHER, // 12
               AVAIL_L | AVAIL_UL | AVAIL_U            | FILTER_EITHER, // 13
               AVAIL_L | AVAIL_UL | AVAIL_U            | FILTER_EITHER, // 14
               AVAIL_L | AVAIL_UL | AVAIL_U            | FILTER_EITHER, // 15
               AVAIL_L | AVAIL_UL | AVAIL_U            | FILTER_EITHER, // 16
               AVAIL_L | AVAIL_UL | AVAIL_U            | FILTER_EITHER, // 17
               AVAIL_L | AVAIL_UL | AVAIL_U            | FILTER_EITHER, // 18
               AVAIL_L | AVAIL_UL | AVAIL_U            | FILTER_EITHER, // 19
               AVAIL_L | AVAIL_UL | AVAIL_U            | FILTER_EITHER, // 20
               AVAIL_L | AVAIL_UL | AVAIL_U            | FILTER_EITHER, // 21
               AVAIL_L | AVAIL_UL | AVAIL_U            | FILTER_EITHER, // 22
               AVAIL_L | AVAIL_UL | AVAIL_U            | FILTER_EITHER, // 23
               AVAIL_L | AVAIL_UL | AVAIL_U            | FILTER_EITHER, // 24
               AVAIL_L | AVAIL_UL | AVAIL_U            | FILTER_EITHER, // 25
                                    AVAIL_U            | 0,             // 26 (V)
                                    AVAIL_U | AVAIL_UR | FILTER_EITHER, // 27
                                    AVAIL_U | AVAIL_UR | FILTER_EITHER, // 28
                                    AVAIL_U | AVAIL_UR | FILTER_EITHER, // 29
                                    AVAIL_U | AVAIL_UR | FILTER_EITHER, // 30
                                    AVAIL_U | AVAIL_UR | FILTER_EITHER, // 31
                                    AVAIL_U | AVAIL_UR | FILTER_EITHER, // 32
                                    AVAIL_U | AVAIL_UR | FILTER_EITHER, // 33
                                    AVAIL_U | AVAIL_UR | FILTER_EITHER  // 34
}
};


#endif

#define filter_light1 FUNC(filter_light1)
static inline pixel filter_light1(pixel a, pixel b, pixel c)
{
    return (a + b*2 + c + 2) >> 2;
}

#define filter_light FUNC(filter_light)
static inline void filter_light(pixel * dst, pixel p1, const pixel * src, const pixel pn, const int sstride, const unsigned int n)
{
    pixel p0;
    pixel p2 = *src;
    // Allow for final pel - it is just clearer to to have the call take the actual number of output pels
    unsigned int n_minus_1 = n - 1;

    do
    {
        src += sstride;
        p0 = p1;
        p1 = p2;
        p2 = *src;
        *dst++ = filter_light1(p0, p1, p2);
    } while (--n_minus_1 != 0);
    *dst = filter_light1(p1, p2, pn);
}

#define filter_strong FUNC(filter_strong)
static inline void filter_strong(pixel * dst, const unsigned int p0, const unsigned int p1, unsigned int n)
{
    unsigned int a = 64 * p0 + 32;
    const int v = p1 - p0;

    do
    {
        *dst++ = (a += v) >> 6;
    } while (--n != 0);
}

#define intra_filter FUNC(intra_filter)
static av_always_inline void intra_filter(
    pixel * const left, pixel * const top,
    const unsigned int req, const unsigned int avail,
    const pixel * const src_l, const pixel * const src_u, const pixel * const src_ur,
    const unsigned int stride,
    const unsigned int top_right_size, const unsigned int down_left_size,
    const unsigned int log2_size)
{
    const unsigned int strong_threshold = 1 << (BIT_DEPTH - 5);
    const unsigned int size = 1 << log2_size;

    // a_ is the first pel in a section working round dl -> ur
    // b_ is the last
    // Beware that top & left work out from UL so usage of a_ & b_ may
    // swap between them.  It is a bad naming scheme but I have found no
    // better
    const pixel * a_dl = src_l + (down_left_size + size - 1) * stride;
    const pixel * b_dl = src_l + size * stride;
    const pixel * a_l  = src_l + (size - 1) * stride;
    const pixel * b_l  = src_l;
    const pixel * ab_ul = src_l - stride;
    const pixel * a_u = src_u;
    const pixel * b_u = src_u + size - 1;
    const pixel * a_ur = src_ur;
    const pixel * b_ur = src_ur + top_right_size - 1;

    const unsigned int want = req & ~avail;
    const unsigned int have = req & avail;
    unsigned int i;

    if ((avail & AVAIL_DL) == 0)
    {
        a_dl = a_ur;
        if ((avail & AVAIL_U) != 0)
            a_dl = a_u;
        if ((avail & AVAIL_UL) != 0)
            a_dl = ab_ul;
        if ((avail & AVAIL_L) != 0)
            a_dl = a_l;
        b_dl = a_dl;
    }

    if ((avail & AVAIL_L) == 0)
    {
        a_l = b_dl;
        b_l = b_dl;
    }
    if ((avail & AVAIL_UL) == 0)
    {
        ab_ul = b_l;
    }
    if ((avail & AVAIL_U) == 0)
    {
        a_u = ab_ul;
        b_u = ab_ul;
    }
    if ((avail & AVAIL_UR) == 0)
    {
        a_ur = b_u;
        b_ur = b_u;
    }

    if ((req & FILTER_LIGHT) == 0 || PRED_C || log2_size == 2)  // PRED_C, log2_size compiler opt hints
    {
        if ((req & AVAIL_UL) != 0)
            left[-1] = *ab_ul;

        if ((want & AVAIL_L) != 0)
            EXTEND(left, *a_l, size);
        if ((want & AVAIL_DL) != 0)
            EXTEND(left + size, *a_dl, size);
        if ((want & AVAIL_U) != 0)
            EXTEND(top, *a_u, size);
        if ((want & AVAIL_UR) != 0)
            EXTEND(top + size, *a_ur, size);

        if ((have & AVAIL_U) != 0)
            // Always good - even with sand
            memcpy(top, a_u, size * sizeof(pixel));
        if ((have & AVAIL_UR) != 0)
        {
            memcpy(top + size, a_ur, top_right_size * sizeof(pixel));
            EXTEND(top + size + top_right_size, *b_ur,
                   size - top_right_size);
        }
        if ((have & AVAIL_L) != 0)
        {
            for (i = 0; i < size; i++)
                left[i] = b_l[stride * i];
        }
        if ((have & AVAIL_DL) != 0)
        {
            for (i = 0; i < down_left_size; i++)
                left[i + size] = b_dl[stride * i];
            EXTEND(left + size + down_left_size, *a_dl,
                   size - down_left_size);
        }
    }
    else if ((req & FILTER_STRONG) != 0 && log2_size == 5 && // log2_size compiler opt hint
            FFABS((int)(*a_dl - *a_l * 2 + *ab_ul)) < strong_threshold &&
            FFABS((int)(*ab_ul - *b_u * 2 + *b_ur)) < strong_threshold)
    {
        if ((req & (AVAIL_U | AVAIL_UR)) != 0)
            filter_strong(top, *ab_ul, *b_ur, size * 2);
        left[-1] = *ab_ul;
        if ((req & (AVAIL_L | AVAIL_DL)) != 0)
            filter_strong(left, *ab_ul, *a_dl, size*2);
    }
    else
    {
        // Same code for both have & want for UL
        if ((req & AVAIL_UL) != 0)
        {
            left[-1] = filter_light1(*b_l, *ab_ul, *a_u);
        }

        if ((want & AVAIL_L) != 0)
        {
            EXTEND(left, *a_l, size);
            left[0] = (*a_l * 3 + *ab_ul + 2) >> 2;
        }
        if ((want & AVAIL_DL) != 0)
        {
            // If we want DL then it cannot be avail so a_dl = a_l so no edge rounding
            EXTEND(left + size, *a_l, size);
        }
        if ((want & AVAIL_U) != 0)
        {
            EXTEND(top, *a_u, size);
            top[size - 1] = (*a_u * 3 + *a_ur + 2) >> 2;
        }
        if ((want & AVAIL_UR) != 0)
        {
            // If we want UR then it cannot be avail so a_ur = b_u so no edge rounding
            EXTEND(top + size, *a_ur, size);
        }

        if ((have & AVAIL_U) != 0)
        {
            filter_light(top, *ab_ul, a_u, *a_ur, 1, size);
        }
        if ((have & AVAIL_UR) != 0) {
            filter_light(top + size, *b_u, a_ur, *b_ur, 1, top_right_size);
            top[size*2 - 1] = *b_ur;
            EXTEND(top + size + top_right_size, *b_ur, size - top_right_size);
        }
        if ((have & AVAIL_L) != 0)
        {
            filter_light(left, *ab_ul, b_l, *b_dl, stride, size);
        }
        if ((have & AVAIL_DL) != 0)
        {
            filter_light(left + size, *a_l, b_dl, *a_dl, stride, down_left_size);
            left[size*2 - 1] = *a_dl;
            EXTEND(left + size + down_left_size, *a_dl, size - down_left_size);
        }
    }
}

#define INTRA_FILTER(log2_size) \
static void FUNC(intra_filter_ ## log2_size)( \
     uint8_t * const left, uint8_t * const top, \
     const unsigned int req, const unsigned int avail, \
     const uint8_t * const src_l, const uint8_t * const src_u, const uint8_t * const src_ur, \
     const unsigned int stride, \
     const unsigned int top_right_size, const unsigned int down_left_size) \
{ \
    intra_filter((pixel *)left, (pixel *)top, req, avail, \
        (const pixel *)src_l, (const pixel *)src_u, (const pixel *)src_ur, stride / sizeof(pixel), top_right_size, down_left_size, log2_size); \
}

INTRA_FILTER(2)
INTRA_FILTER(3)
INTRA_FILTER(4)
INTRA_FILTER(5)

#undef intra_filter
#undef INTRA_FILTER

static void FUNC(intra_pred)(const HEVCRpiContext * const s,
                                              const enum IntraPredMode mode, const unsigned int x0, const unsigned int y0, const unsigned int avail,
                                              const unsigned int log2_size)
{
    // c_idx will alaways be 1 for _c versions and 0 for y
    const unsigned int c_idx = PRED_C;
    const unsigned int hshift = ctx_hshift(s, c_idx);
    const unsigned int vshift = ctx_vshift(s, c_idx);
    const unsigned int size = (1 << log2_size);
    const unsigned int x = x0 >> hshift;
    const unsigned int y = y0 >> vshift;

    const ptrdiff_t stride = frame_stride1(s->frame, c_idx) / sizeof(pixel);
    pixel *const src = c_idx == 0 ?
        (pixel *)av_rpi_sand_frame_pos_y(s->frame, x, y) :
        (pixel *)av_rpi_sand_frame_pos_c(s->frame, x, y);

    // Align so we can do multiple loads in the asm
    // Padded to 16 byte boundary so as not to confuse anything
    DECLARE_ALIGNED(16, pixel, top[2 * MAX_TB_SIZE]);
    DECLARE_ALIGNED(16, pixel, left_array[2 * MAX_TB_SIZE + 16 / sizeof(pixel)]);

    pixel  * const left  = left_array  + 16 / sizeof(pixel);
    const pixel * top_pred = top;

    const pixel * src_l = src - 1;
    const pixel * src_u = src - stride;
    const pixel * src_ur = src_u + size;
#if !PRED_C
    const unsigned int req = req_avail[log2_size - 2][mode] & ~s->ps.sps->intra_filters_disable;
#else
    const unsigned int req = req_avail_c[mode];
#endif

    // If we have nothing to pred from then fill with grey
    // This isn't a common case but dealing with it here means we don't have to
    // test for it later
    if (avail == 0)
    {
dc_only:
#if !PRED_C
        s->hpc.pred_dc0[log2_size - 2]((uint8_t *)src, stride);
#else
        s->hpc.pred_dc0_c[log2_size - 2]((uint8_t *)src, stride);
#endif
        return;
    }

    {
        // N.B. stride is in pixels (not bytes) or in the case of chroma pixel-pairs
        const AVFrame * const frame = s->frame;
        const unsigned int mask = stride - 1; // For chroma pixel=uint16 so stride_c is stride_y / 2
        const unsigned int stripe_adj = (av_rpi_sand_frame_stride2(frame) - 1) * stride;
        if ((x & mask) == 0)
            src_l -= stripe_adj;
        if (((x + size) & mask) == 0)
            src_ur += stripe_adj;
    }

    // Can deal with I-slices in 'normal' code even if CIP
    // This also means that we don't need to generate (elsewhere) is_intra
    // for IRAP frames
    if (s->ps.pps->constrained_intra_pred_flag == 1 &&
        s->sh.slice_type != HEVC_SLICE_I)
    {
        // * If we ever actually care about CIP performance then we should
        //   special case out size 4 stuff (can be done by 'normal') and
        //   have 8-pel avail masks
        unsigned int avail_l = cip_avail_l(s->is_intra + ((y + size * 2 - 1) >> (3 - vshift)) * s->ps.sps->pcm_width + ((x - 1) >> (6 - hshift)),
                                           -(int)(s->ps.sps->pcm_width),
                                           1 << (((x - 1) >> (3 - hshift)) & 7),
                                           1 - hshift,
                                           avail,
                                           size,
                                           FFMIN(size, ((s->ps.sps->height - y0) >> vshift) - size),
                                           vshift != 0 ? 0 : (y >> 2) & 1);

        unsigned int avail_u = cip_avail_u(s->is_intra + ((y - 1) >> (3 - vshift)) * s->ps.sps->pcm_width + (x >> (6 - hshift)),
                                           (x >> (3 - hshift)) & 7,
                                           1 - hshift,
                                           avail,
                                           size,
                                           FFMIN(size, ((s->ps.sps->width - x0) >> hshift) - size),
                                           hshift != 0 ? 0 : (x >> 2) & 1);

        // Anything left?
        if ((avail_l | avail_u) == 0)
            goto dc_only;

        FUNC(cip_fill)(left, top, avail_l, avail_u, src_l, src_u, src_ur, stride, size);

#if !PRED_C
        if ((req & FILTER_LIGHT) != 0)
        {
            const unsigned threshold = 1 << (BIT_DEPTH - 5);
            if ((req & FILTER_STRONG) != 0 &&
                (int)(FFABS(left[-1]  + top[63] - 2 * top[31]))  < threshold &&
                (int)(FFABS(left[-1] + left[63] - 2 * left[31])) < threshold)
            {
                filter_strong(top, left[-1], top[63], 64);
                filter_strong(left, left[-1], left[63], 64);
            } else
            {
                // LHS writes UL too so copy for top
                const pixel p_ul = left[-1];
                filter_light(left - 1, top[0], left - 1, left[2*size - 1], 1, 2*size);
                filter_light(top, p_ul, top, top[2*size - 1], 1, 2*size - 1);
            }
        }
#endif
    }
    else
    {
        const unsigned int ur_size = FFMIN(size, ((s->ps.sps->width - x0) >> hshift) - size);
        if ((req & ~((AVAIL_UR | AVAIL_U) & avail)) == 0 &&
            ((req & AVAIL_UR) == 0 || src_u + 2*size == src_ur + ur_size))
        {
            top_pred = src_u;
        }
        else
        {
#if !PRED_C
            s->hpc.intra_filter[log2_size - 2]
#else
            s->hpc.intra_filter_c[log2_size - 2]
#endif
                ((uint8_t *)left, (uint8_t *)top, req, avail,
                 (const uint8_t *)src_l, (const uint8_t *)src_u, (const uint8_t *)src_ur, stride * sizeof(pixel),
                              ur_size,
                              FFMIN(size, ((s->ps.sps->height - y0) >> vshift) - size));
        }
    }


#if !PRED_C
    switch (mode) {
    case INTRA_PLANAR:
        s->hpc.pred_planar[log2_size - 2]((uint8_t *)src, (uint8_t *)top_pred,
                                          (uint8_t *)left, stride);
        break;
    case INTRA_DC:
        s->hpc.pred_dc[log2_size - 2]((uint8_t *)src, (uint8_t *)top_pred,
                       (uint8_t *)left, stride);
        break;
    case INTRA_ANGULAR_HORIZONTAL:
        s->hpc.pred_horizontal[log2_size - 2]((uint8_t *)src, (uint8_t *)top_pred,
                                           (uint8_t *)left, stride,
                                           mode);
        break;
    case INTRA_ANGULAR_VERTICAL:
        s->hpc.pred_vertical[log2_size - 2]((uint8_t *)src, (uint8_t *)top_pred,
                                           (uint8_t *)left, stride,
                                           mode);
        break;
    default:
        s->hpc.pred_angular[log2_size - 2]((uint8_t *)src, (uint8_t *)top_pred,
                                           (uint8_t *)left, stride,
                                           mode);
        break;
    }
#else
    switch (mode) {
    case INTRA_PLANAR:
        s->hpc.pred_planar_c[log2_size - 2]((uint8_t *)src, (uint8_t *)top_pred,
                                          (uint8_t *)left, stride);
        break;
    case INTRA_DC:
        s->hpc.pred_dc_c[log2_size - 2]((uint8_t *)src, (uint8_t *)top_pred,
                       (uint8_t *)left, stride);
        break;
    case INTRA_ANGULAR_HORIZONTAL:
        s->hpc.pred_horizontal_c[log2_size - 2]((uint8_t *)src, (uint8_t *)top_pred,
                                           (uint8_t *)left, stride,
                                           mode);
        break;
    case INTRA_ANGULAR_VERTICAL:
        s->hpc.pred_vertical_c[log2_size - 2]((uint8_t *)src, (uint8_t *)top_pred,
                                           (uint8_t *)left, stride,
                                           mode);
        break;
    default:
        s->hpc.pred_angular_c[log2_size - 2]((uint8_t *)src, (uint8_t *)top_pred,
                                           (uint8_t *)left, stride,
                                           mode);
        break;
    }

#if DUMP_PRED
    printf("U pred @ %d, %d: mode=%d\n", x, y, mode);
    dump_pred_uv((uint8_t *)src, stride, 1 << log2_size);
    printf("V pred @ %d, %d: mode=%d\n", x, y, mode);
    dump_pred_uv((uint8_t *)src + 1, stride, 1 << log2_size);
#endif
#endif
}

#if !PRED_C
static av_always_inline void FUNC(pred_planar)(uint8_t *_src, const uint8_t *_top,
                                  const uint8_t *_left, ptrdiff_t stride,
                                  int trafo_size)
{
    int x, y;
    pixel *src        = (pixel *)_src;
    const pixel *top  = (const pixel *)_top;
    const pixel *left = (const pixel *)_left;
    int size = 1 << trafo_size;
    for (y = 0; y < size; y++)
        for (x = 0; x < size; x++)
            POS(x, y) = ((size - 1 - x) * left[y] + (x + 1) * top[size]  +
                         (size - 1 - y) * top[x]  + (y + 1) * left[size] + size) >> (trafo_size + 1);
}
#else
static av_always_inline void FUNC(pred_planar)(uint8_t * _src, const uint8_t * _top,
                                  const uint8_t * _left, ptrdiff_t stride,
                                  int trafo_size)
{
    int x, y;
    int size = 1 << trafo_size;
    c_dst_ptr_t src = (c_dst_ptr_t)_src;
    const c_src_ptr_t top = (c_src_ptr_t)_top;
    const c_src_ptr_t left = (c_src_ptr_t)_left;

    for (y = 0; y < size; y++, src += stride)
    {
        for (x = 0; x < size; x++)
        {
            src[x][0] = ((size - 1 - x) * left[y][0] + (x + 1) * top[size][0]  +
                         (size - 1 - y) * top[x][0]  + (y + 1) * left[size][0] + size) >> (trafo_size + 1);
            src[x][1] = ((size - 1 - x) * left[y][1] + (x + 1) * top[size][1]  +
                         (size - 1 - y) * top[x][1]  + (y + 1) * left[size][1] + size) >> (trafo_size + 1);
        }
    }
}
#endif

#define PRED_PLANAR(size)\
static void FUNC(pred_planar_ ## size)(uint8_t *src, const uint8_t *top,        \
                                       const uint8_t *left, ptrdiff_t stride)   \
{                                                                               \
    FUNC(pred_planar)(src, top, left, stride, size + 2);                        \
}

PRED_PLANAR(0)
PRED_PLANAR(1)
PRED_PLANAR(2)
PRED_PLANAR(3)

#undef PRED_PLANAR

#if !PRED_C
static void FUNC(pred_dc)(uint8_t *_src, const uint8_t *_top,
                          const uint8_t *_left,
                          ptrdiff_t stride, int log2_size)
{
    int i, j, x, y;
    int size          = (1 << log2_size);
    pixel *src        = (pixel *)_src;
    const pixel *top  = (const pixel *)_top;
    const pixel *left = (const pixel *)_left;
    int dc            = size;
    pixel4 a;
    for (i = 0; i < size; i++)
        dc += left[i] + top[i];

    dc >>= log2_size + 1;

    a = PIXEL_SPLAT_X4(dc);

    for (i = 0; i < size; i++)
        for (j = 0; j < size; j+=4)
            AV_WN4P(&POS(j, i), a);

//    if (c_idx == 0 && size < 32)
// As we now have separate fns for y & c - no need to test that
    if (size < 32)
    {
        POS(0, 0) = (left[0] + 2 * dc + top[0] + 2) >> 2;
        for (x = 1; x < size; x++)
            POS(x, 0) = (top[x] + 3 * dc + 2) >> 2;
        for (y = 1; y < size; y++)
            POS(0, y) = (left[y] + 3 * dc + 2) >> 2;
    }
}
#else
static void FUNC(pred_dc)(uint8_t *_src, const uint8_t *_top,
                          const uint8_t *_left,
                          ptrdiff_t stride, int log2_size)
{
    unsigned int i, j;
    const unsigned int size = (1 << log2_size);
    c_dst_ptr_t src = (c_dst_ptr_t)_src;
    const c_src_ptr_t top = (c_src_ptr_t)_top;
    const c_src_ptr_t left = (c_src_ptr_t)_left;
    unsigned int dc0 = size;
    unsigned int dc1 = size;

    for (i = 0; i < size; i++)
    {
        dc0 += left[i][0] + top[i][0];
        dc1 += left[i][1] + top[i][1];
    }

    dc0 >>= log2_size + 1;
    dc1 >>= log2_size + 1;

    for (i = 0; i < size; i++, src += stride)
    {
        for (j = 0; j < size; ++j)
        {
            src[j][0] = dc0;
            src[j][1] = dc1;

        }
    }
}
#endif

#define PRED_DC(size)\
static void FUNC(pred_dc_ ## size)(uint8_t *src, const uint8_t *top,        \
                                       const uint8_t *left, ptrdiff_t stride)   \
{                                                                               \
    FUNC(pred_dc)(src, top, left, stride, size + 2);                        \
}

PRED_DC(0)
PRED_DC(1)
PRED_DC(2)
PRED_DC(3)

#undef PRED_DC




#if !PRED_C
static void FUNC(pred_dc0)(uint8_t *_src, ptrdiff_t stride, int log2_size)
{
    int i, j;
    int size          = (1 << log2_size);
    pixel *src        = (pixel *)_src;
    pixel4 a = PIXEL_SPLAT_X4(1 << (BIT_DEPTH - 1));

    for (i = 0; i < size; i++)
        for (j = 0; j < size; j+=4)
            AV_WN4P(&POS(j, i), a);
}
#else
static void FUNC(pred_dc0)(uint8_t *_src, ptrdiff_t stride, int log2_size)
{
    unsigned int i, j;
    const unsigned int size = (1 << log2_size);
    c_dst_ptr_t src = (c_dst_ptr_t)_src;
    const pixel a = (1 << (BIT_DEPTH - 1));

    for (i = 0; i < size; i++, src += stride)
    {
        for (j = 0; j < size; ++j)
        {
            src[j][0] = a;
            src[j][1] = a;
        }
    }
}
#endif

#define PRED_DC0(size)\
static void FUNC(pred_dc0_ ## size)(uint8_t *src, ptrdiff_t stride)   \
{                                                                               \
    FUNC(pred_dc0)(src, stride, size + 2);                        \
}

PRED_DC0(0)
PRED_DC0(1)
PRED_DC0(2)
PRED_DC0(3)

#undef PRED_DC0




#ifndef ANGLE_CONSTS
#define ANGLE_CONSTS
static const int intra_pred_angle[] = {
     32,  26,  21,  17, 13,  9,  5, 2, 0, -2, -5, -9, -13, -17, -21, -26, -32,
    -26, -21, -17, -13, -9, -5, -2, 0, 2,  5,  9, 13,  17,  21,  26,  32
};
static const int inv_angle[] = {
    -4096, -1638, -910, -630, -482, -390, -315, -256, -315, -390, -482,
    -630, -910, -1638, -4096
};
#endif

#if !PRED_C
static av_always_inline void FUNC(pred_angular)(uint8_t *_src,
                                                const uint8_t *_top,
                                                const uint8_t *_left,
                                                ptrdiff_t stride,
                                                int mode, int size)
{
    int x, y;
    pixel *src        = (pixel *)_src;
    const pixel *top  = (const pixel *)_top;
    const pixel *left = (const pixel *)_left;

    int angle = intra_pred_angle[mode - 2];
    pixel ref_array[3 * MAX_TB_SIZE + 4];
    pixel *ref_tmp = ref_array + size;
    const pixel *ref;
    int last = (size * angle) >> 5;

    if (mode >= 18) {
        ref = top - 1;

        if (angle < 0)
        {
            memcpy(ref_tmp + 1, top, size * PW);
            ref_tmp[0] = left[-1];

            for (x = last; x <= -1; x++)
                ref_tmp[x] = left[-1 + ((x * inv_angle[mode - 11] + 128) >> 8)];
            ref = ref_tmp;
        }

        for (y = 0; y < size; y++) {
            int idx  = ((y + 1) * angle) >> 5;
            int fact = ((y + 1) * angle) & 31;
            if (fact) {
                for (x = 0; x < size; x += 4) {
                    POS(x    , y) = ((32 - fact) * ref[x + idx + 1] +
                                           fact  * ref[x + idx + 2] + 16) >> 5;
                    POS(x + 1, y) = ((32 - fact) * ref[x + 1 + idx + 1] +
                                           fact  * ref[x + 1 + idx + 2] + 16) >> 5;
                    POS(x + 2, y) = ((32 - fact) * ref[x + 2 + idx + 1] +
                                           fact  * ref[x + 2 + idx + 2] + 16) >> 5;
                    POS(x + 3, y) = ((32 - fact) * ref[x + 3 + idx + 1] +
                                           fact  * ref[x + 3 + idx + 2] + 16) >> 5;
                }
            } else {
                for (x = 0; x < size; x += 4)
                    AV_WN4P(&POS(x, y), AV_RN4P(&ref[x + idx + 1]));
            }
        }
        if (mode == 26 && size < 32) {
            for (y = 0; y < size; y++)
                POS(0, y) = av_clip_pixel(top[0] + ((left[y] - left[-1]) >> 1));
        }

    } else {
        ref = left - 1;
        if (angle < 0 && last < -1) {
            for (x = 0; x <= size; x += 4)
                AV_WN4P(&ref_tmp[x], AV_RN4P(&left[x - 1]));
            // Inv angle <= -256 so top offset >= 0
            for (x = last; x <= -1; x++)
                ref_tmp[x] = top[-1 + ((x * inv_angle[mode - 11] + 128) >> 8)];
            ref = ref_tmp;
        }

        for (x = 0; x < size; x++) {
            int idx  = ((x + 1) * angle) >> 5;
            int fact = ((x + 1) * angle) & 31;
            if (fact) {
                for (y = 0; y < size; y++) {
                    POS(x, y) = ((32 - fact) * ref[y + idx + 1] +
                                       fact  * ref[y + idx + 2] + 16) >> 5;
                }
            } else {
                for (y = 0; y < size; y++)
                    POS(x, y) = ref[y + idx + 1];
            }
        }
        if (mode == 10 && size < 32) {
            for (x = 0; x < size; x += 4) {
                POS(x,     0) = av_clip_pixel(left[0] + ((top[x    ] - left[-1]) >> 1));
                POS(x + 1, 0) = av_clip_pixel(left[0] + ((top[x + 1] - left[-1]) >> 1));
                POS(x + 2, 0) = av_clip_pixel(left[0] + ((top[x + 2] - left[-1]) >> 1));
                POS(x + 3, 0) = av_clip_pixel(left[0] + ((top[x + 3] - left[-1]) >> 1));
            }
        }
    }
}
#else
static av_always_inline void FUNC(pred_angular)(uint8_t *_src,
                                                const uint8_t *_top,
                                                const uint8_t *_left,
                                                ptrdiff_t stride,
                                                int mode, int size)
{
    int x, y;
    c_dst_ptr_t src  = (c_dst_ptr_t)_src;
    c_src_ptr_t top  = (c_src_ptr_t)_top;
    c_src_ptr_t left = (c_src_ptr_t)_left;

    const int angle = intra_pred_angle[mode - 2];
    cpel ref_array[3 * MAX_TB_SIZE + 4][2];
    c_dst_ptr_t ref_tmp = ref_array + size;
    c_src_ptr_t ref;
    const int last = (size * angle) >> 5;

    if (mode >= 18) {
        ref = top - 1;
        if (angle < 0) {
            memcpy(ref_tmp + 1, top, size * 2 * PW);
            ref_tmp[0][0] = left[-1][0];
            ref_tmp[0][1] = left[-1][1];
            for (x = last; x <= -1; x++)
            {
                ref_tmp[x][0] = left[-1 + ((x * inv_angle[mode - 11] + 128) >> 8)][0];
                ref_tmp[x][1] = left[-1 + ((x * inv_angle[mode - 11] + 128) >> 8)][1];
            }
            ref = (c_src_ptr_t)ref_tmp;
        }

        for (y = 0; y < size; y++, src += stride) {
            const int idx  = ((y + 1) * angle) >> 5;
            const int fact = ((y + 1) * angle) & 31;
            if (fact) {
                for (x = 0; x < size; ++x) {
                    src[x][0] = ((32 - fact) * ref[x + idx + 1][0] +
                                       fact  * ref[x + idx + 2][0] + 16) >> 5;
                    src[x][1] = ((32 - fact) * ref[x + idx + 1][1] +
                                       fact  * ref[x + idx + 2][1] + 16) >> 5;
                }
            } else {
                memcpy(src, ref + idx + 1, size * 2 * PW);
            }
        }
    } else {
        ref = left - 1;
        if (angle < 0 && last < -1) {
            memcpy(ref_tmp, left - 1, (size + 1) * 2 * PW);
            for (x = last; x <= -1; x++)
            {
                ref_tmp[x][0] = top[-1 + ((x * inv_angle[mode - 11] + 128) >> 8)][0];
                ref_tmp[x][1] = top[-1 + ((x * inv_angle[mode - 11] + 128) >> 8)][1];
            }
            ref = (c_src_ptr_t)ref_tmp;
        }

        for (x = 0; x < size; x++, src++) {
            const int idx  = ((x + 1) * angle) >> 5;
            const int fact = ((x + 1) * angle) & 31;
            if (fact) {
                for (y = 0; y < size; y++) {
                    src[y * stride][0] = ((32 - fact) * ref[y + idx + 1][0] +
                                       fact  * ref[y + idx + 2][0] + 16) >> 5;
                    src[y * stride][1] = ((32 - fact) * ref[y + idx + 1][1] +
                                       fact  * ref[y + idx + 2][1] + 16) >> 5;
                }
            } else {
                for (y = 0; y < size; y++)
                {
                    src[y * stride][0] = ref[y + idx + 1][0];
                    src[y * stride][1] = ref[y + idx + 1][1];
                }
            }
        }
    }
}
#endif

static void FUNC(pred_angular_0)(uint8_t *src, const uint8_t *top,
                                 const uint8_t *left,
                                 ptrdiff_t stride, int mode)
{
    FUNC(pred_angular)(src, top, left, stride, mode, 1 << 2);
}

static void FUNC(pred_angular_1)(uint8_t *src, const uint8_t *top,
                                 const uint8_t *left,
                                 ptrdiff_t stride, int mode)
{
    FUNC(pred_angular)(src, top, left, stride, mode, 1 << 3);
}

static void FUNC(pred_angular_2)(uint8_t *src, const uint8_t *top,
                                 const uint8_t *left,
                                 ptrdiff_t stride, int mode)
{
    FUNC(pred_angular)(src, top, left, stride, mode, 1 << 4);
}

static void FUNC(pred_angular_3)(uint8_t *src, const uint8_t *top,
                                 const uint8_t *left,
                                 ptrdiff_t stride, int mode)
{
    FUNC(pred_angular)(src, top, left, stride, mode, 1 << 5);
}

#undef cpel
#undef c_src_ptr_t
#undef c_dst_ptr_t

#undef EXTEND
#undef POS
#undef PW

#undef filter_light1
#undef filter_light
#undef filter_strong
#undef ref_gen

#ifndef INCLUDED_ONCE
#define INCLUDED_ONCE
#endif

