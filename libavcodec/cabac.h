/*
 * H.26L/H.264/AVC/JVT/14496-10/... encoder/decoder
 * Copyright (c) 2003 Michael Niedermayer <michaelni@gmx.at>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 * Context Adaptive Binary Arithmetic Coder.
 */

#ifndef AVCODEC_CABAC_H
#define AVCODEC_CABAC_H

#include <stdint.h>

#include "put_bits.h"

extern const uint8_t ff_h264_cabac_tables[512 + 4*2*64 + 4*64 + 63];
#define H264_NORM_SHIFT_OFFSET 0
#define H264_LPS_RANGE_OFFSET 512
#define H264_MLPS_STATE_OFFSET 1024
#define H264_LAST_COEFF_FLAG_OFFSET_8x8_OFFSET 1280

#ifndef ALTCABAC_VER
#define ALTCABAC_VER 0
#endif

typedef struct Alt0CABACContext{
    int low;
    int range;
    union
    {
        int outstanding_count;
        struct {
            uint16_t bits;
            uint16_t range;
        } by22;
    };
    const uint8_t *bytestream_start;
    const uint8_t *bytestream;
    const uint8_t *bytestream_end;
    PutBitContext pb;
} Alt0CABACContext;

#define CABAC_BITS 16
#define CABAC_MASK ((1<<CABAC_BITS)-1)

void ff_init_cabac_encoder(Alt0CABACContext *c, uint8_t *buf, int buf_size);
int ff_init_cabac_decoder(Alt0CABACContext *c, const uint8_t *buf, int buf_size);

typedef struct Alt1CABACContext{
    uint16_t codIRange;
    uint16_t codIOffset;
    uint32_t b_offset;
    const uint8_t *bytestream_start;
    const uint8_t *bytestream_end;
} Alt1CABACContext;

extern const uint32_t alt1cabac_inv_range[256];
extern const uint16_t alt1cabac_cabac_transIdx[256];
int ff_init_alt1cabac_decoder(Alt1CABACContext *c, const uint8_t *buf, int buf_size);

#if ALTCABAC_VER == 0
#define CABACContext Alt0CABACContext

#elif ALTCABAC_VER == 1

#define CABACContext Alt1CABACContext
#define ff_init_cabac_encoder __no_such_function__
#define ff_init_cabac_decoder ff_init_alt1cabac_decoder

#else
#error Unknown CABAC alternate
#endif

#endif /* AVCODEC_CABAC_H */
