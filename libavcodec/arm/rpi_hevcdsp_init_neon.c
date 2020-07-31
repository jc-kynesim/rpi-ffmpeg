/*
 * Copyright (c) 2014 Seppo Tomperi <seppo.tomperi@vtt.fi>
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

#include "config.h"
#include "libavutil/attributes.h"
#include "libavutil/arm/cpu.h"
#include "libavcodec/rpi_hevcdsp.h"
#include "rpi_hevcdsp_arm.h"
#include "libavcodec/avcodec.h"
#include "libavcodec/bit_depth_template.c"

// NEON inter pred fns for qpel & epel (non-sand) exist in the git repo but
// have been removed from head as we never use them.

void ff_hevc_rpi_v_loop_filter_luma_neon_8(uint8_t *_pix, ptrdiff_t _stride, int _beta, int *_tc, uint8_t *_no_p, uint8_t *_no_q);
void ff_hevc_rpi_h_loop_filter_luma_neon_8(uint8_t *_pix, ptrdiff_t _stride, int _beta, int *_tc, uint8_t *_no_p, uint8_t *_no_q);

void ff_hevc_rpi_v_loop_filter_luma_neon_10(uint8_t *_pix, ptrdiff_t _stride, int _beta, int *_tc, uint8_t *_no_p, uint8_t *_no_q);
void ff_hevc_rpi_h_loop_filter_luma_neon_10(uint8_t *_pix, ptrdiff_t _stride, int _beta, int *_tc, uint8_t *_no_p, uint8_t *_no_q);

void ff_hevc_rpi_h_loop_filter_luma2_neon_8(uint8_t * _pix_r,
                             unsigned int _stride, unsigned int beta, unsigned int tc2, unsigned int no_f);
void ff_hevc_rpi_v_loop_filter_luma2_neon_8(uint8_t * _pix_r,
                             unsigned int _stride, unsigned int beta, unsigned int tc2, unsigned int no_f,
                             uint8_t * _pix_l);
void ff_hevc_rpi_h_loop_filter_uv_neon_8(uint8_t * src, unsigned int stride, uint32_t tc4,
                             unsigned int no_f);
void ff_hevc_rpi_v_loop_filter_uv2_neon_8(uint8_t * src_r, unsigned int stride, uint32_t tc4,
                             uint8_t * src_l,
                             unsigned int no_f);

void ff_hevc_rpi_h_loop_filter_luma2_neon_10(uint8_t * _pix_r,
                             unsigned int _stride, unsigned int beta, unsigned int tc2, unsigned int no_f);
void ff_hevc_rpi_v_loop_filter_luma2_neon_10(uint8_t * _pix_r,
                             unsigned int _stride, unsigned int beta, unsigned int tc2, unsigned int no_f,
                             uint8_t * _pix_l);
void ff_hevc_rpi_h_loop_filter_uv_neon_10(uint8_t * src, unsigned int stride, uint32_t tc4,
                             unsigned int no_f);
void ff_hevc_rpi_v_loop_filter_uv2_neon_10(uint8_t * src_r, unsigned int stride, uint32_t tc4,
                             uint8_t * src_l,
                             unsigned int no_f);

void ff_hevc_rpi_transform_4x4_neon_8(int16_t *coeffs, int col_limit);
void ff_hevc_rpi_transform_8x8_neon_8(int16_t *coeffs, int col_limit);
void ff_hevc_rpi_idct_4x4_dc_neon_8(int16_t *coeffs);
void ff_hevc_rpi_idct_8x8_dc_neon_8(int16_t *coeffs);
void ff_hevc_rpi_idct_16x16_dc_neon_8(int16_t *coeffs);
void ff_hevc_rpi_idct_32x32_dc_neon_8(int16_t *coeffs);
void ff_hevc_rpi_transform_luma_4x4_neon_8(int16_t *coeffs);

void ff_hevc_rpi_transform_4x4_neon_10(int16_t *coeffs, int col_limit);
void ff_hevc_rpi_transform_8x8_neon_10(int16_t *coeffs, int col_limit);
void ff_hevc_rpi_idct_4x4_dc_neon_10(int16_t *coeffs);
void ff_hevc_rpi_idct_8x8_dc_neon_10(int16_t *coeffs);
void ff_hevc_rpi_idct_16x16_dc_neon_10(int16_t *coeffs);
void ff_hevc_rpi_idct_32x32_dc_neon_10(int16_t *coeffs);
void ff_hevc_rpi_transform_luma_4x4_neon_10(int16_t *coeffs);

void ff_hevc_rpi_add_residual_4x4_neon_8(uint8_t *_dst, int16_t *coeffs,
                                     ptrdiff_t stride);
void ff_hevc_rpi_add_residual_8x8_neon_8(uint8_t *_dst, int16_t *coeffs,
                                     ptrdiff_t stride);
void ff_hevc_rpi_add_residual_16x16_neon_8(uint8_t *_dst, int16_t *coeffs,
                                       ptrdiff_t stride);
void ff_hevc_rpi_add_residual_32x32_neon_8(uint8_t *_dst, int16_t *coeffs,
                                       ptrdiff_t stride);

void ff_hevc_rpi_add_residual_4x4_dc_neon_8(uint8_t *_dst, ptrdiff_t stride, int dc);
void ff_hevc_rpi_add_residual_8x8_dc_neon_8(uint8_t *_dst, ptrdiff_t stride, int dc);
void ff_hevc_rpi_add_residual_16x16_dc_neon_8(uint8_t *_dst, ptrdiff_t stride, int dc);
void ff_hevc_rpi_add_residual_32x32_dc_neon_8(uint8_t *_dst, ptrdiff_t stride, int dc);


void ff_hevc_rpi_add_residual_4x4_neon_10(uint8_t *_dst, int16_t *coeffs,
                                     ptrdiff_t stride);
void ff_hevc_rpi_add_residual_8x8_neon_10(uint8_t *_dst, int16_t *coeffs,
                                     ptrdiff_t stride);
void ff_hevc_rpi_add_residual_16x16_neon_10(uint8_t *_dst, int16_t *coeffs,
                                       ptrdiff_t stride);
void ff_hevc_rpi_add_residual_32x32_neon_10(uint8_t *_dst, int16_t *coeffs,
                                       ptrdiff_t stride);

void ff_hevc_rpi_add_residual_4x4_dc_neon_10(uint8_t *_dst, ptrdiff_t stride, int dc);
void ff_hevc_rpi_add_residual_8x8_dc_neon_10(uint8_t *_dst, ptrdiff_t stride, int dc);
void ff_hevc_rpi_add_residual_16x16_dc_neon_10(uint8_t *_dst, ptrdiff_t stride, int dc);
void ff_hevc_rpi_add_residual_32x32_dc_neon_10(uint8_t *_dst, ptrdiff_t stride, int dc);


void ff_hevc_rpi_add_residual_4x4_u_neon_8(uint8_t *_dst, const int16_t * residual,
                                       ptrdiff_t stride, int dc_v);
void ff_hevc_rpi_add_residual_8x8_u_neon_8(uint8_t *_dst, const int16_t * residual,
                                       ptrdiff_t stride, int dc_v);
void ff_hevc_rpi_add_residual_16x16_u_neon_8(uint8_t *_dst, const int16_t * residual,
                                       ptrdiff_t stride, int dc_v);
void ff_hevc_rpi_add_residual_4x4_v_neon_8(uint8_t *_dst, const int16_t * residual,
                                       ptrdiff_t stride, int dc_u);
void ff_hevc_rpi_add_residual_8x8_v_neon_8(uint8_t *_dst, const int16_t * residual,
                                       ptrdiff_t stride, int dc_u);
void ff_hevc_rpi_add_residual_16x16_v_neon_8(uint8_t *_dst, const int16_t * residual,
                                       ptrdiff_t stride, int dc_u);
void ff_hevc_rpi_add_residual_4x4_c_neon_8(uint8_t *_dst, const int16_t * residual,
                                       ptrdiff_t stride);
void ff_hevc_rpi_add_residual_8x8_c_neon_8(uint8_t *_dst, const int16_t * residual,
                                       ptrdiff_t stride);
void ff_hevc_rpi_add_residual_16x16_c_neon_8(uint8_t *_dst, const int16_t * residual,
                                       ptrdiff_t stride);
void ff_hevc_rpi_add_residual_4x4_dc_c_neon_8(uint8_t *_dst, ptrdiff_t stride, int32_t dc);
void ff_hevc_rpi_add_residual_8x8_dc_c_neon_8(uint8_t *_dst, ptrdiff_t stride, int32_t dc);
void ff_hevc_rpi_add_residual_16x16_dc_c_neon_8(uint8_t *_dst, ptrdiff_t stride, int32_t dc);


void ff_hevc_rpi_add_residual_4x4_u_neon_10(uint8_t *_dst, const int16_t * residual,
                                       ptrdiff_t stride, int dc_v);
void ff_hevc_rpi_add_residual_8x8_u_neon_10(uint8_t *_dst, const int16_t * residual,
                                       ptrdiff_t stride, int dc_v);
void ff_hevc_rpi_add_residual_16x16_u_neon_10(uint8_t *_dst, const int16_t * residual,
                                       ptrdiff_t stride, int dc_v);
void ff_hevc_rpi_add_residual_4x4_v_neon_10(uint8_t *_dst, const int16_t * residual,
                                       ptrdiff_t stride, int dc_u);
void ff_hevc_rpi_add_residual_8x8_v_neon_10(uint8_t *_dst, const int16_t * residual,
                                       ptrdiff_t stride, int dc_u);
void ff_hevc_rpi_add_residual_16x16_v_neon_10(uint8_t *_dst, const int16_t * residual,
                                       ptrdiff_t stride, int dc_u);
void ff_hevc_rpi_add_residual_4x4_c_neon_10(uint8_t *_dst, const int16_t * residual,
                                       ptrdiff_t stride);
void ff_hevc_rpi_add_residual_8x8_c_neon_10(uint8_t *_dst, const int16_t * residual,
                                       ptrdiff_t stride);
void ff_hevc_rpi_add_residual_16x16_c_neon_10(uint8_t *_dst, const int16_t * residual,
                                       ptrdiff_t stride);
void ff_hevc_rpi_add_residual_4x4_dc_c_neon_10(uint8_t *_dst, ptrdiff_t stride, int32_t dc);
void ff_hevc_rpi_add_residual_8x8_dc_c_neon_10(uint8_t *_dst, ptrdiff_t stride, int32_t dc);
void ff_hevc_rpi_add_residual_16x16_dc_c_neon_10(uint8_t *_dst, ptrdiff_t stride, int32_t dc);

void ff_hevc_rpi_sao_edge_8_neon_8(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, int16_t *_sao_offset_val, int eo, int width, int height);
void ff_hevc_rpi_sao_edge_16_neon_8(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, int16_t *_sao_offset_val, int eo, int width, int height);
void ff_hevc_rpi_sao_edge_32_neon_8(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, int16_t *_sao_offset_val, int eo, int width, int height);
void ff_hevc_rpi_sao_edge_64_neon_8(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, int16_t *_sao_offset_val, int eo, int width, int height);

void ff_hevc_rpi_sao_edge_8_neon_10(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, int16_t *_sao_offset_val, int eo, int width, int height);
void ff_hevc_rpi_sao_edge_16_neon_10(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, int16_t *_sao_offset_val, int eo, int width, int height);
void ff_hevc_rpi_sao_edge_32_neon_10(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, int16_t *_sao_offset_val, int eo, int width, int height);
void ff_hevc_rpi_sao_edge_64_neon_10(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, int16_t *_sao_offset_val, int eo, int width, int height);

void ff_hevc_rpi_sao_edge_c_8_neon_8(uint8_t *_dst, const uint8_t *_src, ptrdiff_t stride_dst,
                                  const int16_t *_sao_offset_val_u, const int16_t *_sao_offset_val_v,
                                  int eo, int width, int height);
void ff_hevc_rpi_sao_edge_c_16_neon_8(uint8_t *_dst, const uint8_t *_src, ptrdiff_t stride_dst,
                                  const int16_t *_sao_offset_val_u, const int16_t *_sao_offset_val_v,
                                  int eo, int width, int height);
void ff_hevc_rpi_sao_edge_c_32_neon_8(uint8_t *_dst, const uint8_t *_src, ptrdiff_t stride_dst,
                                  const int16_t *_sao_offset_val_u, const int16_t *_sao_offset_val_v,
                                  int eo, int width, int height);

void ff_hevc_rpi_sao_edge_c_8_neon_10(uint8_t *_dst, const uint8_t *_src, ptrdiff_t stride_dst,
                                  const int16_t *_sao_offset_val_u, const int16_t *_sao_offset_val_v,
                                  int eo, int width, int height);
void ff_hevc_rpi_sao_edge_c_16_neon_10(uint8_t *_dst, const uint8_t *_src, ptrdiff_t stride_dst,
                                  const int16_t *_sao_offset_val_u, const int16_t *_sao_offset_val_v,
                                  int eo, int width, int height);
void ff_hevc_rpi_sao_edge_c_32_neon_10(uint8_t *_dst, const uint8_t *_src, ptrdiff_t stride_dst,
                                  const int16_t *_sao_offset_val_u, const int16_t *_sao_offset_val_v,
                                  int eo, int width, int height);

void ff_hevc_rpi_sao_band_c_8_neon_8(uint8_t *_dst, const uint8_t *_src,
                                  ptrdiff_t stride_dst, ptrdiff_t stride_src,
                                  const int16_t *sao_offset_val_u, int sao_left_class_u,
                                  const int16_t *sao_offset_val_v, int sao_left_class_v,
                                  int width, int height);
void ff_hevc_rpi_sao_band_c_16_neon_8(uint8_t *_dst, const uint8_t *_src,
                                  ptrdiff_t stride_dst, ptrdiff_t stride_src,
                                  const int16_t *sao_offset_val_u, int sao_left_class_u,
                                  const int16_t *sao_offset_val_v, int sao_left_class_v,
                                  int width, int height);
void ff_hevc_rpi_sao_band_c_32_neon_8(uint8_t *_dst, const uint8_t *_src,
                                  ptrdiff_t stride_dst, ptrdiff_t stride_src,
                                  const int16_t *sao_offset_val_u, int sao_left_class_u,
                                  const int16_t *sao_offset_val_v, int sao_left_class_v,
                                  int width, int height);

void ff_hevc_rpi_sao_band_c_8_neon_10(uint8_t *_dst, const uint8_t *_src,
                                  ptrdiff_t stride_dst, ptrdiff_t stride_src,
                                  const int16_t *sao_offset_val_u, int sao_left_class_u,
                                  const int16_t *sao_offset_val_v, int sao_left_class_v,
                                  int width, int height);
void ff_hevc_rpi_sao_band_c_16_neon_10(uint8_t *_dst, const uint8_t *_src,
                                  ptrdiff_t stride_dst, ptrdiff_t stride_src,
                                  const int16_t *sao_offset_val_u, int sao_left_class_u,
                                  const int16_t *sao_offset_val_v, int sao_left_class_v,
                                  int width, int height);
void ff_hevc_rpi_sao_band_c_32_neon_10(uint8_t *_dst, const uint8_t *_src,
                                  ptrdiff_t stride_dst, ptrdiff_t stride_src,
                                  const int16_t *sao_offset_val_u, int sao_left_class_u,
                                  const int16_t *sao_offset_val_v, int sao_left_class_v,
                                  int width, int height);

void ff_hevc_rpi_sao_band_8_neon_8(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, ptrdiff_t stride_src,
                                int16_t *sao_offset_val, int sao_left_class, int width, int height);
void ff_hevc_rpi_sao_band_16_neon_8(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, ptrdiff_t stride_src,
                                int16_t *sao_offset_val, int sao_left_class, int width, int height);
void ff_hevc_rpi_sao_band_32_neon_8(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, ptrdiff_t stride_src,
                                int16_t *sao_offset_val, int sao_left_class, int width, int height);
void ff_hevc_rpi_sao_band_64_neon_8(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, ptrdiff_t stride_src,
                                int16_t *sao_offset_val, int sao_left_class, int width, int height);

void ff_hevc_rpi_sao_band_8_neon_10(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, ptrdiff_t stride_src,
                                int16_t *sao_offset_val, int sao_left_class, int width, int height);
void ff_hevc_rpi_sao_band_16_neon_10(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, ptrdiff_t stride_src,
                                int16_t *sao_offset_val, int sao_left_class, int width, int height);
void ff_hevc_rpi_sao_band_32_neon_10(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, ptrdiff_t stride_src,
                                int16_t *sao_offset_val, int sao_left_class, int width, int height);
void ff_hevc_rpi_sao_band_64_neon_10(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, ptrdiff_t stride_src,
                                int16_t *sao_offset_val, int sao_left_class, int width, int height);


uint32_t ff_hevc_rpi_deblocking_boundary_strengths_neon(int pus, int dup, const struct HEVCRpiMvField *curr, const struct HEVCRpiMvField *neigh,
                                                const int *curr_rpl0, const int *curr_rpl1, const int *neigh_rpl0, const int *neigh_rpl1,
                                                int in_inc0, int in_inc1);
void ff_hevc_rpi_cpy_blks8x4_neon(uint8_t *dst, unsigned int stride_dst, const uint8_t *src, unsigned stride_src, unsigned int width, unsigned int height);


static void ff_hevc_rpi_sao_edge_48_neon_8(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, int16_t *_sao_offset_val, int eo, int width, int height)
{
    ff_hevc_rpi_sao_edge_32_neon_8(_dst, _src, stride_dst, _sao_offset_val, eo, 32, height);
    ff_hevc_rpi_sao_edge_16_neon_8(_dst + 32, _src + 32, stride_dst, _sao_offset_val, eo, 16, height);
}
static void ff_hevc_rpi_sao_edge_48_neon_10(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, int16_t *_sao_offset_val, int eo, int width, int height)
{
    ff_hevc_rpi_sao_edge_32_neon_10(_dst, _src, stride_dst, _sao_offset_val, eo, 32, height);
    ff_hevc_rpi_sao_edge_16_neon_10(_dst + 64, _src + 64, stride_dst, _sao_offset_val, eo, 16, height);
}

static void ff_hevc_rpi_sao_band_48_neon_8(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, ptrdiff_t stride_src,
                                int16_t *sao_offset_val, int sao_left_class, int width, int height)
{
    ff_hevc_rpi_sao_band_32_neon_8(_dst, _src, stride_dst, stride_src, sao_offset_val, sao_left_class, 32, height);
    ff_hevc_rpi_sao_band_16_neon_8(_dst + 32, _src + 32, stride_dst, stride_src, sao_offset_val, sao_left_class, 16, height);
}
static void ff_hevc_rpi_sao_band_48_neon_10(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, ptrdiff_t stride_src,
                                int16_t *sao_offset_val, int sao_left_class, int width, int height)
{
    ff_hevc_rpi_sao_band_32_neon_10(_dst, _src, stride_dst, stride_src, sao_offset_val, sao_left_class, 32, height);
    ff_hevc_rpi_sao_band_16_neon_10(_dst + 64, _src + 64, stride_dst, stride_src, sao_offset_val, sao_left_class, 16, height);
}

#if SAO_FILTER_N == 6
static void ff_hevc_rpi_sao_edge_24_neon_8(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, int16_t *_sao_offset_val, int eo, int width, int height)
{
    ff_hevc_rpi_sao_edge_16_neon_8(_dst, _src, stride_dst, _sao_offset_val, eo, 16, height);
    ff_hevc_rpi_sao_edge_8_neon_8(_dst + 16, _src + 16, stride_dst, _sao_offset_val, eo, 8, height);
}
static void ff_hevc_rpi_sao_edge_24_neon_10(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, int16_t *_sao_offset_val, int eo, int width, int height)
{
    ff_hevc_rpi_sao_edge_16_neon_10(_dst, _src, stride_dst, _sao_offset_val, eo, 16, height);
    ff_hevc_rpi_sao_edge_8_neon_10(_dst + 32, _src + 32, stride_dst, _sao_offset_val, eo, 8, height);
}

static void ff_hevc_rpi_sao_band_24_neon_8(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, ptrdiff_t stride_src,
                                int16_t *sao_offset_val, int sao_left_class, int width, int height)
{
    ff_hevc_rpi_sao_band_16_neon_8(_dst, _src, stride_dst, stride_src, sao_offset_val, sao_left_class, 16, height);
    ff_hevc_rpi_sao_band_8_neon_8(_dst + 16, _src + 16, stride_dst, stride_src, sao_offset_val, sao_left_class, 8, height);
}
static void ff_hevc_rpi_sao_band_24_neon_10(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, ptrdiff_t stride_src,
                                int16_t *sao_offset_val, int sao_left_class, int width, int height)
{
    ff_hevc_rpi_sao_band_16_neon_10(_dst, _src, stride_dst, stride_src, sao_offset_val, sao_left_class, 16, height);
    ff_hevc_rpi_sao_band_8_neon_10(_dst + 32, _src + 32, stride_dst, stride_src, sao_offset_val, sao_left_class, 8, height);
}

static void ff_hevc_rpi_sao_edge_c_24_neon_8(uint8_t *_dst, const uint8_t *_src, ptrdiff_t stride_dst,
                                  const int16_t *_sao_offset_val_u, const int16_t *_sao_offset_val_v,
                                  int eo, int width, int height)
{
    ff_hevc_rpi_sao_edge_c_16_neon_8(_dst, _src, stride_dst, _sao_offset_val_u, _sao_offset_val_v, eo, 16, height);
    ff_hevc_rpi_sao_edge_c_8_neon_8(_dst + 32, _src + 32, stride_dst, _sao_offset_val_u, _sao_offset_val_v, eo, 8, height);
}
static void ff_hevc_rpi_sao_edge_c_24_neon_10(uint8_t *_dst, const uint8_t *_src, ptrdiff_t stride_dst,
                                  const int16_t *_sao_offset_val_u, const int16_t *_sao_offset_val_v,
                                  int eo, int width, int height)
{
    ff_hevc_rpi_sao_edge_c_16_neon_10(_dst, _src, stride_dst, _sao_offset_val_u, _sao_offset_val_v, eo, 16, height);
    ff_hevc_rpi_sao_edge_c_8_neon_10(_dst + 64, _src + 64, stride_dst, _sao_offset_val_u, _sao_offset_val_v, eo, 8, height);
}

static void ff_hevc_rpi_sao_band_c_24_neon_8(uint8_t *_dst, const uint8_t *_src,
                                  ptrdiff_t stride_dst, ptrdiff_t stride_src,
                                  const int16_t *sao_offset_val_u, int sao_left_class_u,
                                  const int16_t *sao_offset_val_v, int sao_left_class_v,
                                  int width, int height)
{
    ff_hevc_rpi_sao_band_c_16_neon_8(_dst, _src, stride_dst, stride_src,
                                sao_offset_val_u, sao_left_class_u, sao_offset_val_v, sao_left_class_v, 16, height);
    ff_hevc_rpi_sao_band_c_8_neon_8(_dst + 32, _src + 32, stride_dst, stride_src,
                                sao_offset_val_u, sao_left_class_u, sao_offset_val_v, sao_left_class_v, 8, height);
}
static void ff_hevc_rpi_sao_band_c_24_neon_10(uint8_t *_dst, const uint8_t *_src,
                                  ptrdiff_t stride_dst, ptrdiff_t stride_src,
                                  const int16_t *sao_offset_val_u, int sao_left_class_u,
                                  const int16_t *sao_offset_val_v, int sao_left_class_v,
                                  int width, int height)
{
    ff_hevc_rpi_sao_band_c_16_neon_10(_dst, _src, stride_dst, stride_src,
                                sao_offset_val_u, sao_left_class_u, sao_offset_val_v, sao_left_class_v, 16, height);
    ff_hevc_rpi_sao_band_c_8_neon_10(_dst + 64, _src + 64, stride_dst, stride_src,
                                sao_offset_val_u, sao_left_class_u, sao_offset_val_v, sao_left_class_v, 8, height);
}
#endif



#if RPI_HEVC_SAO_BUF_STRIDE != 160
#error SAO edge src stride not 160 - value used in .S
#endif

av_cold void ff_hevcdsp_rpi_init_neon(HEVCDSPContext *c, const int bit_depth)
{
    if (bit_depth == 8) {
        c->hevc_v_loop_filter_luma     = ff_hevc_rpi_v_loop_filter_luma_neon_8;
        c->hevc_v_loop_filter_luma_c   = ff_hevc_rpi_v_loop_filter_luma_neon_8;
        c->hevc_h_loop_filter_luma     = ff_hevc_rpi_h_loop_filter_luma_neon_8;
        c->hevc_h_loop_filter_luma_c   = ff_hevc_rpi_h_loop_filter_luma_neon_8;
        c->hevc_h_loop_filter_luma2    = ff_hevc_rpi_h_loop_filter_luma2_neon_8;
        c->hevc_v_loop_filter_luma2    = ff_hevc_rpi_v_loop_filter_luma2_neon_8;
        c->hevc_h_loop_filter_uv       = ff_hevc_rpi_h_loop_filter_uv_neon_8;
        c->hevc_v_loop_filter_uv2      = ff_hevc_rpi_v_loop_filter_uv2_neon_8;
        c->idct[0]                     = ff_hevc_rpi_transform_4x4_neon_8;
        c->idct[1]                     = ff_hevc_rpi_transform_8x8_neon_8;
        c->idct_dc[0]                  = ff_hevc_rpi_idct_4x4_dc_neon_8;
        c->idct_dc[1]                  = ff_hevc_rpi_idct_8x8_dc_neon_8;
        c->idct_dc[2]                  = ff_hevc_rpi_idct_16x16_dc_neon_8;
        c->idct_dc[3]                  = ff_hevc_rpi_idct_32x32_dc_neon_8;
        c->add_residual[0]             = ff_hevc_rpi_add_residual_4x4_neon_8;
        c->add_residual[1]             = ff_hevc_rpi_add_residual_8x8_neon_8;
        c->add_residual[2]             = ff_hevc_rpi_add_residual_16x16_neon_8;
        c->add_residual[3]             = ff_hevc_rpi_add_residual_32x32_neon_8;
        c->add_residual_dc[0]          = ff_hevc_rpi_add_residual_4x4_dc_neon_8;
        c->add_residual_dc[1]          = ff_hevc_rpi_add_residual_8x8_dc_neon_8;
        c->add_residual_dc[2]          = ff_hevc_rpi_add_residual_16x16_dc_neon_8;
        c->add_residual_dc[3]          = ff_hevc_rpi_add_residual_32x32_dc_neon_8;
        c->add_residual_u[0]           = ff_hevc_rpi_add_residual_4x4_u_neon_8;
        c->add_residual_u[1]           = ff_hevc_rpi_add_residual_8x8_u_neon_8;
        c->add_residual_u[2]           = ff_hevc_rpi_add_residual_16x16_u_neon_8;
        c->add_residual_v[0]           = ff_hevc_rpi_add_residual_4x4_v_neon_8;
        c->add_residual_v[1]           = ff_hevc_rpi_add_residual_8x8_v_neon_8;
        c->add_residual_v[2]           = ff_hevc_rpi_add_residual_16x16_v_neon_8;
        c->add_residual_c[0]           = ff_hevc_rpi_add_residual_4x4_c_neon_8;
        c->add_residual_c[1]           = ff_hevc_rpi_add_residual_8x8_c_neon_8;
        c->add_residual_c[2]           = ff_hevc_rpi_add_residual_16x16_c_neon_8;
        c->add_residual_dc_c[0]        = ff_hevc_rpi_add_residual_4x4_dc_c_neon_8;
        c->add_residual_dc_c[1]        = ff_hevc_rpi_add_residual_8x8_dc_c_neon_8;
        c->add_residual_dc_c[2]        = ff_hevc_rpi_add_residual_16x16_dc_c_neon_8;
        c->transform_4x4_luma          = ff_hevc_rpi_transform_luma_4x4_neon_8;
        c->sao_band_filter[0]          = ff_hevc_rpi_sao_band_8_neon_8;
        c->sao_band_filter[1]          = ff_hevc_rpi_sao_band_16_neon_8;
        c->sao_band_filter[2]          = ff_hevc_rpi_sao_band_32_neon_8;
        c->sao_band_filter[3]          = ff_hevc_rpi_sao_band_48_neon_8;
        c->sao_band_filter[4]          = ff_hevc_rpi_sao_band_64_neon_8;
        c->sao_edge_filter[0]          = ff_hevc_rpi_sao_edge_8_neon_8;
        c->sao_edge_filter[1]          = ff_hevc_rpi_sao_edge_16_neon_8;
        c->sao_edge_filter[2]          = ff_hevc_rpi_sao_edge_32_neon_8;
        c->sao_edge_filter[3]          = ff_hevc_rpi_sao_edge_48_neon_8;
        c->sao_edge_filter[4]          = ff_hevc_rpi_sao_edge_64_neon_8;
#if SAO_FILTER_N == 6
        c->sao_band_filter[5]          = ff_hevc_rpi_sao_band_24_neon_8;
        c->sao_edge_filter[5]          = ff_hevc_rpi_sao_edge_24_neon_8;
#endif
        c->sao_band_filter_c[0]        = ff_hevc_rpi_sao_band_c_8_neon_8;
        c->sao_band_filter_c[1]        = ff_hevc_rpi_sao_band_c_16_neon_8;
        c->sao_band_filter_c[2]        = ff_hevc_rpi_sao_band_c_32_neon_8;

        c->sao_edge_filter_c[0]        = ff_hevc_rpi_sao_edge_c_8_neon_8;
        c->sao_edge_filter_c[1]        = ff_hevc_rpi_sao_edge_c_16_neon_8;
        c->sao_edge_filter_c[2]        = ff_hevc_rpi_sao_edge_c_32_neon_8;

#if SAO_FILTER_N == 6
        c->sao_band_filter_c[5]        = ff_hevc_rpi_sao_band_c_24_neon_8;
        c->sao_edge_filter_c[5]        = ff_hevc_rpi_sao_edge_c_24_neon_8;
#endif
    }
    else if (bit_depth == 10) {
        c->hevc_v_loop_filter_luma     = ff_hevc_rpi_v_loop_filter_luma_neon_10;
        c->hevc_v_loop_filter_luma_c   = ff_hevc_rpi_v_loop_filter_luma_neon_10;
        c->hevc_h_loop_filter_luma     = ff_hevc_rpi_h_loop_filter_luma_neon_10;
        c->hevc_h_loop_filter_luma_c   = ff_hevc_rpi_h_loop_filter_luma_neon_10;
        c->hevc_h_loop_filter_luma2    = ff_hevc_rpi_h_loop_filter_luma2_neon_10;
        c->hevc_v_loop_filter_luma2    = ff_hevc_rpi_v_loop_filter_luma2_neon_10;
        c->hevc_h_loop_filter_uv       = ff_hevc_rpi_h_loop_filter_uv_neon_10;
        c->hevc_v_loop_filter_uv2      = ff_hevc_rpi_v_loop_filter_uv2_neon_10;
        c->idct[0]                     = ff_hevc_rpi_transform_4x4_neon_10;
        c->idct[1]                     = ff_hevc_rpi_transform_8x8_neon_10;
        c->idct_dc[0]                  = ff_hevc_rpi_idct_4x4_dc_neon_10;
        c->idct_dc[1]                  = ff_hevc_rpi_idct_8x8_dc_neon_10;
        c->idct_dc[2]                  = ff_hevc_rpi_idct_16x16_dc_neon_10;
        c->idct_dc[3]                  = ff_hevc_rpi_idct_32x32_dc_neon_10;
        c->add_residual[0]             = ff_hevc_rpi_add_residual_4x4_neon_10;
        c->add_residual[1]             = ff_hevc_rpi_add_residual_8x8_neon_10;
        c->add_residual[2]             = ff_hevc_rpi_add_residual_16x16_neon_10;
        c->add_residual[3]             = ff_hevc_rpi_add_residual_32x32_neon_10;
        c->add_residual_dc[0]          = ff_hevc_rpi_add_residual_4x4_dc_neon_10;
        c->add_residual_dc[1]          = ff_hevc_rpi_add_residual_8x8_dc_neon_10;
        c->add_residual_dc[2]          = ff_hevc_rpi_add_residual_16x16_dc_neon_10;
        c->add_residual_dc[3]          = ff_hevc_rpi_add_residual_32x32_dc_neon_10;
        c->add_residual_u[0]           = ff_hevc_rpi_add_residual_4x4_u_neon_10;
        c->add_residual_u[1]           = ff_hevc_rpi_add_residual_8x8_u_neon_10;
        c->add_residual_u[2]           = ff_hevc_rpi_add_residual_16x16_u_neon_10;
        c->add_residual_v[0]           = ff_hevc_rpi_add_residual_4x4_v_neon_10;
        c->add_residual_v[1]           = ff_hevc_rpi_add_residual_8x8_v_neon_10;
        c->add_residual_v[2]           = ff_hevc_rpi_add_residual_16x16_v_neon_10;
        c->add_residual_c[0]           = ff_hevc_rpi_add_residual_4x4_c_neon_10;
        c->add_residual_c[1]           = ff_hevc_rpi_add_residual_8x8_c_neon_10;
        c->add_residual_c[2]           = ff_hevc_rpi_add_residual_16x16_c_neon_10;
        c->add_residual_dc_c[0]        = ff_hevc_rpi_add_residual_4x4_dc_c_neon_10;
        c->add_residual_dc_c[1]        = ff_hevc_rpi_add_residual_8x8_dc_c_neon_10;
        c->add_residual_dc_c[2]        = ff_hevc_rpi_add_residual_16x16_dc_c_neon_10;
        c->transform_4x4_luma          = ff_hevc_rpi_transform_luma_4x4_neon_10;
        c->sao_band_filter[0]          = ff_hevc_rpi_sao_band_8_neon_10;
        c->sao_band_filter[1]          = ff_hevc_rpi_sao_band_16_neon_10;
        c->sao_band_filter[2]          = ff_hevc_rpi_sao_band_32_neon_10;
        c->sao_band_filter[3]          = ff_hevc_rpi_sao_band_48_neon_10;
        c->sao_band_filter[4]          = ff_hevc_rpi_sao_band_64_neon_10;

        c->sao_edge_filter[0]          = ff_hevc_rpi_sao_edge_8_neon_10;
        c->sao_edge_filter[1]          = ff_hevc_rpi_sao_edge_16_neon_10;
        c->sao_edge_filter[2]          = ff_hevc_rpi_sao_edge_32_neon_10;
        c->sao_edge_filter[3]          = ff_hevc_rpi_sao_edge_48_neon_10;
        c->sao_edge_filter[4]          = ff_hevc_rpi_sao_edge_64_neon_10;
#if SAO_FILTER_N == 6
        c->sao_band_filter[5]          = ff_hevc_rpi_sao_band_24_neon_10;
        c->sao_edge_filter[5]          = ff_hevc_rpi_sao_edge_24_neon_10;
#endif
        c->sao_band_filter_c[0]        = ff_hevc_rpi_sao_band_c_8_neon_10;
        c->sao_band_filter_c[1]        = ff_hevc_rpi_sao_band_c_16_neon_10;
        c->sao_band_filter_c[2]        = ff_hevc_rpi_sao_band_c_32_neon_10;

        c->sao_edge_filter_c[0]        = ff_hevc_rpi_sao_edge_c_8_neon_10;
        c->sao_edge_filter_c[1]        = ff_hevc_rpi_sao_edge_c_16_neon_10;
        c->sao_edge_filter_c[2]        = ff_hevc_rpi_sao_edge_c_32_neon_10;

#if SAO_FILTER_N == 6
        c->sao_band_filter_c[5]        = ff_hevc_rpi_sao_band_c_24_neon_10;
        c->sao_edge_filter_c[5]        = ff_hevc_rpi_sao_edge_c_24_neon_10;
#endif
    }

    assert(offsetof(HEVCRpiMvField, mv) == 0);
    assert(offsetof(HEVCRpiMvField, ref_idx) == 8);
    assert(offsetof(HEVCRpiMvField, pred_flag) == 10);
    c->hevc_deblocking_boundary_strengths = ff_hevc_rpi_deblocking_boundary_strengths_neon;
    c->cpy_blk = ff_hevc_rpi_cpy_blks8x4_neon;
}
