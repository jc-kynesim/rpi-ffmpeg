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

Authors: John Cox
*/

#include "libavutil/arm/asm.S"


@ General notes:
@ Having done some timing on this in sand8->y8 (Pi4)
@  vst1 (680fps) is a bit faster than vstm (660fps)
@  vldm (680fps) is noticably faster than vld1 (480fps)
@  (or it might be that a mix is what is required)
@
@ At least on a Pi4 it is no more expensive to have a single auto-inc register
@ for dest address than it is to have 2 used alternately (On Pi3 Ben asserted
@ the latter was better)
@
@ vstm will bus error on unaligned access (so will vldm), vst1 is safe unless
@ the memory is uncached.
@ As these are Sand -> planar we can assume that src is going to be aligned but
@ it is possible that dest isn't (converting to .yuv or other packed format).
@ Luckily vst1 is faster than vstm :-) so all is well
@ vst1 has alignment requirements of el size so maybe splitting vst1.32 into 4
@ .8 stores would let us do non-word aligned stores into uncached but it
@ probably isn't worth it.




@ void ff_rpi_sand128b_stripe_to_8_10(
@   uint8_t * dest,             // [r0]
@   const uint8_t * src1,       // [r1]
@   const uint8_t * src2,       // [r2]
@   unsigned int lines);        // [r3]

.macro  stripe2_to_8, bit_depth
        vpush    {q4-q7}
1:
        vldm     r1!, {q0-q7}
        subs     r3, #1
        vldm     r2!, {q8-q15}
        vqrshrn.u16 d0,  q0,  #\bit_depth - 8
        vqrshrn.u16 d1,  q1,  #\bit_depth - 8
        vqrshrn.u16 d2,  q2,  #\bit_depth - 8
        vqrshrn.u16 d3,  q3,  #\bit_depth - 8
        vqrshrn.u16 d4,  q4,  #\bit_depth - 8
        vqrshrn.u16 d5,  q5,  #\bit_depth - 8
        vqrshrn.u16 d6,  q6,  #\bit_depth - 8
        vqrshrn.u16 d7,  q7,  #\bit_depth - 8
        vqrshrn.u16 d8,  q8,  #\bit_depth - 8
        vqrshrn.u16 d9,  q9,  #\bit_depth - 8
        vqrshrn.u16 d10, q10, #\bit_depth - 8
        vqrshrn.u16 d11, q11, #\bit_depth - 8
        vqrshrn.u16 d12, q12, #\bit_depth - 8
        vqrshrn.u16 d13, q13, #\bit_depth - 8
        vqrshrn.u16 d14, q14, #\bit_depth - 8
        vqrshrn.u16 d15, q15, #\bit_depth - 8
        vstm     r0!, {q0-q7}
        bne      1b
        vpop     {q4-q7}
        bx       lr
.endm

function ff_rpi_sand128b_stripe_to_8_10, export=1
        stripe2_to_8     10
endfunc

@ void ff_rpi_sand8_lines_to_planar_y8(
@   uint8_t * dest,             // [r0]
@   unsigned int dst_stride,    // [r1]
@   const uint8_t * src,        // [r2]
@   unsigned int src_stride1,   // [r3]      Ignored - assumed 128
@   unsigned int src_stride2,   // [sp, #0]  -> r3
@   unsigned int _x,            // [sp, #4]  Ignored - 0
@   unsigned int y,             // [sp, #8]  (r7 in prefix)
@   unsigned int _w,            // [sp, #12] -> r6 (cur r5)
@   unsigned int h);            // [sp, #16] -> r7
@
@ Assumes that we are starting on a stripe boundary and that overreading
@ within the stripe is OK. However it does respect the dest size for writing

function ff_rpi_sand8_lines_to_planar_y8, export=1
                push            {r4-r8, lr}     @ +24            L
                ldr             r3,  [sp, #24]
                ldr             r6,  [sp, #36]
                ldr             r7,  [sp, #32]  @ y
                lsl             r3,  #7
                sub             r1,  r6
                add             r8,  r2,  r7,  lsl #7
                ldr             r7,  [sp, #40]

10:
                mov             r2,  r8
                add             r4,  r0,  #24
                mov             r5,  r6
                mov             lr,  #0
1:
                vldm            r2,  {q8-q15}
                add             r2,  r3
                subs            r5,  #128
                blt             2f
                vst1.8          {d16, d17, d18, d19}, [r0]!
                vst1.8          {d20, d21, d22, d23}, [r0]!
                vst1.8          {d24, d25, d26, d27}, [r0]!
                vst1.8          {d28, d29, d30, d31}, [r0]!
                bne             1b
11:
                subs            r7,  #1
                add             r0,  r1
                add             r8,  #128
                bne             10b

                pop             {r4-r8, pc}

@ Partial final write
2:
                cmp             r5,  #64-128
                blt             1f
                vst1.8          {d16, d17, d18, d19}, [r0]!
                vst1.8          {d20, d21, d22, d23}, [r0]!
                beq             11b
                vmov            q8,  q12
                vmov            q9,  q13
                sub             r5,  #64
                vmov            q10, q14
                vmov            q11, q15
1:
                cmp             r5,  #32-128
                blt             1f
                vst1.8          {d16, d17, d18, d19}, [r0]!
                beq             11b
                vmov            q8,  q10
                sub             r5,  #32
                vmov            q9,  q11
1:
                cmp             r5,  #16-128
                blt             1f
                vst1.8          {d16, d17}, [r0]!
                beq             11b
                sub             r5,  #16
                vmov            q8,  q9
1:
                cmp             r5,  #8-128
                blt             1f
                vst1.8          {d16}, [r0]!
                beq             11b
                sub             r5,  #8
                vmov            d16, d17
1:
                cmp             r5,  #4-128
                blt             1f
                vst1.32         {d16[0]}, [r0]!
                beq             11b
                sub             r5,  #4
                vshr.u64        d16, #32
1:
                cmp             r5,  #2-128
                blt             1f
                vst1.16         {d16[0]}, [r0]!
                beq             11b
                vst1.8          {d16[2]}, [r0]!
                b               11b
1:
                vst1.8          {d16[0]}, [r0]!
                b               11b
endfunc

@ void ff_rpi_sand8_lines_to_planar_c8(
@   uint8_t * dst_u,            // [r0]
@   unsigned int dst_stride_u,  // [r1]
@   uint8_t * dst_v,            // [r2]
@   unsigned int dst_stride_v,  // [r3]
@   const uint8_t * src,        // [sp, #0]  -> r4, r5
@   unsigned int stride1,       // [sp, #4]  128
@   unsigned int stride2,       // [sp, #8]  -> r8
@   unsigned int _x,            // [sp, #12] 0
@   unsigned int y,             // [sp, #16] (r7 in prefix)
@   unsigned int _w,            // [sp, #20] -> r12, r6
@   unsigned int h);            // [sp, #24] -> r7
@
@ Assumes that we are starting on a stripe boundary and that overreading
@ within the stripe is OK. However it does respect the dest size for writing

function ff_rpi_sand8_lines_to_planar_c8, export=1
                push            {r4-r8, lr}     @ +24

                ldr             r5,  [sp, #24]
                ldr             r8,  [sp, #32]
                ldr             r7,  [sp, #40]
                ldr             r6,  [sp, #44]
                lsl             r8,  #7
                add             r5,  r5,  r7,  lsl #7
                sub             r1,  r1,  r6
                sub             r3,  r3,  r6
                ldr             r7,  [sp, #48]
                vpush           {q4-q7}

10:
                mov             r4,  r5
                mov             r12, r6
1:
                subs            r12, #64
                vldm            r4,  {q0-q7}
                add             r4,  r8
                it              gt
                vldmgt          r4,  {q8-q15}
                add             r4,  r8

                vuzp.8          q0,  q1
                vuzp.8          q2,  q3
                vuzp.8          q4,  q5
                vuzp.8          q6,  q7

                vuzp.8          q8,  q9
                vuzp.8          q10, q11
                vuzp.8          q12, q13
                vuzp.8          q14, q15
                subs            r12, #64

                @ Rearrange regs so we can use vst1 with 4 regs
                vswp            q1,  q2
                vswp            q5,  q6
                vswp            q9,  q10
                vswp            q13, q14
                blt             2f

                vst1.8          {d0,  d1,  d2,  d3 }, [r0]!
                vst1.8          {d8,  d9,  d10, d11}, [r0]!
                vst1.8          {d16, d17, d18, d19}, [r0]!
                vst1.8          {d24, d25, d26, d27}, [r0]!

                vst1.8          {d4,  d5,  d6,  d7 }, [r2]!
                vst1.8          {d12, d13, d14, d15}, [r2]!
                vst1.8          {d20, d21, d22, d23}, [r2]!
                vst1.8          {d28, d29, d30, d31}, [r2]!
                bne             1b
11:
                subs            r7,  #1
                add             r5,  #128
                add             r0,  r1
                add             r2,  r3
                bne             10b
                vpop            {q4-q7}
                pop             {r4-r8,pc}

2:
                cmp             r12, #64-128
                blt             1f
                vst1.8          {d0,  d1,  d2,  d3 }, [r0]!
                vst1.8          {d8,  d9,  d10, d11}, [r0]!
                vst1.8          {d4,  d5,  d6,  d7 }, [r2]!
                vst1.8          {d12, d13, d14, d15}, [r2]!
                beq             11b
                sub             r12, #64
                vmov            q0,  q8
                vmov            q1,  q9
                vmov            q2,  q10
                vmov            q3,  q11
                vmov            q4,  q12
                vmov            q5,  q13
                vmov            q6,  q14
                vmov            q7,  q15
1:
                cmp             r12, #32-128
                blt             1f
                vst1.8          {d0,  d1,  d2,  d3 }, [r0]!
                vst1.8          {d4,  d5,  d6,  d7 }, [r2]!
                beq             11b
                sub             r12, #32
                vmov            q0,  q4
                vmov            q1,  q5
                vmov            q2,  q6
                vmov            q3,  q7
1:
                cmp             r12, #16-128
                blt             1f
                vst1.8          {d0,  d1 }, [r0]!
                vst1.8          {d4,  d5 }, [r2]!
                beq             11b
                sub             r12, #16
                vmov            q0,  q1
                vmov            q2,  q3
1:
                cmp             r12, #8-128
                blt             1f
                vst1.8          {d0}, [r0]!
                vst1.8          {d4}, [r2]!
                beq             11b
                sub             r12, #8
                vmov            d0,  d1
                vmov            d4,  d5
1:
                cmp             r12, #4-128
                blt             1f
                vst1.32         {d0[0]}, [r0]!
                vst1.32         {d4[0]}, [r2]!
                beq             11b
                sub             r12, #4
                vmov            s0,  s1
                vmov            s8,  s9
1:
                cmp             r12, #2-128
                blt             1f
                vst1.16         {d0[0]}, [r0]!
                vst1.16         {d4[0]}, [r2]!
                beq             11b
                vst1.8          {d0[2]}, [r0]!
                vst1.8          {d4[2]}, [r2]!
                b               11b
1:
                vst1.8          {d0[0]}, [r0]!
                vst1.8          {d4[0]}, [r2]!
                b               11b
endfunc



@ void ff_rpi_sand30_lines_to_planar_y16(
@   uint8_t * dest,             // [r0]
@   unsigned int dst_stride,    // [r1]
@   const uint8_t * src,        // [r2]
@   unsigned int src_stride1,   // [r3]      Ignored - assumed 128
@   unsigned int src_stride2,   // [sp, #0]  -> r3
@   unsigned int _x,            // [sp, #4]  Ignored - 0
@   unsigned int y,             // [sp, #8]  (r7 in prefix)
@   unsigned int _w,            // [sp, #12] -> r6 (cur r5)
@   unsigned int h);            // [sp, #16] -> r7
@
@ Assumes that we are starting on a stripe boundary and that overreading
@ within the stripe is OK. However it does respect the dest size for writing

function ff_rpi_sand30_lines_to_planar_y16, export=1
                push            {r4-r8, lr}     @ +24
                ldr             r3,  [sp, #24]
                ldr             r6,  [sp, #36]
                ldr             r7,  [sp, #32]  @ y
                mov             r12, #48
                vmov.u16        q15, #0x3ff
                sub             r3,  #1
                lsl             r3,  #7
                sub             r1,  r1,  r6,  lsl #1
                add             r8,  r2,  r7,  lsl #7
                ldr             r7,  [sp, #40]

10:
                mov             r2,  r8
                add             r4,  r0,  #24
                mov             r5,  r6
                mov             lr,  #0
1:
                vldm            r2!, {q10-q13}
                add             lr,  #64

                vshr.u32        q14, q10, #20    @ Cannot vshrn.u32 #20!
                ands            lr,  #127
                vshrn.u32       d2,  q10, #10
                vmovn.u32       d0,  q10
                vmovn.u32       d4,  q14

                vshr.u32        q14, q11, #20
                it              eq
                addeq           r2,  r3
                vshrn.u32       d3,  q11, #10
                vmovn.u32       d1,  q11
                vmovn.u32       d5,  q14

                subs            r5,  #48
                vand            q0,  q15
                vand            q1,  q15
                vand            q2,  q15

                vshr.u32        q14, q12, #20
                vshrn.u32       d18, q12, #10
                vmovn.u32       d16, q12
                vmovn.u32       d20, q14

                vshr.u32        q14, q13, #20
                vshrn.u32       d19, q13, #10
                vmovn.u32       d17, q13
                vmovn.u32       d21, q14

                vand            q8,  q15
                vand            q9,  q15
                vand            q10, q15
                blt             2f

                vst3.16         {d0,  d2,  d4},  [r0], r12
                vst3.16         {d1,  d3,  d5},  [r4], r12
                vst3.16         {d16, d18, d20}, [r0], r12
                vst3.16         {d17, d19, d21}, [r4], r12

                bne             1b

11:
                subs            r7,  #1
                add             r0,  r1
                add             r8,  #128
                bne             10b

                pop             {r4-r8, pc}

@ Partial final write
2:
                cmp             r5,  #24-48
                blt             1f
                vst3.16         {d0,  d2,  d4},  [r0], r12
                vst3.16         {d1,  d3,  d5},  [r4]
                beq             11b
                vmov            q0,  q8
                sub             r5,  #24
                vmov            q1,  q9
                vmov            q2,  q10
1:
                cmp             r5,  #12-48
                blt             1f
                vst3.16         {d0,  d2,  d4},  [r0]!
                beq             11b
                vmov            d0, d1
                sub             r5, #12
                vmov            d2, d3
                vmov            d4, d5
1:
                cmp             r5,  #6-48
                add             r4,  r0,  #6    @ avoid [r0]! on sequential instructions
                blt             1f
                vst3.16         {d0[0], d2[0], d4[0]}, [r0]
                vst3.16         {d0[1], d2[1], d4[1]}, [r4]
                add             r0,  #12
                beq             11b
                vmov            s0,  s1
                sub             r5,  #6
                vmov            s4,  s5
                vmov            s8,  s9
1:
                cmp             r5, #3-48
                blt             1f
                vst3.16         {d0[0], d2[0], d4[0]}, [r0]!
                beq             11b
                sub             r5, #3
                vshr.u32        d0, #16
                vshr.u32        d2, #16
1:
                cmp             r5, #2-48
                blt             1f
                vst2.16         {d0[0], d2[0]}, [r0]!
                b               11b
1:
                vst1.16         {d0[0]}, [r0]!
                b               11b

endfunc


@ void ff_rpi_sand30_lines_to_planar_c16(
@   uint8_t * dst_u,            // [r0]
@   unsigned int dst_stride_u,  // [r1]
@   uint8_t * dst_v,            // [r2]
@   unsigned int dst_stride_v,  // [r3]
@   const uint8_t * src,        // [sp, #0]  -> r4, r5
@   unsigned int stride1,       // [sp, #4]  128
@   unsigned int stride2,       // [sp, #8]  -> r8
@   unsigned int _x,            // [sp, #12] 0
@   unsigned int y,             // [sp, #16] (r7 in prefix)
@   unsigned int _w,            // [sp, #20] -> r6, r9
@   unsigned int h);            // [sp, #24] -> r7
@
@ Assumes that we are starting on a stripe boundary and that overreading
@ within the stripe is OK. However it does respect the dest size for writing

function ff_rpi_sand30_lines_to_planar_c16, export=1
                push            {r4-r10, lr}    @ +32
                ldr             r5,  [sp, #32]
                ldr             r8,  [sp, #40]
                ldr             r7,  [sp, #48]
                ldr             r9,  [sp, #52]
                mov             r12, #48
                vmov.u16        q15, #0x3ff
                sub             r8,  #1
                lsl             r8,  #7
                add             r5,  r5,  r7,  lsl #7
                sub             r1,  r1,  r9,  lsl #1
                sub             r3,  r3,  r9,  lsl #1
                ldr             r7,  [sp, #56]
10:
                mov             lr,  #0
                mov             r4,  r5
                mov             r6,  r9
1:
                vldm            r4!, {q0-q3}
                add             lr,  #64

                @ N.B. unpack [0,1,2] -> (reg order) 1, 0, 2
                vshr.u32        q14, q0,  #20
                vshrn.u32       d16, q0,  #10
                vmovn.u32       d18, q0
                ands            lr,  #127
                vmovn.u32       d20, q14

                vshr.u32        q14, q1,  #20
                vshrn.u32       d17, q1,  #10
                vmovn.u32       d19, q1
                vmovn.u32       d21, q14

                vshr.u32        q14, q2,  #20
                vshrn.u32       d22, q2,  #10
                vmovn.u32       d24, q2
                vmovn.u32       d26, q14

                vshr.u32        q14, q3,  #20
                vshrn.u32       d23, q3,  #10
                vmovn.u32       d25, q3
                add             r10, r0,  #24
                vmovn.u32       d27, q14

                it              eq
                addeq           r4,  r8
                vuzp.16         q8,  q11
                vuzp.16         q9,  q12
                vuzp.16         q10, q13

                @ q8   V0, V3,.. -> q0
                @ q9   U0, U3...
                @ q10  U1, U4...
                @ q11  U2, U5,..
                @ q12  V1, V4,.. -> q1
                @ q13  V2, V5,.. -> q2

                subs            r6,  #24
                vand            q11, q15
                vand            q9,  q15
                vand            q10, q15
                vand            q0,  q8,  q15
                vand            q1,  q12, q15
                vand            q2,  q13, q15

                blt             2f

                vst3.16         {d18, d20, d22}, [r0],  r12
                vst3.16         {d19, d21, d23}, [r10]
                add             r10, r2,  #24
                vst3.16         {d0,  d2,  d4},  [r2],  r12
                vst3.16         {d1,  d3,  d5},  [r10]

                bne             1b

11:
                subs            r7,  #1
                add             r5,  #128
                add             r0,  r1
                add             r2,  r3
                bne             10b

                pop             {r4-r10, pc}

@ Partial final write
2:
                cmp             r6,  #-12
                blt             1f
                vst3.16         {d18, d20, d22}, [r0]!
                vst3.16         {d0,  d2,  d4},  [r2]!
                beq             11b
                vmov            d18, d19
                vmov            d20, d21
                vmov            d22, d23
                sub             r6,  #12
                vmov            d0,  d1
                vmov            d2,  d3
                vmov            d4,  d5
1:
                cmp             r6,  #-18
                @ Rezip here as it makes the remaining tail handling easier
                vzip.16         d0,  d18
                vzip.16         d2,  d20
                vzip.16         d4,  d22
                blt             1f
                vst3.16         {d0[1],  d2[1],  d4[1]},  [r0]!
                vst3.16         {d0[0],  d2[0],  d4[0]},  [r2]!
                vst3.16         {d0[3],  d2[3],  d4[3]},  [r0]!
                vst3.16         {d0[2],  d2[2],  d4[2]},  [r2]!
                beq             11b
                vmov            d0,  d18
                vmov            d2,  d20
                sub             r6,  #6
                vmov            d4,  d22
1:
                cmp             r6,  #-21
                blt             1f
                vst3.16         {d0[1], d2[1], d4[1]}, [r0]!
                vst3.16         {d0[0], d2[0], d4[0]}, [r2]!
                beq             11b
                vmov            s4,  s5
                sub             r6,  #3
                vmov            s0,  s1
1:
                cmp             r6,  #-22
                blt             1f
                vst2.16         {d0[1], d2[1]}, [r0]!
                vst2.16         {d0[0], d2[0]}, [r2]!
                b               11b
1:
                vst1.16         {d0[1]}, [r0]!
                vst1.16         {d0[0]}, [r2]!
                b               11b

endfunc

@ void ff_rpi_sand30_lines_to_planar_p010(
@   uint8_t * dest,             // [r0]
@   unsigned int dst_stride,    // [r1]
@   const uint8_t * src,        // [r2]
@   unsigned int src_stride1,   // [r3]      Ignored - assumed 128
@   unsigned int src_stride2,   // [sp, #0]  -> r3
@   unsigned int _x,            // [sp, #4]  Ignored - 0
@   unsigned int y,             // [sp, #8]  (r7 in prefix)
@   unsigned int _w,            // [sp, #12] -> r6 (cur r5)
@   unsigned int h);            // [sp, #16] -> r7
@
@ Assumes that we are starting on a stripe boundary and that overreading
@ within the stripe is OK. However it does respect the dest size for writing

function ff_rpi_sand30_lines_to_planar_p010, export=1
                push            {r4-r8, lr}     @ +24
                ldr             r3,  [sp, #24]
                ldr             r6,  [sp, #36]
                ldr             r7,  [sp, #32]  @ y
                mov             r12, #48
                vmov.u16        q15, #0xffc0
                sub             r3,  #1
                lsl             r3,  #7
                sub             r1,  r1,  r6,  lsl #1
                add             r8,  r2,  r7,  lsl #7
                ldr             r7,  [sp, #40]

10:
                mov             r2,  r8
                add             r4,  r0,  #24
                mov             r5,  r6
                mov             lr,  #0
1:
                vldm            r2!, {q10-q13}
                add             lr,  #64

                vshl.u32        q14, q10, #6
                ands            lr,  #127
                vshrn.u32       d4,  q10, #14
                vshrn.u32       d2,  q10, #4
                vmovn.u32       d0,  q14

                vshl.u32        q14, q11, #6
                it              eq
                addeq           r2,  r3
                vshrn.u32       d5,  q11, #14
                vshrn.u32       d3,  q11, #4
                vmovn.u32       d1,  q14

                subs            r5,  #48
                vand            q2,  q15
                vand            q1,  q15
                vand            q0,  q15

                vshl.u32        q14, q12, #6
                vshrn.u32       d20, q12, #14
                vshrn.u32       d18, q12, #4
                vmovn.u32       d16, q14

                vshl.u32        q14, q13, #6
                vshrn.u32       d21, q13, #14
                vshrn.u32       d19, q13, #4
                vmovn.u32       d17, q14

                vand            q10, q15
                vand            q9,  q15
                vand            q8,  q15
                blt             2f

                vst3.16         {d0,  d2,  d4},  [r0], r12
                vst3.16         {d1,  d3,  d5},  [r4], r12
                vst3.16         {d16, d18, d20}, [r0], r12
                vst3.16         {d17, d19, d21}, [r4], r12

                bne             1b

11:
                subs            r7,  #1
                add             r0,  r1
                add             r8,  #128
                bne             10b

                pop             {r4-r8, pc}

@ Partial final write
2:
                cmp             r5,  #24-48
                blt             1f
                vst3.16         {d0,  d2,  d4},  [r0], r12
                vst3.16         {d1,  d3,  d5},  [r4]
                beq             11b
                vmov            q0,  q8
                sub             r5,  #24
                vmov            q1,  q9
                vmov            q2,  q10
1:
                cmp             r5,  #12-48
                blt             1f
                vst3.16         {d0,  d2,  d4},  [r0]!
                beq             11b
                vmov            d0, d1
                sub             r5, #12
                vmov            d2, d3
                vmov            d4, d5
1:
                cmp             r5,  #6-48
                add             r4,  r0,  #6    @ avoid [r0]! on sequential instructions
                blt             1f
                vst3.16         {d0[0], d2[0], d4[0]}, [r0]
                vst3.16         {d0[1], d2[1], d4[1]}, [r4]
                add             r0,  #12
                beq             11b
                vmov            s0,  s1
                sub             r5,  #6
                vmov            s4,  s5
                vmov            s8,  s9
1:
                cmp             r5, #3-48
                blt             1f
                vst3.16         {d0[0], d2[0], d4[0]}, [r0]!
                beq             11b
                sub             r5, #3
                vshr.u32        d0, #16
                vshr.u32        d2, #16
1:
                cmp             r5, #2-48
                blt             1f
                vst2.16         {d0[0], d2[0]}, [r0]!
                b               11b
1:
                vst1.16         {d0[0]}, [r0]!
                b               11b

endfunc



