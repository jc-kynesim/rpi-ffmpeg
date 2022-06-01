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

    cmp w8, #0
    beq no_main_y8

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
no_main_y8:
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
    // counts down
    ldr w15, [sp, #16]

    // number of full blocks, w8 = _w / (stride1 >> 1) == _w / 64 == _w >> 6
    mov w8, w7
    lsr w8, w8, #6

    // number of pixels in block at the end of every row
    // w9 = _w - (w8 * 64)
    lsl w9, w8, #6
    sub w9, w7, w9

    // Skip at the end of the line to account for stride
    sub w12, w1, w7

    // address delta to the beginning of the next block
    // w10 = (stride2 * stride1 - stride1) = stride2 * 128 - 128
    lsl w10, w6, #7
    sub w10, w10, #128

    // w11 = row address start offset = 0
    eor w11, w11, w11

row_loop_c8:
    // start of the first block within the current row
    // x13 = row offset + src
    mov x13, x4
    add x13, x13, x11

    // w14 = 0, processed block count
    eor w14, w14, w14

    cmp w8, #0
    beq no_main_c8

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

no_main_c8:
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
    add x0, x0, w12, sxtw
    add x2, x2, w12, sxtw

    // jump to row_Loop_c8 iff the row count is small than the height
    subs w15, w15, #1
    bgt row_loop_c8

    ret
endfunc

//void ff_rpi_sand30_lines_to_planar_c16(
//  uint8_t * dst_u,            // [x0]
//  unsigned int dst_stride_u,  // [w1] == _w*2
//  uint8_t * dst_v,            // [x2]
//  unsigned int dst_stride_v,  // [w3] == _w*2
//  const uint8_t * src,        // [x4]
//  unsigned int stride1,       // [w5] == 128
//  unsigned int stride2,       // [w6] 
//  unsigned int _x,            // [w7] == 0
//  unsigned int y,             // [sp, #0] == 0
//  unsigned int _w,            // [sp, #8] -> w3
//  unsigned int h);            // [sp, #16] -> w7

.macro rpi_sand30_lines_to_planar_c16_block_half
    ld1 { v0.4s,  v1.4s, v2.4s, v3.4s }, [x13], #64

    xtn v4.4h, v0.4s
    ushr v0.4s, v0.4s, #10
    xtn v5.4h, v0.4s
    ushr v0.4s, v0.4s, #10
    xtn v6.4h, v0.4s
    xtn2 v4.8h, v1.4s
    ushr v1.4s, v1.4s, #10
    xtn2 v5.8h, v1.4s
    ushr v1.4s, v1.4s, #10
    xtn2 v6.8h, v1.4s
    and v4.16b, v4.16b, v16.16b
    and v5.16b, v5.16b, v16.16b
    and v6.16b, v6.16b, v16.16b
    st3 { v4.8h, v5.8h, v6.8h }, [sp], #48
    
    xtn v4.4h, v2.4s
    ushr v2.4s, v2.4s, #10
    xtn v5.4h, v2.4s
    ushr v2.4s, v2.4s, #10
    xtn v6.4h, v2.4s
    xtn2 v4.8h, v3.4s
    ushr v3.4s, v3.4s, #10
    xtn2 v5.8h, v3.4s
    ushr v3.4s, v3.4s, #10
    xtn2 v6.8h, v3.4s
    and v4.16b, v4.16b, v16.16b
    and v5.16b, v5.16b, v16.16b
    and v6.16b, v6.16b, v16.16b
    st3 { v4.8h, v5.8h, v6.8h }, [sp]
    sub sp, sp, #48
.endm

function ff_rpi_sand30_lines_to_planar_c16, export=1
    stp x19, x20, [sp, #-48]!
    stp x21, x22, [sp, #16]
    stp x23, x24, [sp, #32]

    ldr w3, [sp, #48+8]    // w3 = width
    ldr w7, [sp, #48+16]   // w7 = height

    // reserve space on the stack for intermediate results
    sub sp, sp, #256

    // number of 128byte blocks per row, w8 = width / 48
    mov w9, #48
    udiv w8, w3, w9

    // remaining pixels (rem_pix) per row, w9 = width - w8 * 48
    mul w9, w8, w9
    sub w9, w3, w9

    // row offset, the beginning of the next row to process
    eor w10, w10, w10

    // offset to the beginning of the next block, w11 = stride2 * 128 - 128
    lsl w11, w6, #7
    sub w11, w11, #128

    // decrease the height by one and in case of remaining pixels increase the block count by one
    sub w7, w7, #1
    cmp w9, #0
    cset w19, ne    // w19 == 1 iff reamining pixels != 0
    add w8, w8, w19

    // bytes we have to move dst back by at the end of every row
    mov w21, #48*2
    mul w21, w21, w8
    sub w21, w1, w21

    mov w20, #0     // w20 = flag, last row processed

    mov x12, #0x03ff03ff03ff03ff
    dup v16.2d, x12

    // iterate through rows, row counter = w12 = 0
    eor w12, w12, w12
row_loop_c16:
    cmp w12, w7
    bge row_loop_c16_fin

    // address of row data = src + row_offset
    mov x13, x4
    add x13, x13, x10

    eor w14, w14, w14
block_loop_c16:
    cmp w14, w8
    bge block_loop_c16_fin

    rpi_sand30_lines_to_planar_c16_block_half

    ld2 { v0.8h, v1.8h }, [sp], #32
    ld2 { v2.8h, v3.8h }, [sp], #32
    ld2 { v4.8h, v5.8h }, [sp]
    sub sp, sp, #64

    st1 { v0.8h }, [x0], #16
    st1 { v2.8h }, [x0], #16
    st1 { v4.8h }, [x0], #16
    st1 { v1.8h }, [x2], #16
    st1 { v3.8h }, [x2], #16
    st1 { v5.8h }, [x2], #16

    rpi_sand30_lines_to_planar_c16_block_half

    ld2 { v0.8h, v1.8h }, [sp], #32
    ld2 { v2.8h, v3.8h }, [sp], #32
    ld2 { v4.8h, v5.8h }, [sp]
    sub sp, sp, #64

    st1 { v0.8h }, [x0], #16
    st1 { v2.8h }, [x0], #16
    st1 { v4.8h }, [x0], #16
    st1 { v1.8h }, [x2], #16
    st1 { v3.8h }, [x2], #16
    st1 { v5.8h }, [x2], #16

    add x13, x13, x11 // offset to next block
    add w14, w14, #1
    b block_loop_c16
block_loop_c16_fin:

    add w10, w10, #128
    add w12, w12, #1
    add x0, x0, w21, sxtw  // move dst pointers back by x21
    add x2, x2, w21, sxtw
    b row_loop_c16
row_loop_c16_fin:

    cmp w20, #1
    beq row_loop_c16_fin2
    mov w20, #1
    sub w8, w8, w19 // decrease block count by w19
    add w7, w7, #1 // increase height
    b row_loop_c16

row_loop_c16_fin2:
    sub x0, x0, w21, sxtw // readd x21 in case of the last row
    sub x2, x2, w21, sxtw // so that we can write out the few remaining pixels

    // last incomplete block to be finished
    // read operations are fine, stride2 is more than large enough even if rem_pix is 0
    rpi_sand30_lines_to_planar_c16_block_half
    ld2 { v0.8h, v1.8h }, [sp], #32
    ld2 { v2.8h, v3.8h }, [sp], #32
    ld2 { v4.8h, v5.8h }, [sp], #32
    rpi_sand30_lines_to_planar_c16_block_half
    ld2 { v0.8h, v1.8h }, [sp], #32
    ld2 { v2.8h, v3.8h }, [sp], #32
    ld2 { v4.8h, v5.8h }, [sp]
    sub sp, sp, #160

    mov x4, sp
    eor w20, w20, w20
rem_pix_c16_loop:
    cmp w20, w9
    bge rem_pix_c16_fin

    ldr w22, [x4], #4
    str w22, [x0], #2
    lsr w22, w22, #16
    str w22, [x2], #2 

    add w20, w20, #1
    b rem_pix_c16_loop
rem_pix_c16_fin:

    add sp, sp, #256

    ldp x23, x24, [sp, #32]
    ldp x21, x22, [sp, #16]
    ldp x19, x20, [sp], #48
    ret
endfunc



//void ff_rpi_sand30_lines_to_planar_p010(
//  uint8_t * dest,
//  unsigned int dst_stride,
//  const uint8_t * src,
//  unsigned int src_stride1,
//  unsigned int src_stride2,
//  unsigned int _x,
//  unsigned int y,
//  unsigned int _w,
//  unsigned int h);

// void ff_rpi_sand30_lines_to_planar_y8(
//   uint8_t * dest,            : x0
//   unsigned int dst_stride,   : w1
//   const uint8_t * src,       : x2
//   unsigned int src_stride1,  : w3, always 128
//   unsigned int src_stride2,  : w4
//   unsigned int _x,           : w5
//   unsigned int y,            : w6
//   unsigned int _w,           : w7
//   unsigned int h);           : [sp, #0]
//
// Assumes that we are starting on a stripe boundary and that overreading
// within the stripe is OK. However it does respect the dest size for wri

function ff_rpi_sand30_lines_to_planar_y16, export=1
                lsl             w4,  w4,  #7
                sub             w4,  w4,  #64
                sub             w1,  w1,  w7, lsl #1
                uxtw            x6,  w6
                add             x8,  x2,  x6, lsl #7
                ldr             w6,  [sp, #0]

10:
                mov             x2,  x8
                mov             w5,  w7
1:
                ld1             {v0.4s, v1.4s, v2.4s, v3.4s}, [x2], #64
                ld1             {v4.4s, v5.4s, v6.4s, v7.4s}, [x2], x4

                subs            w5,  w5,  #96

                // v0, v1

                shrn            v18.4h,  v0.4s,   #14
                xtn             v16.4h,  v0.4s
                shrn            v17.4h,  v0.4s,   #10

                shrn2           v18.8h,  v1.4s,   #14
                xtn2            v16.8h,  v1.4s
                shrn2           v17.8h,  v1.4s,   #10

                ushr            v18.8h,  v18.8h,  #6
                bic             v16.8h,  #0xfc,   lsl #8
                bic             v17.8h,  #0xfc,   lsl #8

                // v2, v3

                shrn            v21.4h,  v2.4s,   #14
                xtn             v19.4h,  v2.4s
                shrn            v20.4h,  v2.4s,   #10

                shrn2           v21.8h,  v3.4s,   #14
                xtn2            v19.8h,  v3.4s
                shrn2           v20.8h,  v3.4s,   #10

                ushr            v21.8h,  v21.8h,  #6
                bic             v19.8h,  #0xfc,   lsl #8
                bic             v20.8h,  #0xfc,   lsl #8

                // v4, v5

                shrn            v24.4h,  v4.4s,   #14
                xtn             v22.4h,  v4.4s
                shrn            v23.4h,  v4.4s,   #10

                shrn2           v24.8h,  v5.4s,   #14
                xtn2            v22.8h,  v5.4s
                shrn2           v23.8h,  v5.4s,   #10

                ushr            v24.8h,  v24.8h,  #6
                bic             v22.8h,  #0xfc,   lsl #8
                bic             v23.8h,  #0xfc,   lsl #8

                // v6, v7

                shrn            v27.4h,  v6.4s,   #14
                xtn             v25.4h,  v6.4s
                shrn            v26.4h,  v6.4s,   #10

                shrn2           v27.8h,  v7.4s,   #14
                xtn2            v25.8h,  v7.4s
                shrn2           v26.8h,  v7.4s,   #10

                ushr            v27.8h,  v27.8h,  #6
                bic             v25.8h,  #0xfc,   lsl #8
                bic             v26.8h,  #0xfc,   lsl #8

                blt             2f

                st3             {v16.8h, v17.8h, v18.8h}, [x0], #48
                st3             {v19.8h, v20.8h, v21.8h}, [x0], #48
                st3             {v22.8h, v23.8h, v24.8h}, [x0], #48
                st3             {v25.8h, v26.8h, v27.8h}, [x0], #48

                bne             1b

11:
                subs            w6,  w6,  #1
                add             x0,  x0,  w1,  uxtw
                add             x8,  x8,  #128
                bne             10b

                ret

// Partial final write
2:
                cmp             w5,  #48-96
                blt             1f
                st3             {v16.8h, v17.8h, v18.8h}, [x0], #48
                st3             {v19.8h, v20.8h, v21.8h}, [x0], #48
                beq             11b
                mov             v16.16b, v22.16b
                mov             v17.16b, v23.16b
                sub             w5,  w5,  #48
                mov             v18.16b, v24.16b
                mov             v19.16b, v25.16b
                mov             v20.16b, v26.16b
                mov             v21.16b, v27.16b
1:
                cmp             w5,  #24-96
                blt             1f
                st3             {v16.8h, v17.8h, v18.8h}, [x0], #48
                beq             11b
                mov             v16.16b, v19.16b
                mov             v17.16b, v20.16b
                sub             w5,  w5,  #24
                mov             v18.16b, v21.16b
1:
                cmp             w5,  #12-96
                blt             1f
                st3             {v16.4h, v17.4h, v18.4h}, [x0], #24
                beq             11b
                mov             v16.2d[0], v16.2d[1]
                sub             w5,  w5,  #12
                mov             v17.2d[0], v17.2d[1]
                mov             v18.2d[0], v18.2d[1]
1:
                cmp             w5,  #6-96
                blt             1f
                st3             {v16.h, v17.h, v18.h}[0], [x0], #6
                st3             {v16.h, v17.h, v18.h}[1], [x0], #6
                beq             11b
                mov             v16.2s[0], v16.2s[1]
                sub             w5,  w5,  #6
                mov             v17.2s[0], v17.2s[1]
                mov             v18.2s[0], v18.2s[1]
1:
                cmp             w5,  #3-96
                blt             1f
                st3             {v16.h, v17.h, v18.h}[0], [x0], #6
                beq             11b
                mov             v16.4h[0], v16.4h[1]
                sub             w5,  w5,  #3
                mov             v17.4h[0], v17.4h[1]
1:
                cmp             w5,  #2-96
                blt             1f
                st2             {v16.h, v17.h}[0], [x0], #4
                b               11b
1:
                st1             {v16.h}[0], [x0], #2
                b               11b

endfunc

// void ff_rpi_sand30_lines_to_planar_y8(
//   uint8_t * dest,            : x0
//   unsigned int dst_stride,   : w1
//   const uint8_t * src,       : x2
//   unsigned int src_stride1,  : w3, always 128
//   unsigned int src_stride2,  : w4
//   unsigned int _x,           : w5
//   unsigned int y,            : w6
//   unsigned int _w,           : w7
//   unsigned int h);           : [sp, #0]
//
// Assumes that we are starting on a stripe boundary and that overreading
// within the stripe is OK. However it does respect the dest size for wri

function ff_rpi_sand30_lines_to_planar_y8, export=1
                lsl             w4,  w4,  #7
                sub             w4,  w4,  #64
                sub             w1,  w1,  w7
                uxtw            x6,  w6
                add             x8,  x2,  x6, lsl #7
                ldr             w6,  [sp, #0]

10:
                mov             x2,  x8
                mov             w5,  w7
1:
                ld1             {v0.4s, v1.4s, v2.4s, v3.4s}, [x2], #64
                ld1             {v4.4s, v5.4s, v6.4s, v7.4s}, [x2], x4

                subs            w5,  w5,  #96

                // v0, v1

                shrn            v18.4h,  v0.4s,   #16
                xtn             v16.4h,  v0.4s
                shrn            v17.4h,  v0.4s,   #12

                shrn2           v18.8h,  v1.4s,   #16
                xtn2            v16.8h,  v1.4s
                shrn2           v17.8h,  v1.4s,   #12

                shrn            v18.8b,  v18.8h,  #6
                shrn            v16.8b,  v16.8h,  #2
                xtn             v17.8b,  v17.8h

                // v2, v3

                shrn            v21.4h,  v2.4s,   #16
                xtn             v19.4h,  v2.4s
                shrn            v20.4h,  v2.4s,   #12

                shrn2           v21.8h,  v3.4s,   #16
                xtn2            v19.8h,  v3.4s
                shrn2           v20.8h,  v3.4s,   #12

                shrn2           v18.16b, v21.8h,  #6
                shrn2           v16.16b, v19.8h,  #2
                xtn2            v17.16b, v20.8h

                // v4, v5

                shrn            v24.4h,  v4.4s,   #16
                xtn             v22.4h,  v4.4s
                shrn            v23.4h,  v4.4s,   #12

                shrn2           v24.8h,  v5.4s,   #16
                xtn2            v22.8h,  v5.4s
                shrn2           v23.8h,  v5.4s,   #12

                shrn            v21.8b,  v24.8h,  #6
                shrn            v19.8b,  v22.8h,  #2
                xtn             v20.8b,  v23.8h

                // v6, v7

                shrn            v27.4h,  v6.4s,   #16
                xtn             v25.4h,  v6.4s
                shrn            v26.4h,  v6.4s,   #12

                shrn2           v27.8h,  v7.4s,   #16
                xtn2            v25.8h,  v7.4s
                shrn2           v26.8h,  v7.4s,   #12

                shrn2           v21.16b, v27.8h,  #6
                shrn2           v19.16b, v25.8h,  #2
                xtn2            v20.16b, v26.8h

                blt             2f

                st3             {v16.16b, v17.16b, v18.16b}, [x0], #48
                st3             {v19.16b, v20.16b, v21.16b}, [x0], #48

                bne             1b

11:
                subs            w6,  w6,  #1
                add             x0,  x0,  w1,  uxtw
                add             x8,  x8,  #128
                bne             10b

                ret

// Partial final write
2:
                cmp             w5,  #48-96
                blt             1f
                st3             {v16.16b, v17.16b, v18.16b}, [x0], #48
                beq             11b
                mov             v16.16b, v22.16b
                mov             v17.16b, v23.16b
                sub             w5,  w5,  #48
                mov             v18.16b, v24.16b
1:
                cmp             w5,  #24-96
                blt             1f
                st3             {v16.8b, v17.8b, v18.8b}, [x0], #24
                beq             11b
                mov             v16.2d[0], v16.2d[1]
                sub             w5,  w5,  #24
                mov             v17.2d[0], v17.2d[1]
                mov             v18.2d[0], v18.2d[1]
1:
                cmp             w5,  #12-96
                blt             1f
                st3             {v16.b, v17.b, v18.b}[0], [x0], #3
                st3             {v16.b, v17.b, v18.b}[1], [x0], #3
                st3             {v16.b, v17.b, v18.b}[2], [x0], #3
                st3             {v16.b, v17.b, v18.b}[3], [x0], #3
                beq             11b
                mov             v16.2s[0], v16.2s[1]
                sub             w5,  w5,  #12
                mov             v17.2s[0], v17.2s[1]
                mov             v18.2s[0], v18.2s[1]
1:
                cmp             w5,  #6-96
                blt             1f
                st3             {v16.b, v17.b, v18.b}[0], [x0], #3
                st3             {v16.b, v17.b, v18.b}[1], [x0], #3
                beq             11b
                mov             v16.4h[0], v16.4h[1]
                sub             w5,  w5,  #6
                mov             v17.4h[0], v17.4h[1]
                mov             v18.4h[0], v18.4h[1]
1:
                cmp             w5,  #3-96
                blt             1f
                st3             {v16.b, v17.b, v18.b}[0], [x0], #3
                beq             11b
                mov             v16.8b[0], v16.8b[1]
                sub             w5,  w5,  #3
                mov             v17.8b[0], v17.8b[1]
1:
                cmp             w5,  #2-96
                blt             1f
                st2             {v16.b, v17.b}[0], [x0], #2
                b               11b
1:
                st1             {v16.b}[0], [x0], #1
                b               11b

endfunc

