/*
 * HEVC video decoder
 *
 * Copyright (C) 2012 - 2013 Guillaume Martres
 * Copyright (C) 2013 Anand Meher Kotra
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

#include "hevc.h"
#include "rpi_hevcdec.h"

static av_always_inline int
is_eq_mer(const unsigned int plevel,
    const unsigned int xN, const unsigned int yN,
    const unsigned int xP, const unsigned int yP)
{
    return (((xN ^ xP) | (yN ^ yP)) >> plevel) == 0;
}

// check if the mv's and refidx are the same between A and B
static av_always_inline int compare_mv_ref_idx(const HEVCRpiMvField * const a, const HEVCRpiMvField * const b)
{
    return a->pred_flag == b->pred_flag &&
        ((a->pred_flag & PF_L0) == 0 || (a->ref_idx[0] == b->ref_idx[0] && a->xy[0] == b->xy[0])) &&
        ((a->pred_flag & PF_L1) == 0 || (a->ref_idx[1] == b->ref_idx[1] && a->xy[1] == b->xy[1]));
    return 0;
}

/*
 * 8.5.3.1.7  temporal luma motion vector prediction
 */
static int temporal_luma_motion_vector(const HEVCRpiContext * const s,
                                       const HEVCRpiLocalContext * const lc, const int x0, const int y0,
                                       const int nPbW, const int nPbH, const int refIdxLx,
                                       MvXY * const mvLXCol, const int X)
{
    int x, y;
    const ColMv * cmv = NULL;

    HEVCRpiFrame * const col_ref = s->ref->collocated_ref;
    const RefPicList * const refPicList = s->refPicList + X;
    const int cur_lt = refPicList->isLongTerm[refIdxLx];

    *mvLXCol = 0;
    // Unlikely but we might have a col_ref IDR frame!
    if (col_ref->col_mvf == NULL)
        return 0;

    ff_hevc_rpi_progress_wait_mv(s, lc->jb0, col_ref, y0 + nPbH);

    //bottom right collocated motion vector
    x = x0 + nPbW;
    y = y0 + nPbH;

    if ((y0 >> s->ps.sps->log2_ctb_size) == (y >> s->ps.sps->log2_ctb_size) &&
        y < s->ps.sps->height &&
        x < s->ps.sps->width)
    {
        const ColMvField * const col = col_ref->col_mvf + (x >> 4) +
            (y >> 4) * s->col_mvf_stride;

        if (col->L[0].poc != COL_POC_INTRA &&
            (col->L[1].poc == COL_POC_INTRA ||
             (s->no_backward_pred_flag ? s->sh.collocated_list == L1 : X == 0)))
        {
            cmv = col->L + 0;
        }
        else if (col->L[1].poc != COL_POC_INTRA)
        {
            cmv = col->L + 1;
        }
    }

    // derive center collocated motion vector
    if (cmv == NULL || COL_POC_IS_LT(cmv->poc) != cur_lt)
    {
        cmv = NULL;
        x                  = x0 + (nPbW >> 1);
        y                  = y0 + (nPbH >> 1);

        {
            const ColMvField * const col = col_ref->col_mvf + (x >> 4) +
              (y >> 4) * s->col_mvf_stride;

            if (col->L[0].poc != COL_POC_INTRA &&
              (col->L[1].poc == COL_POC_INTRA ||
               (s->no_backward_pred_flag ? s->sh.collocated_list == L1 : X == 0)))
            {
              cmv = col->L + 0;
            }
            else if (col->L[1].poc != COL_POC_INTRA)
            {
              cmv = col->L + 1;
            }
        }
    }

    if (cmv == NULL || cur_lt != COL_POC_IS_LT(cmv->poc))
        return 0;

    {
        const int col_poc  = col_ref->poc;
        const int ref_poc  = refPicList->list[refIdxLx];

        *mvLXCol = (cur_lt ||
                        cmv->poc == col_poc ||
                        COL_POC_DIFF(col_poc, cmv->poc) == s->poc - ref_poc) ?
                    cmv->xy :
                    mv_scale_xy(cmv->xy, COL_POC_DIFF(col_poc, cmv->poc), s->poc - ref_poc);
    }

    return cmv != NULL;
}

static inline int mvf_eq(const HEVCRpiMvField * const a, const HEVCRpiMvField * const b)
{
    return b != NULL && compare_mv_ref_idx(a, b);
}



/*
 * 8.5.3.1.2  Derivation process for spatial merging candidates
 */
static inline const HEVCRpiMvField *
derive_spatial_merge_candidates(
    const HEVCRpiContext * const s,
    const HEVCRpiLocalContext * const lc,
    const unsigned int x0, const unsigned int y0,
    const unsigned int nPbW, const unsigned int nPbH,
    const unsigned int avail,
    const unsigned int part_idx,
    const unsigned int merge_idx,
    HEVCRpiMvField * const mvf_t)
{
    const unsigned int parts_a1 = (1 << PART_Nx2N) | (1 << PART_nLx2N) | (1 << PART_nRx2N);
    const unsigned int parts_b1 = (1 << PART_2NxN) | (1<< PART_2NxnU) | (1 << PART_2NxnD);

    const HEVCRpiMvField * mvf_a1 = mvf_ptr(s, lc, x0, y0, x0 - 1, y0 + nPbH - 1);
    const HEVCRpiMvField * mvf_a0 = mvf_a1 + mvf_left_stride(s, x0, x0 - 1);
    const HEVCRpiMvField * mvf_b1 = mvf_ptr(s, lc, x0, y0, x0 + nPbW - 1, y0 - 1);
    const HEVCRpiMvField * mvf_b0 = mvf_b1 + 1;
    const unsigned int plevel = s->ps.pps->log2_parallel_merge_level;
    const unsigned int part_mode = lc->cu.part_mode;

    const HEVCRpiMvField * perm[4];
    unsigned int nb_merge_cand = 0;

    // singleMCLFlag => part_idx == 0 so no need to test for it
    if ((avail & AVAIL_L) == 0 ||
        (part_idx == 1 &&
            ((parts_a1 >> part_mode) & 1) != 0 ||
                is_eq_mer(plevel, x0 - 1, y0 + nPbH - 1, x0, y0)) ||
        mvf_a1->pred_flag == PF_INTRA)
    {
        mvf_a1 = NULL;
    }
    else
    {
        if (merge_idx == nb_merge_cand)
            return mvf_a1;
        perm[nb_merge_cand++] = mvf_a1;
    }

    if ((avail & AVAIL_U) == 0 ||
            (part_idx == 1 &&
               ((parts_b1 >> part_mode) & 1) != 0 ||
                   is_eq_mer(plevel, x0 + nPbW - 1, y0 - 1, x0, y0)) ||
            mvf_b1->pred_flag == PF_INTRA)
    {
        mvf_b1 = NULL;
    }
    else if (!mvf_eq(mvf_b1, mvf_a1))
    {
        if (merge_idx == nb_merge_cand)
            return mvf_b1;
        perm[nb_merge_cand++] = mvf_b1;
    }

    // above right spatial merge candidate
    // Never need mvf_b0 again so don't bother zeroing if navail
    if ((avail & AVAIL_UR) != 0 &&
        !is_eq_mer(plevel, x0 + nPbW, y0 - 1, x0, y0) &&
        mvf_b0->pred_flag != PF_INTRA &&
        !mvf_eq(mvf_b0, mvf_b1))
    {
        if (merge_idx == nb_merge_cand)
            return mvf_b0;
        perm[nb_merge_cand++] = mvf_b0;
    }

    // left bottom spatial merge candidate
    // Never need mvf_a0 again so don't bother zeroing if navail
    if ((avail & AVAIL_DL) != 0 &&
        !is_eq_mer(plevel, x0 - 1, y0 + nPbH, x0, y0) &&
        mvf_a0->pred_flag != PF_INTRA &&
        !mvf_eq(mvf_a0, mvf_a1))
    {
        if (merge_idx == nb_merge_cand)
            return mvf_a0;
        perm[nb_merge_cand++] = mvf_a0;
    }

    // above left spatial merge candidate
    if (nb_merge_cand != 4 &&
        (avail & AVAIL_UL) != 0 &&
        !is_eq_mer(plevel, x0 - 1, y0 - 1, x0, y0))
    {
        const HEVCRpiMvField * mvf_b2 = mvf_ptr(s, lc, x0, y0, x0 - 1, y0 - 1);  // UL

        if (mvf_b2->pred_flag != PF_INTRA &&
            !mvf_eq(mvf_b2, mvf_a1) &&
            !mvf_eq(mvf_b2, mvf_b1))
        {
            if (merge_idx == nb_merge_cand)
                return mvf_b2;
            perm[nb_merge_cand++] = mvf_b2;
        }
    }

    // temporal motion vector candidate
    if (s->sh.slice_temporal_mvp_enabled_flag)
    {
        static const HEVCRpiMvField mvf_z = {{0}};

        *mvf_t = mvf_z;

        if (temporal_luma_motion_vector(s, lc, x0, y0, nPbW, nPbH,
                                        0, mvf_t->xy + 0, 0))
            mvf_t->pred_flag = PF_L0;

        if (s->sh.slice_type == HEVC_SLICE_B &&
                temporal_luma_motion_vector(s, lc, x0, y0, nPbW, nPbH,
                                            0, mvf_t->xy + 1, 1))
            mvf_t->pred_flag |= PF_L1;

        if (mvf_t->pred_flag != 0)
        {
            if (merge_idx == nb_merge_cand)
                return mvf_t;
            perm[nb_merge_cand++] = mvf_t;
        }
    }

    // combined bi-predictive merge candidates  (applies for B slices)
    if (s->sh.slice_type == HEVC_SLICE_B && nb_merge_cand > 1)
    {
        unsigned int comb_idx = 0;
        const unsigned int cand_count = nb_merge_cand * (nb_merge_cand - 1);
        const RefPicList * const refPicList = s->refPicList;

        for (comb_idx = 0; comb_idx < cand_count; comb_idx++)
        {
            static const uint8_t l0_l1_cand_idx[12][2] = {
                { 0, 1, },
                { 1, 0, },
                { 0, 2, },
                { 2, 0, },
                { 1, 2, },
                { 2, 1, },
                { 0, 3, },
                { 3, 0, },
                { 1, 3, },
                { 3, 1, },
                { 2, 3, },
                { 3, 2, },
            };

            const unsigned int l0_cand_idx = l0_l1_cand_idx[comb_idx][0];
            const unsigned int l1_cand_idx = l0_l1_cand_idx[comb_idx][1];
            const HEVCRpiMvField * const mvf_c0 = perm[l0_cand_idx];
            const HEVCRpiMvField * const mvf_c1 = perm[l1_cand_idx];

            if ((mvf_c0->pred_flag & PF_L0) != 0 &&
                (mvf_c1->pred_flag & PF_L1) != 0 &&
                (refPicList[0].list[mvf_c0->ref_idx[0]] != refPicList[1].list[mvf_c1->ref_idx[1]] ||
                 mvf_c0->xy[0] != mvf_c1->xy[1]))
            {
                if (merge_idx == nb_merge_cand++)
                {
                    // Need to be a bit careful as we will construct mvf_t and we
                    // may already be using that as one of our condidates
                    // so build & copy rather than build in place
                    const HEVCRpiMvField mvf_m = {
                        .xy = {
                            mvf_c0->xy[0],
                            mvf_c1->xy[1]},
                        .ref_idx = {
                            mvf_c0->ref_idx[0],
                            mvf_c1->ref_idx[1]},
                        .pred_flag = PF_BI
                    };
                    *mvf_t = mvf_m;
                    return mvf_t;
                }
            }
        }
    }

    // "append" Zero motion vector candidates
    {
        const unsigned int nb_refs = (s->sh.slice_type == HEVC_SLICE_B) ?
                            FFMIN(s->sh.nb_refs[0], s->sh.nb_refs[1]) : s->sh.nb_refs[0];
        const unsigned int zero_idx = merge_idx - nb_merge_cand;

        const HEVCRpiMvField mvf_m = {
            .xy = {0, 0},
            .ref_idx = {
                zero_idx < nb_refs ? zero_idx : 0,
                (s->sh.slice_type == HEVC_SLICE_B && zero_idx < nb_refs) ? zero_idx : 0},
            .pred_flag = (s->sh.slice_type == HEVC_SLICE_B) ? PF_BI : PF_L0
        };

        *mvf_t = mvf_m;
        return mvf_t;
    }
}


// 8.5.3.1.1 Derivation process of luma Mvs for merge mode
void ff_hevc_rpi_luma_mv_merge_mode(const HEVCRpiContext * const s, HEVCRpiLocalContext * const lc, int x0, int y0, int nPbW,
                                int nPbH, int log2_cb_size, int part_idx,
                                int merge_idx, HEVCRpiMvField * const mv)
{
    const HEVCRpiMvField * mvf_m = (s->ps.pps->log2_parallel_merge_level > 2 && log2_cb_size == 3) ?
        derive_spatial_merge_candidates(s, lc, lc->cu.x, lc->cu.y, 8, 8,
                                        ff_hevc_rpi_tb_avail_flags(s, lc, lc->cu.x, lc->cu.y, 8, 8),
                                        0, merge_idx, mv) :
        derive_spatial_merge_candidates(s, lc, x0, y0, nPbW, nPbH,
                                        ff_hevc_rpi_tb_avail_flags(s, lc, x0, y0, nPbW, nPbH),
                                        part_idx, merge_idx, mv);

    if (mvf_m != mv)
        *mv = *mvf_m;

    if (mv->pred_flag == PF_BI && (nPbW + nPbH) == 12)
        mv->pred_flag = PF_L0;
}


static av_always_inline const MvXY *
mvf_same_poc(const RefPicList * const rpl, const unsigned int pfi0, const unsigned int pfi1, const int poc0, const HEVCRpiMvField * const mvf)
{
    if (mvf != NULL)
    {
        if (((mvf->pred_flag >> pfi0) & 1) != 0 && rpl[pfi0].list[mvf->ref_idx[pfi0]] == poc0)
            return mvf->xy + pfi0;
        if (((mvf->pred_flag >> pfi1) & 1) != 0 && rpl[pfi1].list[mvf->ref_idx[pfi1]] == poc0)
            return mvf->xy + pfi1;
    }
    return NULL;
}

static av_always_inline const MvXY *
mvf_other_poc(const RefPicList * const rpl, const unsigned int pfi0, const unsigned int pfi1,
              const int islt0, const int poc0, const int poc_cur,
              MvXY * const mv_t, const HEVCRpiMvField * const mvf)
{
    if (mvf != NULL)
    {
        if (((mvf->pred_flag >> pfi0) & 1) != 0 && rpl[pfi0].isLongTerm[mvf->ref_idx[pfi0]] == islt0)
        {
            const int poc1 = rpl[pfi0].list[mvf->ref_idx[pfi0]];
            if (islt0 || poc1 == poc0) {
                return mvf->xy + pfi0;
            }
            *mv_t = mv_scale_xy(mvf->xy[pfi0], poc_cur - poc1, poc_cur - poc0);
            return mv_t;
        }
        if (((mvf->pred_flag >> pfi1) & 1) != 0 && rpl[pfi1].isLongTerm[mvf->ref_idx[pfi1]] == islt0)
        {
            const int poc1 = rpl[pfi1].list[mvf->ref_idx[pfi1]];
            if (islt0 || poc1 == poc0) {
                return mvf->xy + pfi1;
            }
            *mv_t = mv_scale_xy(mvf->xy[pfi1], poc_cur - poc1, poc_cur - poc0);
            return mv_t;
        }
    }
    return NULL;
}

void ff_hevc_rpi_luma_mv_mvp_mode(const HEVCRpiContext * const s, HEVCRpiLocalContext * const lc,
    const unsigned int x0, const unsigned int y0,
    const unsigned int nPbW, const unsigned int nPbH,
    const unsigned int avail,
    HEVCRpiMvField * const mv,
    const unsigned int mvp_lx_flag, const unsigned int LX)
{
    const unsigned int pfi0 = LX;
    const unsigned int pfi1 = LX == 0 ? 1 : 0;
    const RefPicList * const rpl = s->refPicList;
    const int poc0 = rpl[LX].list[mv->ref_idx[LX]];
    const int poc_cur = s->poc;
    const int islt0 = rpl[LX].isLongTerm[mv->ref_idx[LX]];

    const HEVCRpiMvField * mvf_a1 = mvf_ptr(s, lc, x0, y0, x0 - 1, y0 + nPbH - 1);
    const HEVCRpiMvField * mvf_a0 = mvf_a1 + mvf_left_stride(s, x0, x0 - 1);
    const HEVCRpiMvField * mvf_b2 = mvf_ptr(s, lc, x0, y0, x0 - 1, y0 - 1);  // UL
    const HEVCRpiMvField * mvf_b1 = mvf_ptr(s, lc, x0, y0, x0 + nPbW - 1, y0 - 1);
    const HEVCRpiMvField * mvf_b0 = mvf_b1 + 1;
    const MvXY * mva = NULL;
    const MvXY * mvb;
    MvXY * const mv_rv = mv->xy + LX;
    MvXY mvt_a, mvt_b;

    *mv_rv = 0;

    if ((avail & AVAIL_DL) == 0 || mvf_a0->pred_flag == PF_INTRA)
        mvf_a0 = NULL;
    else if ((mva = mvf_same_poc(rpl, pfi0, pfi1, poc0, mvf_a0)) != NULL && mvp_lx_flag == 0)
        goto use_mva;

    if ((avail & AVAIL_L) == 0 || mvf_a1->pred_flag == PF_INTRA)
        mvf_a1 = NULL;

    if (mva == NULL &&
        (mva = mvf_same_poc(rpl, pfi0, pfi1, poc0, mvf_a1)) == NULL &&
        (mva = mvf_other_poc(rpl, pfi0, pfi1, islt0, poc0, poc_cur, &mvt_a, mvf_a0)) == NULL)
        mva = mvf_other_poc(rpl, pfi0, pfi1, islt0, poc0, poc_cur, &mvt_a, mvf_a1);

    if (mvp_lx_flag == 0 && mva != NULL)
        goto use_mva;

    if ((avail & AVAIL_UR) == 0 || mvf_b0->pred_flag == PF_INTRA)
        mvf_b0 = NULL;
    if ((avail & AVAIL_U) == 0 || mvf_b1->pred_flag == PF_INTRA)
        mvf_b1 = NULL;
    if ((avail & AVAIL_UL) == 0 || mvf_b2->pred_flag == PF_INTRA)
        mvf_b2 = NULL;

    if ((mvb = mvf_same_poc(rpl, pfi0, pfi1, poc0, mvf_b0)) == NULL &&
        (mvb = mvf_same_poc(rpl, pfi0, pfi1, poc0, mvf_b1)) == NULL)
        mvb = mvf_same_poc(rpl, pfi0, pfi1, poc0, mvf_b2);

    if (mvf_a0 == NULL && mvf_a1 == NULL) {
        mva = mvb;
        if (mvp_lx_flag == 0 && mva != NULL)
            goto use_mva;

        if ((mvb = mvf_other_poc(rpl, pfi0, pfi1, islt0, poc0, poc_cur, &mvt_b, mvf_b0)) == NULL &&
            (mvb = mvf_other_poc(rpl, pfi0, pfi1, islt0, poc0, poc_cur, &mvt_b, mvf_b1)) == NULL)
            mvb = mvf_other_poc(rpl, pfi0, pfi1, islt0, poc0, poc_cur, &mvt_b, mvf_b2);
    }

    if (mva == NULL) {
        mva = mvb;
        mvb = NULL;
    }

    if (mvb != NULL && *mva == *mvb)  // If A == B then ignore B
        mvb = NULL;

    if (mvp_lx_flag == 0 && mva != NULL) {
        goto use_mva;
    }
    else if (mvp_lx_flag != 0 && mvb != NULL) {
        *mv_rv = *mvb;
    }
    else if (s->sh.slice_temporal_mvp_enabled_flag && ((mvp_lx_flag == 0 && mva == NULL) || (mvp_lx_flag != 0 && mva != NULL))) {
        temporal_luma_motion_vector(s, lc, x0, y0, nPbW,
                                    nPbH, mv->ref_idx[LX],
                                    mv_rv, LX);
    }
    return;

use_mva:
    *mv_rv = *mva;
    return;
}

