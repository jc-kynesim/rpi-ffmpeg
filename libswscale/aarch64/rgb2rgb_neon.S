/*
 * Copyright (c) 2020 Martin Storsjo
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

#include "libavutil/aarch64/asm.S"

// void ff_interleave_bytes_neon(const uint8_t *src1, const uint8_t *src2,
//                               uint8_t *dest, int width, int height,
//                               int src1Stride, int src2Stride, int dstStride);
function ff_interleave_bytes_neon, export=1
        sub             w5,  w5,  w3
        sub             w6,  w6,  w3
        sub             w7,  w7,  w3, lsl #1
1:
        ands            w8,  w3,  #0xfffffff0 // & ~15
        b.eq            3f
2:
        ld1             {v0.16b}, [x0], #16
        ld1             {v1.16b}, [x1], #16
        subs            w8,  w8,  #16
        st2             {v0.16b, v1.16b}, [x2], #32
        b.gt            2b

        tst             w3,  #15
        b.eq            9f

3:
        tst             w3,  #8
        b.eq            4f
        ld1             {v0.8b}, [x0], #8
        ld1             {v1.8b}, [x1], #8
        st2             {v0.8b, v1.8b}, [x2], #16
4:
        tst             w3,  #4
        b.eq            5f

        ld1             {v0.s}[0], [x0], #4
        ld1             {v1.s}[0], [x1], #4
        zip1            v0.8b,   v0.8b,   v1.8b
        st1             {v0.8b}, [x2], #8

5:
        ands            w8,  w3,  #3
        b.eq            9f
6:
        ldrb            w9,  [x0], #1
        ldrb            w10, [x1], #1
        subs            w8,  w8,  #1
        bfi             w9,  w10, #8,  #8
        strh            w9,  [x2], #2
        b.gt            6b

9:
        subs            w4,  w4,  #1
        b.eq            0f
        add             x0,  x0,  w5, sxtw
        add             x1,  x1,  w6, sxtw
        add             x2,  x2,  w7, sxtw
        b               1b

0:
        ret
endfunc

// void ff_rgb24toyv12_aarch64(
//              const uint8_t *src,             // x0
//              uint8_t *ydst,                  // x1
//              uint8_t *udst,                  // x2
//              uint8_t *vdst,                  // x3
//              int width,                      // w4
//              int height,                     // w5
//              int lumStride,                  // w6
//              int chromStride,                // w7
//              int srcStr,                     // [sp, #0]
//              int32_t *rgb2yuv);              // [sp, #8]

function ff_rgb24toyv12_aarch64, export=1
        ldr             x15, [sp, #8]
        ld1             {v3.s}[2], [x15], #4
        ld1             {v3.s}[1], [x15], #4
        ld1             {v3.s}[0], [x15], #4
        ld1             {v4.s}[2], [x15], #4
        ld1             {v4.s}[1], [x15], #4
        ld1             {v4.s}[0], [x15], #4
        ld1             {v5.s}[2], [x15], #4
        ld1             {v5.s}[1], [x15], #4
        ld1             {v5.s}[0], [x15]
        b               99f
endfunc

// void ff_bgr24toyv12_aarch64(
//              const uint8_t *src,             // x0
//              uint8_t *ydst,                  // x1
//              uint8_t *udst,                  // x2
//              uint8_t *vdst,                  // x3
//              int width,                      // w4
//              int height,                     // w5
//              int lumStride,                  // w6
//              int chromStride,                // w7
//              int srcStr,                     // [sp, #0]
//              int32_t *rgb2yuv);              // [sp, #8]

function ff_bgr24toyv12_aarch64, export=1
        ldr             x15, [sp, #8]
        ld3             {v3.s, v4.s, v5.s}[0], [x15], #12
        ld3             {v3.s, v4.s, v5.s}[1], [x15], #12
        ld3             {v3.s, v4.s, v5.s}[2], [x15]
99:
        ldr             w14, [sp, #0]
        movi            v18.8b, #128
        uxtl            v17.8h, v18.8b

        // Even line - YUV
1:
        mov             x10, x0
        mov             x11, x1
        mov             x12, x2
        mov             x13, x3
        mov             w9,  w4

0:
        ld3             {v0.16b, v1.16b, v2.16b}, [x10], #48

        uxtl2           v20.8h, v0.16b
        uxtl2           v21.8h, v1.16b
        uxtl2           v22.8h, v2.16b

        uxtl            v0.8h, v0.8b
        uxtl            v1.8h, v1.8b
        uxtl            v2.8h, v2.8b
        // Y0
        smull           v6.4s, v0.4h, v3.h[0]
        smull2          v7.4s, v0.8h, v3.h[0]
        smlal           v6.4s, v1.4h, v4.h[0]
        smlal2          v7.4s, v1.8h, v4.h[0]
        smlal           v6.4s, v2.4h, v5.h[0]
        smlal2          v7.4s, v2.8h, v5.h[0]
        shrn            v6.4h, v6.4s, #12
        shrn2           v6.8h, v7.4s, #12
        add             v6.8h, v6.8h, v17.8h     // +128 (>> 3 = 16)
        uqrshrn         v16.8b, v6.8h, #3
        // Y1
        smull           v6.4s, v20.4h, v3.h[0]
        smull2          v7.4s, v20.8h, v3.h[0]
        smlal           v6.4s, v21.4h, v4.h[0]
        smlal2          v7.4s, v21.8h, v4.h[0]
        smlal           v6.4s, v22.4h, v5.h[0]
        smlal2          v7.4s, v22.8h, v5.h[0]
        shrn            v6.4h, v6.4s, #12
        shrn2           v6.8h, v7.4s, #12
        add             v6.8h, v6.8h, v17.8h
        uqrshrn2        v16.16b, v6.8h, #3
        // Y0/Y1
        st1             {v16.16b}, [x11], #16

        uzp1            v0.8h, v0.8h, v20.8h
        uzp1            v1.8h, v1.8h, v21.8h
        uzp1            v2.8h, v2.8h, v22.8h

        // U
        // Vector subscript *2 as we loaded into S but are only using H
        smull           v6.4s, v0.4h, v3.h[2]
        smull2          v7.4s, v0.8h, v3.h[2]
        smlal           v6.4s, v1.4h, v4.h[2]
        smlal2          v7.4s, v1.8h, v4.h[2]
        smlal           v6.4s, v2.4h, v5.h[2]
        smlal2          v7.4s, v2.8h, v5.h[2]
        shrn            v6.4h, v6.4s, #14
        shrn2           v6.8h, v7.4s, #14
        sqrshrn         v6.8b, v6.8h, #1
        add             v6.8b, v6.8b, v18.8b     // +128
        st1             {v6.8b}, [x12], #8

        // V
        smull           v6.4s, v0.4h, v3.h[4]
        smull2          v7.4s, v0.8h, v3.h[4]
        smlal           v6.4s, v1.4h, v4.h[4]
        smlal2          v7.4s, v1.8h, v4.h[4]
        smlal           v6.4s, v2.4h, v5.h[4]
        smlal2          v7.4s, v2.8h, v5.h[4]
        shrn            v6.4h, v6.4s, #14
        shrn2           v6.8h, v7.4s, #14
        sqrshrn         v6.8b, v6.8h, #1
        add             v6.8b, v6.8b, v18.8b     // +128
        st1             {v6.8b}, [x13], #8

        subs            w9, w9, #16
        b.gt            0b

        // Odd line - Y only

        add             x0, x0, w14, SXTX
        add             x1, x1, w6, SXTX
        mov             x10, x0
        mov             x11, x1
        mov             w9,  w4

0:
        ld3             {v0.16b, v1.16b, v2.16b}, [x10], #48

        uxtl2           v20.8h, v0.16b
        uxtl2           v21.8h, v1.16b
        uxtl2           v22.8h, v2.16b

        uxtl            v0.8h, v0.8b
        uxtl            v1.8h, v1.8b
        uxtl            v2.8h, v2.8b
        // Y0
        smull           v6.4s, v0.4h, v3.h[0]
        smull2          v7.4s, v0.8h, v3.h[0]
        smlal           v6.4s, v1.4h, v4.h[0]
        smlal2          v7.4s, v1.8h, v4.h[0]
        smlal           v6.4s, v2.4h, v5.h[0]
        smlal2          v7.4s, v2.8h, v5.h[0]
        shrn            v6.4h, v6.4s, #12
        shrn2           v6.8h, v7.4s, #12
        add             v6.8h, v6.8h, v17.8h
        uqrshrn         v16.8b, v6.8h, #3
        // Y1
        smull           v6.4s, v20.4h, v3.h[0]
        smull2          v7.4s, v20.8h, v3.h[0]
        smlal           v6.4s, v21.4h, v4.h[0]
        smlal2          v7.4s, v21.8h, v4.h[0]
        smlal           v6.4s, v22.4h, v5.h[0]
        smlal2          v7.4s, v22.8h, v5.h[0]
        shrn            v6.4h, v6.4s, #12
        shrn2           v6.8h, v7.4s, #12
        add             v6.8h, v6.8h, v17.8h
        uqrshrn2        v16.16b, v6.8h, #3
        // Y0/Y1
        st1             {v16.16b}, [x11], #16

        subs            w9, w9, #16
        b.gt            0b

        add             x0, x0, w14, SXTX
        add             x1, x1, w6, SXTX
        add             x2, x2, w7, SXTX
        add             x3, x3, w7, SXTX
        subs            w5, w5, #2
        b.gt            1b

        ret
endfunc
