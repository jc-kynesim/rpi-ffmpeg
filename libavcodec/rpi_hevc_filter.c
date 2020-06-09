/*
 * HEVC video decoder
 *
 * Originally by:
 * Copyright (C) 2012 - 2013 Guillaume Martres
 * Copyright (C) 2013 Seppo Tomperi
 * Copyright (C) 2013 Wassim Hamidouche
 *
 * Substantially rewritten:
 * Copyright (C) 2018 John Cox, Ben Avison for Raspberry Pi (Trading)
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
    ff_hevc_rpi_copy_vert(s->sao_pixel_buffer_v[c_idx] + (((2 * x_ctb) * h + y) << sh), src, sh, height, 1 << sh, stride_src);

    ff_hevc_rpi_copy_vert(s->sao_pixel_buffer_v[c_idx] + (((2 * x_ctb + 1) * h + y) << sh), src + ((width - 1) << sh), sh, height, 1 << sh, stride_src);
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
                    s->hevcdsp.cpy_blk(bd, stride_src, bs, stride_dst, bwidth, bheight);
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
                s->hevcdsp.cpy_blk(dst, stride_dst, src, stride_src, width << sh, height);
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
                [RPI_HEVC_SAO_BUF_STRIDE * (MAX_PB_SIZE + 2) + 64];

            stride_dst = RPI_HEVC_SAO_BUF_STRIDE;
            dst = dstbuf + stride_dst + 32;

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
                    ff_hevc_rpi_copy_vert(dst - (1 << sh),
                              s->sao_pixel_buffer_v[c_idx] + (((2 * x_ctb - 1) * h + y0) << sh),
                              sh, height, stride_dst, 1 << sh);
                } else {
                    ff_hevc_rpi_copy_vert(dst - (1 << sh),
                              src_l,
                              sh, height, stride_dst, stride_src);
                }
            }
            if (src_r != NULL) {
                if (CTB(s->sao, x_ctb+1, y_ctb).type_idx[c_idx] == SAO_APPLIED) {
                    ff_hevc_rpi_copy_vert(dst + (width << sh),
                              s->sao_pixel_buffer_v[c_idx] + (((2 * x_ctb + 2) * h + y0) << sh),
                              sh, height, stride_dst, 1 << sh);
                } else {
                    ff_hevc_rpi_copy_vert(dst + (width << sh),
                              src_r,
                              sh, height, stride_dst, stride_src);
                }
            }

            s->hevcdsp.cpy_blk(dst, stride_dst, src, stride_src, width << sh, height);

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

// When bits are delivered to deblock we want them
//#define TL 1
//#define TR 2
//#define BL 4
//#define BR 8

// pcm4 returns them as b0 = tl, b1 = tr, b16 = bl, b17 = br
// so we need to rearrange before passing on

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

// We cast away const here as we want this to work for both get and set
static inline uint32_t * bs_ptr32(const uint8_t * bs, const unsigned int stride2, const unsigned int x, const unsigned int y)
{
    return (uint32_t *)(bs +
#if (~3U & (HEVC_RPI_BS_STRIDE1_PEL_MASK >> HEVC_RPI_BS_PELS_PER_BYTE_SHIFT)) != 0
#warning Unexpected masks
        // As it happens we end up with stride1 = sizeof(uint32_t) so this expr vanishes
        ((x >> HEVC_RPI_BS_PELS_PER_BYTE_SHIFT) &
            (~3 & (HEVC_RPI_BS_STRIDE1_PEL_MASK >> HEVC_RPI_BS_PELS_PER_BYTE_SHIFT))) +
#elif HEVC_RPI_BS_STRIDE1_BYTES < 4
#error Stride1 < return size
#endif
        ((y >> HEVC_RPI_BS_Y_SHR) << HEVC_RPI_BS_STRIDE1_BYTE_SHIFT) +
        (x >> HEVC_RPI_BS_STRIDE1_PEL_SHIFT) * stride2);
}

static inline uint8_t * bs_ptr8(const uint8_t * bs, const unsigned int stride2, const unsigned int x, const unsigned int y)
{
    return (uint8_t *)(bs +
        ((x >> HEVC_RPI_BS_PELS_PER_BYTE_SHIFT) &
            (HEVC_RPI_BS_STRIDE1_PEL_MASK >> HEVC_RPI_BS_PELS_PER_BYTE_SHIFT)) +
        ((y >> HEVC_RPI_BS_Y_SHR) << HEVC_RPI_BS_STRIDE1_BYTE_SHIFT) +
        (x >> HEVC_RPI_BS_STRIDE1_PEL_SHIFT) * stride2);
}


// Get block strength
// Given how we call we will always get within the 32bit boundries
static inline uint32_t bs_get32(const uint8_t * bs, unsigned int stride2,
                                unsigned int xl, unsigned int xr, const unsigned int y)
{
    if (xr <= xl) {
        return 0;
    }
    else
    {
#if HAVE_ARMV6T2_INLINE
#if (~3U & (HEVC_RPI_BS_STRIDE1_PEL_MASK >> HEVC_RPI_BS_PELS_PER_BYTE_SHIFT)) != 0
#error This case not yet handled in bs_get32
#elif HEVC_RPI_BS_STRIDE1_BYTES < 4
#error Stride1 < return size
#endif
        uint32_t tmp;
        __asm__ (
            "lsr         %[tmp], %[xl], %[xl_shift]                  \n\t"
            "rsb         %[xr], %[xl], %[xr]                         \n\t"
            "mla         %[stride2], %[stride2], %[tmp], %[bs]       \n\t"
            "add         %[xr], %[xr], #7                            \n\t"
            "lsr         %[bs], %[y], %[y_shift1]                    \n\t"
            "bic         %[xr], %[xr], #7                            \n\t"
            "ubfx        %[xl], %[xl], #1, #5                        \n\t"
            "lsr         %[xr], %[xr], #1                            \n\t"
            "cmp         %[xr], #32                                  \n\t"
            "mvn         %[tmp], #0                                  \n\t"
            "ldr         %[bs], [%[stride2], %[bs], lsl %[y_shift2]] \n\t"
            "lsl         %[tmp], %[tmp], %[xr]                       \n\t"
            "lsr         %[xl], %[bs], %[xl]                         \n\t"
            "it ne                                                   \n\t"
            "bicne       %[bs], %[xl], %[tmp]                        \n\t"
            :  // Outputs
                      [bs]"+r"(bs),
                 [stride2]"+r"(stride2),
                      [xl]"+r"(xl),
                      [xr]"+r"(xr),
                     [tmp]"=&r"(tmp)
            :  // Inputs
                       [y]"r"(y),
                [xl_shift]"M"(HEVC_RPI_BS_STRIDE1_PEL_SHIFT),
                [y_shift1]"M"(HEVC_RPI_BS_Y_SHR),
                [y_shift2]"M"(HEVC_RPI_BS_STRIDE1_BYTE_SHIFT)
            :  // Clobbers
                "cc"
        );
        return (uint32_t) bs;
#else
        const uint32_t a = *bs_ptr32(bs, stride2, xl, y);
        const unsigned int n = ((xr - xl + 7) & ~7) >> 1;

        return n == 32 ? a :
            (a >> ((xl >> 1) & 31)) & ~(~0U << n);
#endif
    }
}

static inline uint32_t hbs_get32(const HEVCRpiContext * const s, const unsigned int xl, const unsigned int xr, const unsigned int y)
{
    av_assert2(((xl ^ (xr - 1)) >> s->ps.sps->log2_ctb_size) == 0);
    return bs_get32(s->bs_horizontal, s->bs_stride2, xl, xr, y);
}

static inline uint32_t vbs_get32(const HEVCRpiContext * const s, const unsigned int xl, const unsigned int xr, const unsigned int y)
{
    av_assert2(((xl ^ (xr - 1)) >> s->ps.sps->log2_ctb_size) == 0);
    return bs_get32(s->bs_vertical, s->bs_stride2, xl, xr, y);
}


static void deblock_y_blk(const HEVCRpiContext * const s, const RpiBlk bounds, const int end_x, const int end_y)
{
    const unsigned int log2_ctb_size = s->ps.sps->log2_ctb_size;
    const unsigned int log2_min_cb_size  = s->ps.sps->log2_min_cb_size;
    const unsigned int ctb_size = (1 << log2_ctb_size);
    const unsigned int cb_r = bounds.x + bounds.w - (end_x ? 0 :  1);
    const unsigned int ctb_n = (bounds.x + bounds.y * s->ps.sps->ctb_width) >> log2_ctb_size;
    const DBParams * cb_dbp = s->deblock + ctb_n;
    const unsigned int b_b = bounds.y + bounds.h - (end_y ? 0 : 8);

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
            uint32_t vbs = vbs_get32(s, bv_l, bv_r, y);

            const DBParams * const dbp = y < bounds.y ? cb_dbp - s->ps.sps->ctb_width : cb_dbp;
            const int8_t * const qta = s->qp_y_tab + ((y - 1) >> log2_min_cb_size) * s->ps.sps->min_cb_width;
            const int8_t * const qtb = s->qp_y_tab + (y >> log2_min_cb_size) * s->ps.sps->min_cb_width;

            if (vbs != 0)
            {
                const uint8_t * const tcv = tctable + dbp->tc_offset;
                const uint8_t * const betav = betatable + dbp->beta_offset;
                unsigned int pcmfa = pcm2(s, bv_l - 1, y);
                unsigned int x;

                for (x = bv_l; vbs != 0; x += 8, vbs >>= 4, pcmfa >>= 1)
                {
                    if ((vbs & 0xf) != 0 && (pcmfa & 3) != 3)
                    {
                        const int qp = (qtb[(x - 1) >> log2_min_cb_size] + qtb[x >> log2_min_cb_size] + 1) >> 1;
                        s->hevcdsp.hevc_v_loop_filter_luma2(av_rpi_sand_frame_pos_y(s->frame, x, y),
                                                         frame_stride1(s->frame, LUMA),
                                                         betav[qp],
                                                         ((vbs & 3) == 0 ? 0 : tcv[qp + (int)(vbs & 2)]) |
                                                          (((vbs & 0xc) == 0 ? 0 : tcv[qp + (int)((vbs >> 2) & 2)]) << 16),
                                                         pcmfa & 3,
                                                         av_rpi_sand_frame_pos_y(s->frame, x - 4, y));
                    }
                }
            }

            if (y != 0)
            {
                uint32_t hbs;

                // H left - mostly separated out so we only need a uint32_t hbs
                if ((hbs = hbs_get32(s, bh_l, cb_x, y)) != 0)
                {
                    const unsigned int x = bh_l;
                    const unsigned int pcmfa = pcm4(s, bh_l, y - 1);
                    const int qp = (qta[x >> log2_min_cb_size] + qtb[x >> log2_min_cb_size] + 1) >> 1;
                    const DBParams * const dbph = dbp - 1;
                    const uint8_t * const tc = tctable + dbph->tc_offset + qp;

                    av_assert2(cb_x - bh_l == 8);

                    s->hevcdsp.hevc_h_loop_filter_luma2(av_rpi_sand_frame_pos_y(s->frame, x, y),
                                                         frame_stride1(s->frame, LUMA),
                                                         betatable[qp + dbph->beta_offset],
                                                         ((hbs & 3) == 0 ? 0 : tc[hbs & 2]) |
                                                            (((hbs & 0xc) == 0 ? 0 : tc[(hbs >> 2) & 2]) << 16),
                                                         (pcmfa & 1) | ((pcmfa & 0x10000) >> 15));
                }

                // H
                if ((hbs = hbs_get32(s, cb_x, bh_r + 1, y)) != 0)  // Will give (x <= bh_r) in for loop
                {
                    unsigned int x;
                    unsigned int pcmfa = pcm4(s, cb_x, y - 1);

                    for (x = cb_x; hbs != 0; x += 8, hbs >>= 4, pcmfa >>= 1)
                    {
                        if ((hbs & 0xf) != 0 && (~pcmfa & 0x10001) != 0)
                        {
                            const int qp = (qta[x >> log2_min_cb_size] + qtb[x >> log2_min_cb_size] + 1) >> 1;
                            const uint8_t * const tc = tctable + dbp->tc_offset + qp;
                            s->hevcdsp.hevc_h_loop_filter_luma2(av_rpi_sand_frame_pos_y(s->frame, x, y),
                                                                frame_stride1(s->frame, LUMA),
                                                                betatable[qp + dbp->beta_offset],
                                                                ((hbs & 3) == 0 ? 0 : tc[hbs & 2]) |
                                                                   (((hbs & 0xc) == 0 ? 0 : tc[(hbs >> 2) & 2]) << 16),
                                                                (pcmfa & 1) | ((pcmfa & 0x10000) >> 15));
                        }
                    }
                }
            }

        }
    }
}

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
    const unsigned int cb_r = bounds.x + bounds.w - (end_x ? 0 :  8);
    const unsigned int ctb_n = (bounds.x + bounds.y * s->ps.sps->ctb_width) >> log2_ctb_size;
    const DBParams * dbp = s->deblock + ctb_n;
    const unsigned int b_b = bounds.y + bounds.h - (end_y ? 0 : 8);
    const uint8_t * const tcq_u = s->ps.pps->qp_dblk_x[1];
    const uint8_t * const tcq_v = s->ps.pps->qp_dblk_x[2];

    unsigned int cb_x;

    av_assert1((bounds.x & (ctb_size - 1)) == 0);
    av_assert1((bounds.y & (ctb_size - 1)) == 0);
    av_assert1(bounds.h <= ctb_size);

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
            const unsigned int y = bounds.y - 8;
            uint32_t vbs = vbs_get32(s, bv_l, bv_r, y) & 0x02020202U;

            if (vbs != 0)
            {
                unsigned int pcmfa = pcm2(s, bv_l - 1, y);
                const uint8_t * const tc = tctable + 2 + (dbp - s->ps.sps->ctb_width)->tc_offset;
                unsigned int x;

                for (x = bv_l; vbs != 0; x += 16, vbs >>= 8, pcmfa >>= 2)
                {
                    if ((vbs & 2) != 0 && (~pcmfa & 3) != 0)
                    {
                        const int qp0 = q2h(s, x, y);
                        s->hevcdsp.hevc_v_loop_filter_uv2(av_rpi_sand_frame_pos_c(s->frame, x >> 1, y >> 1),
                                                       frame_stride1(s->frame, 1),
                                                       tc[tcq_u[qp0]] | (tc[tcq_v[qp0]] << 8),
                                                       av_rpi_sand_frame_pos_c(s->frame, (x >> 1) - 2, y >> 1),
                                                       pcmfa & 3);
                    }
                }
            }
        }

        for (y = bounds.y; y < b_b; y += 16)
        {
            uint32_t vbs = (vbs_get32(s, bv_l, bv_r, y) & 0x02020202U) |
                (y + 16 > b_b ? 0 : (vbs_get32(s, bv_l, bv_r, y + 8) & 0x02020202U) << 4);

            // V
            if (vbs != 0)
            {
                unsigned int x;
                unsigned int pcmfa =
                    (y + 16 > b_b ?
                        pcm2(s, bv_l - 1, y) | 0xffff0000 :
                        pcm4(s, bv_l - 1, y));
                const uint8_t * const tc = tctable + 2 + dbp->tc_offset;

                for (x = bv_l; vbs != 0; x += 16, vbs >>= 8, pcmfa >>= 2)
                {
                    if ((vbs & 0xff) != 0 && (~pcmfa & 0x30003) != 0)
                    {
                        const int qp0 = q2h(s, x, y);
                        const int qp1 = q2h(s, x, y + 8);
                        s->hevcdsp.hevc_v_loop_filter_uv2(av_rpi_sand_frame_pos_c(s->frame, x >> 1, y >> 1),
                            frame_stride1(s->frame, 1),
                            ((vbs & 2) == 0 ? 0 : (tc[tcq_u[qp0]] << 0) | (tc[tcq_v[qp0]] << 8)) |
                                ((vbs & 0x20) == 0 ? 0 : (tc[tcq_u[qp1]] << 16) | (tc[tcq_v[qp1]] << 24)),
                            av_rpi_sand_frame_pos_c(s->frame, (x >> 1) - 2, y >> 1),
                            (pcmfa & 3) | ((pcmfa >> 14) & 0xc));
                    }
                }
            }

            // H
            if (y != 0)
            {
                uint32_t hbs;
                const unsigned int bh_l = bv_l - 16;
                const unsigned int bh_r = cb_x + ctb_size >= cb_r ? cb_r : cb_x + ctb_size - 16;
                const int8_t * const qta = s->qp_y_tab + ((y - 1) >> log2_min_cb_size) * s->ps.sps->min_cb_width;
                const int8_t * const qtb = s->qp_y_tab + (y >> log2_min_cb_size) * s->ps.sps->min_cb_width;

                // H left - mostly separated out so we only need a uint32_t hbs
                // Stub is width 8 to the left of bounds, but width 16 internally
                if ((hbs = hbs_get32(s, bh_l, cb_x, y) & 0x22U) != 0)
                {
                    unsigned int pcmfa = pcm4(s, bh_l, y - 1);

                    // Chop off bits we don't want...
                    if (bh_l < bounds.x) {
                        pcmfa |= 0x10001; // TL|BL pre rearrangement
                        hbs &= ~3;  // Make BS 0
                    }

                    // Double check we still want this
                    if (hbs != 0 && (~pcmfa & 0x30003) != 0)
                    {
                        const unsigned int x = bh_l;
                        const int qp0 = (qta[x >> log2_min_cb_size] + qtb[x >> log2_min_cb_size] + 1) >> 1;
                        const int qp1 = (qta[(x + 8) >> log2_min_cb_size] + qtb[(x + 8) >> log2_min_cb_size] + 1) >> 1;
                        const uint8_t * const tc = tctable + 2 + (dbp - 1)->tc_offset;

                        s->hevcdsp.hevc_h_loop_filter_uv(av_rpi_sand_frame_pos_c(s->frame, x >> 1, y >> 1),
                            frame_stride1(s->frame, 1),
                            ((hbs & 2) == 0 ? 0 : (tc[tcq_u[qp0]] << 0) | (tc[tcq_v[qp0]] << 8)) |
                                ((hbs & 0x20) == 0 ? 0 : (tc[tcq_u[qp1]] << 16) | (tc[tcq_v[qp1]] << 24)),
                            (pcmfa & 3) | ((pcmfa >> 14) & 0xc));
                    }
                }

                // H main
                if ((hbs = (hbs_get32(s, cb_x, bh_r, y) & 0x22222222U)) != 0)
                {
                    unsigned int x;
                    unsigned int pcmfa = pcm4(s, cb_x, y - 1);  // Might like to mask out far right writes but probably not worth it

                    for (x = cb_x; hbs != 0; x += 16, hbs >>= 8, pcmfa >>= 2)
                    {
                        if ((hbs & 0xff) != 0 && (~pcmfa & 0x30003) != 0)
                        {
                            const int qp0 = (qta[x >> log2_min_cb_size] + qtb[x >> log2_min_cb_size] + 1) >> 1;
                            const int qp1 = (qta[(x + 8) >> log2_min_cb_size] + qtb[(x + 8) >> log2_min_cb_size] + 1) >> 1;
                            const uint8_t * const tc = tctable + 2 + dbp->tc_offset;

                            s->hevcdsp.hevc_h_loop_filter_uv(av_rpi_sand_frame_pos_c(s->frame, x >> 1, y >> 1),
                                frame_stride1(s->frame, 1),
                                ((hbs & 2) == 0 ? 0 : (tc[tcq_u[qp0]] << 0) | (tc[tcq_v[qp0]] << 8)) |
                                    ((hbs & 0x20) == 0 ? 0 : (tc[tcq_u[qp1]] << 16) | (tc[tcq_v[qp1]] << 24)),
                                (pcmfa & 3) | ((pcmfa >> 14) & 0xc));
                        }
                    }
                }
            }
        }
    }
}

static inline unsigned int off_boundary(const unsigned int x, const unsigned int log2_n)
{
    return x & ~(~0U << log2_n);
}

static inline void hbs_set(const HEVCRpiContext * const s, const unsigned int x, const unsigned int y, const uint32_t mask, uint32_t bsf)
{
    av_assert2((y & 7) == 0);

    // This doesn't have the same simultainious update issues that bsf_stash
    // does (other threads will have a different y) so we can do it the easy way
    if ((bsf &= mask) != 0)
        *bs_ptr32(s->bs_horizontal, s->bs_stride2, x, y) |= bsf << ((x >> 1) & 31);
}


static void vbs_set(const HEVCRpiContext * const s, const unsigned int x, const unsigned int y, const uint32_t mask, uint32_t bsf)
{
    // We arrange this in a slightly odd fashion but it lines up with
    // how we are going to use it in the actual deblock code & it is easier
    // to do the contortions here than there
    //
    // Arrange (LE) {x0y0, x0y4, x8y0, x8,y4}, {x16y0, x16y4, x24y0, x24y4},...

    av_assert2((x & 7) == 0);

    if ((bsf &= mask) != 0)
    {
        uint8_t *p = bs_ptr8(s->bs_vertical, s->bs_stride2, x, y);
        const unsigned int sh = ((x & 8) | (y & 4)) >> 1;

        if (mask <= 0xf)
        {
            *p |= (bsf << sh);
        }
        else
        {
            do {
                *p |= (bsf & 0xf) << sh;
                p += HEVC_RPI_BS_STRIDE1_BYTES;
            } while ((bsf >>= 4) != 0);
        }
    }
}

static inline uint32_t bsf_mv(const HEVCRpiContext * const s,
                              const unsigned int rep, const unsigned int dup,
                              const unsigned int mvf_stride0,
                              const unsigned int mvf_stride1,
                              const RefPicList * const rpl_p, const RefPicList * const rpl_q,
                              const HEVCRpiMvField * const mvf_p, const HEVCRpiMvField * const mvf_q)
{
    return s->hevcdsp.hevc_deblocking_boundary_strengths(rep, dup,
            mvf_p, mvf_q,
            rpl_p[0].list, rpl_p[1].list, rpl_q[0].list, rpl_q[1].list,
            sizeof(HEVCRpiMvField) * mvf_stride0, sizeof(HEVCRpiMvField) * mvf_stride1);
}


void ff_hevc_rpi_deblocking_boundary_strengths(const HEVCRpiContext * const s,
                                               const HEVCRpiLocalContext * const lc,
                                               const unsigned int x0, const unsigned int y0,
                                               const unsigned int log2_trafo_size,
                                               const int is_coded_block)
{
    const HEVCRpiMvField * const mvf_curr      = mvf_stash_ptr(s, lc, x0, y0);
    const unsigned int log2_min_pu_size = LOG2_MIN_PU_SIZE;
    const RefPicList * const rpl        = s->refPicList;
    // Rep count for bsf_mv when running with min_pu chuncks
    const unsigned int log2_rep_min_pu  = log2_trafo_size <= log2_min_pu_size ? 0 : log2_trafo_size - log2_min_pu_size;
    const unsigned int boundary_flags   = s->sh.no_dblk_boundary_flags & lc->boundary_flags;
    const unsigned int trafo_size       = (1U << log2_trafo_size);
    const uint32_t bsf_mask             = log2_trafo_size > 5 ? ~0U : (1U << (trafo_size >> 1)) - 1;
    const uint32_t bsf_cbf              = (bsf_mask & 0x55555555);

    // Do we cover a pred split line?
    const int has_x_split = x0 < lc->cu.x_split && x0 + trafo_size > lc->cu.x_split;
    const int has_y_split = y0 < lc->cu.y_split && y0 + trafo_size > lc->cu.y_split;

    uint32_t bsf_h;
    uint32_t bsf_v;

#ifdef DISABLE_STRENGTHS
    return;
#endif

    // We are always on a size boundary
    av_assert2((x0 & (trafo_size - 1)) == 0);
    av_assert2((y0 & (trafo_size - 1)) == 0);
    // log2_trafo_size not really a transform size; we can have to deal
    // with size 2^6 blocks
    av_assert2(log2_trafo_size >= 2 && log2_trafo_size <= 6);

    // Retrieve and update coded (b0), intra (b1) bs flags
    //
    // Store on min width (rather than uint32_t) to avoid possible issues
    // with another thread on another core running wpp using the same
    // memory (min CTB = 16 pels = 4 bsf els = 8 bits)
    //
    // In bsf BS=2 is represented by 3 as it is much easier to test & set
    // and the actual deblock code tests for 0 and b1 set/not-set so 2 and
    // 3 will work the same
    {
        // Given where we are called from is_cbf_luma & is_intra will be constant over the block
        const uint32_t bsf0 =  (lc->cu.pred_mode == MODE_INTRA) ? bsf_mask : is_coded_block ? bsf_cbf : 0;
        uint8_t *const p = s->bsf_stash_up + (x0 >> 4);
        uint8_t *const q = s->bsf_stash_left + (y0 >> 4);

        switch (log2_trafo_size)
        {
            case 2:
            case 3:
            {
                const unsigned int sh_h = (x0 >> 1) & 7;
                const unsigned int sh_v = (y0 >> 1) & 7;
                bsf_h = *p;
                bsf_v = *q;
                *p = (bsf_h & ~(bsf_mask << sh_h)) | (bsf0 << sh_h);
                *q = (bsf_v & ~(bsf_mask << sh_v)) | (bsf0 << sh_v);
                bsf_h >>= sh_h;
                bsf_v >>= sh_v;
                break;
            }
            case 4:
                bsf_h = *p;
                bsf_v = *q;
                *p = bsf0;
                *q = bsf0;
                break;
            case 5:
                bsf_h = *(uint16_t *)p;
                bsf_v = *(uint16_t *)q;
                *(uint16_t *)p = bsf0;
                *(uint16_t *)q = bsf0;
                break;
            case 6:
            default:
                bsf_h = *(uint32_t *)p;
                bsf_v = *(uint32_t *)q;
                *(uint32_t *)p = bsf0;
                *(uint32_t *)q = bsf0;
                break;
        }

        bsf_h |= bsf0;
        bsf_v |= bsf0;
    }

    // Do Horizontal
    if ((y0 & 7) == 0)
    {
        // Boundary upper
        if (y0 != 0 &&
            (off_boundary(y0, s->ps.sps->log2_ctb_size) ||
             (boundary_flags & (BOUNDARY_UPPER_SLICE | BOUNDARY_UPPER_TILE)) == 0))
        {
            // Look at MVs (BS=1) if we don't already has a full set of bs bits
            if ((~bsf_h & bsf_cbf) != 0 && (y0 == lc->cu.y || y0 == lc->cu.y_split))
            {
                // If we aren't on the top boundary we must be in the middle
                // and in that case we know where mvf can change
                const unsigned int log2_rep = (y0 == lc->cu.y) ? log2_rep_min_pu : has_x_split ? 1 : 0;
                const RefPicList *const rpl_top = !off_boundary(y0, s->ps.sps->log2_ctb_size) ?
                      s->rpl_up[x0 >> s->ps.sps->log2_ctb_size] :
                      rpl;

                bsf_h |= bsf_mv(s, 1 << log2_rep, trafo_size >> (2 + log2_rep),
                    trafo_size >> (log2_min_pu_size + log2_rep),
                    trafo_size >> (log2_min_pu_size + log2_rep),
                    rpl, rpl_top,
                    mvf_curr, mvf_ptr(s, lc, x0, y0, x0, y0 - 1));
            }

            // Finally put the results into bs
            hbs_set(s, x0, y0, bsf_mask, bsf_h);
        }

        // Max of 1 pu internal split - ignore if not on 8pel boundary
        if (has_y_split && !off_boundary(lc->cu.y_split, 3))
        {
            const HEVCRpiMvField * const mvf = mvf_stash_ptr(s, lc, x0, lc->cu.y_split);
            // If we have the x split as well then it must be in the middle
            const unsigned int log2_rep = has_x_split ? 1 : 0;

            hbs_set(s, x0, lc->cu.y_split, bsf_mask,
                bsf_mv(s, 1 << log2_rep, trafo_size >> (2 + log2_rep),
                   trafo_size >> (log2_min_pu_size + log2_rep),
                   trafo_size >> (log2_min_pu_size + log2_rep),
                   rpl, rpl,
                   mvf, mvf - MVF_STASH_WIDTH_PU));
        }
    }

    // And again for vertical - same logic as horizontal just in the other direction
    if ((x0 & 7) == 0)
    {
        // Boundary left
        if (x0 != 0 &&
            (off_boundary(x0, s->ps.sps->log2_ctb_size) ||
             (boundary_flags & (BOUNDARY_LEFT_SLICE | BOUNDARY_LEFT_TILE)) == 0))
        {
            if ((~bsf_v & bsf_cbf) != 0 && (x0 == lc->cu.x || x0 == lc->cu.x_split))
            {
                const unsigned int log2_rep = (x0 == lc->cu.x) ? log2_rep_min_pu : has_y_split ? 1 : 0;
                const RefPicList *const rpl_left = !off_boundary(x0, s->ps.sps->log2_ctb_size) ?
                    s->rpl_left[y0 >> s->ps.sps->log2_ctb_size] :
                    rpl;

                bsf_v |= bsf_mv(s, 1 << log2_rep, trafo_size >> (2 + log2_rep),
                    (MVF_STASH_WIDTH_PU << log2_trafo_size) >> (log2_min_pu_size + log2_rep),
                    (mvf_left_stride(s, x0, x0 - 1) << log2_trafo_size) >> (log2_min_pu_size + log2_rep),
                    rpl, rpl_left,
                    mvf_curr, mvf_ptr(s, lc, x0, y0, x0 - 1, y0));
            }

            vbs_set(s, x0, y0, bsf_mask, bsf_v);
        }

        if (has_x_split && !off_boundary(lc->cu.x_split, 3))
        {
            const HEVCRpiMvField *const mvf = mvf_stash_ptr(s, lc, lc->cu.x_split, y0);
            const unsigned int log2_rep = has_y_split ? 1 : 0;

            vbs_set(s, lc->cu.x_split, y0, bsf_mask,
                bsf_mv(s, 1 << log2_rep, trafo_size >> (2 + log2_rep),
                   (MVF_STASH_WIDTH_PU << log2_trafo_size) >> (log2_min_pu_size + log2_rep),
                   (MVF_STASH_WIDTH_PU << log2_trafo_size) >> (log2_min_pu_size + log2_rep),
                   rpl, rpl,
                   mvf, mvf - 1));
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

    const unsigned int br = bounds.x + bounds.w;
    const unsigned int bb = bounds.y + bounds.h;

    const int x_end = (br >= s->ps.sps->width);
    const int y_end = (bb >= s->ps.sps->height);

    // Deblock may not touch the edges of the bound as they are still needed
    // for Intra pred
    //
    // Deblock is disabled with a per-slice flag
    // Given that bounds may cover multiple slices & we dblock outside bounds
    // anyway we can't avoid deblock using that flag - about the only thing we
    // could do is have a "no deblock seen yet" flag but it doesn't really
    // seem worth the effort

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

        if (s->ps.sps->sao_enabled)
        {
            for (y = yt; y < yb; y += ctb_size) {
                for (x = xl; x < xr; x += ctb_size) {
                    sao_filter_CTB(s, x, y);
                }
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

