#include "libavutil/arm/asm.S"

@ void rpi_sand128b_stripe_to_8_10(
@   uint8_t * dest,             [r0]
@   const uint8_t * src1,       [r1]
@   const uint8_t * src2,       [r2]
@   unsigned int lines);        [r3]

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

function rpi_sand128b_stripe_to_8_10, export=1
        stripe2_to_8     10
endfunc

