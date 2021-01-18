/*
Copyright (c) 2021 Michael Eiler

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

Authors: Michael Eiler <eiler.mike@gmail.com>
*/

#include "asm.S"

// void ff_rpi_sand8_lines_to_planar_y8(
//   uint8_t * dest,            : x0
//   unsigned int dst_stride,   : w1
//   const uint8_t * src,       : x2
//   unsigned int src_stride1,  : w3, always 128
//   unsigned int src_stride2,  : w4
//   unsigned int _x,           : w5
//   unsigned int y,            : w6
//   unsigned int _w,           : w7
//   unsigned int h);           : [sp, #0]

function ff_rpi_sand8_lines_to_planar_y8, export=1
    // w15 contains the number of rows we need to process
    ldr w15, [sp, #0]

    // w8 will contain the number of blocks per row
    // w8 = floor(_w/stride1)
    // stride1 is assumed to always be 128
    mov w8, w1
    lsr w8, w8, #7

    // in case the width of the image is not a multiple of 128, there will
    // be an incomplete block at the end of every row
    // w9 contains the number of pixels stored within this block
    // w9 = _w - w8 * 128
    lsl w9, w8, #7
    sub w9, w7, w9

    // this is the value we have to add to the src pointer after reading a complete block
    // it will move the address to the start of the next block
    // w10 = stride2 * stride1 - stride1 
    mov w10, w4
    lsl w10, w10, #7
    sub w10, w10, #128

    // w11 is the row offset, meaning the start offset of the first block of every collumn
    // this will be increased with stride1 within every iteration of the row_loop
    eor w11, w11, w11

    // w12 = 0, processed row count
    eor w12, w12, w12
row_loop:
    // start of the first block within the current row
    // x13 = row offset + src
    mov x13, x2
    add x13, x13, x11

    // w14 = 0, processed block count
    eor w14, w14, w14
block_loop:
    // copy 128 bytes (a full block) into the vector registers v0-v7 and increase the src address by 128
    // fortunately these aren't callee saved ones, meaning we don't need to backup them
    ld1 { v0.16b,  v1.16b,  v2.16b,  v3.16b}, [x13], #64
    ld1 { v4.16b,  v5.16b,  v6.16b,  v7.16b}, [x13], #64 

    // write these registers back to the destination vector and increase the dst address by 128
    st1 { v0.16b,  v1.16b,  v2.16b,  v3.16b }, [x0], #64
    st1 { v4.16b,  v5.16b,  v6.16b,  v7.16b }, [x0], #64

    // move the source register to the beginning of the next block (x13 = src + block offset)
    add x13, x13, x10
    // increase the block counter
    add w14, w14, #1

    // continue with the block_loop if we haven't copied all full blocks yet
    cmp w8, w14
    bgt block_loop

    // handle the last block at the end of each row
    // at most 127 byte values copied from src to dst
    eor w5, w5, w5 // i = 0
incomplete_block_loop_y8:
    cmp w5, w9
    bge incomplete_block_loop_end_y8

    ldrb w6, [x13]
    strb w6, [x0]
    add x13, x13, #1
    add x0, x0, #1

    add w5, w5, #1
    b incomplete_block_loop_y8
incomplete_block_loop_end_y8:
    
   
    // increase the row offset by 128 (stride1) 
    add w11, w11, #128
    // increment the row counter
    add w12, w12, #1

    // process the next row if we haven't finished yet
    cmp w15, w12
    bgt row_loop

    ret
endfunc



// void ff_rpi_sand8_lines_to_planar_c8(
//   uint8_t * dst_u,           : x0
//   unsigned int dst_stride_u, : w1 == width
//   uint8_t * dst_v,           : x2
//   unsigned int dst_stride_v, : w3 == width
//   const uint8_t * src,       : x4
//   unsigned int stride1,      : w5 == 128
//   unsigned int stride2,      : w6
//   unsigned int _x,           : w7
//   unsigned int y,            : [sp, #0]
//   unsigned int _w,           : [sp, #8]
//   unsigned int h);           : [sp, #16]

function ff_rpi_sand8_lines_to_planar_c8, export=1
    // w7 = width
    ldr w7, [sp, #8]

    // w15 contains the number of rows we need to process
    ldr w15, [sp, #16]

    // number of full blocks, w8 = _w / (stride1 >> 1) == _w / 64 == _w >> 6
    mov w8, w7
    lsr w8, w8, #6

    // number of pixels in block at the end of every row
    // w9 = _w - (w8 * 64)
    lsl w9, w8, #6
    sub w9, w7, w9

    // address delta to the beginning of the next block
    // w10 = (stride2 * stride1 - stride1) = stride2 * 128 - 128
    lsl w10, w6, #7
    sub w10, w10, #128

    // w11 = row address start offset = 0
    eor w11, w11, w11

    // w12 = 0, row counter
    eor w12, w12, w12 
row_loop_c8:
    // start of the first block within the current row
    // x13 = row offset + src
    mov x13, x4
    add x13, x13, x11

    // w14 = 0, processed block count
    eor w14, w14, w14
block_loop_c8:
    // load the full block -> 128 bytes, the block contains 64 interleaved U and V values 
    ld2 { v0.16b,  v1.16b }, [x13], #32
    ld2 { v2.16b,  v3.16b }, [x13], #32
    ld2 { v4.16b,  v5.16b }, [x13], #32
    ld2 { v6.16b,  v7.16b }, [x13], #32

    // swap register so that we can write them out with a single instruction
    mov v16.16b, v1.16b
    mov v17.16b, v3.16b
    mov v18.16b, v5.16b
    mov v1.16b, v2.16b
    mov v2.16b, v4.16b
    mov v3.16b, v6.16b
    mov v4.16b, v16.16b
    mov v5.16b, v17.16b
    mov v6.16b, v18.16b

    st1 { v0.16b,  v1.16b,  v2.16b,  v3.16b }, [x0], #64
    st1 { v4.16b,  v5.16b,  v6.16b,  v7.16b }, [x2], #64

    // increment row counter and move src to the beginning of the next block
    add w14, w14, #1
    add x13, x13, x10
    
    // jump to block_loop_c8 iff the block count is smaller than the number of full blocks
    cmp w8, w14
    bgt block_loop_c8

    // handle incomplete block at the end of every row
    eor w5, w5, w5 // point counter, this might be 
incomplete_block_loop_c8:
    cmp w5, w9
    bge incomplete_block_loop_end_c8

    ldrb w1, [x13]
    strb w1, [x0]
    add x13, x13, #1

    ldrb w1, [x13]
    strb w1, [x2]
    add x13, x13, #1

    add x0, x0, #1
    add x2, x2, #1

    add w5, w5, #1
    b incomplete_block_loop_c8
incomplete_block_loop_end_c8:


    // increase row_offset by stride1
    add w11, w11, #128
    add w12, w12, #1

    // jump to row_Loop_c8 iff the row count is small than the height
    cmp w15, w12
    bgt row_loop_c8

    ret
endfunc


