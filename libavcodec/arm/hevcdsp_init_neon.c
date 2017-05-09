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
void ff_hevc_transform_4x4_neon_8(int16_t *coeffs, int col_limit);
void ff_hevc_transform_8x8_neon_8(int16_t *coeffs, int col_limit);
void ff_hevc_idct_4x4_dc_neon_8(int16_t *coeffs);
void ff_hevc_idct_8x8_dc_neon_8(int16_t *coeffs);
void ff_hevc_idct_16x16_dc_neon_8(int16_t *coeffs);
void ff_hevc_idct_32x32_dc_neon_8(int16_t *coeffs);
void ff_hevc_transform_luma_4x4_neon_8(int16_t *coeffs);
void ff_hevc_add_residual_4x4_neon_8(uint8_t *_dst, int16_t *coeffs,
                                     ptrdiff_t stride);
void ff_hevc_add_residual_8x8_neon_8(uint8_t *_dst, int16_t *coeffs,
                                     ptrdiff_t stride);
void ff_hevc_add_residual_16x16_neon_8(uint8_t *_dst, int16_t *coeffs,
                                       ptrdiff_t stride);
void ff_hevc_add_residual_32x32_neon_8(uint8_t *_dst, int16_t *coeffs,
                                       ptrdiff_t stride);

void ff_hevc_sao_band_w8_neon_8(uint8_t *_dst, uint8_t *_src, int8_t * offset_table, ptrdiff_t stride_src, ptrdiff_t stride_dst, int height);
void ff_hevc_sao_band_w16_neon_8(uint8_t *_dst, uint8_t *_src, int8_t * offset_table, ptrdiff_t stride_src, ptrdiff_t stride_dst, int height);
void ff_hevc_sao_band_w32_neon_8(uint8_t *_dst, uint8_t *_src, int8_t * offset_table, ptrdiff_t stride_src, ptrdiff_t stride_dst, int height);
void ff_hevc_sao_band_w64_neon_8(uint8_t *_dst, uint8_t *_src, int8_t * offset_table, ptrdiff_t stride_src, ptrdiff_t stride_dst, int height);

void ff_hevc_sao_edge_eo0_w32_neon_8(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, ptrdiff_t stride_src, int height, int8_t *sao_offset_table);
void ff_hevc_sao_edge_eo1_w32_neon_8(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, ptrdiff_t stride_src, int height, int8_t *sao_offset_table);
void ff_hevc_sao_edge_eo2_w32_neon_8(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, ptrdiff_t stride_src, int height, int8_t *sao_offset_table);
void ff_hevc_sao_edge_eo3_w32_neon_8(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, ptrdiff_t stride_src, int height, int8_t *sao_offset_table);

void ff_hevc_sao_edge_eo0_w64_neon_8(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, ptrdiff_t stride_src, int height, int8_t *sao_offset_table);
void ff_hevc_sao_edge_eo1_w64_neon_8(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, ptrdiff_t stride_src, int height, int8_t *sao_offset_table);
void ff_hevc_sao_edge_eo2_w64_neon_8(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, ptrdiff_t stride_src, int height, int8_t *sao_offset_table);
void ff_hevc_sao_edge_eo3_w64_neon_8(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, ptrdiff_t stride_src, int height, int8_t *sao_offset_table);

typedef void ff_hevc_sao_edge_c_eoX_w64_neon_8(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, ptrdiff_t stride_src, int height, int8_t *sao_offset_table_u, int8_t *sao_offset_table_v);
extern ff_hevc_sao_edge_c_eoX_w64_neon_8 ff_hevc_sao_edge_c_eo0_w64_neon_8;
extern ff_hevc_sao_edge_c_eoX_w64_neon_8 ff_hevc_sao_edge_c_eo1_w64_neon_8;
extern ff_hevc_sao_edge_c_eoX_w64_neon_8 ff_hevc_sao_edge_c_eo2_w64_neon_8;
extern ff_hevc_sao_edge_c_eoX_w64_neon_8 ff_hevc_sao_edge_c_eo3_w64_neon_8;

void ff_hevc_sao_band_c_neon_8(uint8_t *_dst, const uint8_t *_src,
                                  ptrdiff_t stride_dst, ptrdiff_t stride_src,
                                  const int16_t *sao_offset_val_u, int sao_left_class_u,
                                  const int16_t *sao_offset_val_v, int sao_left_class_v,
                                  int width, int height);


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

static void ff_hevc_sao_band_neon_wrapper(uint8_t *_dst, uint8_t *_src, ptrdiff_t stride_dst, ptrdiff_t stride_src,
                                          int16_t *sao_offset_val, int sao_left_class, int width, int height)
{
    pixel *dst = (pixel *)_dst;
    pixel *src = (pixel *)_src;
    int8_t offset_table[32] = { 0 };
    int k, y, x;
    int shift  = 3; // BIT_DEPTH - 5
    int cwidth = 0;

    stride_src /= sizeof(pixel);
    stride_dst /= sizeof(pixel);

    for (k = 0; k < 4; k++)
        offset_table[(k + sao_left_class) & 31] = sao_offset_val[k + 1];

    if (height % 8 == 0)
        cwidth = width;

    switch(cwidth){
    case 8:
        ff_hevc_sao_band_w8_neon_8(_dst, _src, offset_table, stride_src, stride_dst, height);
        break;
    case 16:
        ff_hevc_sao_band_w16_neon_8(_dst, _src, offset_table, stride_src, stride_dst, height);
        break;
    case 32:
        ff_hevc_sao_band_w32_neon_8(_dst, _src, offset_table, stride_src, stride_dst, height);
        break;
    case 64:
        ff_hevc_sao_band_w64_neon_8(_dst, _src, offset_table, stride_src, stride_dst, height);
        break;
    default:
        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x++)
                dst[x] = av_clip_pixel(src[x] + offset_table[src[x] >> shift]);
            dst += stride_dst;
            src += stride_src;
        }
    }
}

static void ff_hevc_sao_band_c_neon_wrapper(uint8_t *_dst, const uint8_t *_src,
                                  ptrdiff_t stride_dst, ptrdiff_t stride_src,
                                  const int16_t *sao_offset_val_u, int sao_left_class_u,
                                  const int16_t *sao_offset_val_v, int sao_left_class_v,
                                  int width, int height)
{
    // Width 32 already dealt with
    // width 16 code works in double lines
    if (width == 16 && (height & 1) == 0) {
        ff_hevc_sao_band_c_neon_8(_dst, _src, stride_src, stride_dst,
                                          sao_offset_val_u, sao_left_class_u,
                                          sao_offset_val_v, sao_left_class_v,
                                          width, height);
    }
    else
    {
        const int shift  = 3; // BIT_DEPTH - 5
        int k, y, x;
        pixel *dst = (pixel *)_dst;
        pixel *src = (pixel *)_src;
        int8_t offset_table_u[32] = { 0 };
        int8_t offset_table_v[32] = { 0 };

        stride_src /= sizeof(pixel);
        stride_dst /= sizeof(pixel);

        for (k = 0; k < 4; k++)
            offset_table_u[(k + sao_left_class_u) & 31] = sao_offset_val_u[k + 1];
        for (k = 0; k < 4; k++)
            offset_table_v[(k + sao_left_class_v) & 31] = sao_offset_val_v[k + 1];

        for (y = 0; y < height; y++) {
            for (x = 0; x < width * 2; x += 2)
            {
                dst[x + 0] = av_clip_pixel(src[x + 0] + offset_table_u[src[x + 0] >> shift]);
                dst[x + 1] = av_clip_pixel(src[x + 1] + offset_table_v[src[x + 1] >> shift]);
            }
            dst += stride_dst;
            src += stride_src;

        }
    }
}

#define CMP(a, b) ((a) > (b) ? 1 : ((a) == (b) ? 0 : -1))
static void ff_hevc_sao_edge_neon_wrapper(uint8_t *_dst /* align 16 */, uint8_t *_src /* align 32 */, ptrdiff_t stride_dst,
                                          int16_t *_sao_offset_val, int eo, int width, int height)
{
    static const uint8_t edge_idx[] = { 1, 2, 0, 3, 4 };
    static const int8_t pos[4][2][2] = {
        { { -1,  0 }, {  1, 0 } }, // horizontal
        { {  0, -1 }, {  0, 1 } }, // vertical
        { { -1, -1 }, {  1, 1 } }, // 45 degree
        { {  1, -1 }, { -1, 1 } }, // 135 degree
    };
    int8_t sao_offset_val[8];  // padding of 3 for vld
    ptrdiff_t stride_src = (2*MAX_PB_SIZE + FF_INPUT_BUFFER_PADDING_SIZE);
    pixel *dst = (pixel *)_dst;
    pixel *src = (pixel *)_src;
    int a_stride, b_stride;
    int x, y;
    int cwidth = 0;

    for (x = 0; x < 5; x++) {
        sao_offset_val[x] = _sao_offset_val[edge_idx[x]];
    }

    if (height % 8 == 0)
        cwidth = width;

    stride_src /= sizeof(pixel);
    stride_dst /= sizeof(pixel);

    switch (cwidth) {
    case 32:
        switch(eo) {
        case 0:
            ff_hevc_sao_edge_eo0_w32_neon_8(dst, src, stride_dst, stride_src, height, sao_offset_val);
            break;
        case 1:
            ff_hevc_sao_edge_eo1_w32_neon_8(dst, src, stride_dst, stride_src, height, sao_offset_val);
            break;
        case 2:
            ff_hevc_sao_edge_eo2_w32_neon_8(dst, src, stride_dst, stride_src, height, sao_offset_val);
            break;
        case 3:
            ff_hevc_sao_edge_eo3_w32_neon_8(dst, src, stride_dst, stride_src, height, sao_offset_val);
            break;
        }
        break;
    case 64:
        switch(eo) {
        case 0:
            ff_hevc_sao_edge_eo0_w64_neon_8(dst, src, stride_dst, stride_src, height, sao_offset_val);
            break;
        case 1:
            ff_hevc_sao_edge_eo1_w64_neon_8(dst, src, stride_dst, stride_src, height, sao_offset_val);
            break;
        case 2:
            ff_hevc_sao_edge_eo2_w64_neon_8(dst, src, stride_dst, stride_src, height, sao_offset_val);
            break;
        case 3:
            ff_hevc_sao_edge_eo3_w64_neon_8(dst, src, stride_dst, stride_src, height, sao_offset_val);
            break;
        }
        break;
    default:
        a_stride = pos[eo][0][0] + pos[eo][0][1] * stride_src;
        b_stride = pos[eo][1][0] + pos[eo][1][1] * stride_src;
        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x++) {
                int diff0         = CMP(src[x], src[x + a_stride]);
                int diff1         = CMP(src[x], src[x + b_stride]);
                int idx           = diff0 + diff1;
                if (idx)
                    dst[x] = av_clip_pixel(src[x] + sao_offset_val[idx+2]);
            }
            src += stride_src;
            dst += stride_dst;
        }
    }
}


static void ff_hevc_sao_edge_c_neon_wrapper(uint8_t *_dst, const uint8_t *_src, ptrdiff_t stride_dst,
                                  const int16_t *_sao_offset_val_u, const int16_t *_sao_offset_val_v,
                                  int eo, int width, int height)
{
    static const uint8_t edge_idx[] = { 1, 2, 0, 3, 4 };
    static const int8_t pos[4][2][2] = {
        { { -1,  0 }, {  1, 0 } }, // horizontal
        { {  0, -1 }, {  0, 1 } }, // vertical
        { { -1, -1 }, {  1, 1 } }, // 45 degree
        { {  1, -1 }, { -1, 1 } }, // 135 degree
    };
    int8_t sao_offset_val_u[8];  // padding of 3 for vld
    int8_t sao_offset_val_v[8];  // padding of 3 for vld
    ptrdiff_t stride_src = (2*MAX_PB_SIZE + FF_INPUT_BUFFER_PADDING_SIZE);
    pixel *dst = (pixel *)_dst;
    pixel *src = (pixel *)_src;
    int a_stride, b_stride;
    int x, y;
    int cwidth = 0;

    width *= 2;

    for (x = 0; x < 5; x++) {
        sao_offset_val_u[x] = _sao_offset_val_u[edge_idx[x]];
        sao_offset_val_v[x] = _sao_offset_val_v[edge_idx[x]];
    }

    if (height % 8 == 0 && eo <= 1)
        cwidth = width;

    stride_src /= sizeof(pixel);
    stride_dst /= sizeof(pixel);

    switch (cwidth) {
#if 0
    case
    case 32:
        switch(eo) {
        case 0:
            ff_hevc_sao_edge_eo0_w32_neon_8(dst, src, stride_dst, stride_src, height, sao_offset_val);
            break;
        case 1:
            ff_hevc_sao_edge_eo1_w32_neon_8(dst, src, stride_dst, stride_src, height, sao_offset_val);
            break;
        case 2:
            ff_hevc_sao_edge_eo2_w32_neon_8(dst, src, stride_dst, stride_src, height, sao_offset_val);
            break;
        case 3:
            ff_hevc_sao_edge_eo3_w32_neon_8(dst, src, stride_dst, stride_src, height, sao_offset_val);
            break;
        }
        break;
#endif
    case 64:
    {
        static ff_hevc_sao_edge_c_eoX_w64_neon_8 * const eoX[] = {
            ff_hevc_sao_edge_c_eo0_w64_neon_8,
            ff_hevc_sao_edge_c_eo1_w64_neon_8,
            ff_hevc_sao_edge_c_eo2_w64_neon_8,
            ff_hevc_sao_edge_c_eo3_w64_neon_8
        };
        eoX[eo](dst, src, stride_dst, stride_src, height, sao_offset_val_u, sao_offset_val_v);
        break;
    }
    default:
        a_stride = pos[eo][0][0] * 2 + pos[eo][0][1] * stride_src;
        b_stride = pos[eo][1][0] * 2 + pos[eo][1][1] * stride_src;
        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x += 2) {
                int diff0u = CMP(src[x], src[x + a_stride]);
                int diff1u = CMP(src[x], src[x + b_stride]);
                int diff0v = CMP(src[x+1], src[x+1 + a_stride]);
                int diff1v = CMP(src[x+1], src[x+1 + b_stride]);
                dst[x] = av_clip_pixel(src[x] + sao_offset_val_u[2 + diff0u + diff1u]);
                dst[x+1] = av_clip_pixel(src[x+1] + sao_offset_val_v[2 + diff0v + diff1v]);
            }
            src += stride_src;
            dst += stride_dst;
        }
        break;
    }
}
#undef CMP

void ff_hevc_deblocking_boundary_strengths_neon(int pus, int dup, int in_inc, int out_inc,
                                                int *curr_rpl0, int *curr_rpl1, int *neigh_rpl0, int *neigh_rpl1,
                                                MvField *curr, MvField *neigh, uint8_t *bs);

av_cold void ff_hevcdsp_init_neon(HEVCDSPContext *c, const int bit_depth)
{
    if (bit_depth == 8) {
        int x;
        c->hevc_v_loop_filter_luma     = ff_hevc_v_loop_filter_luma_neon;
        c->hevc_v_loop_filter_luma_c   = ff_hevc_v_loop_filter_luma_neon;
        c->hevc_h_loop_filter_luma     = ff_hevc_h_loop_filter_luma_neon;
        c->hevc_h_loop_filter_luma_c   = ff_hevc_h_loop_filter_luma_neon;
        c->hevc_v_loop_filter_chroma   = ff_hevc_v_loop_filter_chroma_neon;
        c->hevc_h_loop_filter_chroma   = ff_hevc_h_loop_filter_chroma_neon;
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
        c->transform_4x4_luma          = ff_hevc_transform_luma_4x4_neon_8;
        for (x = 0; x < sizeof c->sao_band_filter / sizeof *c->sao_band_filter; x++) {
          c->sao_band_filter[x]        = ff_hevc_sao_band_neon_wrapper;
          c->sao_band_filter_c[x]      = ff_hevc_sao_band_c_neon_wrapper;
          c->sao_edge_filter[x]        = ff_hevc_sao_edge_neon_wrapper;
          c->sao_edge_filter_c[x]      = ff_hevc_sao_edge_c_neon_wrapper;
        }
        c->sao_band_filter_c[2]        = ff_hevc_sao_band_c_neon_8;  // width=32
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

    assert(offsetof(MvField, mv) == 0);
    assert(offsetof(MvField, ref_idx) == 8);
    assert(offsetof(MvField, pred_flag) == 10);
    c->hevc_deblocking_boundary_strengths = ff_hevc_deblocking_boundary_strengths_neon;
}
