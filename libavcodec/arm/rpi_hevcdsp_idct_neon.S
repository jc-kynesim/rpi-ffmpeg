/*
 * Copyright (c) 2014 Seppo Tomperi <seppo.tomperi@vtt.fi>
 * Copyright (C) 2018 John Cox, Ben Avison for Raspberry Pi (Trading)
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

/* uses registers q8 - q13 for temp values */
.macro tr4_luma_shift shift
        vaddl.s16   q8, d28, d30    // c0 = src0 + src2
        vaddl.s16   q9, d30, d31    // c1 = src2 + src3
        vsubl.s16   q10, d28, d31   // c2 = src0 - src3
        vaddl.s16   q11, d28, d31   // src0 + src3

        vmul.i32    q12, q8, d1[0]  // 29 * c0
        vmul.i32    q13, q10, d2[0] // 55 * c2
        vmul.i32    q8, q8, d2[0]   // 55 * c0
        vmull.s16   q14, d29, d0[0] // c3 = 74 * src1

        vsubw.s16   q11, q11, d30   // src0 - src2 + src3
        vmla.i32    q12, q9, d2[0]  // 29 * c0 + 55 * c1
        vmls.i32    q13, q9, d1[0]  // 55 * c2 - 29 * c1
        vmla.i32    q8, q10, d1[0]  // 55 * c0 + 29 * c2

        vmul.i32    q11, q11, d0[0] // dst2 = 74 * (src0 - src2 + src3)
        vadd.i32    q12, q12, q14   // dst0 = 29 * c0 + 55 * c1 + c3
        vadd.i32    q13, q13, q14   // dst1 = 55 * c2 - 29 * c1 + c3
        vsub.i32    q8, q8, q14     // dst3 = 55 * c0 + 29 * c2 - c3

        vqrshrn.s32 d28, q12, \shift
        vqrshrn.s32 d29, q13, \shift
        vqrshrn.s32 d30, q11, \shift
        vqrshrn.s32 d31, q8, \shift
.endm

/* uses registers q8 - q11 for temp values */
.macro tr4_shift shift
        vmull.s16   q9, d29, d0[0]   // 83 * src1
        vmull.s16   q8, d29, d0[1]   // 36 * src1
        vshll.s16   q14, d28, #6     // 64 * src0
        vshll.s16   q10, d30, #6     // 64 * src2
        vmlal.s16   q9, d31, d0[1]   // 83 * src1 + 36 * src3  o0
        vmlsl.s16   q8, d31, d0[0]   // 36 * src1 - 83 * src3  o1
        vadd.s32    q11, q14, q10    // 64 * (src0 + src2)     e0
        vsub.s32    q10, q14, q10    // 64 * (src0 - src2)     e1
        vadd.s32    q14, q11, q9     // e0 + o0
        vadd.s32    q15, q10, q8     // e1 + o1
        vsub.s32    q8, q10, q8      // e1 - o1
        vsub.s32    q9, q11, q9      // e0 - o0

        vqrshrn.s32 d28, q14, \shift
        vqrshrn.s32 d29, q15, \shift
        vqrshrn.s32 d30, q8, \shift
        vqrshrn.s32 d31, q9, \shift
.endm

.macro tr8_process d0, d1, d2, d3, d4, d5, d6, d7,                         \
                   tmp0, /* Q reg which doesn't alias with d4, d6 or d7 */ \
                   tmp1, /* Q reg which doesn't alias with d7 or d0     */ \
                   shift, I1, I2, I3

        vmull.s16  q4, \d1, d1[1]        // 89 * src1
        \I1
        vmull.s16  q5, \d1, d1[0]        // 75 * src1
        \I2
        vmull.s16  q6, \d1, d1[3]        // 50 * src1
        \I3
        vmull.s16  q7, \d1, d1[2]        // 18 * src1
        vmlal.s16  q4, \d3, d1[0]        // 75 * src3
        vmlsl.s16  q5, \d3, d1[2]        //-18 * src3
        vmlsl.s16  q6, \d3, d1[1]        //-89 * src3
        vmlsl.s16  q7, \d3, d1[3]        //-50 * src3

          // tr4
          vmull.s16  q1, \d2, d0[0]      // 83 * src(1*2)
          vmull.s16  q2, \d2, d0[1]      // 36 * src(1*2)

        vmlal.s16  q4, \d5, d1[3]        // 50 * src5
        vmlsl.s16  q5, \d5, d1[1]        //-89 * src5
        vmlal.s16  q6, \d5, d1[2]        // 18 * src5
        vmlal.s16  q7, \d5, d1[0]        // 75 * src5

          vshll.s16  q3, \d0, #6         // 64 * src(0*2)
          vshll.s16  \tmp0, \d4, #6      // 64 * src(2*2)
          vmlal.s16  q1, \d6, d0[1]      // 83 * src(1*2) + 36 * src(3*2)  o0
          vmlsl.s16  q2, \d6, d0[0]      // 36 * src(1*2) - 83 * src(3*2)  o1
          vadd.i32   \tmp1, q3, \tmp0    // 64 * (src(0*2) + src(2*2))     e0
          vsub.i32   \tmp0, q3, \tmp0    // 64 * (src(0*2) - src(2*2))     e1

        vmlal.s16  q4, \d7, d1[2]        // 18 * src7
        vmlsl.s16  q5, \d7, d1[3]        //-50 * src7
        vmlal.s16  q6, \d7, d1[0]        // 75 * src7
        vmlsl.s16  q7, \d7, d1[1]        //-89 * src7

          vsub.i32   q3, \tmp1, q1       // e0 - o0
          vadd.i32   \tmp1, \tmp1, q1    // e0 + o0
          vadd.i32   q1, \tmp0, q2       // e1 + o1
          vsub.i32   q2, \tmp0, q2       // e1 - o1

        vadd.i32   \tmp0, \tmp1, q4      // e_8[0] + o_8[0], dst[0]
        vsub.i32   q4, \tmp1, q4         // e_8[0] - o_8[0], dst[7]
        vsub.i32   \tmp1, q3, q7         // e_8[3] - o_8[3], dst[4]
        vadd.i32   q7, q3, q7            // e_8[3] + o_8[3], dst[3]
        vadd.i32   q3, q1, q5            // e_8[1] + o_8[1], dst[1]
        vsub.i32   q5, q1, q5            // e_8[1] - o_8[1], dst[6]
        vsub.i32   q1, q2, q6            // e_8[2] - o_8[2], dst[5]
        vadd.i32   q6, q2, q6            // e_8[2] + o_8[2], dst[2]
        vqrshrn.s32   \d0, \tmp0, #\shift
        vqrshrn.s32   \d4, \tmp1, #\shift
        vqrshrn.s32   \d1, q3, #\shift
        vqrshrn.s32   \d5, q1, #\shift
        vqrshrn.s32   \d2, q6, #\shift
        vqrshrn.s32   \d6, q5, #\shift
        vqrshrn.s32   \d3, q7, #\shift
        vqrshrn.s32   \d7, q4, #\shift
.endm

.macro tr8_vert d0, d1, d2, d3, d4, d5, d6, d7, q01, q23, I1, I2, I3
        vld1.16     {\d0}, [r0 :64], r3
        vld1.16     {\d1}, [r2 :64], r3
        vld1.16     {\d2}, [r0 :64], r3
        vld1.16     {\d3}, [r2 :64], r3
        vld1.16     {\d4}, [r0 :64], r3
        vld1.16     {\d5}, [r2 :64], r3
        vld1.16     {\d6}, [r0 :64], r3
        vld1.16     {\d7}, [r2 :64], r3

        tr8_process \
            \d0, \d1, \d2, \d3, \d4, \d5, \d6, \d7, \
            \q01, \q23, 7, "\I1", "\I2", "\I3"
.endm

.macro tr8_horiz d0, d1, d2, d3, d4, d5, d6, d7, q01, q23, shift
        tr8_process \
            \d0, \d1, \d2, \d3, \d4, \d5, \d6, \d7, \
            \q01, \q23, \shift

        vzip.16    \d0, \d4
        vzip.16    \d1, \d5
        vzip.16    \d2, \d6
        vzip.16    \d3, \d7
        vst4.16    {\d0-\d3}, [r0 :128], r3
        vst4.16    {\d4-\d7}, [r2 :128], r3
.endm

#define BIT_DEPTH 8
#include "rpi_hevc_idct_fn_neon.S"

.text

.align 4
tr4f:
.word 0x00240053  // 36 and d1[0] = 83
.word 0x00000000
tr8f:
.word 0x0059004b  // 89, d0[0] = 75
.word 0x00320012  // 50, d0[2] = 18
tr16:
.word 0x005a0057  // 90, d2[0] = 87
.word 0x00500046  // 80, d2[2] = 70
.word 0x0039002b  // 57, d2[0] = 43
.word 0x00190009  // 25, d2[2] = 9

#undef BIT_DEPTH
#define BIT_DEPTH 10
#include "rpi_hevc_idct_fn_neon.S"

