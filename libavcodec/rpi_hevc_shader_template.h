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
*/

#ifndef LIBAVCODEC_RPI_SHADER_TEMPLATE_H
#define LIBAVCODEC_RPI_SHADER_TEMPLATE_H

struct HEVCRpiContext;
struct HEVCRpiInterPredEnv;

void ff_hevc_rpi_shader_c8(struct HEVCRpiContext *const s,
                  const struct HEVCRpiInterPredEnv *const ipe_y,
                  const struct HEVCRpiInterPredEnv *const ipe_c);

void ff_hevc_rpi_shader_c16(struct HEVCRpiContext *const s,
                  const struct HEVCRpiInterPredEnv *const ipe_y,
                  const struct HEVCRpiInterPredEnv *const ipe_c);

void rpi_sand_dump8(const char * const name,
                    const uint8_t * const base, const int stride1, const int stride2, int x, int y, int w, int h, const int is_c);

void rpi_sand_dump16(const char * const name,
                     const uint8_t * const base, const int stride1, const int stride2, int x, int y, int w, int h, const int is_c);

#endif

