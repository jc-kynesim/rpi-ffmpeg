/*
 * Copyright (c) 2018 John Cox (for Raspberry Pi)
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

#include "rpi_hevcpred_arm.h"

intra_filter_fn_t ff_hevc_rpi_intra_filter_4_neon_8;
intra_filter_fn_t ff_hevc_rpi_intra_filter_8_neon_8;
intra_filter_fn_t ff_hevc_rpi_intra_filter_4_neon_16;
intra_filter_fn_t ff_hevc_rpi_intra_filter_8_neon_16;
intra_filter_fn_t ff_hevc_rpi_intra_filter_16_neon_16;
intra_filter_fn_t ff_hevc_rpi_intra_filter_4_neon_32;
intra_filter_fn_t ff_hevc_rpi_intra_filter_8_neon_32;
intra_filter_fn_t ff_hevc_rpi_intra_filter_16_neon_32;

void ff_hevc_rpi_pred_angular_4_neon_8(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride, int mode);
void ff_hevc_rpi_pred_angular_8_neon_8(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride, int mode);
void ff_hevc_rpi_pred_angular_16_neon_8(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride, int mode);
void ff_hevc_rpi_pred_angular_32_neon_8(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride, int mode);
void ff_hevc_rpi_pred_angular_c_4_neon_8(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride, int mode);
void ff_hevc_rpi_pred_angular_c_8_neon_8(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride, int mode);
void ff_hevc_rpi_pred_angular_c_16_neon_8(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride, int mode);
void ff_hevc_rpi_pred_angular_4_neon_10(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride, int mode);
void ff_hevc_rpi_pred_angular_8_neon_10(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride, int mode);
void ff_hevc_rpi_pred_angular_16_neon_10(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride, int mode);
void ff_hevc_rpi_pred_angular_32_neon_10(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride, int mode);
void ff_hevc_rpi_pred_angular_c_4_neon_10(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride, int mode);
void ff_hevc_rpi_pred_angular_c_8_neon_10(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride, int mode);
void ff_hevc_rpi_pred_angular_c_16_neon_10(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride, int mode);

void ff_hevc_rpi_pred_vertical_4_neon_8(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride, int mode);
void ff_hevc_rpi_pred_vertical_8_neon_8(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride, int mode);
void ff_hevc_rpi_pred_vertical_16_neon_8(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride, int mode);
void ff_hevc_rpi_pred_vertical_32_neon_8(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride, int mode);
void ff_hevc_rpi_pred_vertical_c_4_neon_8(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride, int mode);
void ff_hevc_rpi_pred_vertical_c_8_neon_8(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride, int mode);
void ff_hevc_rpi_pred_vertical_c_16_neon_8(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride, int mode);
void ff_hevc_rpi_pred_vertical_4_neon_10(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride, int mode);
void ff_hevc_rpi_pred_vertical_8_neon_10(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride, int mode);
void ff_hevc_rpi_pred_vertical_16_neon_10(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride, int mode);
void ff_hevc_rpi_pred_vertical_32_neon_10(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride, int mode);
void ff_hevc_rpi_pred_vertical_c_4_neon_10(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride, int mode);
void ff_hevc_rpi_pred_vertical_c_8_neon_10(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride, int mode);
void ff_hevc_rpi_pred_vertical_c_16_neon_10(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride, int mode);

void ff_hevc_rpi_pred_horizontal_4_neon_8(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride, int mode);
void ff_hevc_rpi_pred_horizontal_8_neon_8(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride, int mode);
void ff_hevc_rpi_pred_horizontal_16_neon_8(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride, int mode);
void ff_hevc_rpi_pred_horizontal_32_neon_8(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride, int mode);
void ff_hevc_rpi_pred_horizontal_c_4_neon_8(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride, int mode);
void ff_hevc_rpi_pred_horizontal_c_8_neon_8(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride, int mode);
void ff_hevc_rpi_pred_horizontal_c_16_neon_8(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride, int mode);
void ff_hevc_rpi_pred_horizontal_4_neon_10(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride, int mode);
void ff_hevc_rpi_pred_horizontal_8_neon_10(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride, int mode);
void ff_hevc_rpi_pred_horizontal_16_neon_10(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride, int mode);
void ff_hevc_rpi_pred_horizontal_32_neon_10(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride, int mode);
void ff_hevc_rpi_pred_horizontal_c_4_neon_10(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride, int mode);
void ff_hevc_rpi_pred_horizontal_c_8_neon_10(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride, int mode);
void ff_hevc_rpi_pred_horizontal_c_16_neon_10(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride, int mode);

void ff_hevc_rpi_pred_planar_4_neon_8(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride);
void ff_hevc_rpi_pred_planar_8_neon_8(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride);
void ff_hevc_rpi_pred_planar_16_neon_8(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride);
void ff_hevc_rpi_pred_planar_32_neon_8(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride);
void ff_hevc_rpi_pred_planar_c_4_neon_8(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride);
void ff_hevc_rpi_pred_planar_c_8_neon_8(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride);
void ff_hevc_rpi_pred_planar_c_16_neon_8(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride);
void ff_hevc_rpi_pred_planar_4_neon_10(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride);
void ff_hevc_rpi_pred_planar_8_neon_10(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride);
void ff_hevc_rpi_pred_planar_16_neon_10(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride);
void ff_hevc_rpi_pred_planar_32_neon_10(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride);
void ff_hevc_rpi_pred_planar_c_4_neon_10(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride);
void ff_hevc_rpi_pred_planar_c_8_neon_10(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride);
void ff_hevc_rpi_pred_planar_c_16_neon_10(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride);

void ff_hevc_rpi_pred_dc_4_neon_8(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride);
void ff_hevc_rpi_pred_dc_8_neon_8(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride);
void ff_hevc_rpi_pred_dc_16_neon_8(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride);
void ff_hevc_rpi_pred_dc_32_neon_8(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride);
void ff_hevc_rpi_pred_dc_c_4_neon_8(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride);
void ff_hevc_rpi_pred_dc_c_8_neon_8(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride);
void ff_hevc_rpi_pred_dc_c_16_neon_8(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride);
void ff_hevc_rpi_pred_dc_4_neon_10(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride);
void ff_hevc_rpi_pred_dc_8_neon_10(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride);
void ff_hevc_rpi_pred_dc_16_neon_10(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride);
void ff_hevc_rpi_pred_dc_32_neon_10(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride);
void ff_hevc_rpi_pred_dc_c_4_neon_10(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride);
void ff_hevc_rpi_pred_dc_c_8_neon_10(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride);
void ff_hevc_rpi_pred_dc_c_16_neon_10(uint8_t *src, const uint8_t *top, const uint8_t *left, ptrdiff_t stride);

void ff_hevc_rpi_pred_init_neon(HEVCRpiPredContext * const c, const int bit_depth)
{
    switch (bit_depth)
    {
    case 8:
        c->intra_filter[0] = ff_hevc_rpi_intra_filter_4_neon_8;
        c->intra_filter[1] = ff_hevc_rpi_intra_filter_8_neon_8;
        c->intra_filter_c[0] = ff_hevc_rpi_intra_filter_4_neon_16;  // Equivalent to c_4_neon_8
        c->intra_filter_c[1] = ff_hevc_rpi_intra_filter_8_neon_16;
        c->intra_filter_c[2] = ff_hevc_rpi_intra_filter_16_neon_16;

        c->pred_angular[0] = ff_hevc_rpi_pred_angular_4_neon_8;
        c->pred_angular[1] = ff_hevc_rpi_pred_angular_8_neon_8;
        c->pred_angular[2] = ff_hevc_rpi_pred_angular_16_neon_8;
        c->pred_angular[3] = ff_hevc_rpi_pred_angular_32_neon_8;
        c->pred_angular_c[0] = ff_hevc_rpi_pred_angular_c_4_neon_8;
        c->pred_angular_c[1] = ff_hevc_rpi_pred_angular_c_8_neon_8;
        c->pred_angular_c[2] = ff_hevc_rpi_pred_angular_c_16_neon_8;

        c->pred_horizontal[0] = ff_hevc_rpi_pred_horizontal_4_neon_8;
        c->pred_horizontal[1] = ff_hevc_rpi_pred_horizontal_8_neon_8;
        c->pred_horizontal[2] = ff_hevc_rpi_pred_horizontal_16_neon_8;
        c->pred_horizontal[3] = ff_hevc_rpi_pred_horizontal_32_neon_8;
        c->pred_horizontal_c[0] = ff_hevc_rpi_pred_horizontal_c_4_neon_8;
        c->pred_horizontal_c[1] = ff_hevc_rpi_pred_horizontal_c_8_neon_8;
        c->pred_horizontal_c[2] = ff_hevc_rpi_pred_horizontal_c_16_neon_8;

        c->pred_vertical[0] = ff_hevc_rpi_pred_vertical_4_neon_8;
        c->pred_vertical[1] = ff_hevc_rpi_pred_vertical_8_neon_8;
        c->pred_vertical[2] = ff_hevc_rpi_pred_vertical_16_neon_8;
        c->pred_vertical[3] = ff_hevc_rpi_pred_vertical_32_neon_8;
        c->pred_vertical_c[0] = ff_hevc_rpi_pred_vertical_c_4_neon_8;
        c->pred_vertical_c[1] = ff_hevc_rpi_pred_vertical_c_8_neon_8;
        c->pred_vertical_c[2] = ff_hevc_rpi_pred_vertical_c_16_neon_8;

        c->pred_planar[0] = ff_hevc_rpi_pred_planar_4_neon_8;
        c->pred_planar[1] = ff_hevc_rpi_pred_planar_8_neon_8;
        c->pred_planar[2] = ff_hevc_rpi_pred_planar_16_neon_8;
        c->pred_planar[3] = ff_hevc_rpi_pred_planar_32_neon_8;
        c->pred_planar_c[0] = ff_hevc_rpi_pred_planar_c_4_neon_8;
        c->pred_planar_c[1] = ff_hevc_rpi_pred_planar_c_8_neon_8;
        c->pred_planar_c[2] = ff_hevc_rpi_pred_planar_c_16_neon_8;

        c->pred_dc[0]   = ff_hevc_rpi_pred_dc_4_neon_8;
        c->pred_dc[1]   = ff_hevc_rpi_pred_dc_8_neon_8;
        c->pred_dc[2]   = ff_hevc_rpi_pred_dc_16_neon_8;
        c->pred_dc[3]   = ff_hevc_rpi_pred_dc_32_neon_8;
        c->pred_dc_c[0] = ff_hevc_rpi_pred_dc_c_4_neon_8;
        c->pred_dc_c[1] = ff_hevc_rpi_pred_dc_c_8_neon_8;
        c->pred_dc_c[2] = ff_hevc_rpi_pred_dc_c_16_neon_8;
        break;
    case 10:
        c->intra_filter[0] = ff_hevc_rpi_intra_filter_4_neon_16;
        c->intra_filter[1] = ff_hevc_rpi_intra_filter_8_neon_16;
        c->intra_filter[2] = ff_hevc_rpi_intra_filter_16_neon_16;
        c->intra_filter_c[0] = ff_hevc_rpi_intra_filter_4_neon_32;
        c->intra_filter_c[1] = ff_hevc_rpi_intra_filter_8_neon_32;
        c->intra_filter_c[2] = ff_hevc_rpi_intra_filter_16_neon_32;

        c->pred_angular[0] = ff_hevc_rpi_pred_angular_4_neon_10;
        c->pred_angular[1] = ff_hevc_rpi_pred_angular_8_neon_10;
        c->pred_angular[2] = ff_hevc_rpi_pred_angular_16_neon_10;
        c->pred_angular[3] = ff_hevc_rpi_pred_angular_32_neon_10;
        c->pred_angular_c[0] = ff_hevc_rpi_pred_angular_c_4_neon_10;
        c->pred_angular_c[1] = ff_hevc_rpi_pred_angular_c_8_neon_10;
        c->pred_angular_c[2] = ff_hevc_rpi_pred_angular_c_16_neon_10;

        c->pred_horizontal[0] = ff_hevc_rpi_pred_horizontal_4_neon_10;
        c->pred_horizontal[1] = ff_hevc_rpi_pred_horizontal_8_neon_10;
        c->pred_horizontal[2] = ff_hevc_rpi_pred_horizontal_16_neon_10;
        c->pred_horizontal[3] = ff_hevc_rpi_pred_horizontal_32_neon_10;
        c->pred_horizontal_c[0] = ff_hevc_rpi_pred_horizontal_c_4_neon_10;
        c->pred_horizontal_c[1] = ff_hevc_rpi_pred_horizontal_c_8_neon_10;
        c->pred_horizontal_c[2] = ff_hevc_rpi_pred_horizontal_c_16_neon_10;

        c->pred_vertical[0] = ff_hevc_rpi_pred_vertical_4_neon_10;
        c->pred_vertical[1] = ff_hevc_rpi_pred_vertical_8_neon_10;
        c->pred_vertical[2] = ff_hevc_rpi_pred_vertical_16_neon_10;
        c->pred_vertical[3] = ff_hevc_rpi_pred_vertical_32_neon_10;
        c->pred_vertical_c[0] = ff_hevc_rpi_pred_vertical_c_4_neon_10;
        c->pred_vertical_c[1] = ff_hevc_rpi_pred_vertical_c_8_neon_10;
        c->pred_vertical_c[2] = ff_hevc_rpi_pred_vertical_c_16_neon_10;

        c->pred_planar[0] = ff_hevc_rpi_pred_planar_4_neon_10;
        c->pred_planar[1] = ff_hevc_rpi_pred_planar_8_neon_10;
        c->pred_planar[2] = ff_hevc_rpi_pred_planar_16_neon_10;
        c->pred_planar[3] = ff_hevc_rpi_pred_planar_32_neon_10;
        c->pred_planar_c[0] = ff_hevc_rpi_pred_planar_c_4_neon_10;
        c->pred_planar_c[1] = ff_hevc_rpi_pred_planar_c_8_neon_10;
        c->pred_planar_c[2] = ff_hevc_rpi_pred_planar_c_16_neon_10;

        c->pred_dc[0]   = ff_hevc_rpi_pred_dc_4_neon_10;
        c->pred_dc[1]   = ff_hevc_rpi_pred_dc_8_neon_10;
        c->pred_dc[2]   = ff_hevc_rpi_pred_dc_16_neon_10;
        c->pred_dc[3]   = ff_hevc_rpi_pred_dc_32_neon_10;
        c->pred_dc_c[0] = ff_hevc_rpi_pred_dc_c_4_neon_10;
        c->pred_dc_c[1] = ff_hevc_rpi_pred_dc_c_8_neon_10;
        c->pred_dc_c[2] = ff_hevc_rpi_pred_dc_c_16_neon_10;
        break;
    default:
        break;
    }
}

