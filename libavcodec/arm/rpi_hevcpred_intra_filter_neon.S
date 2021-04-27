/*
Copyright (c) 2018 Raspberry Pi (Trading) Ltd.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Authors: John Cox, Ben Avison
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

.equ    AVAIL_S_U_DL_CPSR, 31 - 4  @ Shift for u..dl to go into flags via cpsr

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
@  r4   DL + stride
@  r10  L
@  r6   U
@  r5   UR
@
@ Others:
@  r2   req
@  r7   req & avail
@  r3   L + stride
@  r8   DL + stride * 2
@  r9   stride * 2
@  cs   Load U
@  mi   Load UR
@
@ Clobbered:
@  r12

.macro  load_pointers pw_s, log2_s, sp_offset, d_type, d_l, d_ul, d_u, d_ur

.equ    src_l\@,   \sp_offset + 0
.equ    src_u\@,   \sp_offset + 4
.equ    src_ur\@,  \sp_offset + 8
.equ    stride\@,  \sp_offset + 12
.equ    pw\@,      (1 << \pw_s)                 @ pel width in bytes
.equ    b_size\@,  (1 << (\pw_s + \log2_s))     @ size in bytes

@ r9    stride
@                       r7 = ab_ul, r6 = a_u, r5 = a_ur
@ r4 = b_dl, r10 = b_l,             r8 = b_u

        ldr        r5,  [sp, #src_ur\@]
        lsl        r12, r3,  #AVAIL_S_U_DL_CPSR
        ldr        r10, [sp, #src_l\@]
        ldr        r9,  [sp, #stride\@]
        ldr        r6,  [sp, #src_u\@]

        @ This is quite a slow instruction but it replaces
        @ a decent number of tests that yield a max of 2 flags/op
        @ It is annoying we can't branch on Q!
        @ If L navail (ne) then DL must be navail (pl)
        msr        APSR_nzcvq, r12      @ n=dl, z=l, c=ul, v=u, q=ur

        mov        r4,  r5
        sub        r7,  r10, r9
        it vs
        movvs      r4,  r6
        add        r8,  r6,  #b_size\@ - pw\@
        it cs
        movcs      r4,  r7
        ite ne
        movne      r10, r4
        addeq      r4,  r7,  r9,  lsl #\log2_s
        it cc
        movcc      r7,  r10
        it mi
        addmi      r4,  r10, r9,  lsl #\log2_s
        vld1.\d_type {\d_ul}, [r7]
        itt vc
        movvc      r8,  r7
        movvc      r6,  r7
        vld1.\d_type {\d_l }, [r4], r9
        tst        r3,  #AVAIL_UR
        vld1.\d_type {\d_u }, [r6]
        it eq
        moveq      r5,  r8
        and        r7,  r2,  r3
        add        r8,  r4,  r9
        vld1.\d_type {\d_ur}, [r5]
        lsls       r12, r7,  #AVAIL_S_UR_N_U_C
        add        r3,  r10, r9
        lsl        r9,  #1
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
        push       {r4-r10, lr}
        load_pointers pw_s, log2_s, sp_base, 8, d0[], d31[7], d1[], d2[]

        it cs
        vldrcs     s2,  [r6]
        ite pl
        vmovpl     s3,  s4
        vldrmi     s3,  [r5]

        lsls       r7,  #AVAIL_S_L_N_DL_C
        add        r12, r0,  #-pw
        bpl        1f

        vld1.8    {d0[0]}, [r10], r9
        vld1.8    {d0[1]}, [r3],  r9
        vld1.8    {d0[2]}, [r10]
        vld1.8    {d0[3]}, [r3]
1:
        bcc        1f
        vld1.8    {d0[5]}, [r4],  r9
        vld1.8    {d0[6]}, [r8]
        vld1.8    {d0[7]}, [r4]
1:
        vstr       d1,  [r1]            @ Up
        vst1.8    {d31[7]}, [r12]
        vstr       d0,  [r0]            @ Left
        pop       {r4-r10, pc}
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
        push       {r4-r10, lr}
        load_pointers pw_s, log2_s, sp_base, 16, "d0[],d1[]", d31[3], d2[], d3[]

        it cs
        vldrcs     d2,  [r6]
        it mi
        vldrmi     d3,  [r5]
        lsls       r7,  #AVAIL_S_L_N_DL_C
        add        r12, r0, #-pw
        bpl        1f
        vld1.16   {d0[0]}, [r10], r9
        vld1.16   {d0[1]}, [r3],  r9
        vld1.16   {d0[2]}, [r10]
        vld1.16   {d0[3]}, [r3]
1:
        bcc        1f
        vld1.16   {d1[1]}, [r4],  r9
        vld1.16   {d1[2]}, [r8]
        vld1.16   {d1[3]}, [r4]
1:
        vst1.16   {q1}, [r1]           @ Up
        vst1.16   {d31[3]}, [r12]
        vst1.16   {q0}, [r0]           @ Left
        pop       {r4-r10, pc}
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
        push      {r4-r10, lr}
        load_pointers pw_s, log2_s, sp_base, 8, "d0[],d1[]", d31[7], d4[], d5[]

        it cs
        vldrcs     d4,  [r6]
        it mi
        vldrmi     d5,  [r5]

        lsls       r7,  #AVAIL_S_L_N_DL_C
        bpl        1f
        vld1.8    {d0[0]}, [r10], r9
        vld1.8    {d0[1]}, [r3],  r9
        vld1.8    {d0[2]}, [r10], r9
        vld1.8    {d0[3]}, [r3],  r9
        vld1.8    {d0[4]}, [r10], r9
        vld1.8    {d0[5]}, [r3],  r9
        vld1.8    {d0[6]}, [r10]
        vld1.8    {d0[7]}, [r3]
1:
        bcc        1f
        vld1.8    {d1[1]}, [r4],  r9
        vld1.8    {d1[2]}, [r8],  r9
        vld1.8    {d1[3]}, [r4],  r9
        vld1.8    {d1[4]}, [r8],  r9
        vld1.8    {d1[5]}, [r4],  r9
        vld1.8    {d1[6]}, [r8]
        vld1.8    {d1[7]}, [r4]
1:
        tst        r2,  #FILTER_LIGHT
        add        r12, r0,  #-pw
        beq        10f

        @ Luma light filter
        vext.8     q8,  q15, q2,  #15
        vext.8     q12, q15, q0,  #15
        vaddl.u8   q9,  d17, d5
        vaddl.u8   q8,  d16, d4
        vaddl.u8   q13, d25, d1
        vaddl.u8   q12, d24, d0
        vmov.u8    r3,  d5[7]           @ Save final pel
        vmov.u8    r2,  d1[7]           @ Save final pel

        vext.16    q2,  q8,  q9,  #1
        vext.16    q3,  q9,  q9,  #1
        vext.16    q0,  q12, q13, #1
        vext.16    q1,  q13, q13, #1
        vadd.u16   d30, d16, d24        @ d30[0] = l[0] + 2ul + u[0]
        vadd.u16   q2,  q8
        vadd.u16   q3,  q9
        vadd.u16   q0,  q12
        vadd.u16   q1,  q13

        vrshrn.u16 d4,  q2,  #2
        vrshrn.u16 d5,  q3,  #2
        vrshrn.u16 d0,  q0,  #2
        vrshrn.u16 d1,  q1,  #2
        vrshr.u16  d30, #2
        vmov.u8    d5[7], r3            @ Restore final pel
        vmov.u8    d1[7], r2            @ Restore final pel
        vdup.u8    d31, d30[0]          @ d31[3] = d30[0]

10:
        vst1.8    {q2 }, [r1]           @ Up
        vst1.8    {d31[7]}, [r12]       @ Up-left
        vst1.8    {q0 }, [r0]           @ Left
        pop       {r4-r10, pc}
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
        push      {r4-r10, lr}
        load_pointers pw_s, log2_s, sp_base, 16, "d0[],d1[]", d31[3], "d4[],d5[]", "d6[],d7[]"

        it cs
        vldmcs     r6,  {d4, d5}
        ldr        r12, [sp, #ur_size]
        bpl        1f
        cmp        r12, #4
        vldm       r5,  {d6, d7}
        bgt        1f
        vdup.16    d7,  d6[3]
1:
        lsls       r12, r7,  #AVAIL_S_L_N_DL_C
        vdup.16    q1,  d0[0]
        bpl        1f
        vld1.16   {d0[0]}, [r10], r9
        vld1.16   {d0[1]}, [r3],  r9
        vld1.16   {d0[2]}, [r10], r9
        vld1.16   {d0[3]}, [r3],  r9
        vld1.16   {d1[0]}, [r10], r9
        vld1.16   {d1[1]}, [r3],  r9
        vld1.16   {d1[2]}, [r10]
        vld1.16   {d1[3]}, [r3]
1:
        bcc        1f
        ldr        r12, [sp, #dl_size]
        vld1.16   {d2[1]}, [r4],  r9
        cmp        r12, #p_size
        vld1.16   {d2[2]}, [r8],  r9
        vld1.16   {d2[3]}, [r4],  r9
        blt        2f
        vld1.16   {d3[0]}, [r8],  r9
        vld1.16   {d3[1]}, [r4],  r9
        vld1.16   {d3[2]}, [r8]
        vld1.16   {d3[3]}, [r4]
        b          1f
2:
        vdup.16    d3,  d2[3]
1:
        tst        r2,  #FILTER_LIGHT
        add        r12, r0,  #-pw
        beq        10f

        @ Luma light filter
        vext.16    q9,  q2,  q3,  #7
        vext.16    q8,  q15, q2,  #7
        vext.16    q13, q0,  q1,  #7
        vext.16    q12, q15, q0,  #7
        vadd.u16   q9,  q3
        vadd.u16   q8,  q2
        vadd.u16   q13, q1
        vadd.u16   q12, q0
        vmov.u16   r3,  d7[3]           @ Save final pel
        vmov.u16   r2,  d3[3]           @ Save final pel

        vext.16    q2,  q8,  q9,  #1
        vext.16    q3,  q9,  q9,  #1
        vext.16    q0,  q12, q13, #1
        vext.16    q1,  q13, q13, #1
        vadd.u16   d30, d16, d24        @ d30[0] = l[0] + 2ul + u[0]
        vadd.u16   q2,  q8
        vadd.u16   q3,  q9
        vadd.u16   q0,  q12
        vadd.u16   q1,  q13

        vrshr.u16  q2,  #2
        vrshr.u16  q3,  #2
        vrshr.u16  q0,  #2
        vrshr.u16  q1,  #2
        vrshr.u16  d30, #2
        vmov.u16   d7[3], r3            @ Restore final pel
        vmov.u16   d3[3], r2            @ Restore final pel
        vdup.u16   d31, d30[0]          @ d31[3] = d30[0]

10:
        vst1.16   {q2,  q3}, [r1]       @ Up
        vst1.16   {d31[3]}, [r12]       @ Up-left
        vst1.16   {q0,  q1}, [r0]       @ Left
        pop       {r4-r10, pc}
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
        push      {r4-r10, lr}
        load_pointers pw_s, log2_s, sp_base, 16, "d0[],d1[]", d31[3], "d16[],d17[]", "d20[],d21[]"

        vdup.16    q9,  d16[0]
        vdup.16    q11, d20[0]

        it cs
        vldmcs     r6,  {d16-d19}
        ldr        r12, [sp, #ur_size]
        bpl        1f
        cmp        r12, #12
        @ Given chroma frame layout, if UR exists then it is always legit to
        @ load all of it even if most of it is outside the frame.
        vldm       r5,  {d20-d23}
        bgt        1f
        bge        4f
        cmp        r12,  #8
        bge        3f
        vdup.16    d21, d20[3]
3:      vdup.16    d22, d21[3]
4:      vdup.16    d23, d22[3]

1:
        lsls       r7,  #AVAIL_S_L_N_DL_C
        ldr        r12, [sp, #dl_size]
        vdup.16    q1,  d0[0]
        vdup.16    q2,  d0[0]
        vdup.16    q3,  d0[0]
        bpl        1f
        vld1.16   {d0[0]}, [r10], r9
        vld1.16   {d0[1]}, [r3],  r9
        vld1.16   {d0[2]}, [r10], r9
        vld1.16   {d0[3]}, [r3],  r9
        vld1.16   {d1[0]}, [r10], r9
        vld1.16   {d1[1]}, [r3],  r9
        vld1.16   {d1[2]}, [r10], r9
        vld1.16   {d1[3]}, [r3],  r9
        vld1.16   {d2[0]}, [r10], r9
        vld1.16   {d2[1]}, [r3],  r9
        vld1.16   {d2[2]}, [r10], r9
        vld1.16   {d2[3]}, [r3],  r9
        vld1.16   {d3[0]}, [r10], r9
        vld1.16   {d3[1]}, [r3],  r9
        vld1.16   {d3[2]}, [r10]
        vld1.16   {d3[3]}, [r3]
1:
        bcc        1f
        vld1.16   {d4[1]}, [r4],  r9
        cmp        r12, #4
        vld1.16   {d4[2]}, [r8],  r9
        vld1.16   {d4[3]}, [r4],  r9
        ble        2f
        vld1.16   {d5[0]}, [r8],  r9
        vld1.16   {d5[1]}, [r4],  r9
        cmp        r12, #12
        vld1.16   {d5[2]}, [r8],  r9
        vld1.16   {d5[3]}, [r4],  r9
        blt        3f
        vld1.16   {d6[0]}, [r8],  r9
        vld1.16   {d6[1]}, [r4],  r9
        vld1.16   {d6[2]}, [r8],  r9
        vld1.16   {d6[3]}, [r4],  r9
        ble        4f
        vld1.16   {d7[0]}, [r8],  r9
        vld1.16   {d7[1]}, [r4],  r9
        vld1.16   {d7[2]}, [r8]
        vld1.16   {d7[3]}, [r4]
        b          1f
2:      vdup.16    d5,  d4[3]
3:      vdup.16    d6,  d5[3]
4:      vdup.16    d7,  d6[3]
1:
        tst        r2,  #FILTER_LIGHT
        add        r12, r0,  #-pw
        beq        10f

        vpush     {q5}
        @ Luma light filter
        @ Left
        vext.16    q5,  q2,  q3,  #7
        vext.16    q14, q1,  q2,  #7
        vext.16    q13, q0,  q1,  #7
        vext.16    q12, q15, q0,  #7

        vadd.u16   q5,  q3
        vadd.u16   q14, q2
        vadd.u16   q13, q1
        vadd.u16   q12, q0
        vmov.u16   r2,  d7[3]           @ Save final pel

        vext.16    q0,  q12, q13, #1
        vext.16    q1,  q13, q14, #1
        vext.16    q2,  q14, q5,  #1
        vext.16    q3,  q5,  q5,  #1

        vmov       d30, d24             @ d30[0] = l[0] + ul
        vadd.u16   q0,  q12
        vadd.u16   q1,  q13
        vadd.u16   q2,  q14
        vadd.u16   q3,  q5

        vrshr.u16  q0,  #2
        vrshr.u16  q1,  #2
        vrshr.u16  q2,  #2
        vrshr.u16  q3,  #2

        @ Up
        vext.16    q5,  q10, q11, #7
        vext.16    q14, q9,  q10, #7
        vext.16    q13, q8,  q9,  #7
        vext.16    q12, q15, q8,  #7

        vadd.u16   q5,  q11
        vadd.u16   q14, q10
        vadd.u16   q13, q9
        vadd.u16   q12, q8
        vmov.u16   r3,  d23[3]          @ Save final pel

        vext.16    q8,  q12, q13, #1
        vext.16    q9,  q13, q14, #1
        vext.16    q10, q14, q5,  #1
        vext.16    q11, q5,  q5,  #1

        vadd.u16   d30, d24             @ d30[0] = l[0] + 2ul + u[0]
        vadd.u16   q8,  q12
        vadd.u16   q9,  q13
        vadd.u16   q10, q14
        vadd.u16   q11, q5

        vrshr.u16  q8,  #2
        vrshr.u16  q9,  #2
        vrshr.u16  q10, #2
        vrshr.u16  q11, #2

        @ Misc
        vrshr.u16  d30, #2
        vmov.u16   d7[3], r2            @ Restore final pel
        vmov.u16   d23[3], r3           @ Restore final pel
        vdup.u16   d31, d30[0]          @ d31[3] = d30[0]
        vpop      {q5}

10:
        vstm       r1, {d16-d23}        @ Up
        vst1.16   {d31[3]}, [r12]       @ Up-left
        vstm       r0, { d0-d7 }        @ Left
        pop       {r4-r10, pc}
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
        push       {r4-r10, lr}
        load_pointers pw_s, log2_s, sp_base, 32, "d0[],d1[]", d31[1], "d4[],d5[]", "d6[],d7[]"

        it cs
        vldmcs     r6,  {d4, d5}
        it mi
        vldmmi     r5,  {d6, d7}
        lsls       r7,  #AVAIL_S_L_N_DL_C
        vdup.32    q1,  d0[0]
        add        r12, r0,  #-pw
        bpl        1f
        vld1.32   {d0[0]}, [r10], r9
        vld1.32   {d0[1]}, [r3],  r9
        vld1.32   {d1[0]}, [r10]
        vld1.32   {d1[1]}, [r3]
1:
        bcc        1f
        vld1.32   {d2[1]}, [r4],  r9
        vld1.32   {d3[0]}, [r8]
        vld1.32   {d3[1]}, [r4]
1:
        vst1.32    {q2,  q3 }, [r1]     @ Up
        vst1.32    {d31[1]}, [r12]
        vst1.32    {q0,  q1 }, [r0]     @ Left
        pop        {r4-r10, pc}
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
        load_pointers pw_s, log2_s, sp_base, 32, "d0[],d1[]", d31[1], "d16[],d17[]", "d20[],d21[]"

        vdup.32    q9,  d16[0]
        vdup.32    q11, d20[0]

        it cs
        vldmcs     r6,  {q8,  q9 }
        ldr        r12, [sp, #ur_size]
        bpl        1f
        cmp        r12, #p_size
        vldm       r5,  {q10, q11}
        bge        1f
        vdup.32    q11, d21[1]
1:
        lsls       r7,  #AVAIL_S_L_N_DL_C
        vdup.32    q1,  d0[0]
        vdup.32    q2,  d0[0]
        vdup.32    q3,  d0[0]
        bpl        1f
        vld1.32   {d0[0]}, [r10], r9
        vld1.32   {d0[1]}, [r3],  r9
        vld1.32   {d1[0]}, [r10], r9
        vld1.32   {d1[1]}, [r3],  r9
        vld1.32   {d2[0]}, [r10], r9
        vld1.32   {d2[1]}, [r3],  r9
        vld1.32   {d3[0]}, [r10]
        vld1.32   {d3[1]}, [r3]
1:
        bcc        1f
        ldr        r12, [sp, #dl_size]
        vld1.32   {d4[1]}, [r4],  r9
        cmp        r12, #p_size
        vld1.32   {d5[0]}, [r8],  r9
        vld1.32   {d5[1]}, [r4],  r9
        blt        2f
        vld1.32   {d6[0]}, [r8],  r9
        vld1.32   {d6[1]}, [r4],  r9
        vld1.32   {d7[0]}, [r8]
        vld1.32   {d7[1]}, [r4]
        b          1f
2:
        vdup.32    q3,  d5[1]
1:
        add        r12, r0,  #-pw
        vstm       r1,  { q8-q11}       @ Up
        vst1.32   {d31[1]}, [r12]
        vstm       r0,  { q0-q3 }       @ Left
        pop       {r4-r10, pc}
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
        push       {r4-r10, lr}
        load_pointers pw_s, log2_s, sp_base, 32, d30[0], d30[1], d31[0], d31[1]

        @ Once we get this big we have run out of neon regs to store
        @ everything at once so do in pieces

        @ Up (have)
        it cs
        vldmcs     r6,  { q0-q3 }
        ldr        r12, [sp, #ur_size]
        it mi
        vldmmi     r5,  { q8-q11}
        it cs
        vstmcs     r1,  { q0-q3 }
        bpl        1f
        cmp        r12, #12
        add        lr,  r1,  #(pw << log2_s)
        bgt        2f
        cmp        r12, #8
        bge        3f
        vdup.16    q9,  d17[1]
4:      vdup.16    d10, d19[1]
3:      vdup.16    q11, d21[1]
2:      vstm       lr, { q8-q11}
1:

        @ Left (have)
        add        lr,  r0,  #-pw
        lsls       r12, r7,  #AVAIL_S_L_N_DL_C
        vst1.32   {d30[1]}, [lr]        @ UL
        bpl        1f
        vld1.32   { d0[0]}, [r10], r9
        vld1.32   { d0[1]}, [r3],  r9
        vld1.32   { d1[0]}, [r10], r9
        vld1.32   { d1[1]}, [r3],  r9
        vld1.32   { d2[0]}, [r10], r9
        vld1.32   { d2[1]}, [r3],  r9
        vld1.32   { d3[0]}, [r10], r9
        vld1.32   { d3[1]}, [r3],  r9
        vld1.32   { d4[0]}, [r10], r9
        vld1.32   { d4[1]}, [r3],  r9
        vld1.32   { d5[0]}, [r10], r9
        vld1.32   { d5[1]}, [r3],  r9
        vld1.32   { d6[0]}, [r10], r9
        vld1.32   { d6[1]}, [r3],  r9
        vld1.32   { d7[0]}, [r10]
        vld1.32   { d7[1]}, [r3]
        vstm       r0,  { q0-q3 }
1:
        bcc        1f
        ldr        r12, [sp, #dl_size]
        vdup.32    d16, d30[0]          @ d16[0] = d30[0]
        add        lr,  r0,  #(pw << log2_s)
        vld1.32   {d16[1]}, [r4],  r9
        cmp        r12, #4
        vld1.32   {d17[0]}, [r8],  r9
        vld1.32   {d17[1]}, [r4],  r9
        ble        2f
        vld1.32   {d18[0]}, [r8],  r9
        vld1.32   {d18[1]}, [r4],  r9
        cmp        r12, #12
        vld1.32   {d19[0]}, [r8],  r9
        vld1.32   {d19[1]}, [r4],  r9
        blt        3f
        vld1.32   {d20[0]}, [r8],  r9
        vld1.32   {d20[1]}, [r4],  r9
        vld1.32   {d21[0]}, [r8],  r9
        vld1.32   {d21[1]}, [r4],  r9
        ble        4f
        vld1.32   {d22[0]}, [r8],  r9
        vld1.32   {d22[1]}, [r4],  r9
        vld1.32   {d23[0]}, [r8]
        vld1.32   {d23[1]}, [r4]
        b          5f
2:      vdup.32    q9,  d17[1]
3:      vdup.32    q10, d19[1]
4:      vdup.32    q11, d21[1]
5:      vstm       lr,  { q8-q11}
1:
        eors       r7,  r2
        beq        99f

        lsls       r12, r7,  #AVAIL_S_UR_N_U_C
        vdup.32    q0,  d31[0]
        vdup.32    q1,  d31[0]
        vdup.32    q2,  d31[0]
        vdup.32    q3,  d31[0]
        add        lr,  r1,  #(pw << log2_s)
        vdup.32    q8,  d31[1]
        vdup.32    q9,  d31[1]
        vdup.32    q10, d31[1]
        vdup.32    q11, d31[1]
        it cs
        vstmcs     r1,  { q0-q3 }
        it mi
        vstmmi     lr,  { q8-q11}

        lsls       r7,  #AVAIL_S_L_N_DL_C
        vdup.32    q0,  d30[0]
        vdup.32    q1,  d30[0]
        vdup.32    q2,  d30[0]
        vdup.32    q3,  d30[0]
        add        lr,  r0,  #(pw << log2_s)
        it mi
        vstmmi     r0, { q0-q3 }
        it cs
        vstmcs     lr, { q0-q3 }

99:
        pop       {r4-r10, pc}
endfunc




