// File included by v4l2_req_hevc_v* - not compiled on its own

#include "decode.h"
#include "hevcdec.h"
#include "hwconfig.h"
#include "internal.h"
#include "thread.h"

#if HEVC_CTRLS_VERSION == 1
#include "hevc-ctrls-v1.h"

// Fixup renamed entries
#define V4L2_HEVC_PPS_FLAG_DEPENDENT_SLICE_SEGMENT_ENABLED V4L2_HEVC_PPS_FLAG_DEPENDENT_SLICE_SEGMENT

#elif HEVC_CTRLS_VERSION == 2
#include "hevc-ctrls-v2.h"
#elif HEVC_CTRLS_VERSION == 3
#include "hevc-ctrls-v3.h"
#elif HEVC_CTRLS_VERSION == 4
#include <linux/v4l2-controls.h>
#if !defined(V4L2_CID_STATELESS_HEVC_SPS)
#include "hevc-ctrls-v4.h"
#endif
#else
#error Unknown HEVC_CTRLS_VERSION
#endif

#ifndef V4L2_CID_STATELESS_HEVC_SPS
#define V4L2_CID_STATELESS_HEVC_SPS                     V4L2_CID_MPEG_VIDEO_HEVC_SPS
#define V4L2_CID_STATELESS_HEVC_PPS                     V4L2_CID_MPEG_VIDEO_HEVC_PPS
#define V4L2_CID_STATELESS_HEVC_SLICE_PARAMS            V4L2_CID_MPEG_VIDEO_HEVC_SLICE_PARAMS
#define V4L2_CID_STATELESS_HEVC_SCALING_MATRIX          V4L2_CID_MPEG_VIDEO_HEVC_SCALING_MATRIX
#define V4L2_CID_STATELESS_HEVC_DECODE_PARAMS           V4L2_CID_MPEG_VIDEO_HEVC_DECODE_PARAMS
#define V4L2_CID_STATELESS_HEVC_DECODE_MODE             V4L2_CID_MPEG_VIDEO_HEVC_DECODE_MODE
#define V4L2_CID_STATELESS_HEVC_START_CODE              V4L2_CID_MPEG_VIDEO_HEVC_START_CODE

#define V4L2_STATELESS_HEVC_DECODE_MODE_SLICE_BASED     V4L2_MPEG_VIDEO_HEVC_DECODE_MODE_SLICE_BASED
#define V4L2_STATELESS_HEVC_DECODE_MODE_FRAME_BASED     V4L2_MPEG_VIDEO_HEVC_DECODE_MODE_FRAME_BASED
#define V4L2_STATELESS_HEVC_START_CODE_NONE             V4L2_MPEG_VIDEO_HEVC_START_CODE_NONE
#define V4L2_STATELESS_HEVC_START_CODE_ANNEX_B          V4L2_MPEG_VIDEO_HEVC_START_CODE_ANNEX_B
#endif

// Should be in videodev2 but we might not have a good enough one
#ifndef V4L2_PIX_FMT_HEVC_SLICE
#define V4L2_PIX_FMT_HEVC_SLICE v4l2_fourcc('S', '2', '6', '5') /* HEVC parsed slices */
#endif

#include "v4l2_request_hevc.h"

#include "libavutil/hwcontext_drm.h"

#include <semaphore.h>
#include <pthread.h>

#include "v4l2_req_devscan.h"
#include "v4l2_req_dmabufs.h"
#include "v4l2_req_pollqueue.h"
#include "v4l2_req_media.h"
#include "v4l2_req_utils.h"

// Attached to buf[0] in frame
// Pooled in hwcontext so generally create once - 1/frame
typedef struct V4L2MediaReqDescriptor {
    AVDRMFrameDescriptor drm;

    // Media
    uint64_t timestamp;
    struct qent_dst * qe_dst;

    // Decode only - should be NULL by the time we emit the frame
    struct req_decode_ent decode_ent;

    struct media_request *req;
    struct qent_src *qe_src;

#if HEVC_CTRLS_VERSION >= 2
    struct v4l2_ctrl_hevc_decode_params dec;
#endif

    size_t num_slices;
    size_t alloced_slices;
    struct v4l2_ctrl_hevc_slice_params * slice_params;
    struct slice_info * slices;

    size_t num_offsets;
    size_t alloced_offsets;
    uint32_t *offsets;

} V4L2MediaReqDescriptor;

struct slice_info {
    const uint8_t * ptr;
    size_t len; // bytes
    size_t n_offsets;
};

// Handy container for accumulating controls before setting
struct req_controls {
    int has_scaling;
    struct timeval tv;
    struct v4l2_ctrl_hevc_sps sps;
    struct v4l2_ctrl_hevc_pps pps;
    struct v4l2_ctrl_hevc_scaling_matrix scaling_matrix;
};

//static uint8_t nalu_slice_start_code[] = { 0x00, 0x00, 0x01 };


// Get an FFmpeg format from the v4l2 format
static enum AVPixelFormat pixel_format_from_format(const struct v4l2_format *const format)
{
    switch (V4L2_TYPE_IS_MULTIPLANAR(format->type) ?
            format->fmt.pix_mp.pixelformat : format->fmt.pix.pixelformat) {
    case V4L2_PIX_FMT_YUV420:
        return AV_PIX_FMT_YUV420P;
    case V4L2_PIX_FMT_NV12:
        return AV_PIX_FMT_NV12;
#if CONFIG_SAND
    case V4L2_PIX_FMT_NV12_COL128:
        return AV_PIX_FMT_RPI4_8;
    case V4L2_PIX_FMT_NV12_10_COL128:
        return AV_PIX_FMT_RPI4_10;
#endif
    default:
        break;
    }
    return AV_PIX_FMT_NONE;
}

static inline uint64_t frame_capture_dpb(const AVFrame * const frame)
{
    const V4L2MediaReqDescriptor *const rd = (V4L2MediaReqDescriptor *)frame->data[0];
    return rd->timestamp;
}

static inline void frame_set_capture_dpb(AVFrame * const frame, const uint64_t dpb_stamp)
{
    V4L2MediaReqDescriptor *const rd = (V4L2MediaReqDescriptor *)frame->data[0];
    rd->timestamp = dpb_stamp;
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

#if HEVC_CTRLS_VERSION <= 2
static int find_frame_rps_type(const HEVCContext *h, uint64_t timestamp)
{
    const HEVCFrame *frame;
    int i;

    for (i = 0; i < h->rps[ST_CURR_BEF].nb_refs; i++) {
        frame = h->rps[ST_CURR_BEF].ref[i];
        if (frame && timestamp == frame_capture_dpb(frame->frame))
            return V4L2_HEVC_DPB_ENTRY_RPS_ST_CURR_BEFORE;
    }

    for (i = 0; i < h->rps[ST_CURR_AFT].nb_refs; i++) {
        frame = h->rps[ST_CURR_AFT].ref[i];
        if (frame && timestamp == frame_capture_dpb(frame->frame))
            return V4L2_HEVC_DPB_ENTRY_RPS_ST_CURR_AFTER;
    }

    for (i = 0; i < h->rps[LT_CURR].nb_refs; i++) {
        frame = h->rps[LT_CURR].ref[i];
        if (frame && timestamp == frame_capture_dpb(frame->frame))
            return V4L2_HEVC_DPB_ENTRY_RPS_LT_CURR;
    }

    return 0;
}
#endif

static unsigned int
get_ref_pic_index(const HEVCContext *h, const HEVCFrame *frame,
                  const struct v4l2_hevc_dpb_entry * const entries,
                  const unsigned int num_entries)
{
    uint64_t timestamp;

    if (!frame)
        return 0;

    timestamp = frame_capture_dpb(frame->frame);

    for (unsigned int i = 0; i < num_entries; i++) {
        if (entries[i].timestamp == timestamp)
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

static int slice_add(V4L2MediaReqDescriptor * const rd)
{
    if (rd->num_slices >= rd->alloced_slices) {
        struct v4l2_ctrl_hevc_slice_params * p2;
        struct slice_info * s2;
        size_t n2 = rd->alloced_slices == 0 ? 8 : rd->alloced_slices * 2;

        p2 = av_realloc_array(rd->slice_params, n2, sizeof(*p2));
        if (p2 == NULL)
            return AVERROR(ENOMEM);
        rd->slice_params = p2;

        s2 = av_realloc_array(rd->slices, n2, sizeof(*s2));
        if (s2 == NULL)
            return AVERROR(ENOMEM);
        rd->slices = s2;

        rd->alloced_slices = n2;
    }
    ++rd->num_slices;
    return 0;
}

static int offsets_add(V4L2MediaReqDescriptor *const rd, const size_t n, const unsigned * const offsets)
{
    if (rd->num_offsets + n > rd->alloced_offsets) {
        size_t n2 = rd->alloced_slices == 0 ? 128 : rd->alloced_slices * 2;
        void * p2;
        while (rd->num_offsets + n > n2)
            n2 *= 2;
        if ((p2 = av_realloc_array(rd->offsets, n2, sizeof(*rd->offsets))) == NULL)
            return AVERROR(ENOMEM);
        rd->offsets = p2;
        rd->alloced_offsets = n2;
    }
    for (size_t i = 0; i != n; ++i)
        rd->offsets[rd->num_offsets++] = offsets[i] - 1;
    return 0;
}

static unsigned int
fill_dpb_entries(const HEVCContext * const h, struct v4l2_hevc_dpb_entry * const entries)
{
    unsigned int i;
    unsigned int n = 0;
    const HEVCFrame * const pic = h->ref;

    for (i = 0; i < FF_ARRAY_ELEMS(h->DPB); i++) {
        const HEVCFrame * const frame = &h->DPB[i];
        if (frame != pic && (frame->flags & (HEVC_FRAME_FLAG_LONG_REF | HEVC_FRAME_FLAG_SHORT_REF))) {
            struct v4l2_hevc_dpb_entry * const entry = entries + n++;

            entry->timestamp = frame_capture_dpb(frame->frame);
#if HEVC_CTRLS_VERSION <= 2
            entry->rps = find_frame_rps_type(h, entry->timestamp);
#else
            entry->flags = (frame->flags & HEVC_FRAME_FLAG_LONG_REF) == 0 ? 0 :
                V4L2_HEVC_DPB_ENTRY_LONG_TERM_REFERENCE;
#endif
            entry->field_pic = frame->frame->interlaced_frame;

#if HEVC_CTRLS_VERSION <= 3
            /* TODO: Interleaved: Get the POC for each field. */
            entry->pic_order_cnt[0] = frame->poc;
            entry->pic_order_cnt[1] = frame->poc;
#else
            entry->pic_order_cnt_val = frame->poc;
#endif
        }
    }
    return n;
}

static void fill_slice_params(const HEVCContext * const h,
#if HEVC_CTRLS_VERSION >= 2
                              const struct v4l2_ctrl_hevc_decode_params * const dec,
#endif
                              struct v4l2_ctrl_hevc_slice_params *slice_params,
                              uint32_t bit_size, uint32_t bit_offset)
{
    const SliceHeader * const sh = &h->sh;
#if HEVC_CTRLS_VERSION >= 2
    const struct v4l2_hevc_dpb_entry *const dpb = dec->dpb;
    const unsigned int dpb_n = dec->num_active_dpb_entries;
#else
    struct v4l2_hevc_dpb_entry *const dpb = slice_params->dpb;
    unsigned int dpb_n;
#endif
    unsigned int i;
    RefPicList *rpl;

    *slice_params = (struct v4l2_ctrl_hevc_slice_params) {
        .bit_size = bit_size,
#if HEVC_CTRLS_VERSION <= 3
        .data_bit_offset = bit_offset,
#else
        .data_byte_offset = bit_offset / 8 + 1,
#endif
        /* ISO/IEC 23008-2, ITU-T Rec. H.265: General slice segment header */
        .slice_segment_addr = sh->slice_segment_addr,

        /* ISO/IEC 23008-2, ITU-T Rec. H.265: NAL unit header */
        .nal_unit_type = h->nal_unit_type,
        .nuh_temporal_id_plus1 = h->temporal_id + 1,

        /* ISO/IEC 23008-2, ITU-T Rec. H.265: General slice segment header */
        .slice_type = sh->slice_type,
        .colour_plane_id = sh->colour_plane_id,
        .slice_pic_order_cnt = h->ref->poc,
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

#if HEVC_CTRLS_VERSION < 2
        /* ISO/IEC 23008-2, ITU-T Rec. H.265: General slice segment header */
        .num_rps_poc_st_curr_before = h->rps[ST_CURR_BEF].nb_refs,
        .num_rps_poc_st_curr_after = h->rps[ST_CURR_AFT].nb_refs,
        .num_rps_poc_lt_curr = h->rps[LT_CURR].nb_refs,
#endif
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

#if HEVC_CTRLS_VERSION < 2
    dpb_n = fill_dpb_entries(h, dpb);
    slice_params->num_active_dpb_entries = dpb_n;
#endif

    if (sh->slice_type != HEVC_SLICE_I) {
        rpl = &h->ref->refPicList[0];
        for (i = 0; i < rpl->nb_refs; i++)
            slice_params->ref_idx_l0[i] = get_ref_pic_index(h, rpl->ref[i], dpb, dpb_n);
    }

    if (sh->slice_type == HEVC_SLICE_B) {
        rpl = &h->ref->refPicList[1];
        for (i = 0; i < rpl->nb_refs; i++)
            slice_params->ref_idx_l1[i] = get_ref_pic_index(h, rpl->ref[i], dpb, dpb_n);
    }

    fill_pred_table(h, &slice_params->pred_weight_table);

    slice_params->num_entry_point_offsets = sh->num_entry_point_offsets;
#if HEVC_CTRLS_VERSION <= 3
    if (slice_params->num_entry_point_offsets > 256) {
        slice_params->num_entry_point_offsets = 256;
        av_log(NULL, AV_LOG_ERROR, "%s: Currently only 256 entry points are supported, but slice has %d entry points.\n", __func__, sh->num_entry_point_offsets);
    }

    for (i = 0; i < slice_params->num_entry_point_offsets; i++)
        slice_params->entry_point_offset_minus1[i] = sh->entry_point_offset[i] - 1;
#endif
}

#if HEVC_CTRLS_VERSION >= 2
static void
fill_decode_params(const HEVCContext * const h,
                   struct v4l2_ctrl_hevc_decode_params * const dec)
{
    unsigned int i;

    *dec = (struct v4l2_ctrl_hevc_decode_params){
        .pic_order_cnt_val = h->poc,
        .num_poc_st_curr_before = h->rps[ST_CURR_BEF].nb_refs,
        .num_poc_st_curr_after = h->rps[ST_CURR_AFT].nb_refs,
        .num_poc_lt_curr = h->rps[LT_CURR].nb_refs,
    };

    dec->num_active_dpb_entries = fill_dpb_entries(h, dec->dpb);

    // The docn does seem to ask that we fit our 32 bit signed POC into
    // a U8 so... (To be fair 16 bits would be enough)
    // Luckily we (Pi) don't use these fields
    for (i = 0; i != h->rps[ST_CURR_BEF].nb_refs; ++i)
        dec->poc_st_curr_before[i] = h->rps[ST_CURR_BEF].ref[i]->poc;
    for (i = 0; i != h->rps[ST_CURR_AFT].nb_refs; ++i)
        dec->poc_st_curr_after[i] = h->rps[ST_CURR_AFT].ref[i]->poc;
    for (i = 0; i != h->rps[LT_CURR].nb_refs; ++i)
        dec->poc_lt_curr[i] = h->rps[LT_CURR].ref[i]->poc;

    if (IS_IRAP(h))
        dec->flags |= V4L2_HEVC_DECODE_PARAM_FLAG_IRAP_PIC;
    if (IS_IDR(h))
        dec->flags |= V4L2_HEVC_DECODE_PARAM_FLAG_IDR_PIC;
    if (h->sh.no_output_of_prior_pics_flag)
        dec->flags |= V4L2_HEVC_DECODE_PARAM_FLAG_NO_OUTPUT_OF_PRIOR;

}
#endif

static void fill_sps(struct v4l2_ctrl_hevc_sps *ctrl, const HEVCSPS *sps)
{
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
        .chroma_format_idc = sps->chroma_format_idc,
        .sps_max_sub_layers_minus1 = sps->max_sub_layers - 1,
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

static void fill_scaling_matrix(const ScalingList * const sl,
                                struct v4l2_ctrl_hevc_scaling_matrix * const sm)
{
    unsigned int i;

    for (i = 0; i < 6; i++) {
        unsigned int j;

        for (j = 0; j < 16; j++)
            sm->scaling_list_4x4[i][j] = sl->sl[0][i][j];
        for (j = 0; j < 64; j++) {
            sm->scaling_list_8x8[i][j]   = sl->sl[1][i][j];
            sm->scaling_list_16x16[i][j] = sl->sl[2][i][j];
            if (i < 2)
                sm->scaling_list_32x32[i][j] = sl->sl[3][i * 3][j];
        }
        sm->scaling_list_dc_coef_16x16[i] = sl->sl_dc[0][i];
        if (i < 2)
            sm->scaling_list_dc_coef_32x32[i] = sl->sl_dc[1][i * 3];
    }
}

static void fill_pps(struct v4l2_ctrl_hevc_pps * const ctrl, const HEVCPPS * const pps)
{
    uint64_t flags = 0;

    if (pps->dependent_slice_segments_enabled_flag)
        flags |= V4L2_HEVC_PPS_FLAG_DEPENDENT_SLICE_SEGMENT_ENABLED;

    if (pps->output_flag_present_flag)
        flags |= V4L2_HEVC_PPS_FLAG_OUTPUT_FLAG_PRESENT;

    if (pps->sign_data_hiding_flag)
        flags |= V4L2_HEVC_PPS_FLAG_SIGN_DATA_HIDING_ENABLED;

    if (pps->cabac_init_present_flag)
        flags |= V4L2_HEVC_PPS_FLAG_CABAC_INIT_PRESENT;

    if (pps->constrained_intra_pred_flag)
        flags |= V4L2_HEVC_PPS_FLAG_CONSTRAINED_INTRA_PRED;

    if (pps->transform_skip_enabled_flag)
        flags |= V4L2_HEVC_PPS_FLAG_TRANSFORM_SKIP_ENABLED;

    if (pps->cu_qp_delta_enabled_flag)
        flags |= V4L2_HEVC_PPS_FLAG_CU_QP_DELTA_ENABLED;

    if (pps->pic_slice_level_chroma_qp_offsets_present_flag)
        flags |= V4L2_HEVC_PPS_FLAG_PPS_SLICE_CHROMA_QP_OFFSETS_PRESENT;

    if (pps->weighted_pred_flag)
        flags |= V4L2_HEVC_PPS_FLAG_WEIGHTED_PRED;

    if (pps->weighted_bipred_flag)
        flags |= V4L2_HEVC_PPS_FLAG_WEIGHTED_BIPRED;

    if (pps->transquant_bypass_enable_flag)
        flags |= V4L2_HEVC_PPS_FLAG_TRANSQUANT_BYPASS_ENABLED;

    if (pps->tiles_enabled_flag)
        flags |= V4L2_HEVC_PPS_FLAG_TILES_ENABLED;

    if (pps->entropy_coding_sync_enabled_flag)
        flags |= V4L2_HEVC_PPS_FLAG_ENTROPY_CODING_SYNC_ENABLED;

    if (pps->loop_filter_across_tiles_enabled_flag)
        flags |= V4L2_HEVC_PPS_FLAG_LOOP_FILTER_ACROSS_TILES_ENABLED;

    if (pps->seq_loop_filter_across_slices_enabled_flag)
        flags |= V4L2_HEVC_PPS_FLAG_PPS_LOOP_FILTER_ACROSS_SLICES_ENABLED;

    if (pps->deblocking_filter_override_enabled_flag)
        flags |= V4L2_HEVC_PPS_FLAG_DEBLOCKING_FILTER_OVERRIDE_ENABLED;

    if (pps->disable_dbf)
        flags |= V4L2_HEVC_PPS_FLAG_PPS_DISABLE_DEBLOCKING_FILTER;

    if (pps->lists_modification_present_flag)
        flags |= V4L2_HEVC_PPS_FLAG_LISTS_MODIFICATION_PRESENT;

    if (pps->slice_header_extension_present_flag)
        flags |= V4L2_HEVC_PPS_FLAG_SLICE_SEGMENT_HEADER_EXTENSION_PRESENT;

    /* ISO/IEC 23008-2, ITU-T Rec. H.265: Picture parameter set */
    *ctrl = (struct v4l2_ctrl_hevc_pps) {
        .num_extra_slice_header_bits = pps->num_extra_slice_header_bits,
        .init_qp_minus26 = pps->pic_init_qp_minus26,
        .diff_cu_qp_delta_depth = pps->diff_cu_qp_delta_depth,
        .pps_cb_qp_offset = pps->cb_qp_offset,
        .pps_cr_qp_offset = pps->cr_qp_offset,
        .pps_beta_offset_div2 = pps->beta_offset / 2,
        .pps_tc_offset_div2 = pps->tc_offset / 2,
        .log2_parallel_merge_level_minus2 = pps->log2_parallel_merge_level - 2,
        .flags = flags
    };


    if (pps->tiles_enabled_flag) {
        ctrl->num_tile_columns_minus1 = pps->num_tile_columns - 1;
        ctrl->num_tile_rows_minus1 = pps->num_tile_rows - 1;

        for (int i = 0; i < pps->num_tile_columns; i++)
            ctrl->column_width_minus1[i] = pps->column_width[i] - 1;

        for (int i = 0; i < pps->num_tile_rows; i++)
            ctrl->row_height_minus1[i] = pps->row_height[i] - 1;
    }
}

// Called before finally returning the frame to the user
// Set corrupt flag here as this is actually the frame structure that
// is going to the user (in MT land each thread has its own pool)
static int frame_post_process(void *logctx, AVFrame *frame)
{
    V4L2MediaReqDescriptor *rd = (V4L2MediaReqDescriptor*)frame->data[0];

//    av_log(NULL, AV_LOG_INFO, "%s\n", __func__);
    frame->flags &= ~AV_FRAME_FLAG_CORRUPT;
    if (rd->qe_dst) {
        MediaBufsStatus stat = qent_dst_wait(rd->qe_dst);
        if (stat != MEDIABUFS_STATUS_SUCCESS) {
            av_log(logctx, AV_LOG_ERROR, "%s: Decode fail\n", __func__);
            frame->flags |= AV_FRAME_FLAG_CORRUPT;
        }
    }

    return 0;
}

static inline struct timeval cvt_dpb_to_tv(uint64_t t)
{
    t /= 1000;
    return (struct timeval){
        .tv_usec = t % 1000000,
        .tv_sec = t / 1000000
    };
}

static inline uint64_t cvt_timestamp_to_dpb(const unsigned int t)
{
    return (uint64_t)t * 1000;
}

static int v4l2_request_hevc_start_frame(AVCodecContext *avctx,
                                         av_unused const uint8_t *buffer,
                                         av_unused uint32_t size)
{
    const HEVCContext *h = avctx->priv_data;
    V4L2MediaReqDescriptor *const rd = (V4L2MediaReqDescriptor *)h->ref->frame->data[0];
    V4L2RequestContextHEVC * const ctx = avctx->internal->hwaccel_priv_data;

//    av_log(NULL, AV_LOG_INFO, "%s\n", __func__);
    decode_q_add(&ctx->decode_q, &rd->decode_ent);

    rd->num_slices = 0;
    ctx->timestamp++;
    rd->timestamp = cvt_timestamp_to_dpb(ctx->timestamp);

    {
        FrameDecodeData * const fdd = (FrameDecodeData*)h->ref->frame->private_ref->data;
        fdd->post_process = frame_post_process;
    }

    // qe_dst needs to be bound to the data buffer and only returned when that is
    if (!rd->qe_dst)
    {
        if ((rd->qe_dst = mediabufs_dst_qent_alloc(ctx->mbufs, ctx->dbufs)) == NULL) {
            av_log(avctx, AV_LOG_ERROR, "%s: Failed to get dst buffer\n", __func__);
            return AVERROR(ENOMEM);
        }
    }

    ff_thread_finish_setup(avctx); // Allow next thread to enter rpi_hevc_start_frame

    return 0;
}

// Object fd & size will be zapped by this & need setting later
static int drm_from_format(AVDRMFrameDescriptor * const desc, const struct v4l2_format * const format)
{
    AVDRMLayerDescriptor *layer = &desc->layers[0];
    unsigned int width;
    unsigned int height;
    unsigned int bpl;
    uint32_t pixelformat;

    if (V4L2_TYPE_IS_MULTIPLANAR(format->type)) {
        width       = format->fmt.pix_mp.width;
        height      = format->fmt.pix_mp.height;
        pixelformat = format->fmt.pix_mp.pixelformat;
        bpl         = format->fmt.pix_mp.plane_fmt[0].bytesperline;
    }
    else {
        width       = format->fmt.pix.width;
        height      = format->fmt.pix.height;
        pixelformat = format->fmt.pix.pixelformat;
        bpl         = format->fmt.pix.bytesperline;
    }

    switch (pixelformat) {
    case V4L2_PIX_FMT_NV12:
        layer->format = DRM_FORMAT_NV12;
        desc->objects[0].format_modifier = DRM_FORMAT_MOD_LINEAR;
        break;
#if CONFIG_SAND
    case V4L2_PIX_FMT_NV12_COL128:
        layer->format = DRM_FORMAT_NV12;
        desc->objects[0].format_modifier = DRM_FORMAT_MOD_BROADCOM_SAND128_COL_HEIGHT(bpl);
        break;
    case V4L2_PIX_FMT_NV12_10_COL128:
        layer->format = DRM_FORMAT_P030;
        desc->objects[0].format_modifier = DRM_FORMAT_MOD_BROADCOM_SAND128_COL_HEIGHT(bpl);
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
    layer->planes[0].pitch = bpl;
#if CONFIG_SAND
    if (pixelformat == V4L2_PIX_FMT_NV12_COL128) {
        layer->planes[1].object_index = 0;
        layer->planes[1].offset = height * 128;
        layer->planes[0].pitch = width;
        layer->planes[1].pitch = width;
    }
    else if (pixelformat == V4L2_PIX_FMT_NV12_10_COL128) {
        layer->planes[1].object_index = 0;
        layer->planes[1].offset = height * 128;
        layer->planes[0].pitch = width * 2; // Lies but it keeps DRM import happy
        layer->planes[1].pitch = width * 2;
    }
    else
#endif
    {
        layer->planes[1].object_index = 0;
        layer->planes[1].offset = layer->planes[0].pitch * height;
        layer->planes[1].pitch = layer->planes[0].pitch;
    }

    return 0;
}

static int
set_req_ctls(V4L2RequestContextHEVC *ctx, struct media_request * const mreq,
    struct req_controls *const controls,
#if HEVC_CTRLS_VERSION >= 2
    struct v4l2_ctrl_hevc_decode_params * const dec,
#endif
    struct v4l2_ctrl_hevc_slice_params * const slices, const unsigned int slice_count,
    void * const offsets, const size_t offset_count)
{
    int rv;
#if HEVC_CTRLS_VERSION >= 2
    unsigned int n = 4;
#else
    unsigned int n = 3;
#endif

    struct v4l2_ext_control control[6] = {
        {
            .id = V4L2_CID_STATELESS_HEVC_SPS,
            .ptr = &controls->sps,
            .size = sizeof(controls->sps),
        },
        {
            .id = V4L2_CID_STATELESS_HEVC_PPS,
            .ptr = &controls->pps,
            .size = sizeof(controls->pps),
        },
#if HEVC_CTRLS_VERSION >= 2
        {
            .id = V4L2_CID_STATELESS_HEVC_DECODE_PARAMS,
            .ptr = dec,
            .size = sizeof(*dec),
        },
#endif
        {
            .id = V4L2_CID_STATELESS_HEVC_SLICE_PARAMS,
            .ptr = slices,
            .size = sizeof(*slices) * slice_count,
        },
    };

    if (controls->has_scaling)
        control[n++] = (struct v4l2_ext_control) {
            .id = V4L2_CID_STATELESS_HEVC_SCALING_MATRIX,
            .ptr = &controls->scaling_matrix,
            .size = sizeof(controls->scaling_matrix),
        };

#if HEVC_CTRLS_VERSION >= 4
    if (offsets)
        control[n++] = (struct v4l2_ext_control) {
            .id = V4L2_CID_STATELESS_HEVC_ENTRY_POINT_OFFSETS,
            .ptr = offsets,
            .size = sizeof(((struct V4L2MediaReqDescriptor *)0)->offsets[0]) * offset_count,
        };
#endif

    rv = mediabufs_ctl_set_ext_ctrls(ctx->mbufs, mreq, control, n);

    return rv;
}

static int v4l2_request_hevc_decode_slice(AVCodecContext *avctx, const uint8_t *buffer, uint32_t size)
{
    const HEVCContext * const h = avctx->priv_data;
    V4L2RequestContextHEVC * const ctx = avctx->internal->hwaccel_priv_data;
    V4L2MediaReqDescriptor * const rd = (V4L2MediaReqDescriptor*)h->ref->frame->data[0];
    int bcount = get_bits_count(&h->HEVClc->gb);
    uint32_t boff = (ptr_from_index(buffer, bcount/8 + 1) - (buffer + bcount/8 + 1)) * 8 + bcount;

    const unsigned int n = rd->num_slices;
    const unsigned int block_start = (n / ctx->max_slices) * ctx->max_slices;

    int rv;
    struct slice_info * si;

    if ((rv = slice_add(rd)) != 0)
        return rv;

    si = rd->slices + n;
    si->ptr = buffer;
    si->len = size;
    si->n_offsets = rd->num_offsets;

    if (n != block_start) {
        struct slice_info *const si0 = rd->slices + block_start;
        const size_t offset = (buffer - si0->ptr);
        boff += offset * 8;
        size += offset;
        si0->len = si->len + offset;
    }

#if HEVC_CTRLS_VERSION >= 2
    if (n == 0)
        fill_decode_params(h, &rd->dec);
    fill_slice_params(h, &rd->dec, rd->slice_params + n, size * 8, boff);
#else
    fill_slice_params(h, rd->slice_params + n, size * 8, boff);
#endif
    if (ctx->max_offsets != 0 &&
        (rv = offsets_add(rd, h->sh.num_entry_point_offsets, h->sh.entry_point_offset)) != 0)
        return rv;

    return 0;
}

static void v4l2_request_hevc_abort_frame(AVCodecContext * const avctx)
{
    const HEVCContext * const h = avctx->priv_data;
    if (h->ref != NULL) {
        V4L2MediaReqDescriptor *const rd = (V4L2MediaReqDescriptor *)h->ref->frame->data[0];
        V4L2RequestContextHEVC * const ctx = avctx->internal->hwaccel_priv_data;

        media_request_abort(&rd->req);
        mediabufs_src_qent_abort(ctx->mbufs, &rd->qe_src);

        decode_q_remove(&ctx->decode_q, &rd->decode_ent);
    }
}

static int send_slice(AVCodecContext * const avctx,
                      V4L2MediaReqDescriptor * const rd,
                      struct req_controls *const controls,
                      const unsigned int i, const unsigned int j)
{
    V4L2RequestContextHEVC * const ctx = avctx->internal->hwaccel_priv_data;

    const int is_last = (j == rd->num_slices);
    struct slice_info *const si = rd->slices + i;
    struct media_request * req = NULL;
    struct qent_src * src = NULL;
    MediaBufsStatus stat;
    void * offsets = rd->offsets + rd->slices[i].n_offsets;
    size_t n_offsets = (is_last ? rd->num_offsets : rd->slices[j].n_offsets) - rd->slices[i].n_offsets;

    if ((req = media_request_get(ctx->mpool)) == NULL) {
        av_log(avctx, AV_LOG_ERROR, "%s: Failed to alloc media request\n", __func__);
        return AVERROR(ENOMEM);
    }

    if (set_req_ctls(ctx, req,
                     controls,
#if HEVC_CTRLS_VERSION >= 2
                     &rd->dec,
#endif
                     rd->slice_params + i, j - i,
                     offsets, n_offsets)) {
        av_log(avctx, AV_LOG_ERROR, "%s: Failed to set req ctls\n", __func__);
        goto fail1;
    }

    if ((src = mediabufs_src_qent_get(ctx->mbufs)) == NULL) {
        av_log(avctx, AV_LOG_ERROR, "%s: Failed to get src buffer\n", __func__);
        goto fail1;
    }

    if (qent_src_data_copy(src, 0, si->ptr, si->len, ctx->dbufs) != 0) {
        av_log(avctx, AV_LOG_ERROR, "%s: Failed data copy\n", __func__);
        goto fail2;
    }

    if (qent_src_params_set(src, &controls->tv)) {
        av_log(avctx, AV_LOG_ERROR, "%s: Failed src param set\n", __func__);
        goto fail2;
    }

#warning ANNEX_B start code
//        if (ctx->start_code == V4L2_MPEG_VIDEO_HEVC_START_CODE_ANNEX_B) {
//        }

    stat = mediabufs_start_request(ctx->mbufs, &req, &src,
                                   i == 0 ? rd->qe_dst : NULL,
                                   is_last);

    if (stat != MEDIABUFS_STATUS_SUCCESS) {
        av_log(avctx, AV_LOG_ERROR, "%s: Failed to start request\n", __func__);
        return AVERROR_UNKNOWN;
    }
    return 0;

fail2:
    mediabufs_src_qent_abort(ctx->mbufs, &src);
fail1:
    media_request_abort(&req);
    return AVERROR_UNKNOWN;
}

static int v4l2_request_hevc_end_frame(AVCodecContext *avctx)
{
    const HEVCContext * const h = avctx->priv_data;
    V4L2MediaReqDescriptor *rd = (V4L2MediaReqDescriptor*)h->ref->frame->data[0];
    V4L2RequestContextHEVC *ctx = avctx->internal->hwaccel_priv_data;
    struct req_controls rc;
    unsigned int i;
    int rv;

    // It is possible, though maybe a bug, to get an end_frame without
    // a previous start_frame.  If we do then give up.
    if (!decode_q_in_q(&rd->decode_ent)) {
        av_log(avctx, AV_LOG_DEBUG, "%s: Frame not in decode Q\n", __func__);
        return AVERROR_INVALIDDATA;
    }

    {
        const ScalingList *sl = h->ps.pps->scaling_list_data_present_flag ?
                                    &h->ps.pps->scaling_list :
                                h->ps.sps->scaling_list_enable_flag ?
                                    &h->ps.sps->scaling_list : NULL;


        memset(&rc, 0, sizeof(rc));
        rc.tv = cvt_dpb_to_tv(rd->timestamp);
        fill_sps(&rc.sps, h->ps.sps);
        fill_pps(&rc.pps, h->ps.pps);
        if (sl) {
            rc.has_scaling = 1;
            fill_scaling_matrix(sl, &rc.scaling_matrix);
        }
    }

    decode_q_wait(&ctx->decode_q, &rd->decode_ent);

    // qe_dst needs to be bound to the data buffer and only returned when that is
    // Alloc almost certainly wants to be serialised if there is any chance of blocking
    // so we get the next frame to be free in the thread that needs it for decode first.
    //
    // In our current world this probably isn't a concern but put it here anyway
    if (!rd->qe_dst)
    {
        if ((rd->qe_dst = mediabufs_dst_qent_alloc(ctx->mbufs, ctx->dbufs)) == NULL) {
            av_log(avctx, AV_LOG_ERROR, "%s: Failed to get dst buffer\n", __func__);
            rv = AVERROR(ENOMEM);
            goto fail;
        }
    }

    // Send as slices
    for (i = 0; i < rd->num_slices; i += ctx->max_slices) {
        const unsigned int e = FFMIN(rd->num_slices, i + ctx->max_slices);
        if ((rv = send_slice(avctx, rd, &rc, i, e)) != 0)
            goto fail;
    }

    // Set the drm_prime desriptor
    drm_from_format(&rd->drm, mediabufs_dst_fmt(ctx->mbufs));
    rd->drm.objects[0].fd = dmabuf_fd(qent_dst_dmabuf(rd->qe_dst, 0));
    rd->drm.objects[0].size = dmabuf_size(qent_dst_dmabuf(rd->qe_dst, 0));

    decode_q_remove(&ctx->decode_q, &rd->decode_ent);
    return 0;

fail:
    decode_q_remove(&ctx->decode_q, &rd->decode_ent);
    return rv;
}

// Initial check & init
static int
probe(AVCodecContext * const avctx, V4L2RequestContextHEVC * const ctx)
{
    const HEVCContext *h = avctx->priv_data;
    const HEVCSPS * const sps = h->ps.sps;
    struct v4l2_ctrl_hevc_sps ctrl_sps;
    unsigned int i;

    // Check for var slice array
    struct v4l2_query_ext_ctrl qc[] = {
        { .id = V4L2_CID_STATELESS_HEVC_SLICE_PARAMS },
        { .id = V4L2_CID_STATELESS_HEVC_SPS },
        { .id = V4L2_CID_STATELESS_HEVC_PPS },
        { .id = V4L2_CID_STATELESS_HEVC_SCALING_MATRIX },
#if HEVC_CTRLS_VERSION >= 2
        { .id = V4L2_CID_STATELESS_HEVC_DECODE_PARAMS },
#endif
    };
    // Order & size must match!
    static const size_t ctrl_sizes[] = {
        sizeof(struct v4l2_ctrl_hevc_slice_params),
        sizeof(struct v4l2_ctrl_hevc_sps),
        sizeof(struct v4l2_ctrl_hevc_pps),
        sizeof(struct v4l2_ctrl_hevc_scaling_matrix),
#if HEVC_CTRLS_VERSION >= 2
        sizeof(struct v4l2_ctrl_hevc_decode_params),
#endif
    };
    const unsigned int noof_ctrls = FF_ARRAY_ELEMS(qc);

#if HEVC_CTRLS_VERSION == 2
    if (mediabufs_ctl_driver_version(ctx->mbufs) >= MEDIABUFS_DRIVER_VERSION(5, 18, 0))
        return AVERROR(EINVAL);
#elif HEVC_CTRLS_VERSION == 3
    if (mediabufs_ctl_driver_version(ctx->mbufs) < MEDIABUFS_DRIVER_VERSION(5, 18, 0))
        return AVERROR(EINVAL);
#endif

    if (mediabufs_ctl_query_ext_ctrls(ctx->mbufs, qc, noof_ctrls)) {
        av_log(avctx, AV_LOG_DEBUG, "Probed V%d control missing\n", HEVC_CTRLS_VERSION);
        return AVERROR(EINVAL);
    }
    for (i = 0; i != noof_ctrls; ++i) {
        if (ctrl_sizes[i] != (size_t)qc[i].elem_size) {
            av_log(avctx, AV_LOG_DEBUG, "Probed V%d control %d size mismatch %zu != %zu\n",
                   HEVC_CTRLS_VERSION, i, ctrl_sizes[i], (size_t)qc[i].elem_size);
            return AVERROR(EINVAL);
        }
    }

    fill_sps(&ctrl_sps, sps);

    if (mediabufs_set_ext_ctrl(ctx->mbufs, NULL, V4L2_CID_STATELESS_HEVC_SPS, &ctrl_sps, sizeof(ctrl_sps))) {
        av_log(avctx, AV_LOG_ERROR, "Failed to set initial SPS\n");
        return AVERROR(EINVAL);
    }

    return 0;
}

// Final init
static int
set_controls(AVCodecContext * const avctx, V4L2RequestContextHEVC * const ctx)
{
    int ret;

    struct v4l2_query_ext_ctrl querys[] = {
        { .id = V4L2_CID_STATELESS_HEVC_DECODE_MODE, },
        { .id = V4L2_CID_STATELESS_HEVC_START_CODE, },
        { .id = V4L2_CID_STATELESS_HEVC_SLICE_PARAMS, },
#if HEVC_CTRLS_VERSION >= 4
        { .id = V4L2_CID_STATELESS_HEVC_ENTRY_POINT_OFFSETS, },
#endif
    };

    struct v4l2_ext_control ctrls[] = {
        { .id = V4L2_CID_STATELESS_HEVC_DECODE_MODE, },
        { .id = V4L2_CID_STATELESS_HEVC_START_CODE, },
    };

    mediabufs_ctl_query_ext_ctrls(ctx->mbufs, querys, FF_ARRAY_ELEMS(querys));

    ctx->decode_mode = querys[0].default_value;

    if (ctx->decode_mode != V4L2_STATELESS_HEVC_DECODE_MODE_SLICE_BASED &&
        ctx->decode_mode != V4L2_STATELESS_HEVC_DECODE_MODE_FRAME_BASED) {
        av_log(avctx, AV_LOG_ERROR, "%s: unsupported decode mode, %d\n", __func__, ctx->decode_mode);
        return AVERROR(EINVAL);
    }

    ctx->start_code = querys[1].default_value;
    if (ctx->start_code != V4L2_STATELESS_HEVC_START_CODE_NONE &&
        ctx->start_code != V4L2_STATELESS_HEVC_START_CODE_ANNEX_B) {
        av_log(avctx, AV_LOG_ERROR, "%s: unsupported start code, %d\n", __func__, ctx->start_code);
        return AVERROR(EINVAL);
    }

    ctx->max_slices = (!(querys[2].flags & V4L2_CTRL_FLAG_DYNAMIC_ARRAY) ||
                       querys[2].nr_of_dims != 1 || querys[2].dims[0] == 0) ?
        1 : querys[2].dims[0];
    av_log(avctx, AV_LOG_DEBUG, "%s: Max slices %d\n", __func__, ctx->max_slices);

#if HEVC_CTRLS_VERSION >= 4
    ctx->max_offsets = (querys[3].type == 0 || querys[3].nr_of_dims != 1) ?
        0 : querys[3].dims[0];
    av_log(avctx, AV_LOG_INFO, "%s: Entry point offsets %d\n", __func__, ctx->max_offsets);
#else
    ctx->max_offsets = 0;
#endif

    ctrls[0].value = ctx->decode_mode;
    ctrls[1].value = ctx->start_code;

    ret = mediabufs_ctl_set_ext_ctrls(ctx->mbufs, NULL, ctrls, FF_ARRAY_ELEMS(ctrls));
    return !ret ? 0 : AVERROR(-ret);
}

static void v4l2_req_frame_free(void *opaque, uint8_t *data)
{
    AVCodecContext *avctx = opaque;
    V4L2MediaReqDescriptor * const rd = (V4L2MediaReqDescriptor*)data;

    av_log(NULL, AV_LOG_DEBUG, "%s: avctx=%p data=%p\n", __func__, avctx, data);

    qent_dst_unref(&rd->qe_dst);

    // We don't expect req or qe_src to be set
    if (rd->req || rd->qe_src)
        av_log(NULL, AV_LOG_ERROR, "%s: qe_src %p or req %p not NULL\n", __func__, rd->req, rd->qe_src);

    av_freep(&rd->slices);
    av_freep(&rd->slice_params);
    av_freep(&rd->offsets);

    av_free(rd);
}

static AVBufferRef *v4l2_req_frame_alloc(void *opaque, int size)
{
    AVCodecContext *avctx = opaque;
//    V4L2RequestContextHEVC *ctx = avctx->internal->hwaccel_priv_data;
//    V4L2MediaReqDescriptor *req;
    AVBufferRef *ref;
    uint8_t *data;
//    int ret;

    data = av_mallocz(size);
    if (!data)
        return NULL;

    av_log(avctx, AV_LOG_DEBUG, "%s: avctx=%p size=%d data=%p\n", __func__, avctx, size, data);
    ref = av_buffer_create(data, size, v4l2_req_frame_free, avctx, 0);
    if (!ref) {
        av_freep(&data);
        return NULL;
    }
    return ref;
}

#if 0
static void v4l2_req_pool_free(void *opaque)
{
    av_log(NULL, AV_LOG_DEBUG, "%s: opaque=%p\n", __func__, opaque);
}

static void v4l2_req_hwframe_ctx_free(AVHWFramesContext *hwfc)
{
    av_log(NULL, AV_LOG_DEBUG, "%s: hwfc=%p pool=%p\n", __func__, hwfc, hwfc->pool);

    av_buffer_pool_uninit(&hwfc->pool);
}
#endif

static int frame_params(AVCodecContext *avctx, AVBufferRef *hw_frames_ctx)
{
    V4L2RequestContextHEVC *ctx = avctx->internal->hwaccel_priv_data;
    AVHWFramesContext *hwfc = (AVHWFramesContext*)hw_frames_ctx->data;
    const struct v4l2_format *vfmt = mediabufs_dst_fmt(ctx->mbufs);

    hwfc->format = AV_PIX_FMT_DRM_PRIME;
    hwfc->sw_format = pixel_format_from_format(vfmt);
    if (V4L2_TYPE_IS_MULTIPLANAR(vfmt->type)) {
        hwfc->width = vfmt->fmt.pix_mp.width;
        hwfc->height = vfmt->fmt.pix_mp.height;
    } else {
        hwfc->width = vfmt->fmt.pix.width;
        hwfc->height = vfmt->fmt.pix.height;
    }
#if 0
    hwfc->pool = av_buffer_pool_init2(sizeof(V4L2MediaReqDescriptor), avctx, v4l2_req_frame_alloc, v4l2_req_pool_free);
    if (!hwfc->pool)
        return AVERROR(ENOMEM);

    hwfc->free = v4l2_req_hwframe_ctx_free;

    hwfc->initial_pool_size = 1;

    switch (avctx->codec_id) {
    case AV_CODEC_ID_VP9:
        hwfc->initial_pool_size += 8;
        break;
    case AV_CODEC_ID_VP8:
        hwfc->initial_pool_size += 3;
        break;
    default:
        hwfc->initial_pool_size += 2;
    }
#endif
    av_log(avctx, AV_LOG_DEBUG, "%s: avctx=%p ctx=%p hw_frames_ctx=%p hwfc=%p pool=%p width=%d height=%d initial_pool_size=%d\n", __func__, avctx, ctx, hw_frames_ctx, hwfc, hwfc->pool, hwfc->width, hwfc->height, hwfc->initial_pool_size);

    return 0;
}

static int alloc_frame(AVCodecContext * avctx, AVFrame *frame)
{
    int rv;

    frame->buf[0] = v4l2_req_frame_alloc(avctx, sizeof(V4L2MediaReqDescriptor));
    if (!frame->buf[0])
        return AVERROR(ENOMEM);

    frame->data[0] = frame->buf[0]->data;

    frame->hw_frames_ctx = av_buffer_ref(avctx->hw_frames_ctx);

    if ((rv = ff_attach_decode_data(frame)) != 0) {
        av_log(avctx, AV_LOG_ERROR, "Failed to attach decode data to frame\n");
        av_frame_unref(frame);
        return rv;
    }

    return 0;
}

const v4l2_req_decode_fns V(ff_v4l2_req_hevc) = {
    .src_pix_fmt_v4l2 = V4L2_PIX_FMT_HEVC_SLICE,
    .name = "V4L2 HEVC stateless V" STR(HEVC_CTRLS_VERSION),
    .probe = probe,
    .set_controls = set_controls,

    .start_frame    = v4l2_request_hevc_start_frame,
    .decode_slice   = v4l2_request_hevc_decode_slice,
    .end_frame      = v4l2_request_hevc_end_frame,
    .abort_frame    = v4l2_request_hevc_abort_frame,
    .frame_params   = frame_params,
    .alloc_frame    = alloc_frame,
};

