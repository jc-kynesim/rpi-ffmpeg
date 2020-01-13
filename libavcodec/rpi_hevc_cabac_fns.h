/*
 * HEVC CABAC decoding
 *
 * Copyright (C) 2012 - 2013 Guillaume Martres
 * Copyright (C) 2012 - 2013 Gildas Cocherel
 * Copyright (C) 2012 - 2013 Gildas Cocherel
 * Copyright (C) 2018 John Cox
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


#ifndef AVCODEC_RPI_HEVC_CABAC_FNS_H
#define AVCODEC_RPI_HEVC_CABAC_FNS_H

#include "config.h"
#include "rpi_hevcdec.h"

void ff_hevc_rpi_save_states(HEVCRpiContext *s, const HEVCRpiLocalContext * const lc);
int ff_hevc_rpi_cabac_init_decoder(HEVCRpiLocalContext * const lc);
void ff_hevc_rpi_cabac_init(const HEVCRpiContext * const s, HEVCRpiLocalContext *const lc, const unsigned int ctb_flags);
int ff_hevc_rpi_sao_type_idx_decode(HEVCRpiLocalContext * const lc);
int ff_hevc_rpi_sao_band_position_decode(HEVCRpiLocalContext * const lc);
int ff_hevc_rpi_sao_offset_abs_decode(const HEVCRpiContext * const s, HEVCRpiLocalContext * const lc);
int ff_hevc_rpi_sao_offset_sign_decode(HEVCRpiLocalContext * const lc);
int ff_hevc_rpi_sao_eo_class_decode(HEVCRpiLocalContext * const lc);
int ff_hevc_rpi_part_mode_decode(const HEVCRpiContext * const s, HEVCRpiLocalContext * const lc, const int log2_cb_size);
int ff_hevc_rpi_mpm_idx_decode(HEVCRpiLocalContext * const lc);
int ff_hevc_rpi_rem_intra_luma_pred_mode_decode(HEVCRpiLocalContext * const lc);
int ff_hevc_rpi_intra_chroma_pred_mode_decode(HEVCRpiLocalContext * const lc);
int ff_hevc_rpi_merge_idx_decode(const HEVCRpiContext * const s, HEVCRpiLocalContext * const lc);
int ff_hevc_rpi_inter_pred_idc_decode(HEVCRpiLocalContext * const lc, int nPbW, int nPbH);
int ff_hevc_rpi_ref_idx_lx_decode(HEVCRpiLocalContext * const lc, const int num_ref_idx_lx);
int ff_hevc_rpi_log2_res_scale_abs(HEVCRpiLocalContext * const lc, const int idx);

//int ff_hevc_rpi_cu_qp_delta_sign_flag(HEVCRpiLocalContext * const lc);
int ff_hevc_rpi_cu_qp_delta(HEVCRpiLocalContext * const lc);
int ff_hevc_rpi_cu_chroma_qp_offset_idx(const HEVCRpiContext * const s, HEVCRpiLocalContext * const lc);
void ff_hevc_rpi_hls_residual_coding(const HEVCRpiContext * const s, HEVCRpiLocalContext * const lc,
                                const int x0, const int y0,
                                const int log2_trafo_size, const enum ScanType scan_idx,
                                const int c_idx);

MvXY ff_hevc_rpi_hls_mvd_coding(HEVCRpiLocalContext * const lc);
int ff_hevc_rpi_cabac_overflow(const HEVCRpiLocalContext * const lc);

#define HEVC_BIN_SAO_MERGE_FLAG                         0
#define HEVC_BIN_SAO_TYPE_IDX                           1
#define HEVC_BIN_SAO_EO_CLASS                           2
#define HEVC_BIN_SAO_BAND_POSITION                      2
#define HEVC_BIN_SAO_OFFSET_ABS                         2
#define HEVC_BIN_SAO_OFFSET_SIGN                        2
#define HEVC_BIN_END_OF_SLICE_FLAG                      2
#define HEVC_BIN_SPLIT_CODING_UNIT_FLAG                 2
#define HEVC_BIN_CU_TRANSQUANT_BYPASS_FLAG              5
#define HEVC_BIN_SKIP_FLAG                              6
#define HEVC_BIN_CU_QP_DELTA                            9
#define HEVC_BIN_PRED_MODE                              12
#define HEVC_BIN_PART_MODE                              13
#define HEVC_BIN_PCM_FLAG                               17
#define HEVC_BIN_PREV_INTRA_LUMA_PRED_MODE              17
#define HEVC_BIN_MPM_IDX                                18
#define HEVC_BIN_REM_INTRA_LUMA_PRED_MODE               18
#define HEVC_BIN_INTRA_CHROMA_PRED_MODE                 18
#define HEVC_BIN_MERGE_FLAG                             20
#define HEVC_BIN_MERGE_IDX                              21
#define HEVC_BIN_INTER_PRED_IDC                         22
#define HEVC_BIN_REF_IDX_L0                             27
#define HEVC_BIN_REF_IDX_L1                             29
#define HEVC_BIN_ABS_MVD_GREATER0_FLAG                  31
#define HEVC_BIN_ABS_MVD_GREATER1_FLAG                  33
#define HEVC_BIN_ABS_MVD_MINUS2                         35
#define HEVC_BIN_MVD_SIGN_FLAG                          35
#define HEVC_BIN_MVP_LX_FLAG                            35
#define HEVC_BIN_NO_RESIDUAL_DATA_FLAG                  36
#define HEVC_BIN_SPLIT_TRANSFORM_FLAG                   37
#define HEVC_BIN_CBF_LUMA                               40
#define HEVC_BIN_CBF_CB_CR                              42
#define HEVC_BIN_TRANSFORM_SKIP_FLAG                    46
#define HEVC_BIN_EXPLICIT_RDPCM_FLAG                    48
#define HEVC_BIN_EXPLICIT_RDPCM_DIR_FLAG                50
#define HEVC_BIN_LAST_SIGNIFICANT_COEFF_X_PREFIX        52
#define HEVC_BIN_LAST_SIGNIFICANT_COEFF_Y_PREFIX        70
#define HEVC_BIN_LAST_SIGNIFICANT_COEFF_X_SUFFIX        88
#define HEVC_BIN_LAST_SIGNIFICANT_COEFF_Y_SUFFIX        88
#define HEVC_BIN_SIGNIFICANT_COEFF_GROUP_FLAG           88
#define HEVC_BIN_SIGNIFICANT_COEFF_FLAG                 92
#define HEVC_BIN_COEFF_ABS_LEVEL_GREATER1_FLAG          136
#define HEVC_BIN_COEFF_ABS_LEVEL_GREATER2_FLAG          160
#define HEVC_BIN_COEFF_ABS_LEVEL_REMAINING              166
#define HEVC_BIN_COEFF_SIGN_FLAG                        166
#define HEVC_BIN_LOG2_RES_SCALE_ABS                     166
#define HEVC_BIN_RES_SCALE_SIGN_FLAG                    174
#define HEVC_BIN_CU_CHROMA_QP_OFFSET_FLAG               176
#define HEVC_BIN_CU_CHROMA_QP_OFFSET_IDX                177


int ff_hevc_rpi_get_cabac(CABACContext * const c, uint8_t * const state);
int ff_hevc_rpi_get_cabac_terminate(CABACContext * const c);

static inline const uint8_t* ff_hevc_rpi_cabac_skip_bytes(CABACContext * const c, int n) {
    const uint8_t *ptr = c->bytestream;

    if (c->low & 0x1)
        ptr--;
#if CABAC_BITS == 16
    if (c->low & 0x1FF)
        ptr--;
#endif
    if ((int) (c->bytestream_end - ptr) < n)
        return NULL;
    if (ff_init_cabac_decoder(c, ptr + n, c->bytestream_end - ptr - n) < 0)
        return NULL;

    return ptr;
}

static inline int ff_hevc_rpi_sao_merge_flag_decode(HEVCRpiLocalContext * const lc)
{
    return ff_hevc_rpi_get_cabac(&lc->cc, lc->cabac_state + HEVC_BIN_SAO_MERGE_FLAG);
}

static inline int ff_hevc_rpi_cu_transquant_bypass_flag_decode(HEVCRpiLocalContext * const lc)
{
    return ff_hevc_rpi_get_cabac(&lc->cc, lc->cabac_state + HEVC_BIN_CU_TRANSQUANT_BYPASS_FLAG);
}

static inline int ff_hevc_rpi_cu_chroma_qp_offset_flag(HEVCRpiLocalContext * const lc)
{
    return ff_hevc_rpi_get_cabac(&lc->cc, lc->cabac_state + HEVC_BIN_CU_CHROMA_QP_OFFSET_FLAG);
}

static inline int ff_hevc_rpi_split_coding_unit_flag_decode(const HEVCRpiContext * const s, HEVCRpiLocalContext * const lc,
                                                            const unsigned int ct_depth,
                                                            const unsigned int x0, const unsigned int y0)
{
    return ff_hevc_rpi_get_cabac(&lc->cc, lc->cabac_state + HEVC_BIN_SPLIT_CODING_UNIT_FLAG +
                                 ((s->cabac_stash_left[y0 >> 3] >> 1) > ct_depth) +
                                 ((s->cabac_stash_up[x0 >> 3] >> 1) > ct_depth));
}

static inline int ff_hevc_rpi_skip_flag_decode(const HEVCRpiContext * const s, HEVCRpiLocalContext * const lc,
                             const int x0, const int y0, const int x_cb, const int y_cb)
{
    return ff_hevc_rpi_get_cabac(&lc->cc, lc->cabac_state + HEVC_BIN_SKIP_FLAG +
                                 (s->cabac_stash_left[y0 >> 3] & 1) +
                                 (s->cabac_stash_up[x0 >> 3] & 1));
}

static inline int ff_hevc_rpi_pred_mode_decode(HEVCRpiLocalContext * const lc)
{
    return ff_hevc_rpi_get_cabac(&lc->cc, lc->cabac_state + HEVC_BIN_PRED_MODE);
}

static inline int ff_hevc_rpi_pcm_flag_decode(HEVCRpiLocalContext * const lc)
{
    return ff_hevc_rpi_get_cabac_terminate(&lc->cc);
}

static inline int ff_hevc_rpi_prev_intra_luma_pred_flag_decode(HEVCRpiLocalContext * const lc)
{
    return ff_hevc_rpi_get_cabac(&lc->cc, lc->cabac_state + HEVC_BIN_PREV_INTRA_LUMA_PRED_MODE);
}

static inline int ff_hevc_rpi_merge_flag_decode(HEVCRpiLocalContext * const lc)
{
    return ff_hevc_rpi_get_cabac(&lc->cc, lc->cabac_state + HEVC_BIN_MERGE_FLAG);
}

static inline int ff_hevc_rpi_mvp_lx_flag_decode(HEVCRpiLocalContext * const lc)
{
    return ff_hevc_rpi_get_cabac(&lc->cc, lc->cabac_state + HEVC_BIN_MVP_LX_FLAG);
}

static inline int ff_hevc_rpi_no_residual_syntax_flag_decode(HEVCRpiLocalContext * const lc)
{
    return ff_hevc_rpi_get_cabac(&lc->cc, lc->cabac_state + HEVC_BIN_NO_RESIDUAL_DATA_FLAG);
}

static inline int ff_hevc_rpi_cbf_cb_cr_decode(HEVCRpiLocalContext * const lc, const int trafo_depth)
{
    return ff_hevc_rpi_get_cabac(&lc->cc, lc->cabac_state + HEVC_BIN_CBF_CB_CR + trafo_depth);
}

static inline int ff_hevc_rpi_cbf_luma_decode(HEVCRpiLocalContext * const lc, const int trafo_depth)
{
    return ff_hevc_rpi_get_cabac(&lc->cc, lc->cabac_state + HEVC_BIN_CBF_LUMA + !trafo_depth);
}

static inline int ff_hevc_rpi_split_transform_flag_decode(HEVCRpiLocalContext * const lc, const int log2_trafo_size)
{
    return ff_hevc_rpi_get_cabac(&lc->cc, lc->cabac_state + HEVC_BIN_SPLIT_TRANSFORM_FLAG + 5 - log2_trafo_size);
}

static inline int ff_hevc_rpi_res_scale_sign_flag(HEVCRpiLocalContext *const lc, const int idx)
{
    return ff_hevc_rpi_get_cabac(&lc->cc, lc->cabac_state + HEVC_BIN_RES_SCALE_SIGN_FLAG + idx);
}



#endif

