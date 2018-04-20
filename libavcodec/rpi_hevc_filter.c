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
#include "rpi_hevcdec.h"

#include "bit_depth_template.c"

#include "rpi_qpu.h"
#include "rpi_zc.h"
#include "libavutil/rpi_sand_fns.h"

#define LUMA 0
#define CB 1
#define CR 2

// tcoffset: -12,12; qp: 0,51; (bs-1)*2: 0,2
// so -12,75 overall
static const uint8_t tctablex[] = {
    0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  // -ve quant padding
    0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,
    0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,
    0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,

    0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,                          // -12..-1
    0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 0, 0, 1, // QP  0...18
    1, 1, 1, 1, 1, 1, 1,  1,  2,  2,  2,  2,  3,  3,  3,  3, 4, 4, 4, // QP 19...37
    5, 5, 6, 6, 7, 8, 9, 10, 11, 13, 14, 16, 18, 20, 22, 24,          // QP 38...53
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24                    // 54..75
};
#define tctable (tctablex + 12 + 6*8)

static const uint8_t betatablex[] = {
    0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  // -ve quant padding
    0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,
    0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,
    0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,

    0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,                          // -12..-1
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  6,  7,  8, // QP 0...18
     9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, // QP 19...37
    38, 40, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62, 64,                      // QP 38...51
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64                    // 52..73
};
#define betatable (betatablex + 12 + 6*8)

static inline int chroma_tc(const HEVCRpiContext * const s, const int qp_y,
                            const int c_idx, const int tc_offset)
{
    return tctable[(int)s->ps.pps->qp_dblk_x[c_idx][qp_y] + tc_offset + 2];
}

static inline int get_qPy_pred(const HEVCRpiContext * const s, const HEVCRpiLocalContext * const lc,
                               const unsigned int xBase, const unsigned int yBase)
{
    const unsigned int ctb_size_mask        = (1 << s->ps.sps->log2_ctb_size) - 1;
    const unsigned int MinCuQpDeltaSizeMask = ~0U << s->ps.pps->log2_min_cu_qp_delta_size;
    const unsigned int xQgBase              = xBase & MinCuQpDeltaSizeMask;
    const unsigned int yQgBase              = yBase & MinCuQpDeltaSizeMask;
    const unsigned int min_cb_width         = s->ps.sps->min_cb_width;
    const unsigned int x_cb                 = xQgBase >> s->ps.sps->log2_min_cb_size;
    const unsigned int y_cb                 = yQgBase >> s->ps.sps->log2_min_cb_size;
    const int qPy_pred = lc->qPy_pred;

    return (((xQgBase & ctb_size_mask) == 0 ? qPy_pred :
             s->qp_y_tab[(x_cb - 1) + y_cb * min_cb_width]) +
            ((yQgBase & ctb_size_mask) == 0 ? qPy_pred :
             s->qp_y_tab[x_cb + (y_cb - 1) * min_cb_width]) + 1) >> 1;
}

// * Only called from bitstream decode in foreground
//   so should be safe
void ff_hevc_rpi_set_qPy(const HEVCRpiContext * const s, HEVCRpiLocalContext * const lc, int xBase, int yBase)
{
    const int qp_y = get_qPy_pred(s, lc, xBase, yBase);

    if (lc->tu.cu_qp_delta != 0) {
        // ?? I suspect that the -bd_offset here leads to us adding it elsewhere
        int off = s->ps.sps->qp_bd_offset;
        lc->qp_y = FFUMOD(qp_y + lc->tu.cu_qp_delta + 52 + 2 * off,
                                 52 + off) - off;
    } else
        lc->qp_y = qp_y;
}

static inline unsigned int pixel_shift(const HEVCRpiContext * const s, const unsigned int c_idx)
{
    return c_idx != 0 ? 1 + s->ps.sps->pixel_shift : s->ps.sps->pixel_shift;
}

static void copy_CTB(uint8_t *dst, const uint8_t *src, int width, int height,
                     ptrdiff_t stride_dst, ptrdiff_t stride_src)
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

// "DSP" these?
static void copy_pixel(uint8_t *dst, const uint8_t *src, int pixel_shift)
{
    switch (pixel_shift)
    {
        case 2:
            *(uint32_t *)dst = *(uint32_t *)src;
            break;
        case 1:
            *(uint16_t *)dst = *(uint16_t *)src;
            break;
        default:
            *dst = *src;
            break;
    }
}

static void copy_vert(uint8_t *dst, const uint8_t *src,
                      int pixel_shift, int height,
                      ptrdiff_t stride_dst, ptrdiff_t stride_src)
{
    int i;
    switch (pixel_shift)
    {
        case 2:
            for (i = 0; i < height; i++) {
                *(uint32_t *)dst = *(uint32_t *)src;
                dst += stride_dst;
                src += stride_src;
            }
            break;
        case 1:
            for (i = 0; i < height; i++) {
                *(uint16_t *)dst = *(uint16_t *)src;
                dst += stride_dst;
                src += stride_src;
            }
            break;
        default:
            for (i = 0; i < height; i++) {
                *dst = *src;
                dst += stride_dst;
                src += stride_src;
            }
            break;
    }
}

static void copy_CTB_to_hv(const HEVCRpiContext * const s, const uint8_t * const src,
                           ptrdiff_t stride_src, int x, int y, int width, int height,
                           int c_idx, int x_ctb, int y_ctb)
{
    const unsigned int sh = pixel_shift(s, c_idx);
    const unsigned int w = s->ps.sps->width >> ctx_hshift(s, c_idx);
    const unsigned int h = s->ps.sps->height >> ctx_vshift(s, c_idx);

    /* copy horizontal edges */
    memcpy(s->sao_pixel_buffer_h[c_idx] + (((2 * y_ctb) * w + x) << sh),
        src, width << sh);
    memcpy(s->sao_pixel_buffer_h[c_idx] + (((2 * y_ctb + 1) * w + x) << sh),
        src + stride_src * (height - 1), width << sh);

    /* copy vertical edges */
    copy_vert(s->sao_pixel_buffer_v[c_idx] + (((2 * x_ctb) * h + y) << sh), src, sh, height, 1 << sh, stride_src);

    copy_vert(s->sao_pixel_buffer_v[c_idx] + (((2 * x_ctb + 1) * h + y) << sh), src + ((width - 1) << sh), sh, height, 1 << sh, stride_src);
}

// N.B. Src & dst are swapped as this is a restore!
// x0 & y0 are in luma coords
// Width & height are in Y/C pels as appropriate
// * Clear scope for optimsation here but not used enough to be worth it
static void restore_tqb_pixels(const HEVCRpiContext * const s,
                               uint8_t *src1, const uint8_t *dst1,
                               const ptrdiff_t stride_src, const ptrdiff_t stride_dst,
                               const unsigned int x0, const unsigned int y0,
                               const unsigned int width, const int height,
                               const int c_idx)
{
    if (s->ps.pps->transquant_bypass_enable_flag ||
        s->ps.sps->pcm.loop_filter_disable_flag)
    {
        const uint8_t *pcm = s->is_pcm + (x0 >> 6) + (y0 >> 3) * s->ps.sps->pcm_width;
        int blks_y = height >> (c_idx == 0 ? 3 : 2);
        const unsigned int bwidth = 8 << s->ps.sps->pixel_shift;  // Y & C have the same width in sand
        const unsigned int bheight = (c_idx == 0) ? 8 : 4;
        const unsigned int sh = ((x0 >> 3) & 7);
        const unsigned int mask = (1 << (width >> (c_idx == 0 ? 3 : 2))) - 1;

        do {
            unsigned int m = (*pcm >> sh) & mask;
            uint8_t * bd = src1;
            const uint8_t * bs = dst1;
            while (m != 0) {
                if ((m & 1) != 0) {
                    unsigned int i;
                    uint8_t * d = bd;
                    const uint8_t * s = bs;
                    for (i = 0; i != bheight; ++i) {
                        memcpy(d, s, bwidth);
                        d += stride_src;
                        s += stride_dst;
                    }
                }
                m >>= 1;
                bs += bwidth;
                bd += bwidth;
            }
            src1 += stride_src * bheight;
            dst1 += stride_dst * bheight;
            pcm += s->ps.sps->pcm_width;
        } while (--blks_y > 0);
    }
}

#define CTB(tab, x, y) ((tab)[(y) * s->ps.sps->ctb_width + (x)])

static void sao_filter_CTB(const HEVCRpiContext * const s, const int x, const int y)
{
#if SAO_FILTER_N == 5
    static const uint8_t sao_tab[8] = { 0 /* 8 */, 1 /* 16 */, 2 /* 24 */, 2 /* 32 */, 3, 3 /* 48 */, 4, 4 /* 64 */};
#elif SAO_FILTER_N == 6
    static const uint8_t sao_tab[8] = { 0 /* 8 */, 1 /* 16 */, 5 /* 24 */, 2 /* 32 */, 3, 3 /* 48 */, 4, 4 /* 64 */};
#else
#error Confused by size of sao fn array
#endif
    int c_idx;
    int edges[4];  // 0 left 1 top 2 right 3 bottom
    int x_ctb                = x >> s->ps.sps->log2_ctb_size;
    int y_ctb                = y >> s->ps.sps->log2_ctb_size;
    int ctb_addr_rs          = y_ctb * s->ps.sps->ctb_width + x_ctb;
    int ctb_addr_ts          = s->ps.pps->ctb_addr_rs_to_ts[ctb_addr_rs];
    RpiSAOParams *sao           = &CTB(s->sao, x_ctb, y_ctb);
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
    const int sliced = 1;
    const int plane_count = sliced ? 2 : (ctx_cfmt(s) != 0 ? 3 : 1);

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

    for (c_idx = 0; c_idx < plane_count; c_idx++) {
        const unsigned int vshift = ctx_vshift(s, c_idx);
        const unsigned int hshift = ctx_hshift(s, c_idx);
        const int x0 = x >> hshift;
        const int y0 = y >> vshift;
        const ptrdiff_t stride_src = frame_stride1(s->frame, c_idx);
        const int ctb_size_h = (1 << (s->ps.sps->log2_ctb_size)) >> hshift;
        const int ctb_size_v = (1 << (s->ps.sps->log2_ctb_size)) >> vshift;
        const int width    = FFMIN(ctb_size_h, (s->ps.sps->width  >> hshift) - x0);
        const int height = FFMIN(ctb_size_v, (s->ps.sps->height >> vshift) - y0);
        int tab      = sao_tab[(FFALIGN(width, 8) >> 3) - 1];
        ptrdiff_t stride_dst;
        uint8_t *dst;

        const unsigned int sh = s->ps.sps->pixel_shift + (sliced && c_idx != 0);
        const int wants_lr = sao->type_idx[c_idx] == SAO_EDGE && sao->eo_class[c_idx] != 1 /* Vertical */;
        uint8_t * const src = !sliced ?
                &s->frame->data[c_idx][y0 * stride_src + (x0 << sh)] :
            c_idx == 0 ?
                av_rpi_sand_frame_pos_y(s->frame, x0, y0) :
                av_rpi_sand_frame_pos_c(s->frame, x0, y0);
        const uint8_t * const src_l = edges[0] || !wants_lr ? NULL :
            !sliced ? src - (1 << sh) :
            c_idx == 0 ?
                av_rpi_sand_frame_pos_y(s->frame, x0 - 1, y0) :
                av_rpi_sand_frame_pos_c(s->frame, x0 - 1, y0);
        const uint8_t * const src_r = edges[2] || !wants_lr ? NULL :
            !sliced ? src + (width << sh) :
            c_idx == 0 ?
                av_rpi_sand_frame_pos_y(s->frame, x0 + width, y0) :
                av_rpi_sand_frame_pos_c(s->frame, x0 + width, y0);

        if (sliced && c_idx > 1) {
            break;
        }

//        if (c_idx == 1)
//            printf("%d: %dx%d %d,%d: lr=%d\n", c_idx, width, height, x0, y0, wants_lr);

        switch (sao->type_idx[c_idx]) {
        case SAO_BAND:
            copy_CTB_to_hv(s, src, stride_src, x0, y0, width, height, c_idx,
                           x_ctb, y_ctb);
            if (s->ps.pps->transquant_bypass_enable_flag ||
                s->ps.sps->pcm.loop_filter_disable_flag)
            {
                // Can't use the edge buffer here as it may be in use by the foreground
                DECLARE_ALIGNED(64, uint8_t, dstbuf)
                    [2*MAX_PB_SIZE*MAX_PB_SIZE];
                dst = dstbuf;
                stride_dst = 2*MAX_PB_SIZE;
                copy_CTB(dst, src, width << sh, height, stride_dst, stride_src);
                if (sliced && c_idx != 0)
                {
                    s->hevcdsp.sao_band_filter_c[tab](src, dst, stride_src, stride_dst,
                                                    sao->offset_val[1], sao->band_position[1],
                                                    sao->offset_val[2], sao->band_position[2],
                                                    width, height);
                }
                else
                {
                    s->hevcdsp.sao_band_filter[tab](src, dst, stride_src, stride_dst,
                                                    sao->offset_val[c_idx], sao->band_position[c_idx],
                                                    width, height);
                }
                restore_tqb_pixels(s, src, dst, stride_src, stride_dst,
                                   x, y, width, height, c_idx);
            } else {
                if (sliced && c_idx != 0)
                {
                    s->hevcdsp.sao_band_filter_c[tab](src, src, stride_src, stride_src,
                                                    sao->offset_val[1], sao->band_position[1],
                                                    sao->offset_val[2], sao->band_position[2],
                                                    width, height);
                }
                else
                {
                    s->hevcdsp.sao_band_filter[tab](src, src, stride_src, stride_src,
                                                    sao->offset_val[c_idx], sao->band_position[c_idx],
                                                    width, height);
                }
            }
            sao->type_idx[c_idx] = SAO_APPLIED;
            break;
        case SAO_EDGE:
        {
            const int w = s->ps.sps->width >> hshift;
            const int h = s->ps.sps->height >> vshift;
            int top_edge = edges[1];
            int bottom_edge = edges[3];
            // Can't use the edge buffer here as it may be in use by the foreground
            DECLARE_ALIGNED(64, uint8_t, dstbuf)
                [2*(MAX_PB_SIZE + AV_INPUT_BUFFER_PADDING_SIZE)*(MAX_PB_SIZE + 2) + 64];

            stride_dst = 2*MAX_PB_SIZE + AV_INPUT_BUFFER_PADDING_SIZE;
            dst = dstbuf + stride_dst + AV_INPUT_BUFFER_PADDING_SIZE;

            if (!top_edge) {
                uint8_t *dst1;
                int src_idx;
                const uint8_t * const src_spb = s->sao_pixel_buffer_h[c_idx] + (((2 * y_ctb - 1) * w + x0) << sh);

                dst1 = dst - stride_dst;

                if (src_l != NULL) {
                    src_idx = (CTB(s->sao, x_ctb-1, y_ctb-1).type_idx[c_idx] ==
                               SAO_APPLIED);
                    copy_pixel(dst1 - (1 << sh), src_idx ? src_spb - (1 << sh) : src_l - stride_src, sh);
                }

                src_idx = (CTB(s->sao, x_ctb, y_ctb-1).type_idx[c_idx] ==
                           SAO_APPLIED);
                memcpy(dst1, src_idx ? src_spb : src - stride_src, width << sh);

                if (src_r != NULL) {
                    src_idx = (CTB(s->sao, x_ctb+1, y_ctb-1).type_idx[c_idx] ==
                               SAO_APPLIED);
                    copy_pixel(dst1 + (width << sh), src_idx ? src_spb + (width << sh) : src_r - stride_src, sh);
                }
            }
            if (!bottom_edge) {
                uint8_t * const dst1 = dst + height * stride_dst;
                int src_idx;
                const uint8_t * const src_spb = s->sao_pixel_buffer_h[c_idx] + (((2 * y_ctb + 2) * w + x0) << sh);
                const unsigned int hoff = height * stride_src;

                if (src_l != NULL) {
                    src_idx = (CTB(s->sao, x_ctb-1, y_ctb+1).type_idx[c_idx] ==
                               SAO_APPLIED);
                    copy_pixel(dst1 - (1 << sh), src_idx ? src_spb - (1 << sh) : src_l + hoff, sh);
                }

                src_idx = (CTB(s->sao, x_ctb, y_ctb+1).type_idx[c_idx] ==
                           SAO_APPLIED);
                memcpy(dst1, src_idx ? src_spb : src + hoff, width << sh);

                if (src_r != NULL) {
                    src_idx = (CTB(s->sao, x_ctb+1, y_ctb+1).type_idx[c_idx] ==
                               SAO_APPLIED);
                    copy_pixel(dst1 + (width << sh), src_idx ? src_spb + (width << sh) : src_r + hoff, sh);
                }
            }
            if (src_l != NULL) {
                if (CTB(s->sao, x_ctb-1, y_ctb).type_idx[c_idx] == SAO_APPLIED) {
                    copy_vert(dst - (1 << sh),
                              s->sao_pixel_buffer_v[c_idx] + (((2 * x_ctb - 1) * h + y0) << sh),
                              sh, height, stride_dst, 1 << sh);
                } else {
                    copy_vert(dst - (1 << sh),
                              src_l,
                              sh, height, stride_dst, stride_src);
                }
            }
            if (src_r != NULL) {
                if (CTB(s->sao, x_ctb+1, y_ctb).type_idx[c_idx] == SAO_APPLIED) {
                    copy_vert(dst + (width << sh),
                              s->sao_pixel_buffer_v[c_idx] + (((2 * x_ctb + 2) * h + y0) << sh),
                              sh, height, stride_dst, 1 << sh);
                } else {
                    copy_vert(dst + (width << sh),
                              src_r,
                              sh, height, stride_dst, stride_src);
                }
            }

            copy_CTB(dst,
                     src,
                     width << sh,
                     height, stride_dst, stride_src);

            copy_CTB_to_hv(s, src, stride_src, x0, y0, width, height, c_idx,
                           x_ctb, y_ctb);
            if (sliced && c_idx != 0)
            {
                // Class always the same for both U & V (which is just as well :-))
                s->hevcdsp.sao_edge_filter_c[tab](src, dst, stride_src,
                                                sao->offset_val[1], sao->offset_val[2], sao->eo_class[1],
                                                width, height);
                s->hevcdsp.sao_edge_restore_c[restore](src, dst,
                                                    stride_src, stride_dst,
                                                    sao,
                                                    edges, width,
                                                    height, c_idx,
                                                    vert_edge,
                                                    horiz_edge,
                                                    diag_edge);
            }
            else
            {
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
            }
            // ??? Does this actually work for chroma ???
            restore_tqb_pixels(s, src, dst, stride_src, stride_dst,
                               x, y, width, height, c_idx);
            sao->type_idx[c_idx] = SAO_APPLIED;
            break;
        }
        }
    }

#if RPI_ZC_SAND_8_IN_10_BUF
    if (s->frame->format == AV_PIX_FMT_SAND64_10 && s->frame->buf[RPI_ZC_SAND_8_IN_10_BUF] != NULL &&
        (((x + (1 << (s->ps.sps->log2_ctb_size))) & 255) == 0 || edges[2]))
    {
        const unsigned int stride1 = frame_stride1(s->frame, 1);
        const unsigned int stride2 = av_rpi_sand_frame_stride2(s->frame);
        const unsigned int xoff = (x >> 8) * stride2 * stride1;
        const unsigned int ctb_size = (1 << s->ps.sps->log2_ctb_size);
        const uint8_t * const sy = s->frame->data[0] + xoff * 4 + y * stride1;
        uint8_t * const dy = s->frame->buf[4]->data + xoff * 2 + y * stride1;
        const uint8_t * const sc = s->frame->data[1] + xoff * 4 + (y >> 1) * stride1;
        uint8_t * const dc = s->frame->buf[4]->data + (s->frame->data[1] - s->frame->data[0]) + xoff * 2 + (y >> 1) * stride1;
        const unsigned int wy = !edges[2] ? 256 : s->ps.sps->width - (x & ~255);
        const unsigned int hy = !edges[3] ? ctb_size : s->ps.sps->height - y;

//        printf("dy=%p/%p, stride1=%d, stride2=%d, sy=%p/%p, wy=%d, hy=%d, x=%d, y=%d, cs=%d\n", dy, dc, stride1, stride2, sy, sc, wy, hy, x, y, ctb_size);
        av_rpi_sand16_to_sand8(dy, stride1, stride2, sy, stride1, stride2, wy, hy, 3);
        av_rpi_sand16_to_sand8(dc, stride1, stride2, sc, stride1, stride2, wy, hy >> 1, 3);
    }
#endif
}

static inline uint32_t pcm4(const HEVCRpiContext * const s, const unsigned int x, const unsigned int y)
{
    const uint8_t * const pcm = s->is_pcm + (x >> 6) + (y >> 3) * s->ps.sps->pcm_width;
    return (pcm[0] |
        (pcm[1] << 8) |
        (pcm[s->ps.sps->pcm_width] << 16) |
        (pcm[s->ps.sps->pcm_width + 1] << 24)) >> ((x >> 3) & 7);
}

static inline uint32_t pcm2(const HEVCRpiContext * const s, const unsigned int x, const unsigned int y)
{
    const uint8_t * const pcm = s->is_pcm + (x >> 6) + (y >> 3) * s->ps.sps->pcm_width;
    return (pcm[0] | (pcm[1] << 8)) >> ((x >> 3) & 7);
}


static void deblock_y_blk(const HEVCRpiContext * const s, const RpiBlk bounds, const int end_x, const int end_y)
{
    const unsigned int log2_ctb_size = s->ps.sps->log2_ctb_size;
    const unsigned int log2_min_cb_size  = s->ps.sps->log2_min_cb_size;
    const unsigned int ctb_size = (1 << log2_ctb_size);
    const unsigned int cb_r = FFMIN(bounds.x + bounds.w, s->ps.sps->width) - (end_x ? 0 :  1);
    const unsigned int ctb_n = (bounds.x + bounds.y * s->ps.sps->ctb_width) >> log2_ctb_size;
    const DBParams * cb_dbp = s->deblock + ctb_n;
    const unsigned int b_b = FFMIN(bounds.y + bounds.h, s->ps.sps->height) - (end_y ? 0 : 8);

    unsigned int cb_x;

    // Do in CTB-shaped blocks
    for (cb_x = bounds.x; cb_x < cb_r; cb_x += ctb_size, ++cb_dbp)
    {
        const unsigned int bv_r = FFMIN(cb_x + ctb_size, cb_r);
        const unsigned int bv_l = FFMAX(cb_x, 8);
        const unsigned int bh_r = cb_x + ctb_size >= cb_r ? cb_r - 8 : cb_x + ctb_size - 9;
        const unsigned int bh_l = bv_l - 8;
        unsigned int y;

        // Main body
        for (y = (bounds.y == 0 ? 0 : bounds.y - 8); y < b_b; y += 8)
        {
            const DBParams * const dbp = y < bounds.y ? cb_dbp - s->ps.sps->ctb_width : cb_dbp;
            const int8_t * const qta = s->qp_y_tab + ((y - 1) >> log2_min_cb_size) * s->ps.sps->min_cb_width;
            const int8_t * const qtb = s->qp_y_tab + (y >> log2_min_cb_size) * s->ps.sps->min_cb_width;

            {
                const uint8_t * const tcv = tctable + dbp->tc_offset;
                const uint8_t * const betav = betatable + dbp->beta_offset;
                unsigned int pcmfa = pcm2(s, bv_l - 1, y);
                const uint8_t * vbs = s->vertical_bs + (bv_l >> 3) * s->bs_height + (y >> 2);
                unsigned int x;

                for (x = bv_l; x < bv_r; x += 8)
                {
                    const unsigned int pcmf_v = pcmfa & 3;
                    const unsigned int bs0 = vbs[0];
                    const unsigned int bs1 = vbs[1];

                    if ((bs0 | bs1) != 0 && pcmf_v != 3)
                    {
                        const int qp = (qtb[(x - 1) >> log2_min_cb_size] + qtb[x >> log2_min_cb_size] + 1) >> 1;
                        s->hevcdsp.hevc_v_loop_filter_luma2(av_rpi_sand_frame_pos_y(s->frame, x, y),
                                                         frame_stride1(s->frame, LUMA),
                                                         betav[qp],
                                                         (bs0 == 0 ? 0 : tcv[qp + (int)(bs0 & 2)]) |
                                                          ((bs1 == 0 ? 0 : tcv[qp + (int)(bs1 & 2)]) << 16),
                                                         pcmf_v,
                                                         av_rpi_sand_frame_pos_y(s->frame, x - 4, y));
                    }

                    pcmfa >>= 1;
                    vbs += s->bs_height;
                }
            }

            if (y != 0)
            {
                unsigned int x;
                unsigned int pcmfa = pcm4(s, bh_l, y - 1);
                const uint8_t * hbs = s->horizontal_bs + (y >> 3) * s->bs_width + (bh_l >> 2);

                for (x = bh_l; x <= bh_r; x += 8)
                {
                    const unsigned int pcmf_h = (pcmfa & 1) | ((pcmfa & 0x10000) >> 15);
                    const unsigned int bs0 = hbs[0];
                    const unsigned int bs1 = hbs[1];

                    if ((bs0 | bs1) != 0 && pcmf_h != 3)
                    {
                        const int qp = (qta[x >> log2_min_cb_size] + qtb[x >> log2_min_cb_size] + 1) >> 1;
                        const DBParams * const dbph = (x < cb_x ? dbp - 1 : dbp);
                        const uint8_t * const tc = tctable + dbph->tc_offset + qp;
                        s->hevcdsp.hevc_h_loop_filter_luma2(av_rpi_sand_frame_pos_y(s->frame, x, y),
                                                             frame_stride1(s->frame, LUMA),
                                                             betatable[qp + dbph->beta_offset],
                                                             (bs0 == 0 ? 0 : tc[bs0 & 2]) |
                                                                ((bs1 == 0 ? 0 : tc[bs1 & 2]) << 16),
                                                             pcmf_h);
                    }

                    pcmfa >>= 1;
                    hbs += 2;
                }
            }

        }
    }
}

#define TL 1
#define TR 2
#define BL 4
#define BR 8

static av_always_inline int q2h(const HEVCRpiContext * const s, const unsigned int x, const unsigned int y)
{
    const unsigned int log2_min_cb_size  = s->ps.sps->log2_min_cb_size;
    const int8_t * const qt = s->qp_y_tab + (y >> log2_min_cb_size) * s->ps.sps->min_cb_width;
    return (qt[(x - 1) >> log2_min_cb_size] + qt[x >> log2_min_cb_size] + 1) >> 1;
}

static void deblock_uv_blk(const HEVCRpiContext * const s, const RpiBlk bounds, const int end_x, const int end_y)
{
    const unsigned int log2_ctb_size = s->ps.sps->log2_ctb_size;
    const unsigned int log2_min_cb_size  = s->ps.sps->log2_min_cb_size;
    const unsigned int ctb_size = (1 << log2_ctb_size);
    const unsigned int cb_r = FFMIN(bounds.x + bounds.w, s->ps.sps->width) - (end_x ? 0 :  8);
    const unsigned int ctb_n = (bounds.x + bounds.y * s->ps.sps->ctb_width) >> log2_ctb_size;
    const DBParams * dbp = s->deblock + ctb_n;
    const unsigned int b_b = FFMIN(bounds.y + bounds.h, s->ps.sps->height) - (end_y ? 0 : 8);
    const uint8_t * const tcq_u = s->ps.pps->qp_dblk_x[1];
    const uint8_t * const tcq_v = s->ps.pps->qp_dblk_x[2];

    unsigned int cb_x;

    av_assert1((bounds.x & (ctb_size - 1)) == 0);
    av_assert1((bounds.y & (ctb_size - 1)) == 0);
    av_assert1(bounds.h <= ctb_size);

//    printf("%s: %d,%d (%d,%d) cb_r=%d, b_b=%d\n", __func__, bounds.x, bounds.y, bounds.w, bounds.h, cb_r, b_b);

    // Do in CTB-shaped blocks
    for (cb_x = bounds.x; cb_x < cb_r; cb_x += ctb_size, ++dbp) {
        const unsigned int bv_r = FFMIN(cb_x + ctb_size, cb_r);
        const unsigned int bv_l = FFMAX(cb_x, 16);
        unsigned int y;

        // V above
        if (bounds.y != 0) {
            // Deblock V up 8
            // CTB above current
            // Top-half only (tc4 & ~0xffff == 0) is special cased in asm
            unsigned int x;
            const unsigned int y = bounds.y - 8;

            unsigned int pcmfa = pcm2(s, bv_l - 1, y);
            const uint8_t * const tc = tctable + 2 + (dbp - s->ps.sps->ctb_width)->tc_offset;
            const uint8_t * vbs = s->vertical_bs + (bv_l >> 3) * s->bs_height + (y >> 2);

            for (x = bv_l; x < bv_r; x += 16)
            {
                const unsigned int pcmf_v = (pcmfa & 3);
                if ((vbs[0] & 2) != 0 && pcmf_v != 3)
                {
                    const int qp0 = q2h(s, x, y);
                    s->hevcdsp.hevc_v_loop_filter_uv2(av_rpi_sand_frame_pos_c(s->frame, x >> 1, y >> 1),
                                                   frame_stride1(s->frame, 1),
                                                   tc[tcq_u[qp0]] | (tc[tcq_v[qp0]] << 8),
                                                   av_rpi_sand_frame_pos_c(s->frame, (x >> 1) - 2, y >> 1),
                                                   pcmf_v);
                }
                pcmfa >>= 2;
                vbs += s->bs_height * 2;
            }
        }

        for (y = bounds.y; y < b_b; y += 16)
        {
            // V
            {
                unsigned int x;
                unsigned int pcmfa = pcm4(s, bv_l - 1, y);
                const unsigned int pcmf_or = (y + 16 <= b_b) ? 0 : BL | BR;
                const uint8_t * const tc = tctable + 2 + dbp->tc_offset;
                const uint8_t * vbs = s->vertical_bs + (bv_l >> 3) * s->bs_height + (y >> 2);

                for (x = bv_l; x < bv_r; x += 16)
                {
                    const unsigned int pcmf_v = pcmf_or | (pcmfa & 3) | ((pcmfa >> 14) & 0xc);
                    const unsigned int bs0 = (~pcmf_v & (TL | TR)) == 0 ? 0 : vbs[0] & 2;
                    const unsigned int bs1 = (~pcmf_v & (BL | BR)) == 0 ? 0 : vbs[2] & 2;

                    if ((bs0 | bs1) != 0)
                    {
                        const int qp0 = q2h(s, x, y);
                        const int qp1 = q2h(s, x, y + 8);
                        s->hevcdsp.hevc_v_loop_filter_uv2(av_rpi_sand_frame_pos_c(s->frame, x >> 1, y >> 1),
                            frame_stride1(s->frame, 1),
                            ((bs0 == 0) ? 0 : (tc[tcq_u[qp0]] << 0) | (tc[tcq_v[qp0]] << 8)) |
                                ((bs1 == 0) ? 0 : (tc[tcq_u[qp1]] << 16) | (tc[tcq_v[qp1]] << 24)),
                            av_rpi_sand_frame_pos_c(s->frame, (x >> 1) - 2, y >> 1),
                            pcmf_v);
                    }

                    pcmfa >>= 2;
                    vbs += s->bs_height * 2;
                }
            }

            // H
            if (y != 0)
            {
                unsigned int x;
                const unsigned int bh_r = cb_x + ctb_size >= cb_r ? cb_r : cb_x + ctb_size - 16;
                const unsigned int bh_l = bv_l - 16;
                unsigned int pcmfa = pcm4(s, bh_l, y - 1);
                const uint8_t * hbs = s->horizontal_bs + (y >> 3) * s->bs_width + (bh_l >> 2);
                const int8_t * const qta = s->qp_y_tab + ((y - 1) >> log2_min_cb_size) * s->ps.sps->min_cb_width;
                const int8_t * const qtb = s->qp_y_tab + (y >> log2_min_cb_size) * s->ps.sps->min_cb_width;

                for (x = bh_l; x < bh_r; x += 16)
                {
                    const unsigned int pcmf_h = (x < bounds.x ? TL | BL : x + 16 > bh_r ? TR | BR : 0) |
                        (pcmfa & 3) | ((pcmfa >> 14) & 0xc);
                    const int bs0 = (~pcmf_h & (TL | BL)) == 0 ? 0 : hbs[0] & 2;
                    const int bs1 = (~pcmf_h & (TR | BR)) == 0 ? 0 : hbs[2] & 2;

                    if ((bs0 | bs1) != 0)
                    {
                        const int qp0 = (qta[x >> log2_min_cb_size] + qtb[x >> log2_min_cb_size] + 1) >> 1;
                        const int qp1 = (qta[(x + 8) >> log2_min_cb_size] + qtb[(x + 8) >> log2_min_cb_size] + 1) >> 1;
                        const uint8_t * const tc = tctable + 2 + (x < cb_x ? dbp - 1 : dbp)->tc_offset;

                        s->hevcdsp.hevc_h_loop_filter_uv(av_rpi_sand_frame_pos_c(s->frame, x >> 1, y >> 1),
                            frame_stride1(s->frame, 1),
                            ((bs0 == 0) ? 0 : (tc[tcq_u[qp0]] << 0) | (tc[tcq_v[qp0]] << 8)) |
                                ((bs1 == 0) ? 0 : (tc[tcq_u[qp1]] << 16) | (tc[tcq_v[qp1]] << 24)),
                            pcmf_h);
                    }
                    pcmfa >>= 2;
                    hbs += 4;
                }
            }
        }
    }
}

static inline unsigned int off_boundary(const unsigned int x, const unsigned int log2_n)
{
    return x & ~(~0U << log2_n);
}

static inline void bs_set_rep(uint8_t * bs, unsigned int trafo_size, const unsigned int n)
{
    trafo_size >>= 2;
    do
    {
        *bs++ = n;
    } while (--trafo_size != 0);
}

static inline uint32_t bsf_unpack(uint32_t a)
{
    // zip(a, b)
    uint32_t x = 0;
    unsigned int i = 0;
    a &= 0xffff;
    while (a != 0) {
        x |= (a & 1) << i;
        i += 2;
        a >>= 1;
    }
    return x;
}

// Must be on a boundary so no need to faff - max 8 bits wanted
static inline uint32_t cbf_luma_up(const uint8_t *const curr, const unsigned int stride, const unsigned int x0, const uint32_t bsf_mask)
{
    return bsf_unpack(*(const uint32_t *)(curr - stride) >> ((x0 >> 2) & 31)) & bsf_mask;
}

static inline uint32_t bsf_intra(const MvField *mvf, const unsigned int stride, const unsigned int min_pu_in_4pix2, const unsigned int trafo_4pix2)
{
    const unsigned int m = (1 << min_pu_in_4pix2) - 1;
    unsigned int n = 0;
    uint32_t a = 0;
    do
    {
        if (mvf->pred_flag == PF_INTRA)
            a |= m << n;
        mvf += stride;
        n += min_pu_in_4pix2;
    } while (n < trafo_4pix2);
    return a;
}

// Much less fun than up!
static inline unsigned int cbf_luma_left(const uint8_t * cbf, const unsigned int stride, const unsigned int x0, const unsigned int trafo_4pix2)
{
    const unsigned int sh = ((x0 - 1) >> 2) & 31;
    unsigned int a = 0;
    unsigned int n = 0;

    if (sh == 31)
        cbf -= 4;

    do
    {
        a |= ((*(const uint32_t *)cbf >> sh) & 1) << n;
        cbf += stride;
        n += 2;
    } while (n != trafo_4pix2);

    return a;
}

static inline uint32_t bsf_up(
    const uint32_t bsf0, const uint32_t bsf_cbf, const uint32_t bsf_mask,
    const uint8_t *const cbf, const unsigned int cbf_stride, const unsigned int x0,
    const MvField *const mvf, const unsigned int mvf_stride, const unsigned int min_pu_in_4pix2, const int no_mvf,
    const unsigned int trafo_4pix2)
{
    unsigned int bsf = bsf0;

    if (bsf == bsf_mask)
        return bsf;
    if (!no_mvf)
        bsf |= bsf_intra(mvf, mvf_stride, min_pu_in_4pix2, trafo_4pix2) & bsf_mask;
    if ((~bsf & bsf_cbf) == 0)
        return bsf;
    bsf |= cbf_luma_up(cbf, cbf_stride, x0, bsf_mask);
    return bsf;
}

static inline uint32_t bsf_left(
    const uint32_t bsf0, const uint32_t bsf_cbf, const uint32_t bsf_mask,
    const uint8_t *const cbf, const unsigned int cbf_stride, const unsigned int x0,
    const MvField *const mvf, const unsigned int mvf_stride, const unsigned int min_pu_in_4pix2, const int no_mvf,
    const unsigned int trafo_4pix2)
{
    unsigned int bsf = bsf0;

    if (bsf == bsf_mask)
        return bsf;
    if (!no_mvf)
        bsf |= bsf_intra(mvf, mvf_stride, min_pu_in_4pix2, trafo_4pix2) & bsf_mask;
    if ((~bsf & bsf_cbf) == 0)
        return bsf;
    bsf |= cbf_luma_left(cbf, cbf_stride, x0, trafo_4pix2);
    return bsf;
}



void ff_hevc_rpi_deblocking_boundary_strengths(const HEVCRpiContext * const s,
                                               const HEVCRpiLocalContext * const lc,
                                               const unsigned int x0, const unsigned int y0,
                                               const unsigned int log2_trafo_size)
{
    const MvField * const tab_mvf       = s->ref->tab_mvf;
    const unsigned int log2_min_pu_size = s->ps.sps->log2_min_pu_size;
    const unsigned int min_pu_width     = s->ps.sps->min_pu_width;
    const RefPicList * const rpl        = s->ref->refPicList;
    const unsigned int log2_dup         = FFMIN(log2_min_pu_size, log2_trafo_size);
    const unsigned int min_pu_in_4pix   = 1 << (log2_dup - 2);  // Dup
    const unsigned int trafo_in_min_pus = 1 << (log2_trafo_size - log2_dup); // Rep
    const MvField * const curr_mvf      = tab_mvf + (y0 >> log2_min_pu_size) * min_pu_width + (x0 >> log2_min_pu_size);
    // pus per 8pix (dblk H/V interval)
    const int inc                       = log2_min_pu_size == 2 ? 2 : 1;
    // Put on a word boundary so we can load all of up at once
    // but keep as uint8_t * for ease of adding stride
    const uint8_t *const curr_cbf_luma = s->cbf_luma + (y0 >> 2) * s->cbf_luma_stride + ((x0 >> 5) & ~3);
    // Given where we are called from is_cbf_luma & is_intra will be constant over the block
    const int is_cbf = (*(const uint32_t *)curr_cbf_luma >> ((x0 >> 2) & 31)) & 1;
    const int is_intra = curr_mvf->pred_flag == PF_INTRA;
    const unsigned int boundary_flags   = s->sh.no_dblk_boundary_flags & lc->boundary_flags;
    const unsigned int trafo_size       = (1U << log2_trafo_size);
    const unsigned int trafo_4pix2      = trafo_size >> 1;
    const unsigned int min_pu_in_4pix2  = 1 << (log2_min_pu_size - 1);
    const uint32_t bsf_mask = log2_trafo_size > 5 ? ~0U : (1 << trafo_4pix2) - 1;
    const uint32_t bsf_cbf = (bsf_mask & 0x55555555);
    const uint32_t bsf0 = is_intra ? bsf_mask : is_cbf ? bsf_cbf : 0;

    unsigned int i;

#ifdef DISABLE_STRENGTHS
    return;
#endif

    av_assert2(log2_trafo_size <= 6);

    // Do Horizontal
    if ((y0 & 7) == 0) {
        uint8_t * bs = s->horizontal_bs + (x0 >> 2) + (y0 >> 3) * s->bs_width;

        // Boundary upper
        if (y0 != 0 &&
            (off_boundary(y0, s->ps.sps->log2_ctb_size) ||
             (boundary_flags & (BOUNDARY_UPPER_SLICE | BOUNDARY_UPPER_TILE)) == 0))
        {
            uint32_t bsf = bsf_up(
                bsf0, bsf_cbf, bsf_mask,
                curr_cbf_luma, s->cbf_luma_stride, x0,
                curr_mvf - min_pu_width, 1, min_pu_in_4pix2, off_boundary(y0, log2_min_pu_size),
                trafo_4pix2);

            if ((~bsf & bsf_cbf) != 0 && !off_boundary(y0, log2_min_pu_size))
            {
                const RefPicList *const rpl_top = (lc->boundary_flags & BOUNDARY_UPPER_SLICE) ?
                                      ff_hevc_rpi_get_ref_list(s, s->ref, x0, y0 - 1) :
                                      rpl;

                s->hevcdsp.hevc_deblocking_boundary_strengths(trafo_in_min_pus,
                        min_pu_in_4pix, sizeof (MvField), 1,
                        rpl[0].list, rpl[1].list, rpl_top[0].list, rpl_top[1].list,
                        curr_mvf, curr_mvf - min_pu_width, bs);
            }

            for (i = 0; bsf != 0; i += 4, bsf >>= 2) {
                if ((bsf & 3) != 0)
                    bs[i >> 2] = bsf & 3;
            }
        }

        // Only look for MV boundaries to deblock if we might have multiple MVs here
        // If intra then pb_width/height will always be >= trafo size so no need to test
        if (lc->cu.min_pb_height < trafo_size) {
            const MvField * mvf = curr_mvf;

            for (i = inc; i < trafo_in_min_pus; i += inc) {
                mvf += min_pu_width * inc;
                bs += s->bs_width * ((inc << log2_min_pu_size) >> 3);

                s->hevcdsp.hevc_deblocking_boundary_strengths(trafo_in_min_pus,
                        min_pu_in_4pix, sizeof (MvField), 1,
                        rpl[0].list, rpl[1].list, rpl[0].list, rpl[1].list,
                        mvf, mvf - min_pu_width, bs);
            }
        }
    }

    // And again for vertical - same logic as horizontal just in the other direction
    if ((x0 & 7) == 0) {
        uint8_t * bs = s->vertical_bs + (x0 >> 3) * s->bs_height + (y0 >> 2);

        // Boundary left
        if (x0 != 0 &&
            ((x0 & ((1 << s->ps.sps->log2_ctb_size) - 1)) != 0 ||
             (boundary_flags & (BOUNDARY_LEFT_SLICE | BOUNDARY_LEFT_TILE)) == 0))
        {
            uint32_t bsf = bsf_left(
                bsf0, bsf_cbf, bsf_mask,
                curr_cbf_luma, s->cbf_luma_stride, x0,
                curr_mvf - 1, min_pu_width, min_pu_in_4pix2, off_boundary(x0, log2_min_pu_size),
                trafo_4pix2);

            if ((~bsf & bsf_cbf) != 0 && !off_boundary(x0, log2_min_pu_size))
            {
                const RefPicList * const rpl_left = (lc->boundary_flags & BOUNDARY_LEFT_SLICE) ?
                                       ff_hevc_rpi_get_ref_list(s, s->ref, x0 - 1, y0) :
                                       rpl;

                s->hevcdsp.hevc_deblocking_boundary_strengths(trafo_in_min_pus,
                        min_pu_in_4pix, min_pu_width * sizeof (MvField), 1,
                        rpl[0].list, rpl[1].list, rpl_left[0].list, rpl_left[1].list,
                        curr_mvf, curr_mvf - 1, bs);
            }

            for (i = 0; bsf != 0; i += 4, bsf >>= 2) {
                if ((bsf & 3) != 0)
                    bs[i >> 2] = bsf & 3;
            }
        }

        if (lc->cu.min_pb_width < trafo_size) {
            const MvField * mvf = curr_mvf;

            for (i = inc; i < trafo_in_min_pus; i += inc) {
                mvf += inc;
                bs += s->bs_height * ((inc << log2_min_pu_size) >> 3);

                s->hevcdsp.hevc_deblocking_boundary_strengths(trafo_in_min_pus,
                        min_pu_in_4pix, min_pu_width * sizeof (MvField), 1,
                        rpl[0].list, rpl[1].list, rpl[0].list, rpl[1].list,
                        mvf, mvf - 1, bs);
            }
        }
    }
}

#undef LUMA
#undef CB
#undef CR

static inline unsigned int ussub(const unsigned int a, const unsigned int b)
{
    return a < b ? 0 : a - b;
}

static inline int cache_boundry(const AVFrame * const frame, const unsigned int x)
{
    return ((x >> av_rpi_sand_frame_xshl(frame)) & ~63) == 0;
}

int ff_hevc_rpi_hls_filter_blk(const HEVCRpiContext * const s, const RpiBlk bounds, const int eot)
{
    const int ctb_size = (1 << s->ps.sps->log2_ctb_size);
    int x, y;

    const unsigned int br = FFMIN(bounds.x + bounds.w, s->ps.sps->width);
    const unsigned int bb = FFMIN(bounds.y + bounds.h, s->ps.sps->height);

    const int x_end = (br >= s->ps.sps->width);
    const int y_end = (bb >= s->ps.sps->height);

    // Deblock may not touch the edges of the bound as they are still needed
    // for Intra pred

    deblock_y_blk(s, bounds, x_end, y_end);
    deblock_uv_blk(s, bounds, x_end, y_end);

    // SAO needs
    // (a) CTB alignment
    // (b) Valid pixels all the way around the CTB in particular it needs the DR pixel
    {
        const unsigned int xo = bounds.x - ((bounds.x - 16) & ~(ctb_size - 1));
        const unsigned int yo = bounds.y - ((bounds.y - 16) & ~(ctb_size - 1));
        const unsigned int yt = ussub(bounds.y, yo);
        const unsigned int yb = y_end ? bb : ussub(bb, yo);
        const unsigned int xl = ussub(bounds.x, xo);
        const unsigned int xr = x_end ? br : ussub(br, xo);

        for (y = yt; y < yb; y += ctb_size) {
            for (x = xl; x < xr; x += ctb_size) {
                sao_filter_CTB(s, x, y);
            }
        }

        // Cache invalidate
        y = 0;
        if (xr != 0 && yb != 0)
        {
            const unsigned int llen =
                (av_rpi_sand_frame_stride1(s->frame) >> av_rpi_sand_frame_xshl(s->frame));
            const unsigned int mask = ~(llen - 1);
            const unsigned int il = (xl == 0) ? 0 : (xl - 1) & mask;
            const unsigned int ir = x_end || !cache_boundry(s->frame, br) ? br : (xr - 1) & mask;
            const unsigned int it = ussub(yt, 1);
            const unsigned int ib = y_end ? bb : yb - 1;

            if (il < ir) {
                rpi_cache_buf_t cbuf;
                rpi_cache_flush_env_t * const rfe = rpi_cache_flush_init(&cbuf);
                rpi_cache_flush_add_frame_block(rfe, s->frame, RPI_CACHE_FLUSH_MODE_WB_INVALIDATE,
                  il, it, ir - il, ib - it,
                  ctx_vshift(s, 1), 1, 1);

                // *** Tiles where V tile boundries aren't on cache boundries
                // We have a race condition between ARM side recon in the tlle
                // on the left & QPU pred in the tile on the right
                // The code below ameliorates it as does turning off WPP in
                // these cases but it still exists :-(

                // If we have to commit the right hand tile boundry due to
                // cache boundry considerations then at EoTile we must commit
                // that boundry to bottom of tile (bounds)
                if (ib != bb && ir == br && eot) {
                    rpi_cache_flush_add_frame_block(rfe, s->frame, RPI_CACHE_FLUSH_MODE_WB_INVALIDATE,
                      br - 1, ib, 1, bb - ib,
                      ctx_vshift(s, 1), 1, 1);
                }

                rpi_cache_flush_finish(rfe);

                if (x_end)
                    y = y_end ? INT_MAX : ib;

//                printf("Flush: %4d,%4d -> %4d,%4d: signal: %d\n", il, it, ir, ib, y - 1);
            }
        }
    }

    return y;
}

