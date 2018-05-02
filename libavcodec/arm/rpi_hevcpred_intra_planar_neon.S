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

@ Planar intra pred (8.4.4.2.4)
@
@ predSamples[ x ][ y ] =
@ ( ( nTbS - 1 - x ) * p[ -1 ][ y ] +
@   ( x + 1 ) * p[ nTbS ][ -1 ] +
@   ( nTbS - 1 - y ) * p[ x ][ -1 ] +
@   ( y + 1 ) * p[ -1 ][ nTbS ] + nTbS ) >> ( Log2( nTbS ) + 1 )

@ ff_hevc_rpi_pred_planar_8_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_planar_4_neon_8, export=1
        adr         r12, nb_3_0_1_4
        vld1.8     {d24}, [r2]          @ Left
        vld1.8     {d0 }, [r1]          @ Up
        vld1.8     {q8 }, [r12 :128]    @ 3..

        vdup.8      d30, d24[4]
        vdup.8      d31, d0[4]

        vdup.32     d0,  d0[0]          @ copy lo -> hi
        vsubl.u8    q2,  d30, d0        @ Add set up

        vshll.u8    q0,  d0,  #2
        add         r1,  r0,  r3
        vmlal.u8    q0,  d17, d31       @ Acc set up - q8-q9 free

        vshl.i16    q3,  q2,  #1
        vadd.i16    d0,  d4
        vadd.i16    d1,  d6
        lsl         r3,  #1
        vadd.i16    q1,  q0,  q3

        vdup.u8     d20, d24[0]
        vdup.u8     d21, d24[1]
        vdup.u8     d22, d24[2]
        vdup.u8     d23, d24[3]

        vtrn.32     d20, d21
        vtrn.32     d22, d23

        vmull.u8    q10, d16, d20
        vmull.u8    q11, d16, d22
        vadd.i16    q10, q0
        vadd.i16    q11, q1

        vrshrn.u16  d28, q10, #3
        vrshrn.u16  d29, q11, #3

        vst1.32    {d28[0]}, [r0  :32], r3
        vst1.32    {d28[1]}, [r1  :32], r3
        vst1.32    {d29[0]}, [r0  :32]
        vst1.32    {d29[1]}, [r1  :32]

        bx          lr
endfunc


@ ff_hevc_rpi_pred_planar_8_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_planar_8_neon_8, export=1
        adr         r12, nb_7_0_1_8
        vld1.8     {q12}, [r2]          @ Left
        vld1.8     {q0 }, [r1]          @ Up
        vld1.8     {q8 }, [r12 :128]    @ 7..

        vdup.8      d30, d25[0]
        vdup.8      d31, d1[0]

        mov         r1,  #8
        vsubl.u8    q2,  d30, d0        @ Add set up

        vshll.u8    q0,  d0,  #3
        vmlal.u8    q0,  d17, d31       @ Acc set up - q8-q9 free

@ u8   7..0    [1]  d16
@ u8  left[y]  [1]  d24
@ u16 acc      [2]  q0 .. q1 = (x+1)*p[nTbS][-1] + 32*p[x][-1] initially
@ u16 add      [2]  q2 .. q3 = p[-1][nTbs] - p[x][-1]
1:
        vadd.i16    q0,  q2

        vdup.u8     d20, d24[0]
        vext.8      d24, d24, #1

        vmull.u8    q10, d16, d20
        vadd.i16    q10, q0

        vrshrn.u16  d28, q10, #4

        subs        r1,  #1
        vst1.8     {d28}, [r0  :64], r3

        bne         1b

        bx          lr

endfunc


@ ff_hevc_rpi_pred_planar_16_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_planar_16_neon_8, export=1
        vld1.8     {q12}, [r2  :128]    @ Left
        ldrb        r2,  [r2, #16]      @ Down left - could have this in q13, but avoid that much overrread
        adr         r12, nb_15_0_1_16
        vld1.8     {q0 }, [r1  :128]    @ Up
        ldrb        r1,  [r1, #16]      @ Up-right
        vld1.8     {q8,  q9 }, [r12 :128]  @ 15...

        vdup.8      d30, r2
        vdup.8      d31, r1

        mov         r1,  #16
        vsubl.u8    q3,  d30, d1
        vsubl.u8    q2,  d30, d0        @ Add set up

        vshll.u8    q1,  d1,  #4
        vshll.u8    q0,  d0,  #4
        vmlal.u8    q1,  d19, d31
        vmlal.u8    q0,  d18, d31       @ Acc set up - q8-q9 free

@ u8  15..0    [1]  q8
@ u8  left[y]  [1]  q12
@ u16 acc      [2]  q0 .. q1 = (x+1)*p[nTbS][-1] + 32*p[x][-1] initially
@ u16 add      [2]  q2 .. q3 = p[-1][nTbs] - p[x][-1]
1:
        vadd.i16    q1,  q3
        vadd.i16    q0,  q2

        vdup.u8     d20, d24[0]
        vext.8      q12, q12, #1

        vmull.u8    q11, d17, d20
        vmull.u8    q10, d16, d20

        vadd.i16    q11, q1
        vadd.i16    q10, q0

        vrshrn.u16  d29, q11, #5
        vrshrn.u16  d28, q10, #5

        subs        r1,  #1
        vst1.8     {q14}, [r0  :128], r3

        bne         1b

        bx          lr

endfunc


@ ff_hevc_rpi_pred_planar_32_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_planar_32_neon_8, export=1
        vpush      {q4-q7}
        vld1.8     {q12, q13}, [r2  :128]!    @ Left
        adr         r12, nb_31_0_1_32
        vld1.8     {q0,  q1 }, [r1  :128]!    @ Up
        vld1.8     {d30[0]}, [r2]       @ Down left
        vld1.8     {d31[0]}, [r1]       @ Up-right
        vldm        r12, { q8-q11}      @ 1..32, 31..0

        vdup.8      d30, d30[0]
        vdup.8      d31, d31[0]

        vsubl.u8    q7,  d30, d3
        vsubl.u8    q6,  d30, d2
        vsubl.u8    q5,  d30, d1
        vsubl.u8    q4,  d30, d0        @ Add set up

        vshll.u8    q3,  d3,  #5
        vshll.u8    q2,  d2,  #5
        vshll.u8    q1,  d1,  #5
        vshll.u8    q0,  d0,  #5
        vmlal.u8    q3,  d23, d31
        vmlal.u8    q2,  d22, d31
        vmlal.u8    q1,  d21, d31
        vmlal.u8    q0,  d20, d31       @ Acc set up - q8-q9 free

        mov         r1,  #32

@ u8  31..0    [2]  q10, q11
@ u8  left[y]  [2]  q12, q13
@ u16 acc      [4]  q0 .. q3 = (x+1)*p[nTbS][-1] + 32*p[x][-1] initially
@ u16 add      [4]  q4 .. q7 = p[-1][nTbs] - p[x][-1]
1:
        vadd.i16    q3,  q7
        vadd.i16    q2,  q6
        vadd.i16    q1,  q5
        vadd.i16    q0,  q4

        vdup.u8     d20, d24[0]
        vext.8      q12, q13, #1
        vext.8      q13, q13, #1

        vmull.u8    q15, d19, d20
        vmull.u8    q14, d18, d20
        vmull.u8    q11, d17, d20
        vmull.u8    q10, d16, d20

        vadd.i16    q15, q3
        vadd.i16    q14, q2
        vadd.i16    q11, q1
        vadd.i16    q10, q0

        vrshrn.u16  d31, q15, #6
        vrshrn.u16  d30, q14, #6
        vrshrn.u16  d29, q11, #6
        vrshrn.u16  d28, q10, #6

        subs        r1,  #1
        vst1.8     {q14, q15}, [r0  :128], r3

        bne         1b

        vpop       {q4-q7}
        bx          lr

endfunc


@ ff_hevc_rpi_pred_planar_c_4_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_planar_c_4_neon_8, export=1
        vld1.8     {q12}, [r2  :64]     @ Left + down-left - <1d of overread is OK
        adr         r12, nbx2_3_0_1_4
        vld1.8     {q0 }, [r1  :64]     @ Up + up right
        vld1.8     {q8 }, [r12 :128]    @ 3,3..

        vdup.16     d30, d25[0]
        vdup.16     d31, d1[0]

        mov         r1,  #4
        vsubl.u8    q2,  d30, d0        @ Add set up

        lsl         r3,  #1
        vshll.u8    q0,  d0,  #2
        vmlal.u8    q0,  d17, d31       @ Acc set up - q8-q9 free

@ u8  3,3..0,0 [1]  d16
@ u8  left[y]  [1]  d24
@ u16 acc      [1]  q0 = (x+1)*p[nTbS][-1] + 32*p[x][-1] initially
@ u16 add      [1]  q2 = p[-1][nTbs] - p[x][-1]
1:
        vadd.i16    q0,  q2

        vdup.u16    d20, d24[0]
        vext.16     d24, d24, #1

        vmull.u8    q10, d16, d20

        vadd.i16    q10, q0

        vrshrn.u16  d28, q10, #3

        subs        r1,  #1
        vst1.8     {d28}, [r0  :64], r3

        bne         1b

        bx          lr

endfunc


@ ff_hevc_rpi_pred_planar_c_8_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_planar_c_8_neon_8, export=1
        vld1.8     {q12}, [r2  :128]    @ Left
        ldrh        r2,  [r2, #16]      @ Down left - could have this in q13, but avoid that much overrread
        adr         r12, nbx2_7_0_1_8
        vld1.8     {q0 }, [r1  :128]    @ Up
        ldrh        r1,  [r1, #16]      @ Up-right
        vld1.8     {q8,  q9 }, [r12 :128]  @ 7,7...

        vdup.16     d30, r2
        vdup.16     d31, r1

        mov         r1,  #8
        vsubl.u8    q3,  d30, d1
        vsubl.u8    q2,  d30, d0        @ Add set up

        lsl         r3,  #1
        vshll.u8    q1,  d1,  #3
        vshll.u8    q0,  d0,  #3
        vmlal.u8    q1,  d19, d31
        vmlal.u8    q0,  d18, d31       @ Acc set up - q8-q9 free

@ u8  7,7..0,0 [1]  q8
@ u8  left[y]  [1]  q12
@ u16 acc      [2]  q0 .. q1 = (x+1)*p[nTbS][-1] + 32*p[x][-1] initially
@ u16 add      [2]  q2 .. q3 = p[-1][nTbs] - p[x][-1]
1:
        vadd.i16    q1,  q3
        vadd.i16    q0,  q2

        vdup.u16    d20, d24[0]
        vext.16     q12, q12, #1

        vmull.u8    q11, d17, d20
        vmull.u8    q10, d16, d20

        vadd.i16    q11, q1
        vadd.i16    q10, q0

        vrshrn.u16  d29, q11, #4
        vrshrn.u16  d28, q10, #4

        subs        r1,  #1
        vst1.8     {q14}, [r0  :128], r3

        bne         1b

        bx          lr

endfunc



@ ff_hevc_rpi_pred_planar_c_16_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_planar_c_16_neon_8, export=1
        vpush      {q4-q7}
        vld1.8     {q12, q13}, [r2  :128]!  @ Left
        adr         r12, nbx2_15_0_1_16
        vld1.8     {q0,  q1 }, [r1  :128]!  @ Up
        vld1.16    {d30[0]}, [r2]       @ Down left
        vld1.16    {d31[0]}, [r1]       @ Up-right
        vldm        r12, { q8-q11}      @ 1..32, 31..0

        vdup.16     d30, d30[0]
        vdup.16     d31, d31[0]

        mov         r1,  #16
        vsubl.u8    q7,  d30, d3
        vsubl.u8    q6,  d30, d2
        vsubl.u8    q5,  d30, d1
        vsubl.u8    q4,  d30, d0        @ Add set up

        lsl         r3,  #1
        vshll.u8    q3,  d3,  #4
        vshll.u8    q2,  d2,  #4
        vshll.u8    q1,  d1,  #4
        vshll.u8    q0,  d0,  #4
        vmlal.u8    q3,  d23, d31
        vmlal.u8    q2,  d22, d31
        vmlal.u8    q1,  d21, d31
        vmlal.u8    q0,  d20, d31       @ Acc set up - q8-q9 free

@ u8  31..0    [2]  q10, q11
@ u8  left[y]  [2]  q12, q13
@ u16 acc      [4]  q0 .. q3 = (x+1)*p[nTbS][-1] + 32*p[x][-1] initially
@ u16 add      [4]  q4 .. q7 = p[-1][nTbs] - p[x][-1]
1:
        vadd.i16    q3,  q7
        vadd.i16    q2,  q6
        vadd.i16    q1,  q5
        vadd.i16    q0,  q4

        vdup.u16    d20, d24[0]
        vext.16     q12, q13, #1
        vext.16     q13, q13, #1

        vmull.u8    q15, d19, d20
        vmull.u8    q14, d18, d20
        vmull.u8    q11, d17, d20
        vmull.u8    q10, d16, d20

        vadd.i16    q15, q3
        vadd.i16    q14, q2
        vadd.i16    q11, q1
        vadd.i16    q10, q0

        vrshrn.u16  d31, q15, #5
        vrshrn.u16  d30, q14, #5
        vrshrn.u16  d29, q11, #5
        vrshrn.u16  d28, q10, #5

        subs        r1,  #1
        vst1.8     {q14, q15}, [r0  :256], r3

        bne         1b

        vpop       {q4-q7}
        bx          lr

endfunc

@------------------------------------------------------------------------------
@
@ Data - put btween the 2 code lumps so we can reach it with an adr from both
@ Beware - it gets quite close which is why nb_3_0_1_4 is 1st...

        .text
        .balign 64

        @ These could be extracted from the above array, but separate out
        @ out for better (16 byte) alignment
nb_3_0_1_4:
        .byte    3,  2,  1,  0,  3,  2,  1,  0
        .byte    1,  2,  3,  4,  1,  2,  3,  4
nb_7_0_1_8:
        .byte    7,  6,  5,  4,  3,  2,  1,  0
        .byte    1,  2,  3,  4,  5,  6,  7,  8
nbh_3_0_1_4:
        .short   3,  2,  1,  0,  1,  2,  3,  4
nbx2_3_0_1_4:
        .byte    3,  3,  2,  2,  1,  1,  0,  0
        .byte    1,  1,  2,  2,  3,  3,  4,  4

        @ should be back on a 64-byte boundary here
nb_31_0_1_32:
        .byte   31, 30, 29, 28, 27, 26, 25, 24
        .byte   23, 22, 21, 20, 19, 18, 17, 16
nb_15_0_1_16:
        .byte   15, 14, 13, 12, 11, 10,  9,  8
        .byte    7,  6,  5,  4,  3,  2,  1,  0
        .byte    1,  2,  3,  4,  5,  6,  7,  8
        .byte    9, 10, 11, 12, 13, 14, 15, 16
        .byte   17, 18, 19, 20, 21, 22, 23, 24
        .byte   25, 26, 27, 28, 29, 30, 31, 32

        @ should be back on a 64-byte boundary here
nbx2_15_0_1_16:
        .byte   15, 15, 14, 14, 13, 13, 12, 12
        .byte   11, 11, 10, 10,  9,  9,  8,  8
nbx2_7_0_1_8:
        .byte    7,  7,  6,  6,  5,  5,  4,  4
        .byte    3,  3,  2,  2,  1,  1,  0,  0
        .byte    1,  1,  2,  2,  3,  3,  4,  4
        .byte    5,  5,  6,  6,  7,  7,  8,  8
        .byte    9,  9, 10, 10, 11, 11, 12, 12
        .byte   13, 13, 14, 14, 15, 15, 16, 16

@------------------------------------------------------------------------------
@
@ 10 bits
@ (all would work with 9)

@ ff_hevc_rpi_pred_planar_4_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_planar_4_neon_10, export=1
        @ Load from bytes & expand later - at the very least this uses less
        @ memory than having a short table
        adr         r12, nbh_3_0_1_4
        vld1.16    {q14}, [r2  :64]
        vld1.16    {q8 }, [r12 :128]    @ 3..0,1,..4
        vld1.16    {q12}, [r1  :64]     @ Up
        vdup.16     d2,   d29[0]

        lsl         r3,  #1
        vsub.i16    d4,  d2,  d24       @ Add set up

        vdup.16     d0,  d25[0]
        vshl.i16    d24, #2
        vmla.i16    d24, d17, d0        @ Acc set up
        add         r1,  r0,  r3
        vmov        d17, d16

        vadd.i16    d24, d4
        vadd.i16    d25, d24, d4
        vshl.i16    d4,  d4,  #1        @ x2
        lsl         r3,  #1
        vadd.i16    d26, d24, d4
        vadd.i16    d27, d25, d4

        vdup.16     d0,  d28[0]
        vdup.16     d1,  d28[1]
        vdup.16     d2,  d28[2]
        vdup.16     d3,  d28[3]

        vmul.i16    q0,  q8,  q0
        vmul.i16    q1,  q8,  q1
        vadd.i16    q0,  q12
        vadd.i16    q1,  q13

        vrshr.u16   q0,  #3
        vrshr.u16   q1,  #3

        vst1.16    {d0}, [r0], r3
        vst1.16    {d1}, [r1], r3
        vst1.16    {d2}, [r0]
        vst1.16    {d3}, [r1]

        bx         lr
endfunc


@ ff_hevc_rpi_pred_planar_8_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_planar_8_neon_10, export=1
        @ Load from bytes & expand later - at the very least this uses less
        @ memory than having a short table
        adr         r12, nb_7_0_1_8
        vld1.16    {q14}, [r2  :128]
        ldrh        r2,  [r2, #16]      @ Down left
        vld1.8     {q0 }, [r12 :128]    @ 7..0,1,..8
        vld1.16    {q12}, [r1  :128]    @ Up
        ldrh        r1,  [r1, #16]      @ Up-right
        vmovl.u8    q8,  d1
        vdup.16     q1,  r2
        vmovl.u8    q10, d0

        lsl         r3,  #1
        vsub.i16    q2,  q1,  q12       @ Add set up

        vdup.16     q0,  r1
        mov         r1,  #8
        vshl.i16    q12, #3
        vmla.i16    q12, q8,  q0        @ Acc set up - q8-q11 free

@ u16  15..0       [1]  q10
@ u32 left[y]      [1]  q14
@ u16 acc          [1]  q12 = (x+1)*p[nTbS][-1] + 32*p[x][-1] initially
@ u16 add          [1]   q2 = p[-1][nTbs] - p[x][-1]
1:
        vdup.16     q0,  d28[0]
        vext.16     q14, q14, #1

        vadd.i16    q12, q2

        vmul.i16    q0,  q10, q0
        vadd.i16    q0,  q12
        vrshr.u16   q0,  #4

        subs        r1,  #1
        vst1.16    {q0 }, [r0  :128], r3

        bne         1b

        bx         lr
endfunc


@ ff_hevc_rpi_pred_planar_16_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_planar_16_neon_10, export=1
        @ Load from bytes & expand later - at the very least this uses less
        @ memory than having a short table
        adr         r12, nb_15_0_1_16
        vld1.16    {q14, q15}, [r2  :128]
        ldrh        r2,  [r2, #32]      @ Down left
        vld1.8     {q0,  q1 }, [r12 :128]  @ 15..0,1,..16
        vld1.16    {q12, q13}, [r1  :128]  @ Up
        ldrh        r1,  [r1, #32]      @ Up-right
        vmovl.u8    q9,  d3
        vmovl.u8    q8,  d2
        vdup.16     q1,  r2
        vmovl.u8    q11, d1
        vmovl.u8    q10, d0

        lsl         r3,  #1
        vsub.i16    q3,  q1,  q13
        vsub.i16    q2,  q1,  q12       @ Add set up

        vdup.16     q0,  r1
        mov         r1,  #16
        vshl.i16    q13, #4
        vshl.i16    q12, #4
        vmla.i16    q13, q9,  q0
        vmla.i16    q12, q8,  q0        @ Acc set up - q8-q11 free

@ u16  15..0       [2]  q10..q11
@ u32 left[y]      [2]  q14..q15
@ u16 acc          [2]  q12..q13 = (x+1)*p[nTbS][-1] + 32*p[x][-1] initially
@ u16 add          [2]   q2..q3  = p[-1][nTbs] - p[x][-1]
1:
        vdup.16     q0,  d28[0]
        vext.16     q14, q15, #1
        vext.16     q15, q15, #1

        vadd.i16    q13, q3
        vadd.i16    q12, q2

        vmul.i16    q1,  q11, q0
        vmul.i16    q0,  q10, q0

        vadd.i16    q1,  q13
        vadd.i16    q0,  q12

        vrshr.u16   q1,  #5
        vrshr.u16   q0,  #5

        subs        r1,  #1
        vst1.16    {q0,  q1 }, [r0  :128], r3

        bne         1b

        bx         lr
endfunc


@ ff_hevc_rpi_pred_planar_32_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_planar_32_neon_10, export=1
        push       {r4, lr}
        @ Load from bytes & expand later - at the very least this uses less
        @ memory than having a short table
        adr         r12, nb_31_0_1_32
        vpush      { q4-q7 }
        vldm        r12, { q0-q3 }      @ 1..32, r12 points at 31..0
        vldm        r1!, {q12-q15}      @ Up
        ldrh        r12, [r2, #64]      @ Down left
        vmovl.u8    q8,  d4
        vmovl.u8    q9,  d5
        vmovl.u8    q10, d6
        vmovl.u8    q11, d7
        vdup.16     q3,  r12
        vld1.16    {d4[0]}, [r1]        @ Up-right

        vsub.i16    q7,  q3,  q15
        vsub.i16    q6,  q3,  q14
        vsub.i16    q5,  q3,  q13
        vsub.i16    q4,  q3,  q12       @ Add set up

        vshl.i16    q15, #5
        vshl.i16    q14, #5
        vshl.i16    q13, #5
        vshl.i16    q12, #5
        vmla.i16    q15, q11, d4[0]
        vmla.i16    q14, q10, d4[0]
        vmla.i16    q13, q9,  d4[0]
        vmla.i16    q12, q8,  d4[0]     @ Acc set up - q8-q11 free

        mov         r1,  #32
        vmovl.u8    q8,  d0
        vmovl.u8    q9,  d1
        vmovl.u8    q10, d2
        vmovl.u8    q11, d3

@ u8  31..0    [4]  q8..q11
@ u8  left[y]  [4]  [r2]
@ u16 acc      [4]  q12..q15 = (x+1)*p[nTbS][-1] + 32*p[x][-1] initially
@ u16 add      [4]  q4..q7 = p[-1][nTbs] - p[x][-1]
1:
        vld1.16    {d0[0]}, [r2]!

        vadd.i16    q15, q7
        vadd.i16    q14, q6
        vadd.i16    q13, q5
        vadd.i16    q12, q4

        vmul.i16    q3,  q11, d0[0]
        vmul.i16    q2,  q10, d0[0]
        vmul.i16    q1,  q9,  d0[0]
        vmul.i16    q0,  q8,  d0[0]

        vadd.i16    q3,  q15
        vadd.i16    q2,  q14
        vadd.i16    q1,  q13
        vadd.i16    q0,  q12

        vrshr.u16   q3,  #6
        vrshr.u16   q2,  #6
        vrshr.u16   q1,  #6
        vrshr.u16   q0,  #6

        subs        r1,  #1
        vstm        r0, { q0-q3 }
        add         r0,  r0,  r3,  lsl #1

        bne         1b

        vpop       {q4-q7}
        pop        {r4, pc}

endfunc

@ ff_hevc_rpi_pred_planar_c_8_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_planar_c_4_neon_10, export=1
        @ Load from bytes & expand later - at the very least this uses less
        @ memory than having a short table
        adr         r12, nbx2_3_0_1_4
        vld1.8     {q0 }, [r12 :128]    @ 3,3..0,0,1,1..4,4
        vld1.16    {q14}, [r2  :128]    @ left
        ldr         r12, [r2, #16]      @ Down left
        vld1.16    {q12}, [r1  :128]    @ Up
        vmovl.u8    q8,  d1
        vdup.32     q1,  r12
        ldr         r12, [r1, #16]      @ Up-right
        vmovl.u8    q10, d0

        lsl         r3,  #2
        vsub.i16    q2,  q1,  q12       @ Add set up

        mov         r1,  #4
        vdup.32     q0,  r12
        vshl.i16    q12, #2
        vmla.i16    q12, q8,  q0        @ Acc set up - q8-q11 free

@ u16  3,3..0,0    [1]  q10
@ u32 left[y]      [1]  q14
@ u16 acc          [1]  q12 = (x+1)*p[nTbS][-1] + 32*p[x][-1] initially
@ u16 add          [1]   q2 = p[-1][nTbs] - p[x][-1]
1:
        vdup.32     q0,  d28[0]
        vext.32     q14, q14, #1

        vadd.i16    q12, q2

        vmul.i16    q0,  q10, q0

        vadd.i16    q0,  q12

        vrshr.u16   q0,  #3

        subs        r1,  #1
        vst1.16    {q0 }, [r0  :128], r3

        bne         1b

        bx         lr
endfunc


@ ff_hevc_rpi_pred_planar_c_8_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_planar_c_8_neon_10, export=1
        @ Load from bytes & expand later - at the very least this uses less
        @ memory than having a short table
        adr         r12, nbx2_7_0_1_8
        vld1.8     {q0,  q1 }, [r12 :128]  @ 7,7..0,0,1,1..8,8
        vld1.16    {q14, q15}, [r2  :128]
        ldr         r12, [r2, #32]         @ Down left
        vld1.16    {q12, q13}, [r1  :128]  @ Up
        vmovl.u8    q9,  d3
        vmovl.u8    q8,  d2
        vdup.32     q1,  r12
        ldr         r12, [r1, #32]      @ Up-right
        vmovl.u8    q11, d1
        vmovl.u8    q10, d0

        lsl         r3,  #2
        vsub.i16    q3,  q1,  q13
        vsub.i16    q2,  q1,  q12       @ Add set up

        mov         r1,  #8
        vdup.32     q0,  r12
        vshl.i16    q13, #3
        vshl.i16    q12, #3
        vmla.i16    q13, q9,  q0
        vmla.i16    q12, q8,  q0        @ Acc set up - q8-q11 free

@ u16  7,7..0,0    [2]  q10..q11
@ u32 left[y]      [2]  q14..q15
@ u16 acc          [2]  q12..q13 = (x+1)*p[nTbS][-1] + 32*p[x][-1] initially
@ u16 add          [2]   q2..q3  = p[-1][nTbs] - p[x][-1]
1:
        vdup.32     q0,  d28[0]
        vext.32     q14, q15, #1
        vext.32     q15, q15, #1

        vadd.i16    q13, q3
        vadd.i16    q12, q2

        vmul.i16    q1,  q11, q0
        vmul.i16    q0,  q10, q0

        vadd.i16    q1,  q13
        vadd.i16    q0,  q12

        vrshr.u16   q1,  #4
        vrshr.u16   q0,  #4

        subs        r1,  #1
        vst1.16    {q0,  q1 }, [r0  :256], r3

        bne         1b

        bx         lr
endfunc


@ ff_hevc_rpi_pred_planar_c_16_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_planar_c_16_neon_10, export=1
        @ Load from bytes & expand later - at the very least this uses less
        @ memory than having a short table
        adr         r12, nbx2_15_0_1_16
        vpush      { q4-q7 }
        vldm        r12, { q0-q3 }      @ 1..32, r12 points at 31..0
        vldm        r1!, {q12-q15}      @ Up
        ldr         r12, [r2, #64]      @ Down left
        vmovl.u8    q11, d7
        vmovl.u8    q10, d6
        vmovl.u8    q9,  d5
        vmovl.u8    q8,  d4
        vdup.32     q3,  r12
        ldr         r12, [r1]           @ Up-right

        vsub.i16    q7,  q3,  q15
        vsub.i16    q6,  q3,  q14
        vsub.i16    q5,  q3,  q13
        vsub.i16    q4,  q3,  q12       @ Add set up

        vdup.32     q2,  r12
        vshl.i16    q15, #4
        vshl.i16    q14, #4
        vshl.i16    q13, #4
        vshl.i16    q12, #4
        vmla.i16    q15, q11, q2
        vmla.i16    q14, q10, q2
        vmla.i16    q13, q9,  q2
        vmla.i16    q12, q8,  q2        @ Acc set up - q8-q11 free

        mov         r1,  #16
        vmovl.u8    q11, d3
        vmovl.u8    q10, d2
        vmovl.u8    q9,  d1
        vmovl.u8    q8,  d0

@ u16  15,15..0,0  [4]  q8..q11
@ u32 left[y]      [4]  [r2]
@ u16 acc          [4]  q12..q15 = (x+1)*p[nTbS][-1] + 32*p[x][-1] initially
@ u16 add          [4]  q4..q7 = p[-1][nTbs] - p[x][-1]
1:
        ldr         r12, [r2], #4

        vadd.i16    q15, q7
        vadd.i16    q14, q6
        vdup.32     q0,  r12
        vadd.i16    q13, q5
        vadd.i16    q12, q4

        vmul.i16    q3,  q11, q0
        vmul.i16    q2,  q10, q0
        vmul.i16    q1,  q9,  q0
        vmul.i16    q0,  q8,  q0

        vadd.i16    q3,  q15
        vadd.i16    q2,  q14
        vadd.i16    q1,  q13
        vadd.i16    q0,  q12

        vrshr.u16   q3,  #5
        vrshr.u16   q2,  #5
        vrshr.u16   q1,  #5
        vrshr.u16   q0,  #5

        subs        r1,  #1
        vstm        r0, { q0-q3 }
        add         r0,  r0,  r3,  lsl #2

        bne         1b

        vpop       {q4-q7}
        bx         lr
endfunc


