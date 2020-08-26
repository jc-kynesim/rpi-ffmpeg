/*
Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
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

#ifndef AVUTIL_ARM_SAND_NEON_H
#define AVUTIL_ARM_SAND_NEON_H

void ff_rpi_sand128b_stripe_to_8_10(
  uint8_t * dest,             // [r0]
  const uint8_t * src1,       // [r1]
  const uint8_t * src2,       // [r2]
  unsigned int lines);        // [r3]

void ff_rpi_sand8_lines_to_planar_y8(
  uint8_t * dest,             // [r0]
  unsigned int dst_stride,    // [r1]
  const uint8_t * src,        // [r2]
  unsigned int src_stride1,   // [r3]      Ignored - assumed 128
  unsigned int src_stride2,   // [sp, #0]  -> r3
  unsigned int _x,            // [sp, #4]  Ignored - 0
  unsigned int y,             // [sp, #8]  (r7 in prefix)
  unsigned int _w,            // [sp, #12] -> r6 (cur r5)
  unsigned int h);            // [sp, #16] -> r7

void ff_rpi_sand8_lines_to_planar_c8(
  uint8_t * dst_u,            // [r0]
  unsigned int dst_stride_u,  // [r1]
  uint8_t * dst_v,            // [r2]
  unsigned int dst_stride_v,  // [r3]
  const uint8_t * src,        // [sp, #0]  -> r4, r5
  unsigned int stride1,       // [sp, #4]  128
  unsigned int stride2,       // [sp, #8]  -> r8
  unsigned int _x,            // [sp, #12] 0
  unsigned int y,             // [sp, #16] (r7 in prefix)
  unsigned int _w,            // [sp, #20] -> r12, r6
  unsigned int h);            // [sp, #24] -> r7

void ff_rpi_sand30_lines_to_planar_y16(
  uint8_t * dest,             // [r0]
  unsigned int dst_stride,    // [r1]
  const uint8_t * src,        // [r2]
  unsigned int src_stride1,   // [r3]      Ignored - assumed 128
  unsigned int src_stride2,   // [sp, #0]  -> r3
  unsigned int _x,            // [sp, #4]  Ignored - 0
  unsigned int y,             // [sp, #8]  (r7 in prefix)
  unsigned int _w,            // [sp, #12] -> r6 (cur r5)
  unsigned int h);            // [sp, #16] -> r7

void ff_rpi_sand30_lines_to_planar_c16(
  uint8_t * dst_u,            // [r0]
  unsigned int dst_stride_u,  // [r1]
  uint8_t * dst_v,            // [r2]
  unsigned int dst_stride_v,  // [r3]
  const uint8_t * src,        // [sp, #0]  -> r4, r5
  unsigned int stride1,       // [sp, #4]  128
  unsigned int stride2,       // [sp, #8]  -> r8
  unsigned int _x,            // [sp, #12] 0
  unsigned int y,             // [sp, #16] (r7 in prefix)
  unsigned int _w,            // [sp, #20] -> r6, r9
  unsigned int h);            // [sp, #24] -> r7

void ff_rpi_sand30_lines_to_planar_p010(
  uint8_t * dest,             // [r0]
  unsigned int dst_stride,    // [r1]
  const uint8_t * src,        // [r2]
  unsigned int src_stride1,   // [r3]      Ignored - assumed 128
  unsigned int src_stride2,   // [sp, #0]  -> r3
  unsigned int _x,            // [sp, #4]  Ignored - 0
  unsigned int y,             // [sp, #8]  (r7 in prefix)
  unsigned int _w,            // [sp, #12] -> r6 (cur r5)
  unsigned int h);            // [sp, #16] -> r7

#endif // AVUTIL_ARM_SAND_NEON_H

