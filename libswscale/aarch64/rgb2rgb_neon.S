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

// regs
// v0-2         Src bytes - reused as chroma src
// v3-5         Coeffs (packed very inefficiently - could be squashed)
// v6           128b
// v7           128h
// v8-15        Reserved
// v16-18       Lo Src expanded as H
// v19          -
// v20-22       Hi Src expanded as H
// v23          -
// v24          U out
// v25          U tmp
// v26          Y out
// v27-29       Y tmp
// v30          V out
// v31          V tmp

// Assumes Little Endian in tail stores & conversion matrix

function ff_bgr24toyv12_aarch64, export=1
        ldr             x15, [sp, #8]
        ld3             {v3.s, v4.s, v5.s}[0], [x15], #12
        ld3             {v3.s, v4.s, v5.s}[1], [x15], #12
        ld3             {v3.s, v4.s, v5.s}[2], [x15]
99:
        ldr             w14, [sp, #0]
        movi            v7.8b, #128
        uxtl            v6.8h, v7.8b
        // Ensure if nothing to do then we do nothing
        cmp             w4, #0
        b.le            90f
        cmp             w5, #0
        b.le            90f
        // If w % 16 != 0 then -16 so we do main loop 1 fewer times with
        // the remainder done in the tail
        tst             w4, #15
        b.eq            1f
        sub             w4, w4, #16
1:

// -------------------- Even line body - YUV
11:
        subs            w9,  w4, #0
        mov             x10, x0
        mov             x11, x1
        mov             x12, x2
        mov             x13, x3
        b.lt            12f

        ld3             {v0.16b, v1.16b, v2.16b}, [x10], #48
        subs            w9, w9, #16
        b.le            13f

10:
        uxtl            v16.8h, v0.8b
        uxtl            v17.8h, v1.8b
        uxtl            v18.8h, v2.8b

        uxtl2           v20.8h, v0.16b
        uxtl2           v21.8h, v1.16b
        uxtl2           v22.8h, v2.16b

        bic             v0.8h, #0xff, LSL #8
        bic             v1.8h, #0xff, LSL #8
        bic             v2.8h, #0xff, LSL #8

        // Testing shows it is faster to stack the smull/smlal ops together
        // rather than interleave them between channels and indeed even the
        // shift/add sections seem happier not interleaved

        // Y0
        smull           v26.4s, v16.4h, v3.h[0]
        smlal           v26.4s, v17.4h, v4.h[0]
        smlal           v26.4s, v18.4h, v5.h[0]
        smull2          v27.4s, v16.8h, v3.h[0]
        smlal2          v27.4s, v17.8h, v4.h[0]
        smlal2          v27.4s, v18.8h, v5.h[0]
        // Y1
        smull           v28.4s, v20.4h, v3.h[0]
        smlal           v28.4s, v21.4h, v4.h[0]
        smlal           v28.4s, v22.4h, v5.h[0]
        smull2          v29.4s, v20.8h, v3.h[0]
        smlal2          v29.4s, v21.8h, v4.h[0]
        smlal2          v29.4s, v22.8h, v5.h[0]
        shrn            v26.4h, v26.4s, #12
        shrn2           v26.8h, v27.4s, #12
        add             v26.8h, v26.8h, v6.8h     // +128 (>> 3 = 16)
        sqrshrun        v26.8b, v26.8h, #3
        shrn            v28.4h, v28.4s, #12
        shrn2           v28.8h, v29.4s, #12
        add             v28.8h, v28.8h, v6.8h
        sqrshrun2       v26.16b, v28.8h, #3
        // Y0/Y1

        // U
        // Vector subscript *2 as we loaded into S but are only using H
        smull           v24.4s, v0.4h, v3.h[2]
        smlal           v24.4s, v1.4h, v4.h[2]
        smlal           v24.4s, v2.4h, v5.h[2]
        smull2          v25.4s, v0.8h, v3.h[2]
        smlal2          v25.4s, v1.8h, v4.h[2]
        smlal2          v25.4s, v2.8h, v5.h[2]

        // V
        smull           v30.4s, v0.4h, v3.h[4]
        smlal           v30.4s, v1.4h, v4.h[4]
        smlal           v30.4s, v2.4h, v5.h[4]
        smull2          v31.4s, v0.8h, v3.h[4]
        smlal2          v31.4s, v1.8h, v4.h[4]
        smlal2          v31.4s, v2.8h, v5.h[4]

        ld3             {v0.16b, v1.16b, v2.16b}, [x10], #48

        shrn            v24.4h, v24.4s, #14
        shrn2           v24.8h, v25.4s, #14
        sqrshrn         v24.8b, v24.8h, #1
        add             v24.8b, v24.8b, v7.8b     // +128
        shrn            v30.4h, v30.4s, #14
        shrn2           v30.8h, v31.4s, #14
        sqrshrn         v30.8b, v30.8h, #1
        add             v30.8b, v30.8b, v7.8b     // +128

        subs            w9, w9, #16

        st1             {v26.16b}, [x11], #16
        st1             {v24.8b}, [x12], #8
        st1             {v30.8b}, [x13], #8

        b.gt            10b

// -------------------- Even line tail - YUV
// If width % 16 == 0 then simply runs once with preloaded RGB
// If other then deals with preload & then does remaining tail

13:
        // Body is simple copy of main loop body minus preload

        uxtl            v16.8h, v0.8b
        uxtl            v17.8h, v1.8b
        uxtl            v18.8h, v2.8b

        uxtl2           v20.8h, v0.16b
        uxtl2           v21.8h, v1.16b
        uxtl2           v22.8h, v2.16b

        bic             v0.8h, #0xff, LSL #8
        bic             v1.8h, #0xff, LSL #8
        bic             v2.8h, #0xff, LSL #8

        // Y0
        smull           v26.4s, v16.4h, v3.h[0]
        smlal           v26.4s, v17.4h, v4.h[0]
        smlal           v26.4s, v18.4h, v5.h[0]
        smull2          v27.4s, v16.8h, v3.h[0]
        smlal2          v27.4s, v17.8h, v4.h[0]
        smlal2          v27.4s, v18.8h, v5.h[0]
        // Y1
        smull           v28.4s, v20.4h, v3.h[0]
        smlal           v28.4s, v21.4h, v4.h[0]
        smlal           v28.4s, v22.4h, v5.h[0]
        smull2          v29.4s, v20.8h, v3.h[0]
        smlal2          v29.4s, v21.8h, v4.h[0]
        smlal2          v29.4s, v22.8h, v5.h[0]
        shrn            v26.4h, v26.4s, #12
        shrn2           v26.8h, v27.4s, #12
        add             v26.8h, v26.8h, v6.8h     // +128 (>> 3 = 16)
        sqrshrun        v26.8b, v26.8h, #3
        shrn            v28.4h, v28.4s, #12
        shrn2           v28.8h, v29.4s, #12
        add             v28.8h, v28.8h, v6.8h
        sqrshrun2       v26.16b, v28.8h, #3
        // Y0/Y1

        // U
        // Vector subscript *2 as we loaded into S but are only using H
        smull           v24.4s, v0.4h, v3.h[2]
        smlal           v24.4s, v1.4h, v4.h[2]
        smlal           v24.4s, v2.4h, v5.h[2]
        smull2          v25.4s, v0.8h, v3.h[2]
        smlal2          v25.4s, v1.8h, v4.h[2]
        smlal2          v25.4s, v2.8h, v5.h[2]

        // V
        smull           v30.4s, v0.4h, v3.h[4]
        smlal           v30.4s, v1.4h, v4.h[4]
        smlal           v30.4s, v2.4h, v5.h[4]
        smull2          v31.4s, v0.8h, v3.h[4]
        smlal2          v31.4s, v1.8h, v4.h[4]
        smlal2          v31.4s, v2.8h, v5.h[4]

        cmp             w9, #-16

        shrn            v24.4h, v24.4s, #14
        shrn2           v24.8h, v25.4s, #14
        sqrshrn         v24.8b, v24.8h, #1
        add             v24.8b, v24.8b, v7.8b     // +128
        shrn            v30.4h, v30.4s, #14
        shrn2           v30.8h, v31.4s, #14
        sqrshrn         v30.8b, v30.8h, #1
        add             v30.8b, v30.8b, v7.8b     // +128

        // Here:
        // w9 == 0      width % 16 == 0, tail done
        // w9 > -16     1st tail done (16 pels), remainder still to go
        // w9 == -16    shouldn't happen
        // w9 > -32     2nd tail done
        // w9 <= -32    shouldn't happen

        b.lt            2f
        st1             {v26.16b}, [x11], #16
        st1             {v24.8b}, [x12], #8
        st1             {v30.8b}, [x13], #8
        cbz             w9, 3f

12:
        sub             w9, w9, #16

        tbz             w9, #3, 1f
        ld3             {v0.8b, v1.8b, v2.8b},  [x10], #24
1:      tbz             w9, #2, 1f
        ld3             {v0.b, v1.b, v2.b}[8],  [x10], #3
        ld3             {v0.b, v1.b, v2.b}[9],  [x10], #3
        ld3             {v0.b, v1.b, v2.b}[10], [x10], #3
        ld3             {v0.b, v1.b, v2.b}[11], [x10], #3
1:      tbz             w9, #1, 1f
        ld3             {v0.b, v1.b, v2.b}[12], [x10], #3
        ld3             {v0.b, v1.b, v2.b}[13], [x10], #3
1:      tbz             w9, #0, 13b
        ld3             {v0.b, v1.b, v2.b}[14], [x10], #3
        b               13b

2:
        tbz             w9, #3, 1f
        st1             {v26.8b},    [x11], #8
        st1             {v24.s}[0],  [x12], #4
        st1             {v30.s}[0],  [x13], #4
1:      tbz             w9, #2, 1f
        st1             {v26.s}[2],  [x11], #4
        st1             {v24.h}[2],  [x12], #2
        st1             {v30.h}[2],  [x13], #2
1:      tbz             w9, #1, 1f
        st1             {v26.h}[6],  [x11], #2
        st1             {v24.b}[6],  [x12], #1
        st1             {v30.b}[6],  [x13], #1
1:      tbz             w9, #0, 1f
        st1             {v26.b}[14], [x11]
        st1             {v24.b}[7],  [x12]
        st1             {v30.b}[7],  [x13]
1:
3:

// -------------------- Odd line body - Y only

        subs            w5, w5, #1
        b.eq            90f

        subs            w9,  w4, #0
        add             x0, x0, w14, SXTX
        add             x1, x1, w6, SXTX
        mov             x10, x0
        mov             x11, x1
        b.lt            12f

        ld3             {v0.16b, v1.16b, v2.16b}, [x10], #48
        subs            w9, w9, #16
        b.le            13f

10:
        uxtl            v16.8h, v0.8b
        uxtl            v17.8h, v1.8b
        uxtl            v18.8h, v2.8b

        uxtl2           v20.8h, v0.16b
        uxtl2           v21.8h, v1.16b
        uxtl2           v22.8h, v2.16b

        // Testing shows it is faster to stack the smull/smlal ops together
        // rather than interleave them between channels and indeed even the
        // shift/add sections seem happier not interleaved

        // Y0
        smull           v26.4s, v16.4h, v3.h[0]
        smlal           v26.4s, v17.4h, v4.h[0]
        smlal           v26.4s, v18.4h, v5.h[0]
        smull2          v27.4s, v16.8h, v3.h[0]
        smlal2          v27.4s, v17.8h, v4.h[0]
        smlal2          v27.4s, v18.8h, v5.h[0]
        // Y1
        smull           v28.4s, v20.4h, v3.h[0]
        smlal           v28.4s, v21.4h, v4.h[0]
        smlal           v28.4s, v22.4h, v5.h[0]
        smull2          v29.4s, v20.8h, v3.h[0]
        smlal2          v29.4s, v21.8h, v4.h[0]
        smlal2          v29.4s, v22.8h, v5.h[0]

        ld3             {v0.16b, v1.16b, v2.16b}, [x10], #48

        shrn            v26.4h, v26.4s, #12
        shrn2           v26.8h, v27.4s, #12
        add             v26.8h, v26.8h, v6.8h     // +128 (>> 3 = 16)
        sqrshrun        v26.8b, v26.8h, #3
        shrn            v28.4h, v28.4s, #12
        shrn2           v28.8h, v29.4s, #12
        add             v28.8h, v28.8h, v6.8h
        sqrshrun2       v26.16b, v28.8h, #3
        // Y0/Y1

        subs            w9, w9, #16

        st1             {v26.16b}, [x11], #16

        b.gt            10b

// -------------------- Odd line tail - Y
// If width % 16 == 0 then simply runs once with preloaded RGB
// If other then deals with preload & then does remaining tail

13:
        // Body is simple copy of main loop body minus preload

        uxtl            v16.8h, v0.8b
        uxtl            v17.8h, v1.8b
        uxtl            v18.8h, v2.8b

        uxtl2           v20.8h, v0.16b
        uxtl2           v21.8h, v1.16b
        uxtl2           v22.8h, v2.16b

        // Y0
        smull           v26.4s, v16.4h, v3.h[0]
        smlal           v26.4s, v17.4h, v4.h[0]
        smlal           v26.4s, v18.4h, v5.h[0]
        smull2          v27.4s, v16.8h, v3.h[0]
        smlal2          v27.4s, v17.8h, v4.h[0]
        smlal2          v27.4s, v18.8h, v5.h[0]
        // Y1
        smull           v28.4s, v20.4h, v3.h[0]
        smlal           v28.4s, v21.4h, v4.h[0]
        smlal           v28.4s, v22.4h, v5.h[0]
        smull2          v29.4s, v20.8h, v3.h[0]
        smlal2          v29.4s, v21.8h, v4.h[0]
        smlal2          v29.4s, v22.8h, v5.h[0]

        cmp             w9, #-16

        shrn            v26.4h, v26.4s, #12
        shrn2           v26.8h, v27.4s, #12
        add             v26.8h, v26.8h, v6.8h     // +128 (>> 3 = 16)
        sqrshrun        v26.8b, v26.8h, #3
        shrn            v28.4h, v28.4s, #12
        shrn2           v28.8h, v29.4s, #12
        add             v28.8h, v28.8h, v6.8h
        sqrshrun2       v26.16b, v28.8h, #3
        // Y0/Y1

        // Here:
        // w9 == 0      width % 16 == 0, tail done
        // w9 > -16     1st tail done (16 pels), remainder still to go
        // w9 == -16    shouldn't happen
        // w9 > -32     2nd tail done
        // w9 <= -32    shouldn't happen

        b.lt            2f
        st1             {v26.16b}, [x11], #16
        cbz             w9, 3f

12:
        sub             w9, w9, #16

        tbz             w9, #3, 1f
        ld3             {v0.8b, v1.8b, v2.8b},  [x10], #24
1:      tbz             w9, #2, 1f
        ld3             {v0.b, v1.b, v2.b}[8],  [x10], #3
        ld3             {v0.b, v1.b, v2.b}[9],  [x10], #3
        ld3             {v0.b, v1.b, v2.b}[10], [x10], #3
        ld3             {v0.b, v1.b, v2.b}[11], [x10], #3
1:      tbz             w9, #1, 1f
        ld3             {v0.b, v1.b, v2.b}[12], [x10], #3
        ld3             {v0.b, v1.b, v2.b}[13], [x10], #3
1:      tbz             w9, #0, 13b
        ld3             {v0.b, v1.b, v2.b}[14], [x10], #3
        b               13b

2:
        tbz             w9, #3, 1f
        st1             {v26.8b},    [x11], #8
1:      tbz             w9, #2, 1f
        st1             {v26.s}[2],  [x11], #4
1:      tbz             w9, #1, 1f
        st1             {v26.h}[6],  [x11], #2
1:      tbz             w9, #0, 1f
        st1             {v26.b}[14], [x11]
1:
3:

// ------------------- Loop to start

        add             x0, x0, w14, SXTX
        add             x1, x1, w6, SXTX
        add             x2, x2, w7, SXTX
        add             x3, x3, w7, SXTX
        subs            w5, w5, #1
        b.gt            11b
90:
        ret
endfunc
