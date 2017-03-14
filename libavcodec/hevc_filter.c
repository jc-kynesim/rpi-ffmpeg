/*
 * HEVC video decoder
 *
 * Copyright (C) 2012 - 2013 Guillaume Martres
 * Copyright (C) 2013 Seppo Tomperi
 * Copyright (C) 2013 Wassim Hamidouche
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

//#define DISABLE_SAO
//#define DISABLE_DEBLOCK
//#define DISABLE_STRENGTHS
// define DISABLE_DEBLOCK_NONREF for a 6% speed boost (by skipping deblocking on unimportant frames)
//#define DISABLE_DEBLOCK_NONREF

#include "libavutil/common.h"
#include "libavutil/internal.h"

#include "cabac_functions.h"
#include "golomb.h"
#include "hevc.h"

#include "bit_depth_template.c"

#ifdef RPI
#include "rpi_user_vcsm.h"
#include "rpi_qpu.h"
#endif

#define LUMA 0
#define CB 1
#define CR 2

static const uint8_t tctable[54] = {
    0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 0, 0, 1, // QP  0...18
    1, 1, 1, 1, 1, 1, 1,  1,  2,  2,  2,  2,  3,  3,  3,  3, 4, 4, 4, // QP 19...37
    5, 5, 6, 6, 7, 8, 9, 10, 11, 13, 14, 16, 18, 20, 22, 24           // QP 38...53
};

static const uint8_t betatable[52] = {
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  6,  7,  8, // QP 0...18
     9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, // QP 19...37
    38, 40, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62, 64                      // QP 38...51
};

static int chroma_tc(HEVCContext *s, int qp_y, int c_idx, int tc_offset)
{
    static const int qp_c[] = {
        29, 30, 31, 32, 33, 33, 34, 34, 35, 35, 36, 36, 37, 37
    };
    int qp, qp_i, offset, idxt;

    // slice qp offset is not used for deblocking
    if (c_idx == 1)
        offset = s->ps.pps->cb_qp_offset;
    else
        offset = s->ps.pps->cr_qp_offset;

    qp_i = av_clip(qp_y + offset, 0, 57);
    if (s->ps.sps->chroma_format_idc == 1) {
        if (qp_i < 30)
            qp = qp_i;
        else if (qp_i > 43)
            qp = qp_i - 6;
        else
            qp = qp_c[qp_i - 30];
    } else {
        qp = av_clip(qp_i, 0, 51);
    }

    idxt = av_clip(qp + DEFAULT_INTRA_TC_OFFSET + tc_offset, 0, 53);
    return tctable[idxt];
}

static int get_qPy_pred(HEVCContext *s, int xBase, int yBase, int log2_cb_size)
{
    HEVCLocalContext *lc     = s->HEVClc;
    int ctb_size_mask        = (1 << s->ps.sps->log2_ctb_size) - 1;
    int MinCuQpDeltaSizeMask = (1 << (s->ps.sps->log2_ctb_size -
                                      s->ps.pps->diff_cu_qp_delta_depth)) - 1;
    int xQgBase              = xBase - (xBase & MinCuQpDeltaSizeMask);
    int yQgBase              = yBase - (yBase & MinCuQpDeltaSizeMask);
    int min_cb_width         = s->ps.sps->min_cb_width;
    int x_cb                 = xQgBase >> s->ps.sps->log2_min_cb_size;
    int y_cb                 = yQgBase >> s->ps.sps->log2_min_cb_size;
    int availableA           = (xBase   & ctb_size_mask) &&
                               (xQgBase & ctb_size_mask);
    int availableB           = (yBase   & ctb_size_mask) &&
                               (yQgBase & ctb_size_mask);
    int qPy_pred, qPy_a, qPy_b;

    // qPy_pred
    if (lc->first_qp_group || (!xQgBase && !yQgBase)) {
        lc->first_qp_group = !lc->tu.is_cu_qp_delta_coded;
        qPy_pred = s->sh.slice_qp;
    } else {
        qPy_pred = lc->qPy_pred;
    }

    // qPy_a
    if (availableA == 0)
        qPy_a = qPy_pred;
    else
        qPy_a = s->qp_y_tab[(x_cb - 1) + y_cb * min_cb_width];

    // qPy_b
    if (availableB == 0)
        qPy_b = qPy_pred;
    else
        qPy_b = s->qp_y_tab[x_cb + (y_cb - 1) * min_cb_width];

    av_assert2(qPy_a >= -s->ps.sps->qp_bd_offset && qPy_a < 52);
    av_assert2(qPy_b >= -s->ps.sps->qp_bd_offset && qPy_b < 52);

    return (qPy_a + qPy_b + 1) >> 1;
}

void ff_hevc_set_qPy(HEVCContext *s, int xBase, int yBase, int log2_cb_size)
{
    int qp_y = get_qPy_pred(s, xBase, yBase, log2_cb_size);

    if (s->HEVClc->tu.cu_qp_delta != 0) {
        int off = s->ps.sps->qp_bd_offset;
        s->HEVClc->qp_y = FFUMOD(qp_y + s->HEVClc->tu.cu_qp_delta + 52 + 2 * off,
                                 52 + off) - off;
    } else
        s->HEVClc->qp_y = qp_y;
}

static int get_qPy(HEVCContext *s, int xC, int yC)
{
    int log2_min_cb_size  = s->ps.sps->log2_min_cb_size;
    int x                 = xC >> log2_min_cb_size;
    int y                 = yC >> log2_min_cb_size;
    return s->qp_y_tab[x + y * s->ps.sps->min_cb_width];
}

static void copy_CTB(uint8_t *dst, const uint8_t *src, int width, int height,
                     intptr_t stride_dst, intptr_t stride_src)
{
int i, j;

    if (((intptr_t)dst | (intptr_t)src | stride_dst | stride_src) & 15) {
        for (i = 0; i < height; i++) {
            for (j = 0; j < width; j+=8)
                AV_COPY64U(dst+j, src+j);
            dst += stride_dst;
            src += stride_src;
        }
    } else {
        for (i = 0; i < height; i++) {
            for (j = 0; j < width; j+=16)
                AV_COPY128(dst+j, src+j);
            dst += stride_dst;
            src += stride_src;
        }
    }
}

static void copy_pixel(uint8_t *dst, const uint8_t *src, int pixel_shift)
{
    if (pixel_shift)
        *(uint16_t *)dst = *(uint16_t *)src;
    else
        *dst = *src;
}

static void copy_vert(uint8_t *dst, const uint8_t *src,
                      int pixel_shift, int height,
                      int stride_dst, int stride_src)
{
    int i;
    if (pixel_shift == 0) {
        for (i = 0; i < height; i++) {
            *dst = *src;
            dst += stride_dst;
            src += stride_src;
        }
    } else {
        for (i = 0; i < height; i++) {
            *(uint16_t *)dst = *(uint16_t *)src;
            dst += stride_dst;
            src += stride_src;
        }
    }
}

static void copy_CTB_to_hv(HEVCContext *s, const uint8_t *src,
                           int stride_src, int x, int y, int width, int height,
                           int c_idx, int x_ctb, int y_ctb)
{
    int sh = s->ps.sps->pixel_shift;
    int w = s->ps.sps->width >> s->ps.sps->hshift[c_idx];
    int h = s->ps.sps->height >> s->ps.sps->vshift[c_idx];

    /* copy horizontal edges */
    memcpy(s->sao_pixel_buffer_h[c_idx] + (((2 * y_ctb) * w + x) << sh),
        src, width << sh);
    memcpy(s->sao_pixel_buffer_h[c_idx] + (((2 * y_ctb + 1) * w + x) << sh),
        src + stride_src * (height - 1), width << sh);

    /* copy vertical edges */
    copy_vert(s->sao_pixel_buffer_v[c_idx] + (((2 * x_ctb) * h + y) << sh), src, sh, height, 1 << sh, stride_src);

    copy_vert(s->sao_pixel_buffer_v[c_idx] + (((2 * x_ctb + 1) * h + y) << sh), src + ((width - 1) << sh), sh, height, 1 << sh, stride_src);
}

static void restore_tqb_pixels(HEVCContext *s,
                               uint8_t *src1, const uint8_t *dst1,
                               ptrdiff_t stride_src, ptrdiff_t stride_dst,
                               int x0, int y0, int width, int height, int c_idx)
{
    if ( s->ps.pps->transquant_bypass_enable_flag ||
            (s->ps.sps->pcm.loop_filter_disable_flag && s->ps.sps->pcm_enabled_flag)) {
        int x, y;
        int min_pu_size  = 1 << s->ps.sps->log2_min_pu_size;
        int hshift       = s->ps.sps->hshift[c_idx];
        int vshift       = s->ps.sps->vshift[c_idx];
        int x_min        = ((x0         ) >> s->ps.sps->log2_min_pu_size);
        int y_min        = ((y0         ) >> s->ps.sps->log2_min_pu_size);
        int x_max        = ((x0 + width ) >> s->ps.sps->log2_min_pu_size);
        int y_max        = ((y0 + height) >> s->ps.sps->log2_min_pu_size);
        int len          = (min_pu_size >> hshift) << s->ps.sps->pixel_shift;
        for (y = y_min; y < y_max; y++) {
            for (x = x_min; x < x_max; x++) {
                if (s->is_pcm[y * s->ps.sps->min_pu_width + x]) {
                    int n;
                    uint8_t *src = src1 + (((y << s->ps.sps->log2_min_pu_size) - y0) >> vshift) * stride_src + ((((x << s->ps.sps->log2_min_pu_size) - x0) >> hshift) << s->ps.sps->pixel_shift);
                    const uint8_t *dst = dst1 + (((y << s->ps.sps->log2_min_pu_size) - y0) >> vshift) * stride_dst + ((((x << s->ps.sps->log2_min_pu_size) - x0) >> hshift) << s->ps.sps->pixel_shift);
                    for (n = 0; n < (min_pu_size >> vshift); n++) {
                        memcpy(src, dst, len);
                        src += stride_src;
                        dst += stride_dst;
                    }
                }
            }
        }
    }
}

#define CTB(tab, x, y) ((tab)[(y) * s->ps.sps->ctb_width + (x)])

static void sao_filter_CTB(HEVCContext *s, int x, int y)
{
    static const uint8_t sao_tab[8] = { 0, 1, 2, 2, 3, 3, 4, 4 };
    HEVCLocalContext *lc = s->HEVClc;
    int c_idx;
    int edges[4];  // 0 left 1 top 2 right 3 bottom
    int x_ctb                = x >> s->ps.sps->log2_ctb_size;
    int y_ctb                = y >> s->ps.sps->log2_ctb_size;
    int ctb_addr_rs          = y_ctb * s->ps.sps->ctb_width + x_ctb;
    int ctb_addr_ts          = s->ps.pps->ctb_addr_rs_to_ts[ctb_addr_rs];
    SAOParams *sao           = &CTB(s->sao, x_ctb, y_ctb);
    // flags indicating unfilterable edges
    uint8_t vert_edge[]      = { 0, 0 };
    uint8_t horiz_edge[]     = { 0, 0 };
    uint8_t diag_edge[]      = { 0, 0, 0, 0 };
    uint8_t lfase            = CTB(s->filter_slice_edges, x_ctb, y_ctb);
    uint8_t no_tile_filter   = s->ps.pps->tiles_enabled_flag &&
                               !s->ps.pps->loop_filter_across_tiles_enabled_flag;
    uint8_t restore          = no_tile_filter || !lfase;
    uint8_t left_tile_edge   = 0;
    uint8_t right_tile_edge  = 0;
    uint8_t up_tile_edge     = 0;
    uint8_t bottom_tile_edge = 0;

    edges[0]   = x_ctb == 0;
    edges[1]   = y_ctb == 0;
    edges[2]   = x_ctb == s->ps.sps->ctb_width  - 1;
    edges[3]   = y_ctb == s->ps.sps->ctb_height - 1;

#ifdef DISABLE_SAO
    return;
#endif

    if (restore) {
        if (!edges[0]) {
            left_tile_edge  = no_tile_filter && s->ps.pps->tile_id[ctb_addr_ts] != s->ps.pps->tile_id[s->ps.pps->ctb_addr_rs_to_ts[ctb_addr_rs-1]];
            vert_edge[0]    = (!lfase && CTB(s->tab_slice_address, x_ctb, y_ctb) != CTB(s->tab_slice_address, x_ctb - 1, y_ctb)) || left_tile_edge;
        }
        if (!edges[2]) {
            right_tile_edge = no_tile_filter && s->ps.pps->tile_id[ctb_addr_ts] != s->ps.pps->tile_id[s->ps.pps->ctb_addr_rs_to_ts[ctb_addr_rs+1]];
            vert_edge[1]    = (!lfase && CTB(s->tab_slice_address, x_ctb, y_ctb) != CTB(s->tab_slice_address, x_ctb + 1, y_ctb)) || right_tile_edge;
        }
        if (!edges[1]) {
            up_tile_edge     = no_tile_filter && s->ps.pps->tile_id[ctb_addr_ts] != s->ps.pps->tile_id[s->ps.pps->ctb_addr_rs_to_ts[ctb_addr_rs - s->ps.sps->ctb_width]];
            horiz_edge[0]    = (!lfase && CTB(s->tab_slice_address, x_ctb, y_ctb) != CTB(s->tab_slice_address, x_ctb, y_ctb - 1)) || up_tile_edge;
        }
        if (!edges[3]) {
            bottom_tile_edge = no_tile_filter && s->ps.pps->tile_id[ctb_addr_ts] != s->ps.pps->tile_id[s->ps.pps->ctb_addr_rs_to_ts[ctb_addr_rs + s->ps.sps->ctb_width]];
            horiz_edge[1]    = (!lfase && CTB(s->tab_slice_address, x_ctb, y_ctb) != CTB(s->tab_slice_address, x_ctb, y_ctb + 1)) || bottom_tile_edge;
        }
        if (!edges[0] && !edges[1]) {
            diag_edge[0] = (!lfase && CTB(s->tab_slice_address, x_ctb, y_ctb) != CTB(s->tab_slice_address, x_ctb - 1, y_ctb - 1)) || left_tile_edge || up_tile_edge;
        }
        if (!edges[1] && !edges[2]) {
            diag_edge[1] = (!lfase && CTB(s->tab_slice_address, x_ctb, y_ctb) != CTB(s->tab_slice_address, x_ctb + 1, y_ctb - 1)) || right_tile_edge || up_tile_edge;
        }
        if (!edges[2] && !edges[3]) {
            diag_edge[2] = (!lfase && CTB(s->tab_slice_address, x_ctb, y_ctb) != CTB(s->tab_slice_address, x_ctb + 1, y_ctb + 1)) || right_tile_edge || bottom_tile_edge;
        }
        if (!edges[0] && !edges[3]) {
            diag_edge[3] = (!lfase && CTB(s->tab_slice_address, x_ctb, y_ctb) != CTB(s->tab_slice_address, x_ctb - 1, y_ctb + 1)) || left_tile_edge || bottom_tile_edge;
        }
    }

    for (c_idx = 0; c_idx < (s->ps.sps->chroma_format_idc ? 3 : 1); c_idx++) {
        int x0       = x >> s->ps.sps->hshift[c_idx];
        int y0       = y >> s->ps.sps->vshift[c_idx];
        int stride_src = s->frame->linesize[c_idx];
        int ctb_size_h = (1 << (s->ps.sps->log2_ctb_size)) >> s->ps.sps->hshift[c_idx];
        int ctb_size_v = (1 << (s->ps.sps->log2_ctb_size)) >> s->ps.sps->vshift[c_idx];
        int width    = FFMIN(ctb_size_h, (s->ps.sps->width  >> s->ps.sps->hshift[c_idx]) - x0);
        int height   = FFMIN(ctb_size_v, (s->ps.sps->height >> s->ps.sps->vshift[c_idx]) - y0);
        int tab      = sao_tab[(FFALIGN(width, 8) >> 3) - 1];
        uint8_t *src = &s->frame->data[c_idx][y0 * stride_src + (x0 << s->ps.sps->pixel_shift)];
        int stride_dst;
        uint8_t *dst;

        switch (sao->type_idx[c_idx]) {
        case SAO_BAND:
            copy_CTB_to_hv(s, src, stride_src, x0, y0, width, height, c_idx,
                           x_ctb, y_ctb);
            if (s->ps.pps->transquant_bypass_enable_flag ||
                (s->ps.sps->pcm.loop_filter_disable_flag && s->ps.sps->pcm_enabled_flag)) {
            dst = lc->edge_emu_buffer;
            stride_dst = 2*MAX_PB_SIZE;
            copy_CTB(dst, src, width << s->ps.sps->pixel_shift, height, stride_dst, stride_src);
            s->hevcdsp.sao_band_filter[tab](src, dst, stride_src, stride_dst,
                                            sao->offset_val[c_idx], sao->band_position[c_idx],
                                            width, height);
            restore_tqb_pixels(s, src, dst, stride_src, stride_dst,
                               x, y, width, height, c_idx);
            } else {
            s->hevcdsp.sao_band_filter[tab](src, src, stride_src, stride_src,
                                            sao->offset_val[c_idx], sao->band_position[c_idx],
                                            width, height);
            }
            sao->type_idx[c_idx] = SAO_APPLIED;
            break;
        case SAO_EDGE:
        {
            int w = s->ps.sps->width >> s->ps.sps->hshift[c_idx];
            int h = s->ps.sps->height >> s->ps.sps->vshift[c_idx];
            int left_edge = edges[0];
            int top_edge = edges[1];
            int right_edge = edges[2];
            int bottom_edge = edges[3];
            int sh = s->ps.sps->pixel_shift;
            int left_pixels, right_pixels;

            stride_dst = 2*MAX_PB_SIZE + AV_INPUT_BUFFER_PADDING_SIZE;
            dst = lc->edge_emu_buffer + stride_dst + AV_INPUT_BUFFER_PADDING_SIZE;

            if (!top_edge) {
                int left = 1 - left_edge;
                int right = 1 - right_edge;
                const uint8_t *src1[2];
                uint8_t *dst1;
                int src_idx, pos;

                dst1 = dst - stride_dst - (left << sh);
                src1[0] = src - stride_src - (left << sh);
                src1[1] = s->sao_pixel_buffer_h[c_idx] + (((2 * y_ctb - 1) * w + x0 - left) << sh);
                pos = 0;
                if (left) {
                    src_idx = (CTB(s->sao, x_ctb-1, y_ctb-1).type_idx[c_idx] ==
                               SAO_APPLIED);
                    copy_pixel(dst1, src1[src_idx], sh);
                    pos += (1 << sh);
                }
                src_idx = (CTB(s->sao, x_ctb, y_ctb-1).type_idx[c_idx] ==
                           SAO_APPLIED);
                memcpy(dst1 + pos, src1[src_idx] + pos, width << sh);
                if (right) {
                    pos += width << sh;
                    src_idx = (CTB(s->sao, x_ctb+1, y_ctb-1).type_idx[c_idx] ==
                               SAO_APPLIED);
                    copy_pixel(dst1 + pos, src1[src_idx] + pos, sh);
                }
            }
            if (!bottom_edge) {
                int left = 1 - left_edge;
                int right = 1 - right_edge;
                const uint8_t *src1[2];
                uint8_t *dst1;
                int src_idx, pos;

                dst1 = dst + height * stride_dst - (left << sh);
                src1[0] = src + height * stride_src - (left << sh);
                src1[1] = s->sao_pixel_buffer_h[c_idx] + (((2 * y_ctb + 2) * w + x0 - left) << sh);
                pos = 0;
                if (left) {
                    src_idx = (CTB(s->sao, x_ctb-1, y_ctb+1).type_idx[c_idx] ==
                               SAO_APPLIED);
                    copy_pixel(dst1, src1[src_idx], sh);
                    pos += (1 << sh);
                }
                src_idx = (CTB(s->sao, x_ctb, y_ctb+1).type_idx[c_idx] ==
                           SAO_APPLIED);
                memcpy(dst1 + pos, src1[src_idx] + pos, width << sh);
                if (right) {
                    pos += width << sh;
                    src_idx = (CTB(s->sao, x_ctb+1, y_ctb+1).type_idx[c_idx] ==
                               SAO_APPLIED);
                    copy_pixel(dst1 + pos, src1[src_idx] + pos, sh);
                }
            }
            left_pixels = 0;
            if (!left_edge) {
                if (CTB(s->sao, x_ctb-1, y_ctb).type_idx[c_idx] == SAO_APPLIED) {
                    copy_vert(dst - (1 << sh),
                              s->sao_pixel_buffer_v[c_idx] + (((2 * x_ctb - 1) * h + y0) << sh),
                              sh, height, stride_dst, 1 << sh);
                } else {
                    left_pixels = 1;
                }
            }
            right_pixels = 0;
            if (!right_edge) {
                if (CTB(s->sao, x_ctb+1, y_ctb).type_idx[c_idx] == SAO_APPLIED) {
                    copy_vert(dst + (width << sh),
                              s->sao_pixel_buffer_v[c_idx] + (((2 * x_ctb + 2) * h + y0) << sh),
                              sh, height, stride_dst, 1 << sh);
                } else {
                    right_pixels = 1;
                }
            }

            copy_CTB(dst - (left_pixels << sh),
                     src - (left_pixels << sh),
                     (width + left_pixels + right_pixels) << sh,
                     height, stride_dst, stride_src);

            copy_CTB_to_hv(s, src, stride_src, x0, y0, width, height, c_idx,
                           x_ctb, y_ctb);
            s->hevcdsp.sao_edge_filter[tab](src, dst, stride_src, sao->offset_val[c_idx],
                                            sao->eo_class[c_idx], width, height);
            s->hevcdsp.sao_edge_restore[restore](src, dst,
                                                stride_src, stride_dst,
                                                sao,
                                                edges, width,
                                                height, c_idx,
                                                vert_edge,
                                                horiz_edge,
                                                diag_edge);
            restore_tqb_pixels(s, src, dst, stride_src, stride_dst,
                               x, y, width, height, c_idx);
            sao->type_idx[c_idx] = SAO_APPLIED;
            break;
        }
        }
    }
}

static int get_pcm(HEVCContext *s, int x, int y)
{
    int log2_min_pu_size = s->ps.sps->log2_min_pu_size;
    int x_pu, y_pu;

    if (x < 0 || y < 0)
        return 2;

    x_pu = x >> log2_min_pu_size;
    y_pu = y >> log2_min_pu_size;

    if (x_pu >= s->ps.sps->min_pu_width || y_pu >= s->ps.sps->min_pu_height)
        return 2;
    return s->is_pcm[y_pu * s->ps.sps->min_pu_width + x_pu];
}

#define TC_CALC(qp, bs)                                                 \
    tctable[av_clip((qp) + DEFAULT_INTRA_TC_OFFSET * ((bs) - 1) +       \
                    (tc_offset >> 1 << 1),                              \
                    0, MAX_QP + DEFAULT_INTRA_TC_OFFSET)]

static void deblocking_filter_CTB(HEVCContext *s, int x0, int y0)
{
    uint8_t *src;
    int x, y;
    int chroma, beta;
    int32_t c_tc[2], tc[2];
    uint8_t no_p[2] = { 0 };
    uint8_t no_q[2] = { 0 };

    int log2_ctb_size = s->ps.sps->log2_ctb_size;
    int x_end, x_end2, y_end;
    int ctb_size        = 1 << log2_ctb_size;
    int ctb             = (x0 >> log2_ctb_size) +
                          (y0 >> log2_ctb_size) * s->ps.sps->ctb_width;
    int cur_tc_offset   = s->deblock[ctb].tc_offset;
    int cur_beta_offset = s->deblock[ctb].beta_offset;
    int left_tc_offset, left_beta_offset;
    int tc_offset, beta_offset;
    int pcmf = (s->ps.sps->pcm_enabled_flag &&
                s->ps.sps->pcm.loop_filter_disable_flag) ||
               s->ps.pps->transquant_bypass_enable_flag;

#ifdef DISABLE_DEBLOCK_NONREF
    if (!s->used_for_ref)
      return; // Don't deblock non-reference frames
#endif
#ifdef DISABLE_DEBLOCK
    return;
#endif
    if (!s->used_for_ref && s->avctx->skip_loop_filter >= AVDISCARD_NONREF)
        return;
    if (x0) {
        left_tc_offset   = s->deblock[ctb - 1].tc_offset;
        left_beta_offset = s->deblock[ctb - 1].beta_offset;
    } else {
        left_tc_offset   = 0;
        left_beta_offset = 0;
    }

    x_end = x0 + ctb_size;
    if (x_end > s->ps.sps->width)
        x_end = s->ps.sps->width;
    y_end = y0 + ctb_size;
    if (y_end > s->ps.sps->height)
        y_end = s->ps.sps->height;

    tc_offset   = cur_tc_offset;
    beta_offset = cur_beta_offset;

    x_end2 = x_end;
    if (x_end2 != s->ps.sps->width)
        x_end2 -= 8;
    for (y = y0; y < y_end; y += 8) {
        // vertical filtering luma
        for (x = x0 ? x0 : 8; x < x_end; x += 8) {
            const int bs0 = s->vertical_bs[(x +  y      * s->bs_width) >> 2];
            const int bs1 = s->vertical_bs[(x + (y + 4) * s->bs_width) >> 2];
            if (bs0 || bs1) {
                const int qp = (get_qPy(s, x - 1, y)     + get_qPy(s, x, y)     + 1) >> 1;

                beta = betatable[av_clip(qp + beta_offset, 0, MAX_QP)];

                tc[0]   = bs0 ? TC_CALC(qp, bs0) : 0;
                tc[1]   = bs1 ? TC_CALC(qp, bs1) : 0;
                src     = &s->frame->data[LUMA][y * s->frame->linesize[LUMA] + (x << s->ps.sps->pixel_shift)];
                if (pcmf) {
                    no_p[0] = get_pcm(s, x - 1, y);
                    no_p[1] = get_pcm(s, x - 1, y + 4);
                    no_q[0] = get_pcm(s, x, y);
                    no_q[1] = get_pcm(s, x, y + 4);
                    s->hevcdsp.hevc_v_loop_filter_luma_c(src,
                                                         s->frame->linesize[LUMA],
                                                         beta, tc, no_p, no_q);
                } else
#ifdef RPI_DEBLOCK_VPU
                if (s->enable_rpi_deblock) {
                    uint8_t (*setup)[2][2][4];
                    int num16 = (y>>4)*s->setup_width + (x>>4);
                    int a = ((y>>3) & 1) << 1;
                    int b = (x>>3) & 1;
                    setup = s->dvq->y_setup_arm[num16];
                    setup[0][b][0][a] = beta;
                    setup[0][b][0][a + 1] = beta;
                    setup[0][b][1][a] = tc[0];
                    setup[0][b][1][a + 1] = tc[1];
                } else
#endif
                    s->hevcdsp.hevc_v_loop_filter_luma(src,
                                                       s->frame->linesize[LUMA],
                                                       beta, tc, no_p, no_q);
            }
        }

        if(!y)
             continue;

        // horizontal filtering luma
        for (x = x0 ? x0 - 8 : 0; x < x_end2; x += 8) {
            const int bs0 = s->horizontal_bs[( x      + y * s->bs_width) >> 2];
            const int bs1 = s->horizontal_bs[((x + 4) + y * s->bs_width) >> 2];
            if (bs0 || bs1) {
                const int qp = (get_qPy(s, x, y - 1)     + get_qPy(s, x, y)     + 1) >> 1;

                tc_offset   = x >= x0 ? cur_tc_offset : left_tc_offset;
                beta_offset = x >= x0 ? cur_beta_offset : left_beta_offset;

                beta = betatable[av_clip(qp + beta_offset, 0, MAX_QP)];
                tc[0]   = bs0 ? TC_CALC(qp, bs0) : 0;
                tc[1]   = bs1 ? TC_CALC(qp, bs1) : 0;
                src     = &s->frame->data[LUMA][y * s->frame->linesize[LUMA] + (x << s->ps.sps->pixel_shift)];
                if (pcmf) {
                    no_p[0] = get_pcm(s, x, y - 1);
                    no_p[1] = get_pcm(s, x + 4, y - 1);
                    no_q[0] = get_pcm(s, x, y);
                    no_q[1] = get_pcm(s, x + 4, y);
                    s->hevcdsp.hevc_h_loop_filter_luma_c(src,
                                                         s->frame->linesize[LUMA],
                                                         beta, tc, no_p, no_q);
                } else
#ifdef RPI_DEBLOCK_VPU
                if (s->enable_rpi_deblock) {
                    uint8_t (*setup)[2][2][4];
                    int num16 = (y>>4)*s->setup_width + (x>>4);
                    int a = ((x>>3) & 1) << 1;
                    int b = (y>>3) & 1;
                    setup = s->dvq->y_setup_arm[num16];
                    setup[1][b][0][a] = beta;
                    setup[1][b][0][a + 1] = beta;
                    setup[1][b][1][a] = tc[0];
                    setup[1][b][1][a + 1] = tc[1];
                } else
#endif
                    s->hevcdsp.hevc_h_loop_filter_luma(src,
                                                       s->frame->linesize[LUMA],
                                                       beta, tc, no_p, no_q);
            }
        }
    }

    if (s->ps.sps->chroma_format_idc) {
        for (chroma = 1; chroma <= 2; chroma++) {
            int h = 1 << s->ps.sps->hshift[chroma];
            int v = 1 << s->ps.sps->vshift[chroma];

            // vertical filtering chroma
            for (y = y0; y < y_end; y += (8 * v)) {
                for (x = x0 ? x0 : 8 * h; x < x_end; x += (8 * h)) {
                    const int bs0 = s->vertical_bs[(x +  y            * s->bs_width) >> 2];
                    const int bs1 = s->vertical_bs[(x + (y + (4 * v)) * s->bs_width) >> 2];

                    if ((bs0 == 2) || (bs1 == 2)) {
                        const int qp0 = (get_qPy(s, x - 1, y)           + get_qPy(s, x, y)           + 1) >> 1;
                        const int qp1 = (get_qPy(s, x - 1, y + (4 * v)) + get_qPy(s, x, y + (4 * v)) + 1) >> 1;

                        c_tc[0] = (bs0 == 2) ? chroma_tc(s, qp0, chroma, tc_offset) : 0;
                        c_tc[1] = (bs1 == 2) ? chroma_tc(s, qp1, chroma, tc_offset) : 0;
                        src       = &s->frame->data[chroma][(y >> s->ps.sps->vshift[chroma]) * s->frame->linesize[chroma] + ((x >> s->ps.sps->hshift[chroma]) << s->ps.sps->pixel_shift)];
                        if (pcmf) {
                            no_p[0] = get_pcm(s, x - 1, y);
                            no_p[1] = get_pcm(s, x - 1, y + (4 * v));
                            no_q[0] = get_pcm(s, x, y);
                            no_q[1] = get_pcm(s, x, y + (4 * v));
                            s->hevcdsp.hevc_v_loop_filter_chroma_c(src,
                                                                   s->frame->linesize[chroma],
                                                                   c_tc, no_p, no_q);
                        } else
#ifdef RPI_DEBLOCK_VPU
                        if (s->enable_rpi_deblock) {
                            uint8_t (*setup)[2][2][4];
                            int xc = x>>s->ps.sps->hshift[chroma];
                            int yc = y>>s->ps.sps->vshift[chroma];
                            int num16 = (yc>>4)*s->uv_setup_width + (xc>>4);
                            int a = ((yc>>3) & 1) << 1;
                            int b = (xc>>3) & 1;
                            setup = s->dvq->uv_setup_arm[num16];
                            setup[0][b][0][a] = c_tc[0];
                            setup[0][b][0][a + 1] = c_tc[1];
                        } else
#endif
                            s->hevcdsp.hevc_v_loop_filter_chroma(src,
                                                                 s->frame->linesize[chroma],
                                                                 c_tc, no_p, no_q);

                    }
                }

                if(!y)
                    continue;

                // horizontal filtering chroma
                tc_offset = x0 ? left_tc_offset : cur_tc_offset;
                x_end2 = x_end;
                if (x_end != s->ps.sps->width)
                    x_end2 = x_end - 8 * h;
                for (x = x0 ? x0 - 8 * h : 0; x < x_end2; x += (8 * h)) {
                    const int bs0 = s->horizontal_bs[( x          + y * s->bs_width) >> 2];
                    const int bs1 = s->horizontal_bs[((x + 4 * h) + y * s->bs_width) >> 2];
                    if ((bs0 == 2) || (bs1 == 2)) {
                        const int qp0 = bs0 == 2 ? (get_qPy(s, x,           y - 1) + get_qPy(s, x,           y) + 1) >> 1 : 0;
                        const int qp1 = bs1 == 2 ? (get_qPy(s, x + (4 * h), y - 1) + get_qPy(s, x + (4 * h), y) + 1) >> 1 : 0;

                        c_tc[0]   = bs0 == 2 ? chroma_tc(s, qp0, chroma, tc_offset)     : 0;
                        c_tc[1]   = bs1 == 2 ? chroma_tc(s, qp1, chroma, cur_tc_offset) : 0;
                        src       = &s->frame->data[chroma][(y >> s->ps.sps->vshift[1]) * s->frame->linesize[chroma] + ((x >> s->ps.sps->hshift[1]) << s->ps.sps->pixel_shift)];
                        if (pcmf) {
                            no_p[0] = get_pcm(s, x,           y - 1);
                            no_p[1] = get_pcm(s, x + (4 * h), y - 1);
                            no_q[0] = get_pcm(s, x,           y);
                            no_q[1] = get_pcm(s, x + (4 * h), y);
                            s->hevcdsp.hevc_h_loop_filter_chroma_c(src,
                                                                   s->frame->linesize[chroma],
                                                                   c_tc, no_p, no_q);
                        } else
#ifdef RPI_DEBLOCK_VPU
                        if (s->enable_rpi_deblock) {
                            uint8_t (*setup)[2][2][4];
                            int xc = x>>s->ps.sps->hshift[chroma];
                            int yc = y>>s->ps.sps->vshift[chroma];
                            int num16 = (yc>>4)*s->uv_setup_width + (xc>>4);
                            int a = ((xc>>3) & 1) << 1;
                            int b = (yc>>3) & 1;
                            setup = s->dvq->uv_setup_arm[num16];
                            setup[1][b][0][a] = c_tc[0];
                            setup[1][b][0][a + 1] = c_tc[1];
                        } else
#endif
                            s->hevcdsp.hevc_h_loop_filter_chroma(src,
                                                                 s->frame->linesize[chroma],
                                                                 c_tc, no_p, no_q);
                    }
                }
            }
        }
    }
}


void ff_hevc_deblocking_boundary_strengths(HEVCContext *s, int x0, int y0,
                                           int log2_trafo_size)
{
    HEVCLocalContext *lc = s->HEVClc;
    MvField *tab_mvf     = s->ref->tab_mvf;
    int log2_min_pu_size = s->ps.sps->log2_min_pu_size;
    int log2_min_tu_size = s->ps.sps->log2_min_tb_size;
    int min_pu_width     = s->ps.sps->min_pu_width;
    int min_tu_width     = s->ps.sps->min_tb_width;
    int boundary_upper, boundary_left;
    int i, j;
    RefPicList *rpl      = s->ref->refPicList;
    int min_pu_in_4pix   = (1 << log2_min_pu_size) >> 2;
    int trafo_in_min_pus = (1 << log2_trafo_size) >> log2_min_pu_size;
    int y_pu             = y0 >> log2_min_pu_size;
    int x_pu             = x0 >> log2_min_pu_size;
    MvField *curr        = &tab_mvf[y_pu * min_pu_width + x_pu];
    int is_intra         = curr->pred_flag == PF_INTRA;
    int inc              = log2_min_pu_size == 2 ? 2 : 1;
    uint8_t *bs;

#ifdef DISABLE_STRENGTHS
    return;
#endif

    boundary_upper = y0 > 0 && !(y0 & 7);
    if (boundary_upper &&
        ((!s->sh.slice_loop_filter_across_slices_enabled_flag &&
          lc->boundary_flags & BOUNDARY_UPPER_SLICE &&
          (y0 % (1 << s->ps.sps->log2_ctb_size)) == 0) ||
         (!s->ps.pps->loop_filter_across_tiles_enabled_flag &&
          lc->boundary_flags & BOUNDARY_UPPER_TILE &&
          (y0 % (1 << s->ps.sps->log2_ctb_size)) == 0)))
        boundary_upper = 0;

    bs = &s->horizontal_bs[(x0 + y0 * s->bs_width) >> 2];

    if (boundary_upper) {
        RefPicList *rpl_top = (lc->boundary_flags & BOUNDARY_UPPER_SLICE) ?
                              ff_hevc_get_ref_list(s, s->ref, x0, y0 - 1) :
                              rpl;
        MvField *top = curr - min_pu_width;

        if (is_intra) {
            for (i = 0; i < (1 << log2_trafo_size); i += 4)
                bs[i >> 2] = 2;

        } else {
            int y_tu = y0 >> log2_min_tu_size;
            int x_tu = x0 >> log2_min_tu_size;
            uint8_t *curr_cbf_luma = &s->cbf_luma[y_tu * min_tu_width + x_tu];
            uint8_t *top_cbf_luma = curr_cbf_luma - min_tu_width;

            s->hevcdsp.hevc_deblocking_boundary_strengths(trafo_in_min_pus,
                    min_pu_in_4pix, sizeof (MvField), 4 >> 2,
                    rpl[0].list, rpl[1].list, rpl_top[0].list, rpl_top[1].list,
                    curr, top, bs);

            for (i = 0; i < (1 << log2_trafo_size); i += 4) {
                int i_pu = i >> log2_min_pu_size;
                int i_tu = i >> log2_min_tu_size;

                if (top[i_pu].pred_flag == PF_INTRA)
                    bs[i >> 2] = 2;
                else if (curr_cbf_luma[i_tu] || top_cbf_luma[i_tu])
                    bs[i >> 2] = 1;
            }
        }
    }

    if (!is_intra) {
        for (j = inc; j < trafo_in_min_pus; j += inc) {
            MvField *top;

            curr += min_pu_width * inc;
            top = curr - min_pu_width;
            bs += s->bs_width * inc << log2_min_pu_size >> 2;

            s->hevcdsp.hevc_deblocking_boundary_strengths(trafo_in_min_pus,
                    min_pu_in_4pix, sizeof (MvField), 4 >> 2,
                    rpl[0].list, rpl[1].list, rpl[0].list, rpl[1].list,
                    curr, top, bs);
        }
    }

    boundary_left = x0 > 0 && !(x0 & 7);
    if (boundary_left &&
        ((!s->sh.slice_loop_filter_across_slices_enabled_flag &&
          lc->boundary_flags & BOUNDARY_LEFT_SLICE &&
          (x0 % (1 << s->ps.sps->log2_ctb_size)) == 0) ||
         (!s->ps.pps->loop_filter_across_tiles_enabled_flag &&
          lc->boundary_flags & BOUNDARY_LEFT_TILE &&
          (x0 % (1 << s->ps.sps->log2_ctb_size)) == 0)))
        boundary_left = 0;

    curr = &tab_mvf[y_pu * min_pu_width + x_pu];
    bs = &s->vertical_bs[(x0 + y0 * s->bs_width) >> 2];

    if (boundary_left) {
        RefPicList *rpl_left = (lc->boundary_flags & BOUNDARY_LEFT_SLICE) ?
                               ff_hevc_get_ref_list(s, s->ref, x0 - 1, y0) :
                               rpl;
        MvField *left = curr - 1;

        if (is_intra) {
            for (j = 0; j < (1 << log2_trafo_size); j += 4)
                bs[j * s->bs_width >> 2] = 2;

        } else {
            int y_tu = y0 >> log2_min_tu_size;
            int x_tu = x0 >> log2_min_tu_size;
            uint8_t *curr_cbf_luma = &s->cbf_luma[y_tu * min_tu_width + x_tu];
            uint8_t *left_cbf_luma = curr_cbf_luma - 1;

            s->hevcdsp.hevc_deblocking_boundary_strengths(trafo_in_min_pus,
                    min_pu_in_4pix, min_pu_width * sizeof (MvField), 4 * s->bs_width >> 2,
                    rpl[0].list, rpl[1].list, rpl_left[0].list, rpl_left[1].list,
                    curr, left, bs);

            for (j = 0; j < (1 << log2_trafo_size); j += 4) {
                int j_pu = j >> log2_min_pu_size;
                int j_tu = j >> log2_min_tu_size;

                if (left[j_pu * min_pu_width].pred_flag == PF_INTRA)
                    bs[j * s->bs_width >> 2] = 2;
                else if (curr_cbf_luma[j_tu * min_tu_width] || left_cbf_luma[j_tu * min_tu_width])
                    bs[j * s->bs_width >> 2] = 1;
            }
        }
    }

    if (!is_intra) {
        for (i = inc; i < trafo_in_min_pus; i += inc) {
            MvField *left;

            curr += inc;
            left = curr - 1;
            bs += inc << log2_min_pu_size >> 2;

            s->hevcdsp.hevc_deblocking_boundary_strengths(trafo_in_min_pus,
                    min_pu_in_4pix, min_pu_width * sizeof (MvField), 4 * s->bs_width >> 2,
                    rpl[0].list, rpl[1].list, rpl[0].list, rpl[1].list,
                    curr, left, bs);
        }
    }
}

#undef LUMA
#undef CB
#undef CR

#ifdef RPI_DEBLOCK_VPU
// ff_hevc_flush_buffer_lines
// flushes and invalidates all pixel rows in [start,end-1]
static void ff_hevc_flush_buffer_lines(HEVCContext *s, int start, int end, int flush_luma, int flush_chroma)
{
    rpi_cache_flush_env_t * const rfe = rpi_cache_flush_init();
    rpi_cache_flush_add_frame_lines(rfe, s->frame, RPI_CACHE_FLUSH_MODE_WB_INVALIDATE,
      start, end - start, s->ps.sps->vshift[1], flush_luma, flush_chroma);
    rpi_cache_flush_finish(rfe);
}
#endif

#if RPI_MC_CHROMA_QPU || RPI_MC_LUMA_QPU

// Flush some lines of a reference frames
void rpi_flush_ref_frame_progress(HEVCContext *s, ThreadFrame *f, int n)
{
    if (s->enable_rpi && s->used_for_ref) {
        const int curr_y = ((int *)f->progress->data)[0];
        rpi_cache_flush_env_t * const rfe = rpi_cache_flush_init();
        rpi_cache_flush_add_frame_lines(rfe, s->frame, RPI_CACHE_FLUSH_MODE_WB_INVALIDATE,
          curr_y, n - curr_y, s->ps.sps->vshift[1], RPI_MC_LUMA_QPU, RPI_MC_CHROMA_QPU);
        rpi_cache_flush_finish(rfe);
    }
}
#endif

#ifdef RPI_DEBLOCK_VPU
/* rpi_deblock deblocks an entire row of ctbs using the VPU */
static void rpi_deblock(HEVCContext *s, int y, int ctb_size)
{
  // Flush image, 4 lines above to bottom of ctb stripe
  ff_hevc_flush_buffer_lines(s, FFMAX(y-4,0), y+ctb_size, 1, 1);
  // TODO flush buffer of beta/tc setup when it becomes cached

  // Prepare three commands at once to avoid calling overhead
  s->dvq->vpu_cmds_arm[0][0] = get_vc_address_y(s->frame) + s->frame->linesize[0] * y;
  s->dvq->vpu_cmds_arm[0][1] = s->frame->linesize[0];
  s->dvq->vpu_cmds_arm[0][2] = s->setup_width;
  s->dvq->vpu_cmds_arm[0][3] = (int) ( s->dvq->y_setup_vc + s->setup_width * (y>>4) );
  s->dvq->vpu_cmds_arm[0][4] = ctb_size>>4;
  s->dvq->vpu_cmds_arm[0][5] = 2;

  s->dvq->vpu_cmds_arm[1][0] = get_vc_address_u(s->frame) + s->frame->linesize[1] * (y>> s->ps.sps->vshift[1]);
  s->dvq->vpu_cmds_arm[1][1] = s->frame->linesize[1];
  s->dvq->vpu_cmds_arm[1][2] = s->uv_setup_width;
  s->dvq->vpu_cmds_arm[1][3] = (int) ( s->dvq->uv_setup_vc + s->uv_setup_width * ((y>>4)>> s->ps.sps->vshift[1]) );
  s->dvq->vpu_cmds_arm[1][4] = (ctb_size>>4)>> s->ps.sps->vshift[1];
  s->dvq->vpu_cmds_arm[1][5] = 3;

  s->dvq->vpu_cmds_arm[2][0] = get_vc_address_v(s->frame) + s->frame->linesize[2] * (y>> s->ps.sps->vshift[2]);
  s->dvq->vpu_cmds_arm[2][1] = s->frame->linesize[2];
  s->dvq->vpu_cmds_arm[2][2] = s->uv_setup_width;
  s->dvq->vpu_cmds_arm[2][3] = (int) ( s->dvq->uv_setup_vc + s->uv_setup_width * ((y>>4)>> s->ps.sps->vshift[1]) );
  s->dvq->vpu_cmds_arm[2][4] = (ctb_size>>4)>> s->ps.sps->vshift[1];
  s->dvq->vpu_cmds_arm[2][5] = 4;
  // Call VPU
  vpu_qpu_post_code2( vpu_get_fn(), s->dvq->vpu_cmds_vc, 3, 0, 0, 0, 5, // 5 means to do all the commands
                     0, NULL, 0, NULL, &s->dvq->cmd_id);

  s->dvq_n = (s->dvq_n + 1) & (RPI_DEBLOCK_VPU_Q_COUNT - 1);
  s->dvq = s->dvq_ents + s->dvq_n;

  sem_wait(&s->dvq->cmd_id);
}

#endif

void ff_hevc_hls_filter(HEVCContext *s, int x, int y, int ctb_size)
{
    int x_end = x >= s->ps.sps->width  - ctb_size;
#ifdef RPI_DEBLOCK_VPU
    int done_deblock = 0;
#endif
    if (s->avctx->skip_loop_filter < AVDISCARD_ALL)
        deblocking_filter_CTB(s, x, y);
#ifdef RPI_DEBLOCK_VPU
    if (s->enable_rpi_deblock && x_end)
    {
      int y_at_end = y >= s->ps.sps->height - ctb_size;
      int height = 64;  // Deblock in units 64 high to avoid too many VPU calls
      int y_start = y&~63;
      if (y_at_end) height = s->ps.sps->height - y_start;
      if ((((y+ctb_size)&63)==0) || y_at_end) {
        done_deblock = 1;
        rpi_deblock(s, y_start, height);
      }
    }
#endif
    if (s->ps.sps->sao_enabled) {
        int y_end = y >= s->ps.sps->height - ctb_size;
        if (y && x)
            sao_filter_CTB(s, x - ctb_size, y - ctb_size);
        if (x && y_end)
            sao_filter_CTB(s, x - ctb_size, y);
        if (y && x_end) {
            sao_filter_CTB(s, x, y - ctb_size);
            if (s->threads_type == FF_THREAD_FRAME ) {
#if RPI_MC_CHROMA_QPU || RPI_MC_LUMA_QPU
                rpi_flush_ref_frame_progress(s,&s->ref->tf, y);
#endif
                ff_thread_report_progress(&s->ref->tf, y, 0);
            }
        }
        if (x_end && y_end) {
            sao_filter_CTB(s, x , y);
            if (s->threads_type == FF_THREAD_FRAME ) {
#if RPI_MC_CHROMA_QPU || RPI_MC_LUMA_QPU
                rpi_flush_ref_frame_progress(s, &s->ref->tf, y + ctb_size);
#endif
                ff_thread_report_progress(&s->ref->tf, y + ctb_size, 0);
            }
        }
    } else if (s->threads_type == FF_THREAD_FRAME && x_end) {
        //int newh = y + ctb_size - 4;
        //int currh = s->ref->tf.progress->data[0];
        //if (((y + ctb_size)&63)==0)
#ifdef RPI_DEBLOCK_VPU
        if (s->enable_rpi_deblock) {
          // we no longer need to flush the luma buffer as it is in GPU memory when using deblocking on the rpi
          if (done_deblock) {
            ff_thread_report_progress(&s->ref->tf, y + ctb_size - 4, 0);
          }
        } else {
#if RPI_MC_CHROMA_QPU || RPI_MC_LUMA_QPU
          rpi_flush_ref_frame_progress(s, &s->ref->tf, y + ctb_size - 4);
#endif
          ff_thread_report_progress(&s->ref->tf, y + ctb_size - 4, 0);
        }
#else
#if RPI_MC_CHROMA_QPU || RPI_MC_LUMA_QPU
        rpi_flush_ref_frame_progress(s, &s->ref->tf, y + ctb_size - 4);
        // we no longer need to flush the luma buffer as it is in GPU memory when using deblocking on the rpi
#endif
        ff_thread_report_progress(&s->ref->tf, y + ctb_size - 4, 0);
#endif
    }
}

void ff_hevc_hls_filters(HEVCContext *s, int x_ctb, int y_ctb, int ctb_size)
{
    int x_end = x_ctb >= s->ps.sps->width  - ctb_size;
    int y_end = y_ctb >= s->ps.sps->height - ctb_size;
    if (y_ctb && x_ctb)
        ff_hevc_hls_filter(s, x_ctb - ctb_size, y_ctb - ctb_size, ctb_size);
    if (y_ctb && x_end)
        ff_hevc_hls_filter(s, x_ctb, y_ctb - ctb_size, ctb_size);
    if (x_ctb && y_end)
        ff_hevc_hls_filter(s, x_ctb - ctb_size, y_ctb, ctb_size);
}
