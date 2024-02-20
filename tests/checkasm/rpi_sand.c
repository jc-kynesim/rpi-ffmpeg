/*
 * Copyright (c) 2023 John Cox
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with FFmpeg; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <string.h>
#include "checkasm.h"
#include "libavutil/common.h"
#include "libavutil/rpi_sand_fns.h"

#if ARCH_ARM
#include "libavutil/arm/cpu.h"
#include "libavutil/arm/rpi_sand_neon.h"
#elif ARCH_AARCH64
#include "libavutil/aarch64/cpu.h"
#include "libavutil/aarch64/rpi_sand_neon.h"
#else
#define have_neon(flags) 0
#define ff_rpi_sand30_lines_to_planar_y16 NULL
#define ff_rpi_sand30_lines_to_planar_c16 NULL
#endif

static inline uint32_t pack30(unsigned int a, unsigned int b, unsigned int c)
{
    return (a & 0x3ff) | ((b & 0x3ff) << 10) | ((c & 0x3ff) << 20);
}

void checkasm_check_rpi_sand(void)
{
    const unsigned int w = 1280;
    const unsigned int h = 66;
    const unsigned int stride1 = 128;
    const unsigned int stride2 = h*3/2;
    const unsigned int ssize = ((w+95)/96)*128*h*3/2;
    const unsigned int ysize = ((w + 32) * (h + 32) * 2);

    uint8_t * sbuf0 = malloc(ssize);
    uint8_t * sbuf1 = malloc(ssize);
    uint8_t * ybuf0 = malloc(ysize);
    uint8_t * ybuf1 = malloc(ysize);
    uint8_t * vbuf0 = malloc(ysize);
    uint8_t * vbuf1 = malloc(ysize);
    uint8_t * yframe0 = (w + 32) * 16 + ybuf0;
    uint8_t * yframe1 = (w + 32) * 16 + ybuf1;
    uint8_t * vframe0 = (w + 32) * 16 + vbuf0;
    uint8_t * vframe1 = (w + 32) * 16 + vbuf1;
    unsigned int i;

    for (i = 0; i != ssize; i += 4)
        *(uint32_t*)(sbuf0 + i) = rnd();
    memcpy(sbuf1, sbuf0, ssize);

    if (check_func(have_neon(av_get_cpu_flags()) ? ff_rpi_sand30_lines_to_planar_y16 : av_rpi_sand30_to_planar_y16, "rpi_sand30_to_planar_y16")) {
        declare_func(void, uint8_t * dst, const unsigned int dst_stride,
                     const uint8_t * src,
                     unsigned int stride1, unsigned int stride2,
                     unsigned int _x, unsigned int y,
                     unsigned int _w, unsigned int h);

        memset(ybuf0, 0xbb, ysize);
        memset(ybuf1, 0xbb, ysize);

        call_ref(yframe0, (w + 32) * 2, sbuf0, stride1, stride2, 0, 0, w, h);
        call_new(yframe1, (w + 32) * 2, sbuf1, stride1, stride2, 0, 0, w, h);

        if (memcmp(sbuf0, sbuf1, ssize)
            || memcmp(ybuf0, ybuf1, ysize))
            fail();

        bench_new(ybuf1, (w + 32) * 2, sbuf1, stride1, stride2, 0, 0, w, h);
    }

    if (check_func(have_neon(av_get_cpu_flags()) ? ff_rpi_sand30_lines_to_planar_c16 : av_rpi_sand30_to_planar_c16, "rpi_sand30_to_planar_c16")) {
        declare_func(void, uint8_t * u_dst, const unsigned int u_stride,
                     uint8_t * v_dst, const unsigned int v_stride,
                     const uint8_t * src,
                     unsigned int stride1, unsigned int stride2,
                     unsigned int _x, unsigned int y,
                     unsigned int _w, unsigned int h);

        memset(ybuf0, 0xbb, ysize);
        memset(ybuf1, 0xbb, ysize);
        memset(vbuf0, 0xbb, ysize);
        memset(vbuf1, 0xbb, ysize);

        call_ref(yframe0, (w + 32), vframe0, (w + 32), sbuf0, stride1, stride2, 0, 0, w/2, h/2);
        call_new(yframe1, (w + 32), vframe1, (w + 32), sbuf1, stride1, stride2, 0, 0, w/2, h/2);

        if (memcmp(sbuf0, sbuf1, ssize)
            || memcmp(ybuf0, ybuf1, ysize)
            || memcmp(vbuf0, vbuf1, ysize))
            fail();

        bench_new(yframe1, (w + 32), vframe1, (w + 32), sbuf1, stride1, stride2, 0, 0, w/2, h/2);
    }


    report("sand30");

    free(sbuf0);
    free(sbuf1);
    free(ybuf0);
    free(ybuf1);
    free(vbuf0);
    free(vbuf1);
}

