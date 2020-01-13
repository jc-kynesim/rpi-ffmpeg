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
@ r10  Inv angle accumulator (_up only)
@ r12  32 - angle frac (_down) or angle frac (_up)
@ d0   Older reference samples
@ d1=r8+r9  Newer reference samples
@ d2   32 - angle frac
@ d3   Angle frac
@ q2   Partially computed next result (_up only)
@
@ Temps
@ r5   Loop counter
@ r6
@ r7   (_down only)
@ r11  (_up only)
@ q2, q8-q11

patch_h_down_8x8_8:
        ldrd        r8, r9, [r2]        @ Left
        rsb         r12, r6, #32
        vmov        d0, r8, r9
        vdup.8      d3, r6
        lsr         r8, #8
        vdup.8      d2, r12
        orr         r8, r8, r9, lsl #24
        ldr         r9, [r2, #5]!
        vmov        d1, r8, r9
        // drop through...
patch_h_down_8x8_8_continue:
        mov         r5, #8
1:
          subs        r12, r4
        vmull.u8    q2, d0, d2
          it          mi
          addmi       r12, #32
        vmlal.u8    q2, d1, d3
          rsb         r6, r12, #32
        vext.8      q8, q8, q9, #8
          itt         mi
          lsrmi       r7, r8, #8
          vmovmi      d0, r8, r9
          vdup.8      d2, r12
        vext.8      q9, q9, q10, #8
          it          mi
          orrmi       r8, r7, r9, lsl #24
        vext.8      q10, q10, q11, #8
          it          mi
          ldrmi       r9, [r2, #1]!
        vmov        d22, d23
        vrshrn.u16  d23, q2, #5
          it          mi
          vmovmi      d1, r8, r9
        subs        r5, #1
          vdup.8      d3, r6
        bne         1b
        // drop through...
store_tran_8x8_8:
        vzip.8      d16, d17
        add         r6, r0, r3
        vzip.8      d18, d19
        lsl         r3, #1
        vzip.8      d20, d21
        add         r5, r0, r3
        vzip.8      d22, d23
        vzip.16     q8, q9
        vzip.16     q10, q11
        vzip.32     q8, q10
        vzip.32     q9, q11
        vst1.8      {d16}, [r0]!
        vst1.8      {d17}, [r6], r3
        vst1.8      {d20}, [r5], r3
        vst1.8      {d21}, [r6], r3
        vst1.8      {d18}, [r5], r3
        vst1.8      {d19}, [r6], r3
        vst1.8      {d22}, [r5]
        asr         r3, #1
        vst1.8      {d23}, [r6]

        bx          lr

patch_h_up_8x8_8:
        ldrd        r8, r9, [r2]
        rsb         r6, r4, #32
        vmov        d0, r8, r9
        vdup.8      d3, r4
        lsr         r11, r8, #24
        vdup.8      d2, r6
        ldr         r8, [r2, #-1]!
        orr         r9, r11, r9, lsl #8
        vmov        d1, r8, r9
        mov         r12, r4
        vmull.u8    q2, d0, d2
        vmlal.u8    q2, d1, d3
patch_h_up_8x8_8_continue:
        mov         r5, #8
1:
          add         r12, r4
          mov         r11, #0
          cmp         r12, #33
          it          cs
          addcs       r10, r7
        vext.8      q8, q8, q9, #8
          itt         cs
          subcs       r12, #32
          tstcs       r10, #1<<31
          rsb         r6, r12, #32
          it          eq
          asreq       r11, r10, #8
          it          cs
          vmovcs      d0, r8, r9
          vdup.8      d2, r6
          it          cs
          lsrcs       r6, r8, #24
        vext.8      q9, q9, q10, #8
          itt         cs
          orrcs       r9, r6, r9, lsl #8
          ldrbcs      r11, [r1, r11]
          vdup.8      d3, r12
        vext.8      q10, q10, q11, #8
          it          hi
          ldrbhi      r11, [r2, #-1]!
        vmov        d22, d23
        vrshrn.u16  d23, q2, #5
          itt         cs
          orrcs       r8, r11, r8, lsl #8
          vmovcs      d1, r8, r9
          vmull.u8    q2, d0, d2
        subs        r5, #1
          vmlal.u8    q2, d1, d3
        bne         1b

        b           store_tran_8x8_8


.macro ADRT reg, val
@ adr in T32 has enough range but not in A32
A       adrl        \reg, \val
T       adr         \reg, \val
.endm

@ ff_hevc_rpi_pred_angular_4_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride        [r3]
@       unsigned int mode       [sp, #0]  2..34

function ff_hevc_rpi_pred_angular_4_neon_8, export=1
        ldr         r12, [sp]
        push        {r4-r8, lr}
        ADRT        r4, angle_2 - 2
        ADRT        r7, inv_angle - 11*2
        add         r7, r7, r12, lsl #1
        ldrsb       r6, [r4, r12]
        cmp         r12, #26
        ldrsb       r4, [r4, r12]
        bge         26f
        cmp         r12, #18
        bge         18f
        cmp         r12, #10
        bge         10f

@ Down of Horizontal - works down left
        ldr         lr, [r2], #1        @ Top
        rsb         r12, r6, #32
        vmov        s0, lr
        vdup.8      d3, r6
        ldr         lr, [r2], #1
        vdup.8      d2, r12
        vmov        s2, lr
          subs        r12, r4
        vmull.u8    q2, d0, d2
          it          mi
          addmi       r12, #32
        vmlal.u8    q2, d1, d3
          rsb         r6, r12, #32
          itt         mi
          vmovmi      s0, lr
          ldrmi       lr, [r2], #1
          vdup.8      d2, r12
          it          mi
          vmovmi      s2, lr
          vdup.8      d3, r6
        mov         r5, #2
1:
        vrshrn.u16  d20, q2, #5
            subs        r12, r4
          vmull.u8    q2, d0, d2
            it          mi
            addmi       r12, #32
          vmlal.u8    q2, d1, d3
            rsb         r6, r12, #32
        vext.64     q8, q8, q9, #1
            it          mi
            vmovmi      s0, lr
        vext.64     q9, q9, q10, #1
            it          mi
            ldrmi       lr, [r2], #1
            vdup.8      d2, r12
            it          mi
            vmovmi      s2, lr
        subs        r5, #1
            vdup.8      d3, r6
        bne         1b

          vrshrn.u16  d20, q2, #5
            vmull.u8    q2, d0, d2
        add         r12, r0,  r3
            vmlal.u8    q2, d1, d3
        lsl         r3,  #1
          vext.64     q8, q8, q9, #1
          vext.64     q9, q9, q10, #1
            vrshrn.u16  d20, q2, #5

98:
        vst4.8      {d17[0], d18[0], d19[0], d20[0]}, [r0], r3
        vst4.8      {d17[1], d18[1], d19[1], d20[1]}, [r12], r3
        vst4.8      {d17[2], d18[2], d19[2], d20[2]}, [r0]
        vst4.8      {d17[3], d18[3], d19[3], d20[3]}, [r12]
        pop        {r4-r8, pc}

@ Up of Horizontal - works down up
10:
        ldrh        r7, [r7]
        rsb         r12, r6, #32
        ldr         lr, [r2]            @ Left
        ldrb        r2, [r2, #-1]       @ Top-left
        vmov        s0, lr
        vdup.8      d2, r12
        vdup.8      d3, r6
        orr         lr, r2, lr, lsl #8
        vmov        s2, lr
        sub         r8, r7, #128
        mov         r5, #3
2:
        vmull.u8    q2, d0, d2
          subs        r12, r4
        vmlal.u8    q2, d1, d3
T         it          mi
          addmi       r12, #32
T         asr         r6, r8, #8
T         it          mi
T         ldrbmi      r2, [r1, r6]
A         ldrbmi      r2, [r1, r8, asr #8]
          rsb         r6, r12, #32
          vdup.8      d2, r12
          ittt        mi
          vmovmi      s0, lr
          orrmi       lr, r2, lr, lsl #8
          vmovmi      s2, lr
        vrshrn.u16  d20, q2, #5
          vdup.8      d3, r6
          it          mi
          addmi       r8, r7
        subs        r5, #1
        vext.64     q8, q8, q9, #1
        vext.64     q9, q9, q10, #1
        bne         2b

          vmull.u8    q2, d0, d2
        add         r12, r0,  r3
          vmlal.u8    q2, d1, d3
        lsl         r3,  #1
          vrshrn.u16  d20, q2, #5
        b           98b

@ Left of vertical - works down left
18:
        ldrh        r7, [r7]
        rsb         r12, r6, #32
        ldr         lr, [r1]            @ Top
        ldrb        r1, [r2, #-1]       @ Top-left
        vmov        s0, lr
        vdup.8      d2, r12
        vdup.8      d3, r6
        orr         lr, r1, lr, lsl #8
        vmov        s2, lr
        sub         r8, r7, #128
        mov         r5, #3
2:
        vmull.u8    q2, d0, d2
          subs        r12, r4
        vmlal.u8    q2, d1, d3
T         it          mi
          addmi       r12, #32
T         asr         r6, r8, #8
T         it          mi
T         ldrbmi      r1, [r2, r6]
A         ldrbmi      r1, [r2, r8, asr #8]
          rsb         r6, r12, #32
          vdup.8      d2, r12
          ittt        mi
          vmovmi      s0, lr
          orrmi       lr, r1, lr, lsl #8
          vmovmi      s2, lr
        vrshrn.u16  d4, q2, #5
          vdup.8      d3, r6
          it          mi
          addmi       r8, r7
        subs        r5, #1
        vst1.32     {d4[0]}, [r0], r3
        bne         2b

          vmull.u8    q2, d0, d2
          vmlal.u8    q2, d1, d3
          vrshrn.u16  d4, q2, #5
          vst1.32     {d4[0]}, [r0]

        pop         {r4-r8, pc}

@ Right of vertical - works along top - left unused
26:
        ldr         lr, [r1], #1        @ Top
        rsb         r12, r6, #32
        vmov        s0, lr
        vdup.8      d3, r6
        ldr         lr, [r1], #1
        vdup.8      d2, r12
        vmov        s2, lr
          subs        r12, r4
        vmull.u8    q2, d0, d2
          it          mi
          addmi       r12, #32
        vmlal.u8    q2, d1, d3
          rsb         r6, r12, #32
          itt         mi
          vmovmi      s0, lr
          ldrmi       lr, [r1], #1
          vdup.8      d2, r12
          it          mi
          vmovmi      s2, lr
          vdup.8      d3, r6
        mov         r5, #2
1:
        vrshrn.u16  d6, q2, #5
            subs        r12, r4
          vmull.u8    q2, d0, d2
            it          mi
            addmi       r12, #32
          vmlal.u8    q2, d1, d3
            rsb         r6, r12, #32
        vst1.32     {d6[0]}, [r0], r3
            itt         mi
            vmovmi      s0, lr
            ldrmi       lr, [r1], #1
            vdup.8      d2, r12
            it          mi
            vmovmi      s2, lr
        subs        r5, #1
            vdup.8      d3, r6
        bne         1b

          vrshrn.u16  d6, q2, #5
            vmull.u8    q2, d0, d2
            vmlal.u8    q2, d1, d3
          vst1.32     {d6[0]}, [r0], r3
            vrshrn.u16  d6, q2, #5
            vst1.32     {d6[0]}, [r0]

        pop         {r4-r8, pc}

endfunc



@ ff_hevc_rpi_pred_angular_8_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride        [r3]
@       unsigned int mode       [sp, #0]  2..34

function ff_hevc_rpi_pred_angular_8_neon_8, export=1
        ldr         r12, [sp]
        push        {r4-r11, lr}
        ADRT        r4, angle_2 - 2
        ADRT        r7, inv_angle - 11*2
        add         r7, r7, r12, lsl #1
        ldrsb       r6, [r4, r12]
        cmp         r12, #26
        ldrsb       r4, [r4, r12]
        bge         26f
        cmp         r12, #18
        bge         18f
        cmp         r12, #10
        bge         10f

@ Down of Horizontal - works down left
        bl          patch_h_down_8x8_8
        pop         {r4-r11, pc}

@ Up of Horizontal - works down up
10:
        ldrh        r7, [r7]
        mov         r10, #-128
        bl          patch_h_up_8x8_8
        pop         {r4-r11, pc}

@ Left of vertical - works down left
18:
        ldrd        r8, r9, [r1]        @ Top
        rsb         r12, r6, #32
        ldrb        lr, [r2, #-1]       @ Top-left
        ldrh        r7, [r7]
        vmov        d0, r8, r9
        lsl         r9, r9, #8
        vdup.8      d2, r12
        orr         r9, r9, r8, lsr #24
        orr         r8, lr, r8, lsl #8
        vmov        d1, r8, r9
        sub         r1, r7, #128
        mov         r5, #7
1:
        vdup.8      d3, r6
        vmull.u8    q2, d0, d2
          subs        r12, r12, r4
        vmlal.u8    q2, d1, d3
          ittt        mi
          addmi       lr, r2, r1, asr #8
          addmi       r12, r12, #32
          vmovmi      d0, r8, r9
          rsb         r6, r12, #32
          itt         mi
          lslmi       r9, r9, #8
          ldrbmi      lr, [lr]
          vdup.8      d2, r12
        vrshrn.u16  d4, q2, #5
          itttt       mi
          orrmi       r9, r9, r8, lsr #24
          orrmi       r8, lr, r8, lsl #8
          vmovmi      d1, r8, r9
          addmi       r1, r1, r7
        subs        r5, r5, #1
        vst1.8      {d4}, [r0], r3
        bne         1b

          vdup.8      d3, r6
          vmull.u8    q2, d0, d2
          vmlal.u8    q2, d1, d3
          vrshrn.u16  d4, q2, #5
          vst1.8      {d4}, [r0]

        pop         {r4-r11, pc}

@ Right of vertical - works along top - left unused
26:
        ldrd        r8, r9, [r1]        @ Top
        rsb         r12, r6, #32
        vmov        d0, r8, r9
        vdup.8      d3, r6
        mov         r5, #7
        lsr         r8, #8
        vdup.8      d2, r12
        orr         r8, r8, r9, lsl #24
        ldr         r9, [r1, #5]!
        vmov        d1, r8, r9
1:
        vmull.u8    q2, d0, d2
          subs        r12, r4
        vmlal.u8    q2, d1, d3
          it          mi
          addmi       r12, #32
          rsb         r6, r12, #32
          itt         mi
          vmovmi      d0, r8, r9
          lsrmi       r8, #8
          vdup.8      d2, r12
          itt         mi
          orrmi       r8, r8, r9, lsl #24
          ldrmi       r9, [r1, #1]!
        vrshrn.u16  d6, q2, #5
          it          mi
          vmovmi      d1, r8, r9
          vdup.8      d3, r6
        subs        r5, #1
        vst1.8      {d6}, [r0], r3
        bne         1b

          vmull.u8    q2, d0, d2
          vmlal.u8    q2, d1, d3
          vrshrn.u16  d6, q2, #5
          vst1.8      {d6}, [r0]

        pop         {r4-r11, pc}

endfunc


@ ff_hevc_rpi_pred_angular_16_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride        [r3]
@       unsigned int mode       [sp, #0]  2..34

function ff_hevc_rpi_pred_angular_16_neon_8, export=1
        ldr         r12, [sp]
        push        {r4-r11, lr}
        ADRT        r4, angle_2 - 2
        ADRT        r7, inv_angle - 11*2
        add         r7, r7, r12, lsl #1
        ldrsb       r6, [r4, r12]
        cmp         r12, #26
        ldrsb       r4, [r4, r12]
        bge         26f
        cmp         r12, #18
        bge         18f
        cmp         r12, #10
        bge         10f

@ Down of Horizontal - works down left
        mov         r1,  r2             @ save r2 - r1 unused by patch_down

        bl          patch_h_down_8x8_8
        bl          patch_h_down_8x8_8_continue

        add         r2, r1, #8          @ restore r2, but 8 rows further down left
        sub         r0, #16
        mov         r6, r4
        add         r0, r0, r3, lsl #3

        bl          patch_h_down_8x8_8
        bl          patch_h_down_8x8_8_continue

        pop         {r4-r11, pc}

@ Up of Horizontal - works down up
10:
        ldrh        r7, [r7]
        mov         r10, #-128

        push        {r2}
        bl          patch_h_up_8x8_8
        bl          patch_h_up_8x8_8_continue
        pop         {r2}

        sub         r0, #16
        mov         r10, #-128
        add         r2, #8
        add         r0, r0, r3, lsl #3
        sub         r10, r10, r7, lsl #3

        bl          patch_h_up_8x8_8
        bl          patch_h_up_8x8_8_continue

        pop         {r4-r11, pc}

@ Left of vertical - works down left
18:
        vld1.8      {q9}, [r1]
        sub         r1, r2, #1
        rsb         r12, r6, #32
        ldrh        r7, [r7]
        vdup.8      d6, r6
        vext.8      q8, q9, q9, #15
        sub         r8, r7, #128
        vld1.8      {d16[0]}, [r1]
        vdup.8      d7, r12
        mov         r5, #15
1:
        vmull.u8    q0, d18, d7
        subs        r12, r4
        vmlal.u8    q0, d16, d6
        it          cc
        addcc       r12, #32
        vmull.u8    q1, d19, d7
        it          cc
        addcc       r1, r2, r8, asr #8
        vmlal.u8    q1, d17, d6
        rsb         r6, r12, #32
        vext.8      q10, q8, q8, #15
        sub         r5, #1
        vld1.8      {d20[0]}, [r1]
        it          cc
        addcc       r8, r7
        vmov        q11, q8
        teq         r5, #0
        vrshrn.u16  d0, q0, #5
        vrshrn.u16  d1, q1, #5
        vdup.8      d6, r6
        vdup.8      d7, r12
        vst1.8      {q0}, [r0], r3
        bhi         1b
        beq         4f
2:
        vmull.u8    q0, d22, d7
        subs        r12, r4
        vmlal.u8    q0, d20, d6
        it          cc
        addcc       r12, #32
        vmull.u8    q1, d23, d7
        it          cc
        addcc       r1, r2, r8, asr #8
        vmlal.u8    q1, d21, d6
        rsb         r6, r12, #32
        vext.8      q8, q10, q10, #15
        sub         r5, #1
        vld1.8      {d16[0]}, [r1]
        it          cc
        addcc       r8, r7
        vmov        q9, q10
        teq         r5, #0
        vrshrn.u16  d0, q0, #5
        vrshrn.u16  d1, q1, #5
        vdup.8      d6, r6
        vdup.8      d7, r12
        vst1.8      {q0}, [r0], r3
        bhi         2b
        bne         1b
        bcc         5f
3:
        vmull.u8    q0, d22, d7
        vmlal.u8    q0, d20, d6
        vmull.u8    q1, d23, d7
        vmlal.u8    q1, d21, d6
        vrshrn.u16  d0, q0, #5
        vrshrn.u16  d1, q1, #5
        vst1.8      {q0}, [r0]

        pop         {r4-r11, pc}
4:
        bcc         3b
5:
        vmull.u8    q0, d18, d7
        vmlal.u8    q0, d16, d6
        vmull.u8    q1, d19, d7
        vmlal.u8    q1, d17, d6
        vrshrn.u16  d0, q0, #5
        vrshrn.u16  d1, q1, #5
        vst1.8      {q0}, [r0]

        pop         {r4-r11, pc}

@ Right of vertical - works along top - left unused
26:
        vld1.8      {q9}, [r1]!
        rsb         r12, r6, #32
        vdup.8      d6, r6
        vdup.8      d7, r12
        vext.8      q8, q9, q9, #1
        vld1.8      {d17[7]}, [r1]!
        mov         r5, #15
1:
        vmull.u8    q0, d16, d6
        subs        r12, r4
        vmlal.u8    q0, d18, d7
        it          cc
        addcc       r12, #32
        vmull.u8    q1, d17, d6
        rsb         r6, r12, #32
        vmlal.u8    q1, d19, d7
        sub         r5, #1
        vext.8      q10, q8, q8, #1
        teq         r5, #0
        vld1.8      {d21[7]}, [r1]
        it          cc
        addcc       r1, #1
        vmov        q11, q8
        vrshrn.u16  d0, q0, #5
        vrshrn.u16  d1, q1, #5
        vdup.8      d6, r6
        vdup.8      d7, r12
        vst1.8      {q0}, [r0], r3
        bhi         1b
        beq         4f
2:
        vmull.u8    q0, d20, d6
        subs        r12, r4
        vmlal.u8    q0, d22, d7
        it          cc
        addcc       r12, #32
        vmull.u8    q1, d21, d6
        rsb         r6, r12, #32
        vmlal.u8    q1, d23, d7
        sub         r5, #1
        vext.8      q8, q10, q10, #1
        teq         r5, #0
        vld1.8      {d17[7]}, [r1]
        it          cc
        addcc       r1, #1
        vmov        q9, q10
        vrshrn.u16  d0, q0, #5
        vrshrn.u16  d1, q1, #5
        vdup.8      d6, r6
        vdup.8      d7, r12
        vst1.8      {q0}, [r0], r3
        bhi         2b
        bne         1b
        bcc         5f
3:
        vmull.u8    q0, d20, d6
        vmlal.u8    q0, d22, d7
        vmull.u8    q1, d21, d6
        vmlal.u8    q1, d23, d7
        vrshrn.u16  d0, q0, #5
        vrshrn.u16  d1, q1, #5
        vst1.8      {q0}, [r0]

        pop         {r4-r11, pc}
4:
        bcc         3b
5:
        vmull.u8    q0, d16, d6
        vmlal.u8    q0, d18, d7
        vmull.u8    q1, d17, d6
        vmlal.u8    q1, d19, d7
        vrshrn.u16  d0, q0, #5
        vrshrn.u16  d1, q1, #5
        vst1.8      {q0}, [r0]

        pop         {r4-r11, pc}

endfunc


@ ff_hevc_rpi_pred_angular_32_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride        [r3]
@       unsigned int mode       [sp, #0]  2..34

function ff_hevc_rpi_pred_angular_32_neon_8, export=1
        ldr         r12, [sp]
        push        {r4-r11, lr}
        ADRT        r4, angle_2 - 2
        ADRT        r7, inv_angle - 11*2
        add         r7, r7, r12, lsl #1
        ldrsb       r6, [r4, r12]
        cmp         r12, #26
        ldrsb       r4, [r4, r12]
        bge         26f
        cmp         r12, #18
        bge         18f
        cmp         r12, #10
        bge         10f

@ Down of Horizontal - works down left
        mov         r10, #4
        mov         r1, r2
1:
        bl          patch_h_down_8x8_8
        bl          patch_h_down_8x8_8_continue
        bl          patch_h_down_8x8_8_continue
        bl          patch_h_down_8x8_8_continue

        add         r2, r1, #8          @ restore r2, but 8 rows further down left
        add         r1, r1, #8
        mov         r6, r4
        sub         r0, #32
        subs        r10, #1
        add         r0, r0, r3, lsl #3
        bne         1b

        pop        {r4-r11, pc}

@ Up of Horizontal - works down up
10:
        ldrh        r7, [r7]
        mov         r10, #-128
        vmov.i8     d6, #1<<2
1:
        push        {r2,r10}
        bl          patch_h_up_8x8_8
        bl          patch_h_up_8x8_8_continue
        bl          patch_h_up_8x8_8_continue
        bl          patch_h_up_8x8_8_continue
        pop         {r2,r10}

        vmov        r8, s12
        sub         r0, #32
        add         r2, #8
        add         r0, r0, r3, lsl #3
        sub         r10, r10, r7, lsl #3
        vshr.u8     d6, #1
        teq         r8, #0
        bne         1b

        pop        {r4-r11, pc}

@ Left of vertical - works down left
18:
        vld1.8      {q0-q1}, [r1]
        sub         r9, r2, #1
        rsb         r12, r6, #32
        ldrh        r7, [r7]
        mov         r8, #-128
        vdup.8      d18, r6
        vdup.8      d19, r12
        mov         r5, #32
1:
        vld1.8      {d17[7]}, [r9]
        add         r8, r7
        vmov        q2, q0
        vmov        q3, q1
        add         r9, r2, r8, asr #8
        vext.8      q1, q0, q1, #15
        vext.8      q0, q8, q0, #15
2:
        vmull.u8    q10, d4, d19
        subs        r12, r4
        vmlal.u8    q10, d0, d18
        it          cc
        addcc       r12, #32
        vmull.u8    q11, d5, d19
        rsb         r6, r12, #32
        vmlal.u8    q11, d1, d18
        sub         r5, #1
        vmull.u8    q12, d6, d19
        teq         r5, #0
        vmlal.u8    q12, d2, d18
        vmull.u8    q13, d7, d19
        vmlal.u8    q13, d3, d18
        vdup.8      d18, r6
        vdup.8      d19, r12
        vrshrn.u16  d20, q10, #5
        vrshrn.u16  d21, q11, #5
        vrshrn.u16  d22, q12, #5
        vrshrn.u16  d23, q13, #5
        vst1.8      {q10-q11}, [r0], r3
        bhi         2b
        bne         1b

        pop         {r4-r11, pc}

@ Right of vertical - works along top - left unused
26:
        add         r5, r1, #32
        vld1.8      {q0-q1}, [r1]!
        rsb         r12, r6, #32
        vld1.8      {d16[0]}, [r5]
        mov         r5, #32
        vdup.8      d18, r6
        vdup.8      d19, r12
1:
        vmov        q2, q0
        add         r1, #1
        vmov        q3, q1
        vext.8      q0, q0, q1, #1
        vext.8      q1, q1, q8, #1
2:
        vmull.u8    q10, d0, d18
        subs        r12, r4
        vmlal.u8    q10, d4, d19
        it          cc
        addcc       r12, #32
        vmull.u8    q11, d1, d18
        rsb         r6, r12, #32
        vmlal.u8    q11, d5, d19
        sub         r5, #1
        vmull.u8    q12, d2, d18
        teq         r5, #0
        vmlal.u8    q12, d6, d19
        vmull.u8    q13, d3, d18
        vmlal.u8    q13, d7, d19
        vld1.8      {d16[0]}, [r1]
        vdup.8      d18, r6
        vdup.8      d19, r12
        vrshrn.u16  d20, q10, #5
        vrshrn.u16  d21, q11, #5
        vrshrn.u16  d22, q12, #5
        vrshrn.u16  d23, q13, #5
        vst1.8      {q10-q11}, [r0], r3
        bhi         2b
        bne         1b

        pop         {r4-r11, pc}

endfunc


@ Chroma 8 bit 4x4 patch fns
        .text

patch_h_down_c_4x4_8:
        ldrd        r8, r9, [r2]        @ Left
        rsb         r12, r6, #32
        vmov        d0, r8, r9
        vdup.8      d3, r6
        lsr         r8, #16
        vdup.8      d2, r12
        orr         r8, r8, r9, lsl #16
        ldr         r9, [r2, #6]!
        vmov        d1, r8, r9
        // drop through...
patch_h_down_c_4x4_8_continue:
        mov         r5, #4
1:
          subs        r12, r4
        vmull.u8    q2, d0, d2
          it          mi
          addmi       r12, #32
        vmlal.u8    q2, d1, d3
          rsb         r6, r12, #32
        vext.8      q8, q8, q9, #8
          it          mi
          lsrmi       r7, r8, #16
        vmov        d18, d19
          it          mi
          vmovmi      d0, r8, r9
          vdup.8      d2, r12
          it          mi
          orrmi       r8, r7, r9, lsl #16
        vrshrn.u16  d19, q2, #5
          itt         mi
          ldrmi       r9, [r2, #2]!
          vmovmi      d1, r8, r9
        subs        r5, #1
          vdup.8      d3, r6
        bne         1b
        // drop through...
store_tran_c_4x4_8:
        vzip.16     d16, d17
        add         r6, r0, r3
        vzip.16     d18, d19
        lsl         r3, #1
        vzip.32     q8, q9
        add         r5, r0, r3
        vst1.16     {d16}, [r0]!
        vst1.16     {d17}, [r6], r3
        vst1.16     {d18}, [r5]
        asr         r3, #1
        vst1.16     {d19}, [r6]

        bx          lr

patch_h_up_c_4x4_8:
        ldrd        r8, r9, [r2]
        rsb         r6, r4, #32
        vmov        d0, r8, r9
        vdup.8      d3, r4
        lsr         r11, r8, #16
        vdup.8      d2, r6
        ldr         r8, [r2, #-2]!
        orr         r9, r11, r9, lsl #16
        vmov        d1, r8, r9
        mov         r12, r4
        vmull.u8    q2, d0, d2
        vmlal.u8    q2, d1, d3
patch_h_up_c_4x4_8_continue:
        mov         r5, #4
1:
          add         r12, r4
          cmp         r12, #33
          it          cs
          addcs       r10, r7
          mov         r11, #0
          itt         cs
          subcs       r12, #32
          tstcs       r10, #1<<31
          rsb         r6, r12, #32
          it          eq
          asreq       r11, r10, #7
          it          cs
          vmovcs      d0, r8, r9
          it          eq
          biceq       r11, #1
          vdup.8      d2, r6
          it          cs
          lsrcs       r6, r8, #16
          vdup.8      d3, r12
        vext.8      q8, q8, q9, #8
          itt         cs
          orrcs       r9, r6, r9, lsl #16
          ldrhcs      r11, [r1, r11]
        vmov        d18, d19
          it          hi
          ldrhhi      r11, [r2, #-2]!
        vrshrn.u16  d19, q2, #5
          itt         cs
          orrcs       r8, r11, r8, lsl #16
          vmovcs      d1, r8, r9
          vmull.u8    q2, d0, d2
        subs        r5, #1
          vmlal.u8    q2, d1, d3
        bne         1b

        b           store_tran_c_4x4_8


@ ff_hevc_rpi_pred_angular_c_4_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride        [r3]
@       unsigned int mode       [sp, #0]  2..34

function ff_hevc_rpi_pred_angular_c_4_neon_8, export=1
        ldr         r12, [sp]
        push        {r4-r11, lr}
        ADRT        r4, angle_2 - 2
        ADRT        r7, inv_angle - 11*2
        add         r7, r7, r12, lsl #1
        lsl         r3, #1
        ldrsb       r6, [r4, r12]
        cmp         r12, #26
        ldrsb       r4, [r4, r12]
        bge         26f
        cmp         r12, #18
        bge         18f
        cmp         r12, #10
        bge         10f

@ Down of Horizontal - works down left
        bl          patch_h_down_c_4x4_8
        pop         {r4-r11, pc}

@ Up of Horizontal - works down up
10:
        ldrh        r7, [r7]
        mov         r10, #-128
        bl          patch_h_up_c_4x4_8
        pop         {r4-r11, pc}

@ Left of vertical - works down left
18:
        ldrd        r8, r9, [r1]        @ Top
        rsb         r12, r6, #32
        ldrh        lr, [r2, #-2]       @ Top-left
        ldrh        r7, [r7]
        vmov        d0, r8, r9
        lsl         r9, r9, #16
        vdup.8      d2, r12
        orr         r9, r9, r8, lsr #16
        orr         r8, lr, r8, lsl #16
        vmov        d1, r8, r9
        sub         r1, r7, #128
        mov         r5, #3
1:
        vdup.8      d3, r6
        vmull.u8    q2, d0, d2
          subs        r12, r12, r4
        vmlal.u8    q2, d1, d3
          itttt       mi
          addmi       lr, r2, r1, asr #7
          bicmi       lr, #1
          addmi       r12, r12, #32
          vmovmi      d0, r8, r9
          rsb         r6, r12, #32
          itt         mi
          lslmi       r9, r9, #16
          ldrhmi      lr, [lr]
          vdup.8      d2, r12
        vrshrn.u16  d4, q2, #5
          itttt       mi
          orrmi       r9, r9, r8, lsr #16
          orrmi       r8, lr, r8, lsl #16
          vmovmi      d1, r8, r9
          addmi       r1, r1, r7
        subs        r5, r5, #1
        vst1.16     {d4}, [r0], r3
        bne         1b

          vdup.8      d3, r6
          vmull.u8    q2, d0, d2
          vmlal.u8    q2, d1, d3
          vrshrn.u16  d4, q2, #5
          vst1.16     {d4}, [r0]

        pop         {r4-r11, pc}

@ Right of vertical - works along top - left unused
26:
        ldrd        r8, r9, [r1]        @ Top
        rsb         r12, r6, #32
        vmov        d0, r8, r9
        vdup.8      d3, r6
        mov         r5, #3
        lsr         r8, #16
        vdup.8      d2, r12
        orr         r8, r8, r9, lsl #16
        ldr         r9, [r1, #6]!
        vmov        d1, r8, r9
1:
        vmull.u8    q2, d0, d2
          subs        r12, r4
        vmlal.u8    q2, d1, d3
          it          mi
          addmi       r12, #32
          rsb         r6, r12, #32
          itt         mi
          vmovmi      d0, r8, r9
          lsrmi       r8, #16
          vdup.8      d2, r12
          itt         mi
          orrmi       r8, r8, r9, lsl #16
          ldrmi       r9, [r1, #2]!
        vrshrn.u16  d6, q2, #5
          it          mi
          vmovmi      d1, r8, r9
          vdup.8      d3, r6
        subs        r5, #1
        vst1.16     {d6}, [r0], r3
        bne         1b

          vmull.u8    q2, d0, d2
          vmlal.u8    q2, d1, d3
          vrshrn.u16  d6, q2, #5
          vst1.16     {d6}, [r0]

        pop         {r4-r11, pc}

endfunc


@ ff_hevc_rpi_pred_angular_c_8_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride        [r3]
@       unsigned int mode       [sp, #0]  2..34

function ff_hevc_rpi_pred_angular_c_8_neon_8, export=1
        ldr         r12, [sp]
        push        {r4-r11, lr}
        ADRT        r4, angle_2 - 2
        ADRT        r7, inv_angle - 11*2
        add         r7, r7, r12, lsl #1
        lsl         r3, #1
        ldrsb       r6, [r4, r12]
        cmp         r12, #26
        ldrsb       r4, [r4, r12]
        bge         26f
        cmp         r12, #18
        bge         18f
        cmp         r12, #10
        bge         10f

@ Down of Horizontal - works down left
        mov         r1,  r2             @ save r2 - r1 unused by patch_down

        bl          patch_h_down_c_4x4_8
        bl          patch_h_down_c_4x4_8_continue

        add         r2, r1, #4*2        @ restore r2, but 4 rows further down left
        sub         r0, #16
        mov         r6, r4
        add         r0, r0, r3, lsl #2

        bl          patch_h_down_c_4x4_8
        bl          patch_h_down_c_4x4_8_continue

        pop         {r4-r11, pc}

@ Up of Horizontal - works down up
10:
        ldrh        r7, [r7]
        mov         r10, #-128

        push        {r2}
        bl          patch_h_up_c_4x4_8
        bl          patch_h_up_c_4x4_8_continue
        pop         {r2}

        sub         r0, #16
        mov         r10, #-128
        add         r2, #8
        add         r0, r0, r3, lsl #2
        sub         r10, r10, r7, lsl #2

        bl          patch_h_up_c_4x4_8
        bl          patch_h_up_c_4x4_8_continue

        pop         {r4-r11, pc}

@ Left of vertical - works down left
18:
        vld1.8      {q9}, [r1]
        sub         r1, r2, #2
        rsb         r12, r6, #32
        ldrh        r7, [r7]
        vdup.8      d6, r6
        vext.8      q8, q9, q9, #14
        sub         r8, r7, #128
        vld1.16     {d16[0]}, [r1]
        vdup.8      d7, r12
        mov         r5, #7
1:
        subs        r12, r4
        vmull.u8    q0, d18, d7
        it          cc
        asrcc       r1, r8, #8
        vmlal.u8    q0, d16, d6
        it          cc
        addcc       r12, #32
        vmull.u8    q1, d19, d7
        it          cc
        addcc       r1, r2, r1, lsl #1
        vmlal.u8    q1, d17, d6
        rsb         r6, r12, #32
        vext.8      q10, q8, q8, #14
        sub         r5, #1
        vld1.16     {d20[0]}, [r1]
        it          cc
        addcc       r8, r7
        vmov        q11, q8
        teq         r5, #0
        vrshrn.u16  d0, q0, #5
        vrshrn.u16  d1, q1, #5
        vdup.8      d6, r6
        vdup.8      d7, r12
        vst1.8      {q0}, [r0], r3
        bhi         1b
        beq         4f
2:
        subs        r12, r4
        vmull.u8    q0, d22, d7
        it          cc
        asrcc       r1, r8, #8
        vmlal.u8    q0, d20, d6
        it          cc
        addcc       r12, #32
        vmull.u8    q1, d23, d7
        it          cc
        addcc       r1, r2, r1, lsl #1
        vmlal.u8    q1, d21, d6
        rsb         r6, r12, #32
        vext.8      q8, q10, q10, #14
        sub         r5, #1
        vld1.16     {d16[0]}, [r1]
        it          cc
        addcc       r8, r7
        vmov        q9, q10
        teq         r5, #0
        vrshrn.u16  d0, q0, #5
        vrshrn.u16  d1, q1, #5
        vdup.8      d6, r6
        vdup.8      d7, r12
        vst1.8      {q0}, [r0], r3
        bhi         2b
        bne         1b
        bcc         5f
3:
        vmull.u8    q0, d22, d7
        vmlal.u8    q0, d20, d6
        vmull.u8    q1, d23, d7
        vmlal.u8    q1, d21, d6
        vrshrn.u16  d0, q0, #5
        vrshrn.u16  d1, q1, #5
        vst1.8      {q0}, [r0]

        pop         {r4-r11, pc}
4:
        bcc         3b
5:
        vmull.u8    q0, d18, d7
        vmlal.u8    q0, d16, d6
        vmull.u8    q1, d19, d7
        vmlal.u8    q1, d17, d6
        vrshrn.u16  d0, q0, #5
        vrshrn.u16  d1, q1, #5
        vst1.8      {q0}, [r0]

        pop         {r4-r11, pc}

@ Right of vertical - works along top - left unused
26:
        vld1.8      {q9}, [r1]!
        rsb         r12, r6, #32
        vdup.8      d6, r6
        vdup.8      d7, r12
        vext.8      q8, q9, q9, #2
        vld1.16     {d17[3]}, [r1]!
        mov         r5, #7
1:
        vmull.u8    q0, d16, d6
        subs        r12, r4
        vmlal.u8    q0, d18, d7
        it          cc
        addcc       r12, #32
        vmull.u8    q1, d17, d6
        rsb         r6, r12, #32
        vmlal.u8    q1, d19, d7
        sub         r5, #1
        vext.8      q10, q8, q8, #2
        teq         r5, #0
        vld1.16     {d21[3]}, [r1]
        it          cc
        addcc       r1, #2
        vmov        q11, q8
        vrshrn.u16  d0, q0, #5
        vrshrn.u16  d1, q1, #5
        vdup.8      d6, r6
        vdup.8      d7, r12
        vst1.8      {q0}, [r0], r3
        bhi         1b
        beq         4f
2:
        vmull.u8    q0, d20, d6
        subs        r12, r4
        vmlal.u8    q0, d22, d7
        it          cc
        addcc       r12, #32
        vmull.u8    q1, d21, d6
        rsb         r6, r12, #32
        vmlal.u8    q1, d23, d7
        sub         r5, #1
        vext.8      q8, q10, q10, #2
        teq         r5, #0
        vld1.16     {d17[3]}, [r1]
        it          cc
        addcc       r1, #2
        vmov        q9, q10
        vrshrn.u16  d0, q0, #5
        vrshrn.u16  d1, q1, #5
        vdup.8      d6, r6
        vdup.8      d7, r12
        vst1.8      {q0}, [r0], r3
        bhi         2b
        bne         1b
        bcc         5f
3:
        vmull.u8    q0, d20, d6
        vmlal.u8    q0, d22, d7
        vmull.u8    q1, d21, d6
        vmlal.u8    q1, d23, d7
        vrshrn.u16  d0, q0, #5
        vrshrn.u16  d1, q1, #5
        vst1.8      {q0}, [r0]

        pop         {r4-r11, pc}
4:
        bcc         3b
5:
        vmull.u8    q0, d16, d6
        vmlal.u8    q0, d18, d7
        vmull.u8    q1, d17, d6
        vmlal.u8    q1, d19, d7
        vrshrn.u16  d0, q0, #5
        vrshrn.u16  d1, q1, #5
        vst1.8      {q0}, [r0]

        pop         {r4-r11, pc}

endfunc


@ ff_hevc_rpi_pred_angular_c_16_neon_8
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride        [r3]
@       unsigned int mode       [sp, #0]  2..34

function ff_hevc_rpi_pred_angular_c_16_neon_8, export=1
        ldr         r12, [sp]
        push        {r4-r11, lr}
        ADRT        r4, angle_2 - 2
        ADRT        r7, inv_angle - 11*2
        add         r7, r7, r12, lsl #1
        lsl         r3, #1
        ldrsb       r6, [r4, r12]
        cmp         r12, #26
        ldrsb       r4, [r4, r12]
        bge         26f
        cmp         r12, #18
        bge         18f
        cmp         r12, #10
        bge         10f

@ Down of Horizontal - works down left
        mov         r10, #4
        mov         r1, r2
1:
        bl          patch_h_down_c_4x4_8
        bl          patch_h_down_c_4x4_8_continue
        bl          patch_h_down_c_4x4_8_continue
        bl          patch_h_down_c_4x4_8_continue

        add         r2, r1, #4*2         @ restore r2, but 4 rows further down left
        add         r1, r1, #4*2
        mov         r6, r4
        sub         r0, #32
        subs        r10, #1
        add         r0, r0, r3, lsl #2
        bne         1b

        pop         {r4-r11, pc}

@ Up of Horizontal - works down up
10:
        ldrh        r7, [r7]
        mov         r10, #-128
        vmov.i8     d6, #1<<2
1:
        push        {r2, r10}
        bl          patch_h_up_c_4x4_8
        bl          patch_h_up_c_4x4_8_continue
        bl          patch_h_up_c_4x4_8_continue
        bl          patch_h_up_c_4x4_8_continue
        pop         {r2, r10}

        vmov        r8, s12
        sub         r0, #32
        add         r2, #8
        add         r0, r0, r3, lsl #2
        sub         r10, r10, r7, lsl #2
        vshr.u8     d6, #1
        teq         r8, #0
        bne         1b

        pop         {r4-r11, pc}

@ Left of vertical - works down left
18:
        vld1.8      {q0-q1}, [r1]
        sub         r9, r2, #2
        rsb         r12, r6, #32
        ldrh        r7, [r7]
        mov         r8, #-128
        vdup.8      d18, r6
        vdup.8      d19, r12
        mov         r5, #16
1:
        vld1.16     {d17[3]}, [r9]
        add         r8, r7
        vmov        q2, q0
        vmov        q3, q1
        asr         r9, r8, #8
        vext.8      q1, q0, q1, #14
        add         r9, r2, r9, lsl #1
        vext.8      q0, q8, q0, #14
2:
        vmull.u8    q10, d4, d19
        subs        r12, r4
        vmlal.u8    q10, d0, d18
        it          cc
        addcc       r12, #32
        vmull.u8    q11, d5, d19
        rsb         r6, r12, #32
        vmlal.u8    q11, d1, d18
        sub         r5, #1
        vmull.u8    q12, d6, d19
        teq         r5, #0
        vmlal.u8    q12, d2, d18
        vmull.u8    q13, d7, d19
        vmlal.u8    q13, d3, d18
        vdup.8      d18, r6
        vdup.8      d19, r12
        vrshrn.u16  d20, q10, #5
        vrshrn.u16  d21, q11, #5
        vrshrn.u16  d22, q12, #5
        vrshrn.u16  d23, q13, #5
        vst1.8      {q10-q11}, [r0], r3
        bhi         2b
        bne         1b

        pop         {r4-r11, pc}

@ Right of vertical - works along top - left unused
26:
        add         r5, r1, #32
        vld1.8      {q0-q1}, [r1]!
        rsb         r12, r6, #32
        vld1.16     {d16[0]}, [r5]
        mov         r5, #16
        vdup.8      d18, r6
        vdup.8      d19, r12
1:
        vmov        q2, q0
        add         r1, #2
        vmov        q3, q1
        vext.8      q0, q0, q1, #2
        vext.8      q1, q1, q8, #2
2:
        vmull.u8    q10, d0, d18
        subs        r12, r4
        vmlal.u8    q10, d4, d19
        it          cc
        addcc       r12, #32
        vmull.u8    q11, d1, d18
        rsb         r6, r12, #32
        vmlal.u8    q11, d5, d19
        sub         r5, #1
        vmull.u8    q12, d2, d18
        teq         r5, #0
        vmlal.u8    q12, d6, d19
        vmull.u8    q13, d3, d18
        vmlal.u8    q13, d7, d19
        vld1.16     {d16[0]}, [r1]
        vdup.8      d18, r6
        vdup.8      d19, r12
        vrshrn.u16  d20, q10, #5
        vrshrn.u16  d21, q11, #5
        vrshrn.u16  d22, q12, #5
        vrshrn.u16  d23, q13, #5
        vst1.8      {q10-q11}, [r0], r3
        bhi         2b
        bne         1b

        pop         {r4-r11, pc}

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

        .balign   2

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
        ldrd        r8, r9, [r2]        @ Left
        rsb         r12, r6, #32
        vmov        d0, r8, r9
        vdup.16     d3, r6
        lsr         r8, #16
        vdup.16     d2, r12
        orr         r8, r8, r9, lsl #16
        ldr         r9, [r2, #6]!
        vmov        d1, r8, r9
        // drop through...
patch_h_down_4x4_10_continue:
        mov         r5, #4
1:
          subs        r12, r4
        vmul.u16    d4, d0, d2
          it          mi
          addmi       r12, #32
        vmla.u16    d4, d1, d3
          rsb         r6, r12, #32
        vext.16     q8, q8, q9, #4
          it          mi
          lsrmi       r7, r8, #16
        vmov        d18, d19
          it          mi
          vmovmi      d0, r8, r9
          vdup.16     d2, r12
          it          mi
          orrmi       r8, r7, r9, lsl #16
        vrshr.u16   d19, d4, #5
          itt         mi
          ldrmi       r9, [r2, #2]!
          vmovmi      d1, r8, r9
        subs        r5, #1
          vdup.16     d3, r6
        bne         1b
        // drop through...
store_tran_4x4_10:
        vzip.16     d16, d17
        add         r6, r0, r3
        vzip.16     d18, d19
        lsl         r3, #1
        vzip.32     q8, q9
        add         r5, r0, r3
        vst1.16     {d16}, [r0]!
        vst1.16     {d17}, [r6], r3
        vst1.16     {d18}, [r5]
        asr         r3, #1
        vst1.16     {d19}, [r6]

        bx          lr

patch_h_up_4x4_10:
        ldrd        r8, r9, [r2]
        rsb         r6, r4, #32
        vmov        d0, r8, r9
        vdup.16     d3, r4
        lsr         r11, r8, #16
        vdup.16     d2, r6
        ldr         r8, [r2, #-2]!
        orr         r9, r11, r9, lsl #16
        vmov        d1, r8, r9
        mov         r12, r4
        vmul.u16    d4, d0, d2
        vmla.u16    d4, d1, d3
patch_h_up_4x4_10_continue:
        mov         r5, #4
1:
          add         r12, r4
          cmp         r12, #33
          it          cs
          addcs       r10, r7
          mov         r11, #0
          itt         cs
          subcs       r12, #32
          tstcs       r10, #1<<31
          rsb         r6, r12, #32
          it          eq
          asreq       r11, r10, #7
          it          cs
          vmovcs      d0, r8, r9
          it          eq
          biceq       r11, #1
          vdup.16     d2, r6
          it          cs
          lsrcs       r6, r8, #16
          vdup.16     d3, r12
        vext.16     q8, q8, q9, #4
          itt         cs
          orrcs       r9, r6, r9, lsl #16
          ldrhcs      r11, [r1, r11]
        vmov        d18, d19
          it          hi
          ldrhhi      r11, [r2, #-2]!
        vrshr.u16   d19, d4, #5
          itt         cs
          orrcs       r8, r11, r8, lsl #16
          vmovcs      d1, r8, r9
          vmul.u16    d4, d0, d2
        subs        r5, #1
          vmla.u16    d4, d1, d3
        bne         1b

        b           store_tran_4x4_10


@ ff_hevc_rpi_pred_angular_4_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride        [r3]
@       unsigned int mode       [sp, #0]  2..34

function ff_hevc_rpi_pred_angular_4_neon_10, export=1
        ldr         r12, [sp]
        push        {r4-r11, lr}
        ADRT        r4, angle_2 - 2
        ADRT        r7, inv_angle - 11*2
        add         r7, r7, r12, lsl #1
        lsl         r3, #1
        ldrsb       r6, [r4, r12]
        cmp         r12, #26
        ldrsb       r4, [r4, r12]
        bge         26f
        cmp         r12, #18
        bge         18f
        cmp         r12, #10
        bge         10f

@ Down of Horizontal - works down left
        bl          patch_h_down_4x4_10
        pop         {r4-r11, pc}

@ Up of Horizontal - works down up
10:
        ldrh        r7, [r7]
        mov         r10, #-128
        bl          patch_h_up_4x4_10
        pop         {r4-r11, pc}

@ Left of vertical - works down left
18:
        ldrd        r8, r9, [r1]        @ Top
        rsb         r12, r6, #32
        ldrh        lr, [r2, #-2]       @ Top-left
        ldrh        r7, [r7]
        vmov        d0, r8, r9
        lsl         r9, r9, #16
        vdup.16     d2, r12
        orr         r9, r9, r8, lsr #16
        orr         r8, lr, r8, lsl #16
        vmov        d1, r8, r9
        sub         r1, r7, #128
        mov         r5, #3
1:
        sel         lr, lr, lr          @ force pipeline 0 on Cortex-A53
        vdup.16     d3, r6
        vmul.u16    d4, d0, d2
          subs        r12, r12, r4
        vmla.u16    d4, d1, d3
          itttt       mi
          addmi       lr, r2, r1, asr #7
          bicmi       lr, #1
          addmi       r12, r12, #32
          vmovmi      d0, r8, r9
          rsb         r6, r12, #32
          itt         mi
          lslmi       r9, r9, #16
          ldrhmi      lr, [lr]
          vdup.16     d2, r12
        vrshr.u16   d4, d4, #5
          itttt       mi
          orrmi       r9, r9, r8, lsr #16
          orrmi       r8, lr, r8, lsl #16
          vmovmi      d1, r8, r9
          addmi       r1, r1, r7
        subs        r5, r5, #1
        vst1.16     {d4}, [r0], r3
        bne         1b

          vdup.16     d3, r6
          nop                           @ force next insn into pipeline 0 to enable
          vmul.u16    d4, d0, d2        @ vmla to execute back-to-back on Cortex-A53
          vmla.u16    d4, d1, d3
          vrshr.u16   d4, d4, #5
          vst1.16     {d4}, [r0]

        pop         {r4-r11, pc}

@ Right of vertical - works along top - left unused
26:
        ldrd        r8, r9, [r1]        @ Top
        rsb         r12, r6, #32
        vmov        d0, r8, r9
        vdup.16     d3, r6
        lsr         r8, #16
        vdup.16     d2, r12
        orr         r8, r8, r9, lsl #16
        ldr         r9, [r1, #6]!
        vmov        d1, r8, r9
        mov         r5, #3
1:
        vmul.u16    d4, d0, d2
          subs        r12, r4
        vmla.u16    d4, d1, d3
          it          mi
          addmi       r12, #32
          rsb         r6, r12, #32
          itt         mi
          vmovmi      d0, r8, r9
          lsrmi       r8, #16
          vdup.16     d2, r12
          itt         mi
          orrmi       r8, r8, r9, lsl #16
          ldrmi       r9, [r1, #2]!
        vrshr.u16   d4, d4, #5
          it          mi
          vmovmi      d1, r8, r9
          vdup.16     d3, r6
        subs        r5, #1
        vst1.16     {d4}, [r0], r3
        bne         1b

          vmul.u16    d4, d0, d2
          vmla.u16    d4, d1, d3
          vrshr.u16   d4, d4, #5
          vst1.16     {d4}, [r0]

        pop         {r4-r11, pc}

endfunc


@ ff_hevc_rpi_pred_angular_8_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride        [r3]
@       unsigned int mode       [sp, #0]  2..34

function ff_hevc_rpi_pred_angular_8_neon_10, export=1
        ldr         r12, [sp]
        push        {r4-r11, lr}
        ADRT        r4, angle_2 - 2
        ADRT        r7, inv_angle - 11*2
        add         r7, r7, r12, lsl #1
        lsl         r3, #1
        ldrsb       r6, [r4, r12]
        cmp         r12, #26
        ldrsb       r4, [r4, r12]
        bge         26f
        cmp         r12, #18
        bge         18f
        cmp         r12, #10
        bge         10f

@ Down of Horizontal - works down left
        mov         r1,  r2             @ save r2 - r1 unused by patch_down

        bl          patch_h_down_4x4_10
        bl          patch_h_down_4x4_10_continue

        add         r2, r1, #4*2        @ restore r2, but 4 rows further down left
        sub         r0, #16
        mov         r6, r4
        add         r0, r0, r3, lsl #2

        bl          patch_h_down_4x4_10
        bl          patch_h_down_4x4_10_continue

        pop         {r4-r11, pc}

@ Up of Horizontal - works down up
10:
        ldrh        r7, [r7]
        mov         r10, #-128

        push        {r2}
        bl          patch_h_up_4x4_10
        bl          patch_h_up_4x4_10_continue
        pop         {r2}

        sub         r0, #16
        mov         r10, #-128
        add         r2, #8
        add         r0, r0, r3, lsl #2
        sub         r10, r10, r7, lsl #2

        bl          patch_h_up_4x4_10
        bl          patch_h_up_4x4_10_continue

        pop         {r4-r11, pc}

@ Left of vertical - works down left
18:
        vld1.16     {q9}, [r1]
        sub         r1, r2, #2
        rsb         r12, r6, #32
        ldrh        r7, [r7]
        vdup.16     q2, r6
        vext.16     q8, q9, q9, #7
        sub         r8, r7, #128
        vld1.16     {d16[0]}, [r1]
        vdup.16     q3, r12
        mov         r5, #7
1:
        vmul.u16    q0, q9, q3
        subs        r12, r4
        vmla.u16    q0, q8, q2
        ittt        cc
        asrcc       r1, r8, #8
        addcc       r12, #32
        addcc       r1, r2, r1, lsl #1
        vext.16     q10, q8, q8, #7
        rsb         r6, r12, #32
        vmov        q11, q8
        sub         r5, #1
        vrshr.u16   q0, q0, #5
        it          cc
        addcc       r8, r7
        vld1.16     {d20[0]}, [r1]
        teq         r5, #0
        vdup.16     q2, r6
        vdup.16     q3, r12
        vst1.16     {q0}, [r0], r3
        bhi         1b
        beq         4f
2:
        vmul.u16    q0, q11, q3
        subs        r12, r4
        vmla.u16    q0, q10, q2
        ittt        cc
        asrcc       r1, r8, #8
        addcc       r12, #32
        addcc       r1, r2, r1, lsl #1
        vext.16     q8, q10, q10, #7
        rsb         r6, r12, #32
        vmov        q9, q10
        sub         r5, #1
        vrshr.u16   q0, q0, #5
        it          cc
        addcc       r8, r7
        vld1.16     {d16[0]}, [r1]
        teq         r5, #0
        vdup.16     q2, r6
        vdup.16     q3, r12
        vst1.16     {q0}, [r0], r3
        bhi         2b
        bne         1b
        bcc         5f
3:
        vmul.u16    q0, q11, q3
        vmla.u16    q0, q10, q2
        vrshr.u16   q0, q0, #5
        vst1.16     {q0}, [r0]

        pop         {r4-r11, pc}
4:
        bcc         3b
5:
        vmul.u16    q0, q9, q3
        vmla.u16    q0, q8, q2
        vrshr.u16   q0, q0, #5
        vst1.16     {q0}, [r0]

        pop         {r4-r11, pc}

@ Right of vertical - works along top - left unused
26:
        vld1.16     {q9}, [r1]!
        rsb         r12, r6, #32
        vdup.16     q2, r6
        vdup.16     q3, r12
        vext.16     q8, q9, q9, #1
        vld1.16     {d17[3]}, [r1]!
        mov         r5, #7
1:
        vmul.u16    q0, q8, q2
        subs        r12, r4
        vmla.u16    q0, q9, q3
        it          cc
        addcc       r12, #32
        vext.16     q10, q8, q8, #1
        rsb         r6, r12, #32
        vld1.16     {d21[3]}, [r1]
        sub         r5, #1
        vmov        q11, q8
        teq         r5, #0
        vrshr.u16   q0, q0, #5
        it          cc
        addcc       r1, #2
        vdup.16     q2, r6
        vdup.16     q3, r12
        vst1.16     {q0}, [r0], r3
        bhi         1b
        beq         4f
2:
        vmul.u16    q0, q10, q2
        subs        r12, r4
        vmla.u16    q0, q11, q3
        it          cc
        addcc       r12, #32
        vext.16     q8, q10, q10, #1
        rsb         r6, r12, #32
        vld1.16     {d17[3]}, [r1]
        sub         r5, #1
        vmov        q9, q10
        teq         r5, #0
        vrshr.u16   q0, q0, #5
        it          cc
        addcc       r1, #2
        vdup.16     q2, r6
        vdup.16     q3, r12
        vst1.16     {q0}, [r0], r3
        bhi         2b
        bne         1b
        bcc         5f
3:
        vmul.u16    q0, q10, q2
        vmla.u16    q0, q11, q3
        vrshr.u16   q0, q0, #5
        vst1.16     {q0}, [r0]

        pop         {r4-r11, pc}
4:
        bcc         3b
5:
        vmul.u16    q0, q8, q2
        vmla.u16    q0, q9, q3
        vrshr.u16   q0, q0, #5
        vst1.16     {q0}, [r0]

        pop         {r4-r11, pc}

endfunc


@ ff_hevc_rpi_pred_angular_16_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride        [r3]
@       unsigned int mode       [sp, #0]  2..34

function ff_hevc_rpi_pred_angular_16_neon_10, export=1
        ldr         r12, [sp]
        push        {r4-r11, lr}
        ADRT        r4, angle_2 - 2
        ADRT        r7, inv_angle - 11*2
        add         r7, r7, r12, lsl #1
        lsl         r3, #1
        ldrsb       r6, [r4, r12]
        cmp         r12, #26
        ldrsb       r4, [r4, r12]
        bge         26f
        cmp         r12, #18
        bge         18f
        cmp         r12, #10
        bge         10f

@ Down of Horizontal - works down left
        mov         r10, #4
        mov         r1, r2
1:
        bl          patch_h_down_4x4_10
        bl          patch_h_down_4x4_10_continue
        bl          patch_h_down_4x4_10_continue
        bl          patch_h_down_4x4_10_continue

        add         r2, r1, #4*2         @ restore r2, but 4 rows further down left
        add         r1, r1, #4*2
        mov         r6, r4
        sub         r0, #32
        subs        r10, #1
        add         r0, r0, r3, lsl #2
        bne         1b

        pop         {r4-r11, pc}

@ Up of Horizontal - works down up
10:
        ldrh        r7, [r7]
        mov         r10, #-128
        vmov.i8     d6, #1<<2
1:
        push        {r2, r10}
        bl          patch_h_up_4x4_10
        bl          patch_h_up_4x4_10_continue
        bl          patch_h_up_4x4_10_continue
        bl          patch_h_up_4x4_10_continue
        pop         {r2, r10}

        vmov        r8, s12
        sub         r0, #32
        add         r2, #8
        add         r0, r0, r3, lsl #2
        sub         r10, r10, r7, lsl #2
        vshr.u8     d6, #1
        teq         r8, #0
        bne         1b

        pop         {r4-r11, pc}

@ Left of vertical - works down left
18:
        vld1.16     {q0-q1}, [r1]
        sub         r9, r2, #2
        rsb         r12, r6, #32
        ldrh        r7, [r7]
        mov         r8, #-128
        vdup.16     q9, r6
        vdup.16     q10, r12
        mov         r5, #16
1:
        vld1.16     {d17[3]}, [r9]
        add         r8, r7
        vmov        q2, q0
        vmov        q3, q1
        asr         r9, r8, #8
        vext.16     q1, q0, q1, #7
        add         r9, r2, r9, lsl #1
        vext.16     q0, q8, q0, #7
2:
        vmul.u16    q11, q2, q10
        subs        r12, r4
        vmla.u16    q11, q0, q9
        it          cc
        addcc       r12, #32
        vmul.u16    q12, q3, q10
        rsb         r6, r12, #32
        vmla.u16    q12, q1, q9
        sub         r5, #1
        teq         r5, #0
        vdup.16     q9, r6
        vdup.16     q10, r12
        vrshr.u16   q11, q11, #5
        vrshr.u16   q12, q12, #5
        vst1.16     {q11-q12}, [r0], r3
        bhi         2b
        bne         1b

        pop         {r4-r11, pc}

@ Right of vertical - works along top - left unused
26:
        add         r5, r1, #32
        vld1.16     {q0-q1}, [r1]!
        rsb         r12, r6, #32
        vld1.16     {d16[0]}, [r5]
        mov         r5, #16
        vdup.16     q9, r6
        vdup.16     q10, r12
1:
        vmov        q2, q0
        add         r1, #2
        vmov        q3, q1
        vext.16     q0, q0, q1, #1
        vext.16     q1, q1, q8, #1
2:
        vmul.u16    q11, q0, q9
        subs        r12, r4
        vmla.u16    q11, q2, q10
        it          cc
        addcc       r12, #32
        vmul.u16    q12, q1, q9
        rsb         r6, r12, #32
        vmla.u16    q12, q3, q10
        sub         r5, #1
        vld1.16     {d16[0]}, [r1]
        teq         r5, #0
        vdup.16     q9, r6
        vdup.16     q10, r12
        vrshr.u16   q11, q11, #5
        vrshr.u16   q12, q12, #5
        vst1.16     {q11-q12}, [r0], r3
        bhi         2b
        bne         1b

        pop         {r4-r11, pc}

endfunc


@ ff_hevc_rpi_pred_angular_32_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride        [r3]
@       unsigned int mode       [sp, #0]  2..34

function ff_hevc_rpi_pred_angular_32_neon_10, export=1
        ldr         r12, [sp]
        push        {r4-r11, lr}
        ADRT        r4, angle_2 - 2
        ADRT        r7, inv_angle - 11*2
        add         r7, r7, r12, lsl #1
        lsl         r3, #1
        vpush       {d8}
        ldrsb       r6, [r4, r12]
        cmp         r12, #26
        ldrsb       r4, [r4, r12]
        bge         26f
        cmp         r12, #18
        bge         18f
        cmp         r12, #10
        bge         10f

@ Down of Horizontal - works down left
        add         sp, #8
        mov         r10, #8
        mov         r1, r2
1:
        bl          patch_h_down_4x4_10
        bl          patch_h_down_4x4_10_continue
        bl          patch_h_down_4x4_10_continue
        bl          patch_h_down_4x4_10_continue
        bl          patch_h_down_4x4_10_continue
        bl          patch_h_down_4x4_10_continue
        bl          patch_h_down_4x4_10_continue
        bl          patch_h_down_4x4_10_continue

        add         r2, r1, #4*2         @ restore r2, but 4 rows further down left
        add         r1, r1, #4*2
        mov         r6, r4
        sub         r0, #64
        subs        r10, #1
        add         r0, r0, r3, lsl #2
        bne         1b

        pop         {r4-r11, pc}

@ Up of Horizontal - works down up
10:
        add         sp, #8
        ldrh        r7, [r7]
        mov         r10, #-128
        vmov.i8     d6, #1<<6
1:
        push        {r2, r10}
        bl          patch_h_up_4x4_10
        bl          patch_h_up_4x4_10_continue
        bl          patch_h_up_4x4_10_continue
        bl          patch_h_up_4x4_10_continue
        bl          patch_h_up_4x4_10_continue
        bl          patch_h_up_4x4_10_continue
        bl          patch_h_up_4x4_10_continue
        bl          patch_h_up_4x4_10_continue
        pop         {r2, r10}

        vmov        r8, s12
        sub         r0, #64
        add         r2, #8
        add         r0, r0, r3, lsl #2
        sub         r10, r10, r7, lsl #2
        vshr.u8     d6, #1
        teq         r8, #0
        bne         1b

        pop         {r4-r11, pc}

@ Left of vertical - works down left
18:
        add         r5, r1, #32
        vld1.16     {q1-q2}, [r1]
        rsb         r12, r6, r6, lsl #16
        vld1.16     {q3-q4}, [r5]
        sub         r9, r2, #2
        rsb         r4, r12, #0
        rsb         r12, r12, #32 << 16
        ldrh        r7, [r7]
        mov         r8, #-128
        vmov        d0, d9
        vmov        s2, r12
        add         r10, r0, #32
        mov         r5, #32
1:
        vld1.16     {d1[3]}, [r9]
        add         r8, r7
        vmov        q11, q4
        vmov        q10, q3
        asr         r9, r8, #8
        vmov        q9, q2
        add         r9, r2, r9, lsl #1
        vmov        q8, q1
        vext.16     q4, q3, q4, #7
        vext.16     q3, q2, q3, #7
        vext.16     q2, q1, q2, #7
        vext.16     q1, q0, q1, #7
2:
        vmul.u16    q12, q8, d1[1]
        adds        r12, r4
        vmla.u16    q12, q1, d1[0]
        it          cc
        addcc       r12, #32 << 16
        vmul.u16    q13, q9, d1[1]
        it          cc
        subcc       r12, #32
        vmla.u16    q13, q2, d1[0]
        sub         r5, #1
        vmul.u16    q14, q10, d1[1]
        teq         r5, #0
        vmla.u16    q14, q3, d1[0]
        vmul.u16    q15, q11, d1[1]
        vmla.u16    q15, q4, d1[0]
        vmov        s2, r12
        vrshr.u16   q12, q12, #5
        vrshr.u16   q13, q13, #5
        vrshr.u16   q14, q14, #5
        vrshr.u16   q15, q15, #5
        vst1.16     {q12-q13}, [r0], r3
        vst1.16     {q14-q15}, [r10], r3
        bhi         2b
        bne         1b

        vpop        {d8}
        vmov        d9, d0
        pop         {r4-r11, pc}

@ Right of vertical - works along top - left unused
26:
        add         r5, r1, #32
        vld1.16     {q1-q2}, [r1]
        rsb         r12, r6, r6, lsl #16
        vld1.16     {q3-q4}, [r5]
        add         r1, r1, #64
        rsb         r4, r12, #0
        rsb         r12, r12, #32 << 16
        vmov        d1, d9
        vmov        s1, r12
        add         r10, r0, #32
        mov         r5, #32
1:
        vld1.16     {d0[0]}, [r1]!
        vmov        q8, q1
        vmov        q9, q2
        vmov        q10, q3
        vmov        q11, q4
        vext.16     q1, q1, q2, #1
        vext.16     q2, q2, q3, #1
        vext.16     q3, q3, q4, #1
        vext.16     q4, q4, q0, #1
2:
        vmul.u16    q12, q1, d0[2]
        adds        r12, r4
        vmla.u16    q12, q8, d0[3]
        it          cc
        addcc       r12, #32 << 16
        vmul.u16    q13, q2, d0[2]
        it          cc
        subcc       r12, #32
        vmla.u16    q13, q9, d0[3]
        sub         r5, #1
        vmul.u16    q14, q3, d0[2]
        teq         r5, #0
        vmla.u16    q14, q10, d0[3]
        vmul.u16    q15, q4, d0[2]
        vmla.u16    q15, q11, d0[3]
        vmov        s1, r12
        vrshr.u16   q12, q12, #5
        vrshr.u16   q13, q13, #5
        vrshr.u16   q14, q14, #5
        vrshr.u16   q15, q15, #5
        vst1.16     {q12-q13}, [r0], r3
        vst1.16     {q14-q15}, [r10], r3
        bhi         2b
        bne         1b

        vpop        {d8}
        vmov        d9, d1
        pop         {r4-r11, pc}

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
        vld1.16     {q12}, [r2]!
        rsb         r12, r6, #32
        vdup.16     q2, r6
        vdup.16     q3, r12
        mov         r5, #4
1:
        vmov        q13, q12
        vext.16     q12, q12, q12, #2
        vld1.32     {d25[1]}, [r2]!
patch_h_down_c_4x4_10_continue:
2:
        vmov        q8, q9
        subs        r12, r4
        vmul.u16    q0, q13, q3
        it          cc
        addcc       r12, #32
        vmla.u16    q0, q12, q2
        rsb         r6, r12, #32
        vmov        q9, q10
        sub         r5, #1
        vmov        q10, q11
        teq         r5, #0
        vdup.16     q2, r6
        vdup.16     q3, r12
        vrshr.u16   q11, q0, #5
        bhi         2b
        bne         1b

        bcs         3f
        vmov        q13, q12
        vext.16     q12, q12, q12, #2
        vld1.32     {d25[1]}, [r2]!
3:

store_tran_c_4x4_10:
T       add         r6, r0, r3
        vzip.32     q8, q10
A       add         r6, r0, r3
T       lsl         r3, #1
        vzip.32     q9, q11
A       add         r5, r0, r3, lsl #1
T       add         r5, r0, r3
        vst2.32     {d16,d18}, [r0]!
A       lsl         r3, #1
        vst2.32     {d17,d19}, [r6], r3
        asr         r3, #1
        vst2.32     {d20,d22}, [r5]
        mov         r5, #4
        vst2.32     {d21,d23}, [r6]
        bx          lr

patch_h_up_c_4x4_10:
        vld1.16     {q1}, [r2]
        rsb         r12, r6, #32
        vdup.16     q2, r6
        vdup.16     q3, r12
        mov         r5, #4
1:
        adds        r8, r7
        vmov        q12, q1
        it          mi
        ldrmi       r6, [r2, #-4]!
        vext.16     q1, q1, q1, #6
        itt         pl
        asrpl       r6, r8, #8
        ldrpl       r6, [r1, r6, lsl #2]
        vmov        s4, r6
patch_h_up_c_4x4_10_continue:
2:
        vmov        q8, q9
        subs        r12, r4
        vmul.u16    q0, q12, q3
        it          cc
        addcc       r12, #32
        vmla.u16    q0, q1, q2
        rsb         r6, r12, #32
        vmov        q9, q10
        sub         r5, #1
        vmov        q10, q11
        teq         r5, #0
        vdup.16     q2, r6
        vdup.16     q3, r12
        vrshr.u16   q11, q0, #5
        bhi         2b
        bne         1b

        bcs         store_tran_c_4x4_10
        adds        r8, r7
        vmov        q12, q1
        it          mi
        ldrmi       r6, [r2, #-4]!
        vext.16     q1, q1, q1, #6
        itt         pl
        asrpl       r6, r8, #8
        ldrpl       r6, [r1, r6, lsl #2]
        vmov        s4, r6
        b           store_tran_c_4x4_10


@ ff_hevc_rpi_pred_angular_c_4_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride        [r3]
@       unsigned int mode       [sp, #0]  2..34

function ff_hevc_rpi_pred_angular_c_4_neon_10, export=1
        ldr         r12, [sp]
        push        {r4-r8, lr}
        ADRT        r4, angle_2 - 2
        ADRT        r7, inv_angle - 11*2
        add         r7, r7, r12, lsl #1
        lsl         r3, #2
        ldrsb       r6, [r4, r12]
        cmp         r12, #26
        ldrsb       r4, [r4, r12]
        bge         26f
        cmp         r12, #18
        bge         18f
        cmp         r12, #10
        bge         10f

@ Down of Horizontal - works down left
        bl          patch_h_down_c_4x4_10
        pop         {r4-r8, pc}

@ Up of Horizontal - works down up
10:
        ldrh        r7, [r7]
        mov         r8, #-128
        sub         r8, r7
        bl          patch_h_up_c_4x4_10
        pop         {r4-r8, pc}

@ Left of vertical - works down left
18:
        vld1.16     {q9}, [r1]
        sub         r1, r2, #4
        rsb         r12, r6, #32
        ldrh        r7, [r7]
        vdup.16     q2, r6
        vext.16     q8, q9, q9, #6
        sub         r8, r7, #128
        vld1.32     {d16[0]}, [r1]
        vdup.16     q3, r12
        mov         r5, #3
1:
        vmul.u16    q0, q9, q3
        subs        r12, r4
        vmla.u16    q0, q8, q2
        ittt        cc
        asrcc       r1, r8, #8
        addcc       r12, #32
        addcc       r1, r2, r1, lsl #2
        vext.16     q10, q8, q8, #6
        rsb         r6, r12, #32
        vmov        q11, q8
        sub         r5, #1
        vrshr.u16   q0, q0, #5
        it          cc
        addcc       r8, r7
        vld1.32     {d20[0]}, [r1]
        teq         r5, #0
        vdup.16     q2, r6
        vdup.16     q3, r12
        vst1.16     {q0}, [r0], r3
        bhi         1b
        beq         4f
2:
        vmul.u16    q0, q11, q3
        subs        r12, r4
        vmla.u16    q0, q10, q2
        ittt        cc
        asrcc       r1, r8, #8
        addcc       r12, #32
        addcc       r1, r2, r1, lsl #2
        vext.16     q8, q10, q10, #6
        rsb         r6, r12, #32
        vmov        q9, q10
        sub         r5, #1
        vrshr.u16   q0, q0, #5
        it          cc
        addcc       r8, r7
        vld1.32     {d16[0]}, [r1]
        teq         r5, #0
        vdup.16     q2, r6
        vdup.16     q3, r12
        vst1.16     {q0}, [r0], r3
        bhi         2b
        bne         1b
        bcc         5f
3:
        vmul.u16    q0, q11, q3
        vmla.u16    q0, q10, q2
        vrshr.u16   q0, q0, #5
        vst1.16     {q0}, [r0]

        pop         {r4-r8, pc}
4:
        bcc         3b
5:
        vmul.u16    q0, q9, q3
        vmla.u16    q0, q8, q2
        vrshr.u16   q0, q0, #5
        vst1.16     {q0}, [r0]

        pop         {r4-r8, pc}

@ Right of vertical - works along top - left unused
26:
        vld1.16     {q9}, [r1]!
        rsb         r12, r6, #32
        vdup.16     q2, r6
        vdup.16     q3, r12
        vext.16     q8, q9, q9, #2
        vld1.32     {d17[1]}, [r1]!
        mov         r5, #3
1:
        vmul.u16    q0, q8, q2
        subs        r12, r4
        vmla.u16    q0, q9, q3
        it          cc
        addcc       r12, #32
        vext.16     q10, q8, q8, #2
        rsb         r6, r12, #32
        vld1.32     {d21[1]}, [r1]
        sub         r5, #1
        vmov        q11, q8
        teq         r5, #0
        vrshr.u16   q0, q0, #5
        it          cc
        addcc       r1, #4
        vdup.16     q2, r6
        vdup.16     q3, r12
        vst1.16     {q0}, [r0], r3
        bhi         1b
        beq         4f
2:
        vmul.u16    q0, q10, q2
        subs        r12, r4
        vmla.u16    q0, q11, q3
        it          cc
        addcc       r12, #32
        vext.16     q8, q10, q10, #2
        rsb         r6, r12, #32
        vld1.32     {d17[1]}, [r1]
        sub         r5, #1
        vmov        q9, q10
        teq         r5, #0
        vrshr.u16   q0, q0, #5
        it          cc
        addcc       r1, #4
        vdup.16     q2, r6
        vdup.16     q3, r12
        vst1.16     {q0}, [r0], r3
        bhi         2b
        bne         1b
        bcc         5f
3:
        vmul.u16    q0, q10, q2
        vmla.u16    q0, q11, q3
        vrshr.u16   q0, q0, #5
        vst1.16     {q0}, [r0]

        pop         {r4-r8, pc}
4:
        bcc         3b
5:
        vmul.u16    q0, q8, q2
        vmla.u16    q0, q9, q3
        vrshr.u16   q0, q0, #5
        vst1.16     {q0}, [r0]

        pop         {r4-r8, pc}

endfunc


@ ff_hevc_rpi_pred_angular_c_8_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride        [r3]
@       unsigned int mode       [sp, #0]  2..34

function ff_hevc_rpi_pred_angular_c_8_neon_10, export=1
        ldr         r12, [sp]
        push        {r4-r8, lr}
        ADRT        r4, angle_2 - 2
        ADRT        r7, inv_angle - 11*2
        add         r7, r7, r12, lsl #1
        lsl         r3, #2
        ldrsb       r6, [r4, r12]
        cmp         r12, #26
        ldrsb       r4, [r4, r12]
        bge         26f
        cmp         r12, #18
        bge         18f
        cmp         r12, #10
        bge         10f

@ Down of Horizontal - works down left
        mov         r1,  r2             @ save r2 - r1 unused by patch_down

        bl          patch_h_down_c_4x4_10
        bl          patch_h_down_c_4x4_10_continue

        add         r2, r1, #4*4        @ restore r2, but 4 rows further down left
        sub         r0, #32
        mov         r6, r4
        add         r0, r0, r3, lsl #2

        bl          patch_h_down_c_4x4_10
        bl          patch_h_down_c_4x4_10_continue

        pop         {r4-r8, pc}

@ Up of Horizontal - works down up
10:
        ldrh        r7, [r7]
        mov         r8, #-128
        sub         r8, r7

        push        {r2, r8}
        bl          patch_h_up_c_4x4_10
        bl          patch_h_up_c_4x4_10_continue
        pop         {r2, r8}

        sub         r0, #32
        mov         r6, r4
        add         r2, #16
        sub         r8, r8, r7, lsl #2
        add         r0, r0, r3, lsl #2

        bl          patch_h_up_c_4x4_10
        bl          patch_h_up_c_4x4_10_continue

        pop         {r4-r8, pc}

@ Left of vertical - works down left
18:
        vld1.16     {q0-q1}, [r1]
        sub         r9, r2, #4
        rsb         r12, r6, #32
        ldrh        r7, [r7]
        mov         r8, #-128
        vdup.16     q9, r6
        vdup.16     q10, r12
        mov         r5, #8
1:
        vld1.32     {d17[1]}, [r9]
        add         r8, r7
        vmov        q2, q0
        vmov        q3, q1
        asr         r9, r8, #8
        vext.16     q1, q0, q1, #6
        add         r9, r2, r9, lsl #2
        vext.16     q0, q8, q0, #6
2:
        vmul.u16    q11, q2, q10
        subs        r12, r4
        vmla.u16    q11, q0, q9
        it          cc
        addcc       r12, #32
        vmul.u16    q12, q3, q10
        rsb         r6, r12, #32
        vmla.u16    q12, q1, q9
        sub         r5, #1
        teq         r5, #0
        vdup.16     q9, r6
        vdup.16     q10, r12
        vrshr.u16   q11, q11, #5
        vrshr.u16   q12, q12, #5
        vst1.16     {q11-q12}, [r0], r3
        bhi         2b
        bne         1b

        pop         {r4-r8, pc}

@ Right of vertical - works along top - left unused
26:
        add         r5, r1, #32
        vld1.16     {q0-q1}, [r1]!
        rsb         r12, r6, #32
        vld1.32     {d16[0]}, [r5]
        mov         r5, #8
        vdup.16     q9, r6
        vdup.16     q10, r12
1:
        vmov        q2, q0
        add         r1, #4
        vmov        q3, q1
        vext.16     q0, q0, q1, #2
        vext.16     q1, q1, q8, #2
2:
        vmul.u16    q11, q0, q9
        subs        r12, r4
        vmla.u16    q11, q2, q10
        it          cc
        addcc       r12, #32
        vmul.u16    q12, q1, q9
        rsb         r6, r12, #32
        vmla.u16    q12, q3, q10
        sub         r5, #1
        vld1.32     {d16[0]}, [r1]
        teq         r5, #0
        vdup.16     q9, r6
        vdup.16     q10, r12
        vrshr.u16   q11, q11, #5
        vrshr.u16   q12, q12, #5
        vst1.16     {q11-q12}, [r0], r3
        bhi         2b
        bne         1b

        pop         {r4-r8, pc}

endfunc


@ ff_hevc_rpi_pred_angular_c_16_neon_10
@       uint8_t *_src,          [r0]
@       const uint8_t *_top,    [r1]
@       const uint8_t *_left,   [r2]
@       ptrdiff_t stride        [r3]
@       unsigned int mode       [sp, #0]  2..34

function ff_hevc_rpi_pred_angular_c_16_neon_10, export=1
        ldr         r12, [sp]
        push        {r4-r10, lr}
        ADRT        r4, angle_2 - 2
        ADRT        r7, inv_angle - 11*2
        add         r7, r7, r12, lsl #1
        lsl         r3, #2
        vpush       {d8}
        ldrsb       r6, [r4, r12]
        cmp         r12, #26
        ldrsb       r4, [r4, r12]
        bge         26f
        cmp         r12, #18
        bge         18f
        cmp         r12, #10
        bge         10f

@ Down of Horizontal - works down left
        add         sp, #8
        mov         r10, #4
        mov         r1, r2
1:
        bl          patch_h_down_c_4x4_10
        bl          patch_h_down_c_4x4_10_continue
        bl          patch_h_down_c_4x4_10_continue
        bl          patch_h_down_c_4x4_10_continue

        add         r2, r1, #4*4         @ restore r2, but 4 rows further down left
        add         r1, r1, #4*4
        mov         r6, r4
        sub         r0, #64
        subs        r10, #1
        add         r0, r0, r3, lsl #2
        bne         1b

        pop         {r4-r10, pc}

@ Up of Horizontal - works down up
10:
        add         sp, #8
        mov         r10, #4
        ldrh        r7, [r7]
        mov         r8, #-128
        sub         r8, r7
2:
        push        {r2, r8}
        bl          patch_h_up_c_4x4_10
        bl          patch_h_up_c_4x4_10_continue
        bl          patch_h_up_c_4x4_10_continue
        bl          patch_h_up_c_4x4_10_continue
        pop         {r2, r8}

        sub         r0, #64
        mov         r6, r4
        add         r2, #16
        sub         r8, r8, r7, lsl #2
        add         r0, r0, r3, lsl #2
        subs        r10, #1
        bne         2b

        pop         {r4-r10, pc}

@ Left of vertical - works down left
18:
        add         r5, r1, #32
        vld1.16     {q1-q2}, [r1]
        rsb         r12, r6, r6, lsl #16
        vld1.16     {q3-q4}, [r5]
        sub         r9, r2, #4
        rsb         r4, r12, #0
        rsb         r12, r12, #32 << 16
        ldrh        r7, [r7]
        mov         r8, #-128
        vmov        d0, d9
        vmov        s2, r12
        add         r10, r0, #32
        mov         r5, #16
1:
        vld1.32     {d1[1]}, [r9]
        add         r8, r7
        vmov        q11, q4
        vmov        q10, q3
        asr         r9, r8, #8
        vmov        q9, q2
        add         r9, r2, r9, lsl #2
        vmov        q8, q1
        vext.16     q4, q3, q4, #6
        vext.16     q3, q2, q3, #6
        vext.16     q2, q1, q2, #6
        vext.16     q1, q0, q1, #6
2:
        vmul.u16    q12, q8, d1[1]
        adds        r12, r4
        vmla.u16    q12, q1, d1[0]
        it          cc
        addcc       r12, #32 << 16
        vmul.u16    q13, q9, d1[1]
        it          cc
        subcc       r12, #32
        vmla.u16    q13, q2, d1[0]
        sub         r5, #1
        vmul.u16    q14, q10, d1[1]
        teq         r5, #0
        vmla.u16    q14, q3, d1[0]
        vmul.u16    q15, q11, d1[1]
        vmla.u16    q15, q4, d1[0]
        vmov        s2, r12
        vrshr.u16   q12, q12, #5
        vrshr.u16   q13, q13, #5
        vrshr.u16   q14, q14, #5
        vrshr.u16   q15, q15, #5
        vst1.16     {q12-q13}, [r0], r3
        vst1.16     {q14-q15}, [r10], r3
        bhi         2b
        bne         1b

        vpop        {d8}
        vmov        d9, d0
        pop         {r4-r10, pc}

@ Right of vertical - works along top - left unused
26:
        add         r5, r1, #32
        vld1.16     {q1-q2}, [r1]
        rsb         r12, r6, r6, lsl #16
        vld1.16     {q3-q4}, [r5]
        add         r1, r1, #64
        rsb         r4, r12, #0
        rsb         r12, r12, #32 << 16
        vmov        d1, d9
        vmov        s1, r12
        add         r10, r0, #32
        mov         r5, #16
1:
        vld1.32     {d0[0]}, [r1]!
        vmov        q8, q1
        vmov        q9, q2
        vmov        q10, q3
        vmov        q11, q4
        vext.16     q1, q1, q2, #2
        vext.16     q2, q2, q3, #2
        vext.16     q3, q3, q4, #2
        vext.16     q4, q4, q0, #2
2:
        vmul.u16    q12, q1, d0[2]
        adds        r12, r4
        vmla.u16    q12, q8, d0[3]
        it          cc
        addcc       r12, #32 << 16
        vmul.u16    q13, q2, d0[2]
        it          cc
        subcc       r12, #32
        vmla.u16    q13, q9, d0[3]
        sub         r5, #1
        vmul.u16    q14, q3, d0[2]
        teq         r5, #0
        vmla.u16    q14, q10, d0[3]
        vmul.u16    q15, q4, d0[2]
        vmla.u16    q15, q11, d0[3]
        vmov        s1, r12
        vrshr.u16   q12, q12, #5
        vrshr.u16   q13, q13, #5
        vrshr.u16   q14, q14, #5
        vrshr.u16   q15, q15, #5
        vst1.16     {q12-q13}, [r0], r3
        vst1.16     {q14-q15}, [r10], r3
        bhi         2b
        bne         1b

        vpop        {d8}
        vmov        d9, d1
        pop         {r4-r10, pc}

endfunc
