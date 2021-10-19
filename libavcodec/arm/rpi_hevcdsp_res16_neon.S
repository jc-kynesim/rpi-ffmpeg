/*
Copyright (c) 2017 Raspberry Pi (Trading) Ltd.
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

 .arch_extension mp @ enable PLDW

#define BIT_DEPTH 10

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

@ add_residual4x4(
@  uint16_t *_dst,    [r0]
@  int16_t *res,      [r1]
@  ptrdiff_t stride)  [r2]

function JOIN(ff_hevc_rpi_add_residual_4x4_neon_, BIT_DEPTH), export=1
        add         ip, r0, r2
        vld1.16     {q10, q11}, [r1]
        lsl         r2, #1
        vld1.16     {d0}, [r0 :64], r2
        vld1.16     {d1}, [ip :64], r2
        vld1.16     {d2}, [r0 :64]
        vld1.16     {d3}, [ip :64]
        sub         r0, r2
        vqadd.s16   q0,  q10
        sub         ip, r2
        vqadd.s16   q1,  q11
        vmov.i16    q8,  #0
        vmov.i16    q9,  #(1 << BIT_DEPTH) - 1
        vmax.s16    q0,  q0,  q8
        vmax.s16    q1,  q1,  q8
        vmin.s16    q0,  q0,  q9
        vmin.s16    q1,  q1,  q9
        vst1.16     {d0}, [r0 :64], r2
        vst1.16     {d1}, [ip :64], r2
        vst1.16     {d2}, [r0 :64]
        vst1.16     {d3}, [ip :64]
        bx          lr

endfunc

@ add_residual4x4_dc(
@  uint16_t *_dst,    [r0]
@  ptrdiff_t stride,  [r1]
@  int dc)            [r2]

function JOIN(ff_hevc_rpi_add_residual_4x4_dc_neon_, BIT_DEPTH), export=1
        add         ip, r0, r1
        vdup.16     q15, r2
        lsl         r1, #1
        vld1.16     {d0}, [r0 :64], r1
        vld1.16     {d1}, [ip :64], r1
        vld1.16     {d2}, [r0 :64]
        vld1.16     {d3}, [ip :64]
        sub         r0, r1
        vqadd.s16   q0,  q15
        sub         ip, r1
        vqadd.s16   q1,  q15
        vmov.i16    q8,  #0
        vmov.i16    q9,  #(1 << BIT_DEPTH) - 1
        vmax.s16    q0,  q0,  q8
        vmax.s16    q1,  q1,  q8
        vmin.s16    q0,  q0,  q9
        vmin.s16    q1,  q1,  q9
        vst1.16     {d0}, [r0 :64], r1
        vst1.16     {d1}, [ip :64], r1
        vst1.16     {d2}, [r0 :64]
        vst1.16     {d3}, [ip :64]
        bx          lr

endfunc


@ add_residual8x8(
@  uint16_t *_dst,    [r0]
@  int16_t *res,      [r1]
@  ptrdiff_t stride)  [r2]

function JOIN(ff_hevc_rpi_add_residual_8x8_neon_, BIT_DEPTH), export=1
        mov         r3, #8
        vmov.i64    q8,  #0
        add         ip, r0, r2
        vmov.i16    q9,  #(1 << BIT_DEPTH) - 1
        lsl         r2, #1
1:
        vldm        r1!, {q10-q13}
        vld1.16     {q0}, [r0 :128], r2
        vld1.16     {q1}, [ip :128], r2
        vld1.16     {q2}, [r0 :128]
        vld1.16     {q3}, [ip :128]
        sub         r0, r2
        vqadd.s16   q0,  q10
        sub         ip, r2
        vqadd.s16   q1,  q11
        subs        r3, #4
        vqadd.s16   q2,  q12
        vqadd.s16   q3,  q13
        clip16_4    q0, q1, q2, q3, q8, q9
        vst1.16     {q0}, [r0 :128], r2
        vst1.16     {q1}, [ip :128], r2
        vst1.16     {q2}, [r0 :128], r2
        vst1.16     {q3}, [ip :128], r2
        bne         1b
        bx          lr

endfunc

@ add_residual4x4_dc_c(
@  uint16_t *_dst,    [r0]
@  ptrdiff_t stride,  [r1]
@  int dc_uv)         [r2]

function JOIN(ff_hevc_rpi_add_residual_4x4_dc_c_neon_, BIT_DEPTH), export=1
        mov         r3, #4
        vdup.32     q15, r2
        b           9f
endfunc

@ add_residual8x8_dc(
@  uint16_t *_dst,    [r0]
@  ptrdiff_t stride,  [r1]
@  int dc)            [r2]

function JOIN(ff_hevc_rpi_add_residual_8x8_dc_neon_, BIT_DEPTH), export=1
        vdup.16     q15, r2
        mov         r3, #8
9:
        vmov.i16    q8,  #0
        add         ip, r0, r1
        vmov.i16    q9,  #(1 << BIT_DEPTH) - 1
        lsl         r1, #1
1:
        vld1.16     {q0}, [r0 :128], r1
        vld1.16     {q1}, [ip :128], r1
        vld1.16     {q2}, [r0 :128]
        vld1.16     {q3}, [ip :128]
        sub         r0, r1
        vqadd.s16   q0,  q15
        sub         ip, r1
        vqadd.s16   q1,  q15
        subs        r3, #4
        vqadd.s16   q2,  q15
        vqadd.s16   q3,  q15
        clip16_4    q0, q1, q2, q3, q8, q9
        vst1.16     {q0}, [r0 :128], r1
        vst1.16     {q1}, [ip :128], r1
        vst1.16     {q2}, [r0 :128], r1
        vst1.16     {q3}, [ip :128], r1
        bne         1b
        bx          lr

endfunc

@ add_residual16x16(
@  uint16_t *_dst,    [r0]
@  int16_t *res,      [r1]
@  ptrdiff_t stride)  [r2]

function JOIN(ff_hevc_rpi_add_residual_16x16_neon_, BIT_DEPTH), export=1
        add         ip, r0, r2
        vmov.i16    q8,  #0
        lsl         r2, #1
        vmov.i16    q9,  #(1 << BIT_DEPTH) - 1
        mov         r3, #16
1:
        vldm        r1!, {q10-q13}
        @ For RPI Sand we could guarantee :256 but not for general
        @ non-RPI allocation. :128 is as good as we can claim
        vld1.16     {q0, q1}, [r0 :128]
        subs        r3, #2
        vld1.16     {q2, q3}, [ip :128]
        vqadd.s16   q0,  q10
        vqadd.s16   q1,  q11
        vqadd.s16   q2,  q12
        vqadd.s16   q3,  q13
        clip16_4    q0, q1, q2, q3, q8, q9
        vst1.16     {q0, q1}, [r0 :128], r2
        vst1.16     {q2, q3}, [ip :128], r2
        bne         1b
        bx          lr
endfunc

@ add_residual8x8_dc_c(
@  uint16_t *_dst,    [r0]
@  ptrdiff_t stride,  [r1]
@  int dc_uv)         [r2]

function JOIN(ff_hevc_rpi_add_residual_8x8_dc_c_neon_, BIT_DEPTH), export=1
        mov         r3, #8
        vdup.32     q15, r2
        b           9f
endfunc

@ add_residual16x16_dc(
@  uint16_t *_dst,    [r0]
@  ptrdiff_t stride,  [r1]
@  int dc)            [r2]

function JOIN(ff_hevc_rpi_add_residual_16x16_dc_neon_, BIT_DEPTH), export=1
        vdup.i16    q15, r2
        mov         r3, #16
9:
        vmov.i16    q8,  #0
        add         ip, r0, r1
        vmov.i16    q9,  #(1 << BIT_DEPTH) - 1
        lsl         r1, #1
1:
        @ For RPI Sand we could guarantee :256 but not for general
        @ non-RPI allocation. :128 is as good as we can claim
        vld1.16     {q0, q1}, [r0 :128]
        subs        r3, #2
        vqadd.s16   q0,  q15
        vqadd.s16   q1,  q15
        vld1.16     {q2, q3}, [ip :128]
        vqadd.s16   q2,  q15
        vqadd.s16   q3,  q15
        clip16_4    q0, q1, q2, q3, q8, q9
        vst1.16     {q0, q1}, [r0 :128], r1
        vst1.16     {q2, q3}, [ip :128], r1
        bne         1b
        bx          lr

endfunc


@ add_residual32x32(
@  uint16_t *_dst,    [r0]
@  int16_t *res,      [r1]
@  ptrdiff_t stride)  [r2]

function JOIN(ff_hevc_rpi_add_residual_32x32_neon_, BIT_DEPTH), export=1
        push        {lr}
        mov         r3, #32
        vmov.i16    q8,  #0
        add         lr, r0, r2
        vmov.i16    q9,  #(1 << BIT_DEPTH) - 1
        add         ip, r0, #32
1:
        vldm        r1!, {q10-q13}
        vldm        r0,  {q0-q3}
        vqadd.s16   q0,  q10
          pldw        [lr]
        vqadd.s16   q1,  q11
          add         lr, r2
        vqadd.s16   q2,  q12
        subs        r3, #1
        vqadd.s16   q3,  q13
        clip16_4    q0, q1, q2, q3, q8, q9
        vst1.16     {q0-q1}, [r0], r2
        vst1.16     {q2-q3}, [ip], r2
        bne         1b
        pop         {pc}

endfunc

@ add_residual16x16_dc_c(
@  uint16_t *_dst,    [r0]
@  ptrdiff_t stride,  [r1]
@  int dc_uv)         [r2]

function JOIN(ff_hevc_rpi_add_residual_16x16_dc_c_neon_, BIT_DEPTH), export=1
        mov         r3, #16
        vdup.32     q15, r2
        b           9f
endfunc

@ add_residual32x32_dc(
@  uint16_t *_dst,    [r0]
@  ptrdiff_t stride,  [r1]
@  int dc)            [r2]

function JOIN(ff_hevc_rpi_add_residual_32x32_dc_neon_, BIT_DEPTH), export=1
        vdup.16     q15, r2
        mov         r3, #32
9:
        vmov.i16    q8,  #0
        vmov.i16    q9,  #(1 << BIT_DEPTH) - 1
        add         ip, r0, #32
1:
        vldm        r0,  {q0-q3}
        vqadd.s16   q0,  q15
        subs        r3, #1
        vqadd.s16   q1,  q15
        vqadd.s16   q2,  q15
        vqadd.s16   q3,  q15
        clip16_4    q0, q1, q2, q3, q8, q9
        vst1.16     {q0-q1}, [r0], r1
        vst1.16     {q2-q3}, [ip], r1
        bne         1b
        bx          lr

endfunc

@ ============================================================================
@ U add

@ add_residual4x4_u(
@   uint16_t *_dst,       [r0]
@   const int16_t *res,   [r1]
@   ptrdiff_t stride,     [r2]
@   int dc)               [r3]

function JOIN(ff_hevc_rpi_add_residual_4x4_u_neon_, BIT_DEPTH), export=1
        vdup.16     q15, r3
        add         ip, r0, r2
        vld1.16     {q10, q11}, [r1 :256]
        lsl         r2, #1
        vld2.16     {d0, d2}, [r0 :128], r2
        vld2.16     {d1, d3}, [ip :128], r2
        vld2.16     {d4, d6}, [r0 :128]
        vld2.16     {d5, d7}, [ip :128]
        sub         r0, r2
        vmov.i16    q8,  #0
        sub         ip, r2
        vmov.i16    q9,  #(1 << BIT_DEPTH) - 1

        vqadd.s16   q0,  q10
        vqadd.s16   q1,  q15
        vqadd.s16   q2,  q11
        vqadd.s16   q3,  q15
        clip16_4    q0, q1, q2, q3, q8, q9

        vst2.16     {d0, d2}, [r0 :128], r2
        vst2.16     {d1, d3}, [ip :128], r2
        vst2.16     {d4, d6}, [r0 :128]
        vst2.16     {d5, d7}, [ip :128]
        bx          lr
endfunc

@ add_residual8x8_u(
@   uint16_t *_dst,       [r0]
@   const int16_t *res,   [r1]
@   ptrdiff_t stride,     [r2]
@   int dc)               [r3]

function JOIN(ff_hevc_rpi_add_residual_8x8_u_neon_, BIT_DEPTH), export=1
        vdup.16     q15, r3
        mov         r3, #8
        vmov.i16    q8,  #0
        add         ip, r0, r2
        vmov.i16    q9,  #(1 << BIT_DEPTH) - 1
        lsl         r2, #1
1:
        vld2.16     {q0, q1}, [r0 :256]
        subs        r3, #2
        vld2.16     {q2, q3}, [ip :256]
        vld1.16     {q10, q11}, [r1 :256]!
        vqadd.s16   q0,  q10
        vqadd.s16   q1,  q15
        vqadd.s16   q2,  q11
        vqadd.s16   q3,  q15
        clip16_4    q0, q1, q2, q3, q8, q9
        vst2.16     {q0, q1}, [r0 :256], r2
        vst2.16     {q2, q3}, [ip :256], r2
        bne         1b
        bx          lr
endfunc

@ add_residual16x16_u(
@   uint16_t *_dst,       [r0]
@   const int16_t *res,   [r1]
@   ptrdiff_t stride,     [r2]
@   int dc)               [r3]

function JOIN(ff_hevc_rpi_add_residual_16x16_u_neon_, BIT_DEPTH), export=1
        push        {lr}
        vdup.16     q15, r3
        mov         r3, #16
        vmov.i16    q8,  #0
        add         lr, r0, r2
        vmov.i16    q9,  #(1 << BIT_DEPTH) - 1
        add         ip, r0, #32
1:
        vld2.16     {q0, q1}, [r0 :256]
        vld2.16     {q2, q3}, [ip :256]
        vld1.16     {q10, q11}, [r1 :256]!
        vqadd.s16   q0,  q10
          pldw        [lr]
        vqadd.s16   q1,  q15
          add         lr, r2
        vqadd.s16   q2,  q11
        subs        r3, #1
        vqadd.s16   q3,  q15
        clip16_4    q0, q1, q2, q3, q8, q9
        vst2.16     {q0, q1}, [r0 :256], r2
        vst2.16     {q2, q3}, [ip :256], r2
        bne         1b
        pop         {pc}
endfunc

@ ============================================================================
@ V add

@ add_residual4x4_v(
@   uint16_t *_dst,       [r0]
@   const int16_t *res,   [r1]
@   ptrdiff_t stride,     [r2]
@   int dc)               [r3]

function JOIN(ff_hevc_rpi_add_residual_4x4_v_neon_, BIT_DEPTH), export=1
        vdup.16     q15, r3
        add         ip, r0, r2
        vld1.16     {q10, q11}, [r1 :256]
        lsl         r2, #1
        vld2.16     {d0, d2}, [r0 :128], r2
        vld2.16     {d1, d3}, [ip :128], r2
        vld2.16     {d4, d6}, [r0 :128]
        vld2.16     {d5, d7}, [ip :128]
        sub         r0, r2
        vmov.i16    q8,  #0
        sub         ip, r2
        vmov.i16    q9,  #(1 << BIT_DEPTH) - 1

        vqadd.s16   q0,  q15
        vqadd.s16   q1,  q10
        vqadd.s16   q2,  q15
        vqadd.s16   q3,  q11
        clip16_4    q0, q1, q2, q3, q8, q9

        vst2.16     {d0, d2}, [r0 :128], r2
        vst2.16     {d1, d3}, [ip :128], r2
        vst2.16     {d4, d6}, [r0 :128]
        vst2.16     {d5, d7}, [ip :128]
        bx          lr
endfunc

@ add_residual8x8_v(
@   uint16_t *_dst,       [r0]
@   const int16_t *res,   [r1]
@   ptrdiff_t stride,     [r2]
@   int dc)               [r3]

function JOIN(ff_hevc_rpi_add_residual_8x8_v_neon_, BIT_DEPTH), export=1
        vdup.16     q15, r3
        mov         r3, #8
        vmov.i16    q8,  #0
        add         ip, r0, r2
        vmov.i16    q9,  #(1 << BIT_DEPTH) - 1
        lsl         r2, #1
1:
        vld2.16     {q0, q1}, [r0 :256]
        subs        r3, #2
        vld2.16     {q2, q3}, [ip :256]
        vld1.16     {q10, q11}, [r1 :256]!
        vqadd.s16   q0,  q15
        vqadd.s16   q1,  q10
        vqadd.s16   q2,  q15
        vqadd.s16   q3,  q11
        clip16_4    q0, q1, q2, q3, q8, q9
        vst2.16     {q0, q1}, [r0 :256], r2
        vst2.16     {q2, q3}, [ip :256], r2
        bne         1b
        bx          lr
endfunc

@ add_residual16x16_v(
@   uint16_t *_dst,       [r0]
@   const int16_t *res,   [r1]
@   ptrdiff_t stride,     [r2]
@   int dc)               [r3]

function JOIN(ff_hevc_rpi_add_residual_16x16_v_neon_, BIT_DEPTH), export=1
        push        {lr}
        vdup.16     q15, r3
        mov         r3, #16
        vmov.i16    q8,  #0
        add         lr, r0, r2
        vmov.i16    q9,  #(1 << BIT_DEPTH) - 1
        add         ip, r0, #32
1:
        vld2.16     {q0, q1}, [r0 :256]
        vld2.16     {q2, q3}, [ip :256]
        vld1.16     {q10, q11}, [r1 :256]!
        vqadd.s16   q0,  q15
          pldw        [lr]
        vqadd.s16   q1,  q10
          add         lr, r2
        vqadd.s16   q2,  q15
        subs        r3, #1
        vqadd.s16   q3,  q11
        clip16_4    q0, q1, q2, q3, q8, q9
        vst2.16     {q0, q1}, [r0 :256], r2
        vst2.16     {q2, q3}, [ip :256], r2
        bne         1b
        pop         {pc}
endfunc

@ ============================================================================
@ U & V add

@ add_residual4x4_c(
@   uint16_t *_dst,       [r0]
@   const int16_t *res,   [r1]
@   ptrdiff_t stride)     [r2]

function JOIN(ff_hevc_rpi_add_residual_4x4_c_neon_, BIT_DEPTH), export=1
        vmov.i16    q8,  #0
        add         ip, r0, r2
        vmov.i16    q9,  #(1 << BIT_DEPTH) - 1
        lsl         r2, #1
        vldm        r1, {q10-q13}
        vld2.16     {d0, d2}, [r0 :128], r2
        vld2.16     {d1, d3}, [ip :128], r2
        vld2.16     {d4, d6}, [r0 :128]
        vld2.16     {d5, d7}, [ip :128]

        sub         r0, r2
        vqadd.s16   q0,  q10
        sub         ip, r2
        vqadd.s16   q1,  q12
        vqadd.s16   q2,  q11
        vqadd.s16   q3,  q13
        clip16_4    q0, q1, q2, q3, q8, q9

        vst2.16     {d0, d2}, [r0 :128], r2
        vst2.16     {d1, d3}, [ip :128], r2
        vst2.16     {d4, d6}, [r0 :128]
        vst2.16     {d5, d7}, [ip :128]
        bx          lr
endfunc

@ add_residual8x8_c(
@   uint16_t *_dst,       [r0]
@   const int16_t *res,   [r1]
@   ptrdiff_t stride)     [r2]

function JOIN(ff_hevc_rpi_add_residual_8x8_c_neon_, BIT_DEPTH), export=1
        push        {lr}
        add         ip, r0, r2
        lsl         r2, #1
        vmov.i16    q8,  #0
        add         r3, r1, #(8*8*2)  @ Offset to V
        vmov.i16    q9,  #(1 << BIT_DEPTH) - 1
        mov         lr, #8
1:
        vld1.16     {q10, q11}, [r1 :256]!
        subs        lr, #2
        vld2.16     {q0, q1}, [r0 :256]
        vld2.16     {q2, q3}, [ip :256]
        vld1.16     {q12, q13}, [r3 :256]!
        vqadd.s16   q0,  q10
        vqadd.s16   q1,  q12
        vqadd.s16   q2,  q11
        vqadd.s16   q3,  q13
        clip16_4    q0, q1, q2, q3, q8, q9
        vst2.16     {q0, q1}, [r0 :256], r2
        vst2.16     {q2, q3}, [ip :256], r2
        bne         1b
        pop         {pc}
endfunc

@ add_residual16x16_c(
@   uint16_t *_dst,       [r0]
@   const int16_t *res,   [r1]
@   ptrdiff_t stride)     [r2]

function JOIN(ff_hevc_rpi_add_residual_16x16_c_neon_, BIT_DEPTH), export=1
        push        {r4, lr}
        vmov.i16    q8,  #0
        add         r3,  r1, #(16*16*2)  @ Offset to V
        vmov.i16    q9,  #(1 << BIT_DEPTH) - 1
        add         ip, r0, #32
        add         r4, r0, r2
        mov         lr, #16
1:
        vld2.16     {q0, q1}, [r0 :256]
        vld2.16     {q2, q3}, [ip :256]
        vld1.16     {q10, q11}, [r1 :256]!
        vld1.16     {q12, q13}, [r3 :256]!
        vqadd.s16   q0,  q10
          pldw        [r4]
        vqadd.s16   q1,  q12
          add         r4, r2
        vqadd.s16   q2,  q11
        subs        lr, #1
        vqadd.s16   q3,  q13
        clip16_4    q0, q1, q2, q3, q8, q9
        vst2.16     {q0, q1}, [r0 :256], r2
        vst2.16     {q2, q3}, [ip :256], r2
        bne         1b
        pop         {r4,pc}
endfunc

