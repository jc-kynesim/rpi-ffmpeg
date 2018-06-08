/*
 * Copyright (c) 2018 John Cox <jc@kynesim.co.uk> (for Raspberry Pi)
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

/*
 * Horizontal & Vertical special cases of angular intra pred
 *
 * Split out because:
 *  Vertical, at least, is relatively common
 *  Much simpler code than the general angular case
 *  Luma with size < 32 has extra filtering that doesn't happen anywhere else
 *
 * *** Currently luma filtering is mandatory where it occurs, but there are
 *     cases where it should be turned off (rdpcm & an extension sps flag).
 *     These don't occur in the standard conformance suite for Main Profile
 */

#include "libavutil/arm/asm.S"
#include "neon.S"

@ ff_hevc_rpi_pred_vertical_4_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_vertical_4_neon_8, export=1
        ldrb        ip, [r2, #-1]       @ Top-left
        vld1.32     {d0[0]}, [r2 :32]   @ Left
        add         r2, r0, r3
        vld1.8      {d1[]}, [r1]
        lsl         r3, #1
        vdup.8      d4, ip
        vmov.i8     d2, #128
        vhsub.u8    d4, d0, d4
        veor        d1, d2
        vld1.32     {d0[0]}, [r1 :32]   @ Top
        vqadd.s8    d1, d4
        vmov.i64    d3, #0xff
        vmov        d4, d0
        veor        d5, d1, d2
        veor        d1, d1, d2
        vbit        d0, d1, d3
        vshr.u64    d5, #8
        vst1.32     {d0[0]}, [r0], r3
        vshr.u64    d1, #16
        vbit        d4, d5, d3
        vshr.u64    d5, #16
        vst1.32     {d4[0]}, [r2], r3
        vbit        d0, d1, d3
        vst1.32     {d0[0]}, [r0]
        vbit        d4, d5, d3
        vst1.32     {d4[0]}, [r2]

        bx          lr
endfunc


@ ff_hevc_rpi_pred_vertical_8_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_vertical_8_neon_8, export=1
        ldrb        ip, [r2, #-1]       @ Top-left
        vld1.8      {d0}, [r2 :64]      @ Left
        vmov.i8     d1, #128
        vld1.8      {d2[]}, [r1]
        vld1.8      {d3}, [r1 :64]      @ Top
        vdup.8      d4, ip
        vhsub.u8    d4, d0, d4
        veor        d2, d1
        vmov.i64    d0, #0xff
        mov         r1, #8
        vqadd.s8    d2, d4, d2
        veor        d1, d2, d1
1:
        vbit        d3, d1, d0
        vshr.u64    d1, #8
        vst1.8      {d3}, [r0 :64], r3
        subs        r1, #2
        vbit        d3, d1, d0
        vshr.u64    d1, #8
        vst1.8      {d3}, [r0 :64], r3
        bne         1b

        bx          lr
endfunc


@ ff_hevc_rpi_pred_vertical_16_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_vertical_16_neon_8, export=1
        ldrb        ip, [r2, #-1]       @ Top-left
        vld1.8      {q0}, [r2 :128]     @ Left
        vdup.8      q1, ip
        vld1.8      {d4[],d5[]}, [r1]
        vhsub.u8    q0, q1
        vmov.i8     q1, #128
        veor        q2, q1
        vmov.i64    d16, #0xff
        vqadd.s8    q0, q2
        vld1.8      {q3}, [r1 :128]     @ Top
        mov         r1, #16
        veor        q0, q1
        vmov        q1, q3
        vext.8      q2, q0, q0, #1
1:
        vbit        d2, d0, d16
        vbit        d6, d4, d16
        vext.8      q0, q0, q0, #2
        subs        r1, #2
        vst1.8      {q1}, [r0 :128], r3
        vext.8      q2, q2, q2, #2
        vst1.8      {q3}, [r0 :128], r3
        bne         1b

        bx          lr
endfunc


@ ff_hevc_rpi_pred_vert_32_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_vertical_32_neon_8, export=1
        vld1.8     {q0,  q1 }, [r1  :128]    @ Up
        add         r2,  r0,  r3
        lsl         r3,  #1
        mov         r1,  #16
1:
        vst1.8     {q0,  q1 }, [r0  :128], r3
        subs        r1,  #1
        vst1.8     {q0,  q1 }, [r2  :128], r3
        bne         1b

        bx          lr
endfunc


@ ff_hevc_rpi_pred_vertical_c_4_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_vertical_c_4_neon_8, export=1
        vld1.16    {d0 }, [r1  :64]    @ Up
        add         r2,  r0,  r3,  lsl #1
        lsl         r3,  #2

        vst1.16    {d0 }, [r0  :64], r3
        vst1.16    {d0 }, [r2  :64], r3
        vst1.16    {d0 }, [r0  :64]
        vst1.16    {d0 }, [r2  :64]

        bx          lr
endfunc


@ ff_hevc_rpi_pred_vertical_c_8_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_vertical_c_8_neon_8, export=1
        vld1.16    {q0 }, [r1  :128]    @ Up
        add         r2,  r0,  r3,  lsl #1
        lsl         r3,  #2
        mov         r1,  #4
1:
        vst1.16    {q0 }, [r0  :128], r3
        subs        r1,  #2
        vst1.16    {q0 }, [r2  :128], r3
        vst1.16    {q0 }, [r0  :128], r3
        vst1.16    {q0 }, [r2  :128], r3
        bne         1b

        bx          lr
endfunc


@ ff_hevc_rpi_pred_vertical_c_16_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_vertical_c_16_neon_8, export=1
        vld1.16    {q0,  q1 }, [r1  :128]    @ Up
        add         r2,  r0,  r3,  lsl #1
        lsl         r3,  #2
        mov         r1,  #8
1:
        vst1.16    {q0,  q1 }, [r0  :128], r3
        subs        r1,  #1
        vst1.16    {q0,  q1 }, [r2  :128], r3
        bne         1b

        bx          lr
endfunc


@ ff_hevc_rpi_pred_horizontalal_4_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

@ ? Might be faster as simple arm

function ff_hevc_rpi_pred_horizontal_4_neon_8, export=1
        vld1.32    {d0[0] }, [r1  :32]  @ Up
        ldrb        r12, [r2, #-1]      @ Up-left
        vld1.32    {d16[0]}, [r2  :32]  @ left

        vdup.8      d4,  r12
        vmov.u8     d6,  #128
        vhsub.u8    d0,  d4

        veor.8      d2,  d16, d6        @ Make -128,127 so we can qadd
        add         r2,  r0,  r3
        vdup.8      d2,  d2[0]
        lsl         r3,  #1
        vqadd.s8    d0,  d2
        veor.8      d0,  d6

        vdup.8      d1,  d16[1]
        vdup.8      d2,  d16[2]
        vdup.8      d3,  d16[3]
        vst1.32    {d0[0] }, [r0  :32], r3
        vst1.32    {d1[0] }, [r2  :32], r3
        vst1.32    {d2[0] }, [r0  :32]
        vst1.32    {d3[0] }, [r2  :32]

        bx          lr
endfunc


@ ff_hevc_rpi_pred_horizontal_8_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_horizontal_8_neon_8, export=1
        vld1.8     {d0 }, [r1  :64]    @ Up
        ldrb        r12, [r2, #-1]      @ Up-left
        vld1.8     {d16}, [r2  :64]    @ left

        vdup.8      d4,  r12
        vmov.u8     d6,  #128
        vhsub.u8    d0,  d4

        veor.8      d2,  d16, d6        @ Make -128,127 so we can qadd
        add         r2,  r0,  r3
        vdup.8      d2,  d2[0]
        lsl         r3,  #1
        vqadd.s8    d0,  d2
        mov         r1,  #3
        veor.8      d0,  d6

        vdup.8      d4,  d16[1]
        vst1.8     {d0 }, [r0  :64], r3
        vst1.8     {d4 }, [r2  :64], r3

1:
        vext.8      d16,  d16, #2
        subs        r1,  #1
        vdup.8      d0,  d16[0]
        vdup.8      d4,  d16[1]
        vst1.8     {d0 }, [r0  :64], r3
        vst1.8     {d4 }, [r2  :64], r3
        bne         1b

        bx          lr
endfunc


@ ff_hevc_rpi_pred_horizontal_16_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_horizontal_16_neon_8, export=1
        vld1.8     {q0 }, [r1  :128]    @ Up
        ldrb        r12, [r2, #-1]      @ Up-left
        vld1.8     {q8 }, [r2  :128]    @ left

        vdup.8      q2,  r12
        vmov.u8     q3,  #128
        vhsub.u8    q0,  q2

        veor.8      d2,  d16, d6        @ Make -128,127 so we can qadd
        add         r2,  r0,  r3
        vdup.8      q1,  d2[0]
        lsl         r3,  #1
        vqadd.s8    q0,  q1
        mov         r1,  #7
        veor.8      q0,  q3

        vdup.8      q2,  d16[1]
        vst1.8     {q0 }, [r0  :128], r3
        vst1.8     {q2 }, [r2  :128], r3

1:
        vext.8      q8,  q8,  #2
        subs        r1,  #1
        vdup.8      q0,  d16[0]
        vdup.8      q2,  d16[1]
        vst1.8     {q0 }, [r0  :128], r3
        vst1.8     {q2 }, [r2  :128], r3
        bne         1b

        bx          lr
endfunc


@ ff_hevc_rpi_pred_horizontal_32_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_horizontal_32_neon_8, export=1
        vld1.8     {q8,  q9 }, [r2  :128]    @ Left
        add         r2,  r0,  r3
        lsl         r3,  #1
        mov         r1,  #16
1:
        vdup.8      q0,  d16[0]
        vdup.8      q1,  d16[0]
        vdup.8      q2,  d16[1]
        vdup.8      q3,  d16[1]
        vext.8      q8,  q9,  #2
        vext.8      q9,  q9,  #2
        vst1.8     {q0,  q1 }, [r0  :128], r3
        subs        r1,  #1
        vst1.8     {q2,  q3 }, [r2  :128], r3
        bne         1b

        bx          lr
endfunc


@ ff_hevc_rpi_pred_horizontal_c_4_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_horizontal_c_4_neon_8, export=1
        vld1.16    {d16}, [r2  :64]    @ Left
        add         r2,  r0,  r3,  lsl #1
        lsl         r3,  #2

        vdup.16     d0,  d16[0]
        vdup.16     d1,  d16[1]
        vdup.16     d2,  d16[2]
        vdup.16     d3,  d16[3]

        vst1.16    {d0 }, [r0  :64], r3
        vst1.16    {d1 }, [r2  :64], r3
        vst1.16    {d2 }, [r0  :64]
        vst1.16    {d3 }, [r2  :64]

        bx          lr
endfunc


@ ff_hevc_rpi_pred_horizontal_c_8_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_horizontal_c_8_neon_8, export=1
        vld1.16    {q8 }, [r2  :128]    @ Left
        add         r2,  r0,  r3,  lsl #1
        lsl         r3,  #2
        mov         r1,  #4
1:
        vdup.16     q0,  d16[0]
        vdup.16     q2,  d16[1]
        vext.16     q8,  q8,  #2
        vst1.16    {q0 }, [r0  :128], r3
        subs        r1,  #1
        vst1.16    {q2 }, [r2  :128], r3
        bne         1b

        bx          lr
endfunc


@ ff_hevc_rpi_pred_horizontal_c_16_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_horizontal_c_16_neon_8, export=1
        vld1.16    {q8,  q9 }, [r2  :128]    @ Left
        add         r2,  r0,  r3,  lsl #1
        lsl         r3,  #2
        mov         r1,  #8
1:
        vdup.16     q0,  d16[0]
        vdup.16     q1,  d16[0]
        vdup.16     q2,  d16[1]
        vdup.16     q3,  d16[1]
        vext.16     q8,  q9,  #2
        vext.16     q9,  q9,  #2
        vst1.16    {q0,  q1 }, [r0  :128], r3
        subs        r1,  #1
        vst1.16    {q2,  q3 }, [r2  :128], r3
        bne         1b

        bx          lr
endfunc


@------------------------------------------------------------------------------
@
@ 10 Bit
@ Has clipping constants so 10-bit only but could easily be macroed up to
@ 14-bit before we run out of bits


@ ff_hevc_rpi_pred_vertical_4_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_vertical_4_neon_10, export=1
        ldrh        ip, [r2, #-2]       @ Top-left
        vld1.16     {d0}, [r2 :64]      @ Left
        vmov.i16    d2, #0
        vld1.16     {d1[]}, [r1]
T       lsl         r3, #1
        vdup.16     d4, ip
        vmov.i16    d3, #0x3ff
        vld1.16     {d5}, [r1 :64]      @ Top
        vhsub.u16   d4, d0, d4
        vmov.i64    d0, #0xffff
A       add         r2, r0, r3, lsl #1
T       add         r2, r0, r3
        vadd.i16    d1, d1, d4
        vmov        d6, d5
        vmax.s16    d1, d1, d2
        vmin.s16    d2, d1, d3
        vmin.s16    d1, d1, d3
        vbit        d5, d1, d0
A       lsl         r3, #2
T       lsl         r3, #1
        vshr.u64    d2, #16
        vshr.u64    d1, #32
        vbit        d6, d2, d0
        vst1.16     {d5}, [r0], r3
        vshr.u64    d2, #32
        vst1.16     {d6}, [r2], r3
        vbit        d5, d1, d0
        vst1.16     {d5}, [r0]
        vbit        d6, d2, d0
        vst1.16     {d6}, [r2]
        bx          lr
endfunc


@ ff_hevc_rpi_pred_vertical_8_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_vertical_8_neon_10, export=1
        ldrh        ip, [r2, #-2]       @ Top-left
        vld1.16     {q0}, [r2 :128]     @ Left
        lsl         r3, #1
        vdup.16     q1, ip
        vld1.16     {d4[],d5[]}, [r1]
        vhsub.u16   q0, q0, q1
        vmov.i16    q1, #0
        vadd.i16    q0, q2
        vmov.i16    q2, #0x3ff
        vld1.16     {q3}, [r1 :128]     @ Top
        mov         r1, #8
        vmax.s16    q0, q1
        vmov        q1, q3
        vmin.s16    q0, q2
        vmov.i64    d16, #0xffff
        vext.16     q2, q0, q0, #1
1:
        vbit        d2, d0, d16
        vbit        d6, d4, d16
        vext.16     q0, q0, q0, #2
        subs        r1, #2
        vst1.16     {q1}, [r0 :128], r3
        vext.16     q2, q2, q2, #2
        vst1.16     {q3}, [r0 :128], r3
        bne         1b

        bx          lr
endfunc


@ ff_hevc_rpi_pred_vertical_16_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_vertical_16_neon_10, export=1
        ldrh        ip, [r2, #-2]       @ Top-left
        vld1.16     {q0-q1}, [r2 :128]  @ Left
T       lsl         r3, #1
        vdup.16     q2, ip
A       add         r2, r0, r3, lsl #1
T       add         r2, r0, r3
        vld1.16     {d6[],d7[]}, [r1]
A       lsl         r3, #2
T       lsl         r3, #1
        vhsub.u16   q0, q2
        vhsub.u16   q1, q2
        vadd.i16    q0, q3
        vadd.i16    q1, q3
        vmov.i16    q2, #0
        vld1.16     {q8-q9}, [r1 :128]  @ Top
        mov         r1, #0
        vmov.i16    q3, #0x3ff
        vmax.s16    q0, q2
        vmax.s16    q1, q2
        vmin.s16    q0, q3
        vmin.s16    q1, q3
        vmov        q10, q8
        vmov        q11, q9
        vext.16     q2, q0, q1, #1
        vext.16     q3, q1, q1, #1
        vmov.i64    d24, #0xffff
1:
        vbit        d16, d0, d24
        vbit        d20, d4, d24
        vext.16     q0, q0, q0, #2
        subs        r1, #1<<30
        vst1.16     {q8-q9}, [r0 :128], r3
        vext.16     q2, q2, q2, #2
        vst1.16     {q10-q11}, [r2 :128], r3
        bne         1b
1:
        vbit        d16, d2, d24
        vbit        d20, d6, d24
        vext.16     q1, q1, q1, #2
        subs        r1, #1<<30
        vst1.16     {q8-q9}, [r0 :128], r3
        vext.16     q3, q3, q3, #2
        vst1.16     {q10-q11}, [r2 :128], r3
        bne         1b

        bx          lr
endfunc


@ ff_hevc_rpi_pred_vertical_32_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_vertical_32_neon_10, export=1
        vldm        r1, { q0-q3 }    @ Up
        lsl         r3, #1
        mov         r1, #32
        add         r2, r0, #32
1:
        vst1.16     {q0-q1}, [r0 :128], r3
        subs        r1, #1
        vst1.16     {q2-q3}, [r2 :128], r3
        bne         1b

        bx          lr
endfunc


@ ff_hevc_rpi_pred_vertical_c_4_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_vertical_c_4_neon_10, export=1
        vld1.16    {q0 }, [r1  :128]    @ Up
        add         r2,  r0,  r3,  lsl #2
        lsl         r3,  #3

        vst1.16    {q0 }, [r0  :128], r3
        vst1.16    {q0 }, [r2  :128], r3
        vst1.16    {q0 }, [r0  :128]
        vst1.16    {q0 }, [r2  :128]

        bx          lr
endfunc


@ ff_hevc_rpi_pred_vertical_c_8_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_vertical_c_8_neon_10, export=1
        vld1.16    {q0,  q1 }, [r1  :128]    @ Up
        add         r2,  r0,  r3,  lsl #2
        lsl         r3,  #3
        mov         r1,  #4
1:
        vst1.16    {q0,  q1 }, [r0  :128], r3
        subs        r1,  #1
        vst1.16    {q0,  q1 }, [r2  :128], r3
        bne         1b

        bx          lr
endfunc


@ ff_hevc_rpi_pred_vertical_c_16_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_vertical_c_16_neon_10, export=1
        vldm        r1, { q0-q3 }    @ Up
        lsl         r3, #2
        mov         r1, #16
        add         r2, r0, #32
1:
        vst1.16     {q0-q1}, [r0 :128], r3
        subs        r1, #1
        vst1.16     {q2-q3}, [r2 :128], r3
        bne         1b

        bx          lr
endfunc

@ ff_hevc_rpi_pred_horizontal_4_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_horizontal_4_neon_10, export=1
        vld1.16    {d0 }, [r1  :64]     @ Up
        ldrh        r12, [r2, #-2]      @ Up-left
        vld1.16    {d16}, [r2  :64]     @ left

        vdup.16     d4,  r12
        add         r2,  r0,  r3,  lsl #1
        vhsub.u16   d0,  d4

        vdup.16     d6,  d16[0]
        vmov.s16    d4,  #0
        vadd.i16    d0,  d6

        vmov.s16    d6,  #0x3ff
        vmax.s16    d0,  d4
        lsl         r3,  #2
        vmin.s16    d0,  d6

        vdup.16     d1,  d16[1]
        vdup.16     d2,  d16[2]
        vdup.16     d3,  d16[3]

        vst1.16    {d0 }, [r0  :64], r3
        vst1.16    {d1 }, [r2  :64], r3
        vst1.16    {d2 }, [r0  :64]
        vst1.16    {d3 }, [r2  :64]

        bx          lr
endfunc


@ ff_hevc_rpi_pred_horizontal_8_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_horizontal_8_neon_10, export=1
        vld1.16    {q0 }, [r1  :128]    @ Up
        ldrh        r12, [r2, #-2]      @ Up-left
        vld1.16    {q8 }, [r2  :128]    @ left

        vdup.16     q2,  r12
        add         r2,  r0,  r3,  lsl #1
        vhsub.u16   q0,  q2

        vdup.16     q3,  d16[0]
        lsl         r3,  #2
        vmov.s16    q2,  #0
        vadd.i16    q0,  q3

        mov         r1,  #3
        vmov.s16    q3,  #0x3ff
        vmax.s16    q0,  q2
        vmin.s16    q0,  q3

        vdup.16     q2,  d16[1]

        vst1.16    {q0 }, [r0  :128], r3
        vst1.16    {q2 }, [r2  :128], r3
1:
        vext.16     q8,  q8,  #2
        vdup.16     q0,  d16[0]
        vdup.16     q2,  d16[1]
        subs        r1,  #1
        vst1.16    {q0 }, [r0  :128], r3
        vst1.16    {q2 }, [r2  :128], r3
        bne         1b

        bx          lr
endfunc


@ ff_hevc_rpi_pred_horizontalal_16_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_horizontal_16_neon_10, export=1
        vld1.16    {q0,  q1 }, [r1  :128]    @ Up
        ldrh        r12, [r2, #-2]           @ Up-left
        vld1.16    {q8,  q9 }, [r2  :128]    @ left


        vdup.16     q2,  r12
        add         r2,  r0,  r3,  lsl #1
        vhsub.u16   q0,  q2
        vhsub.u16   q1,  q2

        vdup.16     q3,  d16[0]
        lsl         r3,  #2
        vmov.s16    q2,  #0
        vadd.i16    q0,  q3
        vadd.i16    q1,  q3

        mov         r1,  #7
        vmov.s16    q3,  #0x3ff
        vmax.s16    q0,  q2
        vmax.s16    q1,  q2
        vmin.s16    q0,  q3
        vmin.s16    q1,  q3

        vdup.16     q2,  d16[1]
        vdup.16     q3,  d16[1]

        vst1.16    {q0,  q1 }, [r0  :128], r3
        vst1.16    {q2,  q3 }, [r2  :128], r3
1:
        vext.16     q8,  q9,  #2
        vext.16     q9,  q9,  #2
        vdup.16     q0,  d16[0]
        vdup.16     q1,  d16[0]
        vdup.16     q2,  d16[1]
        vdup.16     q3,  d16[1]
        subs        r1,  #1
        vst1.16    {q0,  q1 }, [r0  :128], r3
        vst1.16    {q2,  q3 }, [r2  :128], r3
        bne         1b

        bx          lr
endfunc


@ ff_hevc_rpi_pred_horizontal_32_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_horizontal_32_neon_10, export=1
        vldm        r2, { q8-q11}
        mov         r1,  #16
1:
        vdup.16     q0,  d16[0]
        vdup.16     q1,  d16[0]
        vdup.16     q2,  d16[0]
        vdup.16     q3,  d16[0]
        add         r2,  r0,  r3,  lsl #1
        vdup.16     q12, d16[1]
        vdup.16     q13, d16[1]
        vdup.16     q14, d16[1]
        vdup.16     q15, d16[1]
        vstm        r0, { q0-q3 }
        vstm        r2, {q12-q15}

        vext.16     q8,  q9,  #2
        vext.16     q9,  q10, #2
        add         r0,  r0,  r3,  lsl #2
        vext.16     q10, q11, #2
        subs        r1,  #1
        vext.16     q11, q11, #2

        bne         1b

        bx          lr
endfunc


@ ff_hevc_rpi_pred_horizontal_c_4_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_horizontal_c_4_neon_10, export=1
        vld1.16    {q8 }, [r2  :128]    @ Left
        add         r2,  r0,  r3,  lsl #2
        lsl         r3,  #3

        vdup.32     q0,  d16[0]
        vdup.32     q1,  d16[1]
        vdup.32     q2,  d17[0]
        vdup.32     q3,  d17[1]

        vst1.32    {q0 }, [r0  :128], r3
        vst1.16    {q1 }, [r2  :128], r3
        vst1.32    {q2 }, [r0  :128]
        vst1.16    {q3 }, [r2  :128]

        bx          lr
endfunc


@ ff_hevc_rpi_pred_horizontal_c_8_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_horizontal_c_8_neon_10, export=1
        vld1.16    {q8,  q9 }, [r2  :128]    @ Left
        add         r2,  r0,  r3,  lsl #2
        lsl         r3,  #3
        mov         r1,  #4
1:
        vdup.32     q0,  d16[0]
        vdup.32     q1,  d16[0]
        vdup.32     q2,  d16[1]
        vdup.32     q3,  d16[1]
        vext.32     q8,  q9,  #2
        vext.32     q9,  q9,  #2
        vst1.32    {q0,  q1 }, [r0  :128], r3
        subs        r1,  #1
        vst1.32    {q2,  q3 }, [r2  :128], r3
        bne         1b

        bx          lr
endfunc


@ ff_hevc_rpi_pred_horizontal_c_16_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_horizontal_c_16_neon_10, export=1
        vldm        r2, { q8-q11}
        mov         r1,  #8
1:
        vdup.32     q0,  d16[0]
        vdup.32     q1,  d16[0]
        vdup.32     q2,  d16[0]
        vdup.32     q3,  d16[0]
        add         r2,  r0,  r3,  lsl #2
        vdup.32     q12, d16[1]
        vdup.32     q13, d16[1]
        vdup.32     q14, d16[1]
        vdup.32     q15, d16[1]
        vstm        r0, { q0-q3 }
        vstm        r2, {q12-q15}

        vext.32     q8,  q9,  #2
        vext.32     q9,  q10, #2
        add         r0,  r0,  r3,  lsl #3
        vext.32     q10, q11, #2
        subs        r1,  #1
        vext.32     q11, q11, #2

        bne         1b

        bx          lr
endfunc



