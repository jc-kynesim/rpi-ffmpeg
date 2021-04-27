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

#include "hevc.h"
#include "rpi_hevcdec.h"
#include "libavutil/rpi_sand_fns.h"
#include "rpi_hevc_shader_cmd.h"
#include "rpi_hevc_shader_template.h"

typedef struct shader_track_s
{
    const union qpu_mc_pred_cmd_u *qpu_mc_curr;
    const struct qpu_mc_src_s *last_l0;
    const struct qpu_mc_src_s *last_l1;
    uint32_t width;  // pic_width * PW
    uint32_t height;
    uint32_t stride2;
    uint32_t stride1;
} shader_track_t;

static int wtoidx(const unsigned int w)
{
    static const uint8_t pel_weight[65] = { [2] = 0, [4] = 1, [6] = 2, [8] = 3, [12] = 4, [16] = 5, [24] = 6, [32] = 7, [48] = 8, [64] = 9 };
    return pel_weight[w];
}

static const int fctom(uint32_t x)
{
    int rv;
    // As it happens we can take the 2nd filter term & divide it by 8
    // (dropping fractions) to get the fractional move
    rv = 8 - ((x >> 11) & 0xf);
    av_assert2(rv >= 0 && rv <= 7);
    return rv;
}

static inline int32_t ext(int32_t x, unsigned int shl, unsigned int shr)
{
    return (x << shl) >> shr;
}

static inline int woff_p(HEVCRpiContext *const s, int32_t x)
{
    return ext(x, 0, 17 + s->ps.sps->bit_depth - 8);
}

static inline int woff_b(HEVCRpiContext *const s, int32_t x)
{
    return ext(x - 0x10000, 0, 16 + s->ps.sps->bit_depth - 8);
}

static inline int wweight(int32_t x)
{
    return ext(x, 16, 16);
}


#define PW 1
#include "rpi_hevc_shader_template_fn.h"

#undef PW
#define PW 2
#include "rpi_hevc_shader_template_fn.h"

