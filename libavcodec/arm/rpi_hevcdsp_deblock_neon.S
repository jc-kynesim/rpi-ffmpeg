/*
 * Copyright (c) 2014 Seppo Tomperi <seppo.tomperi@vtt.fi>
 * Copyright (C) 2018 John Cox, Ben Avison for Raspberry Pi (Trading)
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

.macro hevc_loop_filter_uv_body1 P1a, P0a, Q0a, Q1a, I1, I2, I3, I4, I5, I6, I7, I8
        vsubl.u8  q0, \Q0a, \P0a
        vsubl.u8  q1, \P1a, \Q1a
        vdup.16   d4, r2
        \I1
        vshl.i16  q0, #2
        \I2
        vadd.i16  q0, q1
        \I3
        vmovl.u8  q2, d4
        \I4
        vneg.s16  q1, q2
        \I5
        vrshr.s16 q0, #3
        \I6
        \I7
        \I8
        vmin.s16  q0, q2
        vmovl.u8  q2, \Q0a
        vmax.s16  q0, q1
        vaddw.u8  q1, q0, \P0a
        vsub.i16  q0, q2, q0
        vqmovun.s16 \P0a, q1
        vqmovun.s16 \Q0a, q0
.endm


.macro hevc_loop_filter_uv_body2 P1a, P1b, P0a, P0b, Q0a, Q0b, Q1a, Q1b, I1, I2, I3, I4, I5, I6, I7
        vsubl.u8  q0, \Q0a, \P0a  @ q0a - p0a
        lsr       r12, r2, #16
        vsubl.u8  q1, \Q0b, \P0b  @ q0b - p0b
        vsubl.u8  q2, \P1a, \Q1a  @ p1a - q1a
        vsubl.u8  q3, \P1b, \Q1b  @ p1b - q1b
        vshl.i16  q0, #2          @ (q0a - p0a) * 4
        vshl.i16  q1, #2          @ (q0b - p0b) * 4
        vadd.i16  q0, q2          @ ((q0a - p0a) * 4) + p1a - q1a
        vadd.i16  q1, q3          @ ((q0b - p0b) * 4) + p1b - q1b
        vdup.16   d4, r2          @ tc0a, tc0b
        vdup.16   d6, r12         @ tc1a, tc1b
        vrshr.s16 q0, #3          @ (((q0a - p0a) * 4) + p1a - q1a + 4) >> 3
        \I1
        vrshr.s16 q1, #3          @ (((q0b - p0b) * 4) + p1b - q1b + 4) >> 3
        \I2
        vmovl.u8  q2, d4          @ tc0a, tc0b
        \I3
        vmovl.u8  q3, d6          @ tc1a, tc1b
        \I4
        vmin.s16  q0, q2
        \I5
        vneg.s16  q2, q2          @ -tc0a, -tc0b
        \I6
        vmin.s16  q1, q3
        \I7
        vneg.s16  q3, q3          @ -tc1a, -tc1b
        vmax.s16  q0, q2          @ delta0a
        vmovl.u8  q2, \Q0a
        vmax.s16  q1, q3          @ delta0b
        vaddw.u8  q3, q0, \P0a    @ p0a + delta0a
        vsub.i16  q0, q2, q0      @ q0a - delta0a
        vmovl.u8  q2, \Q0b
        vsub.i16  q2, q1          @ q0b - delta0b
        vaddw.u8  q1, \P0b        @ p0b + delta0b
        vqmovun.s16 \Q0a, q0
        vqmovun.s16 \P0a, q3
        vqmovun.s16 \Q0b, q2
        vqmovun.s16 \P0b, q1
.endm


@ Preserves r12
@ Clobbers r2
@ P0a et al all contain UVUVUVUV
@ r2 (tc4) contains
@   [0..7]   tc U a
@   [8..15]  tc V a

.macro hevc_loop_filter_uv_body1_16 P1a, P0a, Q0a, Q1a, bit_depth, I1, I2, I3, I4, I5, I6, I7, I8
        vsub.i16  q0, \Q0a, \P0a
        vsub.i16  q1, \P1a, \Q1a
        vdup.16   d4, r2
        \I1
        vshl.i16  q0, #2
        \I2
        vadd.i16  q0, q1
        \I3
        vshll.u8  q2, d4, #\bit_depth - 8
        \I4
        vneg.s16  q1, q2
        \I5
        vrshr.s16 q0, #3
        \I6
        \I7
        \I8
        vmin.s16  q0, q2
        vmov.i16  q2, #0
        vmax.s16  q0, q1
        vadd.i16  \P0a, q0
        vsub.i16  \Q0a, q0
        vmov.i16  q1, #(1 << \bit_depth) - 1
        vmax.s16  \P0a, q2
        vmax.s16  \Q0a, q2
        vmin.s16  \P0a, q1
        vmin.s16  \Q0a, q1
.endm

@ Clobbers r2, r12
@ P0a et al all contain UVUVUVUV
@ r2 (tc4) contains
@   [0..7]   tc U a
@   [8..15]  tc V a
@  [16..23]  tc U b
@  [24..31]  tc V b

.macro hevc_loop_filter_uv_body2_16 P1a, P1b, P0a, P0b, Q0a, Q0b, Q1a, Q1b, bit_depth, I1, I2, I3, I4, I5, I6, I7
        vsub.i16  q0, \Q0a, \P0a  @ q0a - p0a
        lsr       r12, r2, #16
        vsub.i16  q1, \Q0b, \P0b  @ q0b - p0b
        vsub.i16  q2, \P1a, \Q1a  @ p1a - q1a
        vsub.i16  q3, \P1b, \Q1b  @ p1b - q1b
        vshl.i16  q0, #2          @ (q0a - p0a) * 4
        vshl.i16  q1, #2          @ (q0b - p0b) * 4
        vadd.i16  q0, q2          @ ((q0a - p0a) * 4) + p1a - q1a
        vadd.i16  q1, q3          @ ((q0b - p0b) * 4) + p1b - q1b
        vdup.16   d4, r2          @ tc0a, tc0b
        vdup.16   d6, r12         @ tc1a, tc1b
        vrshr.s16 q0, #3          @ (((q0a - p0a) * 4) + p1a - q1a + 4) >> 3
        \I1
        vrshr.s16 q1, #3          @ (((q0b - p0b) * 4) + p1b - q1b + 4) >> 3
        \I2
        vshll.u8  q2, d4, #\bit_depth - 8 @ tc0a, tc0b
        \I3
        vshll.u8  q3, d6, #\bit_depth - 8 @ tc1a, tc1b
        \I4
        vmin.s16  q0, q2
        \I5
        vneg.s16  q2, q2          @ -tc0a, -tc0b
        \I6
        vmin.s16  q1, q3
        \I7
        vneg.s16  q3, q3          @ -tc1a, -tc1b
        vmax.s16  q0, q2          @ delta0a
        vadd.i16  \P0a, q0        @ p0a + delta0a
        vsub.i16  \Q0a, q0        @ q0a - delta0a
        vmax.s16  q1, q3          @ delta0b
        vadd.i16  \P0b, q1        @ p0b + delta0b
        vsub.i16  \Q0b, q1        @ q0b - delta0b
        vmov.i16  q2, #0
        vmov.i16  q3, #(1 << \bit_depth) - 1
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
        ldrd     r4, r5, [sp, #32]      @ &_no_p
        ldrb     r4, [r4]
        ldrb     r5, [r5]
        movs     r10, r4
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

.macro m_filter_luma bit_depth, Q11, Q15
.if \bit_depth == 8
        vmovl.u8    q14, d22      @ q2,7 q2,6 ... q2,0 = TQ2' ... Q2' TQ2 ... Q2
        vmovl.u8    q13, d21      @ q1,7 q1,6 ... q1,0 = TQ1' ... Q1' TQ1 ... Q1
        vmovl.u8    q12, d20      @ q0,7 q0,6 ... q0,0 = TQ0' ... Q0' TQ0 ... Q0
        vmovl.u8    \Q11, d19     @ p0,7 p0,6 ... p0,0 = TP0' ... P0' TP0 ... P0
        vmovl.u8    q10, d18      @ p1,7 p1,6 ... p1,0 = TP1' ... P1' TP1 ... P1
        vmovl.u8    q9, d17       @ p2,7 p2,6 ... p2,0 = TP2' ... P2' TP2 ... P2
.endif
        vadd.i16    q0, q9, \Q11  @ P2 + P0
.if \bit_depth > 8
        lsl         r3, r3, #(\bit_depth - 8)
.endif
        vadd.i16    q1, q14, q12  @ Q2 + Q0
.if \bit_depth > 8
        lsl         r2, r2, #(\bit_depth - 8)
.endif
        vsub.i16    q0, q10       @ P2 - P1 + P0
        lsr         r5, r3, #16
        vsub.i16    q1, q13       @ Q2 - Q1 + Q0
.if \bit_depth == 8
        vmovl.u8    q8, d16       @ p3,7 p3,6 ... p3,0 = TP3' ... P3' TP3 ... P3
        vmovl.u8    \Q15, d23     @ q3,7 q3,6 ... q3,0 = TQ3' ... Q3' TQ3 ... Q3
.endif
        vabd.s16    q0, q10       @ dp0 = abs(P2 - 2 * P1 + P0)
        vabd.s16    q1, q13       @ dq0 = abs(Q2 - 2 * Q1 + Q0)
        vmov.i64    q2, #0xffffffff0000
        vbic        q0, q2        @ only dp0(') and dp3(')
        vbic        q1, q2        @ only dq0(') and dq3(')
        vsra.u64    q0, #16
        vsra.u64    q1, #16
        vdup.16     q3, r2        @ beta
        vdup.16     d14, r3       @ tC[0]
        vdup.16     d15, r5       @ tC[1]
        vabd.s16    q4, q8, \Q11  @ abs(TP3'-TP0' ... P3'-P0' TP3-TP0 ... P3-P0)
        vmovn.i32   d0, q0        @ dp3' dp0' dp3 dp0
        vmovn.i32   d1, q1        @ dq3' dq0' dq3 dq0
        vadd.i16    d5, d0, d1    @ d3'=dp3'+dq3' d0'=dp0'+dq0' d3=dp3+dq3 d0=dp0+dq0
        vabd.s16    q5, \Q11, q12 @ abs(TP0'-TQ0' ... P0'-Q0' TP0-TQ0 ... P0-Q0)
        vaba.s16    q4, \Q15, q12 @ +abs(TQ3'-TQ0' ... Q3'-Q0' TQ3-TQ0 ... Q3-Q0)
        vpadd.i16   d2, d5, d5    @ dontcare dontcare d0'+d3' d0+d3
        vshl.s16    q6, q7, #2    @ tC[] * 4
        vrhadd.s16  q6, q7        @ tc25 = (tc[] * 5 + 1) >> 1
        vcgt.s16    d2, d6, d2    @ if (d0 + d3 < beta)
        vmov        r7, s4        @ (d2) r7 = mask of blocks to apply filtering (16b/block)
        vshr.s16    q1, q3, #3    @ beta_3 = beta >> 3
        cmp         r7, #0
        beq         .Lbypasswrite

        vcgt.s16    q5, q6, q5    @ if < tc25
        vcgt.s16    q4, q1, q4    @ if (abs({T}P[0-3]{'}-{T}P[0-3]{'})+abs({T}Q[0-3]{'}-{T}Q[0-3]{'}) < beta_3)
        vand        q4, q5
        vbic        d8, d4
        vbic        d9, d4
        vshr.s16    q3, #2        @ beta_2 = beta >> 2
        vsra.u64    q4, #16
        vshl.s16    d5, #1        @ d3'<<1 d0'<<1 d3<<1 d0<<1
        vshl.i16    q7, #1        @ tc2 = tC[] << 1
        vcgt.s16    d6, d5        @ if (d3'<<1 < beta_2) etc
        vmovn.i32   d8, q4        @ beta_3 && tc25 tests, prime block in ms half
        vand        d6, d8        @ && beta_2 tests, prime in ms half
        vpadd.i16   d0, d1        @ dq0'+dq3' dq0+dq3 dp0'+dp3' dp0+dp3
        vneg.s16    q6, q7        @ -tc2
        vmovn.i32   d8, q3
        vshrn.i32   d6, q3, #16
        vand        d6, d8
        vmov        r5, r6, d0    @ r5 = dp0'+dp3' dp0+dp3  r6 = dq0'+dq3' dq0+dq3
        vmov        r8, s12       @ (d6) r8 = mask of strong filtering blocks (16b/block)
        vadd.i16    q0, \Q11, q12 @ p0 + q0
        ands        r9, r7, r8
        beq         1f

        vadd.i16    q2, q0, q10   @ p1 + p0 + q0
        vadd.i16    q3, q0, q13   @ p0 + q0 + q1
        lsr         r3, r9, #16
        vadd.i16    q1, q2, q9    @ p2 + p1 + p0 + q0 (new P1 before clipping)
        vadd.i16    q4, q3, q14   @ p0 + q0 + q1 + q2 (new Q1 before clipping)
        vadd.i16    q0, q8, q9    @ p3 + p2
        vadd.i16    q5, \Q15, q14 @ q2 + q3
        vadd.i16    q2, q1        @ p2 + 2 * p1 + 2 * p0 + 2 * q0
        vadd.i16    q3, q4        @ 2 * p0 + 2 * q0 + 2 * q1 + q2
        vshl.i16    q0, #1        @ 2 * p3 + 2 * p2
        vshl.i16    q5, #1        @ 2 * q2 + 2 * q3
        vadd.i16    q0, q1        @ 2 * p3 + 3 * p2 + p1 + p0 + q0 (new P2 before clipping)
        vadd.i16    q5, q4        @ p0 + q0 + q1 + 3 * q2 + 2 * q3 (new Q2 before clipping)
        vadd.i16    q2, q13       @ p2 + 2 * p1 + 2 * p0 + 2 * q0 + q1 (new P0 before clipping)
        vadd.i16    q3, q10       @ p1 + 2 * p0 + 2 * q0 + 2 * q1 + q2 (new Q0 before clipping)
        vrshr.s16   q0, #3        @ scale, with rounding
        vrshr.s16   q5, #3
        vrshr.s16   q1, #2
        vrshr.s16   q4, #2
        vrshr.s16   q2, #3
        vrshr.s16   q3, #3
        vsub.i16    q0, q9        @ find difference
        vsub.i16    q5, q14
        vsub.i16    q1, q10
        vsub.i16    q4, q13
        vsub.i16    q2, \Q11
        vsub.i16    q3, q12
        vmax.s16    q0, q6        @ clip difference to -tc2 .. tc2
        vmax.s16    q5, q6
        vmax.s16    q1, q6
        vmax.s16    q4, q6
        vmax.s16    q2, q6
        vmax.s16    q3, q6
        vdup.16     d12, r9       @ expand mask, reuse q6 due to register pressure
        vdup.16     d13, r3
        vmin.s16    q0, q7
        vmin.s16    q5, q7
        vmin.s16    q1, q7
        vmin.s16    q4, q7
        vmin.s16    q2, q7
        vmin.s16    q3, q7
        vadd.i16    q0, q9        @ apply difference
        vadd.i16    q5, q14
        vadd.i16    q1, q10
        vadd.i16    q4, q13
        vadd.i16    q2, \Q11
        vadd.i16    q3, q12
        vbit        q9, q0, q6    @ apply filtered values according to mask
        vbit        q14, q5, q6
        vbit        q10, q1, q6
        vbit        q13, q4, q6
        vbit        \Q11, q2, q6
        vbit        q12, q3, q6
        vneg.s16    q6, q7        @ restore -tc2

1:
        bics        r9, r7, r8
        beq         2f

        vsub.i16    q0, q12, \Q11 @ q0 - p0
        vsub.i16    q1, q13, q10  @ q1 - p1
        lsr         r3, r9, #16
        vshl.i16    q2, q0, #3
        lsr         r7, r5, #16
        vadd.i16    q3, q0, q2    @ 9 * (q0 - p0)
        lsr         r8, r6, #16
        vshl.i16    q2, q1, #1
        vadd.i16    q4, q1, q2    @ 3 * (q1 - p1)
        vshr.s16    q6, #1        @ -tc = -tc2 >> 1
        vsub.i16    q5, q3, q4
        vrhadd.s16  q1, q9, \Q11  @ (p2 + p0 + 1) >> 1
        vrhadd.s16  q3, q14, q12  @ (q2 + q0 + 1) >> 1
        vrshr.s16   q5, #4        @ delta0 = (9 * (q0 - p0) - 3 * (q1 - p1) + 8) >> 4
        vsub.i16    q1, q10       @ ((p2 + p0 + 1) >> 1) - p1
        vsub.i16    q3, q13       @ ((q2 + q0 + 1) >> 1) - q1
        vmax.s16    q6, q5        @
        vshr.s16    q4, q7, #1    @ tc = tc2 >> 1
        vdup.16     q0, r2        @ beta
        vmin.s16    q6, q4        @ delta0 clamped to [-tc, tc]
        vshr.s16    q4, #1        @ tc_2 = tc >> 1
        vhadd.s16   q1, q6        @ (((p2 + p0 + 1) >> 1) - p1 + delta0) >> 1
        vhsub.s16   q3, q6        @ (((q2 + q0 + 1) >> 1) - q1 - delta0) >> 1
        vshr.s16    q2, q0, #1    @ beta >> 1
        vadd.i16    q2, q0        @ beta + (beta >> 1)
        vneg.s16    q0, q4        @ -tc_2
        vabs.s16    q5, q5        @ abs(original delta0)
        vshr.s16    q2, #3        @ (beta + (beta >> 1)) >> 3
        vmax.s16    q1, q0
        vmax.s16    q3, q0
        vshl.s16    q0, q7, #2    @ 8 * tc
        vadd.i16    q7, q0        @ 10 * tc
        vdup.16     d0, r9
        vdup.16     d1, r3        @ q0 = mask of blocks to apply filtering
        vmin.s16    q1, q4        @ deltap1 = av_clip((((p2 + p0 + 1) >> 1) - p1 + delta0) >> 1, -tc_2, tc_2)
        vmin.s16    q3, q4        @ deltaq1 = av_clip((((q2 + q0 + 1) >> 1) - q1 + delta0) >> 1, -tc_2, tc_2)
        vdup.16     d8, r5        @ dp0 + dp3
        vdup.16     d9, r7        @ dp0' + dp3'
        vcgt.s16    q7, q5        @ if ((10 * tc) > abs(delta0))
        vdup.16     d10, r6       @ dq0 + dq3
        vdup.16     d11, r8       @ dq0' + dq3'
        vand        q7, q0        @ AND block and line masks
        vcgt.s16    q4, q2, q4    @ if (((beta + (beta >> 1)) >> 3) > dp0 + dp3), i.e. if (nd_p > 1)
        vadd.i16    q0, q1, q10   @ p1 + deltap1
        vcgt.s16    q5, q2, q5    @ if (((beta + (beta >> 1)) >> 3) > dq0 + dq3), i.e. if (nd_q > 1)
        vadd.i16    q3, q3, q13   @ q1 + deltaq1
        vadd.i16    q1, \Q11, q6  @ p0 + delta0
        vsub.i16    q2, q12, q6   @ q0 - delta0
        vand        q4, q7        @ AND nd_p test with block/line masks
        vand        q5, q7        @ AND nd_q test with block/line masks
        vbit        q10, q0, q4
        vbit        \Q11, q1, q7
        vbit        q12, q2, q7
        vbit        q13, q3, q5

2:
.if \bit_depth == 8
        vmovn.i16 d16, q8
        vmovn.i16 d23, \Q15
        neg       r1, r1
        vqmovun.s16 d17, q9
        vqmovun.s16 d18, q10
        vqmovun.s16 d19, \Q11
        lsls      r10, #31
        vqmovun.s16 d20, q12
        vqmovun.s16 d21, q13
        vqmovun.s16 d22, q14
.else
        vmov.i16  q0, #0
        vmov.i16  q1, #(1 << \bit_depth - 1)
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
        bx        lr
.endm

function hevc_loop_filter_luma_body
        m_filter_luma 8, q15, q11
endfunc

@ void ff_hevc_rpi_v_loop_filter_luma_neon_8(
@   uint8_t *_pix,      [r0]
@   ptrdiff_t _stride,  [r1]
@   int _beta,          [r2]
@   int *_tc,           [r3]
@   uint8_t *_no_p,     [sp+0]
@   uint8_t *_no_q)     [sp+4]

function ff_hevc_rpi_v_loop_filter_luma_neon_8, export=1
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

        @ It's slightly faster to do unlaned loads and transpose in the
        @ 8-bit case, even though it needs more instructions, because
        @ VLD4.8 is a really slow way to read from memory.
        vld1.32 {d16[0]}, [r4:32], r1
        vld1.32 {d20[0]}, [r0:32], r1
        vld1.32 {d16[1]}, [r4:32], r1
        vld1.32 {d20[1]}, [r0:32], r1
        vld1.32 {d17[0]}, [r4:32], r1
        vld1.32 {d21[0]}, [r0:32], r1
        vld1.32 {d17[1]}, [r4:32], r1
        vld1.32 {d21[1]}, [r0:32], r1
        vld1.32 {d18[0]}, [r4:32], r1
        vld1.32 {d22[0]}, [r0:32], r1
        vld1.32 {d18[1]}, [r4:32], r1
        vld1.32 {d22[1]}, [r0:32], r1
        vld1.32 {d19[0]}, [r4:32], r1
        vld1.32 {d23[0]}, [r0:32], r1
        vld1.32 {d19[1]}, [r4:32]
        vld1.32 {d23[1]}, [r0:32]
        vuzp.16 q8, q9
        vuzp.16 q10, q11
        vuzp.8  q8, q9
        vuzp.8  q10, q11
        vswp    d17, d18
        vswp    d21, d22

        bl hevc_loop_filter_luma_body

        add     r6, r4, r1
        add     r2, r0, r1
        lsl     r1, #1

        vpop     {d8-d15}

        @ no_p[1]
        bmi     1f
        vst4.8  {d16[7],d17[7],d18[7],d19[7]}, [r4:32], r1
        vst4.8  {d16[6],d17[6],d18[6],d19[6]}, [r6:32], r1
        vst4.8  {d16[5],d17[5],d18[5],d19[5]}, [r4:32], r1
        vst4.8  {d16[4],d17[4],d18[4],d19[4]}, [r6:32], r1

        vst4.8  {d16[3],d17[3],d18[3],d19[3]}, [r4:32], r1
        vst4.8  {d16[2],d17[2],d18[2],d19[2]}, [r6:32], r1
        vst4.8  {d16[1],d17[1],d18[1],d19[1]}, [r4:32], r1
        vst4.8  {d16[0],d17[0],d18[0],d19[0]}, [r6:32]
1:
        @ no_q[1]
        bcs     1f
        vst4.8  {d20[7],d21[7],d22[7],d23[7]}, [r0:32], r1
        vst4.8  {d20[6],d21[6],d22[6],d23[6]}, [r2:32], r1
        vst4.8  {d20[5],d21[5],d22[5],d23[5]}, [r0:32], r1
        vst4.8  {d20[4],d21[4],d22[4],d23[4]}, [r2:32], r1

        vst4.8  {d20[3],d21[3],d22[3],d23[3]}, [r0:32], r1
        vst4.8  {d20[2],d21[2],d22[2],d23[2]}, [r2:32], r1
        vst4.8  {d20[1],d21[1],d22[1],d23[1]}, [r0:32], r1
        vst4.8  {d20[0],d21[0],d22[0],d23[0]}, [r2:32]
1:
        pop      {r4-r10,pc}

.Lbypasswrite:
        vpop     {d8-d15}
        pop      {r4-r10,pc}
endfunc

.macro m_filter_v_luma_16 bit_depth
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

        add      r6, r4, r1
        add      r2, r0, r1
        lsl      r1, #1

        vpop     {d8-d15}

        @ p[1]
        bmi      1f
        vst4.16  {d17[3], d19[3], d21[3], d23[3]}, [r4], r1
        vst4.16  {d17[2], d19[2], d21[2], d23[2]}, [r6], r1
        vst4.16  {d17[1], d19[1], d21[1], d23[1]}, [r4], r1
        vst4.16  {d17[0], d19[0], d21[0], d23[0]}, [r6], r1
        vst4.16  {d16[3], d18[3], d20[3], d22[3]}, [r4], r1
        vst4.16  {d16[2], d18[2], d20[2], d22[2]}, [r6], r1
        vst4.16  {d16[1], d18[1], d20[1], d22[1]}, [r4], r1
        vst4.16  {d16[0], d18[0], d20[0], d22[0]}, [r6]
1:
        @ q[1]
        bcs      1f
        vst4.16  {d25[3], d27[3], d29[3], d31[3]}, [r0], r1
        vst4.16  {d25[2], d27[2], d29[2], d31[2]}, [r2], r1
        vst4.16  {d25[1], d27[1], d29[1], d31[1]}, [r0], r1
        vst4.16  {d25[0], d27[0], d29[0], d31[0]}, [r2], r1
        vst4.16  {d24[3], d26[3], d28[3], d30[3]}, [r0], r1
        vst4.16  {d24[2], d26[2], d28[2], d30[2]}, [r2], r1
        vst4.16  {d24[1], d26[1], d28[1], d30[1]}, [r0], r1
        vst4.16  {d24[0], d26[0], d28[0], d30[0]}, [r2]
1:
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

function ff_hevc_rpi_h_loop_filter_luma_neon_8, export=1
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
        sub      r4, r0, r1, lsl #2
        add      r0, r4, r1
        lsl      r1, #1
        vpush    {d8-d15}

        vld1.8  {d16}, [r4], r1
        vld1.8  {d17}, [r0], r1
        vld1.8  {d18}, [r4], r1
        vld1.8  {d19}, [r0], r1
        vld1.8  {d20}, [r4], r1
        vld1.8  {d21}, [r0], r1
        vld1.8  {d22}, [r4]
        vld1.8  {d23}, [r0]

        bl hevc_loop_filter_luma_body

        add      r0, r0, r1, lsl #1
        add      r2, r4, r1, lsl #1
        add      r6, r4, r1, asr #1
        vpop     {d8-d15}

        @ P2-P0
        bcs      1f
        vst1.8   {d22}, [r4], r1
        vst1.8   {d21}, [r6]
        vst1.8   {d20}, [r4]
1:
        @ Q0-Q2
        bmi      1f
        vst1.8   {d19}, [r0], r1
        vst1.8   {d18}, [r2]
        vst1.8   {d17}, [r0]
1:
        pop      {r4-r10,pc}
endfunc


.macro m_filter_h_luma_16 bit_depth
        sub      r4, r0, r1, lsl #2
        add      r0, r4, r1
        lsl      r1, #1
        vpush    {d8-d15}

        vld1.16 { q8}, [r4], r1
        vld1.16 { q9}, [r0], r1
        vld1.16 {q10}, [r4], r1
        vld1.16 {q11}, [r0], r1
        vld1.16 {q12}, [r4], r1
        vld1.16 {q13}, [r0], r1
        vld1.16 {q14}, [r4]
        vld1.16 {q15}, [r0]

        bl hevc_loop_filter_luma_body_\bit_depth

        add      r0, r0, r1, lsl #1
        add      r2, r4, r1, lsl #1
        add      r6, r4, r1, asr #1
        vpop     {d8-d15}

        @ P2-P0
        bcs      1f
        vst1.16  {q14}, [r4], r1
        vst1.16  {q13}, [r6]
        vst1.16  {q12}, [r4]
1:
        bmi      1f
        vst1.16  {q11}, [r0], r1
        vst1.16  {q10}, [r2]
        vst1.16  { q9}, [r0]
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
        sub      r12, r0, r1
        cmp      r2, #0
        it eq
        bxeq     lr
        vld1.8   {d26,d27}, [r0]
        lsl      r1, #1
        sub      r0, r1
        vld1.8   {d18,d19}, [r12], r1
        vld1.8   {d16,d17}, [r0], r1
        vld1.8   {d28,d29}, [r12]

        hevc_loop_filter_uv_body2 d16, d17, d18, d19, d26, d27, d28, d29, \
        "sub      r12, r0, r1, asr #1"

        lsls     r3, #29                @ b2 -> N, b3 -> C
        it pl
        vstrpl   d26, [r0, #0]
        it cc
        vstrcc   d27, [r0, #8]
        lsls     r3, #2                 @ b0 -> N, b1 -> C
        it pl
        vstrpl   d18, [r12, #0]
        it cc
        vstrcc   d19, [r12, #8]
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
        sub      r12, r0, r1
        cmp      r2, #0
        it eq
        bxeq     lr
        vld1.16  {q12, q13}, [r0]
        lsl      r1, #1
        sub      r0, r1
        vld1.16  {q10, q11}, [r12], r1
        vld1.16  {q8,  q9 }, [r0], r1
        vld1.16  {q14, q15}, [r12]

        hevc_loop_filter_uv_body2_16  q8, q9, q10, q11, q12, q13, q14, q15, \bit_depth, \
        "sub      r12, r0, r1, asr #1", \
        "cmp      r3, #0"

        bne      1f
        vst1.16  {q10, q11}, [r12]
        vst1.16  {q12, q13}, [r0]
        bx       lr

        @ At least one no_f bit is set
        @ Which means we need to break this apart in an ugly fashion
1:
        lsls     r3, #29                @ b2 -> N, b3 -> C
        itt pl
        vstrpl   d24, [r0, #0]
        vstrpl   d25, [r0, #8]
        itt cc
        vstrcc   d26, [r0, #16]
        vstrcc   d27, [r0, #24]
        lsls     r3, #2                 @ b0 -> N, b1 -> C
        itt pl
        vstrpl   d20, [r12, #0]
        vstrpl   d21, [r12, #8]
        itt cc
        vstrcc   d22, [r12, #16]
        vstrcc   d23, [r12, #24]
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
        it eq
        bxeq     lr
        push     {lr}
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

        vld2.16  {d17[0], d19[0]}, [r3], r1
        vld2.16  {d21[0], d23[0]}, [r0], r1

        sub      ip, r0, r3
        vld2.16  {d17[1], d19[1]}, [r3], r1
        vld2.16  {d21[1], d23[1]}, [r0], r1

        cmp      ip, #4
        vld2.16  {d17[2], d19[2]}, [r3], r1
        vld2.16  {d21[2], d23[2]}, [r0], r1

        vld2.16  {d17[3], d19[3]}, [r3]
        vld2.16  {d21[3], d23[3]}, [r0]

        hevc_loop_filter_uv_body2 d16, d17, d18, d19, d20, d21, d22, d23 \
        "ldr      lr, [sp, #4]", \
        "neg      r1, r1",       \
        "it eq; cmpeq lr, #0",   \
        "add      r3, #2",       \
        "add      ip, r3, r1",   \
        "add      r2, r0, r1",   \
        "lsl      r1, #1"

        bne      1f

@ Much/most of the time r0 == r3 + 4 and no_f == 0
@ so it is worth having this special case
        vst2.16   {d19[3], d21[3]}, [r3], r1    @ P0b, Q0b
        vst2.16   {d19[2], d21[2]}, [ip], r1
        vst2.16   {d19[1], d21[1]}, [r3], r1
        vst2.16   {d19[0], d21[0]}, [ip], r1
        vst2.16   {d18[3], d20[3]}, [r3], r1    @ P0a, Q0a
        vst2.16   {d18[2], d20[2]}, [ip], r1
        vst2.16   {d18[1], d20[1]}, [r3]
        vst2.16   {d18[0], d20[0]}, [ip]
        pop       {pc}

@ Either split or partial
1:
        lsls     lr, #29               @ b3 (Q0b) -> C, b2 (P0b) -> N & b31, b1 (Q0a) -> b30, b0 (P0a) -> b29
        ittt cs
        addcs    r0, r0, r1, lsl #1
        addcs    r2, r2, r1, lsl #1
        bcs      1f
        @ Q0b
        vst1.16  {d21[3]}, [r0], r1
        vst1.16  {d21[2]}, [r2], r1
        vst1.16  {d21[1]}, [r0], r1
        vst1.16  {d21[0]}, [r2], r1
1:
        ittt mi
        addmi    r3, r3, r1, lsl #1
        addmi    ip, ip, r1, lsl #1
        bmi      1f
        @ P0b
        vst1.16  {d19[3]}, [r3], r1
        vst1.16  {d19[2]}, [ip], r1
        vst1.16  {d19[1]}, [r3], r1
        vst1.16  {d19[0]}, [ip], r1
1:
        lsls     lr, #2                @ b30 (Q0a) -> C, b29 (P0a) -> N & b31
        bcs      1f
        @ Q0a
        vst1.16  {d20[3]}, [r0], r1
        vst1.16  {d20[2]}, [r2], r1
        vst1.16  {d20[1]}, [r0]
        vst1.16  {d20[0]}, [r2]
1:
        it       mi
        popmi    {pc}
        @ P0a
        vst1.16  {d18[3]}, [r3], r1
        vst1.16  {d18[2]}, [ip], r1
        vst1.16  {d18[1]}, [r3]
        vst1.16  {d18[0]}, [ip]
        pop      {pc}

@ Single lump (rather than double)
10:
        @ As we have post inced r0/r3 in the load the easiest thing to do is
        @ to subtract and write forwards, rather than backwards (as above)
        @ b0 (P0a) -> N, b1 (Q0a) -> C

        hevc_loop_filter_uv_body1 d16, d18, d20, d22 \
        "ldr      lr, [sp, #4]",       \
        "add      r3, #2",             \
        "sub      r0, r0, r1, lsl #2", \
        "sub      r3, r3, r1, lsl #2", \
        "lsls     lr, #31",            \
        "add      r2, r0, r1",         \
        "add      ip, r3, r1",         \
        "lsl      r1, #1"

        bcs      3f
        @ Q0a
        vst1.16  {d20[0]}, [r0], r1
        vst1.16  {d20[1]}, [r2], r1
        vst1.16  {d20[2]}, [r0]
        vst1.16  {d20[3]}, [r2]
3:
        it       mi
        popmi    {pc}
        @ P0a
        vst1.16  {d18[0]}, [r3], r1
        vst1.16  {d18[1]}, [ip], r1
        vst1.16  {d18[2]}, [r3]
        vst1.16  {d18[3]}, [ip]
        pop      {pc}

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
        it eq
        bxeq     lr
        push     {lr}
        vld2.32  {d16[0], d18[0]}, [r3], r1
        vld2.32  {d20[0], d22[0]}, [r0], r1

        cmp      r2, #0x10000
        vld2.32  {d16[1], d18[1]}, [r3], r1
        vld2.32  {d20[1], d22[1]}, [r0], r1

        vld2.32  {d17[0], d19[0]}, [r3], r1
        vld2.32  {d21[0], d23[0]}, [r0], r1

        vld2.32  {d17[1], d19[1]}, [r3], r1
        vld2.32  {d21[1], d23[1]}, [r0], r1
        blo      10f

        vld2.32  {d24[0], d26[0]}, [r3], r1
        vld2.32  {d28[0], d30[0]}, [r0], r1

        sub      ip, r0, r3
        vld2.32  {d24[1], d26[1]}, [r3], r1
        vld2.32  {d28[1], d30[1]}, [r0], r1

        cmp      ip, #8
        vld2.32  {d25[0], d27[0]}, [r3], r1
        vld2.32  {d29[0], d31[0]}, [r0], r1

        vld2.32  {d25[1], d27[1]}, [r3]
        vld2.32  {d29[1], d31[1]}, [r0]

        hevc_loop_filter_uv_body2_16  q8, q12, q9, q13, q10, q14, q11, q15, \bit_depth, \
        "ldr      lr, [sp, #4]", \
        "neg      r1, r1",       \
        "it eq; cmpeq lr, #0",   \
        "add      r3, #4",       \
        "add      ip, r3, r1",   \
        "add      r2, r0, r1",   \
        "lsl      r1, #1"

        bne      1f

@ Much/most of the time r0 == r3 + 8 and no_f == 0
@ so it is worth having this special case
        vst2.32   {d27[1], d29[1]}, [r3], r1    @ P0b, Q0b
        vst2.32   {d27[0], d29[0]}, [ip], r1
        vst2.32   {d26[1], d28[1]}, [r3], r1
        vst2.32   {d26[0], d28[0]}, [ip], r1
        vst2.32   {d19[1], d21[1]}, [r3], r1    @ P0a, Q0a
        vst2.32   {d19[0], d21[0]}, [ip], r1
        vst2.32   {d18[1], d20[1]}, [r3]
        vst2.32   {d18[0], d20[0]}, [ip]
        pop       {pc}

@ Either split or partial
1:
        lsls     lr, #29               @ b3 (Q0b) -> C, b2 (P0b) -> N & b31, b1 (Q0a) -> b30, b0 (P0a) -> b29
        ittt cs
        addcs    r0, r0, r1, lsl #1
        addcs    r2, r2, r1, lsl #1
        bcs      1f
        @ Q0b
        vst1.32  {d29[1]}, [r0], r1
        vst1.32  {d29[0]}, [r2], r1
        vst1.32  {d28[1]}, [r0], r1
        vst1.32  {d28[0]}, [r2], r1
1:
        ittt mi
        addmi    r3, r3, r1, lsl #1
        addmi    ip, ip, r1, lsl #1
        bmi      1f
        @ P0b
        vst1.32  {d27[1]}, [r3], r1
        vst1.32  {d27[0]}, [ip], r1
        vst1.32  {d26[1]}, [r3], r1
        vst1.32  {d26[0]}, [ip], r1
1:
        lsls     lr, #2                @ b30 (Q0a) -> C, b29 (P0a) -> N & b31
        bcs      1f
        @ Q0a
        vst1.32  {d21[1]}, [r0], r1
        vst1.32  {d21[0]}, [r2], r1
        vst1.32  {d20[1]}, [r0]
        vst1.32  {d20[0]}, [r2]
1:
        it       mi
        popmi    {pc}
        @ P0a
        vst1.32  {d19[1]}, [r3], r1
        vst1.32  {d19[0]}, [ip], r1
        vst1.32  {d18[1]}, [r3]
        vst1.32  {d18[0]}, [ip]
        pop      {pc}

@ Single lump (rather than double)
10:
        @ As we have post inced r0/r3 in the load the easiest thing to do is
        @ to subtract and write forwards, rather than backwards (as above)
        @ b0 (P0a) -> N, b1 (Q0a) -> C

        hevc_loop_filter_uv_body1_16  q8, q9, q10, q11, \bit_depth, \
        "ldr      lr, [sp, #4]",       \
        "add      r3, #4",             \
        "sub      r0, r0, r1, lsl #2", \
        "sub      r3, r3, r1, lsl #2", \
        "lsls     lr, #31",            \
        "add      r2, r0, r1",         \
        "add      ip, r3, r1",         \
        "lsl      r1, #1"

        bcs      3f
        @ Q0a
        vst1.32  {d20[0]}, [r0], r1
        vst1.32  {d20[1]}, [r2], r1
        vst1.32  {d21[0]}, [r0]
        vst1.32  {d21[1]}, [r2]
3:
        it       mi
        popmi    {pc}
        @ P0a
        vst1.32  {d18[0]}, [r3], r1
        vst1.32  {d18[1]}, [ip], r1
        vst1.32  {d19[0]}, [r3]
        vst1.32  {d19[1]}, [ip]
        pop      {pc}
.endm


@ The NEON version is faster under ideal circumstances (i.e. everything in L1)
@ But in real world testing it is ~20% slower, presumably due to code size

#if 0 // NEON version

/* uint32_t ff_hevc_rpi_deblocking_boundary_strengths_neon(int pus, int dup, const HEVCRpiMvField *curr, const HEVCRpiMvField *neigh,
 *                                            const int *curr_rpl0, const int *curr_rpl1, const int *neigh_rpl0, const int *neigh_rpl1,
 *                                            int in_inc0, int in_inc1)
 */
function ff_hevc_rpi_deblocking_boundary_strengths_neon, export=1
        mov         ip, sp
        push        {a1-a3,v1-v8,lr}
        ldm         ip, {v1-v6}
        cmp         a1, #2
        bls         2f
        vpush       {d8-d13}
        sub         v5, v5, #10
        sub         v6, v6, #10
1:
        vld2.32     {d0[0], d2[0]}, [a3]!
        vld2.32     {d4[0], d6[0]}, [a4]!
          vmov.u8     q12, #0
        ldrb        a2, [a3], #1
        ldrb        ip, [a4], #1
        ldrb        v8, [a3], #1
        ldrb        lr, [a4], #1
        add         a2, v1, a2, lsl #2
        vld1.8      {d24[0]}, [a3], v5
        add         ip, v3, ip, lsl #2
        vld1.8      {d25[0]}, [a4], v6
        add         v8, v2, v8, lsl #2
        vld1.32     {d16[0]}, [a2]
        add         lr, v4, lr, lsl #2
        vld1.32     {d20[0]}, [ip]
        vld1.32     {d18[0]}, [v8]
        vld1.32     {d22[0]}, [lr]

        vld2.32     {d0[1], d2[1]}, [a3]!
        vld2.32     {d4[1], d6[1]}, [a4]!
        ldrb        a2, [a3], #1
          vmov.u16    d12, #1
        ldrb        ip, [a4], #1
          vmov.u16    d13, #2
        ldrb        v8, [a3], #1
          vmov.u16    d27, #4
        ldrb        lr, [a4], #1
        add         a2, v1, a2, lsl #2
        vld1.8      {d24[2]}, [a3], v5
        add         ip, v3, ip, lsl #2
        vld1.8      {d25[2]}, [a4], v6
        add         v8, v2, v8, lsl #2
        vld1.32     {d16[1]}, [a2]
        add         lr, v4, lr, lsl #2
        vld1.32     {d20[1]}, [ip]
        vld1.32     {d18[1]}, [v8]
        vld1.32     {d22[1]}, [lr]

        vld2.32     {d1[0], d3[0]}, [a3]!
        vld2.32     {d5[0], d7[0]}, [a4]!
        ldrb        a2, [a3], #1
        ldrb        ip, [a4], #1
        ldrb        lr, [a4], #1
        ldrb        v8, [a3], #1
        add         a2, v1, a2, lsl #2
        vld1.8      {d24[4]}, [a3], v5
        add         ip, v3, ip, lsl #2
        vld1.8      {d25[4]}, [a4], v6
        add         v8, v2, v8, lsl #2
        vld1.32     {d17[0]}, [a2]
        add         lr, v4, lr, lsl #2
        vld1.32     {d21[0]}, [ip]
        vld1.32     {d19[0]}, [v8]
        vld1.32     {d23[0]}, [lr]

        vld2.32     {d1[1], d3[1]}, [a3]!
        vld2.32     {d5[1], d7[1]}, [a4]!
        ldrb        a2, [a3], #1
        ldrb        ip, [a4], #1
        ldrb        v8, [a3], #1
        ldrb        lr, [a4], #1
        add         a2, v1, a2, lsl #2
        vld1.8      {d24[6]}, [a3], v5
        add         ip, v3, ip, lsl #2
        vld1.8      {d25[6]}, [a4], v6
        add         v8, v2, v8, lsl #2
        vld1.32     {d17[1]}, [a2]
        add         lr, v4, lr, lsl #2
        vld1.32     {d21[1]}, [ip]
        vld1.32     {d19[1]}, [v8]
        vld1.32     {d23[1]}, [lr]

        @ So now we have:
        @ q0.32[i]  = curr[i].mv[0]
        @ q1.32[i]  = curr[i].mv[1]
        @ q2.32[i]  = neigh[i].mv[0]
        @ q3.32[i]  = neigh[i].mv[1]
        @ q8.32[i]  = curr_rpl0[curr[i].ref_idx[0]]
        @ q9.32[i]  = curr_rpl1[curr[i].ref_idx[1]]
        @ q10.32[i] = neigh_rpl0[neigh[i].ref_idx[0]]
        @ q11.32[i] = neigh_rpl1[neigh[i].ref_idx[1]]
        @ d24.16[i] = curr[i].pred_flag
        @ d25.16[i] = neigh[i].pred_flag

        vtst.16     d28, d24, d12
        vtst.16     d29, d24, d13
        vadd.i16    d8, d24, d12
        vadd.i16    d9, d25, d12
        vtst.16     d30, d25, d12
        vtst.16     d31, d25, d13
        veor        d26, d8, d9
          ldr         lr, [sp, 6*8 + 1*4]
        vmovl.s16   q4, d28
        vmovl.s16   q5, d29
          teq         lr, #1
        vmovl.s16   q14, d30
          it ne
          lslne       v1, lr, #1
        vmovl.s16   q15, d31
          it ne
          rsbne       v2, v1, #32
        vbif        q0, q1, q4
        vbif        q2, q3, q14
        vbif        q1, q0, q5
        vbif        q3, q2, q15
        vabd.s16    q12, q0, q2
        vabd.s16    q2, q1
        vabd.s16    q0, q3
        vabd.s16    q1, q3
        vbif        q8, q9, q4
        vbif        q10, q11, q14
        vbif        q9, q8, q5
        vbif        q11, q10, q15
        vclt.u16    d6, d24, d27
        vclt.u16    d8, d2, d27
        vclt.u16    d7, d25, d27
        vclt.u16    d9, d3, d27
        vclt.u16    d2, d0, d27
        vclt.u16    d0, d4, d27
        vclt.u16    d3, d1, d27
        vclt.u16    d1, d5, d27
        vceq.i32    q12, q10, q8
        vceq.i32    q10, q9
        vceq.i32    q8, q11
        vceq.i32    q9, q11
        vshrn.i32   d6, q3, #8
        vshrn.i32   d7, q4, #8
        vshrn.i32   d8, q1, #8
        vshrn.i32   d9, q0, #8
        vmovn.i32   d4, q12
        vmovn.i32   d2, q10
        vmovn.i32   d3, q8
        vmovn.i32   d5, q9
        vand        q2, q3
        vrev16.8    q3, q3
        vand        q2, q3
        vand        q1, q4
        vrev16.8    q4, q4
        vand        q1, q4
        vand        d4, d5
        vand        d2, d3
        vbic        d0, d12, d4
        vshr.u16    d26, #2
        vbic        d0, d2
        vmov.i16    d1, #0x5555
        vorr        d0, d26
          bne         10f

        @ Merge results into result word, no duplicates
        vmov        a2, s0
        vmov        v8, s1
        vmov.u16    ip, d0[1]
        vmov.u16    lr, d0[3]
        lsl         a2, #30
        lsl         v8, #30
        lsl         ip, #30
        lsl         lr, #30
        orr         a2, ip, a2, lsr #2
        orr         v8, lr, v8, lsr #2
        orr         a2, v8, a2, lsr #4
        subs        a1, #4
        orr         v7, a2, v7, lsr #8
        bhi         1b

        mov         a1, #32
        ldr         a3, [sp, #6*8]
        vpop        {d8-d13}
        sub         a1, a1, a3, lsl #1
        mov         a1, v7, lsr a1
        pop         {a2-a4,v1-v8,pc}
10:
        @ Merge results into result word, with duplicates
        vmul.i16    d0, d1
        vmov        a2, s0
        vmov        v8, s1
        vmov.u16    ip, d0[1]
        vmov.u16    lr, d0[3]
        lsl         a2, v2
        subs        a1, #4
        lsl         v8, v2
        lsl         ip, v2
        lsl         lr, v2
        ldr         v2, [sp, #6*8 + 12*4 + 1*4]
T       lsr         a2, v1
T       orr         a2, ip, a2
A       orr         a2, ip, a2, lsr v1
        lsl         ip, v1, #1
T       lsr         v8, v1
T       orr         v8, lr, v8
A       orr         v8, lr, v8, lsr v1
        lsl         lr, v1, #2
T       lsr         a2, ip
T       orr         a2, v8, a2
A       orr         a2, v8, a2, lsr ip
        ldr         v1, [sp, #6*8 + 12*4]
T       lsr         v7, lr
T       orr         v7, a2, v7
A       orr         v7, a2, v7, lsr lr
        bhi         1b

        mov         a1, #32
        ldrd        a3, a4, [sp, #6*8]
        vpop        {d8-d13}
        mls         a1, a3, a4, a1
        mls         a1, a3, a4, a1
        mov         a1, v7, lsr a1
        pop         {a2-a4,v1-v8,pc}


2:
        sub         v5, v5, #10
        sub         v6, v6, #10
        vmov.u8     d16, #0
        blo         3f
        vld2.32     {d0[0], d1[0]}, [a3]!
        vld2.32     {d2[0], d3[0]}, [a4]!
        ldrb        a2, [a3], #1
        ldrb        ip, [a4], #1
        ldrb        lr, [a4], #1
        ldrb        v8, [a3], #1
        add         a2, v1, a2, lsl #2
        vld1.8      {d16[0]}, [a3], v5
        add         ip, v3, ip, lsl #2
        vld1.8      {d16[4]}, [a4], v6
        add         v8, v2, v8, lsl #2
        vld1.32     {d4[0]}, [a2]
        add         lr, v4, lr, lsl #2
        vld1.32     {d5[0]}, [ip]
        vld1.32     {d6[0]}, [v8]
        vld1.32     {d7[0]}, [lr]

3:
        vld2.32     {d0[1], d1[1]}, [a3]!
        vld2.32     {d2[1], d3[1]}, [a4]!
        ldrb        a2, [a3], #1
          vmov.u16    d17, #1
        ldrb        ip, [a4], #1
          vmov.u16    d18, #2
        ldrb        v8, [a3], #1
          vmov.u16    d19, #4
        ldrb        lr, [a4], #1
        add         a2, v1, a2, lsl #2
        vld1.8      {d16[2]}, [a3], v5
        add         ip, v3, ip, lsl #2
        vld1.8      {d16[6]}, [a4], v6
        add         v8, v2, v8, lsl #2
        vld1.32     {d4[1]}, [a2]
        add         lr, v4, lr, lsl #2
        vld1.32     {d5[1]}, [ip]
        vld1.32     {d6[1]}, [v8]
        vld1.32     {d7[1]}, [lr]

        @ So now we have:
        @ d0.32[i]  = curr[i].mv[0]
        @ d1.32[i]  = curr[i].mv[1]
        @ d2.32[i]  = neigh[i].mv[0]
        @ d3.32[i]  = neigh[i].mv[1]
        @ d4.32[i] = curr_rpl0[curr[i].ref_idx[0]]
        @ d5.32[i] = neigh_rpl0[neigh[i].ref_idx[0]]
        @ d6.32[i] = curr_rpl1[curr[i].ref_idx[1]]
        @ d7.32[i] = neigh_rpl1[neigh[i].ref_idx[1]]
        @ d16.16[i] = curr[i].pred_flag
        @ d16.16[2+i] = neigh[i].pred_flag

        vtst.16     d20, d16, d17
        vtst.16     d22, d16, d18
        vadd.i16    d30, d16, d17
        vswp        d2, d3
        ldr         lr, [sp, #1*4]
        vmovl.s16   q10, d20
          teq         lr, #1
        vmovl.s16   q11, d22
          it ne
          lslne       v1, lr, #1
        vbif        d0, d1, d20
        vbif        d4, d6, d20
        vbif        d3, d2, d21
        vbif        d5, d7, d21
        vbif        d1, d0, d22
        vbif        d6, d4, d22
        vbif        d2, d3, d23
        vbif        d7, d5, d23
        vshr.u16    d30, #2
        vabd.s16    d24, d0, d3
        vabd.s16    d25, d1, d2
        vabd.s16    q0, q0, q1
        vceq.i32    d2, d4, d5
        vceq.i32    d20, d5, d6
        vceq.i32    d21, d4, d7
        vceq.i32    d3, d6, d7
        vclt.u16    d6, d24, d19
        vclt.u16    d7, d25, d19
        vclt.u16    d22, d1, d19
        vclt.u16    d23, d0, d19
        vshrn.i32   d6, q3, #8
        vmovn.i32   d2, q1
        vshrn.i32   d7, q11, #8
        vmovn.i32   d3, q10
        vand        q0, q3, q1
          it ne
          rsbne       v2, v1, #32
        vrev16.8    q3, q3
        vand        q0, q3
        vsra.u64    d30, #32
        vshr.u64    q1, q0, #32
        vand        q0, q1
        vbic        d0, d17, d0
        vand        d30, d30, d17
        vbic        d0, d1
        vmov.i16    d1, #0x5555
        vorr        d0, d30
          bne         10f

        @ Construct result word, no duplicates
        cmp         a1, #2
        vmov.u16    a1, d0[1]
        vmov.u16    a2, d0[0]
        it eq
        orreq       a1, a2, a1, lsl #2
        pop         {a2-a4,v1-v8,pc}
10:
        @ Construct result word, with duplicates
        cmp         a1, #2
        vmul.i16    d0, d1
        vmov        a2, s0
        vmov.u16    a1, d0[1]
        lsl         a2, #16
        pkhbt       a1, a1, a1, lsl #16
        lsr         a2, v2
        lsr         a1, v2
T       itt eq
T       lsleq       a1, v1
T       orreq       a1, a2, a1
A       orreq       a1, a2, a1, lsl v1
        pop         {a2-a4,v1-v8,pc}
endfunc



#else // non-NEON version


/* uint32_t ff_hevc_rpi_deblocking_boundary_strengths_neon(int pus, int dup, const HEVCRpiMvField *curr, const HEVCRpiMvField *neigh,
 *                                            const int *curr_rpl0, const int *curr_rpl1, const int *neigh_rpl0, const int *neigh_rpl1,
 *                                            int in_inc0, in_inc1)
 */
function ff_hevc_rpi_deblocking_boundary_strengths_neon, export=1
        add         ip, sp, #4*4
        push        {a2-a4,v1-v8,lr}
        mov         v6, #32
1:      ldmdb       ip, {v1-v4}
        ldrsb       v5, [a3, #8]    @ curr->ref_idx
        ldrsb       v8, [a3, #9]
        ldrsb       ip, [a4, #8]    @ neigh->ref_idx
        ldrsb       lr, [a4, #9]
        ldr         v1, [v1, v5, lsl #2]
        ldrb        v5, [a3, #10]   @ curr->pred_flag
        ldr         v2, [v2, v8, lsl #2]
        ldrb        v8, [a4, #10]   @ neigh->pred_flag
        ldr         v3, [v3, ip, lsl #2]
        ldr         v4, [v4, lr, lsl #2]
        teq         v5, #3
        beq         20f
        teq         v8, #3
        beq         90f

        tst         v5, #1
        itee        ne
        ldrne       v5, [a3, #0]    @ curr->mv[0]
        moveq       v1, v2
        ldreq       v5, [a3, #4]    @ curr->mv[1]
        tst         v8, #1
        itee        ne
        ldrne       v8, [a4, #0]    @ neigh->mv[0]
        moveq       v3, v4
        ldreq       v8, [a4, #4]    @ neigh->mv[1]
        teq         v1, v3
        bne         10f
        ldr         lr, =0xFFFCFFFC
        ssub16      ip, v8, v5
        ssub16      v5, v5, v8
        sel         v5, v5, ip
        ands        v5, v5, lr
        @ drop through
10:     it          ne
        movne       v5, #1<<30
11:
        sub         v6, v6, #2
T       mov         v7, v7, lsr #2
        subs        a2, a2, #1
A       orr         v7, v5, v7, lsr #2
T       orr         v7, v5, v7
        bhi         11b

        ldrd        v3, v4, [sp, #16*4]
        ldr         a2, [sp]
        add         ip, sp, #16*4
        subs        a1, a1, #1
        add         a3, a3, v3
        add         a4, a4, v4
        bhi         1b
        mov         a1, v7, lsr v6
        pop         {a2-a4,v1-v8,pc}

20:     teq         v8, #3
        bne         10b

        teq         v1, v3
        it          eq
        teqeq       v2, v4
        bne         40f
        teq         v1, v2
        bne         30f

        ldrd        v1, v2, [a3]    @ curr->mv
        ldrd        v3, v4, [a4]    @ neigh->mv
        ldr         lr, =0xFFFCFFFC
        ssub16      ip, v3, v1
        ssub16      v5, v1, v3
        sel         v5, v5, ip
        ands        v5, v5, lr
        bne         25f
        ssub16      ip, v4, v2
        ssub16      v5, v2, v4
        sel         v5, v5, ip
        ands        v5, v5, lr
        beq         11b
        @ drop through
25:     ssub16      ip, v4, v1
        ssub16      v5, v1, v4
        sel         v5, v5, ip
        ands        v5, v5, lr
        bne         10b
        ssub16      ip, v3, v2
        ssub16      v5, v2, v3
        sel         v5, v5, ip
        ands        v5, v5, lr
        b           10b

30:     ldrd        v1, v2, [a3]    @ curr->mv
        ldrd        v3, v4, [a4]    @ neigh->mv
        ldr         lr, =0xFFFCFFFC
        ssub16      ip, v3, v1
        ssub16      v5, v1, v3
        sel         v5, v5, ip
        ands        v5, v5, lr
        bne         10b
        ssub16      ip, v4, v2
        ssub16      v5, v2, v4
        sel         v5, v5, ip
        ands        v5, v5, lr
        b           10b

40:     teq         v1, v4
        ite         eq
        teqeq       v2, v3
        bne         10b

        ldrd        v1, v2, [a3]    @ curr->mv
        ldrd        v3, v4, [a4]    @ neigh->mv
        ldr         lr, =0xFFFCFFFC
        b           25b

90:
        mov         v5, #1<<30
        b           11b
endfunc


#endif


@ =============================================================================
@
@ 10 bit

function hevc_loop_filter_luma_body_10
        m_filter_luma 10, q11, q15
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
        m_filter_v_luma_16 10
endfunc

function ff_hevc_rpi_h_loop_filter_uv_neon_10, export=1
        m_filter_h_uv_16 10
endfunc

function ff_hevc_rpi_v_loop_filter_uv2_neon_10, export=1
        m_filter_v_uv2_16 10
endfunc

