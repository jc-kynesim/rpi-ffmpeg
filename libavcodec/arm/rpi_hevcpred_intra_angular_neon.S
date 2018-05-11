/*
 * Copyright (c) 2018 John Cox <jc@kynesim.co.uk> (for Raspberry Pi)
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

/*
 * General angular pred
 *
 * Horizontal (10) & Vertical (26) cases have their own file
 * and are not dealt with properly here (luma filtering is missing)
 *
 * The inv_angle calculations are annoying - if it wasn't for the +128
 * rounding step then the result would simply be the loop counter :-(
 */


#include "libavutil/arm/asm.S"
#include "neon.S"

.text

@ Horizontal Patch functions
@ These need a transpose before store so exist as smaller patches
@ Patches can be called repeatedly without any intermediate setup
@ to generate a horizontal block
@
@ It is almost certainly the case that larger patch fns can be built
@ and they would be a little faster, but we would still need the small
@ fns and code size (or at least instruction cache size) is an issue
@ given how much code we already have here

@ Generate 8x8 luma 8 patch
@
@ r3   Out stride
@ r4   Angle add
@ r7   Inv angle (_up only)
@
@ In/Out (updated)
@ r0   Out pointer - on exit point to start of next patch horizontally (i.e. r0 + patch width)
@ r2   Left ptr - updated
@ r6   Angle frac (init to r4 + 32)
@ r8   Inv angle accumulator
@ d24  Cur Line - load before 1st call for down - set by _up
@ d16  Cur Line - load before 1st call for up   - set by _down
@
@ Temps
@ r5   Loop counter
@ r12
@ q0-q3, q14, q15

patch_h_down_8x8_8:
        mov         r5,  #8
2:
        cmp         r6,  #32
        ble         1f

        vmov        d16, d24
        vext.8      d24, d24, #1
        sub         r6,  #32
        vld1.8     {d24[7]}, [r2]!

1:
        vext.8      q0,  q1,  #8
        rsb         r12, r6,  #32
        vext.8      q1,  q2,  #8
        vdup.8      d30, r6
        vext.8      q2,  q3,  #8
        vdup.8      d31, r12
        vext.8      q3,  q3,  #8

        vmull.u8    q14, d24, d30
        add         r6,  r4
        vmlal.u8    q14, d16, d31
        subs        r5,  #1
        vrshrn.u16  d7,  q14, #5
        bne         2b

store_tran_8x8_8:
        add         r12, r0,  #4
        vst4.8     {d0[0], d1[0], d2[0], d3[0]}, [r0 ]
        add         r5,  r0,  r3
        vst4.8     {d4[0], d5[0], d6[0], d7[0]}, [r12], r3
        add         r0,  #8
        vst4.8     {d0[1], d1[1], d2[1], d3[1]}, [r5 ], r3
        vst4.8     {d4[1], d5[1], d6[1], d7[1]}, [r12], r3
        vst4.8     {d0[2], d1[2], d2[2], d3[2]}, [r5 ], r3
        vst4.8     {d4[2], d5[2], d6[2], d7[2]}, [r12], r3
        vst4.8     {d0[3], d1[3], d2[3], d3[3]}, [r5 ], r3
        vst4.8     {d4[3], d5[3], d6[3], d7[3]}, [r12], r3
        vst4.8     {d0[4], d1[4], d2[4], d3[4]}, [r5 ], r3
        vst4.8     {d4[4], d5[4], d6[4], d7[4]}, [r12], r3
        vst4.8     {d0[5], d1[5], d2[5], d3[5]}, [r5 ], r3
        vst4.8     {d4[5], d5[5], d6[5], d7[5]}, [r12], r3
        vst4.8     {d0[6], d1[6], d2[6], d3[6]}, [r5 ], r3
        vst4.8     {d4[6], d5[6], d6[6], d7[6]}, [r12], r3
        vst4.8     {d0[7], d1[7], d2[7], d3[7]}, [r5 ]
        vst4.8     {d4[7], d5[7], d6[7], d7[7]}, [r12]
        bx          lr


patch_h_up_8x8_8:
        mov         r5,  #8
2:
        cmp         r6,  #32
        ble         1f

        @ For other widths we may want different logic
        @ r2=left (variable), r1=up (const)
        adds        r8,  r7
        vmov        d24, d16
        ldrbmi      r12, [r2,  #-1]!
        ldrbpl      r12, [r1,  r8,  asr #8]
        vext.8      d16, d16, d16, #7
        sub         r6,  #32
        vmov.8      d16[0], r12

1:
        vdup.8      d31, r6
        vext.8      q0,  q1,  #8
        rsb         r12, r6,  #32
        vext.8      q1,  q2,  #8

        vmull.u8    q14, d16, d31
        vext.8      q2,  q3,  #8
        vdup.8      d30, r12
        vext.8      q3,  q3,  #8
        add         r6,  r4
        vmlal.u8    q14, d24, d30
        subs        r5,  #1
        vrshrn.u16  d7,  q14, #5
        bne         2b
        b           store_tran_8x8_8  @ This will return



@ ff_hevc_rpi_pred_angular_4_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride        [r3]
@       unsigned int mode       [sp, #0]  2..34

function ff_hevc_rpi_pred_angular_4_neon_8, export=1
        ldr         r12, [sp, #0]
        push       {r4-r8, lr}
        adrl        r4,  angle_2 - 2
        adrl        r7,  inv_angle - 11*2
        ldrsb       r4,  [r4, r12]
        add         r7,  r7,  r12, lsl #1

        cmp         r12, #18
        mov         r5,  #4             @ Loop counter for all cases
        add         r6,  r4,  #32       @ Force initial load in main loop
        bge         18f

        cmp         r12, #10
        bge         10f

@ Down of Horizontal - works down left
        vld1.8     {d24}, [r2]
2:
        cmp         r6,  #32
        ble         1f

        vmov        d16, d24
        vext.8      d24, d24, #1
        sub         r6,  #32
1:
        vext.8      q0,  q1,  #8
        rsb         r12, r6,  #32
        vext.8      q1,  q1,  #8
        vdup.8      d30, r6
        vdup.8      d31, r12

        vmull.u8    q14, d24, d30
        add         r6,  r4
        vmlal.u8    q14, d16, d31
        subs        r5,  #1
        vrshrn.u16  d3,  q14, #5
        bne         2b

98:
        add         r12, r0,  r3
        lsl         r3,  #1
        vst4.8     {d0[0], d1[0], d2[0], d3[0]}, [r0 ], r3
        vst4.8     {d0[1], d1[1], d2[1], d3[1]}, [r12], r3
        vst4.8     {d0[2], d1[2], d2[2], d3[2]}, [r0 ]
        vst4.8     {d0[3], d1[3], d2[3], d3[3]}, [r12]
        pop        {r4-r8, pc}

@ Up of Horizontal - works down up
10:
        ldrh        r7,  [r7]
        @ -128 (rather than +128) means we get UL
        @ from L & don't have to offset U
        mov         r8,  #-128
        vld1.32    {d16[0]}, [r2]
        sub         r8,  r7
2:
        cmp         r6,  #32
        ble         1f

        @ For other widths we may want different logic
        @ r2=left (variable), r1=up (const)
        adds        r8,  r7
        vmov        d24, d16
        ldrbmi      r12, [r2,  #-1]!
        ldrbpl      r12, [r1,  r8,  asr #8]
        vext.8      d16, d16, d16, #7
        sub         r6,  #32
        vmov.8      d16[0], r12
1:
        vdup.8      d31, r6
        vext.8      q0,  q1,  #8
        rsb         r12, r6,  #32
        vext.8      q1,  q2,  #8

        vmull.u8    q14, d16, d31
        vdup.8      d30, r12
        add         r6,  r4
        vmlal.u8    q14, d24, d30
        subs        r5,  #1
        vrshrn.u16  d3,  q14, #5
        bne         2b
        b           98b

18:
        cmp         r12, #26
        bge         26f

@ Left of vertical - works down left
        vld1.32    {d16[0]}, [r1  :32]    @ Up
        ldrh        r7,  [r7]
        mov         r8,  #-128

2:
        cmp         r6,  #32
        ble         1f

        @ For other widths we may want different logic
        ldrb        r12, [r2,  r8,  asr #8]

        vmov        d24, d16
        add         r8,  r7
        sub         r6,  #32
        vext.8      d16, d16, #7
        vmov.8      d16[0], r12

1:
        vdup.8      d31, r6
        rsb         r12, r6,  #32

        vmull.u8    q0,  d16, d31
        vdup.8      d30, r12
        add         r6,  r4
        vmlal.u8    q0,  d24, d30
        vrshrn.u16  d0,  q0,  #5

        subs        r5,  #1
        vst1.32    {d0[0]}, [r0], r3
        bne         2b
        pop        {r4-r8, pc}

@ Right of vertical - works along top - left unused
26:
        vld1.8     {d24}, [r1]          @ Up + up-right, may be on 32-bit align rather than 64
2:
        cmp         r6,  #32
        ble         1f

        vmov        d16, d24
        vext.8      d24, d24, #1
        sub         r6,  #32
1:
        rsb         r12, r6,  #32
        vdup.8      d30, r6
        vdup.8      d31, r12

        vmull.u8    q0,  d24, d30
        vmlal.u8    q0,  d16, d31
        vrshrn.u16  d0,  q0,  #5

        add         r6,  r4
        subs        r5,  #1
        vst1.32    {d0[0]}, [r0], r3
        bne         2b
        pop        {r4-r8, pc}

endfunc



@ ff_hevc_rpi_pred_angular_8_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride        [r3]
@       unsigned int mode       [sp, #0]  2..34

function ff_hevc_rpi_pred_angular_8_neon_8, export=1
        ldr         r12, [sp, #0]
        push       {r4-r8, lr}
        adrl        r4,  angle_2 - 2
        adrl        r7,  inv_angle - 11*2
        ldrsb       r4,  [r4, r12]
        add         r7,  r7,  r12, lsl #1

        cmp         r12, #18
        add         r6,  r4,  #32       @ Force initial load in main loop
        bge         18f

        cmp         r12, #10
        bge         10f

@ Down of Horizontal - works down left
        vld1.8     {d24}, [r2]!
        bl          patch_h_down_8x8_8
        pop        {r4-r8, pc}

@ Up of Horizontal - works down up
10:
        ldrh        r7,  [r7]
        @ -128 (rather than +128) means we get UL
        @ from L & don't have to offset U
        mov         r8,  #-128
        vld1.8     {d16}, [r2]
        add         r6,  r4,  #32
        sub         r8,  r7
        bl          patch_h_up_8x8_8
        pop        {r4-r8, pc}

18:
        cmp         r12, #26
        mov         r5,  #8            @ Loop counter for the "easy" cases
        bge         26f

@ Left of vertical - works down left
        vld1.8     {d16}, [r1  :64]    @ Up
        ldrh        r7,  [r7]
        mov         r8,  #-128

2:
        cmp         r6,  #32
        ble         1f

        @ For other widths we may want different logic
        ldrb        r12, [r2,  r8,  asr #8]

        vmov        d24, d16
        add         r8,  r7
        sub         r6,  #32
        vext.8      d16, d16, #7
        vmov.8      d16[0], r12
1:
        vdup.8      d31, r6
        rsb         r12, r6,  #32

        vmull.u8    q0,  d16, d31
        vdup.8      d30, r12
        add         r6,  r4
        vmlal.u8    q0,  d24, d30
        vrshrn.u16  d0,  q0,  #5

        subs        r5,  #1
        vst1.8     {d0 }, [r0], r3
        bne         2b
        pop        {r4-r8, pc}

@ Right of vertical - works along top - left unused
26:
        vld1.8     {d24, d25}, [r1  :64]!    @ Up + UR
2:
        cmp         r6,  #32
        ble         1f

        vmov        d16, d24
        vext.8      q12, q12, #1
        sub         r6,  #32
1:
        rsb         r12, r6,  #32
        vdup.8      d30, r6
        vdup.8      d31, r12

        vmull.u8    q0,  d24, d30
        vmlal.u8    q0,  d16, d31
        vrshrn.u16  d0,  q0,  #5

        add         r6,  r4
        subs        r5,  #1
        vst1.8     {d0 }, [r0], r3
        bne         2b
        pop        {r4-r8, pc}

endfunc


@ ff_hevc_rpi_pred_angular_16_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride        [r3]
@       unsigned int mode       [sp, #0]  2..34

function ff_hevc_rpi_pred_angular_16_neon_8, export=1
        ldr         r12, [sp, #0]
        push       {r4-r8, lr}
        adrl        r4,  angle_2 - 2
        adrl        r7,  inv_angle - 11*2
        ldrsb       r4,  [r4, r12]
        add         r7,  r7,  r12, lsl #1

        cmp         r12, #18
        add         r6,  r4,  #32       @ Force initial load in main loop
        bge         18f

        cmp         r12, #10
        bge         10f

@ Down of Horizontal - works down left
        vld1.8     {d24}, [r2]!
        mov         r1,  r2             @ save r2 - r1 unused by patch_down

        bl          patch_h_down_8x8_8
        bl          patch_h_down_8x8_8

        mov         r2,  r1             @ restore r2
        sub         r0,  #16
        add         r6,  r4,  #32       @ Force initial load in main loop
        vld1.8     {d24}, [r2]!
        add         r0,  r0,  r3,  lsl #3

        bl          patch_h_down_8x8_8
        bl          patch_h_down_8x8_8
        pop        {r4-r8, pc}

@ Up of Horizontal - works down up
10:
        ldrh        r7,  [r7]
        @ -128 (rather than +128) means we get UL
        @ from L & don't have to offset U
        mov         r8,  #-128
        vld1.8     {d16}, [r2]
        sub         r8,  r7

        push       {r2, r8}
        bl          patch_h_up_8x8_8
        bl          patch_h_up_8x8_8
        pop        {r2, r8}

        sub         r0,  #16
        add         r6,  r4,  #32
        add         r2,  r2,  #8
        sub         r8,  r8,  r7,  lsl #3
        add         r0,  r0,  r3,  lsl #3
        vld1.8     {d16}, [r2]

        bl          patch_h_up_8x8_8
        bl          patch_h_up_8x8_8
        pop        {r4-r8, pc}

18:
        cmp         r12, #26
        mov         r5,  #16            @ Loop counter for the "easy" cases
        bge         26f

@ Left of vertical - works down left
        vld1.8     {q8 }, [r1  :128]    @ Up
        ldrh        r7,  [r7]
        mov         r8,  #-128

2:
        cmp         r6,  #32
        ble         1f

        @ For other widths we may want different logic
        ldrb        r12, [r2,  r8,  asr #8]

        vmov        q12, q8
        add         r8,  r7
        sub         r6,  #32
        vext.8      q8,  q8,  q8,  #15
        vmov.8      d16[0], r12

1:
        vdup.8      d31, r6
        rsb         r12, r6,  #32

        vmull.u8    q0,  d16, d31
        vmull.u8    q1,  d17, d31
        vdup.8      d30, r12
        add         r6,  r4
        vmlal.u8    q0,  d24, d30
        vmlal.u8    q1,  d25, d30

        vrshrn.u16  d0,  q0,  #5
        vrshrn.u16  d1,  q1,  #5

        subs        r5,  #1
        vst1.8     {q0 }, [r0], r3
        bne         2b
        pop        {r4-r8, pc}

@ Right of vertical - works along top - left unused
26:
        vld1.8     {q12}, [r1  :128]!   @ Up
2:
        cmp         r6,  #32
        ble         1f

        vmov        q8,  q12
        vext.8      q12, q12, #1
        sub         r6,  #32
        vld1.8     {d25[7]}, [r1]!

1:
        rsb         r12, r6,  #32
        vdup.8      d30, r6
        vdup.8      d31, r12

        vmull.u8    q0,  d24, d30
        vmull.u8    q1,  d25, d30
        vmlal.u8    q0,  d16, d31
        vmlal.u8    q1,  d17, d31

        vrshrn.u16  d0,  q0,  #5
        vrshrn.u16  d1,  q1,  #5

        add         r6,  r4
        subs        r5,  #1
        vst1.8     {q0 }, [r0], r3
        bne         2b
        pop        {r4-r8, pc}

endfunc


@ ff_hevc_rpi_pred_angular_32_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride        [r3]
@       unsigned int mode       [sp, #0]  2..34

function ff_hevc_rpi_pred_angular_32_neon_8, export=1
        ldr         r12, [sp, #0]
        push       {r4-r10, lr}
        adrl        r4,  angle_2 - 2
        adrl        r7,  inv_angle - 11*2
        ldrsb       r4,  [r4, r12]
        add         r7,  r7,  r12, lsl #1

        cmp         r12, #18
        bge         18f

        cmp         r12, #10
        mov         r10, #4             @ Outer loop counter for "hard" cases
        bge         10f

@ Down of Horizontal - works down left
        mov         r1,  r2
2:
        vld1.8     {d24}, [r1]!
        add         r6,  r4,  #32       @ Force initial load in main loop
        mov         r2,  r1

        bl          patch_h_down_8x8_8
        bl          patch_h_down_8x8_8
        bl          patch_h_down_8x8_8
        bl          patch_h_down_8x8_8

        sub         r0,  #32
        subs        r10, #1
        add         r0,  r0,  r3,  lsl #3
        bne         2b
        pop        {r4-r10, pc}

@ Up of Horizontal - works down up
10:
        ldrh        r7,  [r7]
        @ -128 (rather than +128) means we get UL
        @ from L & don't have to offset U
        mov         r8,  #-128
        sub         r8,  r7
2:
        vld1.8     {d16}, [r2]
        add         r6,  r4,  #32

        push       {r2, r8}
        bl          patch_h_up_8x8_8
        bl          patch_h_up_8x8_8
        bl          patch_h_up_8x8_8
        bl          patch_h_up_8x8_8
        pop        {r2, r8}

        sub         r0,  #32
        subs        r10, #1
        add         r2,  r2,  #8
        sub         r8,  r8,  r7,  lsl #3
        add         r0,  r0,  r3,  lsl #3
        bne         2b
        pop        {r4-r10, pc}

18:
        cmp         r12, #26
        mov         r5,  #32            @ Loop counter for the "easy" cases
        bge         26f

@ Left of vertical - works down left
        vld1.8     {q8,  q9 }, [r1  :128]    @ Up
        ldrh        r7,  [r7]
        add         r6,  r4,  #32
        mov         r8,  #-128

2:
        cmp         r6,  #32
        ble         1f

        @ For other widths we may want different logic
        ldrb        r12, [r2,  r8,  asr #8]

        vmov        q12, q8
        add         r8,  r7
        vmov        q13, q9
        sub         r6,  #32
        vext.8      q9,  q8,  q9,  #15
        vext.8      q8,  q8,  q8,  #15
        vmov.8      d16[0], r12

1:
        vdup.8      d31, r6
        rsb         r12, r6,  #32

        vmull.u8    q0,  d16, d31
        vmull.u8    q1,  d17, d31
        vdup.8      d30, r12
        add         r6,  r4
        vmull.u8    q2,  d18, d31
        vmull.u8    q3,  d19, d31
        vmlal.u8    q0,  d24, d30
        vmlal.u8    q1,  d25, d30
        vmlal.u8    q2,  d26, d30
        vmlal.u8    q3,  d27, d30

        vrshrn.u16  d0,  q0,  #5
        vrshrn.u16  d1,  q1,  #5
        vrshrn.u16  d2,  q2,  #5
        vrshrn.u16  d3,  q3,  #5

        subs        r5,  #1
        vst1.8     {q0,  q1 }, [r0], r3
        bne         2b
        pop        {r4-r10, pc}

@ Right of vertical - works along top - left unused
26:
        vld1.8     {q12, q13}, [r1  :128]!   @ Up
        add         r6,  r4,  #32       @ Force initial load in main loop
2:
        cmp         r6,  #32
        ble         1f

        vmov        q8,  q12
        vmov        q9,  q13
        vext.8      q12, q13, #1
        vext.8      q13, q13, #1
        sub         r6,  #32
        vld1.8     {d27[7]}, [r1]!

1:
        rsb         r12, r6,  #32
        vdup.8      d30, r6
        vdup.8      d31, r12

        vmull.u8    q0,  d24, d30
        vmull.u8    q1,  d25, d30
        vmull.u8    q2,  d26, d30
        vmull.u8    q3,  d27, d30
        vmlal.u8    q0,  d16, d31
        vmlal.u8    q1,  d17, d31
        vmlal.u8    q2,  d18, d31
        vmlal.u8    q3,  d19, d31

        vrshrn.u16  d0,  q0,  #5
        vrshrn.u16  d1,  q1,  #5
        vrshrn.u16  d2,  q2,  #5
        vrshrn.u16  d3,  q3,  #5

        add         r6,  r4
        subs        r5,  #1
        vst1.8     {q0,  q1 }, [r0], r3
        bne         2b
        pop        {r4-r10, pc}

endfunc

@ Chroma 8 bit 4x4 patch fns
        .text

patch_h_down_c_4x4_8:
        mov         r5,  #4
2:
        cmp         r6,  #32
        ble         1f

        vmov        d16, d24
        vext.16     d24, d24, #1
        sub         r6,  #32
        vld1.16    {d24[3]}, [r2]!

1:
        vext.8      q0,  q1,  #8
        rsb         r12, r6,  #32
        vext.8      q1,  q1,  #8
        vdup.8      d30, r6
        vdup.8      d31, r12

        vmull.u8    q14, d24, d30
        add         r6,  r4
        vmlal.u8    q14, d16, d31
        subs        r5,  #1
        vrshrn.u16  d3,  q14, #5
        bne         2b

store_tran_c_4x4_8:
        add         r12, r0,  r3
        vst4.16    {d0[0], d1[0], d2[0], d3[0]}, [r0 ]!
        add         r5,  r12, r3
        vst4.16    {d0[1], d1[1], d2[1], d3[1]}, [r12]
        add         r12, r12, r3,  lsl #1
        vst4.16    {d0[2], d1[2], d2[2], d3[2]}, [r5 ]
        vst4.16    {d0[3], d1[3], d2[3], d3[3]}, [r12]
        bx          lr

patch_h_up_c_4x4_8:
        mov         r5,  #4
2:
        cmp         r6,  #32
        ble         1f

        @ If r8 is -ve then we are still tracking left
        adds        r8,  r7
        vmov        d24, d16
        @ Initially r2=left (variable), r1=up (const)
        @ Use r2 for both up and left, we only ever go from left->up so
        @ we assume that we are left and thenm overwrite with up if wanted
        sub         r2,  #2
        addpl       r2,  r1,  r8,  asr #7
        vext.16     d16, d16, d16, #3
        @ We get *2 by >> 7 rather than 8, but that means we need to lose bit 0
        and         r2,  #~1
        sub         r6,  #32
        vld1.16     d16[0], [r2]
1:
        vdup.8      d31, r6
        vext.8      q0,  q1,  #8
        rsb         r12, r6,  #32
        vext.8      q1,  q1,  #8

        vmull.u8    q14, d16, d31
        vdup.8      d30, r12
        add         r6,  r4
        vmlal.u8    q14, d24, d30
        subs        r5,  #1
        vrshrn.u16  d3,  q14, #5
        bne         2b
        b           store_tran_c_4x4_8  @ This will return


@ ff_hevc_rpi_pred_angular_c_4_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride        [r3]
@       unsigned int mode       [sp, #0]  2..34

function ff_hevc_rpi_pred_angular_c_4_neon_8, export=1
        ldr         r12, [sp, #0]
        push       {r4-r8, lr}
        adrl        r4,  angle_2 - 2
        adrl        r7,  inv_angle - 11*2
        ldrsb       r4,  [r4, r12]
        add         r7,  r7,  r12, lsl #1
        lsl         r3,  #1

        cmp         r12, #18
        add         r6,  r4,  #32       @ Force initial load in main loop
        bge         18f

        cmp         r12, #10
        bge         10f

@ Down of Horizontal - works down left
        vld1.8     {d24}, [r2]!
        bl          patch_h_down_c_4x4_8
        pop        {r4-r8, pc}

@ Up of Horizontal - works down up
10:
        ldrh        r7,  [r7]
        @ -128 (rather than +128) means we get UL
        @ from L & don't have to offset U
        mov         r8,  #-128
        sub         r8,  r7
        vld1.8     {d16}, [r2]
        bl          patch_h_up_c_4x4_8
        pop        {r4-r8, pc}

18:
        cmp         r12, #26
        mov         r5,  #4             @ Loop counter for the "easy" cases
        bge         26f

@ Left of vertical - works down left
        vld1.8     {d16}, [r1  :64]     @ Up
        ldrh        r7,  [r7]
        mov         r8,  #-128

2:
        cmp         r6,  #32
        ble         1f

        @ For other widths we may want different logic
        asr         r12, r8,  #8
        vmov        d24, d16
        add         r8,  r7
        vext.16     d16, d16, #3
        add         r12, r2,  r12,  lsl #1
        sub         r6,  #32
        vld1.16    {d16[0]}, [r12]
1:
        vdup.8      d31, r6
        rsb         r12, r6,  #32

        vmull.u8    q0,  d16, d31
        vdup.8      d30, r12
        add         r6,  r4
        vmlal.u8    q0,  d24, d30
        vrshrn.u16  d0,  q0,  #5

        subs        r5,  #1
        vst1.8     {d0 }, [r0], r3
        bne         2b
        pop        {r4-r8, pc}

@ Right of vertical - works along top - left unused
26:
        vld1.8     {q12}, [r1]          @ Up + UR (only 64-bit aligned)
2:
        cmp         r6,  #32
        ble         1f

        vmov        q8,  q12
        vext.16     q12, q12, #1
        sub         r6,  #32

1:
        rsb         r12, r6,  #32
        vdup.8      d30, r6
        vdup.8      d31, r12

        vmull.u8    q0,  d24, d30
        vmlal.u8    q0,  d16, d31

        vrshrn.u16  d0,  q0,  #5

        add         r6,  r4
        subs        r5,  #1
        vst1.8     {d0 }, [r0], r3
        bne         2b
        pop        {r4-r8, pc}

endfunc


@ ff_hevc_rpi_pred_angular_c_8_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride        [r3]
@       unsigned int mode       [sp, #0]  2..34

function ff_hevc_rpi_pred_angular_c_8_neon_8, export=1
        ldr         r12, [sp, #0]
        push       {r4-r8, lr}
        adrl        r4,  angle_2 - 2
        adrl        r7,  inv_angle - 11*2
        ldrsb       r4,  [r4, r12]
        add         r7,  r7,  r12, lsl #1
        lsl         r3,  #1

        cmp         r12, #18
        add         r6,  r4,  #32
        bge         18f

        cmp         r12, #10
        bge         10f

@ Down of Horizontal - works down left
        vld1.8     {d24}, [r2]!
        mov         r1,  r2

        bl          patch_h_down_c_4x4_8
        bl          patch_h_down_c_4x4_8

        sub         r0,  #16
        add         r0,  r0,  r3,  lsl #2
        vld1.8     {d24}, [r1]!
        add         r6,  r4,  #32       @ Force initial load in main loop
        mov         r2,  r1

        bl          patch_h_down_c_4x4_8
        bl          patch_h_down_c_4x4_8
        pop        {r4-r8, pc}

@ Up of Horizontal - works down up
10:
        ldrh        r7,  [r7]
        @ -128 (rather than +128) means we get UL
        @ from L & don't have to offset U
        mov         r8,  #-128
        sub         r8,  r7
        vld1.8     {d16}, [r2]

        push       {r2, r8}
        bl          patch_h_up_c_4x4_8
        bl          patch_h_up_c_4x4_8
        pop        {r2, r8}

        add         r2,  r2,  #8
        sub         r0,  #16
        sub         r8,  r8,  r7,  lsl #2
        vld1.8     {d16}, [r2]
        add         r0,  r0,  r3,  lsl #2
        add         r6,  r4,  #32
        bl          patch_h_up_c_4x4_8
        bl          patch_h_up_c_4x4_8
        pop        {r4-r8, pc}

18:
        cmp         r12, #26
        mov         r5,  #8             @ Loop counter for the "easy" cases
        bge         26f

@ Left of vertical - works down left
        vld1.8     {q8 }, [r1  :128]    @ Up
        ldrh        r7,  [r7]
        mov         r8,  #-128

2:
        cmp         r6,  #32
        ble         1f

        @ For other widths we may want different logic
        asr         r12, r8,  #8
        vmov        q12, q8
        add         r8,  r7
        vext.16     q8,  q8,  #7
        add         r12, r2,  r12, lsl #1
        sub         r6,  #32
        vld1.16    {d16[0]}, [r12]
1:
        vdup.8      d31, r6
        rsb         r12, r6,  #32

        vmull.u8    q0,  d16, d31
        vdup.8      d30, r12
        vmull.u8    q1,  d17, d31
        add         r6,  r4
        vmlal.u8    q0,  d24, d30
        vmlal.u8    q1,  d25, d30

        vrshrn.u16  d0,  q0,  #5
        vrshrn.u16  d1,  q1,  #5

        subs        r5,  #1
        vst1.8     {q0 }, [r0], r3
        bne         2b
        pop        {r4-r8, pc}

@ Right of vertical - works along top - left unused
26:
        vld1.8     {q12}, [r1  :128]!   @ Up
2:
        cmp         r6,  #32
        ble         1f

        vmov        q8,  q12
        vext.16     q12, q12, #1
        sub         r6,  #32
        vld1.16    {d25[3]}, [r1]!

1:
        rsb         r12, r6,  #32
        vdup.8      d30, r6
        vdup.8      d31, r12

        vmull.u8    q0,  d24, d30
        vmull.u8    q1,  d25, d30
        vmlal.u8    q0,  d16, d31
        vmlal.u8    q1,  d17, d31

        vrshrn.u16  d0,  q0,  #5
        vrshrn.u16  d1,  q1,  #5

        add         r6,  r4
        subs        r5,  #1
        vst1.8     {q0 }, [r0], r3
        bne         2b
        pop        {r4-r8, pc}

endfunc


@ ff_hevc_rpi_pred_angular_c_16_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride        [r3]
@       unsigned int mode       [sp, #0]  2..34

function ff_hevc_rpi_pred_angular_c_16_neon_8, export=1
        ldr         r12, [sp, #0]
        push       {r4-r10, lr}
        adrl        r4,  angle_2 - 2
        adrl        r7,  inv_angle - 11*2
        ldrsb       r4,  [r4, r12]
        add         r7,  r7,  r12, lsl #1
        lsl         r3,  #1

        cmp         r12, #18
        bge         18f

        cmp         r12, #10
        mov         r10, #4             @ Outer loop counter for "hard" cases
        bge         10f

@ Down of Horizontal - works down left
        mov         r1,  r2
2:
        vld1.8     {d24}, [r1]!
        add         r6,  r4,  #32       @ Force initial load in main loop
        mov         r2,  r1

        bl          patch_h_down_c_4x4_8
        bl          patch_h_down_c_4x4_8
        bl          patch_h_down_c_4x4_8
        bl          patch_h_down_c_4x4_8

        sub         r0,  #32
        subs        r10, #1
        add         r0,  r0,  r3,  lsl #2
        bne         2b
        pop        {r4-r10, pc}

@ Up of Horizontal - works down up
10:
        ldrh        r7,  [r7]
        @ -128 (rather than +128) means we get UL
        @ from L & don't have to offset U
        mov         r8,  #-128
        sub         r8,  r7
2:
        vld1.8     {d16}, [r2]
        add         r6,  r4,  #32

        push       {r2, r8}
        bl          patch_h_up_c_4x4_8
        bl          patch_h_up_c_4x4_8
        bl          patch_h_up_c_4x4_8
        bl          patch_h_up_c_4x4_8
        pop        {r2, r8}

        sub         r0,  #32
        subs        r10, #1
        add         r2,  r2,  #8
        sub         r8,  r8,  r7,  lsl #2
        add         r0,  r0,  r3,  lsl #2
        bne         2b
        pop        {r4-r10, pc}

18:
        cmp         r12, #26
        mov         r5,  #16            @ Loop counter for the "easy" cases
        bge         26f

@ Left of vertical - works down left
        vld1.8     {q8,  q9 }, [r1  :128]    @ Up
        ldrh        r7,  [r7]
        add         r6,  r4,  #32
        mov         r8,  #-128

2:
        cmp         r6,  #32
        ble         1f

        @ For other widths we may want different logic
        asr         r9,  r8,  #8
        vmov        q12, q8
        add         r8,  r7
        vmov        q13, q9
        add         r9,  r2,  r9,  lsl #1
        vext.16     q9,  q8,  q9,  #7
        sub         r6,  #32
        vext.16     q8,  q8,  q8,  #7
        vld1.16    {d16[0]}, [r9]

1:
        vdup.8      d31, r6
        rsb         r12, r6,  #32

        vmull.u8    q0,  d16, d31
        vmull.u8    q1,  d17, d31
        vdup.8      d30, r12
        add         r6,  r4
        vmull.u8    q2,  d18, d31
        vmull.u8    q3,  d19, d31
        vmlal.u8    q0,  d24, d30
        vmlal.u8    q1,  d25, d30
        vmlal.u8    q2,  d26, d30
        vmlal.u8    q3,  d27, d30

        vrshrn.u16  d0,  q0,  #5
        vrshrn.u16  d1,  q1,  #5
        vrshrn.u16  d2,  q2,  #5
        vrshrn.u16  d3,  q3,  #5

        subs        r5,  #1
        vst1.8     {q0,  q1 }, [r0], r3
        bne         2b
        pop        {r4-r10, pc}

@ Right of vertical - works along top - left unused
26:
        vld1.8     {q12, q13}, [r1  :128]!   @ Up
        add         r6,  r4,  #32       @ Force initial load in main loop
2:
        cmp         r6,  #32
        ble         1f

        vmov        q8,  q12
        vmov        q9,  q13
        vext.16     q12, q13, #1
        vext.16     q13, q13, #1
        sub         r6,  #32
        vld1.16    {d27[3]}, [r1]!

1:
        rsb         r12, r6,  #32
        vdup.8      d30, r6
        vdup.8      d31, r12

        vmull.u8    q0,  d24, d30
        vmull.u8    q1,  d25, d30
        vmull.u8    q2,  d26, d30
        vmull.u8    q3,  d27, d30
        vmlal.u8    q0,  d16, d31
        vmlal.u8    q1,  d17, d31
        vmlal.u8    q2,  d18, d31
        vmlal.u8    q3,  d19, d31

        vrshrn.u16  d0,  q0,  #5
        vrshrn.u16  d1,  q1,  #5
        vrshrn.u16  d2,  q2,  #5
        vrshrn.u16  d3,  q3,  #5

        add         r6,  r4
        subs        r5,  #1
        vst1.8     {q0,  q1 }, [r0], r3
        bne         2b
        pop        {r4-r10, pc}

endfunc

@------------------------------------------------------------------------------
@ Data

        .text
        .balign  64
angle_2:
        .byte    32
        .byte    26,  21,  17,  13,   9,   5,   2,   0
        @ Sign inverted from standards table
        .byte     2,   5,   9,  13,  17,  21,  26,  32
        .byte    26,  21,  17,  13,   9,   5,   2,   0
        @ Standard sign
        .byte     2,   5,   9,  13,  17,  21,  26,  32

        @ Sign inverted from standards table
inv_angle:
        .short   4096, 1638,  910,  630,  482,  390,  315
        .short    256
        .short    315,  390,  482,  630,  910, 1638, 4096

@------------------------------------------------------------------------------
@
@ 10 bit fns
@ Should work for 9 & 11 bit as there is no actual bit-depth specific code
@ but runs out of register width for 12+ bit

        .text
        .balign 64

patch_h_down_4x4_10:
        mov         r5,  #4
2:
        cmp         r6,  #32
        ble         1f

        vmov        d16, d24
        vext.16     d24, d24, #1
        sub         r6,  #32
        vld1.16    {d24[3]}, [r2]!

1:
        rsb         r12, r6,  #32
        vext.16     q1,  q2,  #4
        vmov        s0,  r6
        vmov        s1,  r12
        vext.16     q2,  q2,  #4

        vmul.u16    d1,  d24, d0[0]
        add         r6,  r4
        vmla.u16    d1,  d16, d0[2]
        subs        r5,  #1
        vrshr.u16   d5,  d1,  #5
        bne         2b

store_tran_4x4_10:
        add         r12, r0,  r3
        vst4.16    {d2[0], d3[0], d4[0], d5[0]}, [r0 ]!
        add         r5,  r12, r3
        vst4.16    {d2[1], d3[1], d4[1], d5[1]}, [r12]
        add         r12, r12, r3,  lsl #1
        vst4.16    {d2[2], d3[2], d4[2], d5[2]}, [r5 ]
        vst4.16    {d2[3], d3[3], d4[3], d5[3]}, [r12]
        bx          lr

patch_h_up_4x4_10:
        mov         r5,  #4
2:
        cmp         r6,  #32
        ble         1f

        @ If r8 is -ve then we are still tracking left
        adds        r8,  r7
        vmov        d24, d16
        @ Initially r2=left (variable), r1=up (const)
        @ Use r2 for both up and left, we only ever go from left->up so
        @ we assume that we are left and thenm overwrite with up if wanted
        sub         r2,  #2
        addpl       r2,  r1,  r8,  asr #7
        vext.16     d16, d16, d16, #3
        @ We get *2 by >> 7 rather than 8, but that means we need to lose bit 0
        and         r2,  #~1
        sub         r6,  #32
        vld1.16     d16[0], [r2]

1:
        rsb         r12, r6,  #32
        vext.16     q1,  q2,  #4
        vmov        s0,  r6
        vmov        s1,  r12
        vext.16     q2,  q2,  #4

        vmul.u16    d1,  d24, d0[2]
        add         r6,  r4
        vmla.u16    d1,  d16, d0[0]
        subs        r5,  #1
        vrshr.u16   d5,  d1,  #5
        bne         2b
        b           store_tran_4x4_10  @ This will return


@ ff_hevc_rpi_pred_angular_4_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride        [r3]
@       unsigned int mode       [sp, #0]  2..34

function ff_hevc_rpi_pred_angular_4_neon_10, export=1
        ldr         r12, [sp, #0]
        push       {r4-r8, lr}
        adrl        r4,  angle_2 - 2
        adrl        r7,  inv_angle - 11*2
        lsl         r3,  #1
        ldrsb       r4,  [r4, r12]
        add         r7,  r7,  r12, lsl #1

        cmp         r12, #18
        add         r6,  r4,  #32       @ Force initial load in main loop
        bge         18f

        cmp         r12, #10
        bge         10f

@ Down of Horizontal - works down left
        vld1.16    {d24}, [r2]!
        bl          patch_h_down_4x4_10
        pop        {r4-r8, pc}

@ Up of Horizontal - works down up
10:
        ldrh        r7,  [r7]
        @ -128 (rather than +128) means we get UL
        @ from L & don't have to offset U
        mov         r8,  #-128
        sub         r8,  r7
        vld1.16    {d16}, [r2]
        bl          patch_h_up_4x4_10
        pop        {r4-r8, pc}

18:
        cmp         r12, #26
        mov         r5,  #4             @ Loop counter for the "easy" cases
        bge         26f

@ Left of vertical - works down left
        vld1.16    {d16}, [r1]       @ Up
        ldrh        r7,  [r7]
        mov         r8,  #-128

2:
        cmp         r6,  #32
        ble         1f

        asr         r12, r8,  #8
        vmov        d24, d16
        add         r8,  r7
        add         r12, r2,  r12, lsl #1
        sub         r6,  #32
        vext.16     d16, d16, #3
        vld1.16    {d16[0]}, [r12]
1:
        vmov        s1,  r6
        rsb         r12, r6,  #32
        add         r6,  r4
        vmov        s0,  r12

        vmul.u16    d2,  d16, d0[2]
        vmla.u16    d2,  d24, d0[0]
        vrshr.u16   d2,  #5

        subs        r5,  #1
        vst1.16    {d2 }, [r0], r3
        bne         2b
        pop        {r4-r8, pc}

@ Right of vertical - works along top - left unused
26:
        vld1.16    {d24, d25}, [r1  :64]  @ Up + UR (64bit aligned)
2:
        cmp         r6,  #32
        ble         1f

        vmov        d16, d24
        vext.16     q12, q13, #1
        sub         r6,  #32

1:
        rsb         r12, r6,  #32
        vmov        s0,  r6           @ Have to use d0-d7 for scalar multiply
        vmov        s1,  r12

        vmul.u16    d2,  d24, d0[0]
        vmla.u16    d2,  d16, d0[2]
        vrshr.u16   d2,  #5

        add         r6,  r4
        subs        r5,  #1
        vst1.16    {d2 }, [r0], r3
        bne         2b
        pop        {r4-r8, pc}

endfunc


@ ff_hevc_rpi_pred_angular_8_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride        [r3]
@       unsigned int mode       [sp, #0]  2..34

function ff_hevc_rpi_pred_angular_8_neon_10, export=1
        ldr         r12, [sp, #0]
        push       {r4-r8, lr}
        adrl        r4,  angle_2 - 2
        adrl        r7,  inv_angle - 11*2
        lsl         r3,  #1
        ldrsb       r4,  [r4, r12]
        add         r7,  r7,  r12, lsl #1

        cmp         r12, #18
        add         r6,  r4,  #32
        bge         18f

        cmp         r12, #10
        bge         10f

@ Down of Horizontal - works down left
        vld1.16    {d24}, [r2]!
        mov         r1,  r2
        bl          patch_h_down_4x4_10
        bl          patch_h_down_4x4_10

        vld1.16    {d24}, [r1]!
        sub         r0,  #16
        add         r6,  r4,  #32       @ Force initial load in main loop
        add         r0,  r0,  r3,  lsl #2
        mov         r2,  r1
        bl          patch_h_down_4x4_10
        bl          patch_h_down_4x4_10
        pop        {r4-r8, pc}

@ Up of Horizontal - works down up
10:
        ldrh        r7,  [r7]
        @ -128 (rather than +128) means we get UL
        @ from L & don't have to offset U
        mov         r8,  #-128
        sub         r8,  r7
        vld1.16    {d16}, [r2]

        push       {r2, r8}
        bl          patch_h_up_4x4_10
        bl          patch_h_up_4x4_10
        pop        {r2, r8}

        sub         r0,  #16
        add         r2,  #8
        sub         r8,  r8,  r7,  lsl #2
        add         r0,  r0,  r3,  lsl #2
        vld1.16    {d16}, [r2]
        add         r6,  r4,  #32
        bl          patch_h_up_4x4_10
        bl          patch_h_up_4x4_10
        pop        {r4-r8, pc}

18:
        cmp         r12, #26
        mov         r5,  #8             @ Loop counter for the "easy" cases
        bge         26f

@ Left of vertical - works down left
        vld1.16    {q8 }, [r1]          @ Up
        ldrh        r7,  [r7]
        mov         r8,  #-128

2:
        cmp         r6,  #32
        ble         1f

        asr         r12, r8,  #8
        vmov        q12, q8
        add         r8,  r7
        add         r12, r2,  r12, lsl #1
        sub         r6,  #32
        vext.16     q8,  q8,  q8,  #7
        vld1.16    {d16[0]}, [r12]
1:
        vmov        s1,  r6
        rsb         r12, r6,  #32
        add         r6,  r4
        vmov        s0,  r12

        vmul.u16    q1,  q8,  d0[2]
        vmla.u16    q1,  q12, d0[0]
        vrshr.u16   q1,  #5

        subs        r5,  #1
        vst1.16    {q1 }, [r0], r3
        bne         2b
        pop        {r4-r8, pc}

@ Right of vertical - works along top - left unused
26:
        vld1.16    {q12, q13}, [r1  :128]    @ Up + UR
2:
        cmp         r6,  #32
        ble         1f

        vmov        q8,  q12
        vext.16     q12, q13, #1
        sub         r6,  #32
        vext.16     q13, q13, #1
1:
        rsb         r12, r6,  #32
        vmov        s0,  r6             @ Have to use d0-d7 for scalar multiply
        vmov        s1,  r12

        vmul.u16    q1,  q12, d0[0]
        vmla.u16    q1,  q8,  d0[2]
        vrshr.u16   q1,  #5

        add         r6,  r4
        subs        r5,  #1
        vst1.16    {q1 }, [r0], r3
        bne         2b
        pop        {r4-r8, pc}

endfunc


@ ff_hevc_rpi_pred_angular_16_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride        [r3]
@       unsigned int mode       [sp, #0]  2..34

function ff_hevc_rpi_pred_angular_16_neon_10, export=1
        ldr         r12, [sp, #0]
        push       {r4-r10, lr}
        adrl        r4,  angle_2 - 2
        adrl        r7,  inv_angle - 11*2
        lsl         r3,  #1
        ldrsb       r4,  [r4, r12]
        add         r7,  r7,  r12, lsl #1

        cmp         r12, #18
        bge         18f

        cmp         r12, #10
        mov         r10, #4             @ Outer loop counter for "hard" cases
        bge         10f

@ Down of Horizontal - works down left
        mov         r1,  r2
2:
        vld1.16    {d24}, [r1]!
        add         r6,  r4,  #32       @ Force initial load in main loop
        mov         r2,  r1
        bl          patch_h_down_4x4_10
        bl          patch_h_down_4x4_10
        bl          patch_h_down_4x4_10
        bl          patch_h_down_4x4_10

        sub         r0,  #32
        subs        r10, #1
        add         r0,  r0,  r3,  lsl #2
        bne         2b
        pop        {r4-r10, pc}

@ Up of Horizontal - works down up
10:
        ldrh        r7,  [r7]
        @ -128 (rather than +128) means we get UL
        @ from L & don't have to offset U
        mov         r8,  #-128
        sub         r8,  r7
2:
        vld1.16    {d16}, [r2]
        add         r6,  r4,  #32

        push       {r2, r8}
        bl          patch_h_up_4x4_10
        bl          patch_h_up_4x4_10
        bl          patch_h_up_4x4_10
        bl          patch_h_up_4x4_10
        pop        {r2, r8}

        sub         r0,  #32
        subs        r10, #1
        add         r2,  #8
        sub         r8,  r8,  r7,  lsl #2
        add         r0,  r0,  r3,  lsl #2
        bne         2b
        pop        {r4-r10, pc}

18:
        cmp         r12, #26
        mov         r5,  #16            @ Loop counter for the "easy" cases
        bge         26f

@ Left of vertical - works down left
        vld1.16    {q8,  q9}, [r1]       @ Up
        ldrh        r7,  [r7]
        add         r6,  r4,  #32
        mov         r8,  #-128

2:
        cmp         r6,  #32
        ble         1f

        asr         r9,  r8,  #8
        vmov        q12, q8
        add         r8,  r7
        vmov        q13, q9
        add         r9,  r2,  r9,  lsl #1
        sub         r6,  #32
        vext.16     q9,  q8,  q9,  #7
        vext.16     q8,  q8,  q8,  #7
        vld1.16    {d16[0]}, [r9]
1:
        vmov        s1,  r6
        rsb         r12, r6,  #32
        add         r6,  r4
        vmov        s0,  r12

        vmul.u16    q1,  q8,  d0[2]
        vmul.u16    q2,  q9,  d0[2]
        vmla.u16    q1,  q12, d0[0]
        vmla.u16    q2,  q13, d0[0]

        vrshr.u16   q1,  #5
        vrshr.u16   q2,  #5

        subs        r5,  #1
        vst1.16    {q1,  q2 }, [r0], r3
        bne         2b
        pop        {r4-r10, pc}

@ Right of vertical - works along top - left unused
26:
        vld1.16    {q12, q13}, [r1  :128]!    @ Up
        add         r6,  r4,  #32       @ Force initial load in main loop
2:
        cmp         r6,  #32
        ble         1f

        vmov        q8,  q12
        vmov        q9,  q13
        vext.16     q12, q13, #1
        vext.16     q13, q13, #1
        sub         r6,  #32
        vld1.16    {d27[3]}, [r1]!

1:
        rsb         r12, r6,  #32
        vmov        s0,  r6           @ Have to use d0-d7 for scalar multiply
        vmov        s1,  r12

        vmul.u16    q1,  q12, d0[0]
        vmul.u16    q2,  q13, d0[0]
        vmla.u16    q1,  q8,  d0[2]
        vmla.u16    q2,  q9,  d0[2]

        vrshr.u16   q1,  #5
        vrshr.u16   q2,  #5

        add         r6,  r4
        subs        r5,  #1
        vst1.16    {q1,  q2 }, [r0], r3
        bne         2b
        pop        {r4-r10, pc}

endfunc


@ ff_hevc_rpi_pred_angular_32_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride        [r3]
@       unsigned int mode       [sp, #0]  2..34

function ff_hevc_rpi_pred_angular_32_neon_10, export=1
        ldr         r12, [sp, #0]
        push       {r4-r10, lr}
        vpush      {q4 }
        adrl        r4,  angle_2 - 2
        adrl        r7,  inv_angle - 11*2
        lsl         r3,  #1
        ldrsb       r4,  [r4, r12]
        add         r7,  r7,  r12, lsl #1

        cmp         r12, #18
        bge         18f

        cmp         r12, #10
        mov         r10, #8             @ Outer loop counter for "hard" cases
        bge         10f

@ Down of Horizontal - works down left
        mov         r1,  r2
2:
        vld1.16    {d24}, [r1]!
        add         r6,  r4,  #32       @ Force initial load in main loop
        mov         r2,  r1
        mov         r9,  #4
1:
        bl          patch_h_down_4x4_10
        bl          patch_h_down_4x4_10
        subs        r9,  #1
        bne         1b

        sub         r0,  #64
        subs        r10, #1
        add         r0,  r0,  r3,  lsl #2
        bne         2b
        b           99f

@ Up of Horizontal - works down up
10:
        ldrh        r7,  [r7]
        @ -128 (rather than +128) means we get UL
        @ from L & don't have to offset U
        mov         r8,  #-128
        sub         r8,  r7
2:
        vld1.16    {d16}, [r2]
        add         r6,  r4,  #32

        push       {r2, r8}
        mov         r9,  #4
1:
        bl          patch_h_up_4x4_10
        bl          patch_h_up_4x4_10
        subs        r9,  #1
        bne         1b
        pop        {r2, r8}

        sub         r0,  #64
        subs        r10, #1
        add         r2,  #8
        sub         r8,  r8,  r7,  lsl #2
        add         r0,  r0,  r3,  lsl #2
        bne         2b
        b           99f

18:
        cmp         r12, #26
        mov         r5,  #32            @ Loop counter for the "easy" cases
        bge         26f

@ Left of vertical - works down left
        vldm        r1, {q8-q11}        @ Up
        ldrh        r7,  [r7]
        add         r6,  r4,  #32
        mov         r8,  #-128

2:
        cmp         r6,  #32
        ble         1f

        asr         r9,  r8,  #8
        vmov        q12, q8
        add         r8,  r7
        vmov        q13, q9
        add         r9,  r2,  r9,  lsl #1
        vmov        q14, q10
        vmov        q15, q11
        sub         r6,  #32
        vext.16     q11, q10, q11, #7
        vext.16     q10, q9,  q10, #7
        vext.16     q9,  q8,  q9,  #7
        vext.16     q8,  q8,  q8,  #7
        vld1.16    {d16[0]}, [r9]

1:
        vmov        s1,  r6
        rsb         r12, r6,  #32
        add         r6,  r4
        vmov        s0,  r12

        vmul.u16    q1,  q8,  d0[2]
        vmul.u16    q2,  q9,  d0[2]
        vmul.u16    q3,  q10, d0[2]
        vmul.u16    q4,  q11, d0[2]
        vmla.u16    q1,  q12, d0[0]
        vmla.u16    q2,  q13, d0[0]
        vmla.u16    q3,  q14, d0[0]
        vmla.u16    q4,  q15, d0[0]

        vrshr.u16   q1,  #5
        vrshr.u16   q2,  #5
        vrshr.u16   q3,  #5
        vrshr.u16   q4,  #5

        subs        r5,  #1
        vstm        r0, {q1-q4}
        add         r0,  r3
        bne         2b
        b           99f

@ Right of vertical - works along top - left unused
26:
        vldm        r1, {q12-q15}       @ Up
        add         r6,  r4,  #32       @ Force initial load in main loop
        add         r1,  #64
2:
        cmp         r6,  #32
        ble         1f

        vmov        q8,  q12
        vmov        q9,  q13
        vmov        q10, q14
        vmov        q11, q15
        vext.16     q12, q13, #1
        vext.16     q13, q14, #1
        vext.16     q14, q15, #1
        vext.16     q15, q15, #1
        sub         r6,  #32
        vld1.16    {d31[3]}, [r1]!
1:
        rsb         r12, r6,  #32
        vmov        s0,  r6           @ Have to use d0-d7 for scalar multiply
        vmov        s1,  r12

        vmul.u16    q1,  q12, d0[0]
        vmul.u16    q2,  q13, d0[0]
        vmul.u16    q3,  q14, d0[0]
        vmul.u16    q4,  q15, d0[0]
        vmla.u16    q1,  q8,  d0[2]
        vmla.u16    q2,  q9,  d0[2]
        vmla.u16    q3,  q10, d0[2]
        vmla.u16    q4,  q11, d0[2]

        vrshr.u16   q1,  #5
        vrshr.u16   q2,  #5
        vrshr.u16   q3,  #5
        vrshr.u16   q4,  #5

        add         r6,  r4
        subs        r5,  #1
        vstm        r0, {q1-q4}
        add         r0,  r3
        bne         2b
99:
        vpop       {q4 }
        pop        {r4-r10, pc}

endfunc



@ Generate 4x4 chroma patch
@
@ In (const)
@ r1   Up ptr (_up only)
@ r3   Out stride
@ r4   Angle add
@ r7   Inv angle (_up only)
@
@ In/Out (updated)
@ r0   Out pointer - on exit point to start of next patch horizontally (i.e. r0 + patch width)
@ r2   Left ptr - updated
@ r6   Angle frac (init to r4 + 32)
@ r8   Inv angle accumulator
@ q2   Cur Line - load before 1st call for down - set by _up
@ q8   Cur Line - load before 1st call for up   - set by _down
@
@ Temps
@ r5   Loop counter
@ r12
@ d0, q1, q12-q15

patch_h_down_c_4x4_10:
        mov         r5,  #4
2:
        cmp         r6,  #32
        ble         1f

        vmov        q8,  q2
        vext.32     q2,  q2, #1
        sub         r6,  #32
        vld1.32    {d5[1]}, [r2]!
1:
        rsb         r12, r6,  #32
        vmov        q12, q13
        vmov        s0,  r6
        vmov        s1,  r12
        vmov        q13, q14

        vmul.u16    q3,  q2,  d0[0]
        add         r6,  r4
        vmla.u16    q3,  q8,  d0[2]
        vmov        q14, q15
        subs        r5,  #1
        vrshr.u16   q15, q3,  #5
        bne         2b

store_tran_c_4x4_10:
        add         r12, r0,  r3
        vst4.32    {d24[0], d26[0], d28[0], d30[0]}, [r0 ]!
        add         r5,  r12, r3
        vst4.32    {d24[1], d26[1], d28[1], d30[1]}, [r12]
        add         r12, r12, r3,  lsl #1
        vst4.32    {d25[0], d27[0], d29[0], d31[0]}, [r5 ]
        vst4.32    {d25[1], d27[1], d29[1], d31[1]}, [r12]
        bx          lr

patch_h_up_c_4x4_10:
        mov         r5,  #4
2:
        cmp         r6,  #32
        ble         1f

        @ If r8 is -ve then we are still tracking left
        adds        r8,  r7
        vmov        q2,  q8
        @ Initially r2=left (variable), r1=up (const)
        @ Use r2 for both up and left, we only ever go from left->up so
        @ we assume that we are left and thenm overwrite with up if wanted
        sub         r2,  #4
        addpl       r2,  r1,  r8,  asr #6
        vext.32     q8,  q8,  #3
        @ We get *4 by >> 6 rather than 8, but that means we need to lose bits 0 & 1
        and         r2,  #~3
        sub         r6,  #32
        vld1.32     d16[0], [r2]
1:
        rsb         r12, r6,  #32
        vmov        q12, q13
        vmov        s0,  r6
        vmov        s1,  r12
        vmov        q13, q14

        vmul.u16    q1,  q2,  d0[2]
        add         r6,  r4
        vmla.u16    q1,  q8,  d0[0]
        vmov        q14, q15
        subs        r5,  #1
        vrshr.u16   q15, q1,  #5
        bne         2b
        b           store_tran_c_4x4_10  @ This will return



@ ff_hevc_rpi_pred_angular_c_4_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride        [r3]
@       unsigned int mode       [sp, #0]  2..34

function ff_hevc_rpi_pred_angular_c_4_neon_10, export=1
        ldr         r12, [sp, #0]
        push       {r4-r8, lr}
        adrl        r4,  angle_2 - 2
        adrl        r7,  inv_angle - 11*2
        lsl         r3,  #2
        ldrsb       r4,  [r4, r12]
        add         r7,  r7,  r12, lsl #1

        cmp         r12, #18
        add         r6,  r4,  #32
        bge         18f

        cmp         r12, #10
        bge         10f

@ Down of Horizontal - works down left
        vld1.32    {q2 }, [r2]!
        bl          patch_h_down_c_4x4_10
        pop        {r4-r8, pc}

@ Up of Horizontal - works down up
10:
        ldrh        r7,  [r7]
        @ -128 (rather than +128) means we get UL
        @ from L & don't have to offset U
        mov         r8,  #-128
        sub         r8,  r7
        vld1.32    {q8 }, [r2]
        bl          patch_h_up_c_4x4_10
        pop        {r4-r8, pc}

18:
        cmp         r12, #26
        mov         r5,  #4             @ Loop counter for the "easy" cases
        bge         26f

@ Left of vertical - works down left
        vld1.16    {q8 }, [r1]        @ Up
        ldrh        r7,  [r7]
        mov         r8,  #-128

2:
        cmp         r6,  #32
        ble         1f

        asr         r12, r8,  #8
        vmov        q12, q8
        add         r8,  r7
        vext.32     q8,  q8,  q8,  #3
        add         r12, r2,  r12, lsl #2
        sub         r6,  #32
        vld1.32    {d16[0]}, [r12]

1:
        vmov        s1,  r6
        rsb         r12, r6,  #32
        add         r6,  r4
        vmov        s0,  r12

        vmul.u16    q1,  q8,  d0[2]
        vmla.u16    q1,  q12, d0[0]
        vrshr.u16   q1,  #5

        subs        r5,  #1
        vst1.16    {q1 }, [r0], r3
        bne         2b
        pop        {r4-r8, pc}

@ Right of vertical - works along top - left unused
26:
        vld1.16    {q12, q13}, [r1]     @ Up + UR
2:
        cmp         r6,  #32
        ble         1f

        vmov        q8,  q12
        vext.32     q12, q13, #1
        vext.32     q13, q13, #1
        sub         r6,  #32

1:
        rsb         r12, r6,  #32
        vmov        s0,  r6           @ Have to use d0-d7 for scalar multiply
        vmov        s1,  r12

        vmul.u16    q1,  q12, d0[0]
        vmla.u16    q1,  q8,  d0[2]
        vrshr.u16   q1,  #5

        add         r6,  r4
        subs        r5,  #1
        vst1.16    {q1 }, [r0], r3
        bne         2b
        pop        {r4-r8, pc}

endfunc


@ ff_hevc_rpi_pred_angular_c_8_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride        [r3]
@       unsigned int mode       [sp, #0]  2..34

function ff_hevc_rpi_pred_angular_c_8_neon_10, export=1
        ldr         r12, [sp, #0]
        push       {r4-r8, lr}
        adrl        r4,  angle_2 - 2
        adrl        r7,  inv_angle - 11*2
        lsl         r3,  #2
        ldrsb       r4,  [r4, r12]
        add         r7,  r7,  r12, lsl #1

        cmp         r12, #18
        add         r6,  r4,  #32       @ Force initial load in main loop
        bge         18f

        cmp         r12, #10
        bge         10f

@ Down of Horizontal - works down left
        vld1.32    {q2 }, [r2]!
        mov         r1,  r2
        bl          patch_h_down_c_4x4_10
        bl          patch_h_down_c_4x4_10

        vld1.32    {q2 }, [r1]!
        sub         r0,  #32
        add         r6,  r4,  #32       @ Force initial load in main loop
        add         r0,  r0,  r3,  lsl #2
        mov         r2,  r1
        bl          patch_h_down_c_4x4_10
        bl          patch_h_down_c_4x4_10
        pop        {r4-r8, pc}

@ Up of Horizontal - works down up
10:
        ldrh        r7,  [r7]
        @ -128 (rather than +128) means we get UL
        @ from L & don't have to offset U
        mov         r8,  #-128
        sub         r8,  r7
        vld1.32    {q8 }, [r2]

        push       {r2, r8}
        bl          patch_h_up_c_4x4_10
        bl          patch_h_up_c_4x4_10
        pop        {r2, r8}

        sub         r0,  #32
        add         r2,  #16
        sub         r8,  r8,  r7,  lsl #2
        add         r0,  r0,  r3,  lsl #2
        vld1.32    {q8 }, [r2]
        add         r6,  r4,  #32

        bl          patch_h_up_c_4x4_10
        bl          patch_h_up_c_4x4_10
        pop        {r4-r8, pc}

18:
        cmp         r12, #26
        mov         r5,  #8             @ Loop counter for the "easy" cases
        bge         26f

@ Left of vertical - works down left
        vld1.16    {q8,  q9 }, [r1]        @ Up
        ldrh        r7,  [r7]
        mov         r8,  #-128

2:
        cmp         r6,  #32
        ble         1f

        vmov        q12, q8
        asr         r12, r8,  #8
        vmov        q13, q9
        add         r8,  r7
        vext.32     q9,  q8,  q9,  #3
        add         r12, r2,  r12, lsl #2
        vext.32     q8,  q8,  q8,  #3
        sub         r6,  #32
        vld1.32    {d16[0]}, [r12]
1:
        vmov        s1,  r6
        rsb         r12, r6,  #32
        add         r6,  r4
        vmov        s0,  r12

        vmul.u16    q1,  q8,  d0[2]
        vmul.u16    q2,  q9,  d0[2]
        vmla.u16    q1,  q12, d0[0]
        vmla.u16    q2,  q13, d0[0]
        vrshr.u16   q1,  #5
        vrshr.u16   q2,  #5

        subs        r5,  #1
        vst1.16    {q1,  q2 }, [r0], r3
        bne         2b
        pop        {r4-r8, pc}

@ Right of vertical - works along top - left unused
26:
        vld1.16    {q12, q13}, [r1]!    @ Up
2:
        cmp         r6,  #32
        ble         1f

        vmov        q8,  q12
        vmov        q9,  q13
        vext.32     q12, q13, #1
        vext.32     q13, q14, #1
        sub         r6,  #32
        vld1.32    {d27[1]}, [r1]!

1:
        rsb         r12, r6,  #32
        vmov        s0,  r6           @ Have to use d0-d7 for scalar multiply
        vmov        s1,  r12

        vmul.u16    q1,  q12, d0[0]
        vmul.u16    q2,  q13, d0[0]
        vmla.u16    q1,  q8,  d0[2]
        vmla.u16    q2,  q9,  d0[2]
        vrshr.u16   q1,  #5
        vrshr.u16   q2,  #5

        add         r6,  r4
        subs        r5,  #1
        vst1.16    {q1,  q2 }, [r0], r3
        bne         2b
        pop        {r4-r8, pc}

endfunc


@ ff_hevc_rpi_pred_angular_c_16_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride        [r3]
@       unsigned int mode       [sp, #0]  2..34

function ff_hevc_rpi_pred_angular_c_16_neon_10, export=1
        ldr         r12, [sp, #0]
        push       {r4-r10, lr}
        vpush      {q4 }
        adrl        r4,  angle_2 - 2
        adrl        r7,  inv_angle - 11*2
        lsl         r3,  #2
        ldrsb       r4,  [r4, r12]
        add         r7,  r7,  r12, lsl #1

        cmp         r12, #18
        bge         18f

        cmp         r12, #10
        mov         r10, #4             @ Outer loop counter for "hard" cases
        bge         10f

@ Down of Horizontal - works down left
        mov         r1,  r2
2:
        vld1.32    {q2 }, [r1]!
        add         r6,  r4,  #32       @ Force initial load in main loop
        mov         r2,  r1
        bl          patch_h_down_c_4x4_10
        bl          patch_h_down_c_4x4_10
        bl          patch_h_down_c_4x4_10
        bl          patch_h_down_c_4x4_10

        sub         r0,  #64
        subs        r10, #1
        add         r0,  r0,  r3,  lsl #2
        bne         2b
        b           99f

@ Up of Horizontal - works down up
10:
        ldrh        r7,  [r7]
        @ -128 (rather than +128) means we get UL
        @ from L & don't have to offset U
        mov         r8,  #-128
        sub         r8,  r7
2:
        vld1.32    {q8 }, [r2]
        add         r6,  r4,  #32

        push       {r2, r8}
        bl          patch_h_up_c_4x4_10
        bl          patch_h_up_c_4x4_10
        bl          patch_h_up_c_4x4_10
        bl          patch_h_up_c_4x4_10
        pop        {r2, r8}

        sub         r0,  #64
        subs        r10, #1
        add         r2,  #16
        sub         r8,  r8,  r7,  lsl #2
        add         r0,  r0,  r3,  lsl #2
        bne         2b
        b           99f

18:
        cmp         r12, #26
        mov         r5,  #16            @ Loop counter for the "easy" cases
        bge         26f

@ Left of vertical - works down left
        vldm        r1, {q8-q11}        @ Up
        ldrh        r7,  [r7]
        add         r6,  r4,  #32
        mov         r8,  #-128

2:
        cmp         r6,  #32
        ble         1f

        asr         r9,  r8,  #8
        vmov        q12, q8
        add         r8,  r7
        vmov        q13, q9
        add         r9,  r2,  r9,  lsl #2
        vmov        q14, q10
        vmov        q15, q11
        vext.32     q11, q10, q11, #3
        vext.32     q10, q9,  q10, #3
        vext.32     q9,  q8,  q9,  #3
        vext.32     q8,  q8,  q8,  #3
        sub         r6,  #32
        vld1.32    {d16[0]}, [r9]

1:
        vmov        s1,  r6
        rsb         r12, r6,  #32
        add         r6,  r4
        vmov        s0,  r12

        vmul.u16    q1,  q8,  d0[2]
        vmul.u16    q2,  q9,  d0[2]
        vmul.u16    q3,  q10, d0[2]
        vmul.u16    q4,  q11, d0[2]
        vmla.u16    q1,  q12, d0[0]
        vmla.u16    q2,  q13, d0[0]
        vmla.u16    q3,  q14, d0[0]
        vmla.u16    q4,  q15, d0[0]
        vrshr.u16   q1,  #5
        vrshr.u16   q2,  #5
        vrshr.u16   q3,  #5
        vrshr.u16   q4,  #5

        subs        r5,  #1
        vstm        r0, {q1-q4}
        add         r0,  r3
        bne         2b
        b           99f

@ Right of vertical - works along top - left unused
26:
        vldm        r1, {q12-q15}       @ Up
        add         r6,  r4,  #32       @ Force initial load in main loop
        add         r1,  #64
2:
        cmp         r6,  #32
        ble         1f

        vmov        q8,  q12
        vmov        q9,  q13
        vmov        q10, q14
        vmov        q11, q15
        vext.32     q12, q13, #1
        vext.32     q13, q14, #1
        vext.32     q14, q15, #1
        vext.32     q15, q15, #1
        sub         r6,  #32
        vld1.32    {d31[1]}, [r1]!

1:
        rsb         r12, r6,  #32
        vmov        s0,  r6           @ Have to use d0-d7 for scalar multiply
        vmov        s1,  r12

        vmul.u16    q1,  q12, d0[0]
        vmul.u16    q2,  q13, d0[0]
        vmul.u16    q3,  q14, d0[0]
        vmul.u16    q4,  q15, d0[0]
        vmla.u16    q1,  q8,  d0[2]
        vmla.u16    q2,  q9,  d0[2]
        vmla.u16    q3,  q10, d0[2]
        vmla.u16    q4,  q11, d0[2]

        vrshr.u16   q1,  #5
        vrshr.u16   q2,  #5
        vrshr.u16   q3,  #5
        vrshr.u16   q4,  #5

        add         r6,  r4
        subs        r5,  #1
        vstm        r0, {q1-q4}
        add         r0,  r3
        bne         2b
99:
        vpop       {q4 }
        pop        {r4-r10, pc}

endfunc


