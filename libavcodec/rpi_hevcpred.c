/*
 * HEVC video Decoder
 *
 * Copyright (C) 2012 - 2013 Guillaume Martres
 * Copyright (C) 2018 John Cox for Raspberry Pi (Trading)
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

#include "rpi_hevcdec.h"

#include "rpi_hevcpred.h"
#if (ARCH_ARM)
#include "arm/rpi_hevcpred_arm.h"
#endif

#define PRED_C 0
#define BIT_DEPTH 8
#include "rpi_hevcpred_template.c"
#undef BIT_DEPTH

#define BIT_DEPTH 9
#include "rpi_hevcpred_template.c"
#undef BIT_DEPTH

#define BIT_DEPTH 10
#include "rpi_hevcpred_template.c"
#undef BIT_DEPTH

#define BIT_DEPTH 12
#include "rpi_hevcpred_template.c"
#undef BIT_DEPTH
#undef PRED_C

#define PRED_C 1
#define BIT_DEPTH 8
#include "rpi_hevcpred_template.c"
#undef BIT_DEPTH

#define BIT_DEPTH 9
#include "rpi_hevcpred_template.c"
#undef BIT_DEPTH

#define BIT_DEPTH 10
#include "rpi_hevcpred_template.c"
#undef BIT_DEPTH

#define BIT_DEPTH 12
#include "rpi_hevcpred_template.c"
#undef BIT_DEPTH
#undef PRED_C

void ff_hevc_rpi_pred_init(HEVCRpiPredContext *hpc, int bit_depth)
{
#undef FUNC
#define FUNC(a, depth) a ## _ ## depth

#undef FUNCC
#define FUNCC(a, depth) a ## _ ## depth ## _c

#define HEVC_PRED_Y(depth)                                \
    hpc->intra_pred      = FUNC(intra_pred, depth);     \
    hpc->intra_filter[0] = FUNC(intra_filter_2, depth); \
    hpc->intra_filter[1] = FUNC(intra_filter_3, depth); \
    hpc->intra_filter[2] = FUNC(intra_filter_4, depth); \
    hpc->intra_filter[3] = FUNC(intra_filter_5, depth); \
    hpc->pred_planar[0]  = FUNC(pred_planar_0, depth);  \
    hpc->pred_planar[1]  = FUNC(pred_planar_1, depth);  \
    hpc->pred_planar[2]  = FUNC(pred_planar_2, depth);  \
    hpc->pred_planar[3]  = FUNC(pred_planar_3, depth);  \
    hpc->pred_dc[0]      = FUNC(pred_dc_0, depth);      \
    hpc->pred_dc[1]      = FUNC(pred_dc_1, depth);      \
    hpc->pred_dc[2]      = FUNC(pred_dc_2, depth);      \
    hpc->pred_dc[3]      = FUNC(pred_dc_3, depth);      \
    hpc->pred_vertical[0] = FUNC(pred_angular_0, depth); \
    hpc->pred_vertical[1] = FUNC(pred_angular_1, depth); \
    hpc->pred_vertical[2] = FUNC(pred_angular_2, depth); \
    hpc->pred_vertical[3] = FUNC(pred_angular_3, depth); \
    hpc->pred_horizontal[0] = FUNC(pred_angular_0, depth); \
    hpc->pred_horizontal[1] = FUNC(pred_angular_1, depth); \
    hpc->pred_horizontal[2] = FUNC(pred_angular_2, depth); \
    hpc->pred_horizontal[3] = FUNC(pred_angular_3, depth); \
    hpc->pred_angular[0] = FUNC(pred_angular_0, depth); \
    hpc->pred_angular[1] = FUNC(pred_angular_1, depth); \
    hpc->pred_angular[2] = FUNC(pred_angular_2, depth); \
    hpc->pred_angular[3] = FUNC(pred_angular_3, depth); \
    hpc->pred_dc0[0]     = FUNC(pred_dc0_0, depth);     \
    hpc->pred_dc0[1]     = FUNC(pred_dc0_1, depth);     \
    hpc->pred_dc0[2]     = FUNC(pred_dc0_2, depth);     \
    hpc->pred_dc0[3]     = FUNC(pred_dc0_3, depth);

#define HEVC_PRED_C(depth)                                \
    hpc->intra_pred_c      = FUNCC(intra_pred, depth);     \
	hpc->intra_filter_c[0] = FUNCC(intra_filter_2, depth); \
	hpc->intra_filter_c[1] = FUNCC(intra_filter_3, depth); \
	hpc->intra_filter_c[2] = FUNCC(intra_filter_4, depth); \
	hpc->intra_filter_c[3] = FUNCC(intra_filter_5, depth); \
    hpc->pred_planar_c[0]  = FUNCC(pred_planar_0, depth);  \
    hpc->pred_planar_c[1]  = FUNCC(pred_planar_1, depth);  \
    hpc->pred_planar_c[2]  = FUNCC(pred_planar_2, depth);  \
    hpc->pred_planar_c[3]  = FUNCC(pred_planar_3, depth);  \
    hpc->pred_dc_c[0]      = FUNCC(pred_dc_0, depth);      \
    hpc->pred_dc_c[1]      = FUNCC(pred_dc_1, depth);      \
    hpc->pred_dc_c[2]      = FUNCC(pred_dc_2, depth);      \
    hpc->pred_dc_c[3]      = FUNCC(pred_dc_3, depth);      \
    hpc->pred_vertical_c[0] = FUNCC(pred_angular_0, depth); \
    hpc->pred_vertical_c[1] = FUNCC(pred_angular_1, depth); \
    hpc->pred_vertical_c[2] = FUNCC(pred_angular_2, depth); \
    hpc->pred_vertical_c[3] = FUNCC(pred_angular_3, depth); \
    hpc->pred_horizontal_c[0] = FUNCC(pred_angular_0, depth); \
    hpc->pred_horizontal_c[1] = FUNCC(pred_angular_1, depth); \
    hpc->pred_horizontal_c[2] = FUNCC(pred_angular_2, depth); \
    hpc->pred_horizontal_c[3] = FUNCC(pred_angular_3, depth); \
    hpc->pred_angular_c[0] = FUNCC(pred_angular_0, depth); \
    hpc->pred_angular_c[1] = FUNCC(pred_angular_1, depth); \
    hpc->pred_angular_c[2] = FUNCC(pred_angular_2, depth); \
    hpc->pred_angular_c[3] = FUNCC(pred_angular_3, depth); \
    hpc->pred_dc0_c[0]     = FUNCC(pred_dc0_0, depth);     \
    hpc->pred_dc0_c[1]     = FUNCC(pred_dc0_1, depth);     \
    hpc->pred_dc0_c[2]     = FUNCC(pred_dc0_2, depth);     \
    hpc->pred_dc0_c[3]     = FUNCC(pred_dc0_3, depth);

#define HEVC_PRED(depth) \
    HEVC_PRED_Y(depth); \
    HEVC_PRED_C(depth);

    switch (bit_depth) {
    case 9:
        HEVC_PRED(9);
        break;
    case 10:
        HEVC_PRED(10);
        break;
    case 12:
        HEVC_PRED(12);
        break;
    default:
        HEVC_PRED(8);
        break;
    }

#if (ARCH_ARM)
    ff_hevc_rpi_pred_init_arm(hpc, bit_depth);
#elif (ARCH_MIPS)
    ff_hevc_rpi_pred_init_mips(hpc, bit_depth);
#endif
}
