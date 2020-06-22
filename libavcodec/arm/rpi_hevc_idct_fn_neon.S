/*
 * ARM NEON optimised IDCT functions for HEVC decoding
 * Copyright (c) 2014 Seppo Tomperi <seppo.tomperi@vtt.fi>
 * Copyright (C) 2018 John Cox, ben Avison for Raspberry Pi (Trading)
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

@ Included multiple times from hevc_idct_neon.S
@ Macros defined there

#define DC_SHIFT  (15 - BIT_DEPTH)
#define DC_ADD    (1 | (1 << (14 - BIT_DEPTH)))
#define TRN_SHIFT (20 - BIT_DEPTH)

function JOIN(ff_hevc_rpi_idct_4x4_dc_neon_, BIT_DEPTH), export=1
        ldrsh       r1, [r0]
        add         r1, #DC_ADD
        asr         r1, #DC_SHIFT
        vdup.16     q0, r1
        vdup.16     q1, r1
        vst1.16     {q0, q1}, [r0]
        bx lr
endfunc

function JOIN(ff_hevc_rpi_idct_8x8_dc_neon_, BIT_DEPTH), export=1
        ldrsh       r1, [r0]
        add         r2, r0, #32
        mov         r3, #64
        add         r1, #DC_ADD
        asr         r1, #DC_SHIFT
        vdup.16     q8, r1
        vdup.16     q9, r1
        vst1.16     {q8, q9}, [r0], r3
        vst1.16     {q8, q9}, [r2], r3
        vst1.16     {q8, q9}, [r0]
        vst1.16     {q8, q9}, [r2]
        bx lr
endfunc

function JOIN(ff_hevc_rpi_idct_16x16_dc_neon_, BIT_DEPTH), export=1
        ldrsh       r1, [r0]
        add         r2, r0, #32
        mov         r3, #64
        add         r1, #DC_ADD
        mov         ip, #16*16
        asr         r1, #DC_SHIFT
        vdup.16     q8, r1
        vdup.16     q9, r1
1:      vst1.16     {q8, q9}, [r0], r3
        subs        ip, ip, #32
        vst1.16     {q8, q9}, [r2], r3
        bhi         1b
        bx lr
endfunc

function JOIN(ff_hevc_rpi_idct_32x32_dc_neon_, BIT_DEPTH), export=1
        ldrsh       r1, [r0]
        add         r2, r0, #32
        mov         r3, #64
        add         r1, #DC_ADD
        mov         ip, #32*32
        asr         r1, #DC_SHIFT
        vdup.16     q8, r1
        vdup.16     q9, r1
1:      vst1.16     {q8, q9}, [r0], r3
        subs        ip, ip, #32
        vst1.16     {q8, q9}, [r2], r3
        bhi         1b
        bx lr
endfunc


function JOIN(ff_hevc_rpi_transform_4x4_neon_, BIT_DEPTH), export=1
        vldr.i32    s0, =0x00240053 // 36 and 83
        vld1.16     {q14, q15}, [r0 :256]  // coeffs

        tr4_shift   #7

        vzip.16     d28, d29
        vzip.16     d30, d31
        vzip.32     q14, q15

        tr4_shift   #TRN_SHIFT

        vst4.16     {q14, q15}, [r0 :256]
        bx lr

        .ltorg
endfunc



function JOIN(ff_hevc_rpi_transform_luma_4x4_neon_, BIT_DEPTH), export=1
        vmov.i32    d0, #0x4a  // 74
        vld1.16     {q14, q15}, [r0 :256]  // coeffs
        vmov.i32    d1, #0x1d  // 29
        vmov.i32    d2, #0x37  // 55

        tr4_luma_shift #7

        vzip.16     d28, d29
        vzip.16     d30, d31
        vzip.32     q14, q15

        tr4_luma_shift #TRN_SHIFT

        vst4.16     {q14, q15}, [r0 :256]
        bx lr
endfunc

function JOIN(ff_hevc_rpi_transform_8x8_neon_, BIT_DEPTH), export=1
        add      r2, r0, #16
        adr      r3, tr4f
        vpush    {d8-d15}
        vld1.16  {d0, d1}, [r3]
        mov      r3, #32

        tr8_vert  d16, d17, d18, d19, d24, d25, d26, d27, q8,  q9,  \
            "sub      r0, r0, #128-8",                              \
            "sub      r2, r2, #128-8",                              \
            "cmp      r1, #4"
        ble      2f

        tr8_vert  d20, d21, d22, d23, d28, d29, d30, d31, q10, q11, \
            "sub      r0, r0, #128+8",                              \
            "sub      r2, r2, #128+8+16-32",                        \
            "mov      r3, #64"

        vzip.16  d16, d17
        vzip.16  d18, d19

        vzip.16  d20, d21
        vzip.16  d22, d23
        vzip.16  d28, d29
        vzip.16  d30, d31
        vzip.32  q10, q11
        vzip.32  q14, q15
1:
        vzip.16  d24, d25
        vzip.16  d26, d27
        vzip.32  q8, q9
        vzip.32  q12, q13

        tr8_horiz d16, d17, d18, d19, d20, d21, d22, d23, q8,  q9,  TRN_SHIFT
        tr8_horiz d24, d25, d26, d27, d28, d29, d30, d31, q12, q13, TRN_SHIFT

        vpop     {d8-d15}
        bx       lr

2:      vmov.i64 q10, #0
        sub      r0, r0, #8
        vmov.i64 q11, #0
        sub      r2, r2, #8+16-32
        vmov.i64 q14, #0
        mov      r3, #64
        vmov.i64 q15, #0

        vzip.16  d16, d17
        vzip.16  d18, d19

        b        1b

endfunc

#undef DC_SHIFT
#undef DC_ADD
#undef TRN_SHIFT

