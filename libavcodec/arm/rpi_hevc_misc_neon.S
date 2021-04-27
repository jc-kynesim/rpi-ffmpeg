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

Written by John Cox, Ben Avison
*/

#include "libavutil/arm/asm.S"
#include "neon.S"

@ rpi_zap_coeff_vals_neon(
@   uint16_t * buf,          [r0]
@   unsigned int log_n_m2)   [r1]

function rpi_zap_coeff_vals_neon, export=1
        mov      ip, #1
        vmov.i64 q0, #0
        teq      r1, #0
        vmov.i64 q1, #0
        beq      2f

        lsl      ip, r1    @ 2, 4 or 8
        add      r2, r0, #32
        lsl      ip, r1    @ 4, 16 or 64 = number of 32-byte blocks to zero
        mov      r3, #64
1:      vst1.8   {q0,q1}, [r0:256], r3
        subs     ip, #2
        vst1.8   {q0,q1}, [r2:256], r3
        bne      1b
        bx       lr

2:      vst1.8   {q0,q1}, [r0:256]
        bx       lr
endfunc

@ PIC jump tables are more expensive than absolute for A32 code
.set jent_pic, CONFIG_PIC || CONFIG_THUMB

@ Jump table entry - if in neon mode the bottom bit must be set
@ ? There is probably a real asm instruction to do this but I haven't found it
.macro jent lab
.if jent_pic
T       .short ((0 + \lab) - (0 + 98b)) / 2
A       .short (0 + \lab) - (4 + 98b)
.else
T       .word   1 + \lab
A       .word   \lab
.endif
.endm

.set expected_next, 0

.macro cpy_compound val, p1, p2, drop_thru=0
.if \p1 + \p2 != \val
.error "Bad addition!  \p1 + \p2 != \val"
.endif
.if expected_next != 0 && expected_next != \val
.error "Drop thru failure"
.endif
\val\():
        push       {r0-r3}
        bl          100\p1\()b
        pop        {r0-r3}
        add         r0, #\p1
        add         r2, #\p1
.if \drop_thru == 0
        b           \p2\()b
.set expected_next, 0
.else
.set expected_next, \p2
.endif
.endm

@ ff_hevc_cpy_blks8x4_neon(
@   dst         [r0]
@   dst_stride  [r1]
@   src         [r2]
@   src_stride  [r3]
@   width       [sp, #0] (bytes)
@   height)     [sp, #4]
@
@ Power of 2 widths are directly coded, all others are done in stripes
@ We expect the vast majority of calls to be power of 2
@
@ Currently has min width of 8, but we could make that 4 without issue
@ Min height is 4

function ff_hevc_rpi_cpy_blks8x4_neon, export=1
        ldr         r12, [sp, #0]
        push       {r11, lr}
.if jent_pic
A       adr         lr,  98f - 2
.else
A       adr         lr,  98f - 4
.endif
        lsr         r12, #3
        ldr         r11, [sp, #(8 + 4)]
.if jent_pic
A       lsl         r12, #1
A       ldrsh       lr,  [lr,  r12]
A       add         pc,  lr
T       tbh         [pc, r12, lsl #1]
.else
        @ A32 only, Thumb is always PIC
        ldr         pc,  [lr,  r12, lsl #2]
.endif

98:
T       .short      0 @ unused
        jent        8f
        jent        16f
        jent        24f
        jent        32f
        jent        40f
        jent        48f
        jent        56f
        jent        64f
        jent        72f
        jent        80f
        jent        88f
        jent        96f
        jent        104f
        jent        112f
        jent        120f
        jent        128f

1008:
        push       {r11, lr}
8:
        add         lr,  r2,  r3
        lsl         r3,  #1
        add         r12, r0,  r1
        lsl         r1,  #1
1:
        vld1.32    {d0 }, [r2],  r3
        vld1.32    {d1 }, [lr],  r3
        vld1.32    {d2 }, [r2],  r3
        vld1.32    {d3 }, [lr],  r3
        subs        r11,  #4
        vst1.32    {d0 }, [r0],  r1
        vst1.32    {d1 }, [r12], r1
        vst1.32    {d2 }, [r0],  r1
        vst1.32    {d3 }, [r12], r1
        bgt         1b
        pop        {r11, pc}

10016:
        push       {r11, lr}
16:
        add         lr,  r2,  r3
        lsl         r3,  #1
        add         r12, r0,  r1
        lsl         r1,  #1
1:
        vld1.32    {q0 }, [r2],  r3
        vld1.32    {q1 }, [lr],  r3
        vld1.32    {q2 }, [r2],  r3
        vld1.32    {q3 }, [lr],  r3
        subs        r11, #4
        vst1.32    {q0 }, [r0],  r1
        vst1.32    {q1 }, [r12], r1
        vst1.32    {q2 }, [r0],  r1
        vst1.32    {q3 }, [r12], r1
        bgt         1b
        pop        {r11, pc}

10032:
        push       {r11, lr}
32:
        add         lr,  r2,  r3
        lsl         r3,  #1
        add         r12, r0,  r1
        lsl         r1,  #1
1:
        vld1.32    {q8,  q9 }, [r2],  r3
        vld1.32    {q10, q11}, [lr],  r3
        vld1.32    {q12, q13}, [r2],  r3
        vld1.32    {q14, q15}, [lr],  r3
        subs        r11, #4
        vst1.32    {q8,  q9 }, [r0],  r1
        vst1.32    {q10, q11}, [r12], r1
        vst1.32    {q12, q13}, [r0],  r1
        vst1.32    {q14, q15}, [r12], r1
        bgt         1b
        pop        {r11, pc}

10064:
        push       {r11, lr}
64:
        add         lr,  r2,  #32
        add         r12, r0,  #32
1:
        vld1.32    {q8,  q9 }, [r2],  r3
        vld1.32    {q10, q11}, [lr],  r3
        vld1.32    {q12, q13}, [r2],  r3
        vld1.32    {q14, q15}, [lr],  r3
        subs        r11, #2
        vst1.32    {q8,  q9 }, [r0],  r1
        vst1.32    {q10, q11}, [r12], r1
        vst1.32    {q12, q13}, [r0],  r1
        vst1.32    {q14, q15}, [r12], r1
        bgt         1b
        pop        {r11, pc}

128:
        push       {r4, r5}
        @ We could do this with fewer registers if we jump around but I
        @ have a primative urge to load sequentially
        mov         r4,  #64
        add         lr,  r2,  #32
        add         r12, r0,  #32
        sub         r3,  r4
        sub         r1,  r4
1:
        vld1.32    {q8,  q9 }, [r2],  r4
        vld1.32    {q10, q11}, [lr],  r4
        vld1.32    {q12, q13}, [r2],  r3
        vld1.32    {q14, q15}, [lr],  r3
        subs        r11, #1
        vst1.32    {q8,  q9 }, [r0],  r4
        vst1.32    {q10, q11}, [r12], r4
        vst1.32    {q12, q13}, [r0],  r1
        vst1.32    {q14, q15}, [r12], r1
        bgt         1b
        pop        {r4, r5, r11, pc}

@ Use drop_thru where we can
cpy_compound 104, 64, 40, 1
cpy_compound 40, 32, 8

cpy_compound 112, 64, 48, 1
cpy_compound 48, 32, 16

cpy_compound 120, 64, 56, 1
cpy_compound 56, 32, 24, 1
cpy_compound 24, 16, 8

cpy_compound 72, 64, 8
cpy_compound 80, 64, 16
cpy_compound 88, 64, 24
cpy_compound 96, 64, 32


endfunc

