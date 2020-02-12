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

#ifndef AVCODEC_RPI_HEVCDSP_H
#define AVCODEC_RPI_HEVCDSP_H

#include "hevc.h"
#include "get_bits.h"

struct HEVCRpiMvField;

#define MAX_PB_SIZE 64

#define RPI_HEVC_SAO_BUF_STRIDE 160


typedef struct RpiSAOParams {
    uint8_t band_position[3];   ///< sao_band_position (Y,U,V)
    uint8_t eo_class[3];        ///< sao_eo_class      (Y,U=V)
    uint8_t type_idx[3];        ///< sao_type_idx      (Y,U=V)

    int16_t offset_val[3][5];   ///<SaoOffsetVal       (Y,U,V)

} RpiSAOParams;


// This controls how many sao dsp functions there are
// N=5 has width = 8, 16, 32, 48, 64
// N=6 adds a function for width=24 (in fn array el 5 so existing code should
// still work)
#define SAO_FILTER_N 6


typedef struct HEVCDSPContext {
    void (*put_pcm)(uint8_t *_dst, ptrdiff_t _stride, int width, int height,
                    struct GetBitContext *gb, int pcm_bit_depth);

    void (*add_residual[4])(uint8_t *dst, int16_t *res, ptrdiff_t stride);
    void (*add_residual_dc[4])(uint8_t *dst, ptrdiff_t stride, int dc);
    void (*add_residual_u[4])(uint8_t *dst, const int16_t *res, ptrdiff_t stride, int dc_v);
    void (*add_residual_v[4])(uint8_t *dst, const int16_t *res, ptrdiff_t stride, int dc_u);

    void (*add_residual_c[4])(uint8_t *dst, const int16_t *res, ptrdiff_t stride);
    void (*add_residual_dc_c[4])(uint8_t *dst, ptrdiff_t stride, int32_t dc_uv);
    void (*put_pcm_c)(uint8_t *_dst, ptrdiff_t _stride, int width, int height,
                    struct GetBitContext *gb, int pcm_bit_depth);

    void (*dequant)(int16_t *coeffs, int16_t log2_size);

    void (*transform_rdpcm)(int16_t *coeffs, int16_t log2_size, int mode);

    void (*transform_4x4_luma)(int16_t *coeffs);

    void (*idct[4])(int16_t *coeffs, int col_limit);

    void (*idct_dc[4])(int16_t *coeffs);

    void (*sao_band_filter[SAO_FILTER_N])(uint8_t *_dst, uint8_t *_src, ptrdiff_t _stride_dst, ptrdiff_t _stride_src,
                               int16_t *sao_offset_val, int sao_left_class, int width, int height);
    void (*sao_band_filter_c[SAO_FILTER_N])(uint8_t *_dst, const uint8_t *_src, ptrdiff_t _stride_dst, ptrdiff_t _stride_src,
                               const int16_t *sao_offset_val_u, int sao_left_class_u,
                               const int16_t *sao_offset_val_v, int sao_left_class_v,
                               int width, int height);

    /* implicit stride_src parameter has value of 2 * MAX_PB_SIZE + AV_INPUT_BUFFER_PADDING_SIZE */
    void (*sao_edge_filter[SAO_FILTER_N])(uint8_t *_dst /* align 16 */, uint8_t *_src /* align 32 */, ptrdiff_t stride_dst,
                               int16_t *sao_offset_val, int sao_eo_class, int width, int height);
    void (*sao_edge_filter_c[SAO_FILTER_N])(uint8_t *_dst /* align 16 */, const uint8_t *_src /* align 32 */, ptrdiff_t stride_dst,
                               const int16_t *sao_offset_val_u, const int16_t *sao_offset_val_v, int sao_eo_class, int width, int height);

    void (*sao_edge_restore[2])(uint8_t *_dst, uint8_t *_src, ptrdiff_t _stride_dst, ptrdiff_t _stride_src,
                                struct RpiSAOParams *sao, int *borders, int _width, int _height, int c_idx,
                                uint8_t *vert_edge, uint8_t *horiz_edge, uint8_t *diag_edge);
    void (*sao_edge_restore_c[2])(uint8_t *_dst, uint8_t *_src, ptrdiff_t _stride_dst, ptrdiff_t _stride_src,
                                struct RpiSAOParams *sao, int *borders, int _width, int _height, int c_idx,
                                uint8_t *vert_edge, uint8_t *horiz_edge, uint8_t *diag_edge);

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
    void (*hevc_h_loop_filter_luma2)(uint8_t * _pix_r,
                                 unsigned int _stride, unsigned int beta, unsigned int tc2, unsigned int no_f);
    void (*hevc_v_loop_filter_luma2)(uint8_t * _pix_r,
                                 unsigned int _stride, unsigned int beta, unsigned int tc2, unsigned int no_f,
                                 uint8_t * _pix_l);
    void (*hevc_h_loop_filter_uv)(uint8_t * src, unsigned int stride, uint32_t tc4,
                                 unsigned int no_f);
    void (*hevc_v_loop_filter_uv2)(uint8_t * src_r, unsigned int stride, uint32_t tc4,
                                 uint8_t * src_l,
                                 unsigned int no_f);

    uint32_t (*hevc_deblocking_boundary_strengths)(int pus, int dup, const struct HEVCRpiMvField *curr, const struct HEVCRpiMvField *neigh,
                                               const int *curr_rpl0, const int *curr_rpl1, const int *neigh_rpl0, const int *neigh_rpl1,
                                               int in_inc0, int inc_inc1);

    void (* cpy_blk)(uint8_t * dst, unsigned int dst_stride, const uint8_t * src, unsigned int src_stride, unsigned int width, unsigned int height);
} HEVCDSPContext;

void ff_hevc_rpi_dsp_init(HEVCDSPContext *hpc, int bit_depth);

extern const int8_t ff_hevc_rpi_epel_filters[7][4];
extern const int8_t ff_hevc_rpi_qpel_filters[3][16];

void ff_hevc_rpi_dsp_init_ppc(HEVCDSPContext *c, const int bit_depth);
void ff_hevc_rpi_dsp_init_x86(HEVCDSPContext *c, const int bit_depth);
void ff_hevcdsp_rpi_init_arm(HEVCDSPContext *c, const int bit_depth);
void ff_hevc_rpi_dsp_init_mips(HEVCDSPContext *c, const int bit_depth);
#endif /* AVCODEC_RPI_HEVCDSP_H */
