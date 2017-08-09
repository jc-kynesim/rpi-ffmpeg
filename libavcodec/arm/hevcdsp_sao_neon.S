/*
 * Copyright (c) 2014 - 2015 Seppo Tomperi <seppo.tomperi@vtt.fi>
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

.set EDGE_SRC_STRIDE, 160

.macro init_sao_band
        pld      [r1]
        vld1.8   {q0, q1}, [r2]  // offset table
        ldr       r2, [sp, #0]   // stride_dst
        ldr      r12, [sp, #4]   // height
        vmov.u8  q3, #128
.endm

// 128 in q3
// input q8 - q11
.macro sao_band_64
        vtbl.8   d24, {d0, d1, d2, d3}, d24
        vadd.s8  q8, q3
        vtbl.8   d25, {d0, d1, d2, d3}, d25
        vadd.s8  q9, q3
        vtbl.8   d26, {d0, d1, d2, d3}, d26
        vadd.s8  q10, q3
        vtbl.8   d27, {d0, d1, d2, d3}, d27
        vadd.s8  q11, q3
        vtbl.8   d28, {d0, d1, d2, d3}, d28
        vqadd.s8 q8, q12
        vtbl.8   d29, {d0, d1, d2, d3}, d29
        vqadd.s8 q9, q13
        vtbl.8   d30, {d0, d1, d2, d3}, d30
        vqadd.s8 q10, q14
        vtbl.8   d31, {d0, d1, d2, d3}, d31
        vsub.s8  q8, q3
        vqadd.s8 q11, q15
        vsub.s8  q9, q3
        vsub.s8  q10, q3
        vsub.s8  q11, q3
.endm

.macro sao_band_64_8 XLAT0, XLAT1, Q_K128
        vshr.u8 q12, q8, #3
        vadd.s8  q8, \Q_K128
        vshr.u8 q13, q9, #3
        vadd.s8  q9, \Q_K128

        vtbl.8   d24, \XLAT0, d24
        vtbl.8   d25, \XLAT0, d25
        vtbl.8   d26, \XLAT1, d26
        vtbl.8   d27, \XLAT1, d27

        vqadd.s8 q8, q12
        vshr.u8 q12, q10, #3
        vadd.s8  q10, \Q_K128
        vqadd.s8 q9, q13
        vshr.u8 q13, q11, #3
        vadd.s8  q11, \Q_K128

        vsub.s8  q8, \Q_K128
        vtbl.8   d24, \XLAT0, d24
        vtbl.8   d25, \XLAT0, d25
        vsub.s8  q9, \Q_K128
        vtbl.8   d26, \XLAT1, d26
        vtbl.8   d27, \XLAT1, d27
        vqadd.s8 q10, q12
        vqadd.s8 q11, q13
        vsub.s8  q10, \Q_K128
        vsub.s8  q11, \Q_K128
.endm

.macro clip16_4 Q0, Q1, Q2, Q3, Q_MIN, Q_MAX
        vmax.s16  \Q0, \Q_MIN
        vmax.s16  \Q1, \Q_MIN
        vmax.s16  \Q2, \Q_MIN
        vmax.s16  \Q3, \Q_MIN
        vmin.s16  \Q0, \Q_MAX
        vmin.s16  \Q1, \Q_MAX
        vmin.s16  \Q2, \Q_MAX
        vmin.s16  \Q3, \Q_MAX
.endm

@ Clobbers q6, q7
.macro sao_band_32_16  Q0, Q1, Q2, Q3, XLAT, Q_MIN, Q_MAX, bit_depth
        vshrn.i16 d12, \Q0, #(\bit_depth - 5)
        vshrn.i16 d13, \Q1, #(\bit_depth - 5)
        vshrn.i16 d14, \Q2, #(\bit_depth - 5)
        vshrn.i16 d15, \Q3, #(\bit_depth - 5)
        vtbl.8    d12, \XLAT, d12
        vtbl.8    d13, \XLAT, d13
        vtbl.8    d14, \XLAT, d14
        vtbl.8    d15, \XLAT, d15
        vaddw.s8  \Q0, d12
        vaddw.s8  \Q1, d13
        vaddw.s8  \Q2, d14
        vaddw.s8  \Q3, d15
        clip16_4   \Q0, \Q1, \Q2, \Q3, \Q_MIN, \Q_MAX
.endm

@ q0-q1  Offset table
@ q2     0
@ q3     (1 << bit_depth) - 1
@ q6-q7  clobbered
@ q8-q15 input/output
.macro sao_band_64_16 bit_depth
        @ 32 is wide enough for efficient code
        sao_band_32_16 q8,  q9,  q10, q11, "{d0,d1,d2,d3}", q2, q3, \bit_depth
        sao_band_32_16 q12, q13, q14, q15, "{d0,d1,d2,d3}", q2, q3, \bit_depth
.endm



function ff_hevc_sao_band_w8_neon_8, export=1
        init_sao_band
1:      subs     r12, #8
        vld1.8   {d16}, [r1, :64], r3
        vld1.8   {d17}, [r1, :64], r3
        vshr.u8  q12, q8, #3
        vld1.8   {d18}, [r1, :64], r3
        vld1.8   {d19}, [r1, :64], r3
        vshr.u8  q13, q9, #3
        vld1.8   {d20}, [r1, :64], r3
        vld1.8   {d21}, [r1, :64], r3
        vshr.u8  q14, q10, #3
        vld1.8   {d22}, [r1, :64], r3
        vld1.8   {d23}, [r1, :64], r3
        vshr.u8  q15, q11, #3
        sao_band_64
        vst1.8  {d16}, [r0, :64], r2
        vst1.8  {d17}, [r0, :64], r2
        vst1.8  {d18}, [r0, :64], r2
        vst1.8  {d19}, [r0, :64], r2
        vst1.8  {d20}, [r0, :64], r2
        vst1.8  {d21}, [r0, :64], r2
        vst1.8  {d22}, [r0, :64], r2
        vst1.8  {d23}, [r0, :64], r2
        bne    1b

        bx lr
endfunc

function ff_hevc_sao_band_w16_neon_8, export=1
        init_sao_band
1:      subs     r12, #4
        vld1.8  {q8}, [r1, :128], r3
        vshr.u8  q12, q8, #3
        vld1.8  {q9}, [r1, :128], r3
        vshr.u8  q13, q9, #3
        vld1.8  {q10}, [r1, :128], r3
        vshr.u8  q14, q10, #3
        vld1.8  {q11}, [r1, :128], r3
        vshr.u8  q15, q11, #3
        sao_band_64
        vst1.8   {q8}, [r0, :128], r2
        vst1.8   {q9}, [r0, :128], r2
        vst1.8   {q10}, [r0, :128], r2
        vst1.8   {q11}, [r0, :128], r2
        bne    1b

        bx lr
endfunc

function ff_hevc_sao_band_w32_neon_8, export=1
        init_sao_band
.if 0
1:      subs     r12, #2
        vld1.8   {q8-q9}, [r1, :128], r3
        vshr.u8  q12, q8, #3
        vshr.u8  q13, q9, #3
        vld1.8   {q10-q11}, [r1, :128], r3
        vshr.u8  q14, q10, #3
        vshr.u8  q15, q11, #3
        sao_band_64
        vst1.8   {q8-q9}, [r0, :128], r2
        vst1.8   {q10-q11}, [r0, :128], r2
        bne      1b
.else
1:
        vld1.8   {q8-q9}, [r1, :128], r3
        vld1.8   {q10-q11}, [r1, :128], r3
        subs     r12, #2
        sao_band_64_8 "{d0,d1,d2,d3}", "{d0,d1,d2,d3}", q3
        vst1.8   {q8-q9}, [r0, :128], r2
        vst1.8   {q10-q11}, [r0, :128], r2
        bne      1b
.endif

        bx       lr
endfunc





@ Standard coding rules for sao_offset_abs limit it to 0-31 (Table 9-38)
@ so we are quite safe stuffing it into a byte array
@ There may be a subsequent shl by log2_sao_offset_scale_luma/chroma
@ (7.4.3.3.2 && 7-70) but we should still be safe to at least 12 bits of
@ precision

@ This, somewhat nasty, bit of code builds the {d0-d3} translation
@ array via the stack
@ Given that sao_left_class > 28 can cause wrap we can't just poke
@ all 4 bytes in at once
@
@ It also loads other common regs

function band_load_y
        vmov.i64  q0, #0
        ldr       r12, [sp, #8]         @ &sao_offset_val[0]
        add       r12, #2               @ 1st interesting val is [1]
        vld1.16   {d16}, [r12]          @ Unaligned
        vmov.i64  q1, #0
        ldr       r12, [sp, #12]        @ sao_left_class
        vpush     {q0, q1}              @ Put zero array on stack

        @ stuff the offset vals into the array
        add       r4, r12, sp
        add       r12, #1
        vst1.8    {d16[0]}, [r4]
        and       r12, #31
        add       r4, r12, sp
        add       r12, #1
        vst1.8    {d16[2]}, [r4]
        and       r12, #31
        add       r4, r12, sp
        add       r12, #1
        vst1.8    {d16[4]}, [r4]
        and       r12, #31
        add       r4, r12, sp
        vst1.8    {d16[6]}, [r4]

        vpop      {q0, q1}              @ Pop modified array
        ldr       r12, [sp, #20]        @ height
        pld       [r1]

        subs      r12, #1
        mov       r4, r1
        it ne
        addne     r4, r3
        bx        lr
endfunc


function band_load_c
        vmov.i64  q2, #0
        ldr       r12, [sp, #8]         @ &sao_offset_val1[0]
        add       r12, #2               @ 1st interesting val is [1]
        vld1.16   {d16}, [r12]          @ Unaligned
        vmov.i64  q3, #0
        ldr       r12, [sp, #12]        @ sao_left_class
        vpush     {q2, q3}              @ Put zero array on stack

        @ stuff the offset vals into the array
        add       r4, r12, sp
        add       r12, #1
        vst1.8    {d16[0]}, [r4]
        and       r12, #31
        add       r4, r12, sp
        add       r12, #1
        vst1.8    {d16[2]}, [r4]
        and       r12, #31
        add       r4, r12, sp
        add       r12, #1
        vst1.8    {d16[4]}, [r4]
        and       r12, #31
        add       r4, r12, sp
        vst1.8    {d16[6]}, [r4]
        vpop      {q0, q1}              @ Pop modified array

        @ And again for the 2nd set
        ldr       r12, [sp, #16]        @ &sao_offset_val2[0]
        add       r12, #2               @ 1st interesting val is [1]
        vld1.16   {d16}, [r12]          @ Unaligned
        ldr       r12, [sp, #20]        @ sao_left_class2
        vpush     {q2, q3}              @ Put zero array on stack (again)

        @ stuff the offset vals into the array
        add       r4, r12, sp
        add       r12, #1
        vst1.8    {d16[0]}, [r4]
        and       r12, #31
        add       r4, r12, sp
        add       r12, #1
        vst1.8    {d16[2]}, [r4]
        and       r12, #31
        add       r4, r12, sp
        add       r12, #1
        vst1.8    {d16[4]}, [r4]
        and       r12, #31
        add       r4, r12, sp
        vst1.8    {d16[6]}, [r4]
        vpop      {q2, q3}              @ Pop modified array

        ldr       r12, [sp, #28]        @ height
        pld       [r1]

        subs      r12, #1
        mov       r4, r1
        it ne
        addne     r4, r3
        bx        lr
endfunc


@ ff_hevc_sao_band_64_neon_8 (
@   uint8_t *_dst,              [r0]
@   uint8_t *_src,              [r1]
@   ptrdiff_t stride_dst,       [r2]
@   ptrdiff_t stride_src,       [r3]
@   int16_t *sao_offset_val,    [sp, #0]
@   int sao_left_class,         [sp, #4]
@   int width,                  [sp, #8]
@   int height)                 [sp, #12]

function ff_hevc_sao_band_64_neon_8, export=1
        push      {r4, lr}
        bl        band_load_y
        vmov.u8   q15, #128

1:      subs      r12, #1
        vldm      r1, {q8-q11}
        pld       [r4]
        add       r1, r3

        sao_band_64_8 "{d0,d1,d2,d3}", "{d0,d1,d2,d3}", q15

        it ne
        addne     r4, r3
        vstm      r0, {q8-q11}
        add       r0, r2
        bpl       1b

        pop       {r4, pc}
endfunc

.macro band_64_16 bit_depth
        push      {r4, lr}
        movw      r4, #(1 << \bit_depth) - 1
        vmov.i64  q2, #0
        vdup.i16  q3, r4
        bl        band_load_y
        vpush     {q4-q7}

1:      subs      r12, #1
        vldm      r1, {q8-q15}
        pld       [r4]
        add       r1, r3
        sao_band_64_16 \bit_depth
        it ne
        addne     r4, r3
        vstm      r0, {q8-q15}
        add       r0, r2
        bpl       1b

        vpop      {q4-q7}
        pop       {r4, pc}
.endm

function ff_hevc_sao_band_64_neon_10, export=1
        band_64_16 10
endfunc

.macro band_32_16 bit_depth
        push      {r4, lr}
        movw      r4, #(1 << \bit_depth) - 1
        vmov.i64  q2, #0
        vdup.i16  q3, r4
        bl        band_load_y
        vpush     {q4-q7}

1:      subs      r12, #1
        vldm      r1, {q8-q11}
        pld       [r4]
        add       r1, r3
        sao_band_32_16 q8,  q9,  q10, q11, "{d0,d1,d2,d3}", q2, q3, \bit_depth
        it ne
        addne     r4, r3
        vstm      r0, {q8-q11}
        add       r0, r2
        bpl       1b

        vpop      {q4-q7}
        pop       {r4, pc}
.endm

function ff_hevc_sao_band_32_neon_10, export=1
        band_32_16 10
endfunc

.macro band_c_32_16 bit_depth
        push      {r4, lr}
        bl        band_load_c
        vpush     {q4-q7}
        movw      lr, #(1 << \bit_depth) - 1
        vmov.i64  q4, #0
        vdup.i16  q5, lr
        sub       r2, #96

1:      subs      r12, #1

        vld2.16   { q8, q9 }, [r1, :128]!
        vld2.16   {q10, q11}, [r1, :128]!
        vld2.16   {q12, q13}, [r1, :128]!
        vld2.16   {q14, q15}, [r1, :128], r3

        pld       [r4]
        sub       r1, #96

        sao_band_32_16 q8,  q10, q12, q14, "{d0,d1,d2,d3}", q4, q5, \bit_depth
        sao_band_32_16 q9,  q11, q13, q15, "{d4,d5,d6,d7}", q4, q5, \bit_depth

        it ne
        addne     r4, r3

        vst2.16   { q8, q9 }, [r0, :128]!
        vst2.16   {q10, q11}, [r0, :128]!
        vst2.16   {q12, q13}, [r0, :128]!
        vst2.16   {q14, q15}, [r0, :128], r2

        bpl       1b

        vpop      {q4-q7}
        pop       {r4, pc}
.endm

function ff_hevc_sao_band_c_32_neon_10, export=1
        band_c_32_16 10
endfunc

@ ff_hevc_sao_band_c_32_neon_8(
@   uint8_t * dst          [r0]
@   uint8_t * src          [r1]
@   uint32_t dst_stride    [r2]
@   uint32_t src_stride    [r3]
@   const int16_t * table1 sp[0]
@   uint32_t offset1       sp[4]
@   const int16_t * table2 sp[8]
@   uint32_t offset2       sp[12]
@   int width              sp[16]
@   int height             sp[20]

function ff_hevc_sao_band_c_32_neon_8, export=1
        push    {r4, lr}
        bl      band_load_c

        vmov.i8   q15, #128
        sub       r3, #32
        sub       r2, #32

1:      subs      r12, #1
        vld2.8    { q8, q9 }, [r1, :128]!
        vld2.8    {q10, q11}, [r1, :128], r3

        pld       [r4]

        sao_band_64_8 "{d0,d1,d2,d3}", "{d4,d5,d6,d7}", q15

        vst2.8    { q8, q9 }, [r0, :128]!
        vst2.8    {q10, q11}, [r0, :128], r2

        itt ne
        addne     r4, r3
        addne     r4, #32

        bpl       1b

        pop     {r4, pc}
endfunc


@ ff_hevc_sao_band_c_neon_8(
@   uint8_t * dst          [r0]
@   uint8_t * src          [r1]
@   uint32_t dst_stride    [r2]
@   uint32_t src_stride    [r3]
@   const int16_t * table1 sp[0]
@   uint32_t offset1       sp[4]
@   const int16_t * table2 sp[8]
@   uint32_t offset2       sp[12]
@   int width              sp[16]
@   int height             sp[20]
@
@ Widths 16-4 (less common than 32)

@ As this is often done in-place on the frame buffer it is worth preloading
@ the pixel values but we want to beware of loading ouside our buffer to avoid
@ loading stuff into the cache that should still be invalid (in use by QPU, VPU)


function ff_hevc_sao_band_c_neon_8, export=1
        mov     r12, sp
        push   {r4-r8, lr}  // 24 bytes

        ldm     r12, {r4-r7}

        add     r4, #2
        add     r6, #2
        vld1.16 {d16}, [r4]    @ Unaligned
        lsl     r5, r5, #3
        vld1.16 {d18}, [r6]
        pld     [r1]
        vmov.i8  d17, #0
        mov     r4, r1
        vmov.i8  d19, #0
        lsl     r7, r7, #3
        vdup.8  q1, r5
        ldr     r5, [r12, #16]  @ width
        vdup.8  q2, r7
        ldr     r12, [r12, #20]
        vqmovn.s16 d0, q8
        cmp     r5, #8         @ At some point we may want a table lookup
        vqmovn.s16 d1, q9
        vmov.i8 q3, #128
        add     r4, r3
        sub     r12, #1
        bls     8f

        @ d0 U lookup
        @ d1 V lookup
        @ q1 U raw offset
        @ q2 V raw offset
        @ q3 #128

        @ r4 = r1 = src - Inteded for preload pointer
        @ r12 = height

1:
        subs      r12, #1
        vld2.8    {q8, q9}, [r1, :128], r3
        vsub.u8   q12, q8, q1
        vsub.u8   q13, q9, q2
        pld       [r4]
        vshr.u8   q12, #3
        vadd.s8   q8, q3
        vshr.u8   q13, #3
        vadd.s8   q9, q3

        vtbl.8    d24, {d0}, d24
        vtbl.8    d25, {d0}, d25
        vtbl.8    d26, {d1}, d26
        vtbl.8    d27, {d1}, d27
        vqadd.s8  q8, q12
        vqadd.s8  q9, q13
        vsub.s8   q8, q3
        vsub.s8   q9, q3

        it ne
        addne     r4, r3
        vst2.8    {q8, q9}, [r0, :128], r2
        bpl       1b

        pop    {r4-r8, pc}

8:
        blt       4f
1:
        subs      r12, #1
        vld2.8    {d16, d17}, [r1, :128], r3
        vsub.u8   d24, d16, d2
        vsub.u8   d25, d17, d4
        pld       [r4]
        vshr.u8   q12, #3
        vadd.s8   q8, q3

        vtbl.8    d24, {d0}, d24
        vtbl.8    d25, {d1}, d25
        vqadd.s8  q8, q12
        vsub.s8   q8, q3

        it ne
        addne     r4, r3
        vst2.8    {d16, d17}, [r0, :128], r2
        bpl       1b

        pop    {r4-r8, pc}

@ Unusual but possible width 4
4:
        subs      r12, #2
        vld1.8    {d16}, [r1, :64], r3
        vld1.8    {d17}, [r1, :64], r3
        vuzp.8    d16, d17

        vsub.u8   d24, d16, d2
        vsub.u8   d25, d17, d4
        vshr.u8   q12, #3
        vadd.s8   q8, q3

        vtbl.8    d24, {d0}, d24
        vtbl.8    d25, {d1}, d25
        vqadd.s8  q8, q12
        vsub.s8   q8, q3

        vzip.8    d16, d17
        vst1.8    {d16}, [r0, :64], r2
        vst1.8    {d17}, [r0, :64], r2
        bpl       1b

        pop    {r4-r8, pc}
endfunc


@ =============================================================================
@ SAO EDGE

@ r0    destination address
@ r2    stride to post-increment r0 with
@ [r5]  translate values
@
@ a <- c <- b
@ a in q0 - q3
@ c in q4 - q7
@ b in q8 - q11
@
@ q12-15 used as temp
@
@ Can be used for both Y & C as we unzip/zip the deltas and
@ transform "u/v" separately via d26/d27.  For Y d26=d27

function edge_64b_body_8

        vcgt.u8 q12,  q4,  q0   @ c > a -> -1 , otherwise 0
        vcgt.u8 q13,  q5,  q1
        vcgt.u8 q14,  q6,  q2
        vcgt.u8 q15,  q7,  q3

        vcgt.u8  q0,  q4        @ a > c -> -1 , otherwise 0
        vcgt.u8  q1,  q5
        vcgt.u8  q2,  q6
        vcgt.u8  q3,  q7

        vsub.s8  q0,  q12       @ a = sign(c-a)
        vsub.s8  q1,  q13
        vsub.s8  q2,  q14
        vsub.s8  q3,  q15

        vcgt.u8  q12, q4,  q8   @ c > b -> -1 , otherwise 0
        vcgt.u8  q13, q5,  q9
        vcgt.u8  q14, q6,  q10
        vcgt.u8  q15, q7,  q11

        vsub.s8  q0,  q12
        vsub.s8  q1,  q13
        vsub.s8  q2,  q14
        vsub.s8  q3,  q15

        vcgt.u8  q12, q8,  q4   @ c < b -> -1 , otherwise 0
        vcgt.u8  q13, q9,  q5
        vcgt.u8  q14, q10, q6
        vcgt.u8  q15, q11, q7

        vadd.s8  q0,  q12       @ a = sign(c-a) + sign(c-b)
        vadd.s8  q1,  q13
        vmov.u8  q12, #2
        vadd.s8  q2,  q14
        vadd.s8  q3,  q15

        vadd.s8  q0,  q12
        vadd.s8  q1,  q12

        vld1.8   {d26, d27}, [r5]

        vadd.s8  q2,  q12
        vuzp.8   q0,  q1
        vmov.u8  q15, #128
        vadd.s8  q3,  q12       @ a = 2 + sign(c-a) + sign(c-b)

        vtbl.8   d0,  {d26}, d0
        vadd.s8  q12, q4, q15   @ Add -128 so we can use saturating signed add

        vtbl.8   d1,  {d26}, d1
        vadd.s8  q14, q5, q15

        vtbl.8   d2,  {d27}, d2
        vuzp.8   q2,  q3

        vtbl.8   d3,  {d27}, d3

        vtbl.8   d4,  {d26}, d4
        vzip.8   q0,  q1

        vtbl.8   d5,  {d26}, d5
        vqadd.s8 q0,  q12
        vqadd.s8 q1,  q14
        vadd.s8  q12, q6, q15   @ Add -128 so we can use saturating signed add

        vtbl.8   d6,  {d27}, d6
        vadd.s8  q14, q7, q15   @ Add -128 so we can use saturating signed add

        vtbl.8   d7,  {d27}, d7
        vzip.8   q2,  q3

        vsub.s8  q0,  q15
        vqadd.s8 q2,  q12
        vqadd.s8 q3,  q14
        vsub.s8  q1,  q15
        vsub.s8  q2,  q15
        vsub.s8  q3,  q15

        bx      lr
endfunc

@ r0    destination address
@ r2    stride to post-increment r0 with
@ r4    upper clip value
@ [r5]  translate values
@
@ a <- c <- b
@ a in q0 - q3
@ c in q4 - q7
@ b in q8 - q11
@
@ q12-15 used as temp
@
@ Can be used for both Y & C as we unzip/zip the deltas and
@ transform "u/v" separately via d26/d27.  For Y d26=d27

function edge_64b_body_16

        vcgt.u16 q12, q4, q0  // c > a -> -1 , otherwise 0
        vcgt.u16 q13, q5, q1
        vcgt.u16 q14, q6, q2
        vcgt.u16 q15, q7, q3

        vcgt.u16 q0, q0, q4  // a > c -> -1 , otherwise 0
        vcgt.u16 q1, q1, q5
        vcgt.u16 q2, q2, q6
        vcgt.u16 q3, q3, q7

        vsub.s16 q0, q0, q12 // a = sign(c-a)
        vsub.s16 q1, q1, q13
        vsub.s16 q2, q2, q14
        vsub.s16 q3, q3, q15

        vcgt.u16 q12, q4, q8  // c > b -> -1 , otherwise 0
        vcgt.u16 q13, q5, q9
        vcgt.u16 q14, q6, q10
        vcgt.u16 q15, q7, q11

        vsub.s16 q0, q0, q12
        vsub.s16 q1, q1, q13
        vsub.s16 q2, q2, q14
        vsub.s16 q3, q3, q15

        vcgt.u16 q12, q8, q4  // c < b -> -1 , otherwise 0
        vcgt.u16 q13, q9, q5
        vcgt.u16 q14, q10, q6
        vcgt.u16 q15, q11, q7

        vadd.s16 q0, q0, q12  // a = sign(c-a) + sign(c-b)
        vadd.s16 q1, q1, q13
        vmov.u8  q12, #2
        vadd.s16 q2, q2, q14
        vadd.s16 q3, q3, q15

        vmovn.s16 d0, q0
        vmovn.s16 d1, q1
        vmovn.s16 d2, q2
        vmovn.s16 d3, q3

        vuzp.8   q0, q1

        vld1.8   {d26, d27}, [r5]

        vadd.s8  q0, q0, q12
        vadd.s8  q1, q1, q12

        vtbl.8   d0, {d26}, d0
        vtbl.8   d1, {d26}, d1
        vtbl.8   d2, {d27}, d2
        vtbl.8   d3, {d27}, d3

        vmov.i64 q12, #0

        vzip.8   q0, q1

        vdup.i16 q13, r4

        @ Avoid overwrite whilst widening
        vaddw.s8 q2, q6, d2
        vaddw.s8 q3, q7, d3
        vaddw.s8 q1, q5, d1
        vaddw.s8 q0, q4, d0

        @ now clip
        clip16_4 q2, q3, q1, q0, q12, q13

        bx       lr
endfunc


@ a <- c <- b
@ a in q0
@ c in q1
@ b in q2
@ Temp q3, q9, q10
@
@ d16, d17 (q8) xlat U, V
@ q14.u8 #2
@ q15.u8 #128

function edge_16b_body_8
        vcgt.u8  q3,  q1,  q0   @ c > a -> -1 , otherwise 0
        vcgt.u8  q0,  q1        @ a > c -> -1 , otherwise 0
        vcgt.u8  q9,  q1,  q2   @ c > b -> -1 , otherwise 0
        vcgt.u8  q10, q2,  q1   @ c < b -> -1 , otherwise 0

        vsub.s8  q0,  q3
        vsub.s8  q10, q9
        vadd.s8  q0,  q10       @ a = sign(c-a)

        vadd.s8  q0,  q14
        vuzp.8   d0,  d1
        vadd.s8  q3,  q1, q15   @ Add -128 so we can use saturating signed add

        vtbl.8   d0,  {d16}, d0
        vtbl.8   d1,  {d17}, d1

        vzip.8   d0,  d1
        vqadd.s8 q0,  q3
        vsub.s8  q0,  q15

        bx      lr
endfunc

@ a <- c <- b
@ a in q0
@ c in q1
@ b in q2
@ Temp q3
@
@ q12, #0
@ d16, d17 xlat U, V
@ q14.u8 #2
@ q15.u16 max
function edge_16b_body_16
        vcgt.u16 q3, q1, q0     @ c > a -> -1 , otherwise 0
        vcgt.u16 q0, q1         @ a > c -> -1 , otherwise 0
        vsub.s16 q0, q3         @ a = sign(c-a)
        vcgt.u16 q3, q1, q2     @ c > b -> -1 , otherwise 0
        vsub.s16 q0, q3
        vcgt.u16 q3, q2, q1     @ c < b -> -1 , otherwise 0
        vadd.s16 q0, q3         @ a = sign(c-a) + sign(c-b)

        vmovn.s16 d0, q0
        @ d1 will have random contents that we transform but
        @ that doesn't matter as we then discard them
        vuzp.8   d0, d1

        vadd.s8  q0, q0, q14

        vtbl.8   d0, {d16}, d0
        vtbl.8   d1, {d17}, d1

        vzip.8   d0, d1

        vaddw.s8 q0, q1, d0

        @ now clip
        vmax.s16 q0, q12
        vmin.s16 q0, q15
        bx       lr
endfunc


@ ff_hevc_sao_edge_[c_]xx_neon(
@   uint8_t *_dst,                    [r0]
@   const uint8_t *_src,              [r1]
@   ptrdiff_t stride_dst,             [r2]
@   const int16_t *_sao_offset_val_u, [r3]
@   const int16_t *_sao_offset_val_v, [sp, #0]   // Chroma only
@   int eo,                           [sp, #sp_base + 0]
@   int width,                        [sp, #sp_base + 4]
@   int height)                       [sp, #sp_base + 8]

.macro  edge_xxb_init, bit_depth, is_chroma, jump_tab, setup_64b = 0, setup_16b = 0, check_w4 = 0, do2 = 0
        push     {r4-r6, lr}    @ 16 bytes
.set sp_base, 16

@ Build translate registers
@ As translate values can only be 0-4 we don't care about junk in the rest
@ of the register
        mov      r12, #2
.if \is_chroma
        ldr      r4, [sp, #16]
.set sp_base, sp_base + 4
.endif
        vld1.8   {d16[2]}, [r3], r12
        vld1.8   {d16[0]}, [r3], r12
        vld1.8   {d16[1]}, [r3], r12
        vld1.8   {d16[3]}, [r3], r12
        vld1.8   {d16[4]}, [r3]
.if \is_chroma
        vld1.8   {d17[2]}, [r4], r12
        vld1.8   {d17[0]}, [r4], r12
        vld1.8   {d17[1]}, [r4], r12
        vld1.8   {d17[3]}, [r4], r12
        vld1.8   {d17[4]}, [r4]
.else
        vmov     d17, d16
.endif

@ Setup constant registers
.if \bit_depth > 8
        movw     r4, (1 << \bit_depth) - 1
.endif
.if \setup_16b
.if \bit_depth > 8
        vmov.i64 q12, #0
        vdup.16  q15, r4
.else
        vmov.u8  q15, #128
.endif
        vmov.u8  q14, #2
.endif
        movw     r3, EDGE_SRC_STRIDE

@ If setup_64b we need the xlat table on the stack and q4-q7 saved
.if \setup_64b
        sub      r5, sp, #16
        vpush    {q4-q8}        @ 80 bytes, q8 pushed first
.set sp_base, sp_base + 80
.endif

@ Get jump address
@ We have a special case for width 4 as the calling code doesn't detect it
@ If we may have w4 then we add a 2nd jump table after the 1st
.if \check_w4
        ldr      r12, [sp, #sp_base + 4]        @ width
        cmp      r12, #8
.endif
        ldr      r12, [sp, #sp_base + 0]        @ e0
        adr      r6, \jump_tab
.if \check_w4
        it lt
        addlt    r6, #16
.endif
        ldr      r6, [r6, r12, lsl #2]

        ldr      r12, [sp, #sp_base + 8]        @ height

@ For 16 bit width 64 (or chroma 32) we need to do this in 2 passes
.if \do2
        push     {r0, r1, r6, r12}
        blx      r6
        pop      {r0, r1, r6, r12}

        add      r0, #64
        add      r1, #64
.endif

        blx      r6

@ Tidy up & return
.if \setup_64b
        vpop     {q4-q8}        @ spurious but harmless load of q8
.endif
        pop      {r4-r6, pc}
.endm


.macro  edge_16b_init, bit_depth, is_chroma, check_w4, jump_tab
        edge_xxb_init \bit_depth, \is_chroma, \jump_tab, check_w4=\check_w4, setup_16b=1
.endm

.macro  edge_64b_init, bit_depth, is_chroma, do2, jump_tab
        edge_xxb_init \bit_depth, \is_chroma, \jump_tab, do2=\do2, setup_64b=1
.endm


.macro  edge_64b_e0, body_fn, pb
        mov      r6, lr
        sub      r1, #8
1:      subs     r12, #1
        vld1.64  {d7}, [r1, :64]!
        vld1.64  {q4-q5}, [r1, :128]! // load c
        vld1.64  {q6-q7}, [r1, :128]!
        vld1.64  {d24}, [r1, :64], r3
        sub      r1, #72
        // load a
        vext.8   q0,  q3,  q4, #(16 - \pb)
        vext.8   q1,  q4,  q5, #(16 - \pb)
        vext.8   q2,  q5,  q6, #(16 - \pb)
        vext.8   q3,  q6,  q7, #(16 - \pb)
        // load b
        vext.8   q8,  q4,  q5, #\pb
        vext.8   q9,  q5,  q6, #\pb
        vext.8   q10, q6,  q7, #\pb
        vext.8   q11, q7, q12, #\pb
        bl       \body_fn
        vstm     r0, {q0-q3}
        add      r0, r0, r2
        bgt      1b
        bx       r6
.endm

.macro  edge_32bx2_e0, body_fn, pb
        mov      r6, lr

1:      subs     r12, #2

        vld1.8   {q4-q5}, [r1]
        sub      r1, #\pb
        vld1.8   {q0-q1}, [r1]
        add      r1, #(\pb * 2)
        vld1.8   {q8-q9}, [r1], r3
        sub      r1, #\pb
        vld1.8   {q6-q7}, [r1]
        sub      r1, #\pb
        vld1.8   {q2-q3}, [r1]
        add      r1, #(\pb * 2)
        vld1.8   {q10-q11}, [r1], r3
        sub      r1, #\pb

        bl       \body_fn

        vst1.8   {q0,q1}, [r0], r2
        vst1.8   {q2,q3}, [r0], r2

        bgt      1b
        bx       r6
.endm

.macro  edge_16b_e0, body_fn, pb
        mov      r6, lr
        sub      r1, #8
1:      subs     r12, #1
        vld1.64  {d7}, [r1, :64]!
        vld1.64  {q1}, [r1, :128]! // load c
        vld1.64  {d24}, [r1, :64], r3
        sub      r1, #24
        // load a
        vext.8   q0,  q3,  q1, #(16 - \pb)
        // load b
        vext.8   q2,  q1,  q12, #\pb
        bl       \body_fn
        vst1.8   {q0}, [r0], r2
        bgt      1b
        bx       r6
.endm

.macro  edge_8bx2_e0, body_fn, pb
        mov      r6, lr

1:      subs     r12, #2

        vld1.8   {d2}, [r1, :64]
        sub      r1, #\pb
        vld1.8   {d0}, [r1]
        add      r1, #(\pb * 2)
        vld1.8   {d4}, [r1], r3
        sub      r1, #\pb
        vld1.8   {d3}, [r1, :64]
        sub      r1, #\pb
        vld1.8   {d1}, [r1]
        add      r1, #(\pb * 2)
        vld1.8   {d5}, [r1], r3
        sub      r1, #\pb

        bl       \body_fn

        vst1.8   {d0}, [r0, :64], r2
        vst1.8   {d1}, [r0, :64], r2

        bgt      1b
        bx       r6
.endm

.macro  edge_4bx4_e0, body_fn, pb
        mov      r6, lr

1:      subs     r12, #4

        vld1.32  {d2[0]}, [r1]
        sub      r1, #\pb
        vld1.32  {d0[0]}, [r1]
        add      r1, #(\pb * 2)
        vld1.32  {d4[0]}, [r1], r3
        sub      r1, #\pb
        vld1.32  {d2[1]}, [r1]
        sub      r1, #\pb
        vld1.32  {d0[1]}, [r1]
        add      r1, #(\pb * 2)
        vld1.32  {d4[1]}, [r1], r3
        sub      r1, #\pb
        vld1.32  {d3[0]}, [r1]
        sub      r1, #\pb
        vld1.32  {d1[0]}, [r1]
        add      r1, #(\pb * 2)
        vld1.32  {d5[0]}, [r1], r3
        sub      r1, #\pb
        vld1.32  {d3[1]}, [r1]
        sub      r1, #\pb
        vld1.32  {d1[1]}, [r1]
        add      r1, #(\pb * 2)
        vld1.32  {d5[1]}, [r1], r3
        sub      r1, #\pb

        bl       \body_fn

        vst1.32  {d0[0]}, [r0], r2
        vst1.32  {d0[1]}, [r0], r2
        vst1.32  {d1[0]}, [r0], r2
        vst1.32  {d1[1]}, [r0], r2

        bgt      1b
        bx       r6
.endm


.macro  edge_64b_e1, body_fn
        mov      r6, lr
        sub      r1, r3
        // load a
        vld1.8   {q0-q1}, [r1, :128]!
        vld1.8   {q2-q3}, [r1, :128], r3
        sub      r1, #32
        // load c
        vld1.8   {q4-q5}, [r1, :128]!
        vld1.8   {q6-q7}, [r1, :128], r3
        sub      r1, #32
1:      subs     r12, #1
        // load b
        vld1.8   {q8-q9}, [r1, :128]!
        vld1.8   {q10-q11}, [r1, :128], r3
        sub      r1, #32
        bl       \body_fn
        vstm     r0, {q0-q3}
        add      r0, r0, r2
        // copy c to a
        vmov.64  q0, q4
        vmov.64  q1, q5
        vmov.64  q2, q6
        vmov.64  q3, q7
        // copy b to c
        vmov.64  q4, q8
        vmov.64  q5, q9
        vmov.64  q6, q10
        vmov.64  q7, q11
        bgt      1b
        bx       r6
.endm

.macro  edge_32bx2_e1, body_fn
        mov      r6, lr
        sub      r1, r3
        // load a
        vld1.8   {q0-q1}, [r1, :128], r3
        vld1.8   {q4-q5}, [r1, :128], r3

1:      subs     r12, #2
        @ Given the data duplication here we could obviously do better than
        @ using the generic body_fn but it almost certainly isn't worth it
        vmov     q2, q4
        vmov     q3, q5
        vld1.8   {q8-q9}, [r1, :128], r3
        vld1.8   {q10-q11}, [r1, :128], r3
        vmov     q6, q8
        vmov     q7, q9

        bl       \body_fn

        vst1.8   {q0,q1}, [r0], r2
        vst1.8   {q2,q3}, [r0], r2

        // copy c to a
        vmov.64  q0, q8
        vmov.64  q1, q9

        // copy b to c
        vmov.64  q4, q10
        vmov.64  q5, q11
        bgt      1b
        bx       r6
.endm

.macro  edge_16b_e1, body_fn
        mov      r6, lr
        sub      r1, r3
        // load a
        vld1.8   {q0}, [r1, :128], r3
        // load c
        vld1.8   {q1}, [r1, :128], r3
1:      subs     r12, #1
        // load b
        vld1.8   {q2}, [r1, :128], r3
        bl       \body_fn
        vst1.8   {q0}, [r0], r2
        // copy c to a
        vmov.64  q0, q1
        // copy b to c
        vmov.64  q1, q2
        bgt      1b
        bx       r6
.endm

.macro  edge_8bx2_e1, body_fn
        mov      r6, lr
        sub      r1, r3
        // load a
        vld1.8   {d0}, [r1, :64], r3
        vld1.8   {d2}, [r1, :64], r3

1:      subs     r12, #2
        @ Given the data duplication here we could obviously do better than
        @ using the generic body_fn but it almost certainly isn't worth it
        vmov.64  d1, d2
        vld1.8   {d4}, [r1, :64], r3
        vld1.8   {d5}, [r1, :64], r3
        vmov.64  d3, d4

        bl       \body_fn

        vst1.8   {d0}, [r0], r2
        vst1.8   {d1}, [r0], r2

        // copy c to a
        vmov.64  d0, d4
        // copy b to c
        vmov.64  d2, d5
        bgt      1b
        bx       r6
.endm

.macro  edge_4bx4_e1, body_fn
        mov      r6, lr
        sub      r1, r3
        // load a
        vld1.32  {d0[0]}, [r1], r3
        vld1.32  {d0[1]}, [r1], r3

1:      subs     r12, #4
        @ Given the data duplication here we could probably do better than
        @ using the generic body_fn but it almost certainly isn't worth it
        vld1.32  {d4[0]}, [r1], r3
        vld1.32  {d4[1]}, [r1], r3
        vld1.32  {d5[0]}, [r1], r3
        vld1.32  {d5[1]}, [r1], r3

        vmov.32  d1, d4
        vext.32  q1, q0, q4, #1

        bl       \body_fn

        vst1.32  {d0[0]}, [r0], r2
        vst1.32  {d0[1]}, [r0], r2
        vst1.32  {d1[0]}, [r0], r2
        vst1.32  {d1[1]}, [r0], r2

        vmov.32  d0, d5
        bgt      1b
        bx       r6
.endm

.macro  edge_64b_e2, body_fn, pb
        mov      r6, lr
        sub      r1, #32
        sub      r3, #(32 - \pb)

1:      sub      r1, r3
        // load a
        // TODO: fix unaligned load
        //       don't reload a like in eo1
        vld1.8   {q0-q1}, [r1]!
        vld1.8   {q2-q3}, [r1], r3
        subs     r12, #1
        // load  c
        vld1.8   {q4-q5}, [r1, :128]!
        vld1.8   {q6-q7}, [r1, :128], r3
        // load  b
        vld1.8   {q8-q9}, [r1]!
        vld1.8   {q10-q11}, [r1]
        sub      r1, #(64 + \pb)
        bl       \body_fn
        vstm     r0, {q0-q3}
        add      r0, r0, r2
        bgt      1b

        add      r3, #(32 - \pb)
        bx       r6
.endm

.macro  edge_32bx2_e2, body_fn, pb
        mov      r6, lr
        sub      r1, #\pb

1:      sub      r1, r3
        vld1.8   {q0-q1}, [r1], r3
        vld1.8   {q2-q3}, [r1]
        subs     r12, #2
        // load  c
        add      r1, #\pb
        vld1.8   {q4-q5}, [r1, :128], r3
        vld1.8   {q6-q7}, [r1, :128]
        // load  b
        add      r1, #\pb
        vld1.8   {q8-q9}, [r1], r3
        vld1.8   {q10-q11}, [r1]
        sub      r1, #(\pb * 2)

        bl       \body_fn

        vst1.8   {q0-q1}, [r0], r2
        vst1.8   {q2-q3}, [r0], r2
        bgt      1b

        bx       r6
.endm

.macro  edge_16b_e2, body_fn, pb
        mov      r6, lr
        add     r3, #\pb

1:      sub      r1, r3
        // load a
        vld1.8   {q0}, [r1], r3
        subs     r12, #1
        // load  c
        vld1.8   {q1}, [r1, :128], r3
        // load  b
        vld1.8   {q2}, [r1]
        sub      r1, #\pb
        bl       \body_fn
        vst1.8   {q0}, [r0], r2
        bgt      1b
        bx       r6
.endm

.macro  edge_8bx2_e2, body_fn, pb
        mov      r6, lr
        sub      r1, #\pb

1:      sub      r1, r3
        vld1.8   {d0}, [r1], r3
        vld1.8   {d1}, [r1]
        subs     r12, #2
        // load  c
        add      r1, #\pb
        vld1.8   {d2}, [r1, :64], r3
        vld1.8   {d3}, [r1, :64]
        // load  b
        add      r1, #\pb
        vld1.8   {d4}, [r1], r3
        vld1.8   {d5}, [r1]
        sub      r1, #(\pb * 2)

        bl       \body_fn

        vst1.8   {d0}, [r0], r2
        vst1.8   {d1}, [r0], r2
        bgt      1b

        bx       r6
.endm

.macro  edge_4bx4_e2, body_fn, pb
        mov      r6, lr
        sub      r1, #\pb

1:      sub      r1, r3
        @ line 0 {d0[0], -, -}          r1 lo
        vld1.32  {d0[0]}, [r1], r3
        subs     r12, #4
        @ Line 1 {d0[1], d2[0], -}      r1 lo
        vld1.32  {d0[1]}, [r1]
        add      r1, #\pb
        vld1.32  {d2[0]}, [r1], r3
        @ Line 2 {d1[0], d2[1], d4[0]}  r1 mid
        vld1.32  {d2[1]}, [r1]
        sub      r1, #\pb
        vld1.32  {d1[0]}, [r1]
        add      r1, #\pb * 2
        vld1.32  {d4[0]}, [r1], r3
        @ Line 2 {d1[1], d3[0], d4[1]}  r1 hi
        vld1.32  {d4[1]}, [r1]
        sub      r1, #\pb * 2
        vld1.32  {d1[1]}, [r1], r3
        add      r1, #\pb
        vld1.32  {d3[0]}, [r1]
        @ Line 3 {-, d3[1], d5[0]}      r1 mid
        vld1.32  {d3[1]}, [r1]
        add      r1, #\pb
        vld1.32  {d5[0]}, [r1], r3
        @ Line 4 {-, -, d5[1]}          r1 hi
        vld1.32  {d5[1]}, [r1]
        sub      r1, #(\pb * 2)

        bl       \body_fn

        vst1.32  {d0[0]}, [r0], r2
        vst1.32  {d0[1]}, [r0], r2
        vst1.32  {d1[0]}, [r0], r2
        vst1.32  {d1[1]}, [r0], r2
        bgt      1b

        bx       r6
.endm

.macro  edge_64b_e3, body_fn, pb
        @ e3 is the same as e2 but with the X offset reversed
        edge_64b_e2 \body_fn, (-\pb)
.endm

.macro  edge_32bx2_e3, body_fn, pb
        @ e3 is the same as e2 but with the X offset reversed
        edge_32bx2_e2 \body_fn, (-\pb)
.endm

.macro  edge_16b_e3, body_fn, pb
        @ e3 is the same as e2 but with the X offset reversed
        edge_16b_e2 \body_fn, (-\pb)
.endm

.macro  edge_8bx2_e3, body_fn, pb
        @ e3 is the same as e2 but with the X offset reversed
        edge_8bx2_e2 \body_fn, (-\pb)
.endm

.macro  edge_4bx4_e3, body_fn, pb
        @ e3 is the same as e2 but with the X offset reversed
        edge_4bx4_e2 \body_fn, (-\pb)
.endm

.macro edge_64b_bodies, body_fn, pb
        .word   0f
        .word   10f
        .word   20f
        .word   30f

0:      edge_64b_e0     \body_fn, \pb
10:     edge_64b_e1     \body_fn
20:     edge_64b_e2     \body_fn, \pb
30:     edge_64b_e3     \body_fn, \pb
.endm

.macro edge_32bx2_bodies, body_fn, pb
        .word   0f
        .word   10f
        .word   20f
        .word   30f

0:      edge_32bx2_e0   \body_fn, \pb
10:     edge_32bx2_e1   \body_fn
20:     edge_32bx2_e2   \body_fn, \pb
30:     edge_32bx2_e3   \body_fn, \pb
.endm

.macro edge_16b_bodies, body_fn, pb
        .word   0f
        .word   10f
        .word   20f
        .word   30f

0:      edge_16b_e0     \body_fn, \pb
10:     edge_16b_e1     \body_fn
20:     edge_16b_e2     \body_fn, \pb
30:     edge_16b_e3     \body_fn, \pb
.endm

.macro edge_32bx2_16b_bodies, body_fn_64b, body_fn_16b, pb
        .word   0f
        .word   10f
        .word   20f
        .word   30f
        .word   5f
        .word   15f
        .word   25f
        .word   35f

0:      edge_32bx2_e0   \body_fn_64b, \pb
10:     edge_32bx2_e1   \body_fn_64b
20:     edge_32bx2_e2   \body_fn_64b, \pb
30:     edge_32bx2_e3   \body_fn_64b, \pb
5:      edge_16b_e0     \body_fn_16b, \pb
15:     edge_16b_e1     \body_fn_16b
25:     edge_16b_e2     \body_fn_16b, \pb
35:     edge_16b_e3     \body_fn_16b, \pb
.endm

.macro edge_16b_8bx2_bodies, body_fn, pb
        .word   0f
        .word   10f
        .word   20f
        .word   30f
        .word   5f
        .word   15f
        .word   25f
        .word   35f

0:      edge_16b_e0     \body_fn, \pb
10:     edge_16b_e1     \body_fn
20:     edge_16b_e2     \body_fn, \pb
30:     edge_16b_e3     \body_fn, \pb
5:      edge_8bx2_e0    \body_fn, \pb
15:     edge_8bx2_e1    \body_fn
25:     edge_8bx2_e2    \body_fn, \pb
35:     edge_8bx2_e3    \body_fn, \pb
.endm

.macro edge_8bx2_4bx4_bodies, body_fn, pb
        .word   0f
        .word   10f
        .word   20f
        .word   30f
        .word   5f
        .word   15f
        .word   25f
        .word   35f

0:      edge_8bx2_e0    \body_fn, \pb
10:     edge_8bx2_e1    \body_fn
20:     edge_8bx2_e2    \body_fn, \pb
30:     edge_8bx2_e3    \body_fn, \pb
5:      edge_4bx4_e0    \body_fn, \pb
15:     edge_4bx4_e1    \body_fn
25:     edge_4bx4_e2    \body_fn, \pb
35:     edge_4bx4_e3    \body_fn, \pb
.endm

@ void ff_hevc_sao_edge_8_neon_8(
@   uint8_t *_dst,            [r0]
@   uint8_t *_src,            [r1]
@   int  stride_dst,          [r2]
@   int16_t *_sao_offset_val, [r3]
@   int eo,                   [sp, #0]
@   int width,                [sp, #4]
@   int height)               [sp, #8]

function ff_hevc_sao_edge_8_neon_8, export=1
        edge_16b_init   8, 0, 1, 99f
99:
        edge_8bx2_4bx4_bodies edge_16b_body_8, 1
endfunc

@ void ff_hevc_sao_edge_16_neon_8(
@   uint8_t *_dst,            [r0]
@   uint8_t *_src,            [r1]
@   int  stride_dst,          [r2]
@   int16_t *_sao_offset_val, [r3]
@   int eo,                   [sp, #0]
@   int width,                [sp, #4]
@   int height)               [sp, #8]

function ff_hevc_sao_edge_16_neon_8, export=1
        edge_16b_init   8, 0, 0, 99f
99:
        edge_16b_bodies edge_16b_body_8, 1
endfunc

@ void ff_hevc_sao_edge_32_neon_8(
@   uint8_t *_dst,            [r0]
@   uint8_t *_src,            [r1]
@   int  stride_dst,          [r2]
@   int16_t *_sao_offset_val, [r3]
@   int eo,                   [sp, #0]
@   int width,                [sp, #4]
@   int height)               [sp, #8]

function ff_hevc_sao_edge_32_neon_8, export=1
        edge_64b_init   8, 0, 0, 99f
99:
        edge_32bx2_bodies edge_64b_body_8, 1
endfunc

@ void ff_hevc_sao_edge_64_neon_8(
@   uint8_t *_dst,            [r0]
@   uint8_t *_src,            [r1]
@   int  stride_dst,          [r2]
@   int16_t *_sao_offset_val, [r3]
@   int eo,                   [sp, #0]
@   int width,                [sp, #4]
@   int height)               [sp, #8]

function ff_hevc_sao_edge_64_neon_8, export=1
        edge_64b_init   8, 0, 0, 99f
99:
        edge_64b_bodies edge_64b_body_8, 1
endfunc

@ ff_hevc_sao_edge_c_8_neon_8(
@   uint8_t *_dst,                    [r0]
@   const uint8_t *_src,              [r1]
@   ptrdiff_t stride_dst,             [r2]
@   const int16_t *_sao_offset_val_u, [r3]
@   const int16_t *_sao_offset_val_v, [sp, #0]
@   int eo,                           [sp, #4]
@   int width,                        [sp, #8]
@   int height)                       [sp, #12]

function ff_hevc_sao_edge_c_8_neon_8, export=1
        edge_16b_init   8, 1, 1, 99f
99:
        edge_16b_8bx2_bodies edge_16b_body_8, 2
endfunc

@ ff_hevc_sao_edge_c_16_neon_8(
@   uint8_t *_dst,                    [r0]
@   const uint8_t *_src,              [r1]
@   ptrdiff_t stride_dst,             [r2]
@   const int16_t *_sao_offset_val_u, [r3]
@   const int16_t *_sao_offset_val_v, [sp, #0]
@   int eo,                           [sp, #4]
@   int width,                        [sp, #8]
@   int height)                       [sp, #12]

function ff_hevc_sao_edge_c_16_neon_8, export=1
        edge_64b_init   8, 1, 0, 99f
99:
        edge_32bx2_bodies edge_64b_body_8, 2
endfunc

@ ff_hevc_sao_edge_c_32_neon_8(
@   uint8_t *_dst,                    [r0]
@   const uint8_t *_src,              [r1]
@   ptrdiff_t stride_dst,             [r2]
@   const int16_t *_sao_offset_val_u, [r3]
@   const int16_t *_sao_offset_val_v, [sp, #0]
@   int eo,                           [sp, #4]
@   int width,                        [sp, #8]
@   int height)                       [sp, #12]

function ff_hevc_sao_edge_c_32_neon_8, export=1
        edge_64b_init   8, 1, 0, 99f
99:
        edge_64b_bodies edge_64b_body_8, 2
endfunc

@ void ff_hevc_sao_edge_8_neon_10(
@   uint8_t *_dst,            [r0]
@   uint8_t *_src,            [r1]
@   int  stride_dst,          [r2]
@   int16_t *_sao_offset_val, [r3]
@   int eo,                   [sp, #0]
@   int width,                [sp, #4]
@   int height)               [sp, #8]

function ff_hevc_sao_edge_8_neon_10, export=1
        edge_16b_init   10, 0, 1, 99f
99:
        edge_16b_8bx2_bodies edge_16b_body_16, 2
endfunc

@ void ff_hevc_sao_edge_16_neon_10(
@   uint8_t *_dst,            [r0]
@   uint8_t *_src,            [r1]
@   int  stride_dst,          [r2]
@   int16_t *_sao_offset_val, [r3]
@   int eo,                   [sp, #0]
@   int width,                [sp, #4]
@   int height)               [sp, #8]

function ff_hevc_sao_edge_16_neon_10, export=1
        edge_64b_init   10, 0, 0, 99f
99:
        edge_32bx2_bodies edge_64b_body_16, 2
endfunc

@ void ff_hevc_sao_edge_64_neon_10(
@   uint8_t *_dst,            [r0]
@   uint8_t *_src,            [r1]
@   int  stride_dst,          [r2]
@   int16_t *_sao_offset_val, [r3]
@   int eo,                   [sp, #0]
@   int width,                [sp, #4]
@   int height)               [sp, #8]

@ We simply split the 32 case into 2 vertical stripes
@ and call the fns for w32
@
@ Calling code will always have src != dst so we don't have to worry
@ about edge effects

function ff_hevc_sao_edge_64_neon_10, export=1
        edge_64b_init   10, 0, 1, 99f
endfunc

@ void ff_hevc_sao_edge_32_neon_10(
@   uint8_t *_dst,            [r0]
@   uint8_t *_src,            [r1]
@   int  stride_dst,          [r2]
@   int16_t *_sao_offset_val, [r3]
@   int eo,                   [sp, #0]
@   int width,                [sp, #4]
@   int height)               [sp, #8]

function ff_hevc_sao_edge_32_neon_10, export=1
        edge_64b_init   10, 0, 0, 99f
99:
        edge_64b_bodies edge_64b_body_16, 2
endfunc

@ ff_hevc_sao_edge_c_8_neon_10(
@   uint8_t *_dst,                    [r0]
@   const uint8_t *_src,              [r1]
@   ptrdiff_t stride_dst,             [r2]
@   const int16_t *_sao_offset_val_u, [r3]
@   const int16_t *_sao_offset_val_v, [sp, #0]
@   int eo,                           [sp, #4]
@   int width,                        [sp, #8]
@   int height)                       [sp, #12]

function ff_hevc_sao_edge_c_8_neon_10, export=1
        edge_xxb_init   10, 1, 99f, check_w4=1, setup_16b=1, setup_64b=1
99:
        edge_32bx2_16b_bodies edge_64b_body_16, edge_16b_body_16, 4
endfunc

@ ff_hevc_sao_edge_c_32_neon_10(
@   uint8_t *_dst,                    [r0]
@   const uint8_t *_src,              [r1]
@   ptrdiff_t stride_dst,             [r2]
@   const int16_t *_sao_offset_val_u, [r3]
@   const int16_t *_sao_offset_val_v, [sp, #0]
@   int eo,                           [sp, #4]
@   int width,                        [sp, #8]
@   int height)                       [sp, #12]

function ff_hevc_sao_edge_c_32_neon_10, export=1
        edge_64b_init   10, 1, 1, 99f
endfunc


@ ff_hevc_sao_edge_c_16_neon_10(
@   uint8_t *_dst,                    [r0]
@   const uint8_t *_src,              [r1]
@   ptrdiff_t stride_dst,             [r2]
@   const int16_t *_sao_offset_val_u, [r3]
@   const int16_t *_sao_offset_val_v, [sp, #0]
@   int eo,                           [sp, #4]
@   int width,                        [sp, #8]
@   int height)                       [sp, #12]

function ff_hevc_sao_edge_c_16_neon_10, export=1
        edge_64b_init   10, 1, 0, 99f
99:
        edge_64b_bodies edge_64b_body_16, 4
endfunc

