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

#ifndef AVCODEC_ARM_RPI_HEVC_MISC_H
#define AVCODEC_ARM_RPI_HEVC_MISC_H

#include "config.h"
#if HAVE_NEON_INLINE && !CONFIG_THUMB

static av_noinline void ff_hevc_rpi_copy_vert_v2h_neon(uint8_t *dst, const uint8_t *src,
                                                       int pixel_shift, int height,
                                                       ptrdiff_t stride_src)
{
    const uint8_t *src2 = src + stride_src;
    stride_src <<= 1;
    switch (pixel_shift)
    {
        case 2:
            __asm__ volatile (
                "vld1.32     {d0[0]}, [%[src]], %[stride_src]  \n\t"
                "vld1.32     {d0[1]}, [%[src2]], %[stride_src] \n\t"
                "vld1.32     {d1[0]}, [%[src]], %[stride_src]  \n\t"
                "subs        %[height], #4                     \n\t"
                "vld1.32     {d1[1]}, [%[src2]], %[stride_src] \n\t"
                "beq         2f                                \n\t"
                "1:                                            \n\t"
                "vld1.32     {d2[0]}, [%[src]], %[stride_src]  \n\t"
                "vld1.32     {d2[1]}, [%[src2]], %[stride_src] \n\t"
                "vld1.32     {d3[0]}, [%[src]], %[stride_src]  \n\t"
                "vld1.32     {d3[1]}, [%[src2]], %[stride_src] \n\t"
                "subs        %[height], #4                     \n\t"
                "vst1.32     {q0}, [%[dst]]!                   \n\t"
                "beq         3f                                \n\t"
                "vld1.32     {d0[0]}, [%[src]], %[stride_src]  \n\t"
                "vld1.32     {d0[1]}, [%[src2]], %[stride_src] \n\t"
                "vld1.32     {d1[0]}, [%[src]], %[stride_src]  \n\t"
                "vld1.32     {d1[1]}, [%[src2]], %[stride_src] \n\t"
                "subs        %[height], #4                     \n\t"
                "vst1.32     {q1}, [%[dst]]!                   \n\t"
                "bne         1b                                \n\t"
                "2:                                            \n\t"
                "vst1.32     {q0}, [%[dst]]                    \n\t"
                "b           4f                                \n\t"
                "3:                                            \n\t"
                "vst1.32     {q1}, [%[dst]]                    \n\t"
                "4:                                            \n\t"
                :  // Outputs
                           [src]"+r"(src),
                          [src2]"+r"(src2),
                           [dst]"+r"(dst),
                        [height]"+r"(height)
                :  // Inputs
                    [stride_src]"r"(stride_src)
                :  // Clobbers
                    "cc", "memory"
            );
            break;
        case 1:
            __asm__ volatile (
                "vld1.16     {d0[0]}, [%[src]], %[stride_src]  \n\t"
                "vld1.16     {d1[0]}, [%[src2]], %[stride_src] \n\t"
                "vld1.16     {d0[1]}, [%[src]], %[stride_src]  \n\t"
                "subs        %[height], #4                     \n\t"
                "vld1.16     {d1[1]}, [%[src2]], %[stride_src] \n\t"
                "beq         2f                                \n\t"
                "1:                                            \n\t"
                "vld1.16     {d2[0]}, [%[src]], %[stride_src]  \n\t"
                "vld1.16     {d3[0]}, [%[src2]], %[stride_src] \n\t"
                "vld1.16     {d2[1]}, [%[src]], %[stride_src]  \n\t"
                "vld1.16     {d3[1]}, [%[src2]], %[stride_src] \n\t"
                "vzip.16     d0, d1                            \n\t"
                "subs        %[height], #4                     \n\t"
                "vst1.16     {d0}, [%[dst]]!                   \n\t"
                "beq         3f                                \n\t"
                "vld1.16     {d0[0]}, [%[src]], %[stride_src]  \n\t"
                "vld1.16     {d1[0]}, [%[src2]], %[stride_src] \n\t"
                "vld1.16     {d0[1]}, [%[src]], %[stride_src]  \n\t"
                "vld1.16     {d1[1]}, [%[src2]], %[stride_src] \n\t"
                "vzip.16     d2, d3                            \n\t"
                "subs        %[height], #4                     \n\t"
                "vst1.16     {d2}, [%[dst]]!                   \n\t"
                "bne         1b                                \n\t"
                "2:                                            \n\t"
                "vzip.16     d0, d1                            \n\t"
                "vst1.16     {d0}, [%[dst]]                    \n\t"
                "b           4f                                \n\t"
                "3:                                            \n\t"
                "vzip.16     d2, d3                            \n\t"
                "vst1.16     {d2}, [%[dst]]                    \n\t"
                "4:                                            \n\t"
                :  // Outputs
                           [src]"+r"(src),
                          [src2]"+r"(src2),
                           [dst]"+r"(dst),
                        [height]"+r"(height)
                :  // Inputs
                    [stride_src]"r"(stride_src)
                :  // Clobbers
                    "cc", "memory"
            );
            break;
        default:
            __asm__ volatile (
                "vld1.8      {d0[0]}, [%[src]], %[stride_src]  \n\t"
                "vld1.8      {d1[0]}, [%[src2]], %[stride_src] \n\t"
                "vld1.8      {d0[1]}, [%[src]], %[stride_src]  \n\t"
                "vld1.8      {d1[1]}, [%[src2]], %[stride_src] \n\t"
                "vld1.8      {d0[2]}, [%[src]], %[stride_src]  \n\t"
                "vld1.8      {d1[2]}, [%[src2]], %[stride_src] \n\t"
                "vld1.8      {d0[3]}, [%[src]], %[stride_src]  \n\t"
                "subs        %[height], #8                     \n\t"
                "vld1.8      {d1[3]}, [%[src2]], %[stride_src] \n\t"
                "beq         2f                                \n\t"
                "1:                                            \n\t"
                "vld1.8      {d2[0]}, [%[src]], %[stride_src]  \n\t"
                "vld1.8      {d3[0]}, [%[src2]], %[stride_src] \n\t"
                "vld1.8      {d2[1]}, [%[src]], %[stride_src]  \n\t"
                "vld1.8      {d3[1]}, [%[src2]], %[stride_src] \n\t"
                "vld1.8      {d2[2]}, [%[src]], %[stride_src]  \n\t"
                "vld1.8      {d3[2]}, [%[src2]], %[stride_src] \n\t"
                "vld1.8      {d2[3]}, [%[src]], %[stride_src]  \n\t"
                "vld1.8      {d3[3]}, [%[src2]], %[stride_src] \n\t"
                "vzip.8      d0, d1                            \n\t"
                "subs        %[height], #8                     \n\t"
                "vst1.8      {d0}, [%[dst]]!                   \n\t"
                "beq         3f                                \n\t"
                "vld1.8      {d0[0]}, [%[src]], %[stride_src]  \n\t"
                "vld1.8      {d1[0]}, [%[src2]], %[stride_src] \n\t"
                "vld1.8      {d0[1]}, [%[src]], %[stride_src]  \n\t"
                "vld1.8      {d1[1]}, [%[src2]], %[stride_src] \n\t"
                "vld1.8      {d0[2]}, [%[src]], %[stride_src]  \n\t"
                "vld1.8      {d1[2]}, [%[src2]], %[stride_src] \n\t"
                "vld1.8      {d0[3]}, [%[src]], %[stride_src]  \n\t"
                "vld1.8      {d1[3]}, [%[src2]], %[stride_src] \n\t"
                "vzip.8      d2, d3                            \n\t"
                "subs        %[height], #8                     \n\t"
                "vst1.8      {d2}, [%[dst]]!                   \n\t"
                "bne         1b                                \n\t"
                "2:                                            \n\t"
                "vzip.8      d0, d1                            \n\t"
                "vst1.8      {d0}, [%[dst]]                    \n\t"
                "b           4f                                \n\t"
                "3:                                            \n\t"
                "vzip.8      d2, d3                            \n\t"
                "vst1.8      {d2}, [%[dst]]                    \n\t"
                "4:                                            \n\t"
                :  // Outputs
                           [src]"+r"(src),
                          [src2]"+r"(src2),
                           [dst]"+r"(dst),
                        [height]"+r"(height)
                :  // Inputs
                    [stride_src]"r"(stride_src)
                :  // Clobbers
                    "cc", "memory"
            );
            break;
    }
}

static av_noinline void ff_hevc_rpi_copy_vert_h2v_neon(uint8_t *dst, const uint8_t *src,
                                                       int pixel_shift, int height,
                                                      ptrdiff_t stride_dst)
{
    uint8_t *dst2 = dst + stride_dst;
    stride_dst <<= 1;
    switch (pixel_shift)
    {
        case 2:
            __asm__ volatile (
                "subs        %[height], #4                     \n\t"
                "vld1.32     {q0}, [%[src]]!                   \n\t"
                "beq         2f                                \n\t"
                "1:                                            \n\t"
                "vld1.32     {q1}, [%[src]]!                   \n\t"
                "vst1.32     {d0[0]}, [%[dst]], %[stride_dst]  \n\t"
                "vst1.32     {d0[1]}, [%[dst2]], %[stride_dst] \n\t"
                "vst1.32     {d1[0]}, [%[dst]], %[stride_dst]  \n\t"
                "subs        %[height], #4                     \n\t"
                "vst1.32     {d1[1]}, [%[dst2]], %[stride_dst] \n\t"
                "beq         3f                                \n\t"
                "vld1.32     {q0}, [%[src]]!                   \n\t"
                "vst1.32     {d2[0]}, [%[dst]], %[stride_dst]  \n\t"
                "vst1.32     {d2[1]}, [%[dst2]], %[stride_dst] \n\t"
                "vst1.32     {d3[0]}, [%[dst]], %[stride_dst]  \n\t"
                "subs        %[height], #4                     \n\t"
                "vst1.32     {d3[1]}, [%[dst2]], %[stride_dst] \n\t"
                "bne         1b                                \n\t"
                "2:                                            \n\t"
                "vst1.32     {d0[0]}, [%[dst]], %[stride_dst]  \n\t"
                "vst1.32     {d0[1]}, [%[dst2]], %[stride_dst] \n\t"
                "vst1.32     {d1[0]}, [%[dst]]                 \n\t"
                "vst1.32     {d1[1]}, [%[dst2]]                \n\t"
                "b           4f                                \n\t"
                "3:                                            \n\t"
                "vst1.32     {d2[0]}, [%[dst]], %[stride_dst]  \n\t"
                "vst1.32     {d2[1]}, [%[dst2]], %[stride_dst] \n\t"
                "vst1.32     {d3[0]}, [%[dst]]                 \n\t"
                "vst1.32     {d3[1]}, [%[dst2]]                \n\t"
                "4:                                            \n\t"
                :  // Outputs
                           [dst]"+r"(dst),
                          [dst2]"+r"(dst2),
                           [src]"+r"(src),
                        [height]"+r"(height)
                :  // Inputs
                    [stride_dst]"r"(stride_dst)
                :  // Clobbers
                    "cc", "memory"
            );
            break;
        case 1:
            __asm__ volatile (
                "subs        %[height], #4                     \n\t"
                "vld1.16     {d0}, [%[src]]!                   \n\t"
                "beq         2f                                \n\t"
                "1:                                            \n\t"
                "vld1.16     {d2}, [%[src]]!                   \n\t"
                "vst1.16     {d0[0]}, [%[dst]], %[stride_dst]  \n\t"
                "vst1.16     {d0[1]}, [%[dst2]], %[stride_dst] \n\t"
                "vst1.16     {d0[2]}, [%[dst]], %[stride_dst]  \n\t"
                "subs        %[height], #4                     \n\t"
                "vst1.16     {d0[3]}, [%[dst2]], %[stride_dst] \n\t"
                "beq         3f                                \n\t"
                "vld1.16     {d0}, [%[src]]!                   \n\t"
                "vst1.16     {d2[0]}, [%[dst]], %[stride_dst]  \n\t"
                "vst1.16     {d2[1]}, [%[dst2]], %[stride_dst] \n\t"
                "vst1.16     {d2[2]}, [%[dst]], %[stride_dst]  \n\t"
                "subs        %[height], #4                     \n\t"
                "vst1.16     {d2[3]}, [%[dst2]], %[stride_dst] \n\t"
                "bne         1b                                \n\t"
                "2:                                            \n\t"
                "vst1.16     {d0[0]}, [%[dst]], %[stride_dst]  \n\t"
                "vst1.16     {d0[1]}, [%[dst2]], %[stride_dst] \n\t"
                "vst1.16     {d0[2]}, [%[dst]]                 \n\t"
                "vst1.16     {d0[3]}, [%[dst2]]                \n\t"
                "b           4f                                \n\t"
                "3:                                            \n\t"
                "vst1.16     {d2[0]}, [%[dst]], %[stride_dst]  \n\t"
                "vst1.16     {d2[1]}, [%[dst2]], %[stride_dst] \n\t"
                "vst1.16     {d2[2]}, [%[dst]]                 \n\t"
                "vst1.16     {d2[3]}, [%[dst2]]                \n\t"
                "4:                                            \n\t"
                :  // Outputs
                           [dst]"+r"(dst),
                          [dst2]"+r"(dst2),
                           [src]"+r"(src),
                        [height]"+r"(height)
                :  // Inputs
                    [stride_dst]"r"(stride_dst)
                :  // Clobbers
                    "cc", "memory"
            );
            break;
        default:
            __asm__ volatile (
                "subs        %[height], #8                     \n\t"
                "vld1.8      {d0}, [%[src]]!                   \n\t"
                "beq         2f                                \n\t"
                "1:                                            \n\t"
                "vld1.8      {d2}, [%[src]]!                   \n\t"
                "vst1.8      {d0[0]}, [%[dst]], %[stride_dst]  \n\t"
                "vst1.8      {d0[1]}, [%[dst2]], %[stride_dst] \n\t"
                "vst1.8      {d0[2]}, [%[dst]], %[stride_dst]  \n\t"
                "vst1.8      {d0[3]}, [%[dst2]], %[stride_dst] \n\t"
                "vst1.8      {d0[4]}, [%[dst]], %[stride_dst]  \n\t"
                "vst1.8      {d0[5]}, [%[dst2]], %[stride_dst] \n\t"
                "vst1.8      {d0[6]}, [%[dst]], %[stride_dst]  \n\t"
                "subs        %[height], #8                     \n\t"
                "vst1.8      {d0[7]}, [%[dst2]], %[stride_dst] \n\t"
                "beq         3f                                \n\t"
                "vld1.8      {d0}, [%[src]]!                   \n\t"
                "vst1.8      {d2[0]}, [%[dst]], %[stride_dst]  \n\t"
                "vst1.8      {d2[1]}, [%[dst2]], %[stride_dst] \n\t"
                "vst1.8      {d2[2]}, [%[dst]], %[stride_dst]  \n\t"
                "vst1.8      {d2[3]}, [%[dst2]], %[stride_dst] \n\t"
                "vst1.8      {d2[4]}, [%[dst]], %[stride_dst]  \n\t"
                "vst1.8      {d2[5]}, [%[dst2]], %[stride_dst] \n\t"
                "vst1.8      {d2[6]}, [%[dst]], %[stride_dst]  \n\t"
                "subs        %[height], #8                     \n\t"
                "vst1.8      {d2[7]}, [%[dst2]], %[stride_dst] \n\t"
                "bne         1b                                \n\t"
                "2:                                            \n\t"
                "vst1.8      {d0[0]}, [%[dst]], %[stride_dst]  \n\t"
                "vst1.8      {d0[1]}, [%[dst2]], %[stride_dst] \n\t"
                "vst1.8      {d0[2]}, [%[dst]], %[stride_dst]  \n\t"
                "vst1.8      {d0[3]}, [%[dst2]], %[stride_dst] \n\t"
                "vst1.8      {d0[4]}, [%[dst]], %[stride_dst]  \n\t"
                "vst1.8      {d0[5]}, [%[dst2]], %[stride_dst] \n\t"
                "vst1.8      {d0[6]}, [%[dst]]                 \n\t"
                "vst1.8      {d0[7]}, [%[dst2]]                \n\t"
                "b           4f                                \n\t"
                "3:                                            \n\t"
                "vst1.8      {d2[0]}, [%[dst]], %[stride_dst]  \n\t"
                "vst1.8      {d2[1]}, [%[dst2]], %[stride_dst] \n\t"
                "vst1.8      {d2[2]}, [%[dst]], %[stride_dst]  \n\t"
                "vst1.8      {d2[3]}, [%[dst2]], %[stride_dst] \n\t"
                "vst1.8      {d2[4]}, [%[dst]], %[stride_dst]  \n\t"
                "vst1.8      {d2[5]}, [%[dst2]], %[stride_dst] \n\t"
                "vst1.8      {d2[6]}, [%[dst]]                 \n\t"
                "vst1.8      {d2[7]}, [%[dst2]]                \n\t"
                "4:                                            \n\t"
                :  // Outputs
                           [dst]"+r"(dst),
                          [dst2]"+r"(dst2),
                           [src]"+r"(src),
                        [height]"+r"(height)
                :  // Inputs
                    [stride_dst]"r"(stride_dst)
                :  // Clobbers
                    "cc", "memory"
            );
            break;
    }
}

static av_noinline void ff_hevc_rpi_copy_vert_v2v_neon(uint8_t *dst, const uint8_t *src,
                                                       int pixel_shift, int height,
                                                       ptrdiff_t stride_dst, ptrdiff_t stride_src)
{
    int x, y;
    switch (pixel_shift)
    {
        case 2:
            __asm__ volatile (
                "ldr         %[x], [%[src]], %[stride_src] \n\t"
                "ldr         %[y], [%[src]], %[stride_src] \n\t"
                "str         %[x], [%[dst]], %[stride_dst] \n\t"
                "sub         %[height], #2                 \n\t"
                "1:                                        \n\t"
                "ldr         %[x], [%[src]], %[stride_src] \n\t"
                "str         %[y], [%[dst]], %[stride_dst] \n\t"
                "ldr         %[y], [%[src]], %[stride_src] \n\t"
                "subs        %[height], #2                 \n\t"
                "str         %[x], [%[dst]], %[stride_dst] \n\t"
                "bne         1b                            \n\t"
                "str         %[y], [%[dst]]                \n\t"
                :  // Outputs
                             [x]"=&r"(x),
                             [y]"=&r"(y),
                           [src]"+r"(src),
                           [dst]"+r"(dst),
                        [height]"+r"(height)
                :  // Inputs
                    [stride_src]"r"(stride_src),
                    [stride_dst]"r"(stride_dst)
                :  // Clobbers
                    "cc", "memory"
            );
            break;
        case 1:
            __asm__ volatile (
                "ldrh        %[x], [%[src]], %[stride_src] \n\t"
                "ldrh        %[y], [%[src]], %[stride_src] \n\t"
                "strh        %[x], [%[dst]], %[stride_dst] \n\t"
                "sub         %[height], #2                 \n\t"
                "1:                                        \n\t"
                "ldrh        %[x], [%[src]], %[stride_src] \n\t"
                "strh        %[y], [%[dst]], %[stride_dst] \n\t"
                "ldrh        %[y], [%[src]], %[stride_src] \n\t"
                "subs        %[height], #2                 \n\t"
                "strh        %[x], [%[dst]], %[stride_dst] \n\t"
                "bne         1b                            \n\t"
                "strh        %[y], [%[dst]]                \n\t"
                :  // Outputs
                             [x]"=&r"(x),
                             [y]"=&r"(y),
                           [src]"+r"(src),
                           [dst]"+r"(dst),
                        [height]"+r"(height)
                :  // Inputs
                    [stride_src]"r"(stride_src),
                    [stride_dst]"r"(stride_dst)
                :  // Clobbers
                    "cc", "memory"
            );
            break;
        default:
            __asm__ volatile (
                "ldrb        %[x], [%[src]], %[stride_src] \n\t"
                "ldrb        %[y], [%[src]], %[stride_src] \n\t"
                "strb        %[x], [%[dst]], %[stride_dst] \n\t"
                "sub         %[height], #2                 \n\t"
                "1:                                        \n\t"
                "ldrb        %[x], [%[src]], %[stride_src] \n\t"
                "strb        %[y], [%[dst]], %[stride_dst] \n\t"
                "ldrb        %[y], [%[src]], %[stride_src] \n\t"
                "subs        %[height], #2                 \n\t"
                "strb        %[x], [%[dst]], %[stride_dst] \n\t"
                "bne         1b                            \n\t"
                "strb        %[y], [%[dst]]                \n\t"
                :  // Outputs
                             [x]"=&r"(x),
                             [y]"=&r"(y),
                           [src]"+r"(src),
                           [dst]"+r"(dst),
                        [height]"+r"(height)
                :  // Inputs
                    [stride_src]"r"(stride_src),
                    [stride_dst]"r"(stride_dst)
                :  // Clobbers
                    "cc", "memory"
            );
            break;
    }
}

#define ff_hevc_rpi_copy_vert ff_hevc_rpi_copy_vert_neon
static inline void ff_hevc_rpi_copy_vert_neon(uint8_t *dst, const uint8_t *src,
                                              int pixel_shift, int height,
                                              ptrdiff_t stride_dst, ptrdiff_t stride_src)
{
    if (stride_dst == 1 << pixel_shift)
        ff_hevc_rpi_copy_vert_v2h_neon(dst, src, pixel_shift, height, stride_src);
    else if (stride_src == 1 << pixel_shift)
        ff_hevc_rpi_copy_vert_h2v_neon(dst, src, pixel_shift, height, stride_dst);
    else
        ff_hevc_rpi_copy_vert_v2v_neon(dst, src, pixel_shift, height, stride_dst, stride_src);
}

#endif /* HAVE_NEON_INLINE */

#endif /* AVCODEC_ARM_RPI_HEVC_MISC_H */
