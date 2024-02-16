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

// Expand rgb2 into r0+r1/g0+g1/b0+b1
.macro XRGB3Y r0, g0, b0, r1, g1, b1, r2, g2, b2
        uxtl            \r0\().8h, \r2\().8b
        uxtl            \g0\().8h, \g2\().8b
        uxtl            \b0\().8h, \b2\().8b

        uxtl2           \r1\().8h, \r2\().16b
        uxtl2           \g1\().8h, \g2\().16b
        uxtl2           \b1\().8h, \b2\().16b
.endm

// Expand rgb2 into r0+r1/g0+g1/b0+b1
// and pick every other el to put back into rgb2 for chroma
.macro XRGB3YC r0, g0, b0, r1, g1, b1, r2, g2, b2
        XRGB3Y          \r0, \g0, \b0, \r1, \g1, \b1, \r2, \g2, \b2

        bic             \r2\().8h, #0xff, LSL #8
        bic             \g2\().8h, #0xff, LSL #8
        bic             \b2\().8h, #0xff, LSL #8
.endm

.macro SMLAL3 d0, d1, s0, s1, s2, c0, c1, c2
        smull           \d0\().4s, \s0\().4h, \c0
        smlal           \d0\().4s, \s1\().4h, \c1
        smlal           \d0\().4s, \s2\().4h, \c2
        smull2          \d1\().4s, \s0\().8h, \c0
        smlal2          \d1\().4s, \s1\().8h, \c1
        smlal2          \d1\().4s, \s2\().8h, \c2
.endm

// d0 may be s0
// s0, s2 corrupted
.macro SHRN_Y d0, s0, s1, s2, s3, k128h
        shrn            \s0\().4h, \s0\().4s, #12
        shrn2           \s0\().8h, \s1\().4s, #12
        add             \s0\().8h, \s0\().8h, \k128h\().8h     // +128 (>> 3 = 16)
        sqrshrun        \d0\().8b, \s0\().8h, #3
        shrn            \s2\().4h, \s2\().4s, #12
        shrn2           \s2\().8h, \s3\().4s, #12
        add             \s2\().8h, \s2\().8h, \k128h\().8h
        sqrshrun2       \d0\().16b, v28.8h, #3
.endm

.macro SHRN_C d0, s0, s1, k128b
        shrn            \s0\().4h, \s0\().4s, #14
        shrn2           \s0\().8h, \s1\().4s, #14
        sqrshrn         \s0\().8b, \s0\().8h, #1
        add             \d0\().8b, \s0\().8b, \k128b\().8b     // +128
.endm

.macro STB2V s0, n, a
        st1             {\s0\().b}[(\n+0)], [\a], #1
        st1             {\s0\().b}[(\n+1)], [\a], #1
.endm

.macro STB4V s0, n, a
        STB2V           \s0, (\n+0), \a
        STB2V           \s0, (\n+2), \a
.endm


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
        ld3             {v3.s, v4.s, v5.s}[0], [x15], #12
        ld3             {v3.s, v4.s, v5.s}[1], [x15], #12
        ld3             {v3.s, v4.s, v5.s}[2], [x15]
        mov             v6.16b, v3.16b
        mov             v3.16b, v5.16b
        mov             v5.16b, v6.16b
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
//              int32_t *rgb2yuv);              // [sp, #8] (including Mac)

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
        XRGB3YC         v16, v17, v18,  v20, v21, v22,  v0, v1, v2

        // Testing shows it is faster to stack the smull/smlal ops together
        // rather than interleave them between channels and indeed even the
        // shift/add sections seem happier not interleaved

        // Y0
        SMLAL3          v26, v27, v16, v17, v18, v3.h[0], v4.h[0], v5.h[0]
        // Y1
        SMLAL3          v28, v29, v20, v21, v22, v3.h[0], v4.h[0], v5.h[0]
        SHRN_Y          v26, v26, v27, v28, v29, v6

        // U
        // Vector subscript *2 as we loaded into S but are only using H
        SMLAL3          v24, v25, v0, v1, v2, v3.h[2], v4.h[2], v5.h[2]

        // V
        SMLAL3          v30, v31, v0, v1, v2, v3.h[4], v4.h[4], v5.h[4]

        ld3             {v0.16b, v1.16b, v2.16b}, [x10], #48

        SHRN_C          v24, v24, v25, v7
        SHRN_C          v30, v30, v31, v7

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

        XRGB3YC         v16, v17, v18,  v20, v21, v22,  v0, v1, v2
        // Y0
        SMLAL3          v26, v27, v16, v17, v18, v3.h[0], v4.h[0], v5.h[0]
        // Y1
        SMLAL3          v28, v29, v20, v21, v22, v3.h[0], v4.h[0], v5.h[0]
        SHRN_Y          v26, v26, v27, v28, v29, v6
        // U
        SMLAL3          v24, v25, v0, v1, v2, v3.h[2], v4.h[2], v5.h[2]
        // V
        SMLAL3          v30, v31, v0, v1, v2, v3.h[4], v4.h[4], v5.h[4]

        cmp             w9, #-16

        SHRN_C          v24, v24, v25, v7
        SHRN_C          v30, v30, v31, v7

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
        STB4V           v24, 0, x12
        STB4V           v30, 0, x13
1:      tbz             w9, #2, 1f
        STB4V           v26  8, x11
        STB2V           v24, 4, x12
        STB2V           v30, 4, x13
1:      tbz             w9, #1, 1f
        STB2V           v26, 12, x11
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
        add             x0, x0, w14, sxtw
        add             x1, x1, w6, sxtw
        mov             x10, x0
        mov             x11, x1
        b.lt            12f

        ld3             {v0.16b, v1.16b, v2.16b}, [x10], #48
        subs            w9, w9, #16
        b.le            13f

10:
        XRGB3Y          v16, v17, v18,  v20, v21, v22,  v0, v1, v2
        // Y0
        SMLAL3          v26, v27, v16, v17, v18, v3.h[0], v4.h[0], v5.h[0]
        // Y1
        SMLAL3          v28, v29, v20, v21, v22, v3.h[0], v4.h[0], v5.h[0]

        ld3             {v0.16b, v1.16b, v2.16b}, [x10], #48

        SHRN_Y          v26, v26, v27, v28, v29, v6

        subs            w9, w9, #16

        st1             {v26.16b}, [x11], #16

        b.gt            10b

// -------------------- Odd line tail - Y
// If width % 16 == 0 then simply runs once with preloaded RGB
// If other then deals with preload & then does remaining tail

13:
        // Body is simple copy of main loop body minus preload

        XRGB3Y          v16, v17, v18,  v20, v21, v22,  v0, v1, v2
        // Y0
        SMLAL3          v26, v27, v16, v17, v18, v3.h[0], v4.h[0], v5.h[0]
        // Y1
        SMLAL3          v28, v29, v20, v21, v22, v3.h[0], v4.h[0], v5.h[0]

        cmp             w9, #-16

        SHRN_Y          v26, v26, v27, v28, v29, v6

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
        STB4V           v26, 8,  x11
1:      tbz             w9, #1, 1f
        STB2V           v26, 12, x11
1:      tbz             w9, #0, 1f
        st1             {v26.b}[14], [x11]
1:
3:

// ------------------- Loop to start

        add             x0, x0, w14, sxtw
        add             x1, x1, w6, sxtw
        add             x2, x2, w7, sxtw
        add             x3, x3, w7, sxtw
        subs            w5, w5, #1
        b.gt            11b
90:
        ret
endfunc
