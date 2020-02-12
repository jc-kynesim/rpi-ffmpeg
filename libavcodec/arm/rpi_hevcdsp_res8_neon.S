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

@ General notes:
@
@ Residual is generally only guaranteed to be clipped to 16 bits.
@ This means that we do need to do vmovl, vqadd, vqmovun
@ rather than vaddw, vqmovun (if we were clipped to 15 then we could get away
@ with this).
@
@ There is an exception for the DC case because its transform is guaranteed
@ to be small enough that overflow cannot occur during the first add.

@ ============================================================================
@ Y add

function ff_hevc_rpi_add_residual_4x4_neon_8, export=1
        add         ip, r0, r2
        vld1.16     {q0, q1}, [r1]
        lsl         r2, #1
        vld1.32     d4[0], [r0], r2
        rsb         r3, r2, #0
        vld1.32     d4[1], [ip], r2
        vld1.32     d5[0], [r0], r3
        vld1.32     d5[1], [ip], r3
        vmovl.u8    q8, d4
        vmovl.u8    q9, d5
        vqadd.s16   q0, q8
        vqadd.s16   q1, q9
        vqmovun.s16 d0, q0
        vqmovun.s16 d1, q1
        vst1.32     d0[0], [r0], r2
        vst1.32     d0[1], [ip], r2
        vst1.32     d1[0], [r0]
        vst1.32     d1[1], [ip]
        bx          lr
endfunc

function ff_hevc_rpi_add_residual_8x8_neon_8, export=1
        push        {r4, lr}
        vld1.16     {q0, q1}, [r1]!
        add         ip, r0, r2
        vld1.8      {d6}, [r0]
        add         r4, r0, r2, lsl #1
        vld1.8      {d7}, [ip]
        add         lr, ip, r2, lsl #1
        lsl         r2, #1
        mov         r3, #8-2
        vmovl.u8    q2, d6
        vmovl.u8    q3, d7
        vqadd.s16   q2, q0
        vqadd.s16   q3, q1
1:
          vld1.16     {q0, q1}, [r1]!
        subs        r3, #2
        vqmovun.s16 d4, q2
        vqmovun.s16 d5, q3
          vld1.8      {d6}, [r4], r2
          vld1.8      {d7}, [lr], r2
        vst1.8      {d4}, [r0], r2
        vst1.8      {d5}, [ip], r2
          vmovl.u8    q2, d6
            pldw        [r4]
          vmovl.u8    q3, d7
          vqadd.s16   q2, q0
          vqadd.s16   q3, q1
        bne         1b

          vqmovun.s16 d4, q2
          vqmovun.s16 d5, q3
          vst1.8      {d4}, [r0]
          vst1.8      {d5}, [ip]
          pop         {r4, pc}
endfunc

function ff_hevc_rpi_add_residual_16x16_neon_8, export=1
        vld1.16     {q0, q1}, [r1]!
        add         ip, r0, r2
        vld1.8      {q3}, [r0]
        mov         r3, #16-1
        vmovl.u8    q2, d6
        vmovl.u8    q3, d7
        vqadd.s16   q2, q0
        vqadd.s16   q3, q1
1:
          vld1.16     {q0, q1}, [r1]!
        subs        r3, #1
        vqmovun.s16 d4, q2
        vqmovun.s16 d5, q3
          vld1.8      {q3}, [ip], r2
        vst1.8      {q2}, [r0], r2
          vmovl.u8    q2, d6
            pldw        [ip]
          vmovl.u8    q3, d7
          vqadd.s16   q2, q0
          vqadd.s16   q3, q1
        bne         1b

          vqmovun.s16 d4, q2
          vqmovun.s16 d5, q3
          vst1.8      {q2}, [r0]
          bx          lr
endfunc

function ff_hevc_rpi_add_residual_32x32_neon_8, export=1
        vldm        r1!, {q0-q3}
        vld1.8      {q8, q9}, [r0]
        add         ip, r0, r2
        vmovl.u8    q10, d16
        mov         r3, #32-1
        vmovl.u8    q11, d17
        vmovl.u8    q12, d18
        vmovl.u8    q13, d19
        vqadd.s16   q10, q0
        vqadd.s16   q11, q1
        vqadd.s16   q12, q2
        vqadd.s16   q13, q3
1:
          vldm        r1!, {q0-q3}
        vqmovun.s16 d20, q10
        vqmovun.s16 d21, q11
        vqmovun.s16 d22, q12
        vqmovun.s16 d23, q13
          vld1.8      {q8, q9}, [ip], r2
        subs        r3, #1
        vst1.8      {q10, q11}, [r0], r2
          vmovl.u8    q10, d16
            pldw        [ip]
          vmovl.u8    q11, d17
          vmovl.u8    q12, d18
          vmovl.u8    q13, d19
          vqadd.s16   q10, q0
          vqadd.s16   q11, q1
          vqadd.s16   q12, q2
          vqadd.s16   q13, q3
        bne     1b

          vqmovun.s16 d20, q10
          vqmovun.s16 d21, q11
          vqmovun.s16 d22, q12
          vqmovun.s16 d23, q13
          vst1.8      {q10, q11}, [r0]
          bx          lr
endfunc


@ ff_hevc_rpi_add_residual_4x4_dc_neon_8(
@   uint8_t * dst,              // [r0]
@   unsigned int stride,        // [r1]
@   int dc)                     // [r2]

function ff_hevc_rpi_add_residual_4x4_dc_neon_8, export=1
        add         ip, r0, r1
        vdup.16     q15, r2
        lsl         r1, #1
        vld1.32     d4[0], [r0], r1
        rsb         r3, r1, #0
        vld1.32     d4[1], [ip], r1
        vld1.32     d5[0], [r0], r3
        vld1.32     d5[1], [ip], r3
        vaddw.u8    q0, q15, d4
        vaddw.u8    q1, q15, d5
        vqmovun.s16 d0, q0
        vqmovun.s16 d1, q1
        vst1.32     d0[0], [r0], r1
        vst1.32     d0[1], [ip], r1
        vst1.32     d1[0], [r0]
        vst1.32     d1[1], [ip]
        bx          lr
endfunc

@ ============================================================================
@ DC Y or C add

@ ff_hevc_rpi_add_residual_4x4_dc_c_neon_8(
@   uint8_t * dst,              // [r0]
@   unsigned int stride,        // [r1]
@   int dc)                     // [r2]

function ff_hevc_rpi_add_residual_4x4_dc_c_neon_8, export=1
        mov         r3,  #4-2
        vdup.32     q15, r2
        b           1f
endfunc

@ ff_hevc_rpi_add_residual_8x8_dc_neon_8(
@   uint8_t * dst,              // [r0]
@   unsigned int stride,        // [r1]
@   int dc)                     // [r2]

function ff_hevc_rpi_add_residual_8x8_dc_neon_8, export=1
        vdup.16     q15, r2
        mov         r3, #8-2
1:      vld1.8      d16, [r0]
        add         ip, r0, r1
        push        {r4, lr}
        vld1.8      d17, [ip]
        add         r4, r0, r1, lsl #1
        vaddw.u8    q0, q15, d16
        lsl         r1, #1
        vaddw.u8    q1, q15, d17
        add         lr, ip, r1
1:
          vld1.8      {d16}, [r4], r1
          vld1.8      {d17}, [lr], r1
        subs        r3, #2
        vqmovun.s16 d4, q0
        vqmovun.s16 d5, q1
          vaddw.u8    q0, q15, d16
          vaddw.u8    q1, q15, d17
        vst1.8      {d4}, [r0], r1
        vst1.8      {d5}, [ip], r1
        bne         1b

          vqmovun.s16 d4, q0
          vqmovun.s16 d5, q1
          vst1.8      {d4}, [r0]
          vst1.8      {d5}, [ip]
          pop         {r4, pc}
endfunc


@ ff_hevc_rpi_add_residual_8x8_dc_c_neon_8(
@   uint8_t * dst,              // [r0]
@   unsigned int stride,        // [r1]
@   int dc)                     // [r2]

function ff_hevc_rpi_add_residual_8x8_dc_c_neon_8, export=1
        mov         r3,  #8-1
        vdup.32     q15, r2
        b           1f
endfunc

@ ff_hevc_rpi_add_residual_16x16_dc_neon_8(
@   uint8_t * dst,              // [r0]
@   unsigned int stride,        // [r1]
@   int dc)                     // [r2]

function ff_hevc_rpi_add_residual_16x16_dc_neon_8, export=1
        vdup.16     q15, r2
        mov         r3,  #16-1
1:      vld1.8      {q8}, [r0]
        add         ip, r0, r1
        vaddw.u8    q0, q15, d16
        vaddw.u8    q1, q15, d17
1:
          vld1.8      {q8}, [ip], r1
        subs        r3, #1
        vqmovun.s16 d4, q0
        vqmovun.s16 d5, q1
          vaddw.u8    q0, q15, d16
          vaddw.u8    q1, q15, d17
        vst1.8      {q2}, [r0], r1
        bne         1b

          vqmovun.s16 d4, q0
          vqmovun.s16 d5, q1
          vst1.8      {q2}, [r0]
          bx          lr
endfunc


@ ff_hevc_rpi_add_residual_16x16_dc_c_neon_8(
@   uint8_t * dst,              // [r0]
@   unsigned int stride,        // [r1]
@   int dc)                     // [r2]

function ff_hevc_rpi_add_residual_16x16_dc_c_neon_8, export=1
        mov         r3,  #16-1
        vdup.32     q15, r2
        b           1f
endfunc

@ ff_hevc_rpi_add_residual_32x32_dc_neon_8(
@   uint8_t * dst,              // [r0]
@   unsigned int stride,        // [r1]
@   int dc)                     // [r2]

function ff_hevc_rpi_add_residual_32x32_dc_neon_8, export=1
        vdup.16     q15, r2
        mov         r3, #32-1
1:      vld1.8      {q8, q9}, [r0]
        add         ip, r0, r1
        vaddw.u8    q0, q15, d16
        vaddw.u8    q1, q15, d17
        vaddw.u8    q2, q15, d18
        vaddw.u8    q3, q15, d19
1:
        vqmovun.s16 d20, q0
        vqmovun.s16 d21, q1
        vqmovun.s16 d22, q2
        vqmovun.s16 d23, q3
          vld1.8      {q8, q9}, [ip], r1
        subs        r3, #1
          vaddw.u8    q0, q15, d16
          vaddw.u8    q1, q15, d17
          vaddw.u8    q2, q15, d18
          vaddw.u8    q3, q15, d19
        vst1.8      {q10, q11}, [r0], r1
        bne     1b

          vqmovun.s16 d20, q0
          vqmovun.s16 d21, q1
          vqmovun.s16 d22, q2
          vqmovun.s16 d23, q3
          vst1.8      {q10, q11}, [r0]
          bx          lr
endfunc

@ ============================================================================
@ U add

@ add_residual4x4_c(
@   uint8_t *_dst,        [r0]
@   const int16_t *res,   [r1]
@   ptrdiff_t stride,     [r2]
@   int dc_v)             [r3]

function ff_hevc_rpi_add_residual_4x4_u_neon_8, export=1
        add         ip, r0, r2
        vld1.16     {q0, q1}, [r1]
        lsl         r2, #1
        vld1.8      {d16}, [r0 :64], r2
        vld1.8      {d17}, [ip :64], r2
        vld1.8      {d18}, [r0 :64]
        sub         r0, r2
        vld1.8      {d19}, [ip :64]
        sub         ip, r2
        vdup.16     q2, r3
        vdup.16     q3, r3
        vmovl.u8    q10, d16
        vmovl.u8    q11, d17
        vmovl.u8    q12, d18
        vmovl.u8    q13, d19
        vzip.16     q0, q2
        vzip.16     q1, q3
        vqadd.s16   q0,  q10
        vqadd.s16   q2,  q11
        vqadd.s16   q1,  q12
        vqadd.s16   q3,  q13
        vqmovun.s16 d0,  q0
        vqmovun.s16 d1,  q2
        vqmovun.s16 d2,  q1
        vqmovun.s16 d3,  q3
        vst1.8      {d0}, [r0 :64], r2
        vst1.8      {d1}, [ip :64], r2
        vst1.8      {d2}, [r0 :64]
        vst1.8      {d3}, [ip :64]
        bx          lr
endfunc

@ add_residual8x8_c(
@   uint8_t *_dst,        [r0]
@   const int16_t *res,   [r1]
@   ptrdiff_t stride)     [r2]
@   int dc_v)             [r3]

function ff_hevc_rpi_add_residual_8x8_u_neon_8, export=1
        vdup.16     q15, r3
        add         ip, r0, r2
        push        {r4, lr}
        vld2.8      {d16, d17}, [r0 :128]
        lsl         r2, #1
        vld2.8      {d18, d19}, [ip :128]
        mov         r3, #8-2
        vld1.16     {q0, q1}, [r1 :256]!
        add         r4, r0, r2
        vmovl.u8    q10, d16
        add         lr, ip, r2
        vmovl.u8    q11, d18
        vqadd.s16   q0,  q10
        vaddw.u8    q2,  q15, d17
        vqadd.s16   q1,  q11
        vaddw.u8    q3,  q15, d19
1:
        vqmovun.s16 d20,  q0
        vqmovun.s16 d21,  q2
          vld2.8      {d16, d17}, [r4 :128], r2
        subs        r3, #2
        vqmovun.s16 d22,  q1
        vqmovun.s16 d23,  q3
        vst2.8      {d20, d21}, [r0 :128], r2
          vld2.8      {d18, d19}, [lr :128], r2
        vst2.8      {d22, d23}, [ip :128], r2
          vld1.16     {q0, q1}, [r1 :256]!
          vmovl.u8    q10, d16
          vmovl.u8    q11, d18
          vqadd.s16   q0,  q10
          vaddw.u8    q2,  q15, d17
          vqadd.s16   q1,  q11
          vaddw.u8    q3,  q15, d19
        bne         1b

          vqmovun.s16 d20,  q0
          vqmovun.s16 d21,  q2
          vqmovun.s16 d22,  q1
          vqmovun.s16 d23,  q3
          vst2.8      {d20, d21}, [r0 :128]
          vst2.8      {d22, d23}, [ip :128]
          pop         {r4, pc}
endfunc

@ add_residual16x16_u(
@   uint8_t *_dst,        [r0]
@   const int16_t *res,   [r1]
@   ptrdiff_t stride)     [r2]
@   int dc_v)             [r3]

function ff_hevc_rpi_add_residual_16x16_u_neon_8, export=1
        vdup.16     q15, r3
        add         ip, r0, r2
        vld2.8      {q8, q9}, [r0 :256]
        mov         r3, #16-1
        vld1.16     {q0, q1}, [r1 :256]!
        vmovl.u8    q11, d16
        vmovl.u8    q12, d17
        vqadd.s16   q0,  q11
        vaddw.u8    q11, q15, d18
        vqadd.s16   q1,  q12
        vaddw.u8    q12, q15, d19
1:
          vld2.8      {q8, q9}, [ip :256], r2
        subs        r3, #1
        vqmovun.s16 d20, q0
        vqmovun.s16 d22, q11
        vqmovun.s16 d21, q1
        vqmovun.s16 d23, q12
          vld1.16     {q0, q1}, [r1 :256]!
        vst2.8      {q10, q11}, [r0 :256], r2
          vmovl.u8    q11, d16
            pldw        [ip]
          vmovl.u8    q12, d17
          vqadd.s16   q0,  q11
          vaddw.u8    q11, q15, d18
          vqadd.s16   q1,  q12
          vaddw.u8    q12, q15, d19
        bne         1b

          vqmovun.s16 d20, q0
          vqmovun.s16 d22, q11
          vqmovun.s16 d21, q1
          vqmovun.s16 d23, q12
          vst2.8      {q10, q11}, [r0 :256]
          bx          lr
endfunc

@ ============================================================================
@ V add

@ add_residual4x4_v(
@   uint8_t *_dst,        [r0]
@   const int16_t *res,   [r1]
@   ptrdiff_t stride)     [r2]

function ff_hevc_rpi_add_residual_4x4_v_neon_8, export=1
        add         ip, r0, r2
        vld1.16     {q2, q3}, [r1]
        lsl         r2, #1
        vld1.8      {d16}, [r0 :64], r2
        vld1.8      {d17}, [ip :64], r2
        vld1.8      {d18}, [r0 :64]
        sub         r0, r2
        vld1.8      {d19}, [ip :64]
        sub         ip, r2
        vdup.16     q0, r3
        vdup.16     q1, r3
        vmovl.u8    q10, d16
        vmovl.u8    q11, d17
        vmovl.u8    q12, d18
        vmovl.u8    q13, d19
        vzip.16     q0, q2
        vzip.16     q1, q3
        vqadd.s16   q0,  q10
        vqadd.s16   q2,  q11
        vqadd.s16   q1,  q12
        vqadd.s16   q3,  q13
        vqmovun.s16 d0,  q0
        vqmovun.s16 d1,  q2
        vqmovun.s16 d2,  q1
        vqmovun.s16 d3,  q3
        vst1.8      {d0}, [r0 :64], r2
        vst1.8      {d1}, [ip :64], r2
        vst1.8      {d2}, [r0 :64]
        vst1.8      {d3}, [ip :64]
        bx          lr
endfunc

@ add_residual8x8_v(
@   uint8_t *_dst,        [r0]
@   const int16_t *res,   [r1]
@   ptrdiff_t stride)     [r2]

function ff_hevc_rpi_add_residual_8x8_v_neon_8, export=1
        vdup.16     q15, r3
        add         ip, r0, r2
        push        {r4, lr}
        vld2.8      {d16, d17}, [r0 :128]
        lsl         r2, #1
        vld2.8      {d18, d19}, [ip :128]
        mov         r3, #8-2
        vld1.16     {q0, q1}, [r1 :256]!
        add         r4, r0, r2
        vmovl.u8    q10, d17
        add         lr, ip, r2
        vmovl.u8    q11, d19
        vqadd.s16   q0,  q10
        vaddw.u8    q2,  q15, d16
        vqadd.s16   q1,  q11
        vaddw.u8    q3,  q15, d18
1:
        vqmovun.s16 d20,  q2
        vqmovun.s16 d21,  q0
          vld2.8      {d16, d17}, [r4 :128], r2
        subs        r3, #2
        vqmovun.s16 d22,  q3
        vqmovun.s16 d23,  q1
        vst2.8      {d20, d21}, [r0 :128], r2
          vld2.8      {d18, d19}, [lr :128], r2
        vst2.8      {d22, d23}, [ip :128], r2
          vld1.16     {q0, q1}, [r1 :256]!
          vmovl.u8    q10, d17
          vmovl.u8    q11, d19
          vqadd.s16   q0,  q10
          vaddw.u8    q2,  q15, d16
          vqadd.s16   q1,  q11
          vaddw.u8    q3,  q15, d18
        bne         1b

          vqmovun.s16 d20,  q2
          vqmovun.s16 d21,  q0
          vqmovun.s16 d22,  q3
          vqmovun.s16 d23,  q1
          vst2.8      {d20, d21}, [r0 :128]
          vst2.8      {d22, d23}, [ip :128]
          pop         {r4, pc}
endfunc

@ add_residual16x16_v(
@   uint8_t *_dst,        [r0]
@   const int16_t *res,   [r1]
@   ptrdiff_t stride)     [r2]

function ff_hevc_rpi_add_residual_16x16_v_neon_8, export=1
        vdup.16     q15, r3
        add         ip, r0, r2
        vld2.8      {q8, q9}, [r0 :256]
        mov         r3, #16-1
        vld1.16     {q0, q1}, [r1 :256]!
        vmovl.u8    q11, d18
        vmovl.u8    q12, d19
        vqadd.s16   q0,  q11
        vaddw.u8    q11, q15, d16
        vqadd.s16   q1,  q12
        vaddw.u8    q12, q15, d17
1:
          vld2.8      {q8, q9}, [ip :256], r2
        subs        r3, #1
        vqmovun.s16 d20, q11
        vqmovun.s16 d22, q0
        vqmovun.s16 d21, q12
        vqmovun.s16 d23, q1
          vld1.16     {q0, q1}, [r1 :256]!
        vst2.8      {q10, q11}, [r0 :256], r2
          vmovl.u8    q11, d18
            pldw        [ip]
          vmovl.u8    q12, d19
          vqadd.s16   q0,  q11
          vaddw.u8    q11, q15, d16
          vqadd.s16   q1,  q12
          vaddw.u8    q12, q15, d17
        bne         1b

          vqmovun.s16 d20, q11
          vqmovun.s16 d22, q0
          vqmovun.s16 d21, q12
          vqmovun.s16 d23, q1
          vst2.8      {q10, q11}, [r0 :256]
          bx          lr
endfunc

@ ============================================================================
@ U & V add

@ add_residual4x4_c(
@   uint8_t *_dst,        [r0]
@   const int16_t *res,   [r1]
@   ptrdiff_t stride)     [r2]

function ff_hevc_rpi_add_residual_4x4_c_neon_8, export=1
        add         ip, r0, r2
        vld1.16     {q0, q1}, [r1]!       @ all of U
        lsl         r2, #1
        vld1.8      {d16}, [r0 :64], r2
        rsb         r3, r2, #0
        vld1.8      {d17}, [ip :64], r2
        vld1.16     {q2, q3}, [r1]        @ all of V
        vld1.8      {d18}, [r0 :64], r3
        vld1.8      {d19}, [ip :64], r3
        vmovl.u8    q10, d16
        vmovl.u8    q11, d17
        vmovl.u8    q12, d18
        vmovl.u8    q13, d19
        vzip.16     q0, q2
        vzip.16     q1, q3
        vqadd.s16   q0,  q10
        vqadd.s16   q2,  q11
        vqadd.s16   q1,  q12
        vqadd.s16   q3,  q13
        vqmovun.s16 d0,  q0
        vqmovun.s16 d1,  q2
        vqmovun.s16 d2,  q1
        vqmovun.s16 d3,  q3
        vst1.8      {d0}, [r0 :64], r2
        vst1.8      {d1}, [ip :64], r2
        vst1.8      {d2}, [r0 :64]
        vst1.8      {d3}, [ip :64]
        bx          lr
endfunc

@ add_residual8x8_c(
@   uint8_t *_dst,        [r0]
@   const int16_t *res,   [r1]
@   ptrdiff_t stride)     [r2]

function ff_hevc_rpi_add_residual_8x8_c_neon_8, export=1
        vld2.8      {d16, d17}, [r0 :128]
        add         r3, r1, #(8*8*2)  @ Offset to V
        vld1.16     {q0}, [r1 :128]!
        add         ip, r0, r2
        vld1.16     {q1}, [r3 :128]!
        vmovl.u8    q10, d16
        push        {lr}
        vmovl.u8    q8,  d17
        mov         lr, #8-1
        vqadd.s16   q10, q0
        vqadd.s16   q1,  q8
1:
          vld2.8      {d16, d17}, [ip :128], r2
        subs        lr, #1
          vld1.16     {q0}, [r1 :128]!
        vqmovun.s16 d20, q10
        vqmovun.s16 d21, q1
          vld1.16     {q1}, [r3 :128]!
        vst2.8      {d20, d21}, [r0 :128], r2
          vmovl.u8    q10, d16
            pldw        [ip]
          vmovl.u8    q8,  d17
          vqadd.s16   q10, q0
          vqadd.s16   q1,  q8
        bne         1b

          vqmovun.s16 d20, q10
          vqmovun.s16 d21, q1
          vst2.8      {d20, d21}, [r0 :128]
          pop         {pc}
endfunc

@ add_residual16x16_c(
@   uint8_t *_dst,        [r0]
@   const int16_t *res,   [r1]
@   ptrdiff_t stride)     [r2]

function ff_hevc_rpi_add_residual_16x16_c_neon_8, export=1
        vld2.8      {q8, q9}, [r0 :256]
        add         r3, r1, #(16*16*2)  @ Offset to V
        vld1.16     {q0, q1}, [r1 :256]!
        add         ip, r0, r2
        vld1.16     {q2, q3}, [r3 :256]!
        vmovl.u8    q10, d16
        push        {lr}
        vmovl.u8    q8,  d17
        mov         lr, #16-1
        vmovl.u8    q11, d18
        vmovl.u8    q9,  d19
        vqadd.s16   q0,  q10
        vqadd.s16   q1,  q8
        vqadd.s16   q2,  q11
        vqadd.s16   q3,  q9
1:
          vld2.8      {q8, q9}, [ip :256], r2
        subs        lr, #1
        vqmovun.s16 d20, q0
        vqmovun.s16 d22, q2
        vqmovun.s16 d21, q1
        vqmovun.s16 d23, q3
          vld1.16     {q0, q1}, [r1 :256]!
        vst2.8      {d20-d23}, [r0 :256], r2
          vld1.16     {q2, q3}, [r3 :256]!
          vmovl.u8    q10, d16
            pldw        [ip]
          vmovl.u8    q8,  d17
          vmovl.u8    q11, d18
          vmovl.u8    q9,  d19
          vqadd.s16   q0,  q10
          vqadd.s16   q1,  q8
          vqadd.s16   q2,  q11
          vqadd.s16   q3,  q9
        bne         1b

          vqmovun.s16 d20, q0
          vqmovun.s16 d22, q2
          vqmovun.s16 d21, q1
          vqmovun.s16 d23, q3
          vst2.8      {d20-d23}, [r0 :256]
          pop         {pc}
endfunc

@ 32x32 chroma never occurs so NIF

@ ============================================================================
