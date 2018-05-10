/*
 * Copyright (c) 2017 John Cox <jc@kynesim.co.uk> (for Raspberry Pi)
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
        vbit        d0, d2, d7    @ top[0]+left[0], top[1..3], left[0..3]

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
        mov r1, #2
        vld1.8      {d16}, [r2]
        vmov.i16    q2, #3
        vmov.i64    d7, #0xffff
        vaddl.u8    q1, d0, d16   @ d2[0] = top[0] + left[0]
        vmovl.u8    q0, d0
        vadd.i16    d6, d2, d3    @ d6 has 4 vals
        vmov.16     d4[0], r1     @ 2, 3, 3, 3...
        vbit        d0, d2, d7    @ top[0]+left[0], top[1..3], left[0..3]

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
        vld1.8     {q8 }, [r1]
        vld1.8     {q12}, [r2]
        vaddl.u8    q0,  d16, d17
        vaddl.u8    q2,  d24, d25
        vadd.i16    q0,  q2
        vadd.i16    d0,  d1       @ d0 has 2 val pairs
        mov         r1,  #4
        vpadd.i32   d0,  d0       @ This add U & V separately
        lsl         r3,  #1       @ pels
        vrshrn.u16  d0,  q0,  #4
        vdup.u16    q0,  d0[0]    @ Dup results

        @ Store
1:
        vst1.8     {q0 }, [r0], r3
        subs        r1,  #1
        vst1.8     {q0 }, [r0], r3
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
        vld1.8     { q8}, [r1]
        vld1.8     {q12}, [r2]
        vaddl.u8    q0,  d16, d24
        vaddl.u8    q2,  d17, d25
        vmov.u16    r1,  d0[0]    @ r1 = top[0] + left[0]
        vadd.i16    q0,  q2
        vadd.i16    d0,  d1       @ d0 has 4 vals
        vpadd.i16   d0,  d0       @ 2 (top & bottom the same)
        vpadd.i16   d0,  d0       @ 1 (all the same)
        vrshr.u16   d0,  #5

        vmov.i64    d31, #0xff

        @ top line gets some smoothing
        @ (top[i] + 3*dc + 2) >> 2
        @ top_line[0] is extra special
        @ (top[0] + left[0] + dc * 2)

        vmov.u16    r12, d0[0]   @ dc
        add         r2,  r12, r12, lsl #1  @ dc*3
        add         r1,  r1,  r12, lsl #1  @ top[0] + left[0] + dc*2

        vdup.u16    q3,  r2
        vaddw.u8    q1,  q3,  d16
        vaddw.u8    q2,  q3,  d17
        vmov.u16    d2[0], r1
        vrshrn.u16  d2,  q1,  #2
        vrshrn.u16  d3,  q2,  #2

        @ Construct lhs pels
        vaddw.u8    q2,  q3,  d24
        vaddw.u8    q3,  q3,  d25
        vrshrn.u16  d4,  q2,  #2
        vrshrn.u16  d5,  q3,  #2

        @ Store top line
        vst1.8     { q1}, [r0], r3

        mov         r1,  #15
        vdup.u8     q0,  d0[0]

1:
        vext.8      q2,  q2,  #1
        vbit        d0,  d4,  d31
        subs        r1,  #1
        vst1.8     { q0}, [r0], r3
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
        vld1.8     { q8, q9}, [r1]
        vld1.8     {q12,q13}, [r2]
        vaddl.u8    q0,  d16, d17
        vaddl.u8    q1,  d18, d19
        vaddl.u8    q2,  d24, d25
        vaddl.u8    q3,  d26, d27
        vadd.i16    q0,  q1
        vadd.i16    q2,  q3
        vadd.i16    q0,  q2
        lsl         r3,  #1
        vadd.i16    d0,  d1       @ d0 has 2 val pairs
        mov         r1,  #4
        vpadd.i32   d0,  d0       @ This add U & V separately
        add         r2,  r0,  r3
        vmov        d1,  d0
        lsl         r3,  #1
        vrshrn.u16  d0,  q0,  #5
        vmov        d1,  d0       @ Dup results
        vmov        q1,  q0

        @ Store
1:
        vst1.8     { q0, q1}, [r0], r3
        vst1.8     { q0, q1}, [r2], r3
        subs        r1,  #1
        vst1.8     { q0, q1}, [r0], r3
        vst1.8     { q0, q1}, [r2], r3
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
        vld1.8     {q8,  q9 }, [r1]
        vld1.8     {q12, q13}, [r2]
        vaddl.u8    q0,  d16, d17
        vaddl.u8    q1,  d18, d19
        vaddl.u8    q2,  d24, d25
        vaddl.u8    q3,  d26, d27
        vadd.i16    q0,  q1
        vadd.i16    q2,  q3
        vadd.i16    q0,  q2
        vadd.i16    d0,  d1       @ d0 has 4 vals
        mov         r1,  #8
        vpadd.i16   d0,  d0       @ 2 (top & bottom the same)
        add         r2,  r0,  r3
        vpadd.i16   d0,  d0       @ 1 (all the same)
        lsl         r3,  #1
        vrshrn.u16  d0,  q0,  #6
        vdup.u8     q1,  d0[0]    @ Dup results
        vdup.u8     q0,  d0[0]

        @ Store
1:
        vst1.8     {q0,  q1 }, [r0], r3
        vst1.8     {q0,  q1 }, [r2], r3
        subs        r1,  #1
        vst1.8     {q0,  q1 }, [r0], r3
        vst1.8     {q0,  q1 }, [r2], r3
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
        vbit        d0, d2, d7    @ top[0]+left[0], top[1..3], left[0..3]

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
        vbit        d0, d2, d7    @ top[0]+left[0], top[1..3], left[0..3]

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
        vld1.8     { q8, q9 }, [r1]
        vld1.8     {q12, q13}, [r2]
        vadd.i16    q8,  q9
        vadd.i16    q12, q13
        vadd.i16    q8,  q12
        vadd.i16    d16, d17      @ d16 has 2 pairs
        mov         r1,  #4
        vpadd.i32   d16, d16
        lsl         r3,  #2       @ stride in pels
        vrshr.u16   d16, #4
        vdup.u32    q9,  d16[0];
        vdup.u32    q8,  d16[0];

        @ Store
1:
        vst1.16     {q8,  q9 }, [r0], r3
        subs        r1,  #1
        vst1.16     {q8,  q9 }, [r0], r3
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
        vld1.16    {q8,  q9 }, [r1]
        vld1.16    {q12, q13}, [r2]
        lsl         r3,  #1       @ stride given in pels
        vadd.u16    q0,  q8,  q12
        vadd.u16    q2,  q9,  q13
        vmov.u16    r1,  d0[0]    @ r1 = top[0] + left[0]
        vadd.i16    q0,  q2
        vadd.i16    d0,  d1       @ d0 has 4 vals
        vpadd.i16   d0,  d0       @ 2 (top & bottom the same)
        vpadd.i16   d0,  d0       @ 1 (all the same)
        vrshr.u16   d0,  #5

        vmov.i64    d31, #0xffff

        @ top line gets some smoothing
        @ (top[i] + 3*dc + 2) >> 2
        @ top_line[0] is extra special
        @ (top[0] + left[0] + dc * 2)

        vmov.u16    r12, d0[0]   @ dc
        add         r2,  r12, r12, lsl #1  @ dc*3
        add         r1,  r1,  r12, lsl #1  @ top[0] + left[0] + dc*2

        vdup.u16    q3,  r2
        vadd.u16    q8,  q3
        vadd.u16    q9,  q3
        vmov.u16    d16[0], r1
        vrshr.u16   q8,  #2
        vrshr.u16   q9,  #2

        @ Construct lhs pels
        vadd.u16    q12, q3
        vadd.u16    q13, q3
        vrshr.u16   q12, #2
        vrshr.u16   q13, #2

        @ Store top line
        vst1.16    {q8,  q9 }, [r0], r3

        mov         r1,  #15
        vdup.u16    q1,  d0[0]
        vdup.u16    q0,  d0[0]

1:
        vext.16     q12, q13, #1
        vext.16     q13, q13, #1
        vbit        d0,  d24, d31
        subs        r1,  #1
        vst1.16    {q0,  q1 }, [r0], r3
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
        vldm        r1,  { q8-q11}
        vldm        r2,  {q12-q15}
        vadd.i16    q8,  q9
        vadd.i16    q10, q11
        vadd.i16    q12, q13
        vadd.i16    q14, q15
        vadd.i16    q8,  q10
        vadd.i16    q12, q14
        vadd.i16    q8,  q12
        vadd.i16    d16, d17      @ d16 has 2 pairs
        mov         r1,  #8
        vpadd.i32   d16, d16
        lsl         r3,  #2       @ stride in pels
        vrshr.u16   d16, #5
        vmov        d17, d16      @ Dup results
        vmov        q9,  q8
        vmov        q10, q8
        vmov        q11, q8

        @ Store
1:
        vstm        r0,  {q8-q11}
        add         r0,  r3
        subs        r1,  #1
        vstm        r0,  {q8-q11}
        add         r0,  r3
        bne          1b

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
        vldm        r1,  { q8-q11}
        vldm        r2,  {q12-q15}
        vadd.i16    q8,  q9
        vadd.i16    q10, q11
        vadd.i16    q12, q13
        vadd.i16    q14, q15
        vadd.i16    q8,  q10
        vadd.i16    q12, q14
        vadd.i16    q8,  q12
        vadd.i16    d16, d17      @ d16 has 4 vals
        mov         r1,  #16
        vpadd.i16   d16, d16      @ 2 (top & bottom the same)
        lsl         r3,  #1       @ stride in pels
        vpadd.i16   d16, d16      @ 1 (all the same)
        vrshr.u16   d16, #6
        vmov        d17, d16      @ Dup results
        vmov        q9,  q8
        vmov        q10, q8
        vmov        q11, q8

        @ Store
1:
        vstm        r0,  { q8-q11}
        add         r0,  r3
        subs        r1,  #1
        vstm        r0,  { q8-q11}
        add         r0,  r3
        bne          1b

        bx           lr
endfunc


