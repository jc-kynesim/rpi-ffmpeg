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

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void ff_rpi_sand8_lines_to_planar_y8(uint8_t * dest, unsigned int dst_stride,
  const uint8_t * src, unsigned int src_stride1, unsigned int src_stride2,
  unsigned int _x, unsigned int y, unsigned int _w, unsigned int h);

void ff_rpi_sand8_lines_to_planar_c8(uint8_t * dst_u, unsigned int dst_stride_u,
  uint8_t * dst_v, unsigned int dst_stride_v, const uint8_t * src,
  unsigned int stride1, unsigned int stride2, unsigned int _x, unsigned int y,
  unsigned int _w, unsigned int h);

void ff_rpi_sand30_lines_to_planar_y16(uint8_t * dest, unsigned int dst_stride,
  const uint8_t * src, unsigned int src_stride1, unsigned int src_stride2,
  unsigned int _x, unsigned int y, unsigned int _w, unsigned int h);

void ff_rpi_sand30_lines_to_planar_c16(uint8_t * dst_u, unsigned int dst_stride_u,
  uint8_t * dst_v, unsigned int dst_stride_v, const uint8_t * src, unsigned int stride1,
  unsigned int stride2, unsigned int _x, unsigned int y, unsigned int _w, unsigned int h);

void ff_rpi_sand30_lines_to_planar_y8(uint8_t * dest, unsigned int dst_stride,
  const uint8_t * src, unsigned int src_stride1, unsigned int src_stride2,
  unsigned int _x, unsigned int y, unsigned int _w, unsigned int h);

#ifdef __cplusplus
}
#endif

