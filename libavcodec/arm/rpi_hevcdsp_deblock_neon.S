/*
 * Copyright (c) 2014 Seppo Tomperi <seppo.tomperi@vtt.fi>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1
 */


#include "libavutil/arm/asm.S"
#include "neon.S"

.macro hevc_loop_filter_uv_body1 P1a, P0a, Q0a, Q1a
        vsubl.u8  q0, \Q0a, \P0a
        vsubl.u8  q2, \P1a, \Q1a
        vshl.i16  q0, #2
        vadd.i16  q0, q2
        vdup.16   d4, r2

        vrshr.s16 q0, #3
        vmovl.u8  q2, d4

        vmin.s16  q0, q2
        vneg.s16  q2, q2
        vmax.s16  q0, q2
        vaddw.u8  q2, q0, \P0a

        vqmovun.s16 \P0a, q2
        vmovl.u8  q2, \Q0a
        vsub.i16  q2, q0

        vqmovun.s16 \Q0a, q2
.endm


.macro hevc_loop_filter_uv_body2 P1u, P1v, P0u, P0v, Q0u, Q0v, Q1u, Q1v
        vsubl.u8  q0, \Q0u, \P0u
        vsubl.u8  q1, \Q0v, \P0v
        vsubl.u8  q2, \P1u, \Q1u
        vsubl.u8  q3, \P1v, \Q1v
        vshl.i16  q0, #2
        vshl.i16  q1, #2
        vadd.i16  q0, q2
        vdup.16   d4, r2
        lsr       r2, #16
        vadd.i16  q1, q3

        vrshr.s16 q0, #3
        vdup.16   d6, r2
        vmovl.u8  q2, d4
        vmovl.u8  q3, d6
        vrshr.s16 q1, #3

        vmin.s16  q0, q2
        vneg.s16  q2, q2
        vmin.s16  q1, q3
        vneg.s16  q3, q3
        vmax.s16  q0, q2
        vaddw.u8  q2, q0, \P0u
        vmax.s16  q1, q3
        vaddw.u8  q3, q1, \P0v

        vqmovun.s16 \P0u, q2
        vmovl.u8  q2, \Q0u
        vqmovun.s16 \P0v, q3
        vmovl.u8  q3, \Q0v
        vsub.i16  q2, q0
        vsub.i16  q3, q1

        vqmovun.s16 \Q0u, q2
        vqmovun.s16 \Q0v, q3
.endm


@ Preserves r12
@ Clobbers r2
@ P0a et al all contain UVUVUVUV
@ r2 (tc4) contains
@   [0..7]   tc U a
@   [8..15]  tc V a

.macro hevc_loop_filter_uv_body1_16 P1a, P0a, Q0a, Q1a, bit_depth
        vsub.i16  q0, \Q0a, \P0a
        vsub.i16  q2, \P1a, \Q1a
        vshl.i16  q0, #2
        vadd.i16  q0, q2
        vrshr.s16 q0, #3

        vdup.16   d4, r2
        vshll.u8  q2, d4, #\bit_depth - 8

        movw      r2, #(1 << \bit_depth) - 1
        vmin.s16  q0, q2
        vneg.s16  q2, q2
        vmax.s16  q0, q2
        vmov.i64  q2, #0
        vdup.i16  q3, r2
        vadd.i16  \P0a, q0
        vsub.i16  \Q0a, q0

        vmax.s16  \P0a, q2
        vmax.s16  \Q0a, q2
        vmin.s16  \P0a, q3
        vmin.s16  \Q0a, q3
.endm

@ Preserves r12
@ Clobbers r2
@ P0a et al all contain UVUVUVUV
@ r2 (tc4) contains
@   [0..7]   tc U a
@   [8..15]  tc V a
@  [16..23]  tc U b
@  [24..31]  tc V b

.macro hevc_loop_filter_uv_body2_16 P1a, P1b, P0a, P0b, Q0a, Q0b, Q1a, Q1b, bit_depth
        vsub.i16  q0, \Q0a, \P0a
        vsub.i16  q1, \Q0b, \P0b
        vsub.i16  q2, \P1a, \Q1a
        vsub.i16  q3, \P1b, \Q1b
        vshl.i16  q0, #2
        vshl.i16  q1, #2
        vadd.i16  q0, q2
        vrshr.s16 q0, #3
        vadd.i16  q1, q3
        vrshr.s16 q1, #3

        vdup.16   d4, r2
        lsr       r2, #16
        vdup.16   d6, r2
        vshll.u8  q2, d4, #\bit_depth - 8
        vshll.u8  q3, d6, #\bit_depth - 8

        movw      r2, #(1 << \bit_depth) - 1
        vmin.s16  q0, q2
        vneg.s16  q2, q2
        vmin.s16  q1, q3
        vneg.s16  q3, q3
        vmax.s16  q0, q2
        vmov.i64  q2, #0
        vmax.s16  q1, q3
        vdup.i16  q3, r2
        vadd.i16  \P0a, q0
        vsub.i16  \Q0a, q0
        vadd.i16  \P0b, q1
        vsub.i16  \Q0b, q1

        vmax.s16  \P0a, q2
        vmax.s16  \Q0a, q2
        vmax.s16  \P0b, q2
        vmax.s16  \Q0b, q2
        vmin.s16  \P0a, q3
        vmin.s16  \Q0a, q3
        vmin.s16  \P0b, q3
        vmin.s16  \Q0b, q3
.endm



@   uint8_t *_no_p,     [sp+0]
@   uint8_t *_no_q)     [sp+4]

.macro hevc_loop_filter_luma_start
        ldr     r12, [r3]
        ldr      r3, [r3, #4]
        orrs     r3, r12, r3, lsl #16
        it       eq
        bxeq     lr
        push     {r4-r10,lr}            @ 32 bytes
        ldr      r5, [sp, #32]          @ &_no_p
        ldrb     r10, [r5]
        ldr      r5, [sp, #36]          @ &_no_q
        ldrb     r5, [r5]
        cmp      r10, #0
        it ne
        movne    r10, #1
        cmp      r5, #0
        it ne
        orrne    r10, #2
.endm

@ Input:
@  r2          beta    (raw: needs shift for bitdepth > 8)
@  r3[ 0:15]   tc[0]   (raw: needs shift for bitdepth > 8)
@  r3[16:31]   tc[1]   (raw: needs shift for bitdepth > 8)
@
@ Input & output
@  8-bit: d16-d23      (Q3,Q2,Q1,Q0,P0,P1,P2,P3)
@ 16-bit:  q8-q15
@
@  r1         -r1
@  r10        b1->C, b0->N  (r10 junk)
@
@ Junks:
@  r5, r6, r7, r8, r9

.macro m_filter_luma bit_depth
.if \bit_depth == 8
        vmovl.u8  q15, d23
        vmovl.u8  q14, d22
        vmovl.u8  q13, d21
        vmovl.u8  q12, d20
        vmovl.u8  q11, d19
        vmovl.u8  q10, d18
        vmovl.u8  q9, d17
        vmovl.u8  q8, d16
.endif
        vadd.i16   q7, q9, q11
.if \bit_depth > 8
        lsl        r2, r2, #(\bit_depth - 8)
.endif
        vadd.i16   q6, q14, q12
.if \bit_depth > 8
        lsl        r3, r3, #(\bit_depth - 8)
.endif
        vsub.i16   q7, q10
        vsub.i16   q6, q13
        vabd.s16   q7, q7, q10
        vabd.s16   q6, q6, q13

        vdup.16    q0, r2
        vmov       q4, q7
        vmov       q5, q6
        vdup.16    d4, r3
        lsr        r3, r3, #16
        vtrn.16    q7, q4
        vtrn.16    q6, q5

        vshl.u64   q7, #32
        vshr.u64   q4, #32
        vshl.u64   q6, #32
        vshr.u64   q5, #32
        vshr.u64   q7, #32
        vshr.u64   q6, #32
        vshl.u64   q5, #32
        vshl.u64   q4, #32
        vorr       q6, q5
        vorr       q7, q4
        vdup.16    d5, r3
        vadd.i16   q5, q7, q6

        vmov       q4, q5
        vmov       q3, q5
        vtrn.32    q3, q4

        vadd.i16   q4, q3

        vshl.s16   q5, q5, #1
        vcgt.s16   q3, q0, q4

        vmovn.i16  d6, q3
        vshr.s16   q1, q0, #2
        vmovn.i16  d6, q3
        vcgt.s16   q5, q1, q5
        vmov       r7, s12
        cmp        r7, #0
        beq        .Lbypasswrite

        vpadd.i32  d0, d14, d12
        vpadd.i32  d1, d15, d13
        vmov       q4, q2
        vshl.s16   q2, #2
        vshr.s16   q1, q1, #1
        vrhadd.s16 q2, q4

        vabd.s16   q7, q8, q11
        vaba.s16   q7, q15, q12

        vmovn.i32  d0, q0
        vmov       r5, r6, s0, s1
        vcgt.s16   q6, q1, q7
        vand       q5, q5, q6
        vabd.s16   q7, q11, q12
        vcgt.s16   q6, q2, q7
        vand       q5, q5, q6

        vmov       q2, q5
        vtrn.s16   q5, q2
        vshr.u64   q2, #32
        vshl.u64   q5, #32
        vshl.u64   q2, #32
        vshr.u64   q5, #32
        vorr       q5, q2

        vmov       q2, q5
        vshl.i16   q7, q4, #1
        vtrn.32    q2, q5
        vand       q5, q2
        vneg.s16   q6, q7
        vmovn.i16  d4, q5
        vmovn.i16  d4, q2
        vmov       r8, s8

        and        r9, r8, r7
        cmp        r9, #0
        beq        1f

        vadd.i16  q2, q11, q12
        vadd.i16  q4, q9, q8
        vadd.i16  q1, q2, q10
        vdup.16   d10, r9
        vadd.i16  q0, q1, q9
        vshl.i16  q4, #1
        lsr        r9, #16
        vadd.i16  q1, q0
        vrshr.s16 q3, q0, #2
        vadd.i16  q1, q13
        vadd.i16  q4, q0
        vsub.i16  q3, q10
        vrshr.s16 q1, #3
        vrshr.s16 q4, #3
        vmax.s16  q3, q6
        vsub.i16  q1, q11
        vsub.i16  q4, q9
        vmin.s16  q3, q7
        vmax.s16  q4, q6
        vmax.s16  q1, q6
        vadd.i16  q3, q10
        vmin.s16  q4, q7
        vmin.s16  q1, q7
        vdup.16   d11, r9
        vadd.i16  q4, q9
        vadd.i16  q1, q11
        vbit      q9, q4, q5
        vadd.i16  q4, q2, q13
        vbit      q11, q1, q5
        vadd.i16  q0, q4, q14
        vadd.i16  q2, q15, q14
        vadd.i16  q4, q0

        vshl.i16  q2, #1
        vadd.i16  q4, q10
        vbit      q10, q3, q5
        vrshr.s16 q4, #3
        vadd.i16  q2, q0
        vrshr.s16 q3, q0, #2
        vsub.i16  q4, q12
        vrshr.s16 q2, #3
        vsub.i16  q3, q13
        vmax.s16  q4, q6
        vsub.i16  q2, q14
        vmax.s16  q3, q6
        vmin.s16  q4, q7
        vmax.s16  q2, q6
        vmin.s16  q3, q7
        vadd.i16  q4, q12
        vmin.s16  q2, q7
        vadd.i16  q3, q13
        vbit      q12, q4, q5
        vadd.i16  q2, q14
        vbit      q13, q3, q5
        vbit      q14, q2, q5

1:
        mvn       r8, r8
        and       r9, r8, r7
        cmp       r9, #0
        beq       2f

        vdup.16    q4, r2

        vdup.16   d10, r9
        lsr       r9, #16
        vmov       q1, q4
        vdup.16   d11, r9
        vshr.s16   q1, #1
        vsub.i16  q2, q12, q11
        vadd.i16   q4, q1
        vshl.s16  q0, q2, #3
        vshr.s16   q4, #3
        vadd.i16  q2, q0
        vsub.i16  q0, q13, q10
        vsub.i16  q2, q0
        vshl.i16  q0, q0, #1
        vsub.i16  q2, q0
        vshl.s16  q1, q7, 2
        vrshr.s16 q2, q2, #4
        vadd.i16  q1, q7
        vabs.s16  q3, q2
        vshr.s16  q6, q6, #1
        vcgt.s16  q1, q1, q3
        vand      q5, q1
        vshr.s16  q7, q7, #1
        vmax.s16  q2, q2, q6
        vmin.s16  q2, q2, q7

        vshr.s16  q7, q7, #1
        vrhadd.s16 q3, q9, q11
        vneg.s16  q6, q7
        vsub.s16  q3, q10
        vdup.16   d2, r5
        vhadd.s16 q3, q2
        vdup.16   d3, r6
        vmax.s16  q3, q3, q6
        vcgt.s16  q1, q4, q1
        vmin.s16  q3, q3, q7
        vand      q1, q5
        vadd.i16  q3, q10
        lsr       r5, #16
        lsr       r6, #16
        vbit      q10, q3, q1

        vrhadd.s16 q3, q14, q12
        vdup.16   d2, r5
        vsub.s16  q3, q13
        vdup.16   d3, r6
        vhsub.s16 q3, q2
        vcgt.s16  q1, q4, q1
        vmax.s16  q3, q3, q6
        vand      q1, q5
        vmin.s16  q3, q3, q7
        vadd.i16  q3, q13
        vbit      q13, q3, q1
        vadd.i16  q0, q11, q2
        vsub.i16  q4, q12, q2
        vbit      q11, q0, q5
        vbit      q12, q4, q5

2:
.if \bit_depth == 8
        neg       r1, r1
        vqmovun.s16 d16, q8
        vqmovun.s16 d17, q9
        vqmovun.s16 d18, q10
        vqmovun.s16 d19, q11
        lsls      r10, #31
        vqmovun.s16 d20, q12
        vqmovun.s16 d21, q13
        vqmovun.s16 d22, q14
        vqmovun.s16 d23, q15
.else
        movw      r5, #(1 << \bit_depth - 1)
        vmov.i64  q0, #0
        vdup.i16  q1, r5
        @ q8 & q15 should be unaltered and so don't require clipping
        neg       r1, r1
        vmax.s16  q9,  q0
        vmax.s16  q10, q0
        vmax.s16  q11, q0
        vmax.s16  q12, q0
        vmax.s16  q13, q0
        vmax.s16  q14, q0
        lsls      r10, #31
        vmin.s16  q9,  q1
        vmin.s16  q10, q1
        vmin.s16  q11, q1
        vmin.s16  q12, q1
        vmin.s16  q13, q1
        vmin.s16  q14, q1
.endif
        mov       pc, lr
.endm

function hevc_loop_filter_luma_body
        m_filter_luma 8
endfunc

@ void ff_hevc_rpi_v_loop_filter_luma_neon(
@   uint8_t *_pix,      [r0]
@   ptrdiff_t _stride,  [r1]
@   int _beta,          [r2]
@   int *_tc,           [r3]
@   uint8_t *_no_p,     [sp+0]
@   uint8_t *_no_q)     [sp+4]

function ff_hevc_rpi_v_loop_filter_luma_neon, export=1
        hevc_loop_filter_luma_start

        sub      r4, r0, #4
        b        .Lv_loop_luma_common
endfunc

@ void ff_hevc_rpi_v_loop_filter2_luma_neon(
@   uint8_t * pix_r,    [r0]
@   ptrdiff_t _stride,  [r1]
@   int _beta,          [r2]
@   int tc2,            [r3]
@   int no_f,           [sp+0]
@   uint8_t * pix_l)    [sp+4]

function ff_hevc_rpi_v_loop_filter_luma2_neon_8, export=1
        cmp      r3, #0
        it       eq
        bxeq     lr
        push     {r4-r10,lr}            @ 32 bytes
        ldr      r4, [sp, #36]
        ldr      r10, [sp, #32]

.Lv_loop_luma_common:
        vpush    {d8-d15}

        @ Uses slightly fewer instructions to do laned loads than unlaned
        @ and transpose.  This also means that we can use the same code for
        @ both split & unsplit deblock
        vld4.8  {d16[0],d17[0],d18[0],d19[0]}, [r4:32], r1
        vld4.8  {d20[0],d21[0],d22[0],d23[0]}, [r0:32], r1

        vld4.8  {d16[1],d17[1],d18[1],d19[1]}, [r4:32], r1
        vld4.8  {d20[1],d21[1],d22[1],d23[1]}, [r0:32], r1

        vld4.8  {d16[2],d17[2],d18[2],d19[2]}, [r4:32], r1
        vld4.8  {d20[2],d21[2],d22[2],d23[2]}, [r0:32], r1

        vld4.8  {d16[3],d17[3],d18[3],d19[3]}, [r4:32], r1
        vld4.8  {d20[3],d21[3],d22[3],d23[3]}, [r0:32], r1

        vld4.8  {d16[4],d17[4],d18[4],d19[4]}, [r4:32], r1
        vld4.8  {d20[4],d21[4],d22[4],d23[4]}, [r0:32], r1

        vld4.8  {d16[5],d17[5],d18[5],d19[5]}, [r4:32], r1
        vld4.8  {d20[5],d21[5],d22[5],d23[5]}, [r0:32], r1

        vld4.8  {d16[6],d17[6],d18[6],d19[6]}, [r4:32], r1
        vld4.8  {d20[6],d21[6],d22[6],d23[6]}, [r0:32], r1

        vld4.8  {d16[7],d17[7],d18[7],d19[7]}, [r4:32]
        vld4.8  {d20[7],d21[7],d22[7],d23[7]}, [r0:32]

        bl hevc_loop_filter_luma_body

        @ no_p[1]
        bmi     1f
        vst4.8  {d16[7],d17[7],d18[7],d19[7]}, [r4:32], r1
        vst4.8  {d16[6],d17[6],d18[6],d19[6]}, [r4:32], r1
        vst4.8  {d16[5],d17[5],d18[5],d19[5]}, [r4:32], r1
        vst4.8  {d16[4],d17[4],d18[4],d19[4]}, [r4:32], r1

        vst4.8  {d16[3],d17[3],d18[3],d19[3]}, [r4:32], r1
        vst4.8  {d16[2],d17[2],d18[2],d19[2]}, [r4:32], r1
        vst4.8  {d16[1],d17[1],d18[1],d19[1]}, [r4:32], r1
        vst4.8  {d16[0],d17[0],d18[0],d19[0]}, [r4:32]
1:
        @ no_q[1]
@        tst     r10, #2
        bcs     1f
        vst4.8  {d20[7],d21[7],d22[7],d23[7]}, [r0:32], r1
        vst4.8  {d20[6],d21[6],d22[6],d23[6]}, [r0:32], r1
        vst4.8  {d20[5],d21[5],d22[5],d23[5]}, [r0:32], r1
        vst4.8  {d20[4],d21[4],d22[4],d23[4]}, [r0:32], r1

        vst4.8  {d20[3],d21[3],d22[3],d23[3]}, [r0:32], r1
        vst4.8  {d20[2],d21[2],d22[2],d23[2]}, [r0:32], r1
        vst4.8  {d20[1],d21[1],d22[1],d23[1]}, [r0:32], r1
        vst4.8  {d20[0],d21[0],d22[0],d23[0]}, [r0:32]
1:
.Lbypasswrite:
        vpop     {d8-d15}
        pop      {r4-r10,pc}
endfunc

.macro m_filter_v_luma_common_16 bit_depth
        vpush    {d8-d15}

        @ Uses slightly fewer instructions to do laned loads than unlaned
        @ and transpose.  This also means that we can use the same code for
        @ both split & unsplit deblock
        vld4.16  {d16[0], d18[0], d20[0], d22[0]}, [r4], r1
        vld4.16  {d24[0], d26[0], d28[0], d30[0]}, [r0], r1

        vld4.16  {d16[1], d18[1], d20[1], d22[1]}, [r4], r1
        vld4.16  {d24[1], d26[1], d28[1], d30[1]}, [r0], r1

        vld4.16  {d16[2], d18[2], d20[2], d22[2]}, [r4], r1
        vld4.16  {d24[2], d26[2], d28[2], d30[2]}, [r0], r1

        vld4.16  {d16[3], d18[3], d20[3], d22[3]}, [r4], r1
        vld4.16  {d24[3], d26[3], d28[3], d30[3]}, [r0], r1

        vld4.16  {d17[0], d19[0], d21[0], d23[0]}, [r4], r1
        vld4.16  {d25[0], d27[0], d29[0], d31[0]}, [r0], r1

        vld4.16  {d17[1], d19[1], d21[1], d23[1]}, [r4], r1
        vld4.16  {d25[1], d27[1], d29[1], d31[1]}, [r0], r1

        vld4.16  {d17[2], d19[2], d21[2], d23[2]}, [r4], r1
        vld4.16  {d25[2], d27[2], d29[2], d31[2]}, [r0], r1

        vld4.16  {d17[3], d19[3], d21[3], d23[3]}, [r4]
        vld4.16  {d25[3], d27[3], d29[3], d31[3]}, [r0]

        bl hevc_loop_filter_luma_body_\bit_depth

        @ p[1]
        bmi      1f
        vst4.16  {d17[3], d19[3], d21[3], d23[3]}, [r4], r1
        vst4.16  {d17[2], d19[2], d21[2], d23[2]}, [r4], r1
        vst4.16  {d17[1], d19[1], d21[1], d23[1]}, [r4], r1
        vst4.16  {d17[0], d19[0], d21[0], d23[0]}, [r4], r1
        vst4.16  {d16[3], d18[3], d20[3], d22[3]}, [r4], r1
        vst4.16  {d16[2], d18[2], d20[2], d22[2]}, [r4], r1
        vst4.16  {d16[1], d18[1], d20[1], d22[1]}, [r4], r1
        vst4.16  {d16[0], d18[0], d20[0], d22[0]}, [r4]
1:
        @ q[1]
        bcs      1f
        vst4.16  {d25[3], d27[3], d29[3], d31[3]}, [r0], r1
        vst4.16  {d25[2], d27[2], d29[2], d31[2]}, [r0], r1
        vst4.16  {d25[1], d27[1], d29[1], d31[1]}, [r0], r1
        vst4.16  {d25[0], d27[0], d29[0], d31[0]}, [r0], r1
        vst4.16  {d24[3], d26[3], d28[3], d30[3]}, [r0], r1
        vst4.16  {d24[2], d26[2], d28[2], d30[2]}, [r0], r1
        vst4.16  {d24[1], d26[1], d28[1], d30[1]}, [r0], r1
        vst4.16  {d24[0], d26[0], d28[0], d30[0]}, [r0]
1:
        vpop     {d8-d15}
        pop      {r4-r10,pc}
.endm




@ void (*hevc_h_loop_filter_luma)(uint8_t *pix,     [r0]
@                                 ptrdiff_t stride, [r1]
@                                 int beta,         [r2]
@                                 int32_t *tc,      [r3]
@                                 uint8_t *no_p,    sp[0]
@                                 uint8_t *no_q);   sp[4]
@
@ Src should always be on 8 byte boundry & all in the same slice

function ff_hevc_rpi_h_loop_filter_luma_neon, export=1
        hevc_loop_filter_luma_start
        b        .Lh_loop_filter_luma_common_8
endfunc

function ff_hevc_rpi_h_loop_filter_luma2_neon_8, export=1
        cmp      r3, #0
        it       eq
        bxeq     lr
        push     {r4-r10,lr}            @ 32 bytes
        ldr      r10, [sp, #32]

.Lh_loop_filter_luma_common_8:
        vpush    {d8-d15}
        sub      r0, r0, r1, lsl #2

        vld1.8  {d16}, [r0], r1
        vld1.8  {d17}, [r0], r1
        vld1.8  {d18}, [r0], r1
        vld1.8  {d19}, [r0], r1
        vld1.8  {d20}, [r0], r1
        vld1.8  {d21}, [r0], r1
        vld1.8  {d22}, [r0], r1
        vld1.8  {d23}, [r0]

        bl hevc_loop_filter_luma_body

        add      r2, r0, r1, lsl #2
        add      r0, r0, r1

        vpop     {d8-d15}

        @ P2-P0
        bcs      1f
        vst1.8   {d22}, [r0], r1
        vst1.8   {d21}, [r0], r1
        vst1.8   {d20}, [r0]
1:
        @ Q0-Q2
        bmi      1f
        vst1.8   {d19}, [r2], r1
        vst1.8   {d18}, [r2], r1
        vst1.8   {d17}, [r2]
1:
        pop      {r4-r10,pc}
endfunc


.macro m_filter_h_luma_16 bit_depth
        vpush    {d8-d15}
        sub      r0, r0, r1, lsl #2

        vld1.16 { q8}, [r0], r1
        vld1.16 { q9}, [r0], r1
        vld1.16 {q10}, [r0], r1
        vld1.16 {q11}, [r0], r1
        vld1.16 {q12}, [r0], r1
        vld1.16 {q13}, [r0], r1
        vld1.16 {q14}, [r0], r1
        vld1.16 {q15}, [r0]

        bl hevc_loop_filter_luma_body_\bit_depth

        add      r2, r0, r1, lsl #2
        add      r0, r1

        vpop     {d8-d15}

        @ P2-P0
        bcs      1f
        vst1.16  {q14}, [r0], r1
        vst1.16  {q13}, [r0], r1
        vst1.16  {q12}, [r0]
1:
        bmi      1f
        vst1.16  {q11}, [r2], r1
        vst1.16  {q10}, [r2], r1
        vst1.16  { q9}, [r2]
1:
        pop      {r4-r10,pc}
.endm


@ void ff_hevc_rpi_h_loop_filter_uv_neon(uint8_t * src_r,        // r0
@                                     unsigned int stride,   // r1
@                                     uint32_t tc4,          // r2
@                                     unsigned int no_f);    // r3
@
@ no_f
@ 0  tl P0
@ 1  tr P1
@ 2  bl Q0
@ 3  br Q1
@
@ Probably not worth having the P/Qa only special case in this direction
@ Given layout we won't save any memory reads or avoid any cache dirtying
@ We would save a bit of computation but I expect the partials to be less
@ common in the H direction than V due to how we arrange deblock.

function ff_hevc_rpi_h_loop_filter_uv_neon_8, export=1
        cmp      r2, #0
        bxeq     lr
        sub      r0, r0, r1, lsl #1
        vld1.8   {d16,d17}, [r0], r1
        vld1.8   {d18,d19}, [r0], r1
        vld1.8   {d26,d27}, [r0], r1
        vld1.8   {d28,d29}, [r0]
        sub      r0, r0, r1, lsl #1
        hevc_loop_filter_uv_body2 d16, d17, d18, d19, d26, d27, d28, d29

        lsls     r2, r3, #31            @ b0 -> N, b1 -> C
        vstrpl   d18, [r0, #0]
        vstrcc   d19, [r0, #8]
        add      r0, r1
        lsls     r3, #29                @ b2 -> N, b3 -> C
        vstrpl   d26, [r0, #0]
        vstrcc   d27, [r0, #8]
        bx       lr

endfunc


@ void ff_hevc_rpi_h_loop_filter_uv_neon_10(uint8_t * src_r,     // r0
@                                     unsigned int stride,   // r1
@                                     uint32_t tc4,          // r2
@                                     unsigned int no_f);    // r3
@
@ no-F = b0:no_p[0], b1:no_p[1], b2:no_q[0], b3:no_q[1]
@
@ Macro here actual function near bottom

.macro m_filter_h_uv_16 bit_depth
        cmp      r2, #0
        bxeq     lr
        sub      r0, r0, r1, lsl #1
        vld1.16  {q8,  q9 }, [r0], r1
        vld1.16  {q10, q11}, [r0], r1
        vld1.16  {q12, q13}, [r0], r1
        vld1.16  {q14, q15}, [r0]
        sub      r0, r0, r1, lsl #1

        hevc_loop_filter_uv_body2_16 q8, q9, q10, q11, q12, q13, q14, q15, \bit_depth

        cmp      r3, #0
        bne      1f
        vst1.16  {q10, q11}, [r0], r1
        vst1.16  {q12, q13}, [r0]
        bx       lr

        @ At least one no_f bit is set
        @ Which means we need to break this apart in an ugly fashion
1:
        lsls     r2, r3, #31            @ b0 -> N, b1 -> C
        vstrpl   d20, [r0, #0]
        vstrpl   d21, [r0, #8]
        vstrcc   d22, [r0, #16]
        vstrcc   d23, [r0, #24]
        add      r0, r1
        lsls     r3, #29                @ b2 -> N, b3 -> C
        vstrpl   d24, [r0, #0]
        vstrpl   d25, [r0, #8]
        vstrcc   d26, [r0, #16]
        vstrcc   d27, [r0, #24]
        bx       lr
.endm


@ void ff_hevc_rpi_v_loop_filter_uv2_neon(uint8_t * src_r,       // r0
@                                     unsigned int stride,   // r1
@                                     uint32_t tc4,          // r2
@                                     uint8_t * src_l,       // r3
@                                     unsigned int no_f);   // sp[0]
@
@ no_f:
@ 0  tl P0
@ 1  tr Q0
@ 2  bl P1
@ 3  br Q1

function ff_hevc_rpi_v_loop_filter_uv2_neon_8, export=1
        cmp      r2, #0
        bxeq     lr
        vld2.16  {d16[0], d18[0]}, [r3], r1
        vld2.16  {d20[0], d22[0]}, [r0], r1

        cmp      r2, #0x10000
        vld2.16  {d16[1], d18[1]}, [r3], r1
        vld2.16  {d20[1], d22[1]}, [r0], r1

        vld2.16  {d16[2], d18[2]}, [r3], r1
        vld2.16  {d20[2], d22[2]}, [r0], r1

        vld2.16  {d16[3], d18[3]}, [r3], r1
        vld2.16  {d20[3], d22[3]}, [r0], r1
        blo      10f

        sub      r12, r0, r3
        vld2.16  {d17[0], d19[0]}, [r3], r1
        vld2.16  {d21[0], d23[0]}, [r0], r1

        cmp      r12, #4
        vld2.16  {d17[1], d19[1]}, [r3], r1
        vld2.16  {d21[1], d23[1]}, [r0], r1

        vld2.16  {d17[2], d19[2]}, [r3], r1
        vld2.16  {d21[2], d23[2]}, [r0], r1

        vld2.16  {d17[3], d19[3]}, [r3]
        vld2.16  {d21[3], d23[3]}, [r0]
        it eq
        ldreq    r12, [sp, #0]

        hevc_loop_filter_uv_body2 d16, d17, d18, d19, d20, d21, d22, d23
        cmp      r12, #0
        add      r3, #2
        neg      r1, r1
        bne      1f

@ Much/most of the time r0 == r3 + 4 and no_f == 0
@ so it is worth having this special case
        vst2.16   {d19[3], d21[3]}, [r3], r1    @ P0b, Q0b
        vst2.16   {d19[2], d21[2]}, [r3], r1
        vst2.16   {d19[1], d21[1]}, [r3], r1
        vst2.16   {d19[0], d21[0]}, [r3], r1
        vst2.16   {d18[3], d20[3]}, [r3], r1    @ P0a, Q0a
        vst2.16   {d18[2], d20[2]}, [r3], r1
        vst2.16   {d18[1], d20[1]}, [r3], r1
        vst2.16   {d18[0], d20[0]}, [r3]
        bx       lr

@ Either split or partial
1:
        ldr      r12, [sp, #0]
        @ I have no idea if this is faster than any of the other ways of
        @ testing these bits but it does free up r12
        lsl      r12, #28
        add      r2, r0, r1, lsl #2
        msr      APSR_nzcvq, r12        @ b0 (P0a) -> V, b1 (Q0a) -> C, b2 (P0b) -> Z, b3 (Q0b) -> N
        add      r12, r3, r1, lsl #2
        bmi      1f
        @ Q0b
        vst1.16  {d21[3]}, [r0], r1
        vst1.16  {d21[2]}, [r0], r1
        vst1.16  {d21[1]}, [r0], r1
        vst1.16  {d21[0]}, [r0]
1:
        beq      2f
        @ P0b
        vst1.16  {d19[3]}, [r3], r1
        vst1.16  {d19[2]}, [r3], r1
        vst1.16  {d19[1]}, [r3], r1
        vst1.16  {d19[0]}, [r3]

2:
        bcs      3f
        @ Q0a
        vst1.16  {d20[3]}, [r2], r1
        vst1.16  {d20[2]}, [r2], r1
        vst1.16  {d20[1]}, [r2], r1
        vst1.16  {d20[0]}, [r2]

3:
        it vs
        bxvs     lr
        vst1.16  {d18[3]}, [r12], r1
        vst1.16  {d18[2]}, [r12], r1
        vst1.16  {d18[1]}, [r12], r1
        vst1.16  {d18[0]}, [r12]
        bx       lr

@ Single lump (rather than double)
10:
        hevc_loop_filter_uv_body1 d16, d18, d20, d22

        @ As we have post inced r0/r3 in the load the easiest thing to do is
        @ to subtract and write forwards, rather than backwards (as above)
        ldr      r12, [sp, #0]
        add      r3, #2
        sub      r0, r0, r1, lsl #2
        sub      r3, r3, r1, lsl #2
        lsls     r12, #31               @ b0 (P0a) -> N, b1 (Q0a) -> C

        bcs      3f
        vst1.16  {d20[0]}, [r0], r1
        vst1.16  {d20[1]}, [r0], r1
        vst1.16  {d20[2]}, [r0], r1
        vst1.16  {d20[3]}, [r0]

3:
        it mi
        bxmi     lr
        vst1.16  {d18[0]}, [r3], r1
        vst1.16  {d18[1]}, [r3], r1
        vst1.16  {d18[2]}, [r3], r1
        vst1.16  {d18[3]}, [r3]
        bx       lr

endfunc


@ void ff_hevc_rpi_v_loop_filter_uv2_neon(uint8_t * src_r,       // r0
@                                     unsigned int stride,   // r1
@                                     uint32_t tc4,          // r2
@                                     uint8_t * src_l,       // r3
@                                     unsigned int no_f);   // sp[0]
@

@ no_f
@ 0  tl P0a
@ 1  tr Q0a
@ 2  bl P0b
@ 3  br Q0b

@ P1: q8,  q12
@ P0: q9,  q13
@ Q0: q10, q14
@ Q1: q11, q15

.macro m_filter_v_uv2_16 bit_depth
        cmp      r2, #0
        bxeq     lr

        vld2.32  {d16[0], d18[0]}, [r3], r1
        vld2.32  {d20[0], d22[0]}, [r0], r1

        vld2.32  {d16[1], d18[1]}, [r3], r1
        vld2.32  {d20[1], d22[1]}, [r0], r1

        cmp      r2, #0x10000
        vld2.32  {d17[0], d19[0]}, [r3], r1
        vld2.32  {d21[0], d23[0]}, [r0], r1

        vld2.32  {d17[1], d19[1]}, [r3], r1
        vld2.32  {d21[1], d23[1]}, [r0], r1
        blo      10f

        vld2.32  {d24[0], d26[0]}, [r3], r1
        vld2.32  {d28[0], d30[0]}, [r0], r1

        vld2.32  {d24[1], d26[1]}, [r3], r1
        vld2.32  {d28[1], d30[1]}, [r0], r1
        sub      r12, r0, r3

        vld2.32  {d25[0], d27[0]}, [r3], r1
        vld2.32  {d29[0], d31[0]}, [r0], r1
        cmp      r12, #8

        vld2.32  {d25[1], d27[1]}, [r3]
        vld2.32  {d29[1], d31[1]}, [r0]
        it eq
        ldreq    r12, [sp, #0]

        hevc_loop_filter_uv_body2_16  q8, q12, q9, q13, q10, q14, q11, q15, \bit_depth
        cmp      r12, #0
        add      r3, #4
        neg      r1, r1
        bne      1f

@ Much/most of the time r0 == r3 + 4 and no_f == 0
@ so it is worth having this special case
        vst2.32  {d27[1], d29[1]}, [r3], r1
        vst2.32  {d27[0], d29[0]}, [r3], r1
        vst2.32  {d26[1], d28[1]}, [r3], r1
        vst2.32  {d26[0], d28[0]}, [r3], r1
        vst2.32  {d19[1], d21[1]}, [r3], r1
        vst2.32  {d19[0], d21[0]}, [r3], r1
        vst2.32  {d18[1], d20[1]}, [r3], r1
        vst2.32  {d18[0], d20[0]}, [r3]
        bx       lr

@ Either split or partial
1:
        ldr      r12, [sp, #0]
        lsls     r12, #29               @ b2 (P0b) -> N, b3 (Q0b) -> C
        bcs      1f
        @ Q0b
        mov      r2, r0
        vst1.32  {d29[1]}, [r2], r1
        vst1.32  {d29[0]}, [r2], r1
        vst1.32  {d28[1]}, [r2], r1
        vst1.32  {d28[0]}, [r2]
1:
        bmi      2f
        @ P0b
        mov      r2, r3
        vst1.32  {d27[1]}, [r2], r1
        vst1.32  {d27[0]}, [r2], r1
        vst1.32  {d26[1]}, [r2], r1
        vst1.32  {d26[0]}, [r2]

2:
        lsls     r12, #2                @ b0 (P0a) -> N, b1 (Q0a) -> C
        bcs      3f
        @ Q0a
        add      r0, r0, r1, lsl #2
        vst1.32  {d21[1]}, [r0], r1
        vst1.32  {d21[0]}, [r0], r1
        vst1.32  {d20[1]}, [r0], r1
        vst1.32  {d20[0]}, [r0]

3:
        it mi
        bxmi     lr
        @ P0a
        add      r3, r3, r1, lsl #2
        vst1.32  {d19[1]}, [r3], r1
        vst1.32  {d19[0]}, [r3], r1
        vst1.32  {d18[1]}, [r3], r1
        vst1.32  {d18[0]}, [r3]
        bx       lr


10:
        hevc_loop_filter_uv_body1_16  q8, q9, q10, q11, \bit_depth

        @ As we have post inced r0/r3 in the load the easiest thing to do is
        @ to subtract and write forwards, rather than backwards (as above)
        ldr      r12, [sp, #0]
        add      r3, #4
        sub      r0, r0, r1, lsl #2
        sub      r3, r3, r1, lsl #2
        lsls     r12, #31               @ b0 (P0a) -> N, b1 (Q0a) -> C

        bcs      3f
        @ Q0a
        vst1.32  {d20[0]}, [r0], r1
        vst1.32  {d20[1]}, [r0], r1
        vst1.32  {d21[0]}, [r0], r1
        vst1.32  {d21[1]}, [r0]

3:
        it mi
        bxmi     lr
        @ P0a
        vst1.32  {d18[0]}, [r3], r1
        vst1.32  {d18[1]}, [r3], r1
        vst1.32  {d19[0]}, [r3], r1
        vst1.32  {d19[1]}, [r3]
        bx       lr
.endm




/* ff_hevc_rpi_deblocking_boundary_strengths_neon(int pus, int dup, int in_i
 *                                            int *curr_rpl0, int *curr_
 *                                            MvField *curr, MvField *ne
 */
function ff_hevc_rpi_deblocking_boundary_strengths_neon, export=1
        add         ip, sp, #4*4
        push        {a2-a4,v1-v8,lr}
        ldmia       ip, {v5-v7}
1:      ldmdb       ip, {v1-v4}
        ldrsb       a3, [v5, #8]    @ curr->ref_idx
        ldrsb       v8, [v5, #9]
        ldrsb       ip, [v6, #8]    @ neigh->ref_idx
        ldrsb       lr, [v6, #9]
        ldr         v1, [v1, a3, lsl #2]
        ldrb        a3, [v5, #10]   @ curr->pred_flag
        ldr         v2, [v2, v8, lsl #2]
        ldrb        v8, [v6, #10]   @ neigh->pred_flag
        ldr         v3, [v3, ip, lsl #2]
        ldr         v4, [v4, lr, lsl #2]
        teq         a3, #3
        beq         20f
        teq         v8, #3
        beq         90f

        tst         a3, #1
        itee        ne
        ldrne       a3, [v5, #0]    @ curr->mv[0]
        ldreq       a3, [v5, #4]    @ curr->mv[1]
        moveq       v1, v2
        tst         v8, #1
        itee        ne
        ldrne       v8, [v6, #0]    @ neigh->mv[0]
        ldreq       v8, [v6, #4]    @ neigh->mv[1]
        moveq       v3, v4
        teq         v1, v3
        bne         10f
        ldr         lr, =0xFFFCFFFC
        ssub16      ip, v8, a3
        ssub16      a3, a3, v8
        sel         a3, a3, ip
        ands        a3, a3, lr
        @ drop through
10:     it          ne
        movne       a3, #1
11:     subs        a2, a2, #1
12:
A       strbhs      a3, [v7], a4
T       itt         hs
T       strbhs      a3, [v7]
T       addhs       v7, v7, a4
        subs        a2, a2, #1
        bhs         12b

        ldm         sp, {a2, a3}
        add         ip, sp, #16*4
        subs        a1, a1, #1
        add         v5, v5, a3
        add         v6, v6, a3
        bhi         1b
        pop         {a2-a4,v1-v8,pc}

20:     teq         v8, #3
        bne         10b

        teq         v1, v3
        it          eq
        teqeq       v2, v4
        bne         40f
        teq         v1, v2
        bne         30f

        ldrd        v1, v2, [v5]    @ curr->mv
        ldrd        v3, v4, [v6]    @ neigh->mv
        ldr         lr, =0xFFFCFFFC
        ssub16      ip, v3, v1
        ssub16      a3, v1, v3
        sel         a3, a3, ip
        ands        a3, a3, lr
        bne         25f
        ssub16      ip, v4, v2
        ssub16      a3, v2, v4
        sel         a3, a3, ip
        ands        a3, a3, lr
        beq         11b
        @ drop through
25:     ssub16      ip, v4, v1
        ssub16      a3, v1, v4
        sel         a3, a3, ip
        ands        a3, a3, lr
        bne         10b
        ssub16      ip, v3, v2
        ssub16      a3, v2, v3
        sel         a3, a3, ip
        ands        a3, a3, lr
        b           10b

30:     ldrd        v1, v2, [v5]    @ curr->mv
        ldrd        v3, v4, [v6]    @ neigh->mv
        ldr         lr, =0xFFFCFFFC
        ssub16      ip, v3, v1
        ssub16      a3, v1, v3
        sel         a3, a3, ip
        ands        a3, a3, lr
        bne         10b
        ssub16      ip, v4, v2
        ssub16      a3, v2, v4
        sel         a3, a3, ip
        ands        a3, a3, lr
        b           10b

40:     teq         v1, v4
        ite         eq
        teqeq       v2, v3
        bne         10b

        ldrd        v1, v2, [v5]    @ curr->mv
        ldrd        v3, v4, [v6]    @ neigh->mv
        ldr         lr, =0xFFFCFFFC
        b           25b

90:     mov         a3, #1
        b           11b
endfunc

@ =============================================================================
@
@ 10 bit

function hevc_loop_filter_luma_body_10
        m_filter_luma 10
endfunc

function ff_hevc_rpi_h_loop_filter_luma_neon_10, export=1
        hevc_loop_filter_luma_start
        b        .Lh_loop_luma_common_10
endfunc

function ff_hevc_rpi_h_loop_filter_luma2_neon_10, export=1
        cmp      r3, #0
        it       eq
        bxeq     lr
        push     {r4-r10,lr}            @ 32 bytes
        ldr      r10, [sp, #32]
.Lh_loop_luma_common_10:
        m_filter_h_luma_16 10
endfunc

function ff_hevc_rpi_v_loop_filter_luma_neon_10, export=1
        hevc_loop_filter_luma_start
        sub      r4, r0, #8
        b        .Lv_loop_luma_common_10
endfunc

function ff_hevc_rpi_v_loop_filter_luma2_neon_10, export=1
        cmp      r3, #0
        it       eq
        bxeq     lr
        push     {r4-r10,lr}            @ 32 bytes
        ldr      r4, [sp, #36]
        ldr      r10, [sp, #32]

.Lv_loop_luma_common_10:
        m_filter_v_luma_common_16 10
endfunc

function ff_hevc_rpi_h_loop_filter_uv_neon_10, export=1
        m_filter_h_uv_16 10
endfunc

function ff_hevc_rpi_v_loop_filter_uv2_neon_10, export=1
        m_filter_v_uv2_16 10
endfunc

