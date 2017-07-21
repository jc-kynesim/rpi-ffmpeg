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

.macro hevc_loop_filter_chroma_start
        ldr      r12, [r2]
        ldr      r2, [r2, #4]
        orrs     r2, r12, r2, lsl #16
        it       eq
        bxeq     lr
.endm

@ Uses: d2, d4, d18, d19
@ Returns: d2, d4
@ Modifies: d0-d7, d22-d25, r12

.macro hevc_loop_filter_chroma_body P1, P0, Q0, Q1
        vsubl.u8  q0, \Q0, \P0
        vsubl.u8  q1, \P1, \Q1
        vdup.16   d4, r2
        lsr       r2, r2, #16
        vshl.i16  q0, #2
        ldr       r12, [sp, #0] @ r12 = &no_q
        vadd.i16  q0, q1
        ldrh      r3, [r3]      @ r3[0:8] = no_p[0], r3[8:15] = no_p[1]
        vdup.16   d5, r2

        vrshr.s16 q0, q0, #3
        ldrh      r12, [r12]
        vneg.s16  q3, q2
        vmin.s16  q0, q0, q2
        vmovl.u8  q2, \Q0
        vmax.s16  q0, q0, q3
        vaddw.u8  q1, q0, \P0
        vsub.i16  q2, q0
        orrs      r12, r3, r12, lsl #16  @ So should have b1:no_p[0], b9:no_p[1], b17: no_q[0], b25:no_q[1]
        vqmovun.s16 \P0, q1
        vqmovun.s16 \Q0, q2
.endm

@ Uses r2 (tc a;b)
@ Modifies: q0-q3
@ On exit
@   r12 (and flags) contain no_p;no_q
.macro hevc_loop_filter_chroma_body_16 P1, P0, Q0, Q1, bit_depth
        vsub.i16  q0, \Q0, \P0
        lsl       r12, r2, #(\bit_depth - 8)
        vsub.i16  q1, \P1, \Q1
        vshl.i16  q0, #2
        vdup.16   d4, r12
        lsr       r12, r12, #16
        vadd.i16  q0, q1
        ldrh      r3, [r3]
        vdup.16   d5, r12

        vrshr.s16 q0, q0, #3
        vneg.s16  q3, q2
        movw      r12, #(1 << \bit_depth) - 1
        vmin.s16  q0, q0, q2
        vmax.s16  q0, q0, q3
        vdup.i16  q3, r12
        ldr       r12, [sp, #0]

        vadd.i16  \P0, q0, \P0
        vsub.i16  \Q0, q0

        vmov.i64  q2, #0
        ldrh      r12, [r12]
        vmin.s16  \P0, q3
        vmin.s16  \Q0, q3
        orrs      r12, r3, r12, lsl #16  @ So should have b1:no_p[0], b9:no_p[1], b17: no_q[0], b25:no_q[1]
        vmax.s16  \P0, q2
        vmax.s16  \Q0, q2
.endm


@ Preserves r12
@ Clobbers r2
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

        @ r2[0:7] -> d4.16 (all), r2[8:15] -> d5.16(all)
        vrshr.s16 q0, #3
        vdup.16   d6, r2
        vmovl.u8  q2, d4
        vmovl.u8  q3, d6
        vuzp.16   d4, d5
        vrshr.s16 q1, #3
        vuzp.16   d6, d7

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
.macro hevc_loop_filter_uv_body2_16 P1u, P1v, P0u, P0v, Q0u, Q0v, Q1u, Q1v, bit_depth
        vsub.i16  q0, \Q0u, \P0u
        vsub.i16  q1, \Q0v, \P0v
        vsub.i16  q2, \P1u, \Q1u
        vsub.i16  q3, \P1v, \Q1v
        vshl.i16  q0, #2
        vshl.i16  q1, #2
        vadd.i16  q0, q2
        vdup.16   d4, r2
        lsr       r2, #16
        vadd.i16  q1, q3

        @ r2[0:7] -> d4.16 (all), r2[8:15] -> d5.16(all)
        vrshr.s16 q0, #3
        vdup.16   d6, r2
        vshll.u8  q2, d4, #\bit_depth - 8
        vshll.u8  q3, d6, #\bit_depth - 8
        vuzp.16   d4, d5
        vrshr.s16 q1, #3
        vuzp.16   d6, d7

        movw      r2, #(1 << \bit_depth) - 1
        vmin.s16  q0, q2
        vneg.s16  q2, q2
        vmin.s16  q1, q3
        vneg.s16  q3, q3
        vmax.s16  q0, q2
        vmov.i64  q2, #0
        vmax.s16  q1, q3
        vdup.i16  q3, r2
        vadd.i16  \P0u, q0
        vsub.i16  \Q0u, q0
        vadd.i16  \P0v, q1
        vsub.i16  \Q0v, q1

        vmax.s16  \P0u, q2
        vmax.s16  \Q0u, q2
        vmax.s16  \P0v, q2
        vmax.s16  \Q0v, q2
        vmin.s16  \P0u, q3
        vmin.s16  \Q0u, q3
        vmin.s16  \P0v, q3
        vmin.s16  \Q0v, q3
.endm



.macro hevc_loop_filter_luma_start
        ldr     r12, [r3]
        ldr      r3, [r3, #4]
        orrs     r3, r12, r3, lsl #16
        it       eq
        bxeq     lr
.endm

@ Uses: r2, r3, r12
@ Modifies: r5, r6, r7, r8, r9

@ Input:
@  r2          beta    (raw: needs shift for bitdepth > 8)
@  r3[ 0:15]   tc[0]   (raw: needs shift for bitdepth > 8)
@  r3[16:31]   tc[1]   (raw: needs shift for bitdepth > 8)
@  [sp,#96]    &no_p[0]
@  [sp,#100]   &no_q[0]
@
@ Input & output
@  8-bit: d16-d23
@ 16-bit:  q8-q15
@
@ Output
@  Z           r10==0
@  r10[ 0:7 ]  no_p[0]
@  r10[ 8:15]  no_p[1]
@  r10[16:23]  no_q[0]
@  r10[24:31]  no_q[1]


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
        ldr        r5, [sp, #96]        @ Bolt no_x values together into r10
        vsub.i16   q6, q13
        vabd.s16   q7, q7, q10
        vabd.s16   q6, q6, q13
        ldrh       r10, [r5]

        vdup.16    q0, r2
        vmov       q4, q7
        vmov       q5, q6
        ldr        r5, [sp, #100]
        vdup.16    d4, r3
        lsr        r3, r3, #16
        vtrn.16    q7, q4
        ldrh       r5, [r5]
        vtrn.16    q6, q5

        vshl.u64   q7, #32
        vshr.u64   q4, #32
        vshl.u64   q6, #32
        orr        r10, r10, r5, lsl #16
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
        beq        bypasswrite

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
        vqmovun.s16 d16, q8
        cmp       r10, #0
        vqmovun.s16 d17, q9
        vqmovun.s16 d18, q10
        vqmovun.s16 d19, q11
        vqmovun.s16 d20, q12
        vqmovun.s16 d21, q13
        vqmovun.s16 d22, q14
        vqmovun.s16 d23, q15
.else
        movw      r12, #(1 << \bit_depth - 1)
        vmov.i64  q0, #0
        vdup.i16  q1, r12
        @ q8 & q15 should be unaltered and so don't require clipping
        vmax.s16  q9,  q0
        cmp       r10, #0
        vmax.s16  q10, q0
        vmax.s16  q11, q0
        vmax.s16  q12, q0
        vmax.s16  q13, q0
        vmax.s16  q14, q0
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

@ ff_hevc_v_loop_filter_luma2_neon(src (r0), stride (r1), beta (r2), tc (r3), np_p (sp[0]), no_q (sp[4]), src2 (sp[8]))
function ff_hevc_v_loop_filter_luma2_neon_8, export=1
        hevc_loop_filter_luma_start
        push     {r4-r10,lr}       @ 8 regs = 32 bytes

        ldr      r4, [sp, #40]
        b        v_loop_luma_common
endfunc


@ void ff_hevc_v_loop_filter_luma_neon(
@   uint8_t *_pix,      [r0]
@   ptrdiff_t _stride,  [r1]
@   int _beta,          [r2]
@   int *_tc,           [r3]
@   uint8_t *_no_p,     [sp+0]
@   uint8_t *_no_q)     [sp+4]


function ff_hevc_v_loop_filter_luma_neon, export=1
        hevc_loop_filter_luma_start
        push     {r4-r10,lr}

        sub      r4, r0, #4
v_loop_luma_common:
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

        neg     r1, r1

        @ no_p[1]
        tst     r10, #0xff00
        add     r2, r4, r1, lsl #2
        bne     1f
        vst4.8  {d16[7],d17[7],d18[7],d19[7]}, [r4:32], r1
        vst4.8  {d16[6],d17[6],d18[6],d19[6]}, [r4:32], r1
        vst4.8  {d16[5],d17[5],d18[5],d19[5]}, [r4:32], r1
        vst4.8  {d16[4],d17[4],d18[4],d19[4]}, [r4:32]
1:
        @ no_p[0]
        tst     r10, #0xff
        bne     1f
        vst4.8  {d16[3],d17[3],d18[3],d19[3]}, [r2:32], r1
        vst4.8  {d16[2],d17[2],d18[2],d19[2]}, [r2:32], r1
        vst4.8  {d16[1],d17[1],d18[1],d19[1]}, [r2:32], r1
        vst4.8  {d16[0],d17[0],d18[0],d19[0]}, [r2:32]
1:
        @ no_q[1]
        tst     r10, #0xff000000
        add     r2, r0, r1, lsl #2
        bne     1f
        vst4.8  {d20[7],d21[7],d22[7],d23[7]}, [r0:32], r1
        vst4.8  {d20[6],d21[6],d22[6],d23[6]}, [r0:32], r1
        vst4.8  {d20[5],d21[5],d22[5],d23[5]}, [r0:32], r1
        vst4.8  {d20[4],d21[4],d22[4],d23[4]}, [r0:32]
1:
        @ no_q[0]
        tst     r10, #0xff0000
        bne     1f
        vst4.8  {d20[3],d21[3],d22[3],d23[3]}, [r2:32], r1
        vst4.8  {d20[2],d21[2],d22[2],d23[2]}, [r2:32], r1
        vst4.8  {d20[1],d21[1],d22[1],d23[1]}, [r2:32], r1
        vst4.8  {d20[0],d21[0],d22[0],d23[0]}, [r2:32]
1:
bypasswrite:
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

        neg     r1, r1

        @ p[1]
        tst      r10, #0xff00
        add      r2, r4, r1, lsl #2
        bne      1f
        vst4.16  {d17[3], d19[3], d21[3], d23[3]}, [r4], r1
        vst4.16  {d17[2], d19[2], d21[2], d23[2]}, [r4], r1
        vst4.16  {d17[1], d19[1], d21[1], d23[1]}, [r4], r1
        vst4.16  {d17[0], d19[0], d21[0], d23[0]}, [r4]
1:
        @ p[0]
        tst      r10, #0xff
        bne      1f
        vst4.16  {d16[3], d18[3], d20[3], d22[3]}, [r2], r1
        vst4.16  {d16[2], d18[2], d20[2], d22[2]}, [r2], r1
        vst4.16  {d16[1], d18[1], d20[1], d22[1]}, [r2], r1
        vst4.16  {d16[0], d18[0], d20[0], d22[0]}, [r2]
1:
        @ q[1]
        tst      r10, #0xff000000
        add      r2, r0, r1, lsl #2
        bne      1f
        vst4.16  {d25[3], d27[3], d29[3], d31[3]}, [r0], r1
        vst4.16  {d25[2], d27[2], d29[2], d31[2]}, [r0], r1
        vst4.16  {d25[1], d27[1], d29[1], d31[1]}, [r0], r1
        vst4.16  {d25[0], d27[0], d29[0], d31[0]}, [r0]
1:
        @ q[0]
        tst      r10, #0xff0000
        bne      1f
        vst4.16  {d24[3], d26[3], d28[3], d30[3]}, [r2], r1
        vst4.16  {d24[2], d26[2], d28[2], d30[2]}, [r2], r1
        vst4.16  {d24[1], d26[1], d28[1], d30[1]}, [r2], r1
        vst4.16  {d24[0], d26[0], d28[0], d30[0]}, [r2]
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

function ff_hevc_h_loop_filter_luma_neon, export=1
        hevc_loop_filter_luma_start
        push     {r4-r10,lr}

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

        vpop     {d8-d15}

        neg     r1, r1
        add     r0, r0, r1

        bne      1f

        vst1.8  {d22}, [r0], r1
        vst1.8  {d21}, [r0], r1
        vst1.8  {d20}, [r0], r1
        vst1.8  {d19}, [r0], r1
        vst1.8  {d18}, [r0], r1
        vst1.8  {d17}, [r0]

        pop      {r4-r10,pc}

@ Partial write
1:
        vmov     r2, r3, d22
        vmov     r4, r5, d21
        vmov     r6, r7, d20

        tst      r10, #0xff0000
        ittt eq
        streq    r2, [r0]
        streq    r4, [r0, r1]
        streq    r6, [r0, r1, lsl # 1]

        add      r0, r0, #4
        tst      r10, #0xff000000
        ittt eq
        streq    r3, [r0]
        streq    r5, [r0, r1]
        streq    r7, [r0, r1, lsl # 1]

        vmov     r2, r3, d19
        vmov     r4, r5, d18
        vmov     r6, r7, d17
        add      r0, r0, r1
        add      r0, r0, r1, lsl # 1

        tst      r10, #0xff00
        ittt eq
        streq    r3, [r0]
        streq    r5, [r0, r1]
        streq    r7, [r0, r1, lsl # 1]

        tst      r10, #0xff
        ittt eq
        streq    r2, [r0, #-4]!
        streq    r4, [r0, r1]
        streq    r6, [r0, r1, lsl # 1]

        pop      {r4-r10,pc}

endfunc


.macro m_filter_h_luma_16 bit_depth
        hevc_loop_filter_luma_start
        push     {r4-r10,lr}

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

        vpop     {d8-d15}

        sub      r0, r1
        neg      r1, r1
        bne      1f

        vst1.16  {q14}, [r0], r1
        vst1.16  {q13}, [r0], r1
        vst1.16  {q12}, [r0], r1
        vst1.16  {q11}, [r0], r1
        vst1.16  {q10}, [r0], r1
        vst1.16  { q9}, [r0]
        pop      {r4-r10,pc}

@ Partial write
1:
        tst      r10, #0xff0000
        mov      r2, r0
        bne      1f
        vst1.16  {d28}, [r2], r1
        vst1.16  {d26}, [r2], r1
        vst1.16  {d24}, [r2]

1:
        tst      r10, #0xff000000
        add      r2, r0, #8
        bne      1f
        vst1.16  {d29}, [r2], r1
        vst1.16  {d27}, [r2], r1
        vst1.16  {d25}, [r2]

1:
        tst      r10, #0xff
        @ r0 = r0 + r1 * 3
        add      r0, r0, r1
        add      r0, r0, r1, lsl # 1
        add      r2, r0, #8
        bne      1f
        vst1.16  {d22}, [r0], r1
        vst1.16  {d20}, [r0], r1
        vst1.16  {d18}, [r0]

1:
        tst      r10, #0xff00
        bne      1f
        vst1.16  {d23}, [r2], r1
        vst1.16  {d21}, [r2], r1
        vst1.16  {d19}, [r2]

1:
        pop      {r4-r10,pc}
.endm


@ void ff_hevc_h_loop_filter_uv_neon(uint8_t * src_r,        // r0
@                                     unsigned int stride,   // r1
@                                     uint32_t tc4,          // r2
@                                     unsigned int no_f);    // r3
@
@ no-F = b0:no_p[0], b1:no_p[1], b2:no_q[0], b3:no_q[1]
function ff_hevc_h_loop_filter_uv_neon_8, export=1
        sub      r0, r0, r1, lsl #1
        vld2.8   {d16,d17}, [r0], r1
        vld2.8   {d18,d19}, [r0], r1
        vld2.8   {d26,d27}, [r0], r1
        vld2.8   {d28,d29}, [r0]
        sub      r0, r0, r1, lsl #1
        hevc_loop_filter_uv_body2 d16, d17, d18, d19, d26, d27, d28, d29
        cmp      r3, #0
        bne      1f
        vst2.8   {d18,d19}, [r0], r1
        vst2.8   {d26,d27}, [r0]
        bx       lr

        @ At least one no_f bit is set
        @ Which means we need to break this apart in an ugly fashion
1:      vzip.8   d18, d19
        lsls     r2, r3, #31            @ b0 -> N, b1 -> C
        vzip.8   d26, d27
        sub      r1, r1, #8

        bmi      1f
        vst1.8   {d18}, [r0]
1:      add      r0, r0, #8
        bcs      2f
        vst1.8   {d19}, [r0]
2:      lsls     r2, r3, #29            @ b2 -> N, b3 -> C
        add      r0, r0, r1

        bmi      1f
        vst1.8   {d26}, [r0]
1:      it cs
        bxcs     lr
        add      r0, r0, #8
        vst1.8   {d27}, [r0]
        bx       lr

endfunc


@ void ff_hevc_h_loop_filter_uv_neon_10(uint8_t * src_r,     // r0
@                                     unsigned int stride,   // r1
@                                     uint32_t tc4,          // r2
@                                     unsigned int no_f);    // r3
@
@ no-F = b0:no_p[0], b1:no_p[1], b2:no_q[0], b3:no_q[1]
@
@ Macro here actual function near bottom

.macro m_filter_h_uv_16 bit_depth
        sub      r0, r0, r1, lsl #1
        vld2.16  {q8,  q9 }, [r0], r1
        vld2.16  {q10, q11}, [r0], r1
        vld2.16  {q12, q13}, [r0], r1
        vld2.16  {q14, q15}, [r0]
        sub      r0, r0, r1, lsl #1

        hevc_loop_filter_uv_body2_16 q8, q9, q10, q11, q12, q13, q14, q15, \bit_depth

        cmp      r3, #0
        bne      1f
        vst2.16  {q10, q11}, [r0], r1
        vst2.16  {q12, q13}, [r0]
        bx       lr

        @ At least one no_f bit is set
        @ Which means we need to break this apart in an ugly fashion
1:      vzip.16  q10, q11
        lsls     r2, r3, #31            @ b0 -> N, b1 -> C
        vzip.16  q12, q13
        sub      r1, r1, #16

        bmi      1f
        vst1.16  {q10}, [r0]
1:      add      r0, r0, #16
        bcs      2f
        vst1.16  {q11}, [r0]
2:      lsls     r2, r3, #29            @ b2 -> N, b3 -> C
        add      r0, r0, r1

        bmi      1f
        vst1.16  {q12}, [r0]
1:      it cs
        bxcs     lr
        add      r0, r0, #16
        vst1.16  {q13}, [r0]
        bx       lr
.endm


@ void ff_hevc_v_loop_filter_uv2_neon(uint8_t * src_r,       // r0
@                                     unsigned int stride,   // r1
@                                     uint32_t tc4,          // r2
@                                     uint8_t * src_l,       // r3
@                                     unsigned int no_f);   // sp[0]
@
@ no_f = b0:no_p[0], b1:no_p[1], b2:no_q[0], b3:no_q[1]

function ff_hevc_v_loop_filter_uv2_neon_8, export=1
        vld4.8   {d16[0], d17[0], d18[0], d19[0]}, [r3], r1
        vld4.8   {d20[0], d21[0], d22[0], d23[0]}, [r0], r1
        sub      r12, r0, r3

        vld4.8   {d16[1], d17[1], d18[1], d19[1]}, [r3], r1
        vld4.8   {d20[1], d21[1], d22[1], d23[1]}, [r0], r1
        cmp      r12, #4

        vld4.8   {d16[2], d17[2], d18[2], d19[2]}, [r3], r1
        vld4.8   {d20[2], d21[2], d22[2], d23[2]}, [r0], r1

        vld4.8   {d16[3], d17[3], d18[3], d19[3]}, [r3], r1
        vld4.8   {d20[3], d21[3], d22[3], d23[3]}, [r0], r1

        vld4.8   {d16[4], d17[4], d18[4], d19[4]}, [r3], r1
        vld4.8   {d20[4], d21[4], d22[4], d23[4]}, [r0], r1

        vld4.8   {d16[5], d17[5], d18[5], d19[5]}, [r3], r1
        vld4.8   {d20[5], d21[5], d22[5], d23[5]}, [r0], r1

        vld4.8   {d16[6], d17[6], d18[6], d19[6]}, [r3], r1
        vld4.8   {d20[6], d21[6], d22[6], d23[6]}, [r0], r1

        vld4.8   {d16[7], d17[7], d18[7], d19[7]}, [r3]
        vld4.8   {d20[7], d21[7], d22[7], d23[7]}, [r0]
        it eq
        ldreq    r12, [sp, #0]

        hevc_loop_filter_uv_body2 d16, d17, d18, d19, d20, d21, d22, d23
        cmp      r12, #0
        add      r3, #2
        neg      r1, r1
        bne      1f

@ Much/most of the time r0 == r3 + 4 and no_f == 0
@ so it is worth having this special case
        vst4.8   {d18[7], d19[7], d20[7], d21[7]}, [r3], r1
        vst4.8   {d18[6], d19[6], d20[6], d21[6]}, [r3], r1
        vst4.8   {d18[5], d19[5], d20[5], d21[5]}, [r3], r1
        vst4.8   {d18[4], d19[4], d20[4], d21[4]}, [r3], r1
        vst4.8   {d18[3], d19[3], d20[3], d21[3]}, [r3], r1
        vst4.8   {d18[2], d19[2], d20[2], d21[2]}, [r3], r1
        vst4.8   {d18[1], d19[1], d20[1], d21[1]}, [r3], r1
        vst4.8   {d18[0], d19[0], d20[0], d21[0]}, [r3]
        bx       lr

@ Either split or partial
1:
        ldr      r12, [sp, #0]
        lsls     r12, #29               @ b2 -> N, b3 -> C
        add      r2, r0, r1, lsl #2
        bcs      1f
        vst2.8   {d20[7], d21[7]}, [r0], r1
        vst2.8   {d20[6], d21[6]}, [r0], r1
        vst2.8   {d20[5], d21[5]}, [r0], r1
        vst2.8   {d20[4], d21[4]}, [r0]
1:
        bmi      2f
        vst2.8   {d20[3], d21[3]}, [r2], r1
        vst2.8   {d20[2], d21[2]}, [r2], r1
        vst2.8   {d20[1], d21[1]}, [r2], r1
        vst2.8   {d20[0], d21[0]}, [r2]

2:
        lsls     r12, #2
        add      r2, r3, r1, lsl #2
        bcs      3f
        vst2.8   {d18[7], d19[7]}, [r3], r1
        vst2.8   {d18[6], d19[6]}, [r3], r1
        vst2.8   {d18[5], d19[5]}, [r3], r1
        vst2.8   {d18[4], d19[4]}, [r3]
3:
        it mi
        bxmi     lr
        vst2.8   {d18[3], d19[3]}, [r2], r1
        vst2.8   {d18[2], d19[2]}, [r2], r1
        vst2.8   {d18[1], d19[1]}, [r2], r1
        vst2.8   {d18[0], d19[0]}, [r2]
        bx       lr
endfunc


@ void ff_hevc_v_loop_filter_uv2_neon(uint8_t * src_r,       // r0
@                                     unsigned int stride,   // r1
@                                     uint32_t tc4,          // r2
@                                     uint8_t * src_l,       // r3
@                                     unsigned int no_f);   // sp[0]
@
@ no_f = b0:no_p[0], b1:no_p[1], b2:no_q[0], b3:no_q[1]
.macro m_filter_v_uv2_16 bit_depth
        vld4.16  {d16[0], d18[0], d20[0], d22[0]}, [r3], r1
        vld4.16  {d24[0], d26[0], d28[0], d30[0]}, [r0], r1
        sub      r12, r0, r3

        vld4.16  {d16[1], d18[1], d20[1], d22[1]}, [r3], r1
        vld4.16  {d24[1], d26[1], d28[1], d30[1]}, [r0], r1
        cmp      r12, #8

        vld4.16  {d16[2], d18[2], d20[2], d22[2]}, [r3], r1
        vld4.16  {d24[2], d26[2], d28[2], d30[2]}, [r0], r1

        vld4.16  {d16[3], d18[3], d20[3], d22[3]}, [r3], r1
        vld4.16  {d24[3], d26[3], d28[3], d30[3]}, [r0], r1

        vld4.16  {d17[0], d19[0], d21[0], d23[0]}, [r3], r1
        vld4.16  {d25[0], d27[0], d29[0], d31[0]}, [r0], r1

        vld4.16  {d17[1], d19[1], d21[1], d23[1]}, [r3], r1
        vld4.16  {d25[1], d27[1], d29[1], d31[1]}, [r0], r1

        vld4.16  {d17[2], d19[2], d21[2], d23[2]}, [r3], r1
        vld4.16  {d25[2], d27[2], d29[2], d31[2]}, [r0], r1

        vld4.16  {d17[3], d19[3], d21[3], d23[3]}, [r3]
        vld4.16  {d25[3], d27[3], d29[3], d31[3]}, [r0]
        it eq
        ldreq    r12, [sp, #0]

        hevc_loop_filter_uv_body2_16  q8, q9, q10, q11, q12, q13, q14, q15, \bit_depth
        cmp      r12, #0
        add      r3, #4
        neg      r1, r1
        bne      1f

@ Much/most of the time r0 == r3 + 4 and no_f == 0
@ so it is worth having this special case
        vst4.16  {d21[3], d23[3],d25[3], d27[3]}, [r3], r1
        vst4.16  {d21[2], d23[2],d25[2], d27[2]}, [r3], r1
        vst4.16  {d21[1], d23[1],d25[1], d27[1]}, [r3], r1
        vst4.16  {d21[0], d23[0],d25[0], d27[0]}, [r3], r1
        vst4.16  {d20[3], d22[3],d24[3], d26[3]}, [r3], r1
        vst4.16  {d20[2], d22[2],d24[2], d26[2]}, [r3], r1
        vst4.16  {d20[1], d22[1],d24[1], d26[1]}, [r3], r1
        vst4.16  {d20[0], d22[0],d24[0], d26[0]}, [r3], r1
        bx       lr

@ Either split or partial
1:
        ldr      r12, [sp, #0]
        lsls     r12, #29               @ b2 -> N, b3 -> C
        add      r2, r0, r1, lsl #2
        bcs      1f
        vst2.16  {d25[3], d27[3]}, [r0], r1
        vst2.16  {d25[2], d27[2]}, [r0], r1
        vst2.16  {d25[1], d27[1]}, [r0], r1
        vst2.16  {d25[0], d27[0]}, [r0]
1:
        bmi      2f
        vst2.16  {d24[3], d26[3]}, [r2], r1
        vst2.16  {d24[2], d26[2]}, [r2], r1
        vst2.16  {d24[1], d26[1]}, [r2], r1
        vst2.16  {d24[0], d26[0]}, [r2]

2:
        lsls     r12, #2
        add      r2, r3, r1, lsl #2
        bcs      3f
        vst2.16  {d21[3], d23[3]}, [r3], r1
        vst2.16  {d21[2], d23[2]}, [r3], r1
        vst2.16  {d21[1], d23[1]}, [r3], r1
        vst2.16  {d21[0], d23[0]}, [r3]
3:
        it mi
        bxmi     lr
        vst2.16  {d20[3], d22[3]}, [r2], r1
        vst2.16  {d20[2], d22[2]}, [r2], r1
        vst2.16  {d20[1], d22[1]}, [r2], r1
        vst2.16  {d20[0], d22[0]}, [r2]
        bx       lr
.endm



function ff_hevc_v_loop_filter_chroma_neon, export=1
        hevc_loop_filter_chroma_start

        sub      r0, #2
        vld4.8   {d16[0], d17[0], d18[0], d19[0]}, [r0], r1
        vld4.8   {d16[1], d17[1], d18[1], d19[1]}, [r0], r1
        vld4.8   {d16[2], d17[2], d18[2], d19[2]}, [r0], r1
        vld4.8   {d16[3], d17[3], d18[3], d19[3]}, [r0], r1
        vld4.8   {d16[4], d17[4], d18[4], d19[4]}, [r0], r1
        vld4.8   {d16[5], d17[5], d18[5], d19[5]}, [r0], r1
        vld4.8   {d16[6], d17[6], d18[6], d19[6]}, [r0], r1
        vld4.8   {d16[7], d17[7], d18[7], d19[7]}, [r0], r1

        sub      r0, r0, r1, lsl #3
        add      r0, r0, #1
        hevc_loop_filter_chroma_body d16, d17, d18, d19
        bne      1f

        vst2.8   {d17[0], d18[0]}, [r0], r1
        vst2.8   {d17[1], d18[1]}, [r0], r1
        vst2.8   {d17[2], d18[2]}, [r0], r1
        vst2.8   {d17[3], d18[3]}, [r0], r1
        vst2.8   {d17[4], d18[4]}, [r0], r1
        vst2.8   {d17[5], d18[5]}, [r0], r1
        vst2.8   {d17[6], d18[6]}, [r0], r1
        vst2.8   {d17[7], d18[7]}, [r0], r1
        bx       lr

1:
        tst      r12, #0xff             @ P0a
        bne      2f

        vst1.8   {d17[0]}, [r0], r1
        vst1.8   {d17[1]}, [r0], r1
        vst1.8   {d17[2]}, [r0], r1
        vst1.8   {d17[3]}, [r0], r1
        sub      r0, r0, r1, lsl #2

2:
        tst      r12, #0xff0000         @ Q0a
        add      r0, #1
        bne      3f
        vst1.8   {d18[0]}, [r0], r1
        vst1.8   {d18[1]}, [r0], r1
        vst1.8   {d18[2]}, [r0], r1
        vst1.8   {d18[3]}, [r0], r1
        sub      r0, r0, r1, lsl #2

3:
        tst      r12, #0xff000000       @ Q0b
        add      r0, r0, r1, lsl #2
        bne      4f
        vst1.8   {d18[4]}, [r0], r1
        vst1.8   {d18[5]}, [r0], r1
        vst1.8   {d18[6]}, [r0], r1
        vst1.8   {d18[7]}, [r0], r1
        sub      r0, r0, r1, lsl #2

4:
        tst      r12, #0xff00           @ P0b
        it ne
        bxne     lr

        sub      r0, #1
        vst1.8   {d17[4]}, [r0], r1
        vst1.8   {d17[5]}, [r0], r1
        vst1.8   {d17[6]}, [r0], r1
        vst1.8   {d17[7]}, [r0], r1
        bx       lr

endfunc


.macro m_filter_v_chroma_16 bit_depth
        hevc_loop_filter_chroma_start

        sub      r0, #4
        vld4.16  {d16[0], d18[0], d20[0], d22[0]}, [r0], r1
        vld4.16  {d16[1], d18[1], d20[1], d22[1]}, [r0], r1
        vld4.16  {d16[2], d18[2], d20[2], d22[2]}, [r0], r1
        vld4.16  {d16[3], d18[3], d20[3], d22[3]}, [r0], r1
        vld4.16  {d17[0], d19[0], d21[0], d23[0]}, [r0], r1
        vld4.16  {d17[1], d19[1], d21[1], d23[1]}, [r0], r1
        vld4.16  {d17[2], d19[2], d21[2], d23[2]}, [r0], r1
        vld4.16  {d17[3], d19[3], d21[3], d23[3]}, [r0], r1

        sub      r0, r0, r1, lsl #3
        add      r0, r0, #2
        hevc_loop_filter_chroma_body_16 q8, q9, q10, q11, \bit_depth
        bne      1f

        vst2.16  {d18[0], d20[0]}, [r0], r1
        vst2.16  {d18[1], d20[1]}, [r0], r1
        vst2.16  {d18[2], d20[2]}, [r0], r1
        vst2.16  {d18[3], d20[3]}, [r0], r1
        vst2.16  {d19[0], d21[0]}, [r0], r1
        vst2.16  {d19[1], d21[1]}, [r0], r1
        vst2.16  {d19[2], d21[2]}, [r0], r1
        vst2.16  {d19[3], d21[3]}, [r0], r1
        bx       lr

1:
        tst      r12, #0xff             @ P0a
        bne      2f

        vst1.16  {d18[0]}, [r0], r1
        vst1.16  {d18[1]}, [r0], r1
        vst1.16  {d18[2]}, [r0], r1
        vst1.16  {d18[3]}, [r0], r1
        sub      r0, r0, r1, lsl #2

2:
        tst      r12, #0xff0000         @ Q0a
        add      r0, #1
        bne      3f
        vst1.16  {d20[0]}, [r0], r1
        vst1.16  {d20[1]}, [r0], r1
        vst1.16  {d20[2]}, [r0], r1
        vst1.16  {d20[3]}, [r0], r1
        sub      r0, r0, r1, lsl #2

3:
        tst      r12, #0xff000000       @ Q0b
        add      r0, r0, r1, lsl #2
        bne      4f
        vst1.16  {d21[0]}, [r0], r1
        vst1.16  {d21[1]}, [r0], r1
        vst1.16  {d21[2]}, [r0], r1
        vst1.16  {d21[3]}, [r0], r1
        sub      r0, r0, r1, lsl #2

4:
        tst      r12, #0xff00           @ P0b
        it ne
        bxne     lr

        sub      r0, #1
        vst1.16  {d19[0]}, [r0], r1
        vst1.16  {d19[1]}, [r0], r1
        vst1.16  {d19[2]}, [r0], r1
        vst1.16  {d19[3]}, [r0], r1
        bx       lr
.endm


@ void ff_hevc_h_loop_filter_chroma_neon(
@   uint8_t *_pix,     [r0]
@   ptrdiff_t _stride, [r1]
@   int *_tc,          [r2]
@   uint8_t *_no_p,    [r3]
@   uint8_t *_no_q);   [sp+0]

function ff_hevc_h_loop_filter_chroma_neon, export=1
        hevc_loop_filter_chroma_start
        sub      r0, r0, r1, lsl #1
        vld1.8   {d16}, [r0], r1
        vld1.8   {d17}, [r0], r1
        vld1.8   {d18}, [r0], r1
        vld1.8   {d19}, [r0]
        sub      r0, r0, r1, lsl #1
        hevc_loop_filter_chroma_body d16, d17, d18, d19
        bne      1f     @ Partial write
        vst1.8   {d17}, [r0], r1
        vst1.8   {d18}, [r0]
        bx       lr
1:
        tst      r12, #0xff
        vmov     r2, r3, d17
        it eq
        streq    r2, [r0]
        tst      r12, #0xff00
        it eq
        streq    r3, [r0, #4]

        add      r0, r1
        tst      r12, #0xff0000
        vmov     r2, r3, d18
        it eq
        streq    r2, [r0]
        tst      r12, #0xff000000
        it eq
        streq    r3, [r0, #4]

        bx       lr
endfunc

.macro m_filter_h_chroma_16 bit_depth
        hevc_loop_filter_chroma_start
        sub      r0, r0, r1, lsl #1
        vld1.16  {q8}, [r0], r1
        vld1.16  {q9}, [r0], r1
        vld1.16  {q10}, [r0], r1
        vld1.16  {q11}, [r0]
        sub      r0, r0, r1, lsl #1
        hevc_loop_filter_chroma_body_16 q8, q9, q10, q11, \bit_depth
        bne      1f     @ Partial write
        vst1.16  {q9}, [r0], r1
        vst1.16  {q10}, [r0]
        bx       lr
1:
        tst      r12, #0xff
        bne      2f
        vst1.16  {d18}, [r0]
2:
        tst      r12, #0xff00
        bne      3f
        add      r0, #8
        vst1.16  {d19}, [r0]
        sub      r0, #8
3:
        tst      r12, #0xff0000
        add      r0, r1
        bne      4f
        vst1.16  {d20}, [r0]
4:
        tst      r12, #0xff000000
        it ne
        bxne     lr
        add      r0, #8
        vst1.16  {d21}, [r0]

        bx       lr
.endm


/* ff_hevc_deblocking_boundary_strengths_neon(int pus, int dup, int in_i
 *                                            int *curr_rpl0, int *curr_
 *                                            MvField *curr, MvField *ne
 */
function ff_hevc_deblocking_boundary_strengths_neon, export=1
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

function ff_hevc_h_loop_filter_luma_neon_10, export=1
        m_filter_h_luma_16 10
endfunc

function ff_hevc_v_loop_filter_luma2_neon_10, export=1
        hevc_loop_filter_luma_start
        push     {r4-r10,lr}       @ 8 regs = 32 bytes

        ldr      r4, [sp, #40]
        b        v_loop_luma_common_10
endfunc

function ff_hevc_v_loop_filter_luma_neon_10, export=1
        hevc_loop_filter_luma_start
        push     {r4-r10,lr}

        sub      r4, r0, #8
v_loop_luma_common_10:
        m_filter_v_luma_common_16 10
endfunc

function ff_hevc_h_loop_filter_uv_neon_10, export=1
        m_filter_h_uv_16 10
endfunc

function ff_hevc_v_loop_filter_uv2_neon_10, export=1
        m_filter_v_uv2_16 10
endfunc

function ff_hevc_h_loop_filter_chroma_neon_10, export=1
        m_filter_h_chroma_16 10
endfunc

function ff_hevc_v_loop_filter_chroma_neon_10, export=1
        m_filter_v_chroma_16 10
endfunc

