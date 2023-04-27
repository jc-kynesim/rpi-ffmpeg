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

#include <stdint.h>

#include "config.h"
#include "libavutil/attributes.h"
#include "libavutil/aarch64/cpu.h"
#include "libavutil/cpu.h"
#include "libavutil/bswap.h"
#include "libswscale/rgb2rgb.h"
#include "libswscale/swscale.h"
#include "libswscale/swscale_internal.h"

void ff_interleave_bytes_neon(const uint8_t *src1, const uint8_t *src2,
                              uint8_t *dest, int width, int height,
                              int src1Stride, int src2Stride, int dstStride);
void ff_bgr24toyv12_aarch64(const uint8_t *src, uint8_t *ydst, uint8_t *udst,
                   uint8_t *vdst, int width, int height, int lumStride,
                   int chromStride, int srcStride, int32_t *rgb2yuv);
void ff_rgb24toyv12_aarch64(const uint8_t *src, uint8_t *ydst, uint8_t *udst,
                   uint8_t *vdst, int width, int height, int lumStride,
                   int chromStride, int srcStride, int32_t *rgb2yuv);

// RGB to YUV asm fns process 16 pixels at once so ensure that the output
// will fit into the stride. ARM64 should cope with unaligned SIMD r/w so
// don't test for that
// Fall back to C if we cannot use asm

static inline int chkw(const int width, const int lumStride, const int chromStride)
{
//    const int aw = FFALIGN(width, 16);
//    return aw <= FFABS(lumStride) && aw <= FFABS(chromStride) * 2;
    return 1;
}

static void rgb24toyv12_check(const uint8_t *src, uint8_t *ydst, uint8_t *udst,
                   uint8_t *vdst, int width, int height, int lumStride,
                   int chromStride, int srcStride, int32_t *rgb2yuv)
{
    if (chkw(width, lumStride, chromStride))
        ff_rgb24toyv12_aarch64(src, ydst, udst, vdst, width, height, lumStride, chromStride, srcStride, rgb2yuv);
    else
        ff_rgb24toyv12_c(src, ydst, udst, vdst, width, height, lumStride, chromStride, srcStride, rgb2yuv);
}

static void bgr24toyv12_check(const uint8_t *src, uint8_t *ydst, uint8_t *udst,
                   uint8_t *vdst, int width, int height, int lumStride,
                   int chromStride, int srcStride, int32_t *bgr2yuv)
{
    if (chkw(width, lumStride, chromStride))
        ff_bgr24toyv12_aarch64(src, ydst, udst, vdst, width, height, lumStride, chromStride, srcStride, bgr2yuv);
    else
        ff_bgr24toyv12_c(src, ydst, udst, vdst, width, height, lumStride, chromStride, srcStride, bgr2yuv);
}


av_cold void rgb2rgb_init_aarch64(void)
{
    int cpu_flags = av_get_cpu_flags();

    if (have_neon(cpu_flags)) {
        interleaveBytes = ff_interleave_bytes_neon;
        ff_rgb24toyv12 = rgb24toyv12_check;
        ff_bgr24toyv12 = bgr24toyv12_check;
    }
}
