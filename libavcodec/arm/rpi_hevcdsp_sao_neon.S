/*
 * Copyright (c) 2014 - 2015 Seppo Tomperi <seppo.tomperi@vtt.fi>
 *               2017 John Cox <jc@kynesim.co.uk> (for Raspberry Pi)
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

@ PIC jump tables are fractionally more expensive than absolute in our code
.set jent_pic, CONFIG_PIC


.macro sao_band_64b_8 XLAT0, XLAT1, Q_K128, I1, I2, I3, I4
        vshr.u8   q12, q8, #3
        \I1
        vadd.i8   q8, \Q_K128
        \I2
        vshr.u8   q13, q9, #3
        \I3
        vadd.i8   q9, \Q_K128
        \I4
        vtbl.8    d24, \XLAT0, d24
        vtbl.8    d25, \XLAT0, d25
        vtbl.8    d26, \XLAT1, d26
        vtbl.8    d27, \XLAT1, d27

        vqadd.s8  q8, q12
        vshr.u8   q12, q10, #3
        vadd.i8   q10, \Q_K128
        vqadd.s8  q9, q13
        vshr.u8   q13, q11, #3
        vadd.i8   q11, \Q_K128

        vtbl.8    d24, \XLAT0, d24
        vtbl.8    d25, \XLAT0, d25
        vtbl.8    d26, \XLAT1, d26
        vtbl.8    d27, \XLAT1, d27
        vqadd.s8  q10, q12
        vsub.i8   q8, \Q_K128
        vqadd.s8  q11, q13
        vsub.i8   q9, \Q_K128
        vsub.i8   q10, \Q_K128
        vsub.i8   q11, \Q_K128
.endm

.macro sao_band_16b_8 XLAT0, XLAT1, Q_K128, L1, L2, L3, L4, L5, S1, S2, S3, S4
        \L1
        \L2
        \L3
        \L4
        \L5
        vadd.i8   q12, q8, \Q_K128
        vshr.u8   q8, #3
        vtbl.8    d16, \XLAT0, d16
        vtbl.8    d17, \XLAT1, d17
        vqadd.s8  q12, q8
        bmi       2f
1:        \L1
          \L2
          \L3
          \L4
          \L5
        vsub.i8   q13, q12, \Q_K128
          vadd.i8   q12, q8, \Q_K128
          vshr.u8   q8, #3
        \S1
        \S2
        \S3
        \S4
          vtbl.8    d16, \XLAT0, d16
          vtbl.8    d17, \XLAT1, d17
          vqadd.s8  q12, q8
          bpl       1b
2:        vsub.i8   q13, q12, \Q_K128
          \S1
          \S2
          \S3
          \S4
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

@ Clobbers q12, q13
.macro sao_band_64b_16  Q0, Q1, Q2, Q3, XLAT0, XLAT1, Q_MIN, Q_MAX, bit_depth, I1, I2
        vshrn.i16 d24, \Q0, #(\bit_depth - 5)
        vshrn.i16 d25, \Q1, #(\bit_depth - 5)
        vshrn.i16 d26, \Q2, #(\bit_depth - 5)
        \I1
        vtbl.8    d24, \XLAT0, d24
        vshrn.i16 d27, \Q3, #(\bit_depth - 5)
        vtbl.8    d25, \XLAT1, d25
        \I2
        vtbl.8    d26, \XLAT0, d26
        vtbl.8    d27, \XLAT1, d27
        vaddw.s8  \Q0, d24
        vaddw.s8  \Q1, d25
        vaddw.s8  \Q2, d26
        vaddw.s8  \Q3, d27
        clip16_4   \Q0, \Q1, \Q2, \Q3, \Q_MIN, \Q_MAX
.endm

@ Clobbers q10, q11, q12
.macro sao_band_32b_16 Q0, Q1, XLAT0, XLAT1, Q_MIN, Q_MAX, bit_depth, L1, L2, L3, L4, L5, S1, S2, S3, S4
        \L1
        \L2
        \L3
        \L4
        \L5
        vshrn.i16 d24, \Q0, #\bit_depth - 5
        vshrn.i16 d25, \Q1, #\bit_depth - 5
        vtbl.8    d24, \XLAT0, d24
        vtbl.8    d25, \XLAT1, d25
        vaddw.s8  q10, \Q0, d24
        vaddw.s8  q11, \Q1, d25
        bmi       2f
1:        \L1
          \L2
          \L3
          \L4
          \L5
        vmax.s16  q10, \Q_MIN
        vmax.s16  q11, \Q_MIN
          vshrn.i16 d24, \Q0, #\bit_depth - 5
          vshrn.i16 d25, \Q1, #\bit_depth - 5
        vmin.s16  q10, \Q_MAX
        vmin.s16  q11, \Q_MAX
        \S1
        \S2
        \S3
        \S4
          vtbl.8    d24, \XLAT0, d24
          vtbl.8    d25, \XLAT1, d25
          vaddw.s8  q10, \Q0, d24
          vaddw.s8  q11, \Q1, d25
          bpl       1b
2:        vmax.s16  q10, \Q_MIN
          vmax.s16  q11, \Q_MIN
          vmin.s16  q10, \Q_MAX
          vmin.s16  q11, \Q_MAX
          \S1
          \S2
          \S3
          \S4
.endm


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

@ Beware that the offset read here overrreads by 6 bytes so source must be sized appropriately
function band_load_y
        ldr       ip, [sp, #16]         @ &sao_offset_val[0]
        ldr       r4, [sp, #20]         @ sao_left_class
        vmov.i64  d4, #0
        vmov.i64  q0, #0
        pld       [r1]
        vld2.8    {q8}, [ip]
        sub       ip, sp, #8*5
        vmov.i64  q1, #0
        add       r4, ip, r4
        vpush     {d0-d4}               @ Put zero array on stack
        vshr.u64  d16, d16, #8          @ 1st interesting val is [1]
        ldr       ip, [ip, #8*5 + 28]   @ height
        vst1.32   {d16[0]}, [r4]
        add       r4, r1, r3
        vpop      {d0-d4}               @ Pop modified array
        sub       ip, ip, #1
        vorr      d0, d0, d4
        bx        lr
endfunc

@ Beware that offset reads here overrread by 6 bytes so source must be sized appropriately
function band_load_c
        ldr       ip, [sp, #16]         @ &sao_offset_val1[0]
        ldr       r4, [sp, #20]         @ sao_left_class1
        vmov.i64  d24, #0
        vmov.i64  q10, #0
        pld       [r1]
        vld2.8    {q8}, [ip]
        sub       ip, sp, #8*5
        vmov.i64  q11, #0
        add       r4, ip, r4
        ldr       ip, [sp, #24]         @ &sao_offset_val2[0]
        vpush     {d20-d24}             @ Put zero array on stack
        vld2.8    {q9}, [ip]
        vshr.u64  d16, d16, #8          @ 1st interesting val is [1]
        ldr       ip, [sp, #8*5 + 28]   @ sao_left_class2
        vst1.32   {d16[0]}, [r4]
        add       ip, sp, ip
        vshr.u64  d18, d18, #8          @ 1st interesting val is [1]
        vldmia    sp, {d0-d3}           @ Load modified array
        vldr      d16, [sp, #8*4]
        add       r4, r1, r3
        vstmia    sp, {d20-d24}         @ Put zero array on stack (again)
        vst1.32   {d18[0]}, [ip]
        vorr      d0, d0, d16
        vldmia    sp, {d4-d7}           @ Load modified array
        vldr      d18, [sp, #8*4]
        ldr       ip, [sp, #8*5 + 36]   @ height
        add       sp, sp, #8*5
        vorr      d4, d4, d18
        sub       ip, ip, #1
        bx        lr
endfunc


@ ff_hevc_rpi_sao_band_64_neon_8 (
@   uint8_t *_dst,              [r0]
@   uint8_t *_src,              [r1]
@   ptrdiff_t stride_dst,       [r2]
@   ptrdiff_t stride_src,       [r3]
@   int16_t *sao_offset_val,    [sp, #0]
@   int sao_left_class,         [sp, #4]
@   int width,                  [sp, #8]
@   int height)                 [sp, #12]

function ff_hevc_rpi_sao_band_64_neon_8, export=1
        push      {r4-r6, lr}
        vmov.u8   q15, #128
        bl        band_load_y

1:      vldmia    r1, {q8-q11}
        sao_band_64b_8 {d0-d3}, {d0-d3}, q15, \
            "pld       [r4]",                 \
            "subs      ip, #1",               \
            "it ne; addne r4, r3",            \
            "add       r1, r3"
        vstmia    r0, {q8-q11}
        add       r0, r2
        bpl       1b

        pop       {r4-r6, pc}
endfunc

@ ff_hevc_rpi_sao_band_32_neon_8 (
@   uint8_t *_dst,              [r0]
@   uint8_t *_src,              [r1]
@   ptrdiff_t stride_dst,       [r2]
@   ptrdiff_t stride_src,       [r3]
@   int16_t *sao_offset_val,    [sp, #0]
@   int sao_left_class,         [sp, #4]
@   int width,                  [sp, #8]
@   int height)                 [sp, #12]

function ff_hevc_rpi_sao_band_32_neon_8, export=1
        push      {r4-r6, lr}
        add       r5, r0, r2
        add       r6, r1, r3
        lsl       r2, #1
        lsl       r3, #1
        vmov.u8   q15, #128
        bl        band_load_y

1:      vld1.8    { q8, q9 }, [r1, :128], r3
        subs      ip, #2
        vld1.8    {q10, q11}, [r6, :128], r3

        sao_band_64b_8 {d0-d3}, {d0-d3}, q15

        vst1.8    { q8, q9 }, [r0, :128], r2
        vst1.8    {q10, q11}, [r5, :128], r2
        bpl       1b

        pop       {r4-r6, pc}
endfunc

@ ff_hevc_rpi_sao_band_16_neon_8 (
@   uint8_t *_dst,              [r0]
@   uint8_t *_src,              [r1]
@   ptrdiff_t stride_dst,       [r2]
@   ptrdiff_t stride_src,       [r3]
@   int16_t *sao_offset_val,    [sp, #0]
@   int sao_left_class,         [sp, #4]
@   int width,                  [sp, #8]
@   int height)                 [sp, #12]

function ff_hevc_rpi_sao_band_16_neon_8, export=1
        push      {r4-r6, lr}
        add       r5, r0, r2
        add       r6, r1, r3
        lsl       r2, #1
        lsl       r3, #1
        vmov.u8   q15, #128
        bl        band_load_y

1:      vld1.8    { q8}, [r1, :128], r3
        subs      ip, #4
        vld1.8    { q9}, [r6, :128], r3
        vld1.8    {q10}, [r1, :128], r3
        vld1.8    {q11}, [r6, :128], r3

        sao_band_64b_8 {d0-d3}, {d0-d3}, q15

        vst1.8    { q8}, [r0, :128], r2
        vst1.8    { q9}, [r5, :128], r2
        vst1.8    {q10}, [r0, :128], r2
        vst1.8    {q11}, [r5, :128], r2
        bpl       1b

        pop       {r4-r6, pc}
endfunc

@ ff_hevc_rpi_sao_band_8_neon_8 (
@   uint8_t *_dst,              [r0]
@   uint8_t *_src,              [r1]
@   ptrdiff_t stride_dst,       [r2]
@   ptrdiff_t stride_src,       [r3]
@   int16_t *sao_offset_val,    [sp, #0]
@   int sao_left_class,         [sp, #4]
@   int width,                  [sp, #8]
@   int height)                 [sp, #12]

function ff_hevc_rpi_sao_band_8_neon_8, export=1
        ldr       ip, [sp, #8]          @ width
        push      {r4-r6, lr}
        vmov.u8   q15, #128
        cmp       ip, #8
        bl        band_load_y
        add       r5, r0, r2
        add       r6, r1, r3
        lsl       r2, #1
        lsl       r3, #1
        blt       4f

        sao_band_16b_8 {d0-d3}, {d0-d3}, q15, \
            "vld1.8    {d16}, [r1, :64], r3", \
            "subs      ip, #2",               \
            "vld1.8    {d17}, [r6, :64], r3", \
            "",                               \
            "",                               \
            "vst1.8 {d26}, [r0, :64], r2",    \
            "vst1.8 {d27}, [r5, :64], r2"
        pop       {r4-r6, pc}
4:
        sao_band_16b_8 {d0-d3}, {d0-d3}, q15,    \
            "vld1.32   {d16[0]}, [r1, :32], r3", \
            "subs      ip, #4",                  \
            "vld1.32   {d16[1]}, [r6, :32], r3", \
            "vld1.32   {d17[0]}, [r1, :32], r3", \
            "vld1.32   {d17[1]}, [r6, :32], r3", \
            "vst1.32   {d26[0]}, [r0, :32], r2", \
            "vst1.32   {d26[1]}, [r5, :32], r2", \
            "vst1.32   {d27[0]}, [r0, :32], r2", \
            "vst1.32   {d27[1]}, [r5, :32], r2"
        pop       {r4-r6, pc}
endfunc

@ ff_hevc_rpi_sao_band_c_32_neon_8(
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

function ff_hevc_rpi_sao_band_c_32_neon_8, export=1
        push      {r4-r6, lr}
        add       r5, r0, #32
        add       r6, r1, #32
        vmov.u8   q15, #128
        bl        band_load_c

1:      vld2.8    { q8, q9 }, [r1, :128], r3
        subs      ip, #1
        vld2.8    {q10, q11}, [r6, :128], r3

        sao_band_64b_8 {d0-d3}, {d4-d7}, q15, \
            "pld       [r4]",                 \
            "it ne; addne r4, r3"

        vst2.8    { q8, q9 }, [r0, :128], r2
        vst2.8    {q10, q11}, [r5, :128], r2
        bpl       1b

        pop     {r4-r6, pc}
endfunc

@ ff_hevc_rpi_sao_band_c_16_neon_8(
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

function ff_hevc_rpi_sao_band_c_16_neon_8, export=1
        push      {r4-r6, lr}
        add       r5, r0, r2
        add       r6, r1, r3
        lsl       r2, #1
        lsl       r3, #1
        vmov.u8   q15, #128
        bl        band_load_c

1:      vld2.8    { q8, q9 }, [r1, :128], r3
        subs      ip, #2
        vld2.8    {q10, q11}, [r6, :128], r3

        sao_band_64b_8 {d0-d3}, {d4-d7}, q15

        vst2.8    { q8, q9 }, [r0, :128], r2
        vst2.8    {q10, q11}, [r5, :128], r2
        bpl       1b

        pop     {r4-r6, pc}
endfunc

@ ff_hevc_rpi_sao_band_c_8_neon_8(
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

function ff_hevc_rpi_sao_band_c_8_neon_8, export=1
        ldr       ip, [sp, #16]         @ width
        push      {r4-r6, lr}
        vmov.u8   q15, #128
        cmp       ip, #8
        bl        band_load_c
        blt       4f

        sao_band_16b_8 {d0-d3}, {d4-d7}, q15,      \
            "vld2.8    {d16-d17}, [r1, :128], r3", \
            "subs      ip, #1",                    \
            "",                                    \
            "",                                    \
            "",                                    \
            "vst2.8    {d26-d27}, [r0, :128], r2"
        pop       {r4-r6, pc}
4:
        add       r5, r0, r2
        add       r6, r1, r3
        lsl       r2, #1
        lsl       r3, #1
        sao_band_16b_8 {d0-d3}, {d4-d7}, q15, \
            "vld1.8    {d16}, [r1, :64], r3", \
            "subs      ip, #2",               \
            "vld1.8    {d17}, [r6, :64], r3", \
            "vuzp.8    d16, d17",             \
            "",                               \
            "vzip.8    d26, d27",             \
            "vst1.8    {d26}, [r0, :64], r2", \
            "vst1.8    {d27}, [r5, :64], r2"
        pop       {r4-r6, pc}
endfunc


@ ff_hevc_rpi_sao_band_64_neon_10 (
@   uint8_t *_dst,              [r0]
@   uint8_t *_src,              [r1]
@   ptrdiff_t stride_dst,       [r2]
@   ptrdiff_t stride_src,       [r3]
@   int16_t *sao_offset_val,    [sp, #0]
@   int sao_left_class,         [sp, #4]
@   int width,                  [sp, #8]
@   int height)                 [sp, #12]

.macro band_64_16 bit_depth
        push      {r4-r6, lr}
        vmov.i64  q2, #0
        vmov.i16  q3, #(1 << \bit_depth) - 1
        bl        band_load_y
        vpush     {q4-q7}

1:      vldm      r1, {q4-q11}
        sao_band_64b_16 q4,  q5,  q6,  q7, {d0-d3}, {d0-d3}, q2, q3, \bit_depth, \
            "subs      ip, #1",                                                  \
            "add       r1, r3"
        sao_band_64b_16 q8,  q9, q10, q11, {d0-d3}, {d0-d3}, q2, q3, \bit_depth
        vstm      r0, {q4-q11}
        add       r0, r2
        bpl       1b

        vpop      {q4-q7}
        pop       {r4-r6, pc}
.endm

function ff_hevc_rpi_sao_band_64_neon_10, export=1
        band_64_16 10
endfunc

@ ff_hevc_rpi_sao_band_32_neon_10 (
@   uint8_t *_dst,              [r0]
@   uint8_t *_src,              [r1]
@   ptrdiff_t stride_dst,       [r2]
@   ptrdiff_t stride_src,       [r3]
@   int16_t *sao_offset_val,    [sp, #0]
@   int sao_left_class,         [sp, #4]
@   int width,                  [sp, #8]
@   int height)                 [sp, #12]

.macro band_32_16 bit_depth
        push      {r4-r6, lr}
        vmov.i64  q2, #0
        vmov.i16  q3, #(1 << \bit_depth) - 1
        bl        band_load_y

1:      vldm      r1, {q8-q11}
        sao_band_64b_16 q8,  q9,  q10, q11, {d0-d3}, {d0-d3}, q2, q3, \bit_depth, \
            "subs      ip, #1",                                                   \
            "add       r1, r3"
        vstm      r0, {q8-q11}
        add       r0, r2
        bpl       1b

        pop       {r4-r6, pc}
.endm

function ff_hevc_rpi_sao_band_32_neon_10, export=1
        band_32_16 10
endfunc

@ ff_hevc_rpi_sao_band_16_neon_10 (
@   uint8_t *_dst,              [r0]
@   uint8_t *_src,              [r1]
@   ptrdiff_t stride_dst,       [r2]
@   ptrdiff_t stride_src,       [r3]
@   int16_t *sao_offset_val,    [sp, #0]
@   int sao_left_class,         [sp, #4]
@   int width,                  [sp, #8]
@   int height)                 [sp, #12]

.macro band_16_16 bit_depth
        push      {r4-r6, lr}
        add       r5, r0, r2
        add       r6, r1, r3
        lsl       r2, #1
        lsl       r3, #1
        vmov.i64  q14, #0
        vmov.i16  q15, #(1 << \bit_depth) - 1
        bl        band_load_y

1:      vld1.16   { q8, q9 }, [r1, :128], r3
        subs      r12, #2
        vld1.16   {q10, q11}, [r6, :128], r3
        sao_band_64b_16 q8,  q9,  q10, q11, {d0-d3}, {d0-d3}, q14, q15, \bit_depth
        vst1.16   { q8, q9 }, [r0, :128], r2
        vst1.16   {q10, q11}, [r5, :128], r2
        bpl       1b

        pop       {r4-r6, pc}
.endm

function ff_hevc_rpi_sao_band_16_neon_10, export=1
        band_16_16 10
endfunc

@ ff_hevc_rpi_sao_band_8_neon_10 (
@   uint8_t *_dst,              [r0]
@   uint8_t *_src,              [r1]
@   ptrdiff_t stride_dst,       [r2]
@   ptrdiff_t stride_src,       [r3]
@   int16_t *sao_offset_val,    [sp, #0]
@   int sao_left_class,         [sp, #4]
@   int width,                  [sp, #8]
@   int height)                 [sp, #12]

.macro band_8_16 bit_depth
        ldr       ip, [sp, #8]          @ width
        push      {r4-r6, lr}
        vmov.i64  q14, #0
        cmp       ip, #8
        vmov.i16  q15, #(1 << \bit_depth) - 1
        bl        band_load_y
        add       r5, r0, r2
        add       r6, r1, r3
        lsl       r2, #1
        lsl       r3, #1
        blt       4f

        sao_band_32b_16 q8, q9, {d0-d3}, {d0-d3}, q14, q15, \bit_depth, \
            "vld1.16   {q8}, [r1, :128], r3",                           \
            "subs      ip, #2",                                         \
            "vld1.16   {q9}, [r6, :128], r3",                           \
            "",                                                         \
            "",                                                         \
            "vst1.16   {q10}, [r0, :128], r2",                          \
            "vst1.16   {q11}, [r5, :128], r2"
        pop       {r4-r6, pc}
4:
        sao_band_32b_16 q8, q9, {d0-d3}, {d0-d3}, q14, q15, \bit_depth, \
            "vld1.16   {d16}, [r1, :64], r3",                           \
            "subs      ip, #4",                                         \
            "vld1.16   {d17}, [r6, :64], r3",                           \
            "vld1.16   {d18}, [r1, :64], r3",                           \
            "vld1.16   {d19}, [r6, :64], r3",                           \
            "vst1.16   {d20}, [r0, :64], r2",                           \
            "vst1.16   {d21}, [r5, :64], r2",                           \
            "vst1.16   {d22}, [r0, :64], r2",                           \
            "vst1.16   {d23}, [r5, :64], r2"
        pop       {r4-r6, pc}
.endm

function ff_hevc_rpi_sao_band_8_neon_10, export=1
        band_8_16 10
endfunc


@ ff_hevc_rpi_sao_band_c_32_neon_10(
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

.macro band_c_32_16 bit_depth
        push      {r4-r6, lr}
        add       r5, r0, #32
        add       r6, r1, #32
        sub       r2, #64
        sub       r3, #64
        vmov.i64  q14, #0
        vmov.i16  q15, #(1 << \bit_depth) - 1
        bl        band_load_c
        mov       lr, #64
        vpush     {q4-q7}

1:      vld2.16   { q4, q5 }, [r1, :128], lr
        subs      ip, #1
        vld2.16   { q6, q7 }, [r6, :128], lr
        vld2.16   { q8, q9 }, [r1, :128], r3
        vld2.16   {q10, q11}, [r6, :128], r3

        sao_band_64b_16 q4,  q5,  q6,  q7, {d0-d3}, {d4-d7}, q14, q15, \bit_depth, \
            "pld       [r4]",                                                      \
            "it ne; addne r4, r3"
        sao_band_64b_16 q8,  q9, q10, q11, {d0-d3}, {d4-d7}, q14, q15, \bit_depth

        vst2.16   { q4, q5 }, [r0, :128], lr
        vst2.16   { q6, q7 }, [r5, :128], lr
        vst2.16   { q8, q9 }, [r0, :128], r2
        vst2.16   {q10, q11}, [r5, :128], r2

        bpl       1b

        vpop      {q4-q7}
        pop       {r4-r6, pc}
.endm

function ff_hevc_rpi_sao_band_c_32_neon_10, export=1
        band_c_32_16 10
endfunc


@ ff_hevc_rpi_sao_band_c_16_neon_10(
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

.macro band_c_16_16 bit_depth
        push      {r4-r6, lr}
        add       r5, r0, #32
        add       r6, r1, #32
        vmov.i64  q14, #0
        vmov.i16  q15, #(1 << \bit_depth) - 1
        bl        band_load_c

1:      vld2.16   { q8, q9 }, [r1, :128], r3
        subs      ip, #1
        vld2.16   {q10, q11}, [r6, :128], r3

        sao_band_64b_16 q4,  q5,  q6,  q7, {d0-d3}, {d4-d7}, q14, q15, \bit_depth
        sao_band_64b_16 q8,  q9, q10, q11, {d0-d3}, {d4-d7}, q14, q15, \bit_depth

        vst2.16   { q8, q9 }, [r0, :128], r2
        vst2.16   {q10, q11}, [r5, :128], r2

        bpl       1b
        pop       {r4-r6, pc}
.endm

function ff_hevc_rpi_sao_band_c_16_neon_10, export=1
        band_c_16_16 10
endfunc


@ ff_hevc_rpi_sao_band_c_8_neon_10(
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

.macro band_c_8_16 bit_depth
        ldr       ip, [sp, #16]         @ width
        push      {r4-r6, lr}
        vmov.i64  q14, #0
        cmp       ip, #8
        vmov.i16  q15, #(1 << \bit_depth) - 1
        bl        band_load_c
        blt       4f

        sao_band_32b_16 q8, q9, {d0-d3}, {d4-d7}, q14, q15, \bit_depth, \
            "vld2.16   {q8,q9}, [r1, :128], r3",                        \
            "subs      ip, #1",                                         \
            "",                                                         \
            "",                                                         \
            "",                                                         \
            "vst2.16   {q10,q11}, [r0, :128], r2"
        pop       {r4-r6, pc}
4:
        add       r5, r0, r2
        add       r6, r1, r3
        lsl       r2, #1
        lsl       r3, #1
        sao_band_32b_16 q8, q9, {d0-d3}, {d4-d7}, q14, q15, \bit_depth, \
            "vld2.16   {d16,d18}, [r1, :128], r3",                      \
            "subs      ip, #2",                                         \
            "vld2.16   {d17,d19}, [r6, :128], r3",                      \
            "",                                                         \
            "",                                                         \
            "vst2.16   {d20,d22}, [r0, :128], r2",                      \
            "vst2.16   {d21,d23}, [r5, :128], r2"
        pop       {r4-r6, pc}
.endm

function ff_hevc_rpi_sao_band_c_8_neon_10, export=1
        band_c_8_16 10
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
        vtbl.8   d7,  {d27}, d7
        vadd.s8  q14, q7, q15   @ Add -128 so we can use saturating signed add
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
        vadd.s16 q2, q2, q14
        vadd.s16 q3, q3, q15

        vmov.u8  q12, #2

        vmovn.s16 d0, q0
        vmovn.s16 d1, q1
        vmovn.s16 d2, q2
        vmovn.s16 d3, q3

        vldr     d26, [r5]

        vuzp.8   q0, q1

        vldr     d27, [r5, #8]

        vadd.s8  q0, q0, q12
        vadd.s8  q1, q1, q12

        vmov.i64 q12, #0

        vtbl.8   d0, {d26}, d0
        vtbl.8   d1, {d26}, d1
        vtbl.8   d2, {d27}, d2
        vtbl.8   d3, {d27}, d3

        vdup.i16 q13, r4

        vzip.8   q0, q1

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
        vcgt.u8  q9,  q0,  q1   @ a > c -> -1 , otherwise 0
        vadd.u8  q9,  q14, q9
        vcgt.u8  q0,  q1,  q0   @ c > a -> -1 , otherwise 0
        vsub.u8  q9,  q9,  q0
        vcgt.u8  q0,  q2,  q1   @ c < b -> -1 , otherwise 0
        vadd.u8  q9,  q9,  q0
        vcgt.u8  q0,  q1,  q2   @ c > b -> -1 , otherwise 0
        vsub.u8  q0,  q9,  q0

        vadd.s8  q3,  q1, q15   @ Add -128 so we can use saturating signed add

        vuzp.8   d0,  d1

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
        vcgt.u16 q9, q0, q1     @ a > c -> -1 , otherwise 0
        vadd.u16 q9, q14, q9
        vcgt.u16 q0, q1, q0     @ c > a -> -1 , otherwise 0
        vsub.u16 q9, q9, q0
        vcgt.u16 q0, q2, q1     @ c < b -> -1 , otherwise 0
        vadd.u16 q9, q9, q0
        vcgt.u16 q0, q1, q2     @ c > b -> -1 , otherwise 0
        vsub.u16 q0, q9, q0

        vmovn.s16 d0, q0
        @ d1 will have random contents that we transform but
        @ that doesn't matter as we then discard them
        vuzp.8   d0, d1

        vtbl.8   d0, {d16}, d0
        vtbl.8   d1, {d17}, d1

        vzip.8   d0, d1

        vaddw.s8 q0, q1, d0

        @ now clip
        vmax.s16 q0, q12
        vmin.s16 q0, q15
        bx       lr
endfunc


@ ff_hevc_rpi_sao_edge_[c_]xx_neon(
@   uint8_t *_dst,                    [r0]
@   const uint8_t *_src,              [r1]
@   ptrdiff_t stride_dst,             [r2]
@   const int16_t *_sao_offset_val_u, [r3]
@   const int16_t *_sao_offset_val_v, [sp, #0]   // Chroma only
@   int eo,                           [sp, #sp_base + 0]
@   int width,                        [sp, #sp_base + 4]
@   int height)                       [sp, #sp_base + 8]

@ Jumps via jump_tab with
@   uint8_t *_dst,                    [r0]
@   const uint8_t *_src,              [r1]
@   ptrdiff_t stride_dst,             [r2]
@   EDGE_SRC_STRIDE                   [r3]
@   (1 << \bit_depth) - 1             [r4]
@   * xlat_table                      [r5]  // setup_64b only
@   int height                        [r12]
@
@   0                                 [q12] // > 8 bit
@   2                                 [q14]
@   128                               [q15] // = 8 bit
@   r4                                [q15] // > 8 bit

.macro  edge_xxb_init, bit_depth, is_chroma, jump_tab, setup_64b = 0, setup_16b = 0, check_w4 = 0, do2 = 0, xjump = 0

@ Build translate registers
@ As translate values can only be 0-4 we don't care about junk in the rest
@ of the register
.if \is_chroma
        ldr      ip, [sp, #0]
        push     {r4-r6, lr}    @ 16 bytes
        vld1.8   {d16[2]}, [r3]
        add      r3, r3, #2
        vld1.8   {d17[2]}, [ip]
        add      ip, ip, #2
        vld1.8   {d16[0]}, [r3]
        add      r3, r3, #2
        vld1.8   {d17[0]}, [ip]
        add      ip, ip, #2
        vld1.8   {d16[1]}, [r3]
        add      r3, r3, #2
        vld1.8   {d17[1]}, [ip]
        add      ip, ip, #2
        vld1.8   {d16[3]}, [r3]
        add      r3, r3, #2
        vld1.8   {d17[3]}, [ip]
        add      ip, ip, #2
        vld1.8   {d16[4]}, [r3]
        vld1.8   {d17[4]}, [ip]
        movw     r3, EDGE_SRC_STRIDE
.set sp_base, 20
.else
        add      ip, r3, #4
        vld1.8   {d16[1]}, [r3]
        add      r3, r3, #2
        vld1.8   {d17[0]}, [ip]
        add      ip, ip, #2
        vld1.8   {d16[0]}, [r3]
        add      r3, r3, #6
        vld1.8   {d17[1]}, [ip]
        vld1.8   {d16[2]}, [r3]
        movw     r3, EDGE_SRC_STRIDE
        push     {r4-r6, lr}    @ 16 bytes
        vzip.8   d16, d17
        vmov     d17, d16
.set sp_base, 16
.endif

@ If setup_64b we need the xlat table on the stack
.if \setup_64b
        sub      r5, sp, #16
.endif

@ Get jump address
@ We have a special case for width 4 as the calling code doesn't detect it
@ If we may have w4 then we add a 2nd jump table after the 1st
.if \check_w4
        ldr      r12, [sp, #sp_base + 4]        @ width
        adr      r6, \jump_tab
        ldr      lr, [sp, #sp_base + 0]        @ e0
        cmp      r12, #8
        it lt
        addlt    r6, #16
.else
        ldr      lr, [sp, #sp_base + 0]        @ e0
        adr      r6, \jump_tab
.endif

        ldr      r12, [sp, #sp_base + 8]        @ height

.if \bit_depth > 8
        movw     r4, (1 << \bit_depth) - 1
.endif
.if \setup_16b
.if \bit_depth > 8
        vmov.i64 q12, #0
        vdup.16  q15, r4
        vmov.u16 q14, #2
.else
        vmov.u8  q15, #128
        vmov.u8  q14, #2
.endif
.endif

@ If setup_64b we need q4-q7 saved.
.if \setup_64b
        vpush    {q4-q8}        @ 80 bytes, q8 pushed first
.set sp_base, sp_base + 80
.endif

        ldr      r6, [r6, lr, lsl #2]

@ For 16 bit width 64 (or chroma 32) we need to do this in 2 passes
.if \do2
        push     {r0, r1, r6, r12}
.if jent_pic
        bl       98f
.else
        blx      r6
.endif
        pop      {r0, r1, r6, r12}

        add      r0, #64
        add      r1, #64
.endif

.if jent_pic
        bl       98f
.else
        blx      r6
.endif

@ Tidy up & return
.if \setup_64b
        vpop     {q4-q8}        @ spurious but harmless load of q8
.endif
        pop      {r4-r6, pc}

.if jent_pic && !\xjump
@ Magic label - used as 98b in jent macro
98:
        add      pc, r6
.endif
.endm


.macro  edge_16b_init, bit_depth, is_chroma, check_w4, jump_tab
        edge_xxb_init \bit_depth, \is_chroma, \jump_tab, check_w4=\check_w4, setup_16b=1
.endm

.macro  edge_64b_init, bit_depth, is_chroma, do2, jump_tab, xjump=0
        edge_xxb_init \bit_depth, \is_chroma, \jump_tab, do2=\do2, setup_64b=1, xjump=\xjump
.endm


.macro  edge_64b_e0, body_fn, pb
        sub      r1, #8
        mov      r6, lr
1:      vldm     r1, {d7-d16}
        // load a
        vext.8   q0,  q3,  q4, #(16 - \pb)
        add      r1, r3
        vext.8   q1,  q4,  q5, #(16 - \pb)
        subs     r12, #1
        vext.8   q2,  q5,  q6, #(16 - \pb)
        vext.8   q3,  q6,  q7, #(16 - \pb)
        pld      [r1]
        // load b
        vext.8   q11, q7,  q8, #\pb     @ Avoid overwrite
        pld      [r1, #64]
        vext.8   q8,  q4,  q5, #\pb
        vext.8   q9,  q5,  q6, #\pb
        vext.8   q10, q6,  q7, #\pb
        bl       \body_fn
        vstm     r0, {q0-q3}
        add      r0, r0, r2
        bgt      1b
        bx       r6
.endm

.macro  edge_32bx2_e0, body_fn, pb
        add      r6, r1, r3
        push     {r7,lr}
        sub      r1, #8
        add      r7, r0, r2
        lsl      r2, #1
1:      vldmia   r1, {d7-d12}
        // load a
        vext.8   q0, q3, q4, #16 - \pb
        add      r1, r1, r3, lsl #1
        vext.8   q1, q4, q5, #16 - \pb
        subs     r12, #2
        // load b
        vext.8   q8, q4, q5, #\pb
        vext.8   q9, q5, q6, #\pb
        vldr     d25, [r6, #-8]
        vldmia   r6, {d12-d15}
        vldr     d26, [r6, #32]
        // load a
        vext.8   q2, q12, q6, #16 - \pb
        add      r6, r6, r3, lsl #1
        vext.8   q3, q6, q7, #16 - \pb
        // load b
        vext.8   q10, q6, q7, #\pb
        vext.8   q11, q7, q13, #\pb
        bl       \body_fn
        vst1.8   {q0-q1}, [r0, :256], r2
        vst1.8   {q2-q3}, [r7, :256], r2
        bgt      1b
        pop      {r7,pc}
.endm

.macro  edge_16b_e0, body_fn, pb
        sub      r1, #8
        mov      r6, lr
1:      vldmia   r1, {d1-d4}
        add      r1, r3
        subs     r12, #1
        vext.8   q0, q0, q1, #16 - \pb
        vext.8   q2, q1, q2, #\pb

        bl       \body_fn
        vst1.8   {q0}, [r0, :128], r2
        bgt      1b
        bx       r6
.endm

.macro  edge_8bx2_e0, body_fn, pb
        add      r6, r1, r3
        push     {r7,lr}
        sub      r1, #8
        add      r7, r0, r2
        lsl      r2, #1
1:      vldmia   r1, {d1-d2}
        vldmia   r6, {d3-d4}
        vldr     d6, [r1, #16]
        subs     r12, #2
        vldr     d7, [r6, #-8]
        add      r1, r1, r3, lsl #1
        vext.8   d0, d1, d2, #8 - \pb
        add      r6, r6, r3, lsl #1
        vext.8   d5, d3, d4, #\pb
        vext.8   d4, d2, d6, #\pb
        vext.8   d1, d7, d3, #8 - \pb

        bl       \body_fn
        vst1.8   {d0}, [r0, :64], r2
        vst1.8   {d1}, [r7, :64], r2
        bgt      1b
        pop      {r7,pc}
.endm

.macro  edge_4bx4_e0, body_fn, pb
        add      r6, r1, r3
        push     {r7,lr}
        add      r7, r0, r2
        lsl      r2, #1

        tst      r1, #4
        bne      2f
1:      // r1 (and assumed r6) are 64-bit aligned
        vldr     d2, [r1]
        vldr     d0, [r1, #-8]
        add      r1, r1, r3, lsl #1
        vldr     d20, [r6]
        subs     r12, #4
        vldr     d18, [r6, #-8]
        add      r6, r6, r3, lsl #1
        vldr     d3, [r1]
        vshr.u64 d4, d2, #\pb * 8
        vldr     d1, [r1, #-8]
        add      r1, r1, r3, lsl #1
        vldr     d21, [r6]
        vext.8   d0, d0, d2, #8 - \pb
        vldr     d19, [r6,#-8]
        add      r6, r6, r3, lsl #1
        vshr.u64 d22, d20, #\pb * 8
        vext.8   d18, d18, d20, #8 - \pb
        vshr.u64 d5, d3, #\pb * 8
        vext.8   d1, d1, d3, #8 - \pb
        vshr.u64 d23, d21, #\pb * 8
        vext.8   d19, d19, d21, #8 - \pb
        vsli.64  q1, q10, #32
        vsli.64  q2, q11, #32
        vsli.64  q0, q9, #32

        bl       \body_fn
        vst1.32  {d0[0]}, [r0, :32], r2
        vst1.32  {d0[1]}, [r7, :32], r2
        vst1.32  {d1[0]}, [r0, :32], r2
        vst1.32  {d1[1]}, [r7, :32], r2
        bgt      1b
        pop      {r7,pc}

2:      // r1 (and assumed r6) are 32-bit but not 64-bit aligned
        vldr     d20, [r1, #-4]
        vldr     d22, [r1, #4]
        add      r1, r1, r3, lsl #1
        vldr     d2, [r6, #-4]
        subs     r12, #4
        vldr     d4, [r6, #4]
        add      r6, r6, r3, lsl #1
        vldr     d21, [r1, #-4]
        vshl.i64 d18, d20, #\pb * 8
        vldr     d23, [r1, #4]
        add      r1, r1, r3, lsl #1
        vldr     d3, [r6, #-4]
        vext.8   d22, d20, d22, #\pb
        vldr     d5, [r6, #4]
        add      r6, r6, r3, lsl #1
        vshl.i64 d0, d2, #\pb * 8
        vext.8   d4, d2, d4, #\pb
        vshl.i64 d19, d21, #\pb * 8
        vext.8   d23, d21, d23, #\pb
        vshl.i64 d1, d3, #\pb * 8
        vext.8   d5, d3, d5, #\pb
        vsri.64  q1, q10, #32
        vsri.64  q0, q9, #32
        vsri.64  q2, q11, #32

        bl       \body_fn
        vst1.32  {d0[0]}, [r0, :32], r2
        vst1.32  {d0[1]}, [r7, :32], r2
        vst1.32  {d1[0]}, [r0, :32], r2
        vst1.32  {d1[1]}, [r7, :32], r2
        bgt      2b
        pop      {r7,pc}
.endm


.macro  edge_64b_e1, body_fn
        sub      r1, r3
        push     {lr}
        add      r6, r1, #32
        // load a
        vld1.8   {q0-q1}, [r1, :256], r3
        vld1.8   {q2-q3}, [r6, :256], r3
        // load c
        vld1.8   {q4-q5}, [r1, :256], r3
        vld1.8   {q6-q7}, [r6, :256], r3
1:      // load b
        vld1.8   {q8-q9}, [r1, :256], r3
        subs     r12, #1
        vld1.8   {q10-q11}, [r6, :256], r3
        bl       \body_fn
        vstm     r0, {q0-q3}
        // copy c to a
        vmov.64  q0, q4
        pld      [r1, r3]
        vmov.64  q1, q5
        it       le
        pople    {lr}
        vmov.64  q2, q6
        it       le
        bxle     lr
        vmov.64  q3, q7
        add      r0, r0, r2
        // copy b to c
        vmov.64  q4, q8
        vmov.64  q5, q9
        vmov.64  q6, q10
        vmov.64  q7, q11
        b        1b
.endm

.macro  edge_32bx2_e1, body_fn
        sub      r6, r1, r3
        vld1.8   {q2-q3}, [r1, :256], r3
        vld1.8   {q0-q1}, [r6, :256]
        mov      r6, lr

1:      @ Given the data duplication here we could obviously do better than
        @ using the generic body_fn but it almost certainly isn't worth it
        vld1.8   {q8-q9}, [r1, :256], r3
        subs     r12, #2
        vmov     q4, q2
        vmov     q5, q3
        vld1.8   {q10-q11}, [r1, :256], r3
        vmov     q6, q8
        vmov     q7, q9

        bl       \body_fn

        vst1.8   {q0-q1}, [r0, :256], r2
        // copy b to a
        vmov     q0, q8
        vmov     q1, q9
        vst1.8   {q2-q3}, [r0, :256], r2
        vmov     q2, q10
        it       le
        bxle     r6
        vmov     q3, q11
        b        1b
.endm

.macro  edge_16b_e1, body_fn
        sub      r6, r1, r3
        // load c
        vld1.8   {q1}, [r1, :128], r3
        // load a
        vld1.8   {q0}, [r6, :128]
        mov      r6, lr
1:      // load b
        vld1.8   {q2}, [r1, :128], r3
        bl       \body_fn
        vst1.8   {q0}, [r0, :128], r2
        subs     r12, #1
        // copy c to a
        vmov.64  q0, q1
        it       le
        bxle     r6
        // copy b to c
        vmov.64  q1, q2
        b        1b
.endm

.macro  edge_8bx2_e1, body_fn
        sub      r6, r1, r3
        lsl      r3, #1
        push     {r7, lr}
        vld1.8   {d1}, [r1, :64], r3
        vld1.8   {d0}, [r6, :64], r3
        add      r7, r0, r2
        lsl      r2, #1
1:      @ Given the data duplication here we could obviously do better than
        @ using the generic body_fn but it almost certainly isn't worth it
        vld1.8   {d4}, [r6, :64], r3
        vmov     d2, d1
        vld1.8   {d5}, [r1, :64], r3
        subs     r12, #2
        vmov     d3, d4

        bl       \body_fn

        vst1.8   {d0}, [r0, :64], r2
        vst1.8   {d1}, [r7, :64], r2

        // copy b to a
        vmov     q0, q2
        bgt      1b
        pop      {r7, pc}
.endm

.macro  edge_4bx4_e1, body_fn
        sub      r6, r1, r3
        lsl      r3, #1
        push     {r7, lr}
        vld1.32  {d0[1]}, [r1, :32], r3
        add      r7, r0, r2
        vld1.32  {d0[0]}, [r6, :32], r3
        lsl      r2, #1
        vld1.32  {d4[1]}, [r1, :32], r3
        vld1.32  {d4[0]}, [r6, :32], r3
        vld1.32  {d5[1]}, [r1, :32], r3
        vld1.32  {d5[0]}, [r6, :32], r3
        vmov     d1, d4
        vext.32  d2, d0, d4, #1
        subs     r12, #4
        vmov     d22, d5
        vext.32  d3, d4, d5, #1
        b        2f

1:      vst1.32  {d0[0]}, [r0, :32], r2
        vext.32  d2, d22, d4, #1
        vst1.32  {d0[1]}, [r7, :32], r2
        vmov     d0, d22
        vst1.32  {d1[0]}, [r0, :32], r2
        vext.32  d3, d4, d5, #1
        vst1.32  {d1[1]}, [r7, :32], r2
        vmov     d1, d4
        vmov     d22, d5
2:      @ Given the data duplication here we could probably do better than
        @ using the generic body_fn but it almost certainly isn't worth it
        bl       \body_fn
        ble      3f
        vld1.32  {d4[0]}, [r6, :32], r3
        subs     r12, #4
        vld1.32  {d4[1]}, [r1, :32], r3
        vld1.32  {d5[0]}, [r6, :32], r3
        vld1.32  {d5[1]}, [r1, :32], r3
        b        1b

3:      vst1.32  {d0[0]}, [r0, :32], r2
        vst1.32  {d0[1]}, [r7, :32], r2
        vst1.32  {d1[0]}, [r0, :32]
        vst1.32  {d1[1]}, [r7, :32]
        pop      {r7, pc}
.endm

.macro  edge_64b_e2, body_fn, pb
        push     {lr}
        sub      r6, r1, r3
        // load c and a
        vld1.8   {q4-q5}, [r1, :128]
        vldr     d25, [r6, #-8]
        vldmia   r6, {d16-d23}
        vext.8   q0, q12, q8, #16 - \pb
        add      r6, r1, #32
        vext.8   q1, q8, q9, #16 - \pb
        add      r1, r1, r3
        vext.8   q2, q9, q10, #16 - \pb
        vld1.8   {q6-q7}, [r6, :128]
        sub      r6, r1, r3
        vext.8   q3, q10, q11, #16 - \pb

1:      // load b
        vldmia   r1, {d16-d24}
        vext.8   q8, q8, q9, #\pb
        pld      [r1, r3]
        vext.8   q9, q9, q10, #\pb
        subs     r12, #1
        vext.8   q10, q10, q11, #\pb
        vext.8   q11, q11, q12, #\pb
        bl       \body_fn
        // next a is mostly available in c
        vldr     d25, [r6, #-8]
        vstmia   r0, {q0-q3}
        vext.8   q3, q6, q7, #16 - \pb
        it       le
        pople    {lr}
        vext.8   q2, q5, q6, #16 - \pb
        it       le
        bxle     lr
        vext.8   q1, q4, q5, #16 - \pb
        add      r6, r6, r3
        vext.8   q0, q12, q4, #16 - \pb
        add      r0, r0, r2
        // next c is mostly available in b
        vldr     d8, [r1]
        vext.8   d9, d16, d17, #8 - \pb
        vext.8   q5, q8, q9, #16 - \pb
        add      r1, r1, r3
        vext.8   q6, q9, q10, #16 - \pb
        pld      [r6, #-8]
        vext.8   q7, q10, q11, #16 - \pb
        b        1b
.endm

.macro  edge_32bx2_e2, body_fn, pb
        sub      r6, r1, r3
        push     {r7, lr}
        add      r7, r0, r2
        lsl      r2, #1
        // load a and first 32b of c
        vld1.8   {q4-q5}, [r1, :256]
        vldr     d25, [r6, #-8]
        vld1.8   {q13-q14}, [r6, :256]
        vldr     d31, [r1, #-8]
        add      r6, r6, r3, lsl #1
        vext.8   q0, q12, q13, #16 - \pb
        add      r1, r1, r3, lsl #1
        vext.8   q1, q13, q14, #16 - \pb
        vext.8   q2, q15, q4, #16 - \pb
        vext.8   q3, q4, q5, #16 - \pb
1:
        // load second 32b of c and second 32b of b
        vldmia   r6, {d12-d16}
        vldmia   r1, {d20-d24}
        // first 32b of b is mostly available in second 32b of c
        vext.8   q9, q7, q8, #\pb
        subs     r12, #2
        vext.8   q8, q6, q7, #\pb
        vext.8   q10, q10, q11, #\pb
        vext.8   q11, q11, q12, #\pb

        bl       \body_fn

        vst1.8   {q0-q1}, [r0, :256], r2
        vst1.8   {q2-q3}, [r7, :256], r2
        ble      2f

        vldr     d25, [r6, #-8]
        add      r6, r6, r3, lsl #1
        vldr     d8, [r1]
        vext.8   d9, d20, d21, #8 - \pb
        vldr     d31, [r1, #-8]
        add      r1, r1, r3, lsl #1
        // first 32b of a is mostly available in second 32b of c
        vext.8   q1, q6, q7, #16 - \pb
        vext.8   q0, q12, q6, #16 - \pb
        // first 32b of c is mostly available in second 32b of b
        vext.8   q5, q10, q11, #16 - \pb
        // second 32b of a is mostly available in first 32b of c
        vext.8   q2, q15, q4, #16 - \pb
        vext.8   q3, q4, q5, #16 - \pb
        b        1b

2:      pop      {r7, pc}
.endm

.macro  edge_16b_e2, body_fn, pb
        push     {lr}
        sub      r6, r1, r3
        vld1.8   {q1}, [r1, :128], r3
        vldr     d19, [r6, #-8]
        vld1.8   {q10}, [r6, :128], r3

1:      vldmia   r1, {d4-d6}
        vext.8   q0, q9, q10, #16 - \pb
        subs     r12, #1
        vext.8   q2, q2, q3, #\pb
        bl       \body_fn
        vst1.8   {q0}, [r0, :128], r2
        ble      2f
        vmov     q10, q1
        vldr     d2, [r1]
        add      r1, r1, r3
        vldr     d19, [r6, #-8]
        add      r6, r6, r3
        vext.8   d3, d4, d5, #8 - \pb
        b        1b

2:      pop      {pc}
.endm

.macro  edge_8bx2_e2, body_fn, pb
        sub      r6, r1, r3
        push     {r7, lr}
        add      r7, r0, r2
        lsl      r2, #1
        vldr     d18, [r6, #-8]
        vldr     d19, [r6]
        add      r6, r6, r3, lsl #1
        vldr     d20, [r1, #-8]
        vldr     d2, [r1]
        add      r1, r1, r3, lsl #1
        vldmia   r6, {d3-d4}
        vld1.8   {d21-d22}, [r1, :128]

1:      vext.8   d0, d18, d19, #8 - \pb
        vext.8   d4, d3, d4, #\pb
        vext.8   d1, d20, d2, #8 - \pb
        subs     r12, #2
        vext.8   d5, d21, d22, #\pb

        bl       \body_fn

        vst1.8   {d0}, [r0, :64], r2
        vst1.8   {d1}, [r7, :64], r2
        ble      2f

        vldr     d18, [r6, #-8]
        add      r6, r6, r3, lsl #1
        vldr     d20, [r1, #-8]
        vmov     d19, d3
        vldr     d2, [r1]
        add      r1, r1, r3, lsl #1
        vldmia   r6, {d3-d4}
        vld1.8   {d21-d22}, [r1, :128]
        b        1b

2:      pop      {r7, pc}
.endm

.macro  edge_4bx4_e2, body_fn, pb
        sub      r6, r1, r3
        push     {r7-r9, lr}
        add      r8, r1, r3
        sub      r6, r6, #\pb
        add      r8, r8, #\pb
        add      r7, r0, r2
        lsl      r2, #1

1:      vld1.32  {d0[0]}, [r6], r3
        subs     r12, #4
        vld1.32  {d2[0]}, [r1], r3
        vld1.32  {d4[0]}, [r8], r3
        vld1.32  {d0[1]}, [r6], r3
        vld1.32  {d2[1]}, [r1], r3
        vld1.32  {d4[1]}, [r8], r3
        vld1.32  {d1[0]}, [r6], r3
        vld1.32  {d3[0]}, [r1], r3
        vld1.32  {d5[0]}, [r8], r3
        vld1.32  {d1[1]}, [r6], r3
        vld1.32  {d3[1]}, [r1], r3
        vld1.32  {d5[1]}, [r8], r3

        bl       \body_fn

        vst1.32  {d0[0]}, [r0, :32], r2
        vst1.32  {d0[1]}, [r7, :32], r2
        vst1.32  {d1[0]}, [r0, :32], r2
        vst1.32  {d1[1]}, [r7, :32], r2
        bgt      1b

        pop      {r7-r9,pc}
.endm

.macro  edge_64b_e3, body_fn, pb
        push     {lr}
        sub      r6, r1, r3
        // load c and a
        vld1.8   {q4-q5}, [r1, :128]
        vldmia   r6, {d16-d24}
        vext.8   q0, q8, q9, #\pb
        add      r6, r1, #32
        vext.8   q1, q9, q10, #\pb
        add      r1, r1, r3
        vext.8   q2, q10, q11, #\pb
        vld1.8   {q6-q7}, [r6, :128]
        sub      r6, r1, r3
        vext.8   q3, q11, q12, #\pb

1:      // load b
        vldr     d17, [r1, #-8]
        vldmia   r1, {d18-d25}
        vext.8   q8, q8, q9, #16 - \pb
        pld      [r1, r3]
        vext.8   q9, q9, q10, #16 - \pb
        subs     r12, #1
        vext.8   q10, q10, q11, #16 - \pb
        vext.8   q11, q11, q12, #16 - \pb
        bl       \body_fn
        // next a is mostly available in c
        vldr     d24, [r6, #64]
        vstmia   r0, {q0-q3}
        vext.8   q0, q4, q5, #\pb
        it       le
        pople    {lr}
        vext.8   q1, q5, q6, #\pb
        it       le
        bxle     lr
        vext.8   q2, q6, q7, #\pb
        add      r6, r6, r3
        vext.8   q3, q7, q12, #\pb
        add      r0, r0, r2
        // next c is mostly available in b
        vext.8   d14, d22, d23, #\pb
        vldr     d15, [r1, #56]
        vext.8   q4, q8, q9, #\pb
        add      r1, r1, r3
        vext.8   q5, q9, q10, #\pb
        vext.8   q6, q10, q11, #\pb
        b        1b
.endm

.macro  edge_32bx2_e3, body_fn, pb
        sub      r6, r1, r3
        push     {r7, lr}
        add      r7, r0, r2
        lsl      r2, #1
        // load a and first 32b of c
        vldmia   r1, {d8-d12}
        vldmia   r6, {d24-d28}
        vext.8   q2, q4, q5, #\pb
        add      r6, r6, r3, lsl #1
        vext.8   q3, q5, q6, #\pb
        add      r1, r1, r3, lsl #1
        vext.8   q0, q12, q13, #\pb
        vext.8   q1, q13, q14, #\pb
1:
        // load second 32b of c and second 32b of b
        vldr     d25, [r6, #-8]
        subs     r12, #2
        vldmia   r6, {d12-d15}
        vldr     d27, [r1, #-8]
        vldmia   r1, {d20-d23}
        // first 32b of b is mostly available in second 32b of c
        vext.8   q8, q12, q6, #16 - \pb
        vext.8   q9, q6, q7, #16 - \pb
        vext.8   q11, q10, q11, #16 - \pb
        vext.8   q10, q13, q10, #16 - \pb

        bl       \body_fn

        vst1.8   {q0-q1}, [r0, :256], r2
        vst1.8   {q2-q3}, [r7, :256], r2
        ble      2f

        vldr     d24, [r6, #32]
        add      r6, r6, r3, lsl #1
        vldr     d11, [r1, #24]
        vext.8   d10, d22, d23, #\pb
        vldr     d30, [r1, #32]
        add      r1, r1, r3, lsl #1
        // first 32b of a is mostly available in second 32b of c
        vext.8   q0, q6, q7, #\pb
        vext.8   q1, q7, q12, #\pb
        // first 32b of c is mostly available in second 32b of b
        vext.8   q4, q10, q11, #\pb
        // second 32b of a is mostly available in first 32b of c
        vext.8   q3, q5, q15, #\pb
        vext.8   q2, q4, q5, #\pb
        b        1b

2:      pop      {r7, pc}
.endm

.macro  edge_16b_e3, body_fn, pb
        push     {lr}
        sub      r6, r1, r3
        vld1.8   {q1}, [r1, :128], r3
        vldmia   r6, {d18-d20}
        add      r6, r6, r3

1:      vldr     d5, [r1, #-8]
        vld1.8   {q3}, [r1, :128]
        subs     r12, #1
        vext.8   q0, q9, q10, #\pb
        vext.8   q2, q2, q3, #16 - \pb
        bl       \body_fn
        vst1.8   {q0}, [r0, :128], r2
        ble      2f
        vmov     q9, q1
        vldr     d3, [r1, #8]
        add      r1, r1, r3
        vldr     d20, [r6, #16]
        add      r6, r6, r3
        vext.8   d2, d4, d5, #\pb
        b        1b

2:      pop      {pc}
.endm

.macro  edge_8bx2_e3, body_fn, pb
        sub      r6, r1, r3
        push     {r7, lr}
        add      r7, r0, r2
        lsl      r2, #1
        vld1.8   {d18-d19}, [r6]
        add      r6, r6, r3, lsl #1
        vldr     d20, [r1, #8]
        vldr     d2, [r1]
        add      r1, r1, r3, lsl #1
        vldr     d4, [r6, #-8]
        vldr     d3, [r6]
        vldr     d21, [r1, #-8]
        vldr     d22, [r1]

1:      vext.8   d0, d18, d19, #\pb
        vext.8   d4, d4, d3, #8 - \pb
        vext.8   d1, d2, d20, #\pb
        subs     r12, #2
        vext.8   d5, d21, d22, #8 - \pb

        bl       \body_fn

        vst1.8   {d0}, [r0, :64], r2
        vst1.8   {d1}, [r7, :64], r2
        ble      2f

        vldr     d19, [r6, #8]
        add      r6, r6, r3, lsl #1
        vldr     d20, [r1, #8]
        vmov     d18, d3
        vldr     d2, [r1]
        add      r1, r1, r3, lsl #1
        vldr     d4, [r6, #-8]
        vldr     d3, [r6]
        vldr     d21, [r1, #-8]
        vldr     d22, [r1]
        b        1b

2:      pop      {r7, pc}
.endm

.macro  edge_4bx4_e3, body_fn, pb
        @ e3 is the same as e2 but with the X offset reversed
        edge_4bx4_e2 \body_fn, (-\pb)
.endm

@ Jump table entry - if in neon mode the bottom bit must be set
@ ? There is probably a real asm instruction to do this but I haven't found it
.macro jent lab
.if jent_pic
@ Could use .short here but due to A32 not supporting ldrh [lsl#1] it is
@ simpler and clearer in the code to stick with .word
T       .word  (0 + \lab) - (4 + 98b)
A       .word  (0 + \lab) - (8 + 98b)
.else
T       .word   1 + \lab
A       .word   \lab
.endif
.endm

.macro edge_64b_bodies, body_fn, pb
        jent    0f
        jent    10f
        jent    20f
        jent    30f

0:      edge_64b_e0     \body_fn, \pb
10:     edge_64b_e1     \body_fn
20:     edge_64b_e2     \body_fn, \pb
30:     edge_64b_e3     \body_fn, \pb
.endm

.macro edge_32bx2_bodies, body_fn, pb
        jent    0f
        jent    10f
        jent    20f
        jent    30f

0:      edge_32bx2_e0   \body_fn, \pb
10:     edge_32bx2_e1   \body_fn
20:     edge_32bx2_e2   \body_fn, \pb
30:     edge_32bx2_e3   \body_fn, \pb
.endm

.macro edge_16b_bodies, body_fn, pb
        jent    0f
        jent    10f
        jent    20f
        jent    30f

0:      edge_16b_e0     \body_fn, \pb
10:     edge_16b_e1     \body_fn
20:     edge_16b_e2     \body_fn, \pb
30:     edge_16b_e3     \body_fn, \pb
.endm

.macro edge_32bx2_16b_bodies, body_fn_64b, body_fn_16b, pb
        jent    0f
        jent    10f
        jent    20f
        jent    30f
        jent    5f
        jent    15f
        jent    25f
        jent    35f

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
        jent    0f
        jent    10f
        jent    20f
        jent    30f
        jent    5f
        jent    15f
        jent    25f
        jent    35f

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
        jent    0f
        jent    10f
        jent    20f
        jent    30f
        jent    5f
        jent    15f
        jent    25f
        jent    35f

0:      edge_8bx2_e0    \body_fn, \pb
10:     edge_8bx2_e1    \body_fn
20:     edge_8bx2_e2    \body_fn, \pb
30:     edge_8bx2_e3    \body_fn, \pb
5:      edge_4bx4_e0    \body_fn, \pb
15:     edge_4bx4_e1    \body_fn
25:     edge_4bx4_e2    \body_fn, \pb
35:     edge_4bx4_e3    \body_fn, \pb
.endm

@ void ff_hevc_rpi_sao_edge_8_neon_8(
@   uint8_t *_dst,            [r0]
@   uint8_t *_src,            [r1]
@   int  stride_dst,          [r2]
@   int16_t *_sao_offset_val, [r3]
@   int eo,                   [sp, #0]
@   int width,                [sp, #4]
@   int height)               [sp, #8]

function ff_hevc_rpi_sao_edge_8_neon_8, export=1
        edge_16b_init   8, 0, 1, 99f
99:
        edge_8bx2_4bx4_bodies edge_16b_body_8, 1
endfunc

@ void ff_hevc_rpi_sao_edge_16_neon_8(
@   uint8_t *_dst,            [r0]
@   uint8_t *_src,            [r1]
@   int  stride_dst,          [r2]
@   int16_t *_sao_offset_val, [r3]
@   int eo,                   [sp, #0]
@   int width,                [sp, #4]
@   int height)               [sp, #8]

function ff_hevc_rpi_sao_edge_16_neon_8, export=1
        edge_16b_init   8, 0, 0, 99f
99:
        edge_16b_bodies edge_16b_body_8, 1
endfunc

@ void ff_hevc_rpi_sao_edge_32_neon_8(
@   uint8_t *_dst,            [r0]
@   uint8_t *_src,            [r1]
@   int  stride_dst,          [r2]
@   int16_t *_sao_offset_val, [r3]
@   int eo,                   [sp, #0]
@   int width,                [sp, #4]
@   int height)               [sp, #8]

function ff_hevc_rpi_sao_edge_32_neon_8, export=1
        edge_64b_init   8, 0, 0, 99f
99:
        edge_32bx2_bodies edge_64b_body_8, 1
endfunc

@ void ff_hevc_rpi_sao_edge_64_neon_8(
@   uint8_t *_dst,            [r0]
@   uint8_t *_src,            [r1]
@   int  stride_dst,          [r2]
@   int16_t *_sao_offset_val, [r3]
@   int eo,                   [sp, #0]
@   int width,                [sp, #4]
@   int height)               [sp, #8]

function ff_hevc_rpi_sao_edge_64_neon_8, export=1
        edge_64b_init   8, 0, 0, 99f
99:
        edge_64b_bodies edge_64b_body_8, 1
endfunc

@ ff_hevc_rpi_sao_edge_c_8_neon_8(
@   uint8_t *_dst,                    [r0]
@   const uint8_t *_src,              [r1]
@   ptrdiff_t stride_dst,             [r2]
@   const int16_t *_sao_offset_val_u, [r3]
@   const int16_t *_sao_offset_val_v, [sp, #0]
@   int eo,                           [sp, #4]
@   int width,                        [sp, #8]
@   int height)                       [sp, #12]

function ff_hevc_rpi_sao_edge_c_8_neon_8, export=1
        edge_16b_init   8, 1, 1, 99f
99:
        edge_16b_8bx2_bodies edge_16b_body_8, 2
endfunc

@ ff_hevc_rpi_sao_edge_c_16_neon_8(
@   uint8_t *_dst,                    [r0]
@   const uint8_t *_src,              [r1]
@   ptrdiff_t stride_dst,             [r2]
@   const int16_t *_sao_offset_val_u, [r3]
@   const int16_t *_sao_offset_val_v, [sp, #0]
@   int eo,                           [sp, #4]
@   int width,                        [sp, #8]
@   int height)                       [sp, #12]

function ff_hevc_rpi_sao_edge_c_16_neon_8, export=1
        edge_64b_init   8, 1, 0, 99f
99:
        edge_32bx2_bodies edge_64b_body_8, 2
endfunc

@ ff_hevc_rpi_sao_edge_c_32_neon_8(
@   uint8_t *_dst,                    [r0]
@   const uint8_t *_src,              [r1]
@   ptrdiff_t stride_dst,             [r2]
@   const int16_t *_sao_offset_val_u, [r3]
@   const int16_t *_sao_offset_val_v, [sp, #0]
@   int eo,                           [sp, #4]
@   int width,                        [sp, #8]
@   int height)                       [sp, #12]

function ff_hevc_rpi_sao_edge_c_32_neon_8, export=1
        edge_64b_init   8, 1, 0, 99f
99:
        edge_64b_bodies edge_64b_body_8, 2
endfunc

@ void ff_hevc_rpi_sao_edge_8_neon_10(
@   uint8_t *_dst,            [r0]
@   uint8_t *_src,            [r1]
@   int  stride_dst,          [r2]
@   int16_t *_sao_offset_val, [r3]
@   int eo,                   [sp, #0]
@   int width,                [sp, #4]
@   int height)               [sp, #8]

function ff_hevc_rpi_sao_edge_8_neon_10, export=1
        edge_16b_init   10, 0, 1, 99f
99:
        edge_16b_8bx2_bodies edge_16b_body_16, 2
endfunc

@ void ff_hevc_rpi_sao_edge_16_neon_10(
@   uint8_t *_dst,            [r0]
@   uint8_t *_src,            [r1]
@   int  stride_dst,          [r2]
@   int16_t *_sao_offset_val, [r3]
@   int eo,                   [sp, #0]
@   int width,                [sp, #4]
@   int height)               [sp, #8]

function ff_hevc_rpi_sao_edge_16_neon_10, export=1
        edge_64b_init   10, 0, 0, 99f
99:
        edge_32bx2_bodies edge_64b_body_16, 2
endfunc

@ void ff_hevc_rpi_sao_edge_64_neon_10(
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

function ff_hevc_rpi_sao_edge_64_neon_10, export=1
        edge_64b_init   10, 0, 1, 99f, xjump=1
endfunc

@ void ff_hevc_rpi_sao_edge_32_neon_10(
@   uint8_t *_dst,            [r0]
@   uint8_t *_src,            [r1]
@   int  stride_dst,          [r2]
@   int16_t *_sao_offset_val, [r3]
@   int eo,                   [sp, #0]
@   int width,                [sp, #4]
@   int height)               [sp, #8]

function ff_hevc_rpi_sao_edge_32_neon_10, export=1
        edge_64b_init   10, 0, 0, 99f
99:
        edge_64b_bodies edge_64b_body_16, 2
endfunc

@ ff_hevc_rpi_sao_edge_c_8_neon_10(
@   uint8_t *_dst,                    [r0]
@   const uint8_t *_src,              [r1]
@   ptrdiff_t stride_dst,             [r2]
@   const int16_t *_sao_offset_val_u, [r3]
@   const int16_t *_sao_offset_val_v, [sp, #0]
@   int eo,                           [sp, #4]
@   int width,                        [sp, #8]
@   int height)                       [sp, #12]

function ff_hevc_rpi_sao_edge_c_8_neon_10, export=1
        edge_xxb_init   10, 1, 99f, check_w4=1, setup_16b=1, setup_64b=1
99:
        edge_32bx2_16b_bodies edge_64b_body_16, edge_16b_body_16, 4
endfunc

@ ff_hevc_rpi_sao_edge_c_32_neon_10(
@   uint8_t *_dst,                    [r0]
@   const uint8_t *_src,              [r1]
@   ptrdiff_t stride_dst,             [r2]
@   const int16_t *_sao_offset_val_u, [r3]
@   const int16_t *_sao_offset_val_v, [sp, #0]
@   int eo,                           [sp, #4]
@   int width,                        [sp, #8]
@   int height)                       [sp, #12]

function ff_hevc_rpi_sao_edge_c_32_neon_10, export=1
        edge_64b_init   10, 1, 1, 99f, xjump=1
endfunc


@ ff_hevc_rpi_sao_edge_c_16_neon_10(
@   uint8_t *_dst,                    [r0]
@   const uint8_t *_src,              [r1]
@   ptrdiff_t stride_dst,             [r2]
@   const int16_t *_sao_offset_val_u, [r3]
@   const int16_t *_sao_offset_val_v, [sp, #0]
@   int eo,                           [sp, #4]
@   int width,                        [sp, #8]
@   int height)                       [sp, #12]

function ff_hevc_rpi_sao_edge_c_16_neon_10, export=1
        edge_64b_init   10, 1, 0, 99f
99:
        edge_64b_bodies edge_64b_body_16, 4
endfunc

