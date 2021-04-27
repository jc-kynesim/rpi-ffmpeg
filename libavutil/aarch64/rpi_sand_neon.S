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

// Unzip chroma
//
// On entry:
// a0 = V0, U2,  ...
// a1 = U0, V1,  ...
// a2 = U1, V2,  ...
// b0 = V8, U10, ...
// b1 = U8, V9,  ...
// b2 = U9, V10, ...
//
// On exit:
// d0 = U0, U3, ...
// ...
// a0 = V0, V3, ..
// ...
//
// Reg order for USAND is a1, a0, a2 (i.e. swap natural order of 1st 2 dest regs)

.macro UZPH_C d0, d1, d2, a0, a1, a2, b0, b1, b2
                uzp1            \d0\().8h, \a1\().8h, \b1\().8h
                uzp1            \d1\().8h, \a2\().8h, \b2\().8h
                uzp2            \d2\().8h, \a0\().8h, \b0\().8h

                uzp1            \a0\().8h, \a0\().8h, \b0\().8h
                uzp2            \a1\().8h, \a1\().8h, \b1\().8h
                uzp2            \a2\().8h, \a2\().8h, \b2\().8h
.endm

// SAND30 -> 10bit
.macro USAND10 d0, d1, d2, a0, a1
                shrn            \d2\().4h, \a0\().4s, #14
                shrn            \d1\().4h, \a0\().4s, #10

                shrn2           \d2\().8h, \a1\().4s, #14
                shrn2           \d1\().8h, \a1\().4s, #10
                uzp1            \d0\().8h, \a0\().8h, \a1\().8h

                ushr            \d2\().8h, \d2\().8h, #6
                bic             \d0\().8h, #0xfc,     lsl #8
                bic             \d1\().8h, #0xfc,     lsl #8
.endm

// SAND30 -> 8bit
.macro USAND8 d0, d1, d2, a0, a1, a2, a3, t0, t1, t2
                shrn            \d1\().4h,  \a0\().4s,  #12
                shrn2           \d1\().8h,  \a1\().4s,  #12
                uzp1            \d0\().8h,  \a0\().8h,  \a1\().8h
                uzp2            \d2\().8h,  \a0\().8h,  \a1\().8h

                shrn            \t1\().4h,  \a2\().4s,  #12
                shrn2           \t1\().8h,  \a3\().4s,  #12
                uzp1            \t0\().8h,  \a2\().8h,  \a3\().8h
                uzp2            \t2\().8h,  \a2\().8h,  \a3\().8h

                shrn            \d0\().8b,  \d0\().8h,  #2
                shrn2           \d0\().16b, \t0\().8h,  #2
                shrn            \d2\().8b,  \d2\().8h,  #6
                shrn2           \d2\().16b, \t2\().8h,  #6
                uzp1            \d1\().16b, \d1\().16b, \t1\().16b
.endm


// void ff_rpi_sand30_lines_to_planar_c16(
//   uint8_t * dst_u,            // [x0]
//   unsigned int dst_stride_u,  // [w1]
//   uint8_t * dst_v,            // [x2]
//   unsigned int dst_stride_v,  // [w3]
//   const uint8_t * src,        // [x4]
//   unsigned int stride1,       // [w5]      128
//   unsigned int stride2,       // [w6]
//   unsigned int _x,            // [w7]      0
//   unsigned int y,             // [sp, #0]
//   unsigned int _w,            // [sp, #8]  w9
//   unsigned int h);            // [sp, #16] w10

function ff_rpi_sand30_lines_to_planar_c16, export=1
                ldr             w7,  [sp, #0]                   // y
                ldr             w8,  [sp, #8]                   // _w
                ldr             w10, [sp, #16]                  // h
                lsl             w6,  w6,  #7                    // Fixup stride2
                sub             w6,  w6,  #64
                uxtw            x6,  w6
                sub             w1,  w1,  w8,  LSL #1           // Fixup chroma strides
                sub             w3,  w3,  w8,  LSL #1
                lsl             w7,  w7,  #7                    // Add y to src
                add             x4,  x4,  w7,  UXTW
10:
                mov             w13, #0
                mov             x5,  x4
                mov             w9,  w8
1:
                ld1             {v0.4s-v3.4s}, [x5], #64
                ld1             {v4.4s-v7.4s}, [x5], x6
                subs            w9,  w9,  #48

                USAND10         v17, v16, v18, v0, v1
                USAND10         v20, v19, v21, v2, v3
                UZPH_C          v0, v1, v2, v16, v17, v18, v19, v20, v21
                USAND10         v23, v22, v24, v4, v5
                USAND10         v26, v25, v27, v6, v7
                UZPH_C          v4, v5, v6, v22, v23, v24, v25, v26, v27

                blt             2f

                st3             {v0.8h-v2.8h},   [x0], #48
                st3             {v4.8h-v6.8h},   [x0], #48
                st3             {v16.8h-v18.8h}, [x2], #48
                st3             {v22.8h-v24.8h}, [x2], #48

                bne             1b
11:
                subs            w10, w10, #1
                add             x4,  x4,  #128
                add             x0,  x0,  w1,  UXTW
                add             x2,  x2,  w3,  UXTW
                bne             10b
99:
                ret

// Partial final write
2:
                cmp             w9,  #24-48
                blt             1f
                st3             {v0.8h  - v2.8h},  [x0], #48
                st3             {v16.8h - v18.8h}, [x2], #48
                beq             11b
                mov             v0.16b,  v4.16b
                mov             v1.16b,  v5.16b
                sub             w9,  w9,  #24
                mov             v2.16b,  v6.16b
                mov             v16.16b, v22.16b
                mov             v17.16b, v23.16b
                mov             v18.16b, v24.16b
1:
                cmp             w9,  #12-48
                blt             1f
                st3             {v0.4h  - v2.4h},  [x0], #24
                st3             {v16.4h - v18.4h}, [x2], #24
                beq             11b
                mov             v0.d[0],  v0.d[1]
                sub             w9,  w9,  #12
                mov             v1.d[0],  v1.d[1]
                mov             v2.d[0],  v2.d[1]
                mov             v16.d[0], v16.d[1]
                mov             v17.d[0], v17.d[1]
                mov             v18.d[0], v18.d[1]
1:
                cmp             w9,  #6-48
                blt             1f
                st3             {v0.h  - v2.h}[0],  [x0], #6
                st3             {v0.h  - v2.h}[1],  [x0], #6
                st3             {v16.h - v18.h}[0], [x2], #6
                st3             {v16.h - v18.h}[1], [x2], #6
                beq             11b
                mov             v0.s[0],  v0.s[1]
                sub             w9,  w9,  #6
                mov             v1.s[0],  v1.s[1]
                mov             v2.s[0],  v2.s[1]
                mov             v16.s[0], v16.s[1]
                mov             v17.s[0], v17.s[1]
                mov             v18.s[0], v18.s[1]
1:
                cmp             w9,  #3-48
                blt             1f
                st3             {v0.h  - v2.h}[0],  [x0], #6
                st3             {v16.h - v18.h}[0], [x2], #6
                beq             11b
                mov             v0.h[0],  v0.h[1]
                sub             w9,  w9,  #3
                mov             v1.h[0],  v1.h[1]
                mov             v16.h[0], v16.h[1]
                mov             v17.h[0], v17.h[1]
1:
                cmp             w9,  #2-48
                blt             1f
                st2             {v0.h  - v1.h}[0],  [x0], #4
                st2             {v16.h - v17.h}[0], [x2], #4
                b               11b
1:
                st1             {v0.h}[0],  [x0], #2
                st1             {v16.h}[0], [x2], #2
                b               11b
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
                uxtw            x4,  w4
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

                USAND10         v16, v17, v18, v0, v1
                USAND10         v19, v20, v21, v2, v3
                USAND10         v22, v23, v24, v4, v5
                USAND10         v25, v26, v27, v6, v7

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
                mov             v16.d[0], v16.d[1]
                sub             w5,  w5,  #12
                mov             v17.d[0], v17.d[1]
                mov             v18.d[0], v18.d[1]
1:
                cmp             w5,  #6-96
                blt             1f
                st3             {v16.h, v17.h, v18.h}[0], [x0], #6
                st3             {v16.h, v17.h, v18.h}[1], [x0], #6
                beq             11b
                mov             v16.s[0], v16.s[1]
                sub             w5,  w5,  #6
                mov             v17.s[0], v17.s[1]
                mov             v18.s[0], v18.s[1]
1:
                cmp             w5,  #3-96
                blt             1f
                st3             {v16.h, v17.h, v18.h}[0], [x0], #6
                beq             11b
                mov             v16.h[0], v16.h[1]
                sub             w5,  w5,  #3
                mov             v17.h[0], v17.h[1]
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
                uxtw            x4,  w4
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
                USAND8          v16, v17, v18, v0, v1, v2, v3, v22, v23, v24
                USAND8          v19, v20, v21, v4, v5, v6, v7, v22, v23, v24

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
                mov             v16.d[0], v16.d[1]
                sub             w5,  w5,  #24
                mov             v17.d[0], v17.d[1]
                mov             v18.d[0], v18.d[1]
1:
                cmp             w5,  #12-96
                blt             1f
                st3             {v16.b, v17.b, v18.b}[0], [x0], #3
                st3             {v16.b, v17.b, v18.b}[1], [x0], #3
                st3             {v16.b, v17.b, v18.b}[2], [x0], #3
                st3             {v16.b, v17.b, v18.b}[3], [x0], #3
                beq             11b
                mov             v16.s[0], v16.s[1]
                sub             w5,  w5,  #12
                mov             v17.s[0], v17.s[1]
                mov             v18.s[0], v18.s[1]
1:
                cmp             w5,  #6-96
                blt             1f
                st3             {v16.b, v17.b, v18.b}[0], [x0], #3
                st3             {v16.b, v17.b, v18.b}[1], [x0], #3
                beq             11b
                mov             v16.h[0], v16.h[1]
                sub             w5,  w5,  #6
                mov             v17.h[0], v17.h[1]
                mov             v18.h[0], v18.h[1]
1:
                cmp             w5,  #3-96
                blt             1f
                st3             {v16.b, v17.b, v18.b}[0], [x0], #3
                beq             11b
                mov             v16.b[0], v16.b[1]
                sub             w5,  w5,  #3
                mov             v17.b[0], v17.b[1]
1:
                cmp             w5,  #2-96
                blt             1f
                st2             {v16.b, v17.b}[0], [x0], #2
                b               11b
1:
                st1             {v16.b}[0], [x0], #1
                b               11b

endfunc

