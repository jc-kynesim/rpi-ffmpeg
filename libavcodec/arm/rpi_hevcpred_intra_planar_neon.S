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

@ Planar intra pred (8.4.4.2.4)
@
@ predSamples[ x ][ y ] =
@ ( ( nTbS - 1 - x ) * p[ -1 ][ y ] +
@   ( x + 1 ) * p[ nTbS ][ -1 ] +
@   ( nTbS - 1 - y ) * p[ x ][ -1 ] +
@   ( y + 1 ) * p[ -1 ][ nTbS ] + nTbS ) >> ( Log2( nTbS ) + 1 )

@ All 10-bit functions would work with 9


@ ff_hevc_rpi_pred_planar_8_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_planar_4_neon_8, export=1

        vld1.8      {d0}, [r1]          @ Top
        adr         ip, nb_3_0_1_4
        vld1.8      {d1}, [r2]          @ Left
        vmov.i64    d2, #0xffffffff
        vldr        d3, [ip, #8]        @ {1,2,3,4,1,2,3,4}
        add         r1, r0, r3
        vdup.32     d4, d0[0]           @ {t0,t1,t2,t3,t0,t1,t2,t3}
        vdup.8      d0, d0[4]           @ {t4,t4,t4,t4,t4,t4,t4,t4}
        vdup.8      d5, d1[4]           @ {l4,l4,l4,l4,l4,l4,l4,l4}
        vdup.8      d6, d1[0]           @ {l0,l0,l0,l0,l0,l0,l0,l0}
        vshll.u8    q8, d4, #2
        lsl         r3, #1
        vsubl.u8    q2, d5, d4
        vmlal.u8    q8, d0, d3
        vld1.8      {d0}, [ip]          @ {3,2,1,0,3,2,1,0}
        vdup.8      d7, d1[1]           @ {l1,l1,l1,l1,l1,l1,l1,l1}
        vshl.s16    q9, q2, #1
        vbif        d6, d7, d2          @ {l0,l0,l0,l0,l1,l1,l1,l1}
        vadd.i16    d16, d4
        vdup.8      d7, d1[2]           @ {l2,l2,l2,l2,l2,l2,l2,l2}
        vadd.i16    d17, d18
        vdup.8      d1, d1[3]           @ {l3,l3,l3,l3,l3,l3,l3,l3}
        vadd.i16    q2, q8, q9
        vmlal.u8    q8, d0, d6
        vbif        d7, d1, d2          @ {l2,l2,l2,l2,l3,l3,l3,l3}
        vmlal.u8    q2, d0, d7
        vrshrn.i16  d0, q8, #3
        vst1.32     d0[0], [r0 :32], r3
        vst1.32     d0[1], [r1 :32], r3
        vrshrn.i16  d0, q2, #3
        vst1.32     d0[0], [r0 :32]
        vst1.32     d0[1], [r1 :32]

        bx          lr
endfunc


@ ff_hevc_rpi_pred_planar_4_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_planar_4_neon_10, export=1
        @ Load from bytes & expand later - at the very least this uses less
        @ memory than having a short table
        vld1.16     {q0}, [r1 :64]      @ Top
        adr         ip, nbh_3_0_1_4
        vldr        d2, [r2, #8]        @ Left (lower)
        vldr        d3, [ip, #8]        @ {1,2,3,4}
T       lsl         r3, #1
        vshl.s16    d4, d0, #2
        vdup.16     d1, d1[0]           @ {t4,t4,t4,t4}
        vldr        d5, [r2]            @ Left (upper)
        vdup.16     d2, d2[0]           @ {l4,l4,l4,l4}
        vldr        d6, [ip]            @ {3,2,1,0}
        vmla.i16    d4, d3, d1          @ Acc set up
        vsub.i16    d0, d2, d0          @ Add set up
        vmov        d7, d6
        vdup.16     d2, d5[0]
        vdup.16     d3, d5[1]
        vdup.16     d16, d5[2]
        vadd.i16    d18, d0, d4
        vshl.s16    d0, #1              @ x2
        vadd.i16    d19, d0, d4
        vdup.16     d17, d5[3]
        vadd.i16    d4, d0, d18
A       add         r1, r0, r3, lsl #1
T       add         r1, r0, r3
        vadd.i16    d5, d0, d19
A       lsl         r3, #2
T       lsl         r3, #1
        vmla.i16    q9, q1, q3
        vmla.i16    q2, q8, q3
        vrshr.u16   q0, q9, #3
        vst1.16     {d0}, [r0], r3
        vrshr.u16   d2, d4, #3
        vst1.16     {d1}, [r1], r3
        vrshr.u16   d3, d5, #3
        vst1.16     {d2}, [r0]
        vst1.16     {d3}, [r1]

        bx         lr
endfunc


@ ff_hevc_rpi_pred_planar_8_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_planar_8_neon_8, export=1

        vld1.8      {q0}, [r1]          @ Top
        adr         ip, nb_7_0_1_8
        vldr        d2, [r2, #8]        @ Left (lower)
        mov         r1, #8
        vldr        d3, [ip, #8]        @ {1,2,3,4,5,6,7,8}
        vshll.u8    q2, d0, #3
        vdup.8      d1, d1[0]           @ {t8,t8,t8,t8,t8,t8,t8,t8}
        vdup.8      d2, d2[0]           @ {l8,l8,l8,l8,l8,l8,l8,l8}
        vldr        d6, [r2]            @ Left (upper)
        vmlal.u8    q2, d3, d1
        vsubl.u8    q0, d2, d0
        vldr        d7, [ip]            @ {7,6,5,4,3,2,1,0}

@ u8   7..0    [1]  d7
@ u8  left[y]  [1]  d6
@ u16 acc      [2]  q2 (even rows) or q8 (odd rows) = (x+1)*p[nTbS][-1] + 32*p[x][-1] initially
@ u16 add      [2]  q0 = p[-1][nTbs] - p[x][-1]

        vdup.8      d2, d6[0]
        vadd.i16    q2, q0
        vdup.8      d3, d6[1]
        vadd.i16    q8, q2, q0
1:
        vmlal.u8    q2, d7, d2
        subs        r1, #2
        vadd.i16    q9, q8, q0
        vmlal.u8    q8, d7, d3
        vdup.8      d2, d6[2]
        vdup.8      d3, d6[3]
        vrshrn.i16  d20, q2, #4
        vshr.u64    d6, #16
        vmov        q2, q9
        vst1.8      {d20}, [r0], r3
        vrshrn.i16  d20, q8, #4
        vadd.i16    q8, q2, q0
        vst1.8      {d20}, [r0], r3
        bne         1b

        bx          lr

endfunc


@ ff_hevc_rpi_pred_planar_8_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_planar_8_neon_10, export=1

        adr         ip, nb_7_0_1_8
        vld1.16     {q0}, [r1 :128]!    @ Top (left)
        lsl         r3, #1
        vld1.16     {q1}, [ip :128]     @ {7,6,5,4,3,2,1,0,1,2,3,4,5,6,7,8}
        add         ip, r2, #16
        vld1.16     {d4[],d5[]}, [r1]   @ Top (right)
        mov         r1, #8-2
        vshl.s16    q3, q0, #3
        vmovl.u8    q8, d3              @ {1,2,3,4,5,6,7,8}
        vld1.16     {d18[],d19[]}, [ip] @ Left (lower)
        vmla.i16    q3, q8, q2          @ Acc set up
        vsub.i16    q0, q9, q0          @ Add set up
        vmovl.u8    q1, d2              @ {7,6,5,4,3,2,1,0}
        vadd.i16    q2, q3, q0

@ u16  7..0        [1]  q1
@ u32 left[y]      [1]  [r2]
@ u16 acc          [1]  q3 = (x+1)*p[nTbS][-1] + 32*p[x][-1] initially
@ u16 add          [1]  q0 = p[-1][nTbs] - p[x][-1]

        vld1.16     {d6[],d7[]}, [r2]!
        vadd.i16    q8, q2, q0
        vld1.16     {d18[],d19[]}, [r2]!
        vmla.i16    q2, q1, q3
        vadd.i16    q3, q8, q0
        vmla.i16    q8, q1, q9
1:
        vrshr.u16   q9, q2, #4
        subs        r1, #2
        vmov        q2, q3
        vrshr.u16   q10, q8, #4
          vld1.16     {d6[],d7[]}, [r2]!
        vst1.16     {q9}, [r0 :128], r3
          vadd.i16    q8, q2, q0
          vld1.16     {d18[],d19[]}, [r2]!
          vmla.i16    q2, q1, q3
          vadd.i16    q3, q8, q0
          vmla.i16    q8, q1, q9
        vst1.16     {q10}, [r0 :128], r3
        bne         1b

        vrshr.u16   q9, q2, #4
        add         r3, r0
        vrshr.u16   q10, q8, #4
        vst1.16     {q9}, [r0 :128]
        vst1.16     {q10}, [r3 :128]

        bx         lr
endfunc


@------------------------------------------------------------------------------
@
@ Data - has to be in two lumps to ensure we can always reach using adr

        .balign 64

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

@------------------------------------------------------------------------------


@ ff_hevc_rpi_pred_planar_16_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_planar_16_neon_8, export=1

        adr         ip, nb_15_0_1_16 + 16
        vld1.8      {q0}, [r1 :128]!    @ Top (left)
        add         r2, #16
        vld1.8      {q1}, [ip: 128]     @ {1,2,3...16}
        vld1.8      {d4[]}, [r1]        @ Top (right)
        sub         ip, #16
        vshll.u8    q3, d0, #4
        mov         r1, #16
        vshll.u8    q8, d1, #4
        vld1.8      {d5[]}, [r2]        @ Left (lower)
        sub         r2, #16
        vmlal.u8    q3, d2, d4
        vmlal.u8    q8, d3, d4          @ Acc set up
        vsubl.u8    q1, d5, d0
        vsubl.u8    q0, d5, d1          @ Add set up
        vld1.8      {q2}, [ip :128]     @ {15,14,13...0}

@ u8  15..0    [1]  q2
@ u8  left[y]  [1]  [r2]
@ u16 acc      [2]  q3,q8 = (x+1)*p[nTbS][-1] + 32*p[x][-1] initially
@ u16 add      [2]  q1,q0 = p[-1][nTbs] - p[x][-1]

        vadd.i16    q3, q1
        vadd.i16    q8, q0
1:
        vadd.i16    q10, q3, q1
        subs        r1, #2
        vld1.8      {d18[]}, [r2]!
        vadd.i16    q11, q8, q0
        vld1.8      {d19[]}, [r2]!
        vmlal.u8    q3, d4, d18
        vmlal.u8    q8, d5, d18
        vadd.i16    q12, q10, q1
        vmlal.u8    q10, d4, d19
        vadd.i16    q13, q11, q0
        vmlal.u8    q11, d5, d19
        vrshrn.u16  d18, q3, #5
        vrshrn.u16  d19, q8, #5
        vmov        q3, q12
        vst1.8      {q9}, [r0 :128], r3
        vrshrn.u16  d18, q10, #5
        vrshrn.u16  d19, q11, #5
        vmov        q8, q13
        vst1.8      {q9}, [r0 :128], r3
        bne         1b

        bx          lr

endfunc


@ ff_hevc_rpi_pred_planar_16_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_planar_16_neon_10, export=1

        @ Load from bytes & expand later - at the very least this uses less
        @ memory than having a short table
        adr         ip, nb_15_0_1_16 + 16
        vld1.16     {q0-q1}, [r1 :128]! @ Top (left)
        add         r2, #32
        vld1.8      {q2}, [ip :128]     @ {1,2,3...16}
        lsl         r3, #1
        vld1.16     {d6[],d7[]}, [r1]   @ Top (right)
        sub         ip, #16
        vmovl.u8    q8, d4
        mov         r1, #16
        vshl.i16    q9, q0, #4
        vmovl.u8    q2, d5
        vshl.i16    q10, q1, #4
        vld1.16     {d22[],d23[]}, [r2] @ Left (lower)
        sub         r2, #32
        vld1.8      {q12}, [ip]         @ {15,14,13...0}
        vmla.i16    q9, q8, q3
        vmla.i16    q10, q2, q3         @ Acc set up
        vsub.i16    q0, q11, q0
        vsub.i16    q1, q11, q1         @ Add set up
        vadd.i16    q2, q9, q0
        vadd.i16    q3, q10, q1
        vmovl.u8    q8, d24
        vmovl.u8    q9, d25

@ u16  15..0       [2]  q8,q9
@ u32 left[y]      [2]  [r2]
@ u16 acc          [2]  q2,q3 = (x+1)*p[nTbS][-1] + 32*p[x][-1] initially
@ u16 add          [2]  q0,q1 = p[-1][nTbs] - p[x][-1]

1:
        vadd.i16    q10, q2, q0
        subs        r1, #2
        vld1.16     {d24[],d25[]}, [r2]!
        vadd.i16    q11, q3, q1
        vld1.16     {d28[],d29[]}, [r2]!
        vmla.i16    q2, q8, q12
        vmla.i16    q3, q9, q12
        vadd.i16    q12, q10, q0
        vmla.i16    q10, q8, q14
        vadd.i16    q13, q11, q1
        vmla.i16    q11, q9, q14
        vrshr.u16   q14, q2, #5
        vrshr.u16   q15, q3, #5
        vmov        q2, q12
        vst1.16     {q14-q15}, [r0 :128], r3
        vrshr.u16   q14, q10, #5
        vrshr.u16   q15, q11, #5
        vmov        q3, q13
        vst1.16     {q14-q15}, [r0 :128], r3
        bne         1b

        bx         lr
endfunc


@ ff_hevc_rpi_pred_planar_32_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_planar_32_neon_8, export=1

        vld1.8      {q0-q1}, [r1 :128]! @ Top (left)
        adr         ip, nb_31_0_1_32 + 32
        vpush       {d8-d12}
        vld1.8      {q2-q3}, [ip :128]  @ {1,2,3...32}
        add         r2, #32
        vld1.8      {d8[]}, [r1]        @ Top (right)
        sub         ip, #32
        vshll.u8    q8, d0, #5
        mov         r1, #32
        vld1.8      {d9[]}, [r2]        @ Left (lower)
        sub         r2, #32
        vshll.u8    q9, d1, #5
        vshll.u8    q10, d2, #5
        vshll.u8    q11, d3, #5
        vmlal.u8    q8, d4, d8
        vsubl.u8    q12, d9, d0
        vmlal.u8    q9, d5, d8
        vsubl.u8    q13, d9, d1
        vmlal.u8    q10, d6, d8
        vsubl.u8    q14, d9, d2
        vmlal.u8    q11, d7, d8         @ Acc set up
        vsubl.u8    q15, d9, d3         @ Add set up
        vadd.i16    q8, q12
        vadd.i16    q9, q13
        vadd.i16    q10, q14
        vadd.i16    q11, q15
        vld1.8      {q4-q5}, [ip :128]  @ {31,30,29...0}

@ u8  31..0    [2]  q4,q5
@ u8  left[y]  [2]  [r2]
@ u16 acc      [4]  q8-q11  = (x+1)*p[nTbS][-1] + 32*p[x][-1] initially
@ u16 add      [4]  q12-q15 = p[-1][nTbs] - p[x][-1]

        vld1.8      {d12[]}, [r2]!
        vadd.i16    q0, q8, q12
        b           2f
1:
          vld1.8      {d12[]}, [r2]!
        vrshrn.u16  d3, q1, #6
        vrshrn.u16  d2, q0, #6
          vadd.i16    q0, q8, q12
        vrshrn.u16  d4, q2, #6
        vrshrn.u16  d5, q3, #6
        vst1.8      {q1-q2}, [r0 :128], r3
2:        vadd.i16    q1, q9, q13
          subs        r1, #2
          vadd.i16    q2, q10, q14
          vadd.i16    q3, q11, q15
          vmlal.u8    q8, d8, d12
          vmlal.u8    q9, d9, d12
          vmlal.u8    q10, d10, d12
          vmlal.u8    q11, d11, d12
            vld1.8      {d12[]}, [r2]!
          vrshrn.u16  d19, q9, #6
          vrshrn.u16  d18, q8, #6
            vadd.i16    q8, q0, q12
          vrshrn.u16  d20, q10, #6
          vrshrn.u16  d21, q11, #6
          vst1.8      {q9-q10}, [r0 :128], r3
            vadd.i16    q9, q1, q13
            vadd.i16    q10, q2, q14
            vadd.i16    q11, q3, q15
            vmlal.u8    q0, d8, d12
            vmlal.u8    q1, d9, d12
            vmlal.u8    q2, d10, d12
            vmlal.u8    q3, d11, d12

        bne         1b

        vpop        {d8-d12}

        vrshrn.u16  d3, q1, #6
        vrshrn.u16  d2, q0, #6
        vrshrn.u16  d4, q2, #6
        vrshrn.u16  d5, q3, #6
        vst1.8      {q1-q2}, [r0 :128]

        bx          lr

endfunc


@ ff_hevc_rpi_pred_planar_32_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_planar_32_neon_10, export=1

        @ Load from bytes & expand later - at the very least this uses less
        @ memory than having a short table
        vld1.16     {q0-q1}, [r1 :128]!  @ Top (left)
        adr         ip, nb_31_0_1_32 + 32
        vpush       {q4-q7}
        vld1.16     {q2-q3}, [r1 :128]!  @ Top (centre)
        add         r2, #64
        vld1.8      {q14-q15}, [ip :128] @ {1,2,3...32}
T       lsl         r3, #1
        vld1.16     {d8[],d9[]}, [r1]    @ Top (right)
        sub         ip, #32
        vmovl.u8    q12, d28
        mov         r1, #32
        vmovl.u8    q13, d29
        vld1.8      {q6-q7}, [ip :128]   @ {31,30,29...0}
        vmovl.u8    q14, d30
        vmovl.u8    q15, d31
        vld1.16     {d10[],d11[]}, [r2]  @ Left (lower)
        sub         r2, #64
        vshl.i16    q8, q0, #5
        vshl.i16    q9, q1, #5
        vshl.i16    q10, q2, #5
        vshl.i16    q11, q3, #5
        vmla.i16    q8, q12, q4
        vsub.i16    q0, q5, q0
        vmla.i16    q9, q13, q4
        vsub.i16    q1, q5, q1
        vmla.i16    q10, q14, q4
        vmov.u16    ip, d0[0]
        vsub.i16    q2, q5, q2
        vmla.i16    q11, q15, q4         @ Acc set up
        vsub.i16    q3, q5, q3           @ Add set up
        vadd.i16    q8, q0
        vadd.i16    q9, q1
        vadd.i16    q10, q2
        vadd.i16    q11, q3
        vmovl.u8    q4, d12
        vmovl.u8    q5, d13
        vmovl.u8    q6, d14
        vmovl.u8    q7, d15

@ u16 31..0    [4]  q4-q7
@ u16 left[y]  [4]  [r2]
@ u16 acc      [4]  q8-q11 = (x+1)*p[nTbS][-1] + 32*p[x][-1] initially
@ u16 add      [4]  q0-q3  = p[-1][nTbs] - p[x][-1]

        vadd.i16    q12, q8, q0
A       sub         r0, r0, r3, lsl #1
T       sub         r0, r3
1:
        vld1.16     {d0[0]}, [r2]!
A       add         r0, r0, r3, lsl #1
T       add         r0, r3
        vadd.i16    q13, q9, q1
        subs        r1, #2
        vadd.i16    q14, q10, q2
        vadd.i16    q15, q11, q3
        vmla.i16    q8, q4, d0[0]
        vmla.i16    q9, q5, d0[0]
        vmla.i16    q10, q6, d0[0]
        vmla.i16    q11, q7, d0[0]
        vmov.16     d0[0], ip
        vrshr.u16   q8, #6
        vrshr.u16   q9, #6
        vrshr.u16   q10, #6
        vrshr.u16   q11, #6
        vstm        r0, {q8-q11}
        vadd.i16    q8, q12, q0
A       add         r0, r0, r3, lsl #1
T       add         r0, r3
        vld1.16     {d0[0]}, [r2]!
        vadd.i16    q9, q13, q1
        vadd.i16    q10, q14, q2
        vadd.i16    q11, q15, q3
        vmla.i16    q12, q4, d0[0]
        vmla.i16    q13, q5, d0[0]
        vmla.i16    q14, q6, d0[0]
        vmla.i16    q15, q7, d0[0]
        vmov.16     d0[0], ip
        vrshr.u16   q12, #6
        vrshr.u16   q13, #6
        vrshr.u16   q14, #6
        vrshr.u16   q15, #6
        vstm        r0, {q12-q15}
        vadd.i16    q12, q8, q0
        bne         1b

        vpop        {q4-q7}
        bx          lr

endfunc


@ ff_hevc_rpi_pred_planar_c_4_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_planar_c_4_neon_8, export=1

        vld1.8      {q0}, [r1]          @ Top
        adr         ip, nbx2_3_0_1_4
        vldr        d2, [r2, #8]        @ Left (lower)
        mov         r1, #4
        vldr        d3, [ip, #8]        @ {1,1,2,2,3,3,4,4}
        lsl         r3, #1
        vshll.u8    q2, d0, #2
        vdup.16     d1, d1[0]           @ {t4,t4,t4,t4,t4,t4,t4,t4}
        vdup.16     d2, d2[0]           @ {l4,l4,l4,l4,l4,l4,l4,l4}
        vldr        d6, [r2]            @ Left (upper)
        vmlal.u8    q2, d3, d1
        vsubl.u8    q0, d2, d0
        vldr        d7, [ip]            @ {3,3,2,2,1,1,0,0}

@ u8   3..0    [1]  d7
@ u8  left[y]  [1]  d6
@ u16 acc      [2]  q2 (even rows) or q8 (odd rows) = (x+1)*p[nTbS][-1] + 32*p[x][-1] initially
@ u16 add      [2]  q0 = p[-1][nTbs] - p[x][-1]

        vdup.16     d2, d6[0]
        vadd.i16    q2, q0
        vdup.16     d3, d6[1]
        vadd.i16    q8, q2, q0
1:
        vmlal.u8    q2, d7, d2
        subs        r1, #2
        vadd.i16    q9, q8, q0
        vmlal.u8    q8, d7, d3
        vdup.16     d2, d6[2]
        vdup.16     d3, d6[3]
        vrshrn.i16  d20, q2, #3
        vmov        q2, q9
        vst1.8      {d20}, [r0], r3
        vrshrn.i16  d20, q8, #3
        vadd.i16    q8, q2, q0
        vst1.8      {d20}, [r0], r3
        bne         1b

        bx          lr

endfunc


@ ff_hevc_rpi_pred_planar_c_4_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_planar_c_4_neon_10, export=1

        adr         ip, nbx2_3_0_1_4
        vld1.16     {q0}, [r1 :128]!    @ Top (left)
        lsl         r3, #2
        vld1.16     {q1}, [ip :128]     @ {3,3,2,2,1,1,0,0,1,1,2,2,3,3,4,4}
        add         ip, r2, #16
        vld1.32     {d4[],d5[]}, [r1]   @ Top (right)
        vshl.s16    q3, q0, #2
        vmovl.u8    q8, d3              @ {1,1,2,2,3,3,4,4}
        vld1.32     {d18[],d19[]}, [ip] @ Left (lower)
        vmla.i16    q3, q8, q2          @ Acc set up
        vsub.i16    q0, q9, q0          @ Add set up
        vmovl.u8    q1, d2              @ {3,3,2,2,1,1,0,0}
        vadd.i16    q2, q3, q0

@ u16  3..0        [1]  q1
@ u32 left[y]      [1]  [r2]
@ u16 acc          [1]  q3 = (x+1)*p[nTbS][-1] + 32*p[x][-1] initially
@ u16 add          [1]  q0 = p[-1][nTbs] - p[x][-1]

        vld1.32     {d6[],d7[]}, [r2]!
        vadd.i16    q8, q2, q0
        vld1.32     {d18[],d19[]}, [r2]!
        vmla.i16    q2, q1, q3
        vadd.i16    q3, q8, q0
        vmla.i16    q8, q1, q9

        vrshr.u16   q9, q2, #3
        vmov        q2, q3
        vrshr.u16   q10, q8, #3
          vld1.32     {d6[],d7[]}, [r2]!
        vst1.16     {q9}, [r0 :128], r3
          vadd.i16    q8, q2, q0
          vld1.32     {d18[],d19[]}, [r2]!
          vmla.i16    q2, q1, q3
          vadd.i16    q3, q8, q0
          vmla.i16    q8, q1, q9
        vst1.16     {q10}, [r0 :128], r3

          vrshr.u16   q9, q2, #3
          add         r3, r0
          vrshr.u16   q10, q8, #3
          vst1.16     {q9}, [r0 :128]
          vst1.16     {q10}, [r3 :128]

          bx         lr
endfunc


@ ff_hevc_rpi_pred_planar_c_8_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_planar_c_8_neon_8, export=1

        adr         ip, nbx2_7_0_1_8 + 16
        vld1.8      {q0}, [r1 :128]!    @ Top (left)
        add         r2, #16
        vld1.8      {q1}, [ip: 128]     @ {1,1,2,2,3,3...8,8}
        lsl         r3, #1
        vld1.16     {d4[]}, [r1]        @ Top (right)
        sub         ip, #16
        vshll.u8    q3, d0, #3
        mov         r1, #8
        vshll.u8    q8, d1, #3
        vld1.16     {d5[]}, [r2]        @ Left (lower)
        sub         r2, #16
        vmlal.u8    q3, d2, d4
        vmlal.u8    q8, d3, d4          @ Acc set up
        vsubl.u8    q1, d5, d0
        vsubl.u8    q0, d5, d1          @ Add set up
        vld1.8      {q2}, [ip :128]     @ {7,7,6,6,5,5...0,0}

@ u8  7..0     [1]  q2
@ u8  left[y]  [1]  [r2]
@ u16 acc      [2]  q3,q8 = (x+1)*p[nTbS][-1] + 32*p[x][-1] initially
@ u16 add      [2]  q1,q0 = p[-1][nTbs] - p[x][-1]

        vadd.i16    q3, q1
        vadd.i16    q8, q0
1:
        vadd.i16    q10, q3, q1
        subs        r1, #2
        vld1.16     {d18[]}, [r2]!
        vadd.i16    q11, q8, q0
        vld1.16     {d19[]}, [r2]!
        vmlal.u8    q3, d4, d18
        vmlal.u8    q8, d5, d18
        vadd.i16    q12, q10, q1
        vmlal.u8    q10, d4, d19
        vadd.i16    q13, q11, q0
        vmlal.u8    q11, d5, d19
        vrshrn.u16  d18, q3, #4
        vrshrn.u16  d19, q8, #4
        vmov        q3, q12
        vst1.8      {q9}, [r0 :128], r3
        vrshrn.u16  d18, q10, #4
        vrshrn.u16  d19, q11, #4
        vmov        q8, q13
        vst1.8      {q9}, [r0 :128], r3
        bne         1b

        bx          lr

endfunc


@------------------------------------------------------------------------------
@
@ Data - has to be in two lumps to ensure we can always reach using adr

        .balign 64

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

        @ should be back on a 64-byte boundary here

nbx2_3_0_1_4:
        .byte    3,  3,  2,  2,  1,  1,  0,  0
        .byte    1,  1,  2,  2,  3,  3,  4,  4

@------------------------------------------------------------------------------


@ ff_hevc_rpi_pred_planar_c_8_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_planar_c_8_neon_10, export=1

        @ Load from bytes & expand later - at the very least this uses less
        @ memory than having a short table
        adr         ip, nbx2_7_0_1_8 + 16
        vld1.16     {q0-q1}, [r1 :128]! @ Top (left)
        add         r2, #32
        vld1.8      {q2}, [ip :128]     @ {1,1,2,2,3,3...8,8}
        lsl         r3, #2
        vld1.32     {d6[],d7[]}, [r1]   @ Top (right)
        sub         ip, #16
        vmovl.u8    q8, d4
        mov         r1, #8
        vshl.i16    q9, q0, #3
        vmovl.u8    q2, d5
        vshl.i16    q10, q1, #3
        vld1.32     {d22[],d23[]}, [r2] @ Left (lower)
        sub         r2, #32
        vld1.8      {q12}, [ip]         @ {7,7,6,6,5,5...0,0}
        vmla.i16    q9, q8, q3
        vmla.i16    q10, q2, q3         @ Acc set up
        vsub.i16    q0, q11, q0
        vsub.i16    q1, q11, q1         @ Add set up
        vadd.i16    q2, q9, q0
        vadd.i16    q3, q10, q1
        vmovl.u8    q8, d24
        vmovl.u8    q9, d25

@ u16  7..0        [2]  q8,q9
@ u32 left[y]      [2]  [r2]
@ u16 acc          [2]  q2,q3 = (x+1)*p[nTbS][-1] + 32*p[x][-1] initially
@ u16 add          [2]  q0,q1 = p[-1][nTbs] - p[x][-1]

1:
        vadd.i16    q10, q2, q0
        subs        r1, #2
        vld1.32     {d24[],d25[]}, [r2]!
        vadd.i16    q11, q3, q1
        vld1.32     {d28[],d29[]}, [r2]!
        vmla.i16    q2, q8, q12
        vmla.i16    q3, q9, q12
        vadd.i16    q12, q10, q0
        vmla.i16    q10, q8, q14
        vadd.i16    q13, q11, q1
        vmla.i16    q11, q9, q14
        vrshr.u16   q14, q2, #4
        vrshr.u16   q15, q3, #4
        vmov        q2, q12
        vst1.16     {q14-q15}, [r0 :128], r3
        vrshr.u16   q14, q10, #4
        vrshr.u16   q15, q11, #4
        vmov        q3, q13
        vst1.16     {q14-q15}, [r0 :128], r3
        bne         1b

        bx         lr
endfunc


@ ff_hevc_rpi_pred_planar_c_16_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_planar_c_16_neon_8, export=1

        vld1.8      {q0-q1}, [r1 :128]! @ Top (left)
        adr         ip, nbx2_15_0_1_16 + 32
        vpush       {d8-d12}
        vld1.8      {q2-q3}, [ip :128]  @ {1,1,2,2,3,3...16,16}
        add         r2, #32
        vld1.16     {d8[]}, [r1]        @ Top (right)
        sub         ip, #32
        vshll.u8    q8, d0, #4
        mov         r1, #16
        vld1.16     {d9[]}, [r2]        @ Left (lower)
        sub         r2, #32
        vshll.u8    q9, d1, #4
        lsl         r3, #1
        vshll.u8    q10, d2, #4
        vshll.u8    q11, d3, #4
        vmlal.u8    q8, d4, d8
        vsubl.u8    q12, d9, d0
        vmlal.u8    q9, d5, d8
        vsubl.u8    q13, d9, d1
        vmlal.u8    q10, d6, d8
        vsubl.u8    q14, d9, d2
        vmlal.u8    q11, d7, d8         @ Acc set up
        vsubl.u8    q15, d9, d3         @ Add set up
        vadd.i16    q8, q12
        vadd.i16    q9, q13
        vadd.i16    q10, q14
        vadd.i16    q11, q15
        vld1.8      {q4-q5}, [ip :128]  @ {15,15,14,14,13,13...0,0}

@ u8  15..0    [2]  q4,q5
@ u8  left[y]  [2]  [r2]
@ u16 acc      [4]  q8-q11  = (x+1)*p[nTbS][-1] + 32*p[x][-1] initially
@ u16 add      [4]  q12-q15 = p[-1][nTbs] - p[x][-1]

        vld1.16     {d12[]}, [r2]!
        vadd.i16    q0, q8, q12
        b           2f
1:
          vld1.16     {d12[]}, [r2]!
        vrshrn.u16  d3, q1, #5
        vrshrn.u16  d2, q0, #5
          vadd.i16    q0, q8, q12
        vrshrn.u16  d4, q2, #5
        vrshrn.u16  d5, q3, #5
        vst1.8      {q1-q2}, [r0 :128], r3
2:        vadd.i16    q1, q9, q13
          subs        r1, #2
          vadd.i16    q2, q10, q14
          vadd.i16    q3, q11, q15
          vmlal.u8    q8, d8, d12
          vmlal.u8    q9, d9, d12
          vmlal.u8    q10, d10, d12
          vmlal.u8    q11, d11, d12
            vld1.16     {d12[]}, [r2]!
          vrshrn.u16  d19, q9, #5
          vrshrn.u16  d18, q8, #5
            vadd.i16    q8, q0, q12
          vrshrn.u16  d20, q10, #5
          vrshrn.u16  d21, q11, #5
          vst1.8      {q9-q10}, [r0 :128], r3
            vadd.i16    q9, q1, q13
            vadd.i16    q10, q2, q14
            vadd.i16    q11, q3, q15
            vmlal.u8    q0, d8, d12
            vmlal.u8    q1, d9, d12
            vmlal.u8    q2, d10, d12
            vmlal.u8    q3, d11, d12

        bne         1b

        vpop        {d8-d12}

        vrshrn.u16  d3, q1, #5
        vrshrn.u16  d2, q0, #5
        vrshrn.u16  d4, q2, #5
        vrshrn.u16  d5, q3, #5
        vst1.8      {q1-q2}, [r0 :128]

        bx          lr

endfunc


@ ff_hevc_rpi_pred_planar_c_16_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride)       [r3]

function ff_hevc_rpi_pred_planar_c_16_neon_10, export=1

        @ Load from bytes & expand later - at the very least this uses less
        @ memory than having a short table
        vld1.16     {q0-q1}, [r1 :128]!  @ Top (left)
        adr         ip, nbx2_15_0_1_16 + 32
        vpush       {q4-q7}
        vld1.16     {q2-q3}, [r1 :128]!  @ Top (centre)
        add         r2, #64
        vld1.8      {q14-q15}, [ip :128] @ {1,1,2,2,3,3...16,16}
T       lsl         r3, #2
        vld1.32     {d8[],d9[]}, [r1]    @ Top (right)
        sub         ip, #32
        vmovl.u8    q12, d28
        mov         r1, #16
        vmovl.u8    q13, d29
        vld1.8      {q6-q7}, [ip :128]   @ {15,15,14,14,13,13...0,0}
        vmovl.u8    q14, d30
        vmovl.u8    q15, d31
        vld1.32     {d10[],d11[]}, [r2]  @ Left (lower)
        sub         r2, #64
        vshl.i16    q8, q0, #4
        vshl.i16    q9, q1, #4
        vshl.i16    q10, q2, #4
        vshl.i16    q11, q3, #4
        vmla.i16    q8, q12, q4
        vsub.i16    q0, q5, q0
        vmla.i16    q9, q13, q4
        vpush       {q0}
        vsub.i16    q1, q5, q1
        vmla.i16    q10, q14, q4
        vsub.i16    q2, q5, q2
        vmla.i16    q11, q15, q4         @ Acc set up
        vsub.i16    q3, q5, q3           @ Add set up
        vadd.i16    q8, q0
        vadd.i16    q9, q1
        vadd.i16    q10, q2
        vadd.i16    q11, q3
        vmovl.u8    q4, d12
        vmovl.u8    q5, d13
        vmovl.u8    q6, d14
        vmovl.u8    q7, d15

@ u16 31..0    [4]  q4-q7
@ u16 left[y]  [4]  [r2]
@ u16 acc      [4]  q8-q11 = (x+1)*p[nTbS][-1] + 32*p[x][-1] initially
@ u16 add      [4]  q0-q3  = p[-1][nTbs] - p[x][-1]

        vadd.i16    q12, q8, q0
A       sub         r0, r0, r3, lsl #2
T       sub         r0, r3
1:
        vld1.32     {d0[],d1[]}, [r2]!
A       add         r0, r0, r3, lsl #2
T       add         r0, r3
        vadd.i16    q13, q9, q1
        subs        r1, #2
        vadd.i16    q14, q10, q2
        vadd.i16    q15, q11, q3
        vmla.i16    q8, q4, q0
        vmla.i16    q9, q5, q0
        vmla.i16    q10, q6, q0
        vmla.i16    q11, q7, q0
        vld1.16     {q0}, [sp]
        vrshr.u16   q8, #5
        vrshr.u16   q9, #5
        vrshr.u16   q10, #5
        vrshr.u16   q11, #5
        vstm        r0, {q8-q11}
        vadd.i16    q8, q12, q0
A       add         r0, r0, r3, lsl #2
T       add         r0, r3
        vld1.32     {d0[],d1[]}, [r2]!
        vadd.i16    q9, q13, q1
        vadd.i16    q10, q14, q2
        vadd.i16    q11, q15, q3
        vmla.i16    q12, q4, q0
        vmla.i16    q13, q5, q0
        vmla.i16    q14, q6, q0
        vmla.i16    q15, q7, q0
        vld1.16     {q0}, [sp]
        vrshr.u16   q12, #5
        vrshr.u16   q13, #5
        vrshr.u16   q14, #5
        vrshr.u16   q15, #5
        vstm        r0, {q12-q15}
        vadd.i16    q12, q8, q0
        bne         1b

        vpop        {q3-q7}
        bx          lr

endfunc
