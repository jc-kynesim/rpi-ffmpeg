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

