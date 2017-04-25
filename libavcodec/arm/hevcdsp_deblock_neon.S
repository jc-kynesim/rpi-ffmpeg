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
        ldr      r3, [r2, #4]
        add      r2, r3, r12
        cmp      r2, #0
        it       eq
        bxeq     lr
.endm

@ Uses: d2, d4, d18, d19
@ Returns: d2, d4
@ Modifies: d0-d7, d22-d25
.macro hevc_loop_filter_chroma_body
        vsubl.u8  q3, d4, d2
        vsubl.u8  q11, d18, d19
        vshl.i16  q3, #2
        vadd.i16  q11, q3
        vdup.16   d0, r12
        vdup.16   d1, r3
        vrshr.s16 q11, q11, #3
        vneg.s16  q12, q0
        vmovl.u8  q2, d4
        vmin.s16  q11, q11, q0
        vmax.s16  q11, q11, q12
        vaddw.u8  q1, q11, d2
        vsub.i16  q2, q11
        vqmovun.s16 d2, q1
        vqmovun.s16 d4, q2
.endm


@ Uses r2[0:7], r2[8:15]
@ Modifies: d0-d7, d22-d25
.macro hevc_loop_filter_uv_body P1, P0, Q0, Q1
        vsubl.u8  q3, \Q0, \P0
        vsubl.u8  q11, \P1, \Q0
        vshl.i16  q3, #2
        vadd.i16  q11, q3

        @ r2[0:7] -> d0.16 (all), r2[8:15] -> d1.16(all)
        vdup.16   d0, r2
        vmovl.u8  q0, d0
        vuzp.16   d0, d1

        vrshr.s16 q11, q11, #3
        vneg.s16  q12, q0
        vmovl.u8  q2, \Q0
        vmin.s16  q11, q11, q0
        vmax.s16  q11, q11, q12
        vaddw.u8  q1, q11, \P0
        vsub.i16  q2, q11
        vqmovun.s16 \P0, q1
        vqmovun.s16 \Q0, q2
.endm



.macro hevc_loop_filter_luma_start
        ldr     r12, [r3]
        ldr      r3, [r3, #4]
        lsl      r3, #16
        orr      r3, r12
        cmp      r3, #0
        it       eq
        bxeq     lr
        lsr      r3, #16
.endm

@ Uses: r2, r3, r12
@ Modifies: r5, r6, r7, r8, r9
function hevc_loop_filter_luma_body
        vmovl.u8  q15, d23
        vmovl.u8  q14, d22
        vmovl.u8  q13, d21
        vmovl.u8  q12, d20
        vmovl.u8  q11, d19
        vmovl.u8  q10, d18
        vmovl.u8  q9, d17
        vmovl.u8  q8, d16

        vadd.i16   q7, q9, q11
        vadd.i16   q6, q14, q12
        vsub.i16   q7, q10
        vsub.i16   q6, q13
        vabd.s16   q7, q7, q10
        vabd.s16   q6, q6, q13

        vdup.16    q0, r2
        vmov       q4, q7
        vmov       q5, q6
        vdup.16    d4, r12
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
        beq        weakfilter_

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

weakfilter_:
        mvn       r8, r8
        and       r9, r8, r7
        cmp       r9, #0
        beq       ready_

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

ready_:
        vqmovun.s16 d16, q8
        vqmovun.s16 d17, q9
        vqmovun.s16 d18, q10
        vqmovun.s16 d19, q11
        vqmovun.s16 d20, q12
        vqmovun.s16 d21, q13
        vqmovun.s16 d22, q14
        vqmovun.s16 d23, q15
        mov       pc, lr
endfunc

@ ff_hevc_v_loop_filter_luma2_neon(src (r0), stride (r1), beta (r2), tc (r3), np_p (sp[0]), no_q (sp[4]), src2 (sp[8]))
function ff_hevc_v_loop_filter_luma2_neon, export=1
        hevc_loop_filter_luma_start
        push     {r4-r10,lr}       @ 8 regs = 32 bytes
        ldr      r4, [sp, #40]
        b        v_loop_luma_common
endfunc


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

        vst4.8  {d16[7],d17[7],d18[7],d19[7]}, [r4:32], r1
        vst4.8  {d20[7],d21[7],d22[7],d23[7]}, [r0:32], r1

        vst4.8  {d16[6],d17[6],d18[6],d19[6]}, [r4:32], r1
        vst4.8  {d20[6],d21[6],d22[6],d23[6]}, [r0:32], r1

        vst4.8  {d16[5],d17[5],d18[5],d19[5]}, [r4:32], r1
        vst4.8  {d20[5],d21[5],d22[5],d23[5]}, [r0:32], r1

        vst4.8  {d16[4],d17[4],d18[4],d19[4]}, [r4:32], r1
        vst4.8  {d20[4],d21[4],d22[4],d23[4]}, [r0:32], r1

        vst4.8  {d16[3],d17[3],d18[3],d19[3]}, [r4:32], r1
        vst4.8  {d20[3],d21[3],d22[3],d23[3]}, [r0:32], r1

        vst4.8  {d16[2],d17[2],d18[2],d19[2]}, [r4:32], r1
        vst4.8  {d20[2],d21[2],d22[2],d23[2]}, [r0:32], r1

        vst4.8  {d16[1],d17[1],d18[1],d19[1]}, [r4:32], r1
        vst4.8  {d20[1],d21[1],d22[1],d23[1]}, [r0:32], r1

        vst4.8  {d16[0],d17[0],d18[0],d19[0]}, [r4:32]
        vst4.8  {d20[0],d21[0],d22[0],d23[0]}, [r0:32]

        vpop     {d8-d15}
        pop      {r4-r10,pc}
endfunc

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

        neg     r1, r1
        add     r0, r0, r1
        vst1.8  {d22}, [r0], r1
        vst1.8  {d21}, [r0], r1
        vst1.8  {d20}, [r0], r1
        vst1.8  {d19}, [r0], r1
        vst1.8  {d18}, [r0], r1
        vst1.8  {d17}, [r0]

bypasswrite:
        vpop     {d8-d15}
        pop      {r4-r10,pc}
endfunc

function ff_hevc_h_loop_filter_uv_neon, export=1
        sub      r0, r0, r1, lsl #1
        vld2.8   {d16,d17}, [r0], r1
        vld2.8   {d18,d19}, [r0], r1
        vld2.8   {d26,d27}, [r0], r1
        vld2.8   {d28,d29}, [r0]
        sub      r0, r0, r1, lsl #1
        hevc_loop_filter_uv_body d16, d18, d26, d28
        lsr      r2, r2, #16
        hevc_loop_filter_uv_body d17, d19, d27, d29
        vst2.8   {d18,d19}, [r0], r1
        vst2.8   {d26,d27}, [r0]
        bx       lr
endfunc


@ void ff_hevc_v_loop_filter_uv2_neon(uint8_t * src_r,       // r0
@                                     unsigned int stride,   // r1
@                                     uint32_t tc4,          // r2
@                                     const uint8_t no_p[2], // r3
@                                     const uint8_t no_q[2], // sp[0]
@                                     uint8_t * src_l);      // sp[4]

function ff_hevc_v_loop_filter_uv2_neon, export=1
        ldr      r3, [sp, #4]

        vld4.8   {d16[0], d17[0], d18[0], d19[0]}, [r3], r1
        vld4.8   {d26[0], d27[0], d28[0], d29[0]}, [r0], r1

        vld4.8   {d16[1], d17[1], d18[1], d19[1]}, [r3], r1
        vld4.8   {d26[1], d27[1], d28[1], d29[1]}, [r0], r1

        vld4.8   {d16[2], d17[2], d18[2], d19[2]}, [r3], r1
        vld4.8   {d26[2], d27[2], d28[2], d29[2]}, [r0], r1

        vld4.8   {d16[3], d17[3], d18[3], d19[3]}, [r3], r1
        vld4.8   {d26[3], d27[3], d28[3], d29[3]}, [r0], r1

        vld4.8   {d16[4], d17[4], d18[4], d19[4]}, [r3], r1
        vld4.8   {d26[4], d27[4], d28[4], d29[4]}, [r0], r1

        vld4.8   {d16[5], d17[5], d18[5], d19[5]}, [r3], r1
        vld4.8   {d26[5], d27[5], d28[5], d29[5]}, [r0], r1

        vld4.8   {d16[6], d17[6], d18[6], d19[6]}, [r3], r1
        vld4.8   {d26[6], d27[6], d28[6], d29[6]}, [r0], r1

        vld4.8   {d16[7], d17[7], d18[7], d19[7]}, [r3]
        vld4.8   {d26[7], d27[7], d28[7], d29[7]}, [r0]

        hevc_loop_filter_uv_body d16, d18, d26, d28
        lsr      r2, r2, #16
        hevc_loop_filter_uv_body d17, d19, d27, d29

        neg      r1, r1

        vld4.8   {d16[7], d17[7], d18[7], d19[7]}, [r3], r1
        vld4.8   {d26[7], d27[7], d28[7], d29[7]}, [r0], r1

        vld4.8   {d16[6], d17[6], d18[6], d19[6]}, [r3], r1
        vld4.8   {d26[6], d27[6], d28[6], d29[6]}, [r0], r1

        vld4.8   {d16[5], d17[5], d18[5], d19[5]}, [r3], r1
        vld4.8   {d26[5], d27[5], d28[5], d29[5]}, [r0], r1

        vld4.8   {d16[4], d17[4], d18[4], d19[4]}, [r3], r1
        vld4.8   {d26[4], d27[4], d28[4], d29[4]}, [r0], r1

        vld4.8   {d16[3], d17[3], d18[3], d19[3]}, [r3], r1
        vld4.8   {d26[3], d27[3], d28[3], d29[3]}, [r0], r1

        vld4.8   {d16[2], d17[2], d18[2], d19[2]}, [r3], r1
        vld4.8   {d26[2], d27[2], d28[2], d29[2]}, [r0], r1

        vld4.8   {d16[1], d17[1], d18[1], d19[1]}, [r3], r1
        vld4.8   {d26[1], d27[1], d28[1], d29[1]}, [r0], r1

        vld4.8   {d16[0], d17[0], d18[0], d19[0]}, [r3]
        vld4.8   {d26[0], d27[0], d28[0], d29[0]}, [r0]

        bx       lr
endfunc


function ff_hevc_v_loop_filter_chroma_neon, export=1
        hevc_loop_filter_chroma_start
        sub      r0, #4
        vld1.8   {d16}, [r0], r1
        vld1.8   {d17}, [r0], r1
        vld1.8   {d18}, [r0], r1
        vld1.8   {d2},  [r0], r1
        vld1.8   {d4},  [r0], r1
        vld1.8   {d19}, [r0], r1
        vld1.8   {d20}, [r0], r1
        vld1.8   {d21}, [r0], r1
        sub      r0, r0, r1, lsl #3
        transpose_8x8 d16, d17, d18, d2, d4, d19, d20, d21
        hevc_loop_filter_chroma_body
        transpose_8x8 d16, d17, d18, d2, d4, d19, d20, d21
        vst1.8   {d16}, [r0], r1
        vst1.8   {d17}, [r0], r1
        vst1.8   {d18}, [r0], r1
        vst1.8   {d2},  [r0], r1
        vst1.8   {d4},  [r0], r1
        vst1.8   {d19}, [r0], r1
        vst1.8   {d20}, [r0], r1
        vst1.8   {d21}, [r0]
        bx       lr
endfunc

function ff_hevc_h_loop_filter_chroma_neon, export=1
        hevc_loop_filter_chroma_start
        sub      r0, r0, r1, lsl #1
        vld1.8   {d18}, [r0], r1
        vld1.8   {d2}, [r0], r1
        vld1.8   {d4}, [r0], r1
        vld1.8   {d19}, [r0]
        sub      r0, r0, r1, lsl #1
        hevc_loop_filter_chroma_body
        vst1.8   {d2}, [r0], r1
        vst1.8   {d4}, [r0]
        bx       lr
endfunc

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

