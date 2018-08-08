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

#include "libavutil/arm/asm.S"
#include "neon.S"

@ All functions have the call
@
@ int ff_hevc_rpi_intra_filter_N_neon_PW(
@    pixel * const left,                   [r0]
@    pixel * const top,                    [r1]
@    const unsigned int req,               [r2]
@    const unsigned int avail,             [r3]
@    const pixel * const src_l,            [sp, #0]
@    const pixel * const src_u,            [sp, #4]
@    const pixel * const src_ur,           [sp, #8]
@    const unsigned int stride,            [sp, #12] (pels)
@    const unsigned int top_right_size,    [sp, #16]
@    const unsigned int down_left_size)    [sp, #20]
@
@ Assumptions:
@ (that wouldn't apply to all frame layoouts but do apply to sand, so beware
@  if reuseing this code)
@
@ Min ctb size is 8 so we don't need to worry about tr_size or dl_size for
@ N==4, but do for chroma N>=8.  As we share Y/C fns that means we can ignore
@ N==8,PW=8 (chroma always PW>8) but have to cope for larger
@
@ We always have at least 64 pixel H frame width rounding - this lets us
@ load UR widthout having to worry about exactly how many pixels are actually
@ within the frame.  As partial loads will only occur very occasionally this
@ should be a win in nearly all cases.
@
@ 16 bit fns can be used as 8 bit chroma fns as chroma never filters
@ so we do no maths on the contents
@
@ No filtering in 32bit fns as they are chroma only


.equ    AVAIL_UR, 1
.equ    AVAIL_U,  2
.equ    AVAIL_UL, 4
.equ    AVAIL_L,  8
.equ    AVAIL_DL, 16

.equ    FILTER_LIGHT, 0x40
.equ    FILTER_STRONG, 0x80

.equ    AVAIL_S_UR_N_U_C, 32 - 1
.equ    AVAIL_S_U_N_UL_C, 32 - 2
.equ    AVAIL_S_UL_N_L_C, 32 - 3
.equ    AVAIL_S_L_N_DL_C, 32 - 4

@ On entry
@  r2   req
@  r3   avail
@ [sp, #sp_offset...]  args
@
@ On Exit:
@
@ Extend values:
@  d_l  scalar contains value for L & DL
@       if DL avail then this is is DL[0] so we don't need to load that
@  d_ul scalar containing value for UL
@  d_u  scalar containing value for U
@  d_ur scalar containing value for UR
@ If DL avail then d_l == b_dl elif L avail then d_l == a_l else...
@ This means that L-light-filter works even if nreq DL (we never filter
@ req-DL without req-L, but we do filter req-L without req-DL)
@ If UR avail then d_ur == a_ur so U-filter good too
@
@ Data load pointers (only load if req & avail):
@  r8   DL + stride
@  r6   L
@  r7   U
@  r4   UR
@
@ Others:
@  r2   req (if preserve_req)
@  r3   req & avail (if preserve_req)
@  r2   req & avail (if !preserve_req)
@  r10  L + stride
@  r5   DL + stride * 2
@  r12  stride * 2
@  cs   Load U
@  mi   Load UR
@
@ Clobbered:
@  r9, lr

.macro  load_pointers pw_s, log2_s, sp_offset, d_type, d_l, d_ul, d_u, d_ur, preserve_req, I1, I2

.equ    src_l,   \sp_offset + 0
.equ    src_u,   \sp_offset + 4
.equ    src_ur,  \sp_offset + 8
.equ    stride,  \sp_offset + 12
.equ    pw,      (1 << \pw_s)                 @ pel width in bytes
.equ    b_size,  (1 << (\pw_s + \log2_s))     @ size in bytes

        ldrd        r4, r5, [sp, #src_ur] @ and stride
        ldrd        r6, r7, [sp, #src_l]  @ and src_u
        lsls        lr, r3, #AVAIL_S_U_N_UL_C
        mov         r8, r4
        sub         r9, r6, r5
        it          mi
        movmi       r8, r7
        it          cs
        movcs       r8, r9
        lsls        lr, r3, #AVAIL_S_L_N_DL_C
        ite         pl
        movpl       r6, r8
        addmi       r8, r9, r5, lsl #\log2_s
        it          cs
        addcs       r8, r6, r5, lsl #\log2_s
        .if !\preserve_req
        and         r2, r2, r3
        .endif
        add         r10, r6, r5
        lsl         r12, r5, #1
        lsls        lr, r3, #AVAIL_S_U_N_UL_C
        it          cc
        movcc       r9, r6
        vld1.\d_type {\d_l}, [r8], r5
        add         lr, r7, #b_size - pw
        add         r5, r8, r5
        itt         pl
        movpl       lr, r9
        movpl       r7, r9
        tst         r3, #AVAIL_UR
        vld1.\d_type {\d_ul}, [r9]
        it          eq
        moveq       r4, lr
        \I1
        .if \preserve_req
        and         r3, r2, r3
        .else
        lsls        lr, r2, #AVAIL_S_UR_N_U_C
        .endif
        vld1.\d_type {\d_u}, [r7]
        \I2
        vld1.\d_type {\d_ur}, [r4]
        .if \preserve_req
        lsls        lr, r3, #AVAIL_S_UR_N_U_C
        .endif
.endm



@ int ff_hevc_rpi_intra_filter_4_neon_8(
@    pixel * const left,                   [r0]
@    pixel * const top,                    [r1]
@    const unsigned int req,               [r2]
@    const unsigned int avail,             [r3]
@    const pixel * const src_l,            [sp, #0]
@    const pixel * const src_u,            [sp, #4]
@    const pixel * const src_ur,           [sp, #8]
@    const unsigned int stride,            [sp, #12] (pels)
@    const unsigned int top_right_size,    [sp, #16]
@    const unsigned int down_left_size)    [sp, #20]

.set    sp_base, 8*4
.set    pw_s,    0
.set    pw,      (1 << pw_s)
.set    log2_s,  2

function ff_hevc_rpi_intra_filter_4_neon_8, export=1
        push        {r4-r10, lr}
        load_pointers pw_s, log2_s, sp_base, 8, "d0[],d1[]", d2[0], d3[], d4[], 0

        sub         r3, r0, #pw
        it          mi
        vldrmi      s7, [r4]
        it          cs
        vldrcs      s6, [r7]
        it          pl
        vmovpl.f32  s7, s8
        lsls        lr, r2, #AVAIL_S_L_N_DL_C
        bpl         1f
        vld1.8      {d0[0]}, [r6], r12
        vld1.8      {d1[0]}, [r10], r12
        vld1.8      {d0[1]}, [r6]
        vld1.8      {d1[1]}, [r10]
1:
        bcc         1f
        vld1.8      {d1[2]}, [r8], r12
        vld1.8      {d0[3]}, [r5]
        vld1.8      {d1[3]}, [r8]
1:
        vst1.8      {d2[0]}, [r3]
        vst1.8      {d3}, [r1]
        vzip.8      d0, d1
        vst1.8      {d0}, [r0]
        pop         {r4-r10, pc}
endfunc


@ int ff_hevc_rpi_intra_filter_4_neon_16(
@    pixel * const left,                   [r0]
@    pixel * const top,                    [r1]
@    const unsigned int req,               [r2]
@    const unsigned int avail,             [r3]
@    const pixel * const src_l,            [sp, #0]
@    const pixel * const src_u,            [sp, #4]
@    const pixel * const src_ur,           [sp, #8]
@    const unsigned int stride,            [sp, #12] (pels)
@    const unsigned int top_right_size,    [sp, #16]
@    const unsigned int down_left_size)    [sp, #20]

.set    sp_base, 8*4
.set    pw_s,    1
.set    pw,      (1 << pw_s)
.set    log2_s,  2

function ff_hevc_rpi_intra_filter_4_neon_16, export=1
        push        {r4-r10, lr}
        load_pointers pw_s, log2_s, sp_base, 16, "d0[],d1[]", d2[0], d3[], d4[], 0

        sub         r3, r0, #pw
        it          mi
        vldrmi      d4, [r4]
        it          cs
        vldrcs      d3, [r7]
        lsls        lr, r2, #AVAIL_S_L_N_DL_C
        bpl         1f
        vld1.16     {d0[0]}, [r6], r12
        vld1.16     {d1[0]}, [r10], r12
        vld1.16     {d0[1]}, [r6]
        vld1.16     {d1[1]}, [r10]
1:
        bcc         1f
        vld1.16     {d1[2]}, [r8], r12
        vld1.16     {d0[3]}, [r5]
        vld1.16     {d1[3]}, [r8]
1:
        vst1.16     {d2[0]}, [r3]
        vst1.16     {d3, d4}, [r1]
        vzip.16     d0, d1
        vst1.16     {q0}, [r0]
        pop         {r4-r10, pc}
endfunc


@ int ff_hevc_rpi_intra_filter_8_neon_8(
@    pixel * const left,                   [r0]
@    pixel * const top,                    [r1]
@    const unsigned int req,               [r2]
@    const unsigned int avail,             [r3]
@    const pixel * const src_l,            [sp, #0]
@    const pixel * const src_u,            [sp, #4]
@    const pixel * const src_ur,           [sp, #8]
@    const unsigned int stride,            [sp, #12] (pels)
@    const unsigned int top_right_size,    [sp, #16]
@    const unsigned int down_left_size)    [sp, #20]

.set    sp_base, 8*4
.set    pw_s,    0
.set    pw,      (1 << pw_s)
.set    log2_s,  3

function ff_hevc_rpi_intra_filter_8_neon_8, export=1
        push        {r4-r10, lr}
        load_pointers pw_s, log2_s, sp_base, 8, "d0[],d1[]", d3[7], d4[], d5[], 1

        it          mi
        vldrmi      d5, [r4]
        sub         r0, #pw
        it          cs
        vldrcs      d4, [r7]
        lsls        lr, r3, #AVAIL_S_L_N_DL_C
        bpl         1f
        vld1.8      {d0[0]}, [r6], r12
        vld1.8      {d1[0]}, [r10], r12
        vld1.8      {d0[1]}, [r6], r12
        vld1.8      {d1[1]}, [r10], r12
        vld1.8      {d0[2]}, [r6], r12
        vld1.8      {d1[2]}, [r10], r12
        vld1.8      {d0[3]}, [r6]
        vld1.8      {d1[3]}, [r10]
1:
        bcc         1f
        vld1.8      {d1[4]}, [r8], r12
        vld1.8      {d0[5]}, [r5], r12
        vld1.8      {d1[5]}, [r8], r12
        vld1.8      {d0[6]}, [r5], r12
        vld1.8      {d1[6]}, [r8], r12
        vld1.8      {d0[7]}, [r5], r12
        vld1.8      {d1[7]}, [r8], r12
1:
        vext.8      q3, q1, q2, #15
        vmov.u8     r4, d5[7]           @ Save final pel
        tst         r2, #FILTER_LIGHT
        vzip.8      d0, d1
        beq         1f

        @ Luma light filter
        vaddl.u8    q8, d7, d5
        vext.8      q1, q1, q0, #15
        vaddl.u8    q2, d6, d4
        vaddl.u8    q3, d3, d1
        vaddl.u8    q9, d2, d0
        vext.16     q10, q8, q8, #1
        vext.16     q11, q3, q3, #1
        vadd.u16    q10, q8
        vadd.u16    q11, q3
        vadd.u16    d2, d4, d18         @ d2[0] = l[0] + 2ul + u[0]
        vmov.u8     r5, d1[7]           @ Save final pel
        vext.16     q0, q2, q8, #1
        vext.16     q3, q9, q3, #1
        vadd.u16    q8, q0, q2
        vadd.u16    q3, q9
        vrshrn.u16  d5, q10, #2
        vrshrn.u16  d1, q11, #2
        vrshr.u16   d2, #2
        vrshrn.u16  d4, q8, #2
        vrshrn.u16  d0, q3, #2
        vmov.8      d5[7], r4           @ Restore final pel
        vmov.8      d1[7], r5           @ Restore final pel
        vdup.8      d3, d2[0]
1:
        vst1.8      {d3[7]}, [r0]!
        vst1.8      {q2}, [r1]
        vst1.8      {q0}, [r0]
        pop         {r4-r10, pc}
endfunc


@ int ff_hevc_rpi_intra_filter_8_neon_16(
@    pixel * const left,                   [r0]
@    pixel * const top,                    [r1]
@    const unsigned int req,               [r2]
@    const unsigned int avail,             [r3]
@    const pixel * const src_l,            [sp, #0]
@    const pixel * const src_u,            [sp, #4]
@    const pixel * const src_ur,           [sp, #8]
@    const unsigned int stride,            [sp, #12] (pels)
@    const unsigned int top_right_size,    [sp, #16]
@    const unsigned int down_left_size)    [sp, #20]

.set    sp_base, 8*4
.set    ur_size, sp_base + 16
.set    dl_size, sp_base + 20
.set    pw_s,    1
.set    pw,      (1 << pw_s)
.set    log2_s,  3
.set    p_size,  (1 << log2_s)          @ size in pels

function ff_hevc_rpi_intra_filter_8_neon_16, export=1
        push        {r4-r10, lr}
        load_pointers pw_s, log2_s, sp_base, 16, "d0[],d1[]", d5[3], "d16[],d17[]", "d18[],d19[]", 1, \
            "ldr         r9, [sp, #ur_size]", \
            "sub         r0, #pw"

        vmov        q1, q0
        ldrh        lr, [r4, #3*2]
        it          mi
        vldmmi      r4, {d18, d19}
        it          cs
        vldmcs      r7, {d16, d17}
        itt         mi
        cmpmi       r9, #p_size
        vdupmi.16   d19, lr
        lsls        lr, r3, #AVAIL_S_L_N_DL_C
        bpl         1f
        vld1.16     {d0[0]}, [r6], r12
        vld1.16     {d2[0]}, [r10], r12
        vld1.16     {d0[1]}, [r6], r12
        vld1.16     {d2[1]}, [r10], r12
        vld1.16     {d0[2]}, [r6], r12
        vld1.16     {d2[2]}, [r10], r12
        vld1.16     {d0[3]}, [r6]
        vld1.16     {d2[3]}, [r10]
1:
        ldr         lr, [sp, #dl_size]
        bcc         2f
        vld1.16     {d3[0]}, [r8], r12
        vld1.16     {d1[1]}, [r5], r12
        cmp         lr, #p_size
        vld1.16     {d3[1]}, [r8], r12
        bcc         10f
        vld1.16     {d1[2]}, [r5], r12
        vld1.16     {d3[2]}, [r8], r12
        vld1.16     {d1[3]}, [r5]
        vld1.16     {d3[3]}, [r8]
2:
        vext.16     q3, q8, q9, #7
        vext.16     q10, q2, q8, #7
        tst         r2, #FILTER_LIGHT
        vzip.16     q0, q1
        beq         3f

        @ Luma light filter
        vadd.i16    q3, q9
        vext.16     q11, q0, q1, #7
        vext.16     q2, q2, q0, #7
        vadd.i16    q8, q10
        vadd.i16    q10, q11, q1
        vadd.i16    q0, q2
        vext.16     q11, q3, q3, #1
        vadd.i16    d4, d16, d0         @ d4[0] = l[0] + 2ul + u[0]
        vmov.u16    r4, d19[3]          @ Save final pel
        vext.16     q9, q10, q10, #1
        vext.16     q12, q8, q3, #1
        vext.16     q13, q0, q10, #1
        vadd.i16    q3, q11
        vadd.i16    q10, q9
        vadd.i16    q8, q12
        vadd.i16    q0, q13
        vmov.u16    r5, d3[3]           @ Save final pel
        vrshr.u16   d4, d4, #2
        vrshr.u16   q9, q3, #2
        vrshr.u16   q1, q10, #2
        vrshr.u16   q8, #2
        vrshr.u16   q0, #2
        vmov.16     d19[3], r4          @ Restore final pel
        vmov.16     d3[3], r5           @ Restore final pel
        vdup.16     d5, d4[0]
3:
        vst1.16     {d5[3]}, [r0]!
        vst1.16     {q8-q9}, [r1]
        vst1.16     {q0-q1}, [r0]
        pop         {r4-r10, pc}

10:
A       ldrh        r9, [r8, -r12]
T       sub         r9, r8, r12
T       ldrh        r9, [r9]
        orr         r9, r9, r9, lsl #16
        vmov.32     d1[1], r9
        vmov.32     d3[1], r9
        b           2b
endfunc

@ int ff_hevc_rpi_intra_filter_16_neon_16(
@    pixel * const left,                   [r0]
@    pixel * const top,                    [r1]
@    const unsigned int req,               [r2]
@    const unsigned int avail,             [r3]
@    const pixel * const src_l,            [sp, #0]
@    const pixel * const src_u,            [sp, #4]
@    const pixel * const src_ur,           [sp, #8]
@    const unsigned int stride,            [sp, #12] (pels)
@    const unsigned int top_right_size,    [sp, #16]
@    const unsigned int down_left_size)    [sp, #20]

.set    sp_base, 8*4
.set    ur_size, sp_base + 16
.set    dl_size, sp_base + 20
.set    pw_s,    1
.set    pw,      (1 << pw_s)
.set    log2_s,  4
.set    p_size,  (1 << log2_s)          @ size in pels

function ff_hevc_rpi_intra_filter_16_neon_16, export=1
        push        {r4-r10, lr}
        load_pointers pw_s, log2_s, sp_base, 16, "d4[],d5[]", d17[3], "d18[],d19[]", "d22[],d23[]", 1, \
            "ldr         r9, [sp, #ur_size]", \
            "sub         r0, #pw"

        vmov       q10, q9
        ldr        lr, [sp, #dl_size]
        vmov       q12, q11
        it         cs
        vldmcs     r7, {q9-q10}
        @ Given chroma frame layout, if UR exists then it is always legit to
        @ load all of it even if most of it is outside the frame.
        itt        mi
        vldmmi     r4, {q11-q12}
        cmpmi      r9, #p_size
        bmi        10f
1:
        lsls       r3, #AVAIL_S_L_N_DL_C
        bpl        20f
        vld1.16    {d0[0]}, [r6], r12
        vld1.16    {d2[0]}, [r10], r12
        vld1.16    {d0[1]}, [r6], r12
        vld1.16    {d2[1]}, [r10], r12
        vld1.16    {d0[2]}, [r6], r12
        vld1.16    {d2[2]}, [r10], r12
        vld1.16    {d0[3]}, [r6], r12
        vld1.16    {d2[3]}, [r10], r12
        vld1.16    {d1[0]}, [r6], r12
        vld1.16    {d3[0]}, [r10], r12
        vld1.16    {d1[1]}, [r6], r12
        vld1.16    {d3[1]}, [r10], r12
        vld1.16    {d1[2]}, [r6], r12
        vld1.16    {d3[2]}, [r10], r12
        vld1.16    {d1[3]}, [r6]
        vld1.16    {d3[3]}, [r10]
2:      bcc        30f
        vld1.16    {d6[0]}, [r8], r12
        vld1.16    {d4[1]}, [r5], r12
        cmp        lr, #p_size
        vld1.16    {d6[1]}, [r8], r12
        bcc        40f
        vld1.16    {d4[2]}, [r5], r12
        vld1.16    {d6[2]}, [r8], r12
        vld1.16    {d4[3]}, [r5], r12
        vld1.16    {d6[3]}, [r8], r12
        vld1.16    {d5[0]}, [r5], r12
        vld1.16    {d7[0]}, [r8], r12
        vld1.16    {d5[1]}, [r5], r12
        vld1.16    {d7[1]}, [r8], r12
        vld1.16    {d5[2]}, [r5], r12
        vld1.16    {d7[2]}, [r8], r12
        vld1.16    {d5[3]}, [r5]
        vld1.16    {d7[3]}, [r8]
3:
        vzip.16    q0, q1
        tst        r2, #FILTER_LIGHT
        vzip.16    q2, q3
        beq        4f

        vext.16    q13, q8, q0, #7
        vadd.i16   q13, q0
        vext.16    q0, q0, q1, #7
        vadd.i16   q0, q1
        vext.16    q1, q1, q2, #7
        vadd.i16   q1, q2
        vext.16    q2, q2, q3, #7
        vadd.i16   q2, q3
        vext.16    q14, q8, q9, #7
        vadd.i16   q14, q9
        vext.16    q9, q9, q10, #7
        vadd.i16   q9, q10
        vext.16    q10, q10, q11, #7
        vadd.i16   q10, q11
        vext.16    q11, q11, q12, #7
        vadd.i16   q11, q12
        vadd.i16   d17, d26, d28        @ d17[0] = l[0] + 2ul + u[0]
        vmov.u16   r4, d7[3]            @ Save final pel
        vext.16    q3, q2, q2, #1
        vadd.i16   q3, q2
        vext.16    q2, q1, q2, #1
        vadd.i16   q2, q1
        vext.16    q1, q0, q1, #1
        vadd.i16   q1, q0
        vext.16    q0, q13, q0, #1
        vadd.i16   q0, q13
        vext.16    q13, q11, q11, #1
        vadd.i16   q13, q11
        vext.16    q11, q10, q11, #1
        vadd.i16   q11, q10
        vext.16    q10, q9, q10, #1
        vadd.i16   q10, q9
        vext.16    q9, q14, q9, #1
        vadd.i16   q9, q14
        vrshr.u16  d17, #2
        vmov.u16   r5, d25[3]           @ Save final pel
        vrshr.u16  q3, #2
        vrshr.u16  q12, q13, #2
        vrshr.u16  q0, #2
        vrshr.u16  q1, #2
        vrshr.u16  q2, #2
        vrshr.u16  q9, #2
        vrshr.u16  q10, #2
        vrshr.u16  q11, #2
        vdup.16    d17, d17[0]
        vmov.16    d7[3], r4            @ Restore final pel
        vmov.16    d25[3], r5           @ Restore final pel
4:
        vst1.16    {d17[3]}, [r0]!
        vst1.16    {q9-q10}, [r1]!
        vst1.16    {q0-q1}, [r0]!
        vst1.16    {q11-q12}, [r1]
        vst1.16    {q2-q3}, [r0]
        pop        {r4-r10, pc}

10:     cmp        r9, #8
        bhi        12f
        beq        11f
        vdup.16    d21, d20[3]
11:     vdup.16    d22, d21[3]
12:     vdup.16    d23, d22[3]
        b          1b

20:     vmov       q0, q2
        vmov       q1, q2
        b          2b

30:     vmov       q3, q2
        b          3b

40:     cmp        lr, #8
        bhi        42f
        beq        41f
        vdup.16    d5, d6[1]
        vdup.16    d7, d6[1]
        vmov.f32   s9, s10
        vmov.f32   s13, s10
        b          3b
41:     vld1.16    {d4[2]}, [r5], r12
        vld1.16    {d6[2]}, [r8], r12
        vld1.16    {d4[3]}, [r5]
        vld1.16    {d6[3]}, [r8]
        vdup.16    d5, d6[3]
        vdup.16    d7, d6[3]
        b          3b
42:     vld1.16    {d4[2]}, [r5], r12
        vld1.16    {d6[2]}, [r8], r12
        vld1.16    {d4[3]}, [r5], r12
        vld1.16    {d6[3]}, [r8], r12
        vld1.16    {d5[0]}, [r5], r12
        ldrh       lr, [r8, r12]
        vld1.16    {d7[0]}, [r8], r12
        vld1.16    {d5[1]}, [r5]
        vld1.16    {d7[1]}, [r8]
        orr        lr, lr, lr, lsl #16
        vmov       s11, lr
        vmov       s15, lr
        b          3b
endfunc

@ int ff_hevc_rpi_intra_filter_4_neon_32(
@    pixel * const left,                   [r0]
@    pixel * const top,                    [r1]
@    const unsigned int req,               [r2]
@    const unsigned int avail,             [r3]
@    const pixel * const src_l,            [sp, #0]
@    const pixel * const src_u,            [sp, #4]
@    const pixel * const src_ur,           [sp, #8]
@    const unsigned int stride,            [sp, #12] (pels)
@    const unsigned int top_right_size,    [sp, #16]
@    const unsigned int down_left_size)    [sp, #20]

.set    sp_base, 8*4
.set    pw_s,    2
.set    pw,      (1 << pw_s)
.set    log2_s,  2

function ff_hevc_rpi_intra_filter_4_neon_32, export=1
        push        {r4-r10, lr}
        load_pointers pw_s, log2_s, sp_base, 32, "d0[],d1[]", d16[0], "d4[],d5[]", "d6[],d7[]", 0, \
            "vmov        q1, q0"

        sub         r3, r0, #pw
        it          mi
        vldmmi      r4, {d6, d7}
        it          cs
        vldmcs      r7, {d4, d5}
        lsls        lr, r2, #AVAIL_S_L_N_DL_C
        bpl         1f
        vld1.32     {d0[0]}, [r6], r12
        vld1.32     {d0[1]}, [r10], r12
        vld1.32     {d1[0]}, [r6]
        vld1.32     {d1[1]}, [r10]
1:
        bcc         1f
        vld1.32     {d2[1]}, [r8], r12
        vld1.32     {d3[0]}, [r5]
        vld1.32     {d3[1]}, [r8]
1:
        vst1.32     {d16[0]}, [r3]
        vst1.32     {q2, q3}, [r1]
        vst1.32     {q0, q1}, [r0]
        pop         {r4-r10, pc}
endfunc


@ int ff_hevc_rpi_intra_filter_8_neon_32(
@    pixel * const left,                   [r0]
@    pixel * const top,                    [r1]
@    const unsigned int req,               [r2]
@    const unsigned int avail,             [r3]
@    const pixel * const src_l,            [sp, #0]
@    const pixel * const src_u,            [sp, #4]
@    const pixel * const src_ur,           [sp, #8]
@    const unsigned int stride,            [sp, #12] (pels)
@    const unsigned int top_right_size,    [sp, #16]
@    const unsigned int down_left_size)    [sp, #20]

.set    sp_base, 8*4
.set    ur_size, sp_base + 16
.set    dl_size, sp_base + 20
.set    pw_s,    2
.set    pw,      (1 << pw_s)
.set    log2_s,  3
.set    p_size,  (1 << log2_s)          @ size in pels

function ff_hevc_rpi_intra_filter_8_neon_32, export=1
        push       {r4-r10, lr}
        load_pointers pw_s, log2_s, sp_base, 32, "d0[],d1[]", d31[1], "d16[],d17[]", "d20[],d21[]", 0, \
            "vmov       r3, s0"

        vmov       q9, q8
        ldr        r9, [r4, #3*4]
        vmov       q11, q10
        ldr        lr, [sp, #ur_size]
        it         cs
        vldmcs     r7, {q8, q9}
        ittt       mi
        vldmmi     r4, {q10, q11}
        cmpmi      lr, #p_size
        vdupmi.32  q11, r9
        lsls       lr, r2, #AVAIL_S_L_N_DL_C
        vdup.32    q1, r3
        vdup.32    q2, r3
        vdup.32    q3, r3
        it         cs
        ldrcs      r9, [r8, r12]
        bpl        1f
        vld1.32    {d0[0]}, [r6], r12
        vld1.32    {d0[1]}, [r10], r12
        vld1.32    {d1[0]}, [r6], r12
        vld1.32    {d1[1]}, [r10], r12
        vld1.32    {d2[0]}, [r6], r12
        vld1.32    {d2[1]}, [r10], r12
        vld1.32    {d3[0]}, [r6]
        vld1.32    {d3[1]}, [r10]
1:
        ldr        lr, [sp, #dl_size]
        bcc        2f
        vld1.32    {d4[1]}, [r8], r12
        vld1.32    {d5[0]}, [r5], r12
        cmp        lr, #p_size
        vld1.32    {d5[1]}, [r8], r12
        bcc        1f
        vld1.32    {d6[0]}, [r5], r12
        vld1.32    {d6[1]}, [r8], r12
        vld1.32    {d7[0]}, [r5]
        vld1.32    {d7[1]}, [r8]
1:
        it         cc
        vdupcc.32  q3, r9
2:
        vst1.32    {q8-q9}, [r1]!
        sub        r3, r0, #pw
        vst1.32    {q0-q1}, [r0]!
        vst1.32    {q10-q11}, [r1]
        vst1.32    {q2-q3}, [r0]
        vst1.32    {d31[1]}, [r3]
        pop        {r4-r10, pc}
endfunc


@ int ff_hevc_rpi_intra_filter_16_neon_32(
@    pixel * const left,                   [r0]
@    pixel * const top,                    [r1]
@    const unsigned int req,               [r2]
@    const unsigned int avail,             [r3]
@    const pixel * const src_l,            [sp, #0]
@    const pixel * const src_u,            [sp, #4]
@    const pixel * const src_ur,           [sp, #8]
@    const unsigned int stride,            [sp, #12] (pels)
@    const unsigned int top_right_size,    [sp, #16]
@    const unsigned int down_left_size)    [sp, #20]

.set    sp_base, 8*4
.set    ur_size, sp_base + 16
.set    dl_size, sp_base + 20
.set    pw_s,    2
.set    pw,      (1 << pw_s)
.set    log2_s,  4
.set    p_size,  (1 << log2_s)          @ size in pels

function ff_hevc_rpi_intra_filter_16_neon_32, export=1
        push        {r4-r10, lr}
        load_pointers pw_s, log2_s, sp_base, 32, d30[0], d30[1], d31[0], d31[1], 1, \
            "ldr         r9, [sp, #ur_size]", \
            "sub         r0, #pw"

        @ Once we get this big we have run out of neon regs to store
        @ everything at once so do in pieces

        @ Up and/or up-right (have)
        add         lr, r1, #(pw << log2_s)
        bcc         1f
        vldm        r7, {q0-q3}
        vstm        r1, {q0-q3}
1:
        bpl         3f
        vldm        r4, {q8-q11}
        cmp         r9, #16
        blo         10f
2:      vstm        lr, {q8-q11}
3:
        @ Up-left
        vst1.32     {d30[1]}, [r0]!

        @ Left and/or down-left (have)
        lsls        lr, r3, #AVAIL_S_L_N_DL_C
        ldr         r9, [sp, #dl_size]
        bpl         4f
        vld1.32     {d0[0]}, [r6], r12
        vld1.32     {d0[1]}, [r10], r12
        vld1.32     {d1[0]}, [r6], r12
        vld1.32     {d1[1]}, [r10], r12
        vld1.32     {d2[0]}, [r6], r12
        vld1.32     {d2[1]}, [r10], r12
        vld1.32     {d3[0]}, [r6], r12
        vld1.32     {d3[1]}, [r10], r12
        vld1.32     {d4[0]}, [r6], r12
        vld1.32     {d4[1]}, [r10], r12
        vld1.32     {d5[0]}, [r6], r12
        vld1.32     {d5[1]}, [r10], r12
        vld1.32     {d6[0]}, [r6], r12
        vld1.32     {d6[1]}, [r10], r12
        vld1.32     {d7[0]}, [r6]
        vld1.32     {d7[1]}, [r10]
        vstm        r0, {q0-q3}
4:      add         lr, r0, #(pw << log2_s)
        bcc         6f
        vdup.32     d16, d30[0]
        vld1.32     {d16[1]}, [r8], r12
        vld1.32     {d17[0]}, [r5], r12
        cmp         r9, #16
        vld1.32     {d17[1]}, [r8], r12
        blo         20f
        vld1.32     {d18[0]}, [r5], r12
        vld1.32     {d18[1]}, [r8], r12
        vld1.32     {d19[0]}, [r5], r12
        vld1.32     {d19[1]}, [r8], r12
        vld1.32     {d20[0]}, [r5], r12
        vld1.32     {d20[1]}, [r8], r12
        vld1.32     {d21[0]}, [r5], r12
        vld1.32     {d21[1]}, [r8], r12
        vld1.32     {d22[0]}, [r5], r12
        vld1.32     {d22[1]}, [r8], r12
        vld1.32     {d23[0]}, [r5]
        vld1.32     {d23[1]}, [r8]
5:      vstm        lr, {q8-q11}
6:
        eors        r3, r2          @ (req & avail) ^ req = (req & ~avail)
        bne         7f
        pop         {r4-r10, pc}
7:
        @ Up and/or up-right (don't have)
        vdup.32     q0, d31[0]
        lsls        lr, r3, #AVAIL_S_UR_N_U_C
        vdup.32     q1, d31[0]
        add         lr, r1,  #(pw << log2_s)
        vdup.32     q8, d31[1]
        vdup.32     q9, d31[1]
        it          cs
        vstmcs      r1!, {q0-q1}
        it          mi
        vstmmi      lr!, {q8-q9}
        it          cs
        vstmcs      r1, {q0-q1}
        it          mi
        vstmmi      lr, {q8-q9}

        @ Left and/or down-left (don't have)
        vdup.32     q0, d30[0]
        lsls        lr, r3, #AVAIL_S_L_N_DL_C
        vdup.32     q1, d30[0]
        add         lr,  r0,  #(pw << log2_s)
        it          mi
        vstmmi      r0!, {q0-q1}
        it          cs
        vstmcs      lr!, {q0-q1}
        it          mi
        vstmmi      r0, {q0-q1}
        it          cs
        vstmcs      lr, {q0-q1}
        pop         {r4-r10, pc}

10:     cmp         r9, #8
        bhi         12f
        beq         11f
        vdup.32     q9, d17[1]
11:     vdup.32     q10, d19[1]
12:     vdup.32     q11, d21[1]
        b           2b

20:     cmp         r9, #8
        blo         21f
        vld1.32     {d18[0]}, [r5], r12
        vld1.32     {d18[1]}, [r8], r12
        vld1.32     {d19[0]}, [r5], r12
        vld1.32     {d19[1]}, [r8], r12
        beq         22f
        vld1.32     {d20[0]}, [r5], r12
        vld1.32     {d20[1]}, [r8], r12
        vld1.32     {d21[0]}, [r5]
        vld1.32     {d21[1]}, [r8]
        b           23f
21:     vdup.32     q9, d17[1]
22:     vdup.32     q10, d19[1]
23:     vdup.32     q11, d21[1]
        b           5b
endfunc




