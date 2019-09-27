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


@ ff_hevc_rpi_pred_dc_4_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_dc_4_neon_8, export=1

        @ Average the els of top & left
        ldr         r2, [r2]
        vld1.32     {d0[0]}, [r1]
        mov         r1, #2
        vmov        s1, r2
        vmov        s2, r2
        vmov.i16    q2, #3
        add         r2, r0, r3
        vaddl.u8    q1, d0, d1    @ d2[0] = top[0] + left[0]
        lsl         r3, #1
        vmovl.u8    q0, d0
        vmov.i64    d7, #0xffff
        vmov.16     d4[0], r1     @ 2, 3, 3, 3...
        vpadd.i16   d6, d2, d2    @ 2 (top & bottom of vector the same)
        vbit        d0, d2, d7    @ q0 = top[0]+left[0], top[1..3], left[0..3]

        @ top line gets some smoothing
        @ (top[i] + 3*dc + 2) >> 2
        @ as does left
        @ top_line[0] is extra special
        @ (top[0] + left[0] + 2*dc + 2) >> 2

        vmov.i64    d7, #0xff
        vpadd.i16   d6, d6        @ 1 (all the same)
        vrshr.u16   d6, #3
        vmla.i16    q0, q2, d6[0]
        vdup.8      d6, d6[0]
        vrshrn.i16  d0, q0, #2

        @ Store top line
        vst1.32     {d0[0]}, [r0], r3

        @ Store the rest
        vshr.u64    d1, d0, #5*8
        vshr.u64    d2, d0, #6*8
        vshr.u64    d3, d0, #7*8
        vbif        d1, d6, d7
        vbif        d2, d6, d7
        vst1.32     {d1[0]}, [r2], r3
        vbif        d3, d6, d7
        vst1.32     {d2[0]}, [r0]
        vst1.32     {d3[0]}, [r2]

        bx          lr
endfunc


@ ff_hevc_rpi_pred_dc_c_4_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_dc_c_4_neon_8, export=1

        @ Average the els of top & left
        vld1.8      {d0}, [r1]
        vld1.8      {d1}, [r2]
A       add         r2, r0, r3, lsl #1
A       lsl         r3, #2
T       lsl         r3, #1
T       add         r2, r0, r3
T       lsl         r3, #1
        vaddl.u8    q0, d0, d1
        vadd.i16    d0, d1       @ d0 has 2 val pairs
        vpadd.i32   d2, d0, d0   @ This adds U & V separately
        vpadd.i32   d3, d0, d0
        vrshrn.u16  d0, q1, #3

        @ Store
        vst1.8      {d0}, [r0], r3
        vst1.8      {d0}, [r2], r3
        vst1.8      {d0}, [r0]
        vst1.8      {d0}, [r2]

        bx          lr
endfunc


@ ff_hevc_rpi_pred_dc_8_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_dc_8_neon_8, export=1

        @ Average the els of top & left
        vld1.8      {d0}, [r1]
        mov         r1, #2
        vld1.8      {d16}, [r2]
        vmov.i16    q2, #3
        vmov.i64    d7, #0xffff
        vaddl.u8    q1, d0, d16   @ d2[0] = top[0] + left[0]
        vmovl.u8    q0, d0
        vadd.i16    d6, d2, d3    @ d6 has 4 vals
        vmov.16     d4[0], r1     @ 2, 3, 3, 3...
        vbit        d0, d2, d7    @ q0 = top[0]+left[0], top[1..7]

        @ top line gets some smoothing
        @ (top[i] + 3*dc + 2) >> 2
        @ as does left
        @ top_line[0] is extra special
        @ (top[0] + left[0] + 2*dc + 2) >> 2

        vmov.i64    d7, #0xff
        vmovl.u8    q1, d16
        vpadd.i16   d6, d6        @ 2 (top & bottom of vector the same)
        vpadd.i16   d6, d6        @ 1 (all the same)
        vrshr.u16   d6, #4
        vmla.i16    q1, q2, d6[0]
        vmla.i16    q0, q2, d6[0]
        vdup.8      d6, d6[0]
        vrshrn.i16  d2, q1, #2
        vrshrn.i16  d0, q0, #2

        @ Store top line
        vst1.8      {d0}, [r0], r3

        @ Store the rest
        vshr.u64    d2, #8
        vbit        d6, d2, d7
        vshr.u64    d2, #8
        vst1.8      {d6}, [r0], r3
        mov         r1, #6
1:
        vbit        d6, d2, d7
        vshr.u64    d2, #8
        vst1.8      {d6}, [r0], r3
        subs        r1, #2
        vbit        d6, d2, d7
        vshr.u64    d2, #8
        vst1.8      {d6}, [r0], r3
        bne         1b

        bx          lr
endfunc


@ ff_hevc_rpi_pred_dc_c_8_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_dc_c_8_neon_8, export=1

        @ Average the els of top & left
        vld1.8      {q0}, [r1]
        mov         r1, #8
        vld1.8      {q1}, [r2]
T       lsl         r3, #1
        vaddl.u8    q0, d0, d1
A       add         r2, r0, r3, lsl #1
A       lsl         r3, #2
T       add         r2, r0, r3
T       lsl         r3, #1
        vaddl.u8    q1, d2, d3
        vadd.i16    q1, q0
        vadd.i16    d3, d2        @ d3 has 2 val pairs
        vpadd.i32   d2, d3, d3    @ This add U & V separately
        vpadd.i32   d3, d3, d3
        vrshrn.u16  d0, q1, #4
        vrshrn.u16  d1, q1, #4

        @ Store
1:
        vst1.8      {q0}, [r0], r3
        subs        r1, #4
        vst1.8      {q0}, [r2], r3
        vst1.8      {q0}, [r0], r3
        vst1.8      {q0}, [r2], r3
        bne         1b

        bx          lr
endfunc


@ ff_hevc_rpi_pred_dc_16_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_dc_16_neon_8, export=1

        @ Average the els of top & left
        vld1.8      {q8}, [r1]
        mov         r1, #2
        vld1.8      {q9}, [r2]
        vaddl.u8    q10, d16, d17
        vaddl.u8    q11, d16, d18
        vaddl.u8    q0, d18, d19
        vmov.i16    q1, #3
        vadd.i16    q10, q0
        vmovl.u8    q0, d18
        vadd.i16    d20, d21
        vmov.i16    d2[0], r1     @ 2, 3, 3, 3...

        @ top line gets some smoothing
        @ (top[i] + 3*dc + 2) >> 2
        @ as does left
        @ top_line[0] is extra special
        @ (top[0] + left[0] + 2*dc + 2) >> 2

        vmovl.u8    q2, d16
        vmovl.u8    q9, d19
        vpadd.i16   d20, d20      @ 2 (top & bottom of vector the same)
        vmov.i64    d7, #0xffff
        vmovl.u8    q8, d17
        vbit        d4, d22, d7   @ q2 = top[0]+left[0], top[1..7]
        vmov.i64    d7, #0xff
        vpadd.i16   d20, d20      @ 1 (all the same)
        vrshr.u16   d21, d20, #5
        vrshr.u16   d20, d20, #5
        vmla.i16    q0, q10, d2[1]
        vmla.i16    q9, q10, d2[1]
        vmla.i16    q2, q10, q1
        vmla.i16    q8, q10, d2[1]
        vdup.8      q1, d20[0]
        vrshrn.i16  d0, q0, #2
        vrshrn.i16  d1, q9, #2
        vrshrn.i16  d4, q2, #2
        vrshrn.i16  d5, q8, #2
        vext.8      q0, q0, q0, #1

        @ Store top line
        vst1.8      {q2}, [r0], r3

        @ Store the rest
        mov         r1, #15
1:
        vbit        d2, d0, d7
        vext.8      q0, q0, q0, #1
        subs        r1, #1
        vst1.8      {q1}, [r0], r3
        bne         1b

        bx          lr
endfunc


@ ff_hevc_rpi_pred_dc_c_16_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_dc_c_16_neon_8, export=1

        @ Average the els of top & left
        vld1.8      {q0-q1}, [r1]
        mov         r1, #16
        vld1.8      {q2-q3}, [r2]
T       lsl         r3, #1
        vaddl.u8    q0, d0, d1
A       add         r2, r0, r3, lsl #1
T       add         r2, r0, r3
        vaddl.u8    q1, d2, d3
A       lsl         r3, #2
T       lsl         r3, #1
        vaddl.u8    q2, d4, d5
        vaddl.u8    q3, d6, d7
        vadd.i16    q0, q1
        vadd.i16    q2, q3
        vadd.i16    q0, q2
        vadd.i16    d0, d1        @ d0 has 2 val pairs
        vpadd.i32   d4, d0, d0    @ This adds U & V separately
        vpadd.i32   d5, d0, d0
        vrshrn.u16  d0, q2, #5
        vrshrn.u16  d1, q2, #5
        vrshrn.u16  d2, q2, #5
        vrshrn.u16  d3, q2, #5

        @ Store
1:
        vst1.8      {q0-q1}, [r0], r3
        subs        r1, #2
        vst1.8      {q0-q1}, [r2], r3
        bne         1b

        bx          lr
endfunc


@ ff_hevc_rpi_pred_dc_32_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_dc_32_neon_8, export=1

        @ Average the els of top & left
        vld1.8      {q0-q1}, [r1]
        mov         r1, #32
        vld1.8      {q2-q3}, [r2]
        add         r2, r0, r3
        vaddl.u8    q0, d0, d1
        lsl         r3, #1
        vaddl.u8    q1, d2, d3
        vaddl.u8    q2, d4, d5
        vaddl.u8    q3, d6, d7
        vadd.i16    q0, q1
        vadd.i16    q2, q3
        vadd.i16    q0, q2
        vadd.i16    d0, d1        @ d0 has 4 vals
        vpadd.i16   d0, d0        @ 2 (top & bottom the same)
        vpadd.i16   d4, d0, d0    @ 1 (all the same)
        vpadd.i16   d5, d0, d0
        vrshrn.u16  d0, q2, #6
        vrshrn.u16  d1, q2, #6
        vrshrn.u16  d2, q2, #6
        vrshrn.u16  d3, q2, #6

        @ Store
1:
        vst1.8      {q0-q1}, [r0], r3
        subs        r1, #2
        vst1.8      {q0-q1}, [r2], r3
        bne         1b

        bx          lr
endfunc


@ -----------------------------------------------------------------------------
@
@ 10 Bit versions
@
@ There is no actual bit depth dependency in this code except that our
@ intermediate results will overflow the 16 bits they are stored in
@ All there functions are good to 10 bits - with the worst case being
@ in dc_32 where we use all 16 bits.


@ ff_hevc_rpi_pred_dc_4_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_dc_4_neon_10, export=1

        @ Average the els of top & left
        vld1.16     {d0}, [r1]
        mov         r1, #2
        vld1.16     {d1}, [r2]
T       lsl         r3, #1
        vmov.i16    q2, #3
A       add         r2, r0, r3, lsl #1
T       add         r2, r0, r3
        vadd.u16    d2, d0, d1    @ d2[0] = top[0] + left[0]
A       lsl         r3, #2
T       lsl         r3, #1
        vmov.16     d4[0], r1     @ 2, 3, 3, 3...
        vmov.i64    d7, #0xffff
        vbit        d0, d2, d7    @ q0 = top[0]+left[0], top[1..3], left[0..3]

        @ top line gets some smoothing
        @ (top[i] + 3*dc + 2) >> 2
        @ as does left
        @ top_line[0] is extra special
        @ (top[0] + left[0] + 2*dc + 2) >> 2

        vpadd.i16   d6, d2, d2    @ 2 (top & bottom of vector the same)
        vpadd.i16   d6, d6        @ 1 (all the same)
        vrshr.u16   d6, #3
        vmla.i16    q0, q2, d6[0]
        vrshr.u16   q0, #2

        @ Store top line
        vst1.16     {d0}, [r0], r3

        @ Store the rest
        vshr.u64    d3, d1, #1*16
        vshr.u64    d4, d1, #2*16
        vshr.u64    d5, d1, #3*16
        vbif        d3, d6, d7
        vbif        d4, d6, d7
        vst1.16     {d3}, [r2], r3
        vbif        d5, d6, d7
        vst1.16     {d4}, [r0]
        vst1.16     {d5}, [r2]

        bx          lr
endfunc


@ ff_hevc_rpi_pred_dc_c_4_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]  (In pels - needs * 4)

function ff_hevc_rpi_pred_dc_c_4_neon_10, export=1

        @ Average the els of top & left
        vld1.8      {q0}, [r1]
        vld1.8      {q1}, [r2]
A       add         r2, r0, r3, lsl #2
A       lsl         r3, #3
T       lsl         r3, #2
T       add         r2, r0, r3
T       lsl         r3, #1
        vadd.i16    q0, q1
        vadd.i16    d0, d1       @ d0 has 2 val pairs
        vpadd.i32   d2, d0, d0   @ This adds U & V separately
        vpadd.i32   d3, d0, d0
        vrshr.u16   q0, q1, #3

        vst1.16     {q0}, [r0], r3
        vst1.16     {q0}, [r2], r3
        vst1.16     {q0}, [r0]
        vst1.16     {q0}, [r2]

        bx          lr
endfunc


@ ff_hevc_rpi_pred_dc_8_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_dc_8_neon_10, export=1

        @ Average the els of top & left
        vld1.16     {q0}, [r1]
        mov         r1, #2
        vld1.16     {q8}, [r2]
T       lsl         r3, #1
        vmov.i16    q2, #3
A       add         r2, r0, r3, lsl #1
T       add         r2, r0, r3
        vadd.i16    q1, q0, q8    @ q1[0] = top[0] + left[0]
A       lsl         r3, #2
T       lsl         r3, #1
        vmov.i64    d7, #0xffff
        vmov.16     d4[0], r1     @ 2, 3, 3, 3...
        vadd.i16    d6, d2, d3    @ d6 has 4 vals
        vbit        d0, d2, d7    @ q0 = top[0]+left[0], top[1..7]

        @ top line gets some smoothing
        @ (top[i] + 3*dc + 2) >> 2
        @ as does left
        @ top_line[0] is extra special
        @ (top[0] + left[0] + 2*dc + 2) >> 2

        vpadd.i16   d6, d6        @ 2 (top & bottom of vector the same)
        vpadd.i16   d6, d6        @ 1 (all the same)
        vrshr.u16   d6, #4
        vmla.i16    q8, q2, d6[0]
        vmla.i16    q0, q2, d6[0]
        vdup.16     q2, d6[0]
        vdup.16     q9, d6[0]
        vrshr.u16   q8, q8, #2
        vrshr.u16   q0, q0, #2
        vext.16     q1, q8, q8, #1

        @ Store top line
        vst1.16     {q0}, [r0], r3

        @ Store the rest
        vbit        d18, d2, d7
        vst1.16     {q9}, [r2], r3
        mov         r1, #6
1:
        vext.16     q8, q8, q8, #2
        subs        r1, #2
        vext.16     q1, q1, q1, #2
        vbit        d4, d16, d7
        vst1.16     {q2}, [r0], r3
        vbit        d18, d2, d7
        vst1.16     {q9}, [r2], r3
        bne         1b

        bx          lr
endfunc


@ ff_hevc_rpi_pred_dc_c_8_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]  (In pels - needs * 4)

function ff_hevc_rpi_pred_dc_c_8_neon_10, export=1

        @ Average the els of top & left
        vld1.16     {q0-q1}, [r1]
        mov         r1, #8
        vld1.16     {q2-q3}, [r2]
T       lsl         r3, #2
        vadd.i16    q1, q0
A       add         r2, r0, r3, lsl #2
A       lsl         r3, #3
T       add         r2, r0, r3
T       lsl         r3, #1
        vadd.i16    q2, q3
        vadd.i16    q1, q2
        vadd.i16    d3, d2        @ d3 has 2 val pairs
        vpadd.i32   d2, d3, d3    @ This add U & V separately
        vpadd.i32   d3, d3, d3
        vrshr.u16   q0, q1, #4
        vrshr.u16   q1, q1, #4

        @ Store
1:
        vst1.8      {q0-q1}, [r0], r3
        subs        r1, #2
        vst1.8      {q0-q1}, [r2], r3
        bne         1b

        bx          lr
endfunc


@ ff_hevc_rpi_pred_dc_16_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_dc_16_neon_10, export=1

        @ Average the els of top & left
        vld1.16     {q8-q9}, [r1]
        mov         r1, #2
        vld1.16     {q10-q11}, [r2]
        lsl         r3, #1        @ stride given in pels
        vadd.i16    q0, q8, q9
        vadd.i16    q1, q10, q11
        vmov.i16    q3, #3
        vadd.i16    q1, q0
        vadd.i16    d0, d16, d20
        vmov.i64    d31, #0xffff
        vadd.i16    d3, d2
        vmov.16     d6[0], r1     @ 2, 3, 3, 3...

        @ top line gets some smoothing
        @ (top[i] + 3*dc + 2) >> 2
        @ as does left
        @ topline[0] is extra special
        @ (top[0] + left[0] + 2*dc + 2) >> 2

        vbit        d16, d0, d31  @ q8 = top[0]+left[0], top[1..7]
        vpadd.i16   d3, d3        @ 2 (top & bottom of vector the same)
        vpadd.i16   d3, d3        @ 1 (all the same)
        vrshr.u16   d2, d3, #5
        vrshr.u16   d3, d3, #5
        vmov        q0, q1
        vmla.i16    q10, q1, d6[1]
        vmla.i16    q11, q1, d6[1]
        vmla.i16    q8, q1, q3
        vmla.i16    q9, q1, d6[1]
        vrshr.u16   q2, q10, #2
        vrshr.u16   q3, q11, #2
        vrshr.u16   q8, #2
        vrshr.u16   q9, #2
        vext.16     q2, q2, q2, #1
        mov         r1, #7<<29

        @ Store top line
        vst1.16     {q8-q9}, [r0], r3

        @ Store the rest
1:
        vbit        d0, d4, d31
        vext.16     q2, q2, q2, #1
        subs        r1, #1<<29
        vst1.16     {q0-q1}, [r0], r3
        bne         1b
1:
        vbit        d0, d6, d31
        vext.16     q3, q3, q3, #1
        subs        r1, #1<<29
        vst1.16     {q0-q1}, [r0], r3
        bne         1b

        bx          lr
endfunc


@ ff_hevc_rpi_pred_dc_c_16_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]  (In pels - needs * 4)

function ff_hevc_rpi_pred_dc_c_16_neon_10, export=1

        @ Average the els of top & left
        vldm        r1, {q0-q3}
        vldm        r2, {q8-q11}
        vadd.i16    q0, q1
        mov         r1, #16
        vadd.i16    q2, q3
        add         r2, r0, #32
        vadd.i16    q8, q9
        lsl         r3, #2
        vadd.i16    q10, q11
        vadd.u16    q0, q2
        vadd.u16    q8, q10
        vadd.i16    q0, q8
        vadd.i16    d0, d1        @ d0 has 2 val pairs
        vpadd.i32   d4, d0, d0    @ This adds U & V separately
        vpadd.i32   d5, d0, d0
        vrshr.u16   q0, q2, #5
        vrshr.u16   q1, q2, #5

        @ Store
1:
        vst1.16     {q0-q1}, [r0], r3
        subs        r1, #1
        vst1.16     {q0-q1}, [r2], r3
        bne         1b

        bx           lr
endfunc


@ ff_hevc_rpi_pred_dc_32_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]  (In pels)

function ff_hevc_rpi_pred_dc_32_neon_10, export=1

        @ Average the els of top & left
        @ With 10 bits we are (just) safe from overflow in i16
        vldm        r1, {q0-q3}
        vldm        r2, {q8-q11}
        vadd.i16    q0, q1
        mov         r1, #32
        vadd.i16    q2, q3
        add         r2, r0, #32
        vadd.i16    q8, q9
        lsl         r3, #1
        vadd.i16    q10, q11
        vadd.u16    q0, q2
        vadd.u16    q8, q10
        vadd.i16    q0, q8
        vadd.i16    d0, d1        @ d0 has 4 vals
        vpadd.i16   d0, d0        @ 2 (top & bottom the same)
        vpadd.i16   d4, d0, d0    @ 1 (all the same)
        vpadd.i16   d5, d0, d0
        vrshr.u16   q0, q2, #6
        vrshr.u16   q1, q2, #6

        @ Store
1:
        vst1.16     {q0-q1}, [r0], r3
        subs        r1, #1
        vst1.16     {q0-q1}, [r2], r3
        bne         1b

        bx           lr
endfunc


