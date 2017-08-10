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

#include "libavutil/attributes.h"
#include "libavutil/arm/cpu.h"
#include "libavcodec/hevcdsp.h"
#include "hevcdsp_arm.h"
#include "libavcodec/avcodec.h"
#include "libavcodec/bit_depth_template.c"

void ff_hevc_v_loop_filter_luma_neon(uint8_t *_pix, ptrdiff_t _stride, int _beta, int *_tc, uint8_t *_no_p, uint8_t *_no_q);
void ff_hevc_h_loop_filter_luma_neon(uint8_t *_pix, ptrdiff_t _stride, int _beta, int *_tc, uint8_t *_no_p, uint8_t *_no_q);
void ff_hevc_v_loop_filter_chroma_neon(uint8_t *_pix, ptrdiff_t _stride, int *_tc, uint8_t *_no_p, uint8_t *_no_q);
void ff_hevc_h_loop_filter_chroma_neon(uint8_t *_pix, ptrdiff_t _stride, int *_tc, uint8_t *_no_p, uint8_t *_no_q);

void ff_hevc_v_loop_filter_luma_neon_10(uint8_t *_pix, ptrdiff_t _stride, int _beta, int *_tc, uint8_t *_no_p, uint8_t *_no_q);
void ff_hevc_h_loop_filter_luma_neon_10(uint8_t *_pix, ptrdiff_t _stride, int _beta, int *_tc, uint8_t *_no_p, uint8_t *_no_q);
void ff_hevc_v_loop_filter_chroma_neon_10(uint8_t *_pix, ptrdiff_t _stride, int *_tc, uint8_t *_no_p, uint8_t *_no_q);
void ff_hevc_h_loop_filter_chroma_neon_10(uint8_t *_pix, ptrdiff_t _stride, int *_tc, uint8_t *_no_p, uint8_t *_no_q);

#ifdef RPI
void ff_hevc_v_loop_filter_luma2_neon_8(uint8_t * _pix_r,
                             unsigned int _stride, unsigned int beta, const int32_t tc[2],
                             const uint8_t no_p[2], const uint8_t no_q[2],
                             uint8_t * _pix_l);
void ff_hevc_h_loop_filter_uv_neon_8(uint8_t * src, unsigned int stride, uint32_t tc4,
                             unsigned int no_f);
void ff_hevc_v_loop_filter_uv2_neon_8(uint8_t * src_r, unsigned int stride, uint32_t tc4,
                             uint8_t * src_l,
                             unsigned int no_f);

void ff_hevc_v_loop_filter_luma2_neon_10(uint8_t * _pix_r,
                             unsigned int _stride, unsigned int beta, const int32_t tc[2],
                             const uint8_t no_p[2], const uint8_t no_q[2],
                             uint8_t * _pix_l);
void ff_hevc_h_loop_filter_uv_neon_10(uint8_t * src, unsigned int stride, uint32_t tc4,
                             unsigned int no_f);
void ff_hevc_v_loop_filter_uv2_neon_10(uint8_t * src_r, unsigned int stride, uint32_t tc4,
                             uint8_t * src_l,
                             unsigned int no_f);
#endif

void ff_hevc_transform_4x4_neon_8(int16_t *coeffs, int col_limit);
void ff_hevc_transform_8x8_neon_8(int16_t *coeffs, int col_limit);
void ff_hevc_idct_4x4_dc_neon_8(int16_t *coeffs);
void ff_hevc_idct_8x8_dc_neon_8(int16_t *coeffs);
void ff_hevc_idct_16x16_dc_neon_8(int16_t *coeffs);
void ff_hevc_idct_32x32_dc_neon_8(int16_t *coeffs);
void ff_hevc_transform_luma_4x4_neon_8(int16_t *coeffs);

void ff_hevc_transform_4x4_neon_10(int16_t *coeffs, int col_limit);
void ff_hevc_transform_8x8_neon_10(int16_t *coeffs, int col_limit);
void ff_hevc_idct_4x4_dc_neon_10(int16_t *coeffs);
void ff_hevc_idct_8x8_dc_neon_10(int16_t *coeffs);
void ff_hevc_idct_16x16_dc_neon_10(int16_t *coeffs);
void ff_hevc_idct_32x32_dc_neon_10(int16_t *coeffs);
void ff_hevc_transform_luma_4x4_neon_10(int16_t *coeffs);

void ff_hevc_add_residual_4x4_neon_8(uint8_t *_dst, int16_t *coeffs,
                                     ptrdiff_t stride);
void ff_hevc_add_residual_8x8_neon_8(uint8_t *_dst, int16_t *coeffs,
                                     ptrdiff_t stride);
void ff_hevc_add_residual_16x16_neon_8(uint8_t *_dst, int16_t *coeffs,
                                       ptrdiff_t stride);
void ff_hevc_add_residual_32x32_neon_8(uint8_t *_dst, int16_t *coeffs,
                                       ptrdiff_t stride);

void ff_hevc_add_residual_4x4_neon_10(uint8_t *_dst, int16_t *coeffs,
                                     ptrdiff_t stride);
void ff_hevc_add_residual_8x8_neon_10(uint8_t *_dst, int16_t *coeffs,
                                     ptrdiff_t stride);
void ff_hevc_add_residual_16x16_neon_10(uint8_t *_dst, int16_t *coeffs,
                                       ptrdiff_t stride);
void ff_hevc_add_residual_32x32_neon_10(uint8_t *_dst, int16_t *coeffs,
                                       ptrdiff_t stride);


#if RPI_HEVC_SAND
void ff_hevc_add_residual_4x4_u_neon_8(uint8_t *_dst, const int16_t * residual,
                                       ptrdiff_t stride);
void ff_hevc_add_residual_8x8_u_neon_8(uint8_t *_dst, const int16_t * residual,
                                       ptrdiff_t stride);
void ff_hevc_add_residual_16x16_u_neon_8(uint8_t *_dst, const int16_t * residual,
                                       ptrdiff_t stride);
void ff_hevc_add_residual_4x4_v_neon_8(uint8_t *_dst, const int16_t * residual,
                                       ptrdiff_t stride);
void ff_hevc_add_residual_8x8_v_neon_8(uint8_t *_dst, const int16_t * residual,
                                       ptrdiff_t stride);
void ff_hevc_add_residual_16x16_v_neon_8(uint8_t *_dst, const int16_t * residual,
                                       ptrdiff_t stride);
void ff_hevc_add_residual_4x4_c_neon_8(uint8_t *_dst, const int16_t * residual,
                                       ptrdiff_t stride);
void ff_hevc_add_residual_8x8_c_neon_8(uint8_t *_dst, const int16_t * residual,
                                       ptrdiff_t stride);
void ff_hevc_add_residual_16x16_c_neon_8(uint8_t *_dst, const int16_t * residual,
                                       ptrdiff_t stride);


void ff_hevc_add_residual_4x4_u_neon_10(uint8_t *_dst, const int16_t * residual,
                                       ptrdiff_t stride);
void ff_hevc_add_residual_8x8_u_neon_10(uint8_t *_dst, const int16_t * residual,
                                       ptrdiff_t stride);
void ff_hevc_add_residual_16x16_u_neon_10(uint8_t *_dst, const int16_t * residual,
                                       ptrdiff_t stride);
void ff_hevc_add_residual_4x4_v_neon_10(uint8_t *_dst, const int16_t * residual,
                                       ptrdiff_t stride);
void ff_hevc_add_residual_8x8_v_neon_10(uint8_t *_dst, const int16_t * residual,
                                       ptrdiff_t stride);
void ff_hevc_add_residual_16x16_v_neon_10(uint8_t *_dst, const int16_t * residual,
                                       ptrdiff_t stride);
void ff_hevc_add_residual_4x4_c_neon_10(uint8_t *_dst, const int16_t * residual,
                                       ptrdiff_t stride);
void ff_hevc_add_residual_8x8_c_neon_10(uint8_t *_dst, const int16_t * residual,
                                       ptrdiff_t stride);
void ff_hevc_add_residual_16x16_c_neon_10(uint8_t *_dst, const int16_t * residual,
                                       ptrdiff_t stride);
#endif

void ff_hevc_sao_edge_8_neon_8(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, int16_t *_sao_offset_val, int eo, int width, int height);
void ff_hevc_sao_edge_16_neon_8(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, int16_t *_sao_offset_val, int eo, int width, int height);
void ff_hevc_sao_edge_32_neon_8(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, int16_t *_sao_offset_val, int eo, int width, int height);
void ff_hevc_sao_edge_64_neon_8(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, int16_t *_sao_offset_val, int eo, int width, int height);

void ff_hevc_sao_edge_8_neon_10(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, int16_t *_sao_offset_val, int eo, int width, int height);
void ff_hevc_sao_edge_16_neon_10(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, int16_t *_sao_offset_val, int eo, int width, int height);
void ff_hevc_sao_edge_32_neon_10(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, int16_t *_sao_offset_val, int eo, int width, int height);
void ff_hevc_sao_edge_64_neon_10(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, int16_t *_sao_offset_val, int eo, int width, int height);

#if RPI_HEVC_SAND
void ff_hevc_sao_edge_c_8_neon_8(uint8_t *_dst, const uint8_t *_src, ptrdiff_t stride_dst,
                                  const int16_t *_sao_offset_val_u, const int16_t *_sao_offset_val_v,
                                  int eo, int width, int height);
void ff_hevc_sao_edge_c_16_neon_8(uint8_t *_dst, const uint8_t *_src, ptrdiff_t stride_dst,
                                  const int16_t *_sao_offset_val_u, const int16_t *_sao_offset_val_v,
                                  int eo, int width, int height);
void ff_hevc_sao_edge_c_32_neon_8(uint8_t *_dst, const uint8_t *_src, ptrdiff_t stride_dst,
                                  const int16_t *_sao_offset_val_u, const int16_t *_sao_offset_val_v,
                                  int eo, int width, int height);

void ff_hevc_sao_edge_c_8_neon_10(uint8_t *_dst, const uint8_t *_src, ptrdiff_t stride_dst,
                                  const int16_t *_sao_offset_val_u, const int16_t *_sao_offset_val_v,
                                  int eo, int width, int height);
void ff_hevc_sao_edge_c_16_neon_10(uint8_t *_dst, const uint8_t *_src, ptrdiff_t stride_dst,
                                  const int16_t *_sao_offset_val_u, const int16_t *_sao_offset_val_v,
                                  int eo, int width, int height);
void ff_hevc_sao_edge_c_32_neon_10(uint8_t *_dst, const uint8_t *_src, ptrdiff_t stride_dst,
                                  const int16_t *_sao_offset_val_u, const int16_t *_sao_offset_val_v,
                                  int eo, int width, int height);

void ff_hevc_sao_band_c_8_neon_8(uint8_t *_dst, const uint8_t *_src,
                                  ptrdiff_t stride_dst, ptrdiff_t stride_src,
                                  const int16_t *sao_offset_val_u, int sao_left_class_u,
                                  const int16_t *sao_offset_val_v, int sao_left_class_v,
                                  int width, int height);
void ff_hevc_sao_band_c_16_neon_8(uint8_t *_dst, const uint8_t *_src,
                                  ptrdiff_t stride_dst, ptrdiff_t stride_src,
                                  const int16_t *sao_offset_val_u, int sao_left_class_u,
                                  const int16_t *sao_offset_val_v, int sao_left_class_v,
                                  int width, int height);
void ff_hevc_sao_band_c_32_neon_8(uint8_t *_dst, const uint8_t *_src,
                                  ptrdiff_t stride_dst, ptrdiff_t stride_src,
                                  const int16_t *sao_offset_val_u, int sao_left_class_u,
                                  const int16_t *sao_offset_val_v, int sao_left_class_v,
                                  int width, int height);

void ff_hevc_sao_band_c_8_neon_10(uint8_t *_dst, const uint8_t *_src,
                                  ptrdiff_t stride_dst, ptrdiff_t stride_src,
                                  const int16_t *sao_offset_val_u, int sao_left_class_u,
                                  const int16_t *sao_offset_val_v, int sao_left_class_v,
                                  int width, int height);
void ff_hevc_sao_band_c_16_neon_10(uint8_t *_dst, const uint8_t *_src,
                                  ptrdiff_t stride_dst, ptrdiff_t stride_src,
                                  const int16_t *sao_offset_val_u, int sao_left_class_u,
                                  const int16_t *sao_offset_val_v, int sao_left_class_v,
                                  int width, int height);
void ff_hevc_sao_band_c_32_neon_10(uint8_t *_dst, const uint8_t *_src,
                                  ptrdiff_t stride_dst, ptrdiff_t stride_src,
                                  const int16_t *sao_offset_val_u, int sao_left_class_u,
                                  const int16_t *sao_offset_val_v, int sao_left_class_v,
                                  int width, int height);
#endif

void ff_hevc_sao_band_8_neon_8(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, ptrdiff_t stride_src,
                                int16_t *sao_offset_val, int sao_left_class, int width, int height);
void ff_hevc_sao_band_16_neon_8(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, ptrdiff_t stride_src,
                                int16_t *sao_offset_val, int sao_left_class, int width, int height);
void ff_hevc_sao_band_32_neon_8(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, ptrdiff_t stride_src,
                                int16_t *sao_offset_val, int sao_left_class, int width, int height);
void ff_hevc_sao_band_64_neon_8(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, ptrdiff_t stride_src,
                                int16_t *sao_offset_val, int sao_left_class, int width, int height);

void ff_hevc_sao_band_8_neon_10(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, ptrdiff_t stride_src,
                                int16_t *sao_offset_val, int sao_left_class, int width, int height);
void ff_hevc_sao_band_16_neon_10(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, ptrdiff_t stride_src,
                                int16_t *sao_offset_val, int sao_left_class, int width, int height);
void ff_hevc_sao_band_32_neon_10(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, ptrdiff_t stride_src,
                                int16_t *sao_offset_val, int sao_left_class, int width, int height);
void ff_hevc_sao_band_64_neon_10(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, ptrdiff_t stride_src,
                                int16_t *sao_offset_val, int sao_left_class, int width, int height);


#define PUT_PIXELS(name) \
    void name(int16_t *dst, uint8_t *src, \
                                ptrdiff_t srcstride, int height, \
                                intptr_t mx, intptr_t my, int width)
PUT_PIXELS(ff_hevc_put_pixels_w2_neon_8);
PUT_PIXELS(ff_hevc_put_pixels_w4_neon_8);
PUT_PIXELS(ff_hevc_put_pixels_w6_neon_8);
PUT_PIXELS(ff_hevc_put_pixels_w8_neon_8);
PUT_PIXELS(ff_hevc_put_pixels_w12_neon_8);
PUT_PIXELS(ff_hevc_put_pixels_w16_neon_8);
PUT_PIXELS(ff_hevc_put_pixels_w24_neon_8);
PUT_PIXELS(ff_hevc_put_pixels_w32_neon_8);
PUT_PIXELS(ff_hevc_put_pixels_w48_neon_8);
PUT_PIXELS(ff_hevc_put_pixels_w64_neon_8);
#undef PUT_PIXELS
void ff_hevc_put_epel_h_neon_8(int16_t *dst, uint8_t *src,
                                ptrdiff_t srcstride, int height,
                                intptr_t mx, intptr_t my, int width);
void ff_hevc_put_epel_v_neon_8(int16_t *dst, uint8_t *src,
                                ptrdiff_t srcstride, int height,
                                intptr_t mx, intptr_t my, int width);
void ff_hevc_put_epel_hv_neon_8(int16_t *dst, uint8_t *src,
                                ptrdiff_t srcstride, int height,
                                intptr_t mx, intptr_t my, int width);

static void (*put_hevc_qpel_neon[4][4])(int16_t *dst, ptrdiff_t dststride, uint8_t *src, ptrdiff_t srcstride,
                                   int height, int width);
static void (*put_hevc_qpel_uw_neon[4][4])(uint8_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                   int width, int height, int16_t* src2, ptrdiff_t src2stride);
void ff_hevc_put_qpel_neon_wrapper(int16_t *dst, uint8_t *src, ptrdiff_t srcstride,
                                   int height, intptr_t mx, intptr_t my, int width);
void ff_hevc_put_qpel_uni_neon_wrapper(uint8_t *dst, ptrdiff_t dststride, uint8_t *src, ptrdiff_t srcstride,
                                   int height, intptr_t mx, intptr_t my, int width);
void ff_hevc_put_qpel_bi_neon_wrapper(uint8_t *dst, ptrdiff_t dststride, uint8_t *src, ptrdiff_t srcstride,
                                       int16_t *src2,
                                       int height, intptr_t mx, intptr_t my, int width);
#define QPEL_FUNC(name) \
    void name(int16_t *dst, ptrdiff_t dststride, uint8_t *src, ptrdiff_t srcstride, \
                                   int height, int width)

QPEL_FUNC(ff_hevc_put_qpel_v1_neon_8);
QPEL_FUNC(ff_hevc_put_qpel_v2_neon_8);
QPEL_FUNC(ff_hevc_put_qpel_v3_neon_8);
QPEL_FUNC(ff_hevc_put_qpel_h1_neon_8);
QPEL_FUNC(ff_hevc_put_qpel_h2_neon_8);
QPEL_FUNC(ff_hevc_put_qpel_h3_neon_8);
QPEL_FUNC(ff_hevc_put_qpel_h1v1_neon_8);
QPEL_FUNC(ff_hevc_put_qpel_h1v2_neon_8);
QPEL_FUNC(ff_hevc_put_qpel_h1v3_neon_8);
QPEL_FUNC(ff_hevc_put_qpel_h2v1_neon_8);
QPEL_FUNC(ff_hevc_put_qpel_h2v2_neon_8);
QPEL_FUNC(ff_hevc_put_qpel_h2v3_neon_8);
QPEL_FUNC(ff_hevc_put_qpel_h3v1_neon_8);
QPEL_FUNC(ff_hevc_put_qpel_h3v2_neon_8);
QPEL_FUNC(ff_hevc_put_qpel_h3v3_neon_8);
#undef QPEL_FUNC

#define QPEL_FUNC_UW_PIX(name) \
    void name(uint8_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride, \
                                   int height, intptr_t mx, intptr_t my, int width);
QPEL_FUNC_UW_PIX(ff_hevc_put_qpel_uw_pixels_w4_neon_8);
QPEL_FUNC_UW_PIX(ff_hevc_put_qpel_uw_pixels_w8_neon_8);
QPEL_FUNC_UW_PIX(ff_hevc_put_qpel_uw_pixels_w16_neon_8);
QPEL_FUNC_UW_PIX(ff_hevc_put_qpel_uw_pixels_w24_neon_8);
QPEL_FUNC_UW_PIX(ff_hevc_put_qpel_uw_pixels_w32_neon_8);
QPEL_FUNC_UW_PIX(ff_hevc_put_qpel_uw_pixels_w48_neon_8);
QPEL_FUNC_UW_PIX(ff_hevc_put_qpel_uw_pixels_w64_neon_8);
#undef QPEL_FUNC_UW_PIX

#define QPEL_FUNC_UW(name) \
    void name(uint8_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride, \
                                   int width, int height, int16_t* src2, ptrdiff_t src2stride);
QPEL_FUNC_UW(ff_hevc_put_qpel_uw_pixels_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel_uw_v1_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel_uw_v2_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel_uw_v3_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel_uw_h1_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel_uw_h2_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel_uw_h3_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel_uw_h1v1_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel_uw_h1v2_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel_uw_h1v3_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel_uw_h2v1_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel_uw_h2v2_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel_uw_h2v3_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel_uw_h3v1_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel_uw_h3v2_neon_8);
QPEL_FUNC_UW(ff_hevc_put_qpel_uw_h3v3_neon_8);
#undef QPEL_FUNC_UW

void ff_hevc_put_qpel_neon_wrapper(int16_t *dst, uint8_t *src, ptrdiff_t srcstride,
                                   int height, intptr_t mx, intptr_t my, int width) {

    put_hevc_qpel_neon[my][mx](dst, MAX_PB_SIZE, src, srcstride, height, width);
}

void ff_hevc_put_qpel_uni_neon_wrapper(uint8_t *dst, ptrdiff_t dststride, uint8_t *src, ptrdiff_t srcstride,
                                   int height, intptr_t mx, intptr_t my, int width) {

    put_hevc_qpel_uw_neon[my][mx](dst, dststride, src, srcstride, width, height, NULL, 0);
}

void ff_hevc_put_qpel_bi_neon_wrapper(uint8_t *dst, ptrdiff_t dststride, uint8_t *src, ptrdiff_t srcstride,
                                       int16_t *src2,
                                       int height, intptr_t mx, intptr_t my, int width) {
    put_hevc_qpel_uw_neon[my][mx](dst, dststride, src, srcstride, width, height, src2, MAX_PB_SIZE);
}

void ff_hevc_deblocking_boundary_strengths_neon(int pus, int dup, int in_inc, int out_inc,
                                                int *curr_rpl0, int *curr_rpl1, int *neigh_rpl0, int *neigh_rpl1,
                                                MvField *curr, MvField *neigh, uint8_t *bs);


static void ff_hevc_sao_edge_48_neon_8(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, int16_t *_sao_offset_val, int eo, int width, int height)
{
    ff_hevc_sao_edge_32_neon_8(_dst, _src, stride_dst, _sao_offset_val, eo, 32, height);
    ff_hevc_sao_edge_16_neon_8(_dst + 32, _src + 32, stride_dst, _sao_offset_val, eo, 16, height);
}
static void ff_hevc_sao_edge_48_neon_10(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, int16_t *_sao_offset_val, int eo, int width, int height)
{
    ff_hevc_sao_edge_32_neon_10(_dst, _src, stride_dst, _sao_offset_val, eo, 32, height);
    ff_hevc_sao_edge_16_neon_10(_dst + 64, _src + 64, stride_dst, _sao_offset_val, eo, 16, height);
}

static void ff_hevc_sao_band_48_neon_8(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, ptrdiff_t stride_src,
                                int16_t *sao_offset_val, int sao_left_class, int width, int height)
{
    ff_hevc_sao_band_32_neon_8(_dst, _src, stride_dst, stride_src, sao_offset_val, sao_left_class, 32, height);
    ff_hevc_sao_band_16_neon_8(_dst + 32, _src + 32, stride_dst, stride_src, sao_offset_val, sao_left_class, 16, height);
}
static void ff_hevc_sao_band_48_neon_10(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, ptrdiff_t stride_src,
                                int16_t *sao_offset_val, int sao_left_class, int width, int height)
{
    ff_hevc_sao_band_32_neon_10(_dst, _src, stride_dst, stride_src, sao_offset_val, sao_left_class, 32, height);
    ff_hevc_sao_band_16_neon_10(_dst + 64, _src + 64, stride_dst, stride_src, sao_offset_val, sao_left_class, 16, height);
}

#if SAO_FILTER_N == 6
static void ff_hevc_sao_edge_24_neon_8(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, int16_t *_sao_offset_val, int eo, int width, int height)
{
    ff_hevc_sao_edge_16_neon_8(_dst, _src, stride_dst, _sao_offset_val, eo, 16, height);
    ff_hevc_sao_edge_8_neon_8(_dst + 16, _src + 16, stride_dst, _sao_offset_val, eo, 8, height);
}
static void ff_hevc_sao_edge_24_neon_10(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, int16_t *_sao_offset_val, int eo, int width, int height)
{
    ff_hevc_sao_edge_16_neon_10(_dst, _src, stride_dst, _sao_offset_val, eo, 16, height);
    ff_hevc_sao_edge_8_neon_10(_dst + 32, _src + 32, stride_dst, _sao_offset_val, eo, 8, height);
}

static void ff_hevc_sao_band_24_neon_8(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, ptrdiff_t stride_src,
                                int16_t *sao_offset_val, int sao_left_class, int width, int height)
{
    ff_hevc_sao_band_16_neon_8(_dst, _src, stride_dst, stride_src, sao_offset_val, sao_left_class, 16, height);
    ff_hevc_sao_band_8_neon_8(_dst + 16, _src + 16, stride_dst, stride_src, sao_offset_val, sao_left_class, 8, height);
}
static void ff_hevc_sao_band_24_neon_10(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, ptrdiff_t stride_src,
                                int16_t *sao_offset_val, int sao_left_class, int width, int height)
{
    ff_hevc_sao_band_16_neon_10(_dst, _src, stride_dst, stride_src, sao_offset_val, sao_left_class, 16, height);
    ff_hevc_sao_band_8_neon_10(_dst + 32, _src + 32, stride_dst, stride_src, sao_offset_val, sao_left_class, 8, height);
}

#if RPI_HEVC_SAND
static void ff_hevc_sao_edge_c_24_neon_8(uint8_t *_dst, const uint8_t *_src, ptrdiff_t stride_dst,
                                  const int16_t *_sao_offset_val_u, const int16_t *_sao_offset_val_v,
                                  int eo, int width, int height)
{
    ff_hevc_sao_edge_c_16_neon_8(_dst, _src, stride_dst, _sao_offset_val_u, _sao_offset_val_v, eo, 16, height);
    ff_hevc_sao_edge_c_8_neon_8(_dst + 32, _src + 32, stride_dst, _sao_offset_val_u, _sao_offset_val_v, eo, 8, height);
}
static void ff_hevc_sao_edge_c_24_neon_10(uint8_t *_dst, const uint8_t *_src, ptrdiff_t stride_dst,
                                  const int16_t *_sao_offset_val_u, const int16_t *_sao_offset_val_v,
                                  int eo, int width, int height)
{
    ff_hevc_sao_edge_c_16_neon_10(_dst, _src, stride_dst, _sao_offset_val_u, _sao_offset_val_v, eo, 16, height);
    ff_hevc_sao_edge_c_8_neon_10(_dst + 64, _src + 64, stride_dst, _sao_offset_val_u, _sao_offset_val_v, eo, 8, height);
}

static void ff_hevc_sao_band_c_24_neon_8(uint8_t *_dst, const uint8_t *_src,
                                  ptrdiff_t stride_dst, ptrdiff_t stride_src,
                                  const int16_t *sao_offset_val_u, int sao_left_class_u,
                                  const int16_t *sao_offset_val_v, int sao_left_class_v,
                                  int width, int height)
{
    ff_hevc_sao_band_c_16_neon_8(_dst, _src, stride_dst, stride_src,
                                sao_offset_val_u, sao_left_class_u, sao_offset_val_v, sao_left_class_v, 16, height);
    ff_hevc_sao_band_c_8_neon_8(_dst + 32, _src + 32, stride_dst, stride_src,
                                sao_offset_val_u, sao_left_class_u, sao_offset_val_v, sao_left_class_v, 8, height);
}
static void ff_hevc_sao_band_c_24_neon_10(uint8_t *_dst, const uint8_t *_src,
                                  ptrdiff_t stride_dst, ptrdiff_t stride_src,
                                  const int16_t *sao_offset_val_u, int sao_left_class_u,
                                  const int16_t *sao_offset_val_v, int sao_left_class_v,
                                  int width, int height)
{
    ff_hevc_sao_band_c_16_neon_10(_dst, _src, stride_dst, stride_src,
                                sao_offset_val_u, sao_left_class_u, sao_offset_val_v, sao_left_class_v, 16, height);
    ff_hevc_sao_band_c_8_neon_10(_dst + 64, _src + 64, stride_dst, stride_src,
                                sao_offset_val_u, sao_left_class_u, sao_offset_val_v, sao_left_class_v, 8, height);
}
#endif
#endif



#if (2*MAX_PB_SIZE + FF_INPUT_BUFFER_PADDING_SIZE) != 160
#error SAO edge src stride not 160 - value used in .S
#endif

av_cold void ff_hevcdsp_init_neon(HEVCDSPContext *c, const int bit_depth)
{
    if (bit_depth == 8) {
        int x;
        c->hevc_v_loop_filter_luma     = ff_hevc_v_loop_filter_luma_neon;
        c->hevc_v_loop_filter_luma_c   = ff_hevc_v_loop_filter_luma_neon;
        c->hevc_h_loop_filter_luma     = ff_hevc_h_loop_filter_luma_neon;
        c->hevc_h_loop_filter_luma_c   = ff_hevc_h_loop_filter_luma_neon;
        c->hevc_v_loop_filter_chroma   = ff_hevc_v_loop_filter_chroma_neon;
        c->hevc_v_loop_filter_chroma_c = ff_hevc_v_loop_filter_chroma_neon;
        c->hevc_h_loop_filter_chroma   = ff_hevc_h_loop_filter_chroma_neon;
        c->hevc_h_loop_filter_chroma_c = ff_hevc_h_loop_filter_chroma_neon;
#ifdef RPI
        c->hevc_v_loop_filter_luma2    = ff_hevc_v_loop_filter_luma2_neon_8;
        c->hevc_h_loop_filter_uv       = ff_hevc_h_loop_filter_uv_neon_8;
        c->hevc_v_loop_filter_uv2      = ff_hevc_v_loop_filter_uv2_neon_8;
#endif
        c->idct[0]                     = ff_hevc_transform_4x4_neon_8;
        c->idct[1]                     = ff_hevc_transform_8x8_neon_8;
        c->idct_dc[0]                  = ff_hevc_idct_4x4_dc_neon_8;
        c->idct_dc[1]                  = ff_hevc_idct_8x8_dc_neon_8;
        c->idct_dc[2]                  = ff_hevc_idct_16x16_dc_neon_8;
        c->idct_dc[3]                  = ff_hevc_idct_32x32_dc_neon_8;
        c->add_residual[0]             = ff_hevc_add_residual_4x4_neon_8;
        c->add_residual[1]             = ff_hevc_add_residual_8x8_neon_8;
        c->add_residual[2]             = ff_hevc_add_residual_16x16_neon_8;
        c->add_residual[3]             = ff_hevc_add_residual_32x32_neon_8;
#if RPI_HEVC_SAND
        c->add_residual_u[0]           = ff_hevc_add_residual_4x4_u_neon_8;
        c->add_residual_u[1]           = ff_hevc_add_residual_8x8_u_neon_8;
        c->add_residual_u[2]           = ff_hevc_add_residual_16x16_u_neon_8;
        c->add_residual_v[0]           = ff_hevc_add_residual_4x4_v_neon_8;
        c->add_residual_v[1]           = ff_hevc_add_residual_8x8_v_neon_8;
        c->add_residual_v[2]           = ff_hevc_add_residual_16x16_v_neon_8;
        c->add_residual_c[0]           = ff_hevc_add_residual_4x4_c_neon_8;
        c->add_residual_c[1]           = ff_hevc_add_residual_8x8_c_neon_8;
        c->add_residual_c[2]           = ff_hevc_add_residual_16x16_c_neon_8;
#endif
        c->transform_4x4_luma          = ff_hevc_transform_luma_4x4_neon_8;
        c->sao_band_filter[0]          = ff_hevc_sao_band_8_neon_8;
        c->sao_band_filter[1]          = ff_hevc_sao_band_16_neon_8;
        c->sao_band_filter[2]          = ff_hevc_sao_band_32_neon_8;
        c->sao_band_filter[3]          = ff_hevc_sao_band_48_neon_8;
        c->sao_band_filter[4]          = ff_hevc_sao_band_64_neon_8;
        c->sao_edge_filter[0]          = ff_hevc_sao_edge_8_neon_8;
        c->sao_edge_filter[1]          = ff_hevc_sao_edge_16_neon_8;
        c->sao_edge_filter[2]          = ff_hevc_sao_edge_32_neon_8;
        c->sao_edge_filter[3]          = ff_hevc_sao_edge_48_neon_8;
        c->sao_edge_filter[4]          = ff_hevc_sao_edge_64_neon_8;
#if SAO_FILTER_N == 6
        c->sao_band_filter[5]          = ff_hevc_sao_band_24_neon_8;
        c->sao_edge_filter[5]          = ff_hevc_sao_edge_24_neon_8;
#endif
#if RPI_HEVC_SAND
        c->sao_band_filter_c[0]        = ff_hevc_sao_band_c_8_neon_8;
        c->sao_band_filter_c[1]        = ff_hevc_sao_band_c_16_neon_8;
        c->sao_band_filter_c[2]        = ff_hevc_sao_band_c_32_neon_8;

        c->sao_edge_filter_c[0]        = ff_hevc_sao_edge_c_8_neon_8;
        c->sao_edge_filter_c[1]        = ff_hevc_sao_edge_c_16_neon_8;
        c->sao_edge_filter_c[2]        = ff_hevc_sao_edge_c_32_neon_8;

#if SAO_FILTER_N == 6
        c->sao_band_filter_c[5]        = ff_hevc_sao_band_c_24_neon_8;
        c->sao_edge_filter_c[5]        = ff_hevc_sao_edge_c_24_neon_8;
#endif
#endif
        put_hevc_qpel_neon[1][0]       = ff_hevc_put_qpel_v1_neon_8;
        put_hevc_qpel_neon[2][0]       = ff_hevc_put_qpel_v2_neon_8;
        put_hevc_qpel_neon[3][0]       = ff_hevc_put_qpel_v3_neon_8;
        put_hevc_qpel_neon[0][1]       = ff_hevc_put_qpel_h1_neon_8;
        put_hevc_qpel_neon[0][2]       = ff_hevc_put_qpel_h2_neon_8;
        put_hevc_qpel_neon[0][3]       = ff_hevc_put_qpel_h3_neon_8;
        put_hevc_qpel_neon[1][1]       = ff_hevc_put_qpel_h1v1_neon_8;
        put_hevc_qpel_neon[1][2]       = ff_hevc_put_qpel_h2v1_neon_8;
        put_hevc_qpel_neon[1][3]       = ff_hevc_put_qpel_h3v1_neon_8;
        put_hevc_qpel_neon[2][1]       = ff_hevc_put_qpel_h1v2_neon_8;
        put_hevc_qpel_neon[2][2]       = ff_hevc_put_qpel_h2v2_neon_8;
        put_hevc_qpel_neon[2][3]       = ff_hevc_put_qpel_h3v2_neon_8;
        put_hevc_qpel_neon[3][1]       = ff_hevc_put_qpel_h1v3_neon_8;
        put_hevc_qpel_neon[3][2]       = ff_hevc_put_qpel_h2v3_neon_8;
        put_hevc_qpel_neon[3][3]       = ff_hevc_put_qpel_h3v3_neon_8;
        put_hevc_qpel_uw_neon[1][0]      = ff_hevc_put_qpel_uw_v1_neon_8;
        put_hevc_qpel_uw_neon[2][0]      = ff_hevc_put_qpel_uw_v2_neon_8;
        put_hevc_qpel_uw_neon[3][0]      = ff_hevc_put_qpel_uw_v3_neon_8;
        put_hevc_qpel_uw_neon[0][1]      = ff_hevc_put_qpel_uw_h1_neon_8;
        put_hevc_qpel_uw_neon[0][2]      = ff_hevc_put_qpel_uw_h2_neon_8;
        put_hevc_qpel_uw_neon[0][3]      = ff_hevc_put_qpel_uw_h3_neon_8;
        put_hevc_qpel_uw_neon[1][1]      = ff_hevc_put_qpel_uw_h1v1_neon_8;
        put_hevc_qpel_uw_neon[1][2]      = ff_hevc_put_qpel_uw_h2v1_neon_8;
        put_hevc_qpel_uw_neon[1][3]      = ff_hevc_put_qpel_uw_h3v1_neon_8;
        put_hevc_qpel_uw_neon[2][1]      = ff_hevc_put_qpel_uw_h1v2_neon_8;
        put_hevc_qpel_uw_neon[2][2]      = ff_hevc_put_qpel_uw_h2v2_neon_8;
        put_hevc_qpel_uw_neon[2][3]      = ff_hevc_put_qpel_uw_h3v2_neon_8;
        put_hevc_qpel_uw_neon[3][1]      = ff_hevc_put_qpel_uw_h1v3_neon_8;
        put_hevc_qpel_uw_neon[3][2]      = ff_hevc_put_qpel_uw_h2v3_neon_8;
        put_hevc_qpel_uw_neon[3][3]      = ff_hevc_put_qpel_uw_h3v3_neon_8;
        for (x = 0; x < 10; x++) {
            c->put_hevc_qpel[x][1][0]         = ff_hevc_put_qpel_neon_wrapper;
            c->put_hevc_qpel[x][0][1]         = ff_hevc_put_qpel_neon_wrapper;
            c->put_hevc_qpel[x][1][1]         = ff_hevc_put_qpel_neon_wrapper;
            c->put_hevc_qpel_uni[x][1][0]     = ff_hevc_put_qpel_uni_neon_wrapper;
            c->put_hevc_qpel_uni[x][0][1]     = ff_hevc_put_qpel_uni_neon_wrapper;
            c->put_hevc_qpel_uni[x][1][1]     = ff_hevc_put_qpel_uni_neon_wrapper;
            c->put_hevc_qpel_bi[x][1][0]      = ff_hevc_put_qpel_bi_neon_wrapper;
            c->put_hevc_qpel_bi[x][0][1]      = ff_hevc_put_qpel_bi_neon_wrapper;
            c->put_hevc_qpel_bi[x][1][1]      = ff_hevc_put_qpel_bi_neon_wrapper;
            c->put_hevc_epel[x][1][0]         = ff_hevc_put_epel_v_neon_8;
            c->put_hevc_epel[x][0][1]         = ff_hevc_put_epel_h_neon_8;
            c->put_hevc_epel[x][1][1]         = ff_hevc_put_epel_hv_neon_8;
        }
        c->put_hevc_epel[0][0][0]  = ff_hevc_put_pixels_w2_neon_8;
        c->put_hevc_epel[1][0][0]  = ff_hevc_put_pixels_w4_neon_8;
        c->put_hevc_epel[2][0][0]  = ff_hevc_put_pixels_w6_neon_8;
        c->put_hevc_epel[3][0][0]  = ff_hevc_put_pixels_w8_neon_8;
        c->put_hevc_epel[4][0][0]  = ff_hevc_put_pixels_w12_neon_8;
        c->put_hevc_epel[5][0][0]  = ff_hevc_put_pixels_w16_neon_8;
        c->put_hevc_epel[6][0][0]  = ff_hevc_put_pixels_w24_neon_8;
        c->put_hevc_epel[7][0][0]  = ff_hevc_put_pixels_w32_neon_8;
        c->put_hevc_epel[8][0][0]  = ff_hevc_put_pixels_w48_neon_8;
        c->put_hevc_epel[9][0][0]  = ff_hevc_put_pixels_w64_neon_8;

        c->put_hevc_qpel[0][0][0]  = ff_hevc_put_pixels_w2_neon_8;
        c->put_hevc_qpel[1][0][0]  = ff_hevc_put_pixels_w4_neon_8;
        c->put_hevc_qpel[2][0][0]  = ff_hevc_put_pixels_w6_neon_8;
        c->put_hevc_qpel[3][0][0]  = ff_hevc_put_pixels_w8_neon_8;
        c->put_hevc_qpel[4][0][0]  = ff_hevc_put_pixels_w12_neon_8;
        c->put_hevc_qpel[5][0][0]  = ff_hevc_put_pixels_w16_neon_8;
        c->put_hevc_qpel[6][0][0]  = ff_hevc_put_pixels_w24_neon_8;
        c->put_hevc_qpel[7][0][0]  = ff_hevc_put_pixels_w32_neon_8;
        c->put_hevc_qpel[8][0][0]  = ff_hevc_put_pixels_w48_neon_8;
        c->put_hevc_qpel[9][0][0]  = ff_hevc_put_pixels_w64_neon_8;

        c->put_hevc_qpel_uni[1][0][0]  = ff_hevc_put_qpel_uw_pixels_w4_neon_8;
        c->put_hevc_qpel_uni[3][0][0]  = ff_hevc_put_qpel_uw_pixels_w8_neon_8;
        c->put_hevc_qpel_uni[5][0][0]  = ff_hevc_put_qpel_uw_pixels_w16_neon_8;
        c->put_hevc_qpel_uni[6][0][0]  = ff_hevc_put_qpel_uw_pixels_w24_neon_8;
        c->put_hevc_qpel_uni[7][0][0]  = ff_hevc_put_qpel_uw_pixels_w32_neon_8;
        c->put_hevc_qpel_uni[8][0][0]  = ff_hevc_put_qpel_uw_pixels_w48_neon_8;
        c->put_hevc_qpel_uni[9][0][0]  = ff_hevc_put_qpel_uw_pixels_w64_neon_8;
    }
    else if (bit_depth == 10) {
        c->hevc_v_loop_filter_luma     = ff_hevc_v_loop_filter_luma_neon_10;
        c->hevc_v_loop_filter_luma_c   = ff_hevc_v_loop_filter_luma_neon_10;
        c->hevc_h_loop_filter_luma     = ff_hevc_h_loop_filter_luma_neon_10;
        c->hevc_h_loop_filter_luma_c   = ff_hevc_h_loop_filter_luma_neon_10;
        c->hevc_v_loop_filter_chroma   = ff_hevc_v_loop_filter_chroma_neon_10;
        c->hevc_v_loop_filter_chroma_c = ff_hevc_v_loop_filter_chroma_neon_10;
        c->hevc_h_loop_filter_chroma   = ff_hevc_h_loop_filter_chroma_neon_10;
        c->hevc_h_loop_filter_chroma_c = ff_hevc_h_loop_filter_chroma_neon_10;
#ifdef RPI
        c->hevc_v_loop_filter_luma2    = ff_hevc_v_loop_filter_luma2_neon_10;
        c->hevc_h_loop_filter_uv       = ff_hevc_h_loop_filter_uv_neon_10;
        c->hevc_v_loop_filter_uv2      = ff_hevc_v_loop_filter_uv2_neon_10;
#endif
        c->idct[0]                     = ff_hevc_transform_4x4_neon_10;
        c->idct[1]                     = ff_hevc_transform_8x8_neon_10;
        c->idct_dc[0]                  = ff_hevc_idct_4x4_dc_neon_10;
        c->idct_dc[1]                  = ff_hevc_idct_8x8_dc_neon_10;
        c->idct_dc[2]                  = ff_hevc_idct_16x16_dc_neon_10;
        c->idct_dc[3]                  = ff_hevc_idct_32x32_dc_neon_10;
        c->add_residual[0]             = ff_hevc_add_residual_4x4_neon_10;
        c->add_residual[1]             = ff_hevc_add_residual_8x8_neon_10;
        c->add_residual[2]             = ff_hevc_add_residual_16x16_neon_10;
        c->add_residual[3]             = ff_hevc_add_residual_32x32_neon_10;
#if RPI_HEVC_SAND
        c->add_residual_u[0]           = ff_hevc_add_residual_4x4_u_neon_10;
        c->add_residual_u[1]           = ff_hevc_add_residual_8x8_u_neon_10;
        c->add_residual_u[2]           = ff_hevc_add_residual_16x16_u_neon_10;
        c->add_residual_v[0]           = ff_hevc_add_residual_4x4_v_neon_10;
        c->add_residual_v[1]           = ff_hevc_add_residual_8x8_v_neon_10;
        c->add_residual_v[2]           = ff_hevc_add_residual_16x16_v_neon_10;
        c->add_residual_c[0]           = ff_hevc_add_residual_4x4_c_neon_10;
        c->add_residual_c[1]           = ff_hevc_add_residual_8x8_c_neon_10;
        c->add_residual_c[2]           = ff_hevc_add_residual_16x16_c_neon_10;
#endif
        c->transform_4x4_luma          = ff_hevc_transform_luma_4x4_neon_10;
        c->sao_band_filter[0]          = ff_hevc_sao_band_8_neon_10;
        c->sao_band_filter[1]          = ff_hevc_sao_band_16_neon_10;
        c->sao_band_filter[2]          = ff_hevc_sao_band_32_neon_10;
        c->sao_band_filter[3]          = ff_hevc_sao_band_48_neon_10;
        c->sao_band_filter[4]          = ff_hevc_sao_band_64_neon_10;

        c->sao_edge_filter[0]          = ff_hevc_sao_edge_8_neon_10;
        c->sao_edge_filter[1]          = ff_hevc_sao_edge_16_neon_10;
        c->sao_edge_filter[2]          = ff_hevc_sao_edge_32_neon_10;
        c->sao_edge_filter[3]          = ff_hevc_sao_edge_48_neon_10;
        c->sao_edge_filter[4]          = ff_hevc_sao_edge_64_neon_10;
#if SAO_FILTER_N == 6
        c->sao_band_filter[5]          = ff_hevc_sao_band_24_neon_10;
        c->sao_edge_filter[5]          = ff_hevc_sao_edge_24_neon_10;
#endif
#if RPI_HEVC_SAND
        c->sao_band_filter_c[0]        = ff_hevc_sao_band_c_8_neon_10;
        c->sao_band_filter_c[1]        = ff_hevc_sao_band_c_16_neon_10;
        c->sao_band_filter_c[2]        = ff_hevc_sao_band_c_32_neon_10;

        c->sao_edge_filter_c[0]        = ff_hevc_sao_edge_c_8_neon_10;
        c->sao_edge_filter_c[1]        = ff_hevc_sao_edge_c_16_neon_10;
        c->sao_edge_filter_c[2]        = ff_hevc_sao_edge_c_32_neon_10;

#if SAO_FILTER_N == 6
        c->sao_band_filter_c[5]        = ff_hevc_sao_band_c_24_neon_10;
        c->sao_edge_filter_c[5]        = ff_hevc_sao_edge_c_24_neon_10;
#endif
#endif
    }

    assert(offsetof(MvField, mv) == 0);
    assert(offsetof(MvField, ref_idx) == 8);
    assert(offsetof(MvField, pred_flag) == 10);
    c->hevc_deblocking_boundary_strengths = ff_hevc_deblocking_boundary_strengths_neon;
}
