/*
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

#include "decode.h"
#include "hevcdec.h"
#include "hwconfig.h"
#include "v4l2_request.h"
#include "hevc-ctrls.h"
#include "v4l2_phase.h"

#include "v4l2_req_devscan.h"
#include "v4l2_req_dmabufs.h"
#include "v4l2_req_pollqueue.h"
#include "v4l2_req_media.h"
#include "v4l2_req_utils.h"

#define MAX_SLICES 16

#include <drm_fourcc.h>

#ifndef DRM_FORMAT_NV15
#define DRM_FORMAT_NV15 fourcc_code('N', 'V', '1', '5')
#endif

#ifndef DRM_FORMAT_NV20
#define DRM_FORMAT_NV20 fourcc_code('N', 'V', '2', '0')
#endif

// P030 should be defined in drm_fourcc.h and hopefully will be sometime
// in the future but until then...
#ifndef DRM_FORMAT_P030
#define DRM_FORMAT_P030 fourcc_code('P', '0', '3', '0')
#endif

#ifndef DRM_FORMAT_NV15
#define DRM_FORMAT_NV15 fourcc_code('N', 'V', '1', '5')
#endif

#ifndef DRM_FORMAT_NV20
#define DRM_FORMAT_NV20 fourcc_code('N', 'V', '2', '0')
#endif



typedef struct V4L2RequestControlsHEVC {
    struct v4l2_ctrl_hevc_sps sps;
    struct v4l2_ctrl_hevc_pps pps;
    struct v4l2_ctrl_hevc_scaling_matrix scaling_matrix;
    struct v4l2_ctrl_hevc_slice_params slice_params[MAX_SLICES];
    int first_slice;
    int num_slices; //TODO: this should be in control
} V4L2RequestControlsHEVC;

typedef struct V4L2RequestContextHEVC {
    V4L2RequestContext base;
    int decode_mode;
    int start_code;
    int max_slices;

    unsigned int order;
    V4L2PhaseControl * pctrl;

    struct devscan *devscan;
    struct dmabufs_ctl *dbufs;
    struct pollqueue *pq;
    struct media_pool * mpool;
    struct mediabufs_ctl *mbufs;
} V4L2RequestContextHEVC;

typedef struct V4L2ReqFrameDataPrivHEVC {
    int not_first_slice;
    struct mediabufs_ctl *mbufs;
    struct media_request *req;
    struct qent_src *qe_src;  // qe_dst held in V4L2RequestDescriptor (data[0])
} V4L2ReqFrameDataPrivHEVC;

static uint8_t nalu_slice_start_code[] = { 0x00, 0x00, 0x01 };

static inline uint64_t frame_capture_timestamp(const AVFrame * const frame)
{
    const V4L2RequestDescriptor *const rd = (V4L2RequestDescriptor *)frame->data[0];
    return rd->timestamp;
}

static inline void frame_set_capture_timestamp(AVFrame * const frame, const uint64_t timestamp)
{
    V4L2RequestDescriptor *const rd = (V4L2RequestDescriptor *)frame->data[0];
    rd->timestamp = timestamp;
}

static void fill_pred_table(const HEVCContext *h, struct v4l2_hevc_pred_weight_table *table)
{
    int32_t luma_weight_denom, chroma_weight_denom;
    const SliceHeader *sh = &h->sh;

    if (sh->slice_type == HEVC_SLICE_I ||
        (sh->slice_type == HEVC_SLICE_P && !h->ps.pps->weighted_pred_flag) ||
        (sh->slice_type == HEVC_SLICE_B && !h->ps.pps->weighted_bipred_flag))
        return;

    table->luma_log2_weight_denom = sh->luma_log2_weight_denom;

    if (h->ps.sps->chroma_format_idc)
        table->delta_chroma_log2_weight_denom = sh->chroma_log2_weight_denom - sh->luma_log2_weight_denom;

    luma_weight_denom = (1 << sh->luma_log2_weight_denom);
    chroma_weight_denom = (1 << sh->chroma_log2_weight_denom);

    for (int i = 0; i < 15 && i < sh->nb_refs[L0]; i++) {
        table->delta_luma_weight_l0[i] = sh->luma_weight_l0[i] - luma_weight_denom;
        table->luma_offset_l0[i] = sh->luma_offset_l0[i];
        table->delta_chroma_weight_l0[i][0] = sh->chroma_weight_l0[i][0] - chroma_weight_denom;
        table->delta_chroma_weight_l0[i][1] = sh->chroma_weight_l0[i][1] - chroma_weight_denom;
        table->chroma_offset_l0[i][0] = sh->chroma_offset_l0[i][0];
        table->chroma_offset_l0[i][1] = sh->chroma_offset_l0[i][1];
    }

    if (sh->slice_type != HEVC_SLICE_B)
        return;

    for (int i = 0; i < 15 && i < sh->nb_refs[L1]; i++) {
        table->delta_luma_weight_l1[i] = sh->luma_weight_l1[i] - luma_weight_denom;
        table->luma_offset_l1[i] = sh->luma_offset_l1[i];
        table->delta_chroma_weight_l1[i][0] = sh->chroma_weight_l1[i][0] - chroma_weight_denom;
        table->delta_chroma_weight_l1[i][1] = sh->chroma_weight_l1[i][1] - chroma_weight_denom;
        table->chroma_offset_l1[i][0] = sh->chroma_offset_l1[i][0];
        table->chroma_offset_l1[i][1] = sh->chroma_offset_l1[i][1];
    }
}

static int find_frame_rps_type(const HEVCContext *h, uint64_t timestamp)
{
    const HEVCFrame *frame;
    int i;

    for (i = 0; i < h->rps[ST_CURR_BEF].nb_refs; i++) {
        frame = h->rps[ST_CURR_BEF].ref[i];
        if (frame && timestamp == frame_capture_timestamp(frame->frame))
            return V4L2_HEVC_DPB_ENTRY_RPS_ST_CURR_BEFORE;
    }

    for (i = 0; i < h->rps[ST_CURR_AFT].nb_refs; i++) {
        frame = h->rps[ST_CURR_AFT].ref[i];
        if (frame && timestamp == frame_capture_timestamp(frame->frame))
            return V4L2_HEVC_DPB_ENTRY_RPS_ST_CURR_AFTER;
    }

    for (i = 0; i < h->rps[LT_CURR].nb_refs; i++) {
        frame = h->rps[LT_CURR].ref[i];
        if (frame && timestamp == frame_capture_timestamp(frame->frame))
            return V4L2_HEVC_DPB_ENTRY_RPS_LT_CURR;
    }

    return 0;
}

static uint8_t get_ref_pic_index(const HEVCContext *h, const HEVCFrame *frame,
                                 struct v4l2_ctrl_hevc_slice_params *slice_params)
{
    uint64_t timestamp;

    if (!frame)
        return 0;

    timestamp = frame_capture_timestamp(frame->frame);

    for (uint8_t i = 0; i < slice_params->num_active_dpb_entries; i++) {
        struct v4l2_hevc_dpb_entry *entry = &slice_params->dpb[i];
        if (entry->timestamp == timestamp)
            return i;
    }

    return 0;
}

static const uint8_t * ptr_from_index(const uint8_t * b, unsigned int idx)
{
    unsigned int z = 0;
    while (idx--) {
        if (*b++ == 0) {
            ++z;
            if (z >= 2 && *b == 3) {
                ++b;
                z = 0;
            }
        }
        else {
            z = 0;
        }
    }
    return b;
}

static void v4l2_request_hevc_fill_slice_params(const HEVCContext *h,
                                                struct v4l2_ctrl_hevc_slice_params *slice_params)
{
    const HEVCFrame *pic = h->ref;
    const SliceHeader *sh = &h->sh;
    int i, entries = 0;
    RefPicList *rpl;

    *slice_params = (struct v4l2_ctrl_hevc_slice_params) {
        .bit_size = 0, // Set later
        .data_bit_offset = 0, // Set later

        /* ISO/IEC 23008-2, ITU-T Rec. H.265: General slice segment header */
        .slice_segment_addr = sh->slice_segment_addr,

        /* ISO/IEC 23008-2, ITU-T Rec. H.265: NAL unit header */
        .nal_unit_type = h->nal_unit_type,
        .nuh_temporal_id_plus1 = h->temporal_id + 1,

        /* ISO/IEC 23008-2, ITU-T Rec. H.265: General slice segment header */
        .slice_type = sh->slice_type,
        .colour_plane_id = sh->colour_plane_id,
        .slice_pic_order_cnt = pic->poc,
        .num_ref_idx_l0_active_minus1 = sh->nb_refs[L0] ? sh->nb_refs[L0] - 1 : 0,
        .num_ref_idx_l1_active_minus1 = sh->nb_refs[L1] ? sh->nb_refs[L1] - 1 : 0,
        .collocated_ref_idx = sh->slice_temporal_mvp_enabled_flag ? sh->collocated_ref_idx : 0,
        .five_minus_max_num_merge_cand = sh->slice_type == HEVC_SLICE_I ? 0 : 5 - sh->max_num_merge_cand,
        .slice_qp_delta = sh->slice_qp_delta,
        .slice_cb_qp_offset = sh->slice_cb_qp_offset,
        .slice_cr_qp_offset = sh->slice_cr_qp_offset,
        .slice_act_y_qp_offset = 0,
        .slice_act_cb_qp_offset = 0,
        .slice_act_cr_qp_offset = 0,
        .slice_beta_offset_div2 = sh->beta_offset / 2,
        .slice_tc_offset_div2 = sh->tc_offset / 2,

        /* ISO/IEC 23008-2, ITU-T Rec. H.265: Picture timing SEI message */
        .pic_struct = h->sei.picture_timing.picture_struct,

        /* ISO/IEC 23008-2, ITU-T Rec. H.265: General slice segment header */
        .num_rps_poc_st_curr_before = h->rps[ST_CURR_BEF].nb_refs,
        .num_rps_poc_st_curr_after = h->rps[ST_CURR_AFT].nb_refs,
        .num_rps_poc_lt_curr = h->rps[LT_CURR].nb_refs,
    };

    if (sh->slice_sample_adaptive_offset_flag[0])
        slice_params->flags |= V4L2_HEVC_SLICE_PARAMS_FLAG_SLICE_SAO_LUMA;

    if (sh->slice_sample_adaptive_offset_flag[1])
        slice_params->flags |= V4L2_HEVC_SLICE_PARAMS_FLAG_SLICE_SAO_CHROMA;

    if (sh->slice_temporal_mvp_enabled_flag)
        slice_params->flags |= V4L2_HEVC_SLICE_PARAMS_FLAG_SLICE_TEMPORAL_MVP_ENABLED;

    if (sh->mvd_l1_zero_flag)
        slice_params->flags |= V4L2_HEVC_SLICE_PARAMS_FLAG_MVD_L1_ZERO;

    if (sh->cabac_init_flag)
        slice_params->flags |= V4L2_HEVC_SLICE_PARAMS_FLAG_CABAC_INIT;

    if (sh->collocated_list == L0)
        slice_params->flags |= V4L2_HEVC_SLICE_PARAMS_FLAG_COLLOCATED_FROM_L0;

    if (sh->disable_deblocking_filter_flag)
        slice_params->flags |= V4L2_HEVC_SLICE_PARAMS_FLAG_SLICE_DEBLOCKING_FILTER_DISABLED;

    if (sh->slice_loop_filter_across_slices_enabled_flag)
        slice_params->flags |= V4L2_HEVC_SLICE_PARAMS_FLAG_SLICE_LOOP_FILTER_ACROSS_SLICES_ENABLED;

    if (sh->dependent_slice_segment_flag)
        slice_params->flags |= V4L2_HEVC_SLICE_PARAMS_FLAG_DEPENDENT_SLICE_SEGMENT;

    for (i = 0; i < FF_ARRAY_ELEMS(h->DPB); i++) {
        const HEVCFrame *frame = &h->DPB[i];
        if (frame != pic && (frame->flags & (HEVC_FRAME_FLAG_LONG_REF | HEVC_FRAME_FLAG_SHORT_REF))) {
            struct v4l2_hevc_dpb_entry *entry = &slice_params->dpb[entries++];

            entry->timestamp = frame_capture_timestamp(frame->frame);
            entry->rps = find_frame_rps_type(h, entry->timestamp);
            entry->field_pic = frame->frame->interlaced_frame;

            /* TODO: Interleaved: Get the POC for each field. */
            entry->pic_order_cnt[0] = frame->poc;
            entry->pic_order_cnt[1] = frame->poc;
        }
    }

    slice_params->num_active_dpb_entries = entries;

    if (sh->slice_type != HEVC_SLICE_I) {
        rpl = &h->ref->refPicList[0];
        for (i = 0; i < rpl->nb_refs; i++)
            slice_params->ref_idx_l0[i] = get_ref_pic_index(h, rpl->ref[i], slice_params);
    }

    if (sh->slice_type == HEVC_SLICE_B) {
        rpl = &h->ref->refPicList[1];
        for (i = 0; i < rpl->nb_refs; i++)
            slice_params->ref_idx_l1[i] = get_ref_pic_index(h, rpl->ref[i], slice_params);
    }

    fill_pred_table(h, &slice_params->pred_weight_table);

    slice_params->num_entry_point_offsets = sh->num_entry_point_offsets;
    if (slice_params->num_entry_point_offsets > 256) {
        slice_params->num_entry_point_offsets = 256;
        av_log(NULL, AV_LOG_ERROR, "%s: Currently only 256 entry points are supported, but slice has %d entry points.\n", __func__, sh->num_entry_point_offsets);
    }

    for (i = 0; i < slice_params->num_entry_point_offsets; i++)
        slice_params->entry_point_offset_minus1[i] = sh->entry_point_offset[i] - 1;
}

static void fill_sps(struct v4l2_ctrl_hevc_sps *ctrl, const HEVCContext *h)
{
    const HEVCSPS *sps = h->ps.sps;

    /* ISO/IEC 23008-2, ITU-T Rec. H.265: Sequence parameter set */
    *ctrl = (struct v4l2_ctrl_hevc_sps) {
        .chroma_format_idc = sps->chroma_format_idc,
        .pic_width_in_luma_samples = sps->width,
        .pic_height_in_luma_samples = sps->height,
        .bit_depth_luma_minus8 = sps->bit_depth - 8,
        .bit_depth_chroma_minus8 = sps->bit_depth - 8,
        .log2_max_pic_order_cnt_lsb_minus4 = sps->log2_max_poc_lsb - 4,
        .sps_max_dec_pic_buffering_minus1 = sps->temporal_layer[sps->max_sub_layers - 1].max_dec_pic_buffering - 1,
        .sps_max_num_reorder_pics = sps->temporal_layer[sps->max_sub_layers - 1].num_reorder_pics,
        .sps_max_latency_increase_plus1 = sps->temporal_layer[sps->max_sub_layers - 1].max_latency_increase + 1,
        .log2_min_luma_coding_block_size_minus3 = sps->log2_min_cb_size - 3,
        .log2_diff_max_min_luma_coding_block_size = sps->log2_diff_max_min_coding_block_size,
        .log2_min_luma_transform_block_size_minus2 = sps->log2_min_tb_size - 2,
        .log2_diff_max_min_luma_transform_block_size = sps->log2_max_trafo_size - sps->log2_min_tb_size,
        .max_transform_hierarchy_depth_inter = sps->max_transform_hierarchy_depth_inter,
        .max_transform_hierarchy_depth_intra = sps->max_transform_hierarchy_depth_intra,
        .pcm_sample_bit_depth_luma_minus1 = sps->pcm.bit_depth - 1,
        .pcm_sample_bit_depth_chroma_minus1 = sps->pcm.bit_depth_chroma - 1,
        .log2_min_pcm_luma_coding_block_size_minus3 = sps->pcm.log2_min_pcm_cb_size - 3,
        .log2_diff_max_min_pcm_luma_coding_block_size = sps->pcm.log2_max_pcm_cb_size - sps->pcm.log2_min_pcm_cb_size,
        .num_short_term_ref_pic_sets = sps->nb_st_rps,
        .num_long_term_ref_pics_sps = sps->num_long_term_ref_pics_sps,
    };

    if (sps->separate_colour_plane_flag)
        ctrl->flags |= V4L2_HEVC_SPS_FLAG_SEPARATE_COLOUR_PLANE;

    if (sps->scaling_list_enable_flag)
        ctrl->flags |= V4L2_HEVC_SPS_FLAG_SCALING_LIST_ENABLED;

    if (sps->amp_enabled_flag)
        ctrl->flags |= V4L2_HEVC_SPS_FLAG_AMP_ENABLED;

    if (sps->sao_enabled)
        ctrl->flags |= V4L2_HEVC_SPS_FLAG_SAMPLE_ADAPTIVE_OFFSET;

    if (sps->pcm_enabled_flag)
        ctrl->flags |= V4L2_HEVC_SPS_FLAG_PCM_ENABLED;

    if (sps->pcm.loop_filter_disable_flag)
        ctrl->flags |= V4L2_HEVC_SPS_FLAG_PCM_LOOP_FILTER_DISABLED;

    if (sps->long_term_ref_pics_present_flag)
        ctrl->flags |= V4L2_HEVC_SPS_FLAG_LONG_TERM_REF_PICS_PRESENT;

    if (sps->sps_temporal_mvp_enabled_flag)
        ctrl->flags |= V4L2_HEVC_SPS_FLAG_SPS_TEMPORAL_MVP_ENABLED;

    if (sps->sps_strong_intra_smoothing_enable_flag)
        ctrl->flags |= V4L2_HEVC_SPS_FLAG_STRONG_INTRA_SMOOTHING_ENABLED;
}

// Called before finally returning the frame to the user
// Set corrupt flag here as this is actually the frame structure that
// is going to the user (in MT land each thread has its own pool)
static int v4l2_request_post_process(void *logctx, AVFrame *frame)
{
    V4L2RequestDescriptor *rd = (V4L2RequestDescriptor*)frame->data[0];
    FrameDecodeData * const fdd = (FrameDecodeData*)frame->private_ref->data;
    V4L2ReqFrameDataPrivHEVC *const rfdp = fdd->hwaccel_priv;

    av_log(NULL, AV_LOG_INFO, "%s\n", __func__);
    if (rd->qe_dst) {
        MediaBufsStatus stat = qent_dst_wait(rd->qe_dst);
        if (stat != MEDIABUFS_STATUS_SUCCESS) {
            av_log(logctx, AV_LOG_ERROR, "%s: Decode fail\n", __func__);
        }
        mediabufs_ctl_unref(&rfdp->mbufs);
    }

    if (rd) {
        av_log(logctx, AV_LOG_DEBUG, "%s: flags=%#x, ts=%ld.%06ld\n", __func__, rd->capture.buffer.flags,
               rd->capture.buffer.timestamp.tv_sec, rd->capture.buffer.timestamp.tv_usec);
        frame->flags = (rd->capture.buffer.flags & V4L2_BUF_FLAG_ERROR) == 0 ? 0 : AV_FRAME_FLAG_CORRUPT;
    }



    return 0;
}

static void v4l2_req_frame_data_priv_free(void * priv)
{
    V4L2ReqFrameDataPrivHEVC * const rfdp = priv;
    av_log(NULL, AV_LOG_INFO, "%s\n", __func__);
    mediabufs_ctl_unref(&rfdp->mbufs);
    free(rfdp);
}

static int v4l2_request_hevc_start_frame(AVCodecContext *avctx,
                                         av_unused const uint8_t *buffer,
                                         av_unused uint32_t size)
{
    const HEVCContext *h = avctx->priv_data;
    const HEVCSPS *sps = h->ps.sps;
    const HEVCPPS *pps = h->ps.pps;
    const ScalingList *sl = pps->scaling_list_data_present_flag ?
                            &pps->scaling_list :
                            sps->scaling_list_enable_flag ?
                            &sps->scaling_list : NULL;
    V4L2RequestControlsHEVC *controls = h->ref->hwaccel_picture_private;
    V4L2RequestDescriptor *const rd = (V4L2RequestDescriptor *)h->ref->frame->data[0];
    V4L2RequestContextHEVC * const ctx = avctx->internal->hwaccel_priv_data;
    int rv;

    av_log(NULL, AV_LOG_INFO, "%s\n", __func__);
    fill_sps(&controls->sps, h);

    if (sl) {
        for (int i = 0; i < 6; i++) {
            for (int j = 0; j < 16; j++)
                controls->scaling_matrix.scaling_list_4x4[i][j] = sl->sl[0][i][j];
            for (int j = 0; j < 64; j++) {
                controls->scaling_matrix.scaling_list_8x8[i][j]   = sl->sl[1][i][j];
                controls->scaling_matrix.scaling_list_16x16[i][j] = sl->sl[2][i][j];
                if (i < 2)
                    controls->scaling_matrix.scaling_list_32x32[i][j] = sl->sl[3][i * 3][j];
            }
            controls->scaling_matrix.scaling_list_dc_coef_16x16[i] = sl->sl_dc[0][i];
            if (i < 2)
                controls->scaling_matrix.scaling_list_dc_coef_32x32[i] = sl->sl_dc[1][i * 3];
        }
    }

    /* ISO/IEC 23008-2, ITU-T Rec. H.265: Picture parameter set */
    controls->pps = (struct v4l2_ctrl_hevc_pps) {
        .num_extra_slice_header_bits = pps->num_extra_slice_header_bits,
        .init_qp_minus26 = pps->pic_init_qp_minus26,
        .diff_cu_qp_delta_depth = pps->diff_cu_qp_delta_depth,
        .pps_cb_qp_offset = pps->cb_qp_offset,
        .pps_cr_qp_offset = pps->cr_qp_offset,
        .pps_beta_offset_div2 = pps->beta_offset / 2,
        .pps_tc_offset_div2 = pps->tc_offset / 2,
        .log2_parallel_merge_level_minus2 = pps->log2_parallel_merge_level - 2,
    };

    if (pps->dependent_slice_segments_enabled_flag)
        controls->pps.flags |= V4L2_HEVC_PPS_FLAG_DEPENDENT_SLICE_SEGMENT;

    if (pps->output_flag_present_flag)
        controls->pps.flags |= V4L2_HEVC_PPS_FLAG_OUTPUT_FLAG_PRESENT;

    if (pps->sign_data_hiding_flag)
        controls->pps.flags |= V4L2_HEVC_PPS_FLAG_SIGN_DATA_HIDING_ENABLED;

    if (pps->cabac_init_present_flag)
        controls->pps.flags |= V4L2_HEVC_PPS_FLAG_CABAC_INIT_PRESENT;

    if (pps->constrained_intra_pred_flag)
        controls->pps.flags |= V4L2_HEVC_PPS_FLAG_CONSTRAINED_INTRA_PRED;

    if (pps->transform_skip_enabled_flag)
        controls->pps.flags |= V4L2_HEVC_PPS_FLAG_TRANSFORM_SKIP_ENABLED;

    if (pps->cu_qp_delta_enabled_flag)
        controls->pps.flags |= V4L2_HEVC_PPS_FLAG_CU_QP_DELTA_ENABLED;

    if (pps->pic_slice_level_chroma_qp_offsets_present_flag)
        controls->pps.flags |= V4L2_HEVC_PPS_FLAG_PPS_SLICE_CHROMA_QP_OFFSETS_PRESENT;

    if (pps->weighted_pred_flag)
        controls->pps.flags |= V4L2_HEVC_PPS_FLAG_WEIGHTED_PRED;

    if (pps->weighted_bipred_flag)
        controls->pps.flags |= V4L2_HEVC_PPS_FLAG_WEIGHTED_BIPRED;

    if (pps->transquant_bypass_enable_flag)
        controls->pps.flags |= V4L2_HEVC_PPS_FLAG_TRANSQUANT_BYPASS_ENABLED;

    if (pps->tiles_enabled_flag)
        controls->pps.flags |= V4L2_HEVC_PPS_FLAG_TILES_ENABLED;

    if (pps->entropy_coding_sync_enabled_flag)
        controls->pps.flags |= V4L2_HEVC_PPS_FLAG_ENTROPY_CODING_SYNC_ENABLED;

    if (pps->loop_filter_across_tiles_enabled_flag)
        controls->pps.flags |= V4L2_HEVC_PPS_FLAG_LOOP_FILTER_ACROSS_TILES_ENABLED;

    if (pps->seq_loop_filter_across_slices_enabled_flag)
        controls->pps.flags |= V4L2_HEVC_PPS_FLAG_PPS_LOOP_FILTER_ACROSS_SLICES_ENABLED;

    if (pps->deblocking_filter_override_enabled_flag)
        controls->pps.flags |= V4L2_HEVC_PPS_FLAG_DEBLOCKING_FILTER_OVERRIDE_ENABLED;

    if (pps->disable_dbf)
        controls->pps.flags |= V4L2_HEVC_PPS_FLAG_PPS_DISABLE_DEBLOCKING_FILTER;

    if (pps->lists_modification_present_flag)
        controls->pps.flags |= V4L2_HEVC_PPS_FLAG_LISTS_MODIFICATION_PRESENT;

    if (pps->slice_header_extension_present_flag)
        controls->pps.flags |= V4L2_HEVC_PPS_FLAG_SLICE_SEGMENT_HEADER_EXTENSION_PRESENT;

    if (pps->tiles_enabled_flag) {
        controls->pps.num_tile_columns_minus1 = pps->num_tile_columns - 1;
        controls->pps.num_tile_rows_minus1 = pps->num_tile_rows - 1;

        for (int i = 0; i < pps->num_tile_columns; i++)
            controls->pps.column_width_minus1[i] = pps->column_width[i] - 1;

        for (int i = 0; i < pps->num_tile_rows; i++)
            controls->pps.row_height_minus1[i] = pps->row_height[i] - 1;
    }

    controls->first_slice = 1;
    controls->num_slices = 0;
    ctx->base.timestamp++;

    if ((rv = ff_v4l2_request_reset_frame(avctx, h->ref->frame)) != 0)
        return rv;

    {
        FrameDecodeData * const fdd = (FrameDecodeData*)h->ref->frame->private_ref->data;
        fdd->post_process = v4l2_request_post_process;
        if (fdd->hwaccel_priv) {
            av_log(avctx, AV_LOG_WARNING, "%s: HWaccel already allocated\n", __func__);
        }
        else {
            V4L2ReqFrameDataPrivHEVC * const rfdp = av_mallocz(sizeof(*rfdp));
            if (!rfdp)
                return AVERROR(ENOMEM);
            fdd->hwaccel_priv = rfdp;
            fdd->hwaccel_priv_free = v4l2_req_frame_data_priv_free;

            rfdp->mbufs = mediabufs_ctl_ref(ctx->mbufs);

            rfdp->req = media_request_get(ctx->mpool);
            if (!rfdp->req) {
                av_log(avctx, AV_LOG_ERROR, "%s: Failed to alloc media request\n", __func__);
                return AVERROR_UNKNOWN;
            }

            if (mediabufs_set_ext_ctrl(rfdp->mbufs, rfdp->req, V4L2_CID_MPEG_VIDEO_HEVC_SPS,
                                       &controls->sps, sizeof(controls->sps)) != MEDIABUFS_STATUS_SUCCESS) {
                av_log(avctx, AV_LOG_ERROR, "%s: Failed to set ext ctrl SPS\n", __func__);
                return AVERROR_UNKNOWN;
            }
            if (mediabufs_set_ext_ctrl(rfdp->mbufs, rfdp->req, V4L2_CID_MPEG_VIDEO_HEVC_PPS,
                                       &controls->pps, sizeof(controls->pps)) != MEDIABUFS_STATUS_SUCCESS) {
                av_log(avctx, AV_LOG_ERROR, "%s: Failed to set ext ctrl PPS\n", __func__);
                return AVERROR_UNKNOWN;
            }
            if (sl &&
                mediabufs_set_ext_ctrl(rfdp->mbufs, rfdp->req, V4L2_CID_MPEG_VIDEO_HEVC_SCALING_MATRIX,
                                       &controls->scaling_matrix, sizeof(controls->scaling_matrix)) != MEDIABUFS_STATUS_SUCCESS) {
                av_log(avctx, AV_LOG_ERROR, "%s: Failed to set ext ctrl scaling matrix\n", __func__);
                return AVERROR_UNKNOWN;
            }

            // qe_dst needs to be bound to the data buffer and only returned when that is
            if (!rd->qe_dst)
            {
                if ((rd->qe_dst = mediabufs_dst_qent_alloc(ctx->mbufs, ctx->dbufs)) == NULL) {
                    av_log(avctx, AV_LOG_ERROR, "%s: Failed to get dst buffer\n", __func__);
                    return AVERROR(ENOMEM);
                }
            }
        }
    }


    ff_v4l2_request_start_phase_control(h->ref->frame, ctx->pctrl);

    ff_thread_finish_setup(avctx); // Allow next thread to enter rpi_hevc_start_frame

    return 0;
}

// Object fd & size will be zapped by this & need setting later
static int drm_from_format(AVDRMFrameDescriptor * const desc, const struct v4l2_format * const format)
{
    AVDRMLayerDescriptor *layer = &desc->layers[0];
    uint32_t pixelformat = V4L2_TYPE_IS_MULTIPLANAR(format->type) ? format->fmt.pix_mp.pixelformat : format->fmt.pix.pixelformat;

    switch (pixelformat) {
    case V4L2_PIX_FMT_NV12:
        layer->format = DRM_FORMAT_NV12;
        desc->objects[0].format_modifier = DRM_FORMAT_MOD_LINEAR;
        break;
#if CONFIG_SAND
    case V4L2_PIX_FMT_NV12_COL128:
        layer->format = DRM_FORMAT_NV12;
        desc->objects[0].format_modifier = DRM_FORMAT_MOD_BROADCOM_SAND128_COL_HEIGHT(format->fmt.pix.bytesperline);
        break;
    case V4L2_PIX_FMT_NV12_10_COL128:
        layer->format = DRM_FORMAT_P030;
        desc->objects[0].format_modifier = DRM_FORMAT_MOD_BROADCOM_SAND128_COL_HEIGHT(format->fmt.pix.bytesperline);
        break;
#endif
#ifdef DRM_FORMAT_MOD_ALLWINNER_TILED
    case V4L2_PIX_FMT_SUNXI_TILED_NV12:
        layer->format = DRM_FORMAT_NV12;
        desc->objects[0].format_modifier = DRM_FORMAT_MOD_ALLWINNER_TILED;
        break;
#endif
#if defined(V4L2_PIX_FMT_NV15) && defined(DRM_FORMAT_NV15)
    case V4L2_PIX_FMT_NV15:
        layer->format = DRM_FORMAT_NV15;
        desc->objects[0].format_modifier = DRM_FORMAT_MOD_LINEAR;
        break;
#endif
    case V4L2_PIX_FMT_NV16:
        layer->format = DRM_FORMAT_NV16;
        desc->objects[0].format_modifier = DRM_FORMAT_MOD_LINEAR;
        break;
#if defined(V4L2_PIX_FMT_NV20) && defined(DRM_FORMAT_NV20)
    case V4L2_PIX_FMT_NV20:
        layer->format = DRM_FORMAT_NV20;
        desc->objects[0].format_modifier = DRM_FORMAT_MOD_LINEAR;
        break;
#endif
    default:
        return -1;
    }

    desc->nb_objects = 1;
    desc->objects[0].fd = -1;
    desc->objects[0].size = 0;

    desc->nb_layers = 1;
    layer->nb_planes = 2;

    layer->planes[0].object_index = 0;
    layer->planes[0].offset = 0;
    layer->planes[0].pitch = V4L2_TYPE_IS_MULTIPLANAR(format->type) ? format->fmt.pix_mp.plane_fmt[0].bytesperline : format->fmt.pix.bytesperline;
#if CONFIG_SAND
    if (pixelformat == V4L2_PIX_FMT_NV12_COL128) {
        layer->planes[1].object_index = 0;
        layer->planes[1].offset = format->fmt.pix.height * 128;
        layer->planes[0].pitch = format->fmt.pix.width;
        layer->planes[1].pitch = format->fmt.pix.width;
    }
    else if (pixelformat == V4L2_PIX_FMT_NV12_10_COL128) {
        layer->planes[1].object_index = 0;
        layer->planes[1].offset = format->fmt.pix.height * 128;
        layer->planes[0].pitch = format->fmt.pix.width * 2; // Lies but it keeps DRM import happy
        layer->planes[1].pitch = format->fmt.pix.width * 2;
    }
    else
#endif
    {
        layer->planes[1].object_index = 0;
        layer->planes[1].offset = layer->planes[0].pitch * (V4L2_TYPE_IS_MULTIPLANAR(format->type) ? format->fmt.pix_mp.height : format->fmt.pix.height);
        layer->planes[1].pitch = layer->planes[0].pitch;
    }

    return 0;
}

static int v4l2_request_hevc_queue_decode(AVCodecContext *avctx, int last_slice)
{
    const HEVCContext *h = avctx->priv_data;
    V4L2RequestControlsHEVC *controls = h->ref->hwaccel_picture_private;
    V4L2RequestContextHEVC *ctx = avctx->internal->hwaccel_priv_data;
    V4L2RequestDescriptor *rd = (V4L2RequestDescriptor*)h->ref->frame->data[0];

    struct v4l2_ext_control control[] = {
        {
            .id = V4L2_CID_MPEG_VIDEO_HEVC_SPS,
            .ptr = &controls->sps,
            .size = sizeof(controls->sps),
        },
        {
            .id = V4L2_CID_MPEG_VIDEO_HEVC_PPS,
            .ptr = &controls->pps,
            .size = sizeof(controls->pps),
        },
        {
            .id = V4L2_CID_MPEG_VIDEO_HEVC_SCALING_MATRIX,
            .ptr = &controls->scaling_matrix,
            .size = sizeof(controls->scaling_matrix),
        },
        {
            .id = V4L2_CID_MPEG_VIDEO_HEVC_SLICE_PARAMS,
            .ptr = &controls->slice_params,
            .size = sizeof(controls->slice_params[0]) * FFMAX(FFMIN(controls->num_slices, MAX_SLICES), ctx->max_slices),
        },
    };

    if (ctx->decode_mode == V4L2_MPEG_VIDEO_HEVC_DECODE_MODE_SLICE_BASED) {
        int rv = ff_v4l2_request_decode_slice(avctx, h->ref->frame, control, FF_ARRAY_ELEMS(control), controls->first_slice, last_slice);

        drm_from_format(&rd->drm, mediabufs_dst_fmt(ctx->mbufs));
        rd->drm.objects[0].fd = dmabuf_fd(qent_dst_dmabuf(rd->qe_dst, 0));
        rd->drm.objects[0].size = dmabuf_size(qent_dst_dmabuf(rd->qe_dst, 0));
        return rv;
    }

    return ff_v4l2_request_decode_frame(avctx, h->ref->frame, control, FF_ARRAY_ELEMS(control));
}

static int v4l2_request_hevc_decode_slice(AVCodecContext *avctx, const uint8_t *buffer, uint32_t size)
{
    const HEVCContext *h = avctx->priv_data;
    V4L2RequestControlsHEVC *controls = h->ref->hwaccel_picture_private;
    V4L2RequestContextHEVC *ctx = avctx->internal->hwaccel_priv_data;
    V4L2RequestDescriptor *req = (V4L2RequestDescriptor*)h->ref->frame->data[0];
    int ret, slice = FFMIN(controls->num_slices, MAX_SLICES - 1);
    int bcount = get_bits_count(&h->HEVClc->gb);
    uint32_t boff = (ptr_from_index(buffer, bcount/8 + 1) - (buffer + bcount/8 + 1)) * 8 + bcount;

    FrameDecodeData * const fdd = (FrameDecodeData*)h->ref->frame->private_ref->data;
    V4L2ReqFrameDataPrivHEVC *const rfdp = fdd->hwaccel_priv;

    if (rfdp->not_first_slice) {
        MediaBufsStatus stat;

        // Dispatch previous slice
        stat = mediabufs_start_request(rfdp->mbufs, rfdp->req, rfdp->qe_src, req->qe_dst, 0);
        rfdp->req = NULL;
        rfdp->qe_src = NULL;

        if (stat != MEDIABUFS_STATUS_SUCCESS) {
            av_log(avctx, AV_LOG_ERROR, "%s: Failed to start request\n", __func__);
            return AVERROR_UNKNOWN;
        }

        // Get new req
        if ((rfdp->req = media_request_get(ctx->mpool)) == NULL) {
            av_log(avctx, AV_LOG_ERROR, "%s: Failed to alloc media request\n", __func__);
            return AVERROR_UNKNOWN;
        }
    }
    else {
        rfdp->not_first_slice = 1;
    }

    {
        struct v4l2_ctrl_hevc_slice_params slice_params;
        v4l2_request_hevc_fill_slice_params(h, &slice_params);

        slice_params.bit_size = size * 8;    //FIXME
        slice_params.data_bit_offset = boff; //FIXME

        if (mediabufs_set_ext_ctrl(rfdp->mbufs, rfdp->req, V4L2_CID_MPEG_VIDEO_HEVC_SLICE_PARAMS,
                                   &slice_params, sizeof(slice_params)) != MEDIABUFS_STATUS_SUCCESS) {
            av_log(avctx, AV_LOG_ERROR, "%s: Failed to set ext ctrl slice params\n", __func__);
            return AVERROR_UNKNOWN;
        }
    }

    if ((rfdp->qe_src = mediabufs_src_qent_get(rfdp->mbufs)) == NULL) {
        av_log(avctx, AV_LOG_ERROR, "%s: Failed to get src buffer\n", __func__);
        return AVERROR(ENOMEM);
    }

    if (qent_src_data_copy(rfdp->qe_src, buffer, size) != 0) {
        av_log(avctx, AV_LOG_ERROR, "%s: Failed data copy\n", __func__);
        return AVERROR(ENOMEM);
    }

    {
        struct timeval ts = {
            .tv_usec = ctx->base.timestamp,
        };
        frame_set_capture_timestamp(h->ref->frame, ctx->base.timestamp * 1000); // dpb timestamps are in ns
        qent_src_params_set(rfdp->qe_src, &ts);
    }

    if (ctx->decode_mode == V4L2_MPEG_VIDEO_HEVC_DECODE_MODE_SLICE_BASED && slice) {
        ret = v4l2_request_hevc_queue_decode(avctx, 0);
        if (ret)
            return ret;

        ff_v4l2_request_reset_frame(avctx, h->ref->frame);
        slice = controls->num_slices = 0;
        controls->first_slice = 0;
    }

    v4l2_request_hevc_fill_slice_params(h, &controls->slice_params[slice]);

    if (ctx->start_code == V4L2_MPEG_VIDEO_HEVC_START_CODE_ANNEX_B) {
        // ?? Do we really not need the nal type ??
        ret = ff_v4l2_request_append_output_buffer(avctx, h->ref->frame, nalu_slice_start_code, 3);
        if (ret)
            return ret;
    }
    boff += req->output.used * 8;

    ret = ff_v4l2_request_append_output_buffer(avctx, h->ref->frame, buffer, size);
    if (ret)
        return ret;

    controls->slice_params[slice].bit_size = req->output.used * 8; //FIXME
    controls->slice_params[slice].data_bit_offset = boff; //FIXME
    controls->num_slices++;
    return 0;
}

static void v4l2_request_hevc_abort_frame(AVCodecContext * const avctx) {
    const HEVCContext *h = avctx->priv_data;

    if (h->ref != NULL)
        ff_v4l2_request_abort_phase_control(h->ref->frame);
}

static int v4l2_request_hevc_end_frame(AVCodecContext *avctx)
{
    const HEVCContext * const h = avctx->priv_data;
    FrameDecodeData * const fdd = (FrameDecodeData*)h->ref->frame->private_ref->data;
    V4L2RequestDescriptor *rd = (V4L2RequestDescriptor*)h->ref->frame->data[0];
    V4L2ReqFrameDataPrivHEVC *const rfdp = fdd->hwaccel_priv;
    MediaBufsStatus stat;
    int rv;
    av_log(NULL, AV_LOG_INFO, "%s\n", __func__);

    // Dispatch previous slice
    stat = mediabufs_start_request(rfdp->mbufs, rfdp->req, rfdp->qe_src, rd->qe_dst, 1);
    rfdp->req = NULL;
    rfdp->qe_src = NULL;

    if (stat != MEDIABUFS_STATUS_SUCCESS) {
        av_log(avctx, AV_LOG_ERROR, "%s: Failed to start request\n", __func__);
        return AVERROR_UNKNOWN;
    }

    rv = v4l2_request_hevc_queue_decode(avctx, 1);
    if (rv < 0)
        v4l2_request_hevc_abort_frame(avctx);
    return rv;
}

static int v4l2_request_hevc_alloc_frame(AVCodecContext * avctx, AVFrame *frame)
{
    int ret;

    // This dups the remainder of ff_get_buffer but adds a post_process callback
    ret = avctx->get_buffer2(avctx, frame, AV_GET_BUFFER_FLAG_REF);
    if (ret < 0)
        goto fail;

    ret = ff_attach_decode_data(frame);
    if (ret < 0)
        goto fail;

    return 0;

fail:
    if (ret < 0) {
        av_log(avctx, AV_LOG_ERROR, "%s failed\n", __func__);
        av_frame_unref(frame);
    }

    return ret;
}

static int v4l2_request_hevc_set_controls(AVCodecContext *avctx)
{
    V4L2RequestContextHEVC *ctx = avctx->internal->hwaccel_priv_data;
    int ret;

    struct v4l2_ext_control control[] = {
        { .id = V4L2_CID_MPEG_VIDEO_HEVC_DECODE_MODE, },
        { .id = V4L2_CID_MPEG_VIDEO_HEVC_START_CODE, },
    };
    struct v4l2_query_ext_ctrl slice_params = {
        .id = V4L2_CID_MPEG_VIDEO_HEVC_SLICE_PARAMS,
    };

    ctx->decode_mode = ff_v4l2_request_query_control_default_value(avctx, V4L2_CID_MPEG_VIDEO_HEVC_DECODE_MODE);
    if (ctx->decode_mode != V4L2_MPEG_VIDEO_HEVC_DECODE_MODE_SLICE_BASED &&
        ctx->decode_mode != V4L2_MPEG_VIDEO_HEVC_DECODE_MODE_FRAME_BASED) {
        av_log(avctx, AV_LOG_ERROR, "%s: unsupported decode mode, %d\n", __func__, ctx->decode_mode);
        return AVERROR(EINVAL);
    }

    ctx->start_code = ff_v4l2_request_query_control_default_value(avctx, V4L2_CID_MPEG_VIDEO_HEVC_START_CODE);
    if (ctx->start_code != V4L2_MPEG_VIDEO_HEVC_START_CODE_NONE &&
        ctx->start_code != V4L2_MPEG_VIDEO_HEVC_START_CODE_ANNEX_B) {
        av_log(avctx, AV_LOG_ERROR, "%s: unsupported start code, %d\n", __func__, ctx->start_code);
        return AVERROR(EINVAL);
    }

    ret = ff_v4l2_request_query_control(avctx, &slice_params);
    if (ret)
        return ret;

    ctx->max_slices = slice_params.elems;
    if (ctx->max_slices > MAX_SLICES) {
        av_log(avctx, AV_LOG_ERROR, "%s: unsupported max slices, %d\n", __func__, ctx->max_slices);
        return AVERROR(EINVAL);
    }

    control[0].value = ctx->decode_mode;
    control[1].value = ctx->start_code;

    return ff_v4l2_request_set_controls(avctx, control, FF_ARRAY_ELEMS(control));
}

static int v4l2_request_hevc_uninit(AVCodecContext *avctx)
{
    V4L2RequestContextHEVC * const ctx = avctx->internal->hwaccel_priv_data;

    mediabufs_ctl_unref(&ctx->mbufs);
    media_pool_delete(&ctx->mpool);
    pollqueue_delete(&ctx->pq);
    dmabufs_ctl_delete(&ctx->dbufs);
    devscan_delete(&ctx->devscan);

    ff_v4l2_phase_control_deletez(&ctx->pctrl);
    return ff_v4l2_request_uninit(avctx);
}

static int v4l2_request_hevc_init(AVCodecContext *avctx)
{
    const HEVCContext *h = avctx->priv_data;
    V4L2RequestContextHEVC * const ctx = avctx->internal->hwaccel_priv_data;
    struct v4l2_ctrl_hevc_sps sps;
    int ret;
    const struct decdev * decdev;
    uint32_t src_pix_fmt = V4L2_PIX_FMT_HEVC_SLICE;

    struct v4l2_ext_control control[] = {
        {
            .id = V4L2_CID_MPEG_VIDEO_HEVC_SPS,
            .ptr = &sps,
            .size = sizeof(sps),
        },
    };

    if ((ctx->pctrl = ff_v4l2_phase_control_new(2)) == NULL)
        return AVERROR(ENOMEM);

    if ((ret = devscan_build(avctx, &ctx->devscan)) != 0) {
        av_log(avctx, AV_LOG_ERROR, "Failed to find any V4L2 devices\n");
        return (AVERROR(-ret));
    }
    if ((decdev = devscan_find(ctx->devscan, src_pix_fmt)) == NULL)
    {
        av_log(avctx, AV_LOG_ERROR, "Failed to find a V4L2 device for H265\n");
        goto fail0;
    }
    av_log(avctx, AV_LOG_ERROR, "Trying V4L2 devices: %s,%s\n",
           decdev_media_path(decdev), decdev_video_path(decdev));

    if ((ctx->dbufs = dmabufs_ctl_new()) == NULL) {
        av_log(avctx, AV_LOG_ERROR, "Unable to open dmabufs\n");
        goto fail0;
    }

    if ((ctx->pq = pollqueue_new()) == NULL) {
        av_log(avctx, AV_LOG_ERROR, "Unable to create pollqueue\n");
        goto fail1;
    }

    if ((ctx->mpool = media_pool_new(decdev_media_path(decdev), ctx->pq, 4)) == NULL) {
        av_log(avctx, AV_LOG_ERROR, "Unable to create media pool\n");
        goto fail2;
    }

    if ((ctx->mbufs = mediabufs_ctl_new(avctx, decdev_video_path(decdev), ctx->pq)) == NULL) {
        av_log(avctx, AV_LOG_ERROR, "Unable to create media controls\n");
        goto fail3;
    }

    fill_sps(&sps, h);

    if (mediabufs_src_fmt_set(ctx->mbufs, src_pix_fmt, avctx->width, avctx->height)) {
        char tbuf1[5];
        av_log(avctx, AV_LOG_ERROR, "Failed to set source format: %s %dx%d\n", strfourcc(tbuf1, src_pix_fmt), avctx->width, avctx->height);
        goto fail4;
    }

    if (mediabufs_set_ext_ctrl(ctx->mbufs, NULL, V4L2_CID_MPEG_VIDEO_HEVC_SPS, &sps, sizeof(sps))) {
        av_log(avctx, AV_LOG_ERROR, "Failed to set initial SPS\n");
        goto fail4;
    }

    if (mediabufs_dst_fmt_set(ctx->mbufs, 0 /** rt fmt **/, avctx->width, avctx->height)) {
        char tbuf1[5];
        av_log(avctx, AV_LOG_ERROR, "Failed to set destination format: %s %dx%d\n", strfourcc(tbuf1, src_pix_fmt), avctx->width, avctx->height);
        goto fail4;
    }

    if (mediabufs_src_pool_create(ctx->mbufs, ctx->dbufs, 6)) {
        av_log(avctx, AV_LOG_ERROR, "Failed to create source pool\n");
        goto fail4;
    }

    if (mediabufs_dst_slots_create(ctx->mbufs, 1)) {
        av_log(avctx, AV_LOG_ERROR, "Failed to create destination slots\n");
        goto fail4;
    }

    if (mediabufs_stream_on(ctx->mbufs)) {
        av_log(avctx, AV_LOG_ERROR, "Failed stream on\n");
        goto fail4;
    }

    ret = ff_v4l2_request_init(avctx, V4L2_PIX_FMT_HEVC_SLICE, 4 * 1024 * 1024, control, FF_ARRAY_ELEMS(control));
    if (ret)
        return ret;

    return v4l2_request_hevc_set_controls(avctx);

fail4:
    mediabufs_ctl_unref(&ctx->mbufs);
fail3:
    media_pool_delete(&ctx->mpool);
fail2:
    pollqueue_delete(&ctx->pq);
fail1:
    dmabufs_ctl_delete(&ctx->dbufs);
fail0:
    devscan_delete(&ctx->devscan);
    return AVERROR(ENOMEM);
}

const AVHWAccel ff_hevc_v4l2request_hwaccel = {
    .name           = "hevc_v4l2request",
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_HEVC,
    .pix_fmt        = AV_PIX_FMT_DRM_PRIME,
    .alloc_frame    = v4l2_request_hevc_alloc_frame,
    .start_frame    = v4l2_request_hevc_start_frame,
    .decode_slice   = v4l2_request_hevc_decode_slice,
    .end_frame      = v4l2_request_hevc_end_frame,
    .abort_frame    = v4l2_request_hevc_abort_frame,
    .frame_priv_data_size = sizeof(V4L2RequestControlsHEVC),
    .init           = v4l2_request_hevc_init,
    .uninit         = v4l2_request_hevc_uninit,
    .priv_data_size = sizeof(V4L2RequestContextHEVC),
    .frame_params   = ff_v4l2_request_frame_params,
    .caps_internal  = HWACCEL_CAP_ASYNC_SAFE | HWACCEL_CAP_MT_SAFE,
};
