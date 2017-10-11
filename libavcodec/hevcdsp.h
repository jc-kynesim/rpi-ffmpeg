/*
 * HEVC video decoder
 *
 * Copyright (C) 2012 - 2013 Guillaume Martres
 * Copyright (C) 2013 - 2014 Pierre-Edouard Lepere
 *
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

#ifndef AVCODEC_HEVCDSP_H
#define AVCODEC_HEVCDSP_H

#include "get_bits.h"

#define MAX_PB_SIZE 64

typedef struct SAOParams {
    int offset_abs[3][4];   ///< sao_offset_abs
    int offset_sign[3][4];  ///< sao_offset_sign

    uint8_t band_position[3];   ///< sao_band_position

    int eo_class[3];        ///< sao_eo_class

    int16_t offset_val[3][5];   ///<SaoOffsetVal

    uint8_t type_idx[3];    ///< sao_type_idx
} SAOParams;

typedef struct Mv {
    int16_t x;  ///< horizontal component of motion vector
    int16_t y;  ///< vertical component of motion vector
} Mv;

typedef struct MvField {
    DECLARE_ALIGNED(4, Mv, mv)[2];
    int8_t ref_idx[2];
    int8_t pred_flag;
} MvField;

#ifdef RPI
#define SAO_FILTER_N 6
#define RPI_HEVC_SAND 1
#else
#define SAO_FILTER_N 5
#define RPI_HEVC_SAND 0
#endif


typedef struct HEVCDSPContext {
    void (*put_pcm)(uint8_t *_dst, ptrdiff_t _stride, int width, int height,
                    struct GetBitContext *gb, int pcm_bit_depth);

    // add_residual was transform_add - import 3.3 names
    void (*transform_add[4])(uint8_t *dst, int16_t *res, ptrdiff_t stride);
    void (*add_residual_dc[4])(uint8_t *dst, ptrdiff_t stride, int dc);
#if RPI_HEVC_SAND
    void (*add_residual_u[4])(uint8_t *dst, const int16_t *res, ptrdiff_t stride, int dc_v);
    void (*add_residual_v[4])(uint8_t *dst, const int16_t *res, ptrdiff_t stride, int dc_u);

    void (*add_residual_c[4])(uint8_t *dst, const int16_t *res, ptrdiff_t stride);
    void (*add_residual_dc_c[4])(uint8_t *dst, ptrdiff_t stride, int32_t dc_uv);
    void (*put_pcm_c)(uint8_t *_dst, ptrdiff_t _stride, int width, int height,
                    struct GetBitContext *gb, int pcm_bit_depth);
#endif

    void (*transform_skip)(int16_t *coeffs, int16_t log2_size);

    void (*transform_rdpcm)(int16_t *coeffs, int16_t log2_size, int mode);

    void (*idct_4x4_luma)(int16_t *coeffs);

    void (*idct[4])(int16_t *coeffs, int col_limit);

    void (*idct_dc[4])(int16_t *coeffs);

    void (*sao_band_filter[SAO_FILTER_N])(uint8_t *_dst, uint8_t *_src, ptrdiff_t _stride_dst, ptrdiff_t _stride_src,
                               int16_t *sao_offset_val, int sao_left_class, int width, int height);
#if RPI_HEVC_SAND
    void (*sao_band_filter_c[SAO_FILTER_N])(uint8_t *_dst, const uint8_t *_src, ptrdiff_t _stride_dst, ptrdiff_t _stride_src,
                               const int16_t *sao_offset_val_u, int sao_left_class_u,
                               const int16_t *sao_offset_val_v, int sao_left_class_v,
                               int width, int height);
#endif

    /* implicit stride_src parameter has value of 2 * MAX_PB_SIZE + AV_INPUT_BUFFER_PADDING_SIZE */
    void (*sao_edge_filter[SAO_FILTER_N])(uint8_t *_dst /* align 16 */, uint8_t *_src /* align 32 */, ptrdiff_t stride_dst,
                               int16_t *sao_offset_val, int sao_eo_class, int width, int height);
#if RPI_HEVC_SAND
    void (*sao_edge_filter_c[SAO_FILTER_N])(uint8_t *_dst /* align 16 */, const uint8_t *_src /* align 32 */, ptrdiff_t stride_dst,
                               const int16_t *sao_offset_val_u, const int16_t *sao_offset_val_v, int sao_eo_class, int width, int height);
#endif

    void (*sao_edge_restore[2])(uint8_t *_dst, uint8_t *_src, ptrdiff_t _stride_dst, ptrdiff_t _stride_src,
                                struct SAOParams *sao, int *borders, int _width, int _height, int c_idx,
                                uint8_t *vert_edge, uint8_t *horiz_edge, uint8_t *diag_edge);
#if RPI_HEVC_SAND
    void (*sao_edge_restore_c[2])(uint8_t *_dst, uint8_t *_src, ptrdiff_t _stride_dst, ptrdiff_t _stride_src,
                                struct SAOParams *sao, int *borders, int _width, int _height, int c_idx,
                                uint8_t *vert_edge, uint8_t *horiz_edge, uint8_t *diag_edge);
#endif

    void (*put_hevc_qpel[10][2][2])(int16_t *dst, uint8_t *src, ptrdiff_t srcstride,
                                    int height, intptr_t mx, intptr_t my, int width);
    void (*put_hevc_qpel_uni[10][2][2])(uint8_t *dst, ptrdiff_t dststride, uint8_t *src, ptrdiff_t srcstride,
                                        int height, intptr_t mx, intptr_t my, int width);
    void (*put_hevc_qpel_uni_w[10][2][2])(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                          int height, int denom, int wx, int ox, intptr_t mx, intptr_t my, int width);

    void (*put_hevc_qpel_bi[10][2][2])(uint8_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                       int16_t *src2,
                                       int height, intptr_t mx, intptr_t my, int width);
    void (*put_hevc_qpel_bi_w[10][2][2])(uint8_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                         int16_t *src2,
                                         int height, int denom, int wx0, int wx1,
                                         int ox0, int ox1, intptr_t mx, intptr_t my, int width);
    void (*put_hevc_epel[10][2][2])(int16_t *dst, uint8_t *src, ptrdiff_t srcstride,
                                    int height, intptr_t mx, intptr_t my, int width);

    void (*put_hevc_epel_uni[10][2][2])(uint8_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                        int height, intptr_t mx, intptr_t my, int width);
    void (*put_hevc_epel_uni_w[10][2][2])(uint8_t *_dst, ptrdiff_t _dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                          int height, int denom, int wx, int ox, intptr_t mx, intptr_t my, int width);
    void (*put_hevc_epel_bi[10][2][2])(uint8_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                       int16_t *src2,
                                       int height, intptr_t mx, intptr_t my, int width);
    void (*put_hevc_epel_bi_w[10][2][2])(uint8_t *dst, ptrdiff_t dststride, uint8_t *_src, ptrdiff_t _srcstride,
                                         int16_t *src2,
                                         int height, int denom, int wx0, int ox0, int wx1,
                                         int ox1, intptr_t mx, intptr_t my, int width);

    void (*hevc_h_loop_filter_luma)(uint8_t *pix, ptrdiff_t stride,
                                    int beta, int32_t *tc,
                                    uint8_t *no_p, uint8_t *no_q);
    void (*hevc_v_loop_filter_luma)(uint8_t *pix, ptrdiff_t stride,
                                    int beta, int32_t *tc,
                                    uint8_t *no_p, uint8_t *no_q);
    void (*hevc_h_loop_filter_chroma)(uint8_t *pix, ptrdiff_t stride,
                                      int32_t *tc, uint8_t *no_p, uint8_t *no_q);
    void (*hevc_v_loop_filter_chroma)(uint8_t *pix, ptrdiff_t stride,
                                      int32_t *tc, uint8_t *no_p, uint8_t *no_q);
    void (*hevc_h_loop_filter_luma_c)(uint8_t *pix, ptrdiff_t stride,
                                      int beta, int32_t *tc,
                                      uint8_t *no_p, uint8_t *no_q);
    void (*hevc_v_loop_filter_luma_c)(uint8_t *pix, ptrdiff_t stride,
                                      int beta, int32_t *tc,
                                      uint8_t *no_p, uint8_t *no_q);
    void (*hevc_h_loop_filter_chroma_c)(uint8_t *pix, ptrdiff_t stride,
                                        int32_t *tc, uint8_t *no_p,
                                        uint8_t *no_q);
    void (*hevc_v_loop_filter_chroma_c)(uint8_t *pix, ptrdiff_t stride,
                                        int32_t *tc, uint8_t *no_p,
                                        uint8_t *no_q);
#ifdef RPI
    void (*hevc_v_loop_filter_luma2)(uint8_t * _pix_r,
                                 unsigned int _stride, unsigned int beta, const int32_t tc[2],
                                 const uint8_t no_p[2], const uint8_t no_q[2],
                                 uint8_t * _pix_l);
    void (*hevc_h_loop_filter_uv)(uint8_t * src, unsigned int stride, uint32_t tc4,
                                 unsigned int no_f);
    void (*hevc_v_loop_filter_uv2)(uint8_t * src_r, unsigned int stride, uint32_t tc4,
                                 uint8_t * src_l,
                                 unsigned int no_f);

#endif

    void (*hevc_deblocking_boundary_strengths)(int pus, int dup, int in_inc, int out_inc,
                                               int *curr_rpl0, int *curr_rpl1, int *neigh_rpl0, int *neigh_rpl1,
                                               MvField *curr, MvField *neigh, uint8_t *bs);
} HEVCDSPContext;

void ff_hevc_dsp_init(HEVCDSPContext *hpc, int bit_depth);

extern const int8_t ff_hevc_epel_filters[7][4];
extern const int8_t ff_hevc_qpel_filters[3][16];

void ff_hevc_dsp_init_x86(HEVCDSPContext *c, const int bit_depth);
void ff_hevcdsp_init_arm(HEVCDSPContext *c, const int bit_depth);
void ff_hevc_dsp_init_mips(HEVCDSPContext *c, const int bit_depth);
#endif /* AVCODEC_HEVCDSP_H */
