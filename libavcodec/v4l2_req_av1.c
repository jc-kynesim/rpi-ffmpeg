#include "config.h"
#include "av1dec.h"
#include "decode.h"
#include "hwaccel_internal.h"
#include "hwconfig.h"
#include "internal.h"
#include "thread.h"

#include "v4l2_fmt.h"
#include "v4l2_req_dmabufs.h"
#include "v4l2_req_media.h"

#include <linux/v4l2-controls.h>

#include "v4l2_request_hevc.h"
#include "libavutil/avassert.h"
#include "libavutil/mem.h"
#include "libavutil/pixdesc.h"
#include "libavutil/fourcc_drm.h"
#include "libavutil/fourcc_v4l2.h"
#include "libavutil/hwcontext_drm.h"

// From AV1 spec but not in ffmpeg header
#define RESTORATION_TILESIZE_MAX 256

#define AV1_MC_IDENTITY 0

#ifndef V4L2_AV1_SEQUENCE_FLAG_MC_IDENTITY
#define V4L2_AV1_SEQUENCE_FLAG_MC_IDENTITY		  0x00100000
#endif

// Attached to buf[0] in frame
// Pooled in hwcontext so generally create once - 1/frame
typedef struct req_av1_frame_env_s {
    AVDRMFrameDescriptor drm;

    // Media
    uint64_t timestamp;
    struct qent_dst * qe_dst;

    // Refs to source frames
    AVBufferRef * refs[18]; // 16 + 1 + 1

    // Decode only - should be NULL by the time we emit the frame
    struct req_decode_ent decode_ent;

    struct media_request *req;
    struct qent_src *qe_src;

    const uint8_t * buffer0;
    size_t num_tiles;
    struct v4l2_ctrl_av1_tile_group_entry * tile_groups;

} req_av1_frame_env_t;

// Per decode info  not common to other decoders
typedef struct req_av1_dec_env_s {
    bool seq_set;
    struct v4l2_ctrl_av1_sequence last_seq;
} req_av1_dec_env_t;


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

static inline req_av1_frame_env_t *
av1frame_rd(const AV1Frame * const av1f)
{
    return av1f->f == NULL ? NULL : (req_av1_frame_env_t *)av1f->f->data[0];
}

static inline req_av1_frame_env_t *
av1ctx_rd(const AV1DecContext * const h)
{
    return av1frame_rd(&h->cur_frame);
}

static int
get_bit_depth_from_seq(const AV1RawSequenceHeader *seq)
{
    if (seq->seq_profile == 2 && seq->color_config.high_bitdepth)
        return seq->color_config.twelve_bit ? 12 : 10;
    else if (seq->seq_profile <= 2 && seq->color_config.high_bitdepth)
        return 10;
    else
        return 8;
}

// Object fd & size will be zapped by this & need setting later
// **** Export & common this
static int drm_from_format(AVDRMFrameDescriptor * const desc, const struct v4l2_format * const format)
{
    AVDRMLayerDescriptor *layer = &desc->layers[0];
    unsigned int width;
    unsigned int height;
    unsigned int bpl;
    uint32_t pixelformat;
    unsigned int planes = 2;
    unsigned int cmulx2 = 2;
    uint64_t mod = DRM_FORMAT_MOD_LINEAR;

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
    case V4L2_PIX_FMT_GREY:
        layer->format = DRM_FORMAT_R8;
        planes = 1;
        break;
    case V4L2_PIX_FMT_Y10:
        layer->format = DRM_FORMAT_R10;
        planes = 1;
        break;
    case V4L2_PIX_FMT_Y12:
        layer->format = DRM_FORMAT_R12;
        planes = 1;
        break;
    case V4L2_PIX_FMT_Y16:
        layer->format = DRM_FORMAT_R16;
        planes = 1;
        break;
    case V4L2_PIX_FMT_NV12:
        layer->format = DRM_FORMAT_NV12;
        break;
    case V4L2_PIX_FMT_NV16:
        layer->format = DRM_FORMAT_NV16;
        break;
    case V4L2_PIX_FMT_NV24:
        layer->format = DRM_FORMAT_NV24;
        cmulx2 = 4;
        break;
    case V4L2_PIX_FMT_P010:
        layer->format = DRM_FORMAT_P010;
        break;
    case V4L2_PIX_FMT_P012:
        layer->format = DRM_FORMAT_P012;
        break;
    case V4L2_PIX_FMT_P210:
        layer->format = DRM_FORMAT_P210;
        break;
    case V4L2_PIX_FMT_P212:
        layer->format = DRM_FORMAT_P212;
        break;
    case V4L2_PIX_FMT_P410:
        layer->format = DRM_FORMAT_P410;
        cmulx2 = 4;
        break;
    case V4L2_PIX_FMT_P412:
        layer->format = DRM_FORMAT_P412;
        cmulx2 = 4;
        break;
#if CONFIG_SAND
    case V4L2_PIX_FMT_NV12_COL128:
        layer->format = DRM_FORMAT_NV12;
        mod = DRM_FORMAT_MOD_BROADCOM_SAND128_COL_HEIGHT(bpl);
        break;
    case V4L2_PIX_FMT_NV12_10_COL128:
        layer->format = DRM_FORMAT_P030;
        mod = DRM_FORMAT_MOD_BROADCOM_SAND128_COL_HEIGHT(bpl);
        break;
#endif
#ifdef DRM_FORMAT_MOD_ALLWINNER_TILED
    case V4L2_PIX_FMT_SUNXI_TILED_NV12:
        layer->format = DRM_FORMAT_NV12;
        mod = DRM_FORMAT_MOD_ALLWINNER_TILED;
        break;
#endif
#if defined(V4L2_PIX_FMT_NV15) && defined(DRM_FORMAT_NV15)
    case V4L2_PIX_FMT_NV15:
        layer->format = DRM_FORMAT_NV15;
        break;
#endif
#if defined(V4L2_PIX_FMT_NV20) && defined(DRM_FORMAT_NV20)
    case V4L2_PIX_FMT_NV20:
        layer->format = DRM_FORMAT_NV20;
        break;
#endif
    default:
        return -1;
    }

    desc->nb_objects = 1;
    desc->objects[0].fd = -1;
    desc->objects[0].size = 0;
    desc->objects[0].format_modifier = mod;

    desc->nb_layers = 1;
    layer->nb_planes = planes;

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
    if (planes == 2) {
        layer->planes[1].object_index = 0;
        layer->planes[1].offset = layer->planes[0].pitch * height;
        layer->planes[1].pitch = layer->planes[0].pitch * cmulx2 / 2;
    }

    return 0;
}

// N.B. Does not fill reference_frame_ts as that is outside the bitstream
static void
fill_frame(struct v4l2_ctrl_av1_frame * const vframe,
           const AV1RawSequenceHeader *const seq,
           const AV1RawFrameHeader * const fh,
           const AV1Frame * const cur_frame,
           const AV1Frame * const refs)
{
    const unsigned int superres_denom = fh->use_superres ?
        fh->coded_denom  + AV1_SUPERRES_DENOM_MIN :
        AV1_SUPERRES_NUM;

    memset(vframe, 0, sizeof(*vframe));

//    struct v4l2_av1_tile_info
    {
        struct v4l2_av1_tile_info * const vti = &vframe->tile_info;
        const unsigned int sbShift = seq->use_128x128_superblock ? 5 : 4;
        const unsigned int frame_width = (cur_frame->f->width * AV1_SUPERRES_NUM + superres_denom / 2) / superres_denom;
        const unsigned int frame_height = cur_frame->f->height;
        const unsigned int MiCols = 2 * ((frame_width + 7) >> 3);
        const unsigned int MiRows = 2 * ((frame_height + 7) >> 3);
        unsigned int i;
        unsigned int n;

        vti->flags = 0;
        if (fh->uniform_tile_spacing_flag)
            vti->flags |= V4L2_AV1_TILE_INFO_FLAG_UNIFORM_TILE_SPACING;
        vti->context_update_tile_id = fh->context_update_tile_id;  // ?? uint8/16
        vti->tile_cols = fh->tile_cols;
        vti->tile_rows = fh->tile_rows;
        n = 0;
        for (i = 0; i != vti->tile_cols; ++i) {
            vti->width_in_sbs_minus_1[i] = fh->width_in_sbs_minus_1[i];
            vti->mi_col_starts[i] = n;
            n += (fh->width_in_sbs_minus_1[i] + 1) << sbShift;
        }
        vti->mi_col_starts[i] = MiCols;

        n = 0;
        for (i = 0; i != vti->tile_rows; ++i) {
            vti->height_in_sbs_minus_1[i] = fh->height_in_sbs_minus_1[i];
            vti->mi_row_starts[i] = n;
            n += (fh->height_in_sbs_minus_1[i] + 1) << sbShift;
        }
        vti->mi_row_starts[i] = MiRows;
    }

//  struct v4l2_av1_quantization quantization;
    {
        struct v4l2_av1_quantization *const q = &vframe->quantization;

        q->flags = 0;
        if (fh->diff_uv_delta)
            q->flags |= V4L2_AV1_QUANTIZATION_FLAG_DIFF_UV_DELTA;
        if (fh->using_qmatrix)
            q->flags |= V4L2_AV1_QUANTIZATION_FLAG_USING_QMATRIX;
        if (fh->delta_q_present)
            q->flags |= V4L2_AV1_QUANTIZATION_FLAG_DELTA_Q_PRESENT;
        q->base_q_idx = fh->base_q_idx;
        q->delta_q_y_dc = fh->delta_q_y_dc;
        q->delta_q_u_dc = fh->delta_q_u_dc;
        q->delta_q_u_ac = fh->delta_q_u_ac;
        q->delta_q_v_dc = fh->delta_q_v_dc;
        q->delta_q_v_ac = fh->delta_q_v_ac;
        q->qm_y = fh->qm_y;
        q->qm_u = fh->qm_u;
        q->qm_v = fh->qm_v;
        q->delta_q_res = fh->delta_q_res;
    }

    vframe->superres_denom = superres_denom;

//  struct v4l2_av1_segmentation segmentation;
    {
        struct v4l2_av1_segmentation *const ss = &vframe->segmentation;
        unsigned int i, j;
        bool SegIdPreSkip = false;
        unsigned int LastActiveSegId = 0;

        // AV1 5.9.14
        // FFmpeg structure holds unclipped values
        static const uint8_t Segmentation_Feature_Max[AV1_SEG_LVL_MAX] = {
            255, AV1_MAX_LOOP_FILTER, AV1_MAX_LOOP_FILTER,
            AV1_MAX_LOOP_FILTER, AV1_MAX_LOOP_FILTER, 7,
            0, 0 };
        static const char Segmentation_Feature_Signed[AV1_SEG_LVL_MAX] = { 1, 1, 1, 1, 1, 0, 0, 0 };

        // Everythgng zeroed at start of fn. so no need to zap unused
        for (i = 0; i < AV1_MAX_SEGMENTS; i++) {
            unsigned int feature_enabled = 0;
            for (j = 0; j < AV1_SEG_LVL_MAX; j++) {
                if (fh->feature_enabled[i][j]) {
                    const int max_val = Segmentation_Feature_Max[j];
                    const int min_val = Segmentation_Feature_Signed[j] ? -max_val : 0;
                    ss->feature_data[i][j] = av_clip(fh->feature_value[i][j], min_val, max_val);
                    feature_enabled |= V4L2_AV1_SEGMENT_FEATURE_ENABLED(j);
                    LastActiveSegId = i;
                    if (j >= AV1_SEG_LVL_REF_FRAME)
                        SegIdPreSkip = true;
                }
            }
            ss->feature_enabled[i] = feature_enabled;
        }

        ss->flags = 0;
        if (fh->segmentation_enabled)
            ss->flags |= V4L2_AV1_SEGMENTATION_FLAG_ENABLED;
        if (fh->segmentation_update_map)
            ss->flags |= V4L2_AV1_SEGMENTATION_FLAG_UPDATE_MAP;
        if (fh->segmentation_temporal_update)
            ss->flags |= V4L2_AV1_SEGMENTATION_FLAG_TEMPORAL_UPDATE;
        if (fh->segmentation_update_data)
            ss->flags |= V4L2_AV1_SEGMENTATION_FLAG_UPDATE_DATA;
        if (SegIdPreSkip)
            ss->flags |= V4L2_AV1_SEGMENTATION_FLAG_SEG_ID_PRE_SKIP;

        ss->last_active_seg_id = LastActiveSegId;
    }

//  struct v4l2_av1_loop_filter loop_filter;
    {
        struct v4l2_av1_loop_filter *const lf = &vframe->loop_filter;
        unsigned int i;

        lf->flags = 0;
        if (fh->loop_filter_delta_enabled)
            lf->flags |= V4L2_AV1_LOOP_FILTER_FLAG_DELTA_ENABLED;
        if (fh->loop_filter_delta_update)
            lf->flags |= V4L2_AV1_LOOP_FILTER_FLAG_DELTA_UPDATE;
        if (fh->delta_lf_present)
            lf->flags |= V4L2_AV1_LOOP_FILTER_FLAG_DELTA_LF_PRESENT;
        if (fh->delta_lf_multi)
            lf->flags |= V4L2_AV1_LOOP_FILTER_FLAG_DELTA_LF_MULTI;

        for (i = 0; i != 4; ++i)
            lf->level[i] = fh->loop_filter_level[i];
        lf->sharpness = fh->loop_filter_sharpness;
        for (i = 0; i != V4L2_AV1_TOTAL_REFS_PER_FRAME; ++i)
            lf->ref_deltas[i] = fh->loop_filter_ref_deltas[i];
        for (i = 0; i != 2; ++i)
            lf->mode_deltas[i] = fh->loop_filter_mode_deltas[i];
        lf->delta_lf_res = fh->delta_lf_res;
    }

//  struct v4l2_av1_cdef cdef;
    {
        struct v4l2_av1_cdef * const cd = &vframe->cdef;
        unsigned int i;

        cd->damping_minus_3 = fh->cdef_damping_minus_3;
        cd->bits = fh->cdef_bits;
        for (i = 0; i != V4L2_AV1_CDEF_MAX; ++i)
            cd->y_pri_strength[i] = fh->cdef_y_pri_strength[i];
        for (i = 0; i != V4L2_AV1_CDEF_MAX; ++i)
            cd->y_sec_strength[i] = fh->cdef_y_sec_strength[i];
        for (i = 0; i != V4L2_AV1_CDEF_MAX; ++i)
            cd->uv_pri_strength[i] = fh->cdef_uv_pri_strength[i];
        for (i = 0; i != V4L2_AV1_CDEF_MAX; ++i)
            cd->uv_sec_strength[i] = fh->cdef_uv_sec_strength[i];
    }

    vframe->skip_mode_frame[0] = cur_frame->skip_mode_frame_idx[0];
    vframe->skip_mode_frame[1] = cur_frame->skip_mode_frame_idx[1];
    vframe->primary_ref_frame = fh->primary_ref_frame;

//  struct v4l2_av1_loop_restoration loop_restoration;
    {
        static const enum v4l2_av1_frame_restoration_type Remap_Lr_Type[4] = {
            V4L2_AV1_FRAME_RESTORE_NONE,
            V4L2_AV1_FRAME_RESTORE_SWITCHABLE,
            V4L2_AV1_FRAME_RESTORE_WIENER,
            V4L2_AV1_FRAME_RESTORE_SGRPROJ};
        struct v4l2_av1_loop_restoration * const lr = &vframe->loop_restoration;
        unsigned int i;
        bool UsesLr = false;
        bool usesChromaLr = false;

        for (i = 0; i != V4L2_AV1_NUM_PLANES_MAX; ++i) {
            lr->frame_restoration_type[i] = Remap_Lr_Type[fh->lr_type[i]];
            if (fh->lr_type[i] != AV1_RESTORE_NONE) {
                UsesLr = true;
                if (i != 0)
                    usesChromaLr = true;
            }
        }

        lr->flags = 0;
        if (UsesLr)
            lr->flags |= V4L2_AV1_LOOP_RESTORATION_FLAG_USES_LR;
        if (usesChromaLr)
            lr->flags |= V4L2_AV1_LOOP_RESTORATION_FLAG_USES_CHROMA_LR;

        lr->lr_unit_shift = fh->lr_unit_shift;
        lr->lr_uv_shift = fh->lr_uv_shift;
        lr->loop_restoration_size[0] = RESTORATION_TILESIZE_MAX >> (2 - lr->lr_unit_shift);
        lr->loop_restoration_size[1] = lr->loop_restoration_size[0] >> lr->lr_uv_shift;
        lr->loop_restoration_size[2] = lr->loop_restoration_size[0] >> lr->lr_uv_shift;
    }

//  struct v4l2_av1_global_motion global_motion;
    // *** gm params are currently only partially parsed by the bitstream reader
    {
        struct v4l2_av1_global_motion * const gm = &vframe->global_motion;
        unsigned int i, j;

        av_assert0(V4L2_AV1_TOTAL_REFS_PER_FRAME == AV1_NUM_REF_FRAMES);

        for (i = 1; i != AV1_NUM_REF_FRAMES; i++) {
            gm->flags[i] = 0;
            if (fh->is_global[i])
                gm->flags[i] |= V4L2_AV1_GLOBAL_MOTION_FLAG_IS_GLOBAL;
            if (fh->is_rot_zoom[i])
                gm->flags[i] |= V4L2_AV1_GLOBAL_MOTION_FLAG_IS_ROT_ZOOM;
            if (fh->is_translation[i])
                gm->flags[i] |= V4L2_AV1_GLOBAL_MOTION_FLAG_IS_TRANSLATION;
        }

        for (i = 1; i != AV1_NUM_REF_FRAMES; i++)
            gm->type[i] = cur_frame->gm_type[i];
        for (i = 1; i != AV1_NUM_REF_FRAMES; i++)
            for (j = 0; j < 6; ++j)
                gm->params[i][j] = cur_frame->gm_params[i][j];
        gm->invalid = 0;
        for (i = 1; i != AV1_NUM_REF_FRAMES; i++)
            if (cur_frame->gm_invalid[i])
                gm->invalid |= V4L2_AV1_GLOBAL_MOTION_IS_INVALID(i);
    }

    vframe->flags = 0;
    if (fh->show_frame)
        vframe->flags |= V4L2_AV1_FRAME_FLAG_SHOW_FRAME;
    if (fh->showable_frame)
        vframe->flags |= V4L2_AV1_FRAME_FLAG_SHOWABLE_FRAME;
    if (fh->error_resilient_mode)
        vframe->flags |= V4L2_AV1_FRAME_FLAG_ERROR_RESILIENT_MODE;
    if (fh->disable_cdf_update)
        vframe->flags |= V4L2_AV1_FRAME_FLAG_DISABLE_CDF_UPDATE;
    if (fh->allow_screen_content_tools)
        vframe->flags |= V4L2_AV1_FRAME_FLAG_ALLOW_SCREEN_CONTENT_TOOLS;
    if (fh->force_integer_mv)
        vframe->flags |= V4L2_AV1_FRAME_FLAG_FORCE_INTEGER_MV;
    if (fh->allow_intrabc)
        vframe->flags |= V4L2_AV1_FRAME_FLAG_ALLOW_INTRABC;
    if (fh->use_superres)
        vframe->flags |= V4L2_AV1_FRAME_FLAG_USE_SUPERRES;
    if (fh->allow_high_precision_mv)
        vframe->flags |= V4L2_AV1_FRAME_FLAG_ALLOW_HIGH_PRECISION_MV;
    if (fh->is_motion_mode_switchable)
        vframe->flags |= V4L2_AV1_FRAME_FLAG_IS_MOTION_MODE_SWITCHABLE;
    if (fh->use_ref_frame_mvs)
        vframe->flags |= V4L2_AV1_FRAME_FLAG_USE_REF_FRAME_MVS;
    if (fh->disable_frame_end_update_cdf)
        vframe->flags |= V4L2_AV1_FRAME_FLAG_DISABLE_FRAME_END_UPDATE_CDF;
    if (fh->allow_warped_motion)
        vframe->flags |= V4L2_AV1_FRAME_FLAG_ALLOW_WARPED_MOTION;
    if (fh->reference_select)
        vframe->flags |= V4L2_AV1_FRAME_FLAG_REFERENCE_SELECT;
    if (fh->reduced_tx_set)
        vframe->flags |= V4L2_AV1_FRAME_FLAG_REDUCED_TX_SET;
    // V4L2_AV1_FRAME_FLAG_SKIP_MODE_ALLOWED not recorded in fh and is not
    // used by any current decode so just set to skip_mode_present so
    // as not to run afoul of consistency checkers
    if (fh->skip_mode_present) {
        vframe->flags |= V4L2_AV1_FRAME_FLAG_SKIP_MODE_ALLOWED;
        vframe->flags |= V4L2_AV1_FRAME_FLAG_SKIP_MODE_PRESENT;
    }
    if (fh->frame_size_override_flag)
        vframe->flags |= V4L2_AV1_FRAME_FLAG_FRAME_SIZE_OVERRIDE;
    if (fh->buffer_removal_time_present_flag)
        vframe->flags |= V4L2_AV1_FRAME_FLAG_BUFFER_REMOVAL_TIME_PRESENT;
    if (fh->frame_refs_short_signaling)
        vframe->flags |= V4L2_AV1_FRAME_FLAG_FRAME_REFS_SHORT_SIGNALING;

    vframe->frame_type = fh->frame_type;
    vframe->order_hint = fh->order_hint;
    vframe->upscaled_width = cur_frame->f->width;
    vframe->interpolation_filter = fh->interpolation_filter;
    vframe->tx_mode = fh->tx_mode;

    vframe->frame_width_minus_1 = fh->frame_width_minus_1;
    vframe->frame_height_minus_1 = fh->frame_height_minus_1;
    vframe->render_width_minus_1 = fh->render_width_minus_1;
    vframe->render_height_minus_1 = fh->render_height_minus_1;

    vframe->current_frame_id = fh->current_frame_id;

    for (unsigned int i = 0; i != AV1_MAX_OPERATING_POINTS; ++i)
        vframe->buffer_removal_time[i] = fh->buffer_removal_time[i];

//  __u64 reference_frame_ts[V4L2_AV1_TOTAL_REFS_PER_FRAME];

    {
        const bool FrameIsIntra = vframe->frame_type == V4L2_AV1_INTRA_ONLY_FRAME || vframe->frame_type == V4L2_AV1_KEY_FRAME;
        unsigned int i;

        vframe->order_hints[0] = 0;
        for (i = 0; i != V4L2_AV1_REFS_PER_FRAME; ++i) {
            const unsigned int idx = fh->ref_frame_idx[i];
            vframe->ref_frame_idx[i] = idx;
            vframe->order_hints[i + 1] = FrameIsIntra ? 0 : refs[idx].raw_frame_header->order_hint;
        }
    }

    vframe->refresh_frame_flags = fh->refresh_frame_flags;
}

static void
fill_sequence(struct v4l2_ctrl_av1_sequence * const vseq,
              const AV1RawSequenceHeader *const seq)
{
    const AV1RawColorConfig *const rcc = &seq->color_config;

    memset(vseq, 0, sizeof(*vseq));

    vseq->seq_profile              = seq->seq_profile;
    vseq->order_hint_bits          = seq->order_hint_bits_minus_1 + 1;
    vseq->bit_depth                = get_bit_depth_from_seq(seq);
    vseq->max_frame_width_minus_1  = seq->max_frame_width_minus_1;
    vseq->max_frame_height_minus_1 = seq->max_frame_height_minus_1;

    vseq->flags = 0;
    if (seq->use_128x128_superblock)
        vseq->flags |= V4L2_AV1_SEQUENCE_FLAG_USE_128X128_SUPERBLOCK;
    if (seq->still_picture)
        vseq->flags |= V4L2_AV1_SEQUENCE_FLAG_STILL_PICTURE;
    if (seq->enable_filter_intra)
        vseq->flags |= V4L2_AV1_SEQUENCE_FLAG_ENABLE_FILTER_INTRA;
    if (seq->enable_intra_edge_filter)
        vseq->flags |= V4L2_AV1_SEQUENCE_FLAG_ENABLE_INTRA_EDGE_FILTER;
    if (seq->enable_interintra_compound)
        vseq->flags |= V4L2_AV1_SEQUENCE_FLAG_ENABLE_INTERINTRA_COMPOUND;
    if (seq->enable_masked_compound)
        vseq->flags |= V4L2_AV1_SEQUENCE_FLAG_ENABLE_MASKED_COMPOUND;
    if (seq->enable_warped_motion)
        vseq->flags |= V4L2_AV1_SEQUENCE_FLAG_ENABLE_WARPED_MOTION;
    if (seq->enable_dual_filter)
        vseq->flags |= V4L2_AV1_SEQUENCE_FLAG_ENABLE_DUAL_FILTER;
    if (seq->enable_order_hint)
        vseq->flags |= V4L2_AV1_SEQUENCE_FLAG_ENABLE_ORDER_HINT;
    if (seq->enable_jnt_comp)
        vseq->flags |= V4L2_AV1_SEQUENCE_FLAG_ENABLE_JNT_COMP;
    if (seq->enable_ref_frame_mvs)
        vseq->flags |= V4L2_AV1_SEQUENCE_FLAG_ENABLE_REF_FRAME_MVS;
    if (seq->enable_superres)
        vseq->flags |= V4L2_AV1_SEQUENCE_FLAG_ENABLE_SUPERRES;
    if (seq->enable_cdef)
        vseq->flags |= V4L2_AV1_SEQUENCE_FLAG_ENABLE_CDEF;
    if (seq->enable_restoration)
        vseq->flags |= V4L2_AV1_SEQUENCE_FLAG_ENABLE_RESTORATION;
    if (rcc->mono_chrome)
        vseq->flags |= V4L2_AV1_SEQUENCE_FLAG_MONO_CHROME;
    if (rcc->color_range)
        vseq->flags |= V4L2_AV1_SEQUENCE_FLAG_COLOR_RANGE;
    if (rcc->subsampling_x)
        vseq->flags |= V4L2_AV1_SEQUENCE_FLAG_SUBSAMPLING_X;
    if (rcc->subsampling_y)
        vseq->flags |= V4L2_AV1_SEQUENCE_FLAG_SUBSAMPLING_Y;
    if (seq->film_grain_params_present)
        vseq->flags |= V4L2_AV1_SEQUENCE_FLAG_FILM_GRAIN_PARAMS_PRESENT;
    if (rcc->separate_uv_delta_q)
        vseq->flags |= V4L2_AV1_SEQUENCE_FLAG_SEPARATE_UV_DELTA_Q;
    if (rcc->matrix_coefficients == AV1_MC_IDENTITY)
        vseq->flags |= V4L2_AV1_SEQUENCE_FLAG_MC_IDENTITY;
}

static void
fill_film_grain(struct v4l2_ctrl_av1_film_grain * const vgrain,
                const AV1RawFilmGrainParams * const fg)
{
    unsigned int i;

    memset(vgrain, 0, sizeof(*vgrain));

    vgrain->flags = 0;
    if (fg->apply_grain)
        vgrain->flags |= V4L2_AV1_FILM_GRAIN_FLAG_APPLY_GRAIN;
    if (fg->update_grain)
        vgrain->flags |= V4L2_AV1_FILM_GRAIN_FLAG_UPDATE_GRAIN;
    if (fg->chroma_scaling_from_luma)
        vgrain->flags |= V4L2_AV1_FILM_GRAIN_FLAG_CHROMA_SCALING_FROM_LUMA;
    if (fg->overlap_flag)
        vgrain->flags |= V4L2_AV1_FILM_GRAIN_FLAG_OVERLAP;
    if (fg->clip_to_restricted_range)
        vgrain->flags |= V4L2_AV1_FILM_GRAIN_FLAG_CLIP_TO_RESTRICTED_RANGE;

    vgrain->cr_mult = fg->cr_mult;
    vgrain->grain_seed = fg->grain_seed;
    vgrain->film_grain_params_ref_idx = fg->film_grain_params_ref_idx;
    // V4L2 & ffmpeg structs not same size
    vgrain->num_y_points = fg->num_y_points;
    for (i = 0; i != vgrain->num_y_points; ++i)
        vgrain->point_y_value[i] = fg->point_y_value[i];
    for (i = 0; i != vgrain->num_y_points; ++i)
        vgrain->point_y_scaling[i] = fg->point_y_scaling[i];
    vgrain->num_cb_points = fg->num_cb_points;
    // V4L2 & ffmpeg structs not same size
    for (i = 0; i != vgrain->num_cb_points; ++i)
        vgrain->point_cb_value[i] = fg->point_cb_value[i];
    for (i = 0; i != vgrain->num_cb_points; ++i)
        vgrain->point_cb_scaling[i] = fg->point_cb_scaling[i];
    // V4L2 & ffmpeg structs not same size
    vgrain->num_cr_points = fg->num_cr_points;
    for (i = 0; i != vgrain->num_cr_points; ++i)
        vgrain->point_cr_value[i] = fg->point_cr_value[i];
    for (i = 0; i != vgrain->num_cr_points; ++i)
        vgrain->point_cr_scaling[i] = fg->point_cr_scaling[i];
    vgrain->grain_scaling_minus_8 = fg->grain_scaling_minus_8;
    vgrain->ar_coeff_lag = fg->ar_coeff_lag;
    // V4L2 & ffmpeg structs not same size (Y only!)
    for (i = 0; i != 24; ++i)
        vgrain->ar_coeffs_y_plus_128[i] = fg->ar_coeffs_y_plus_128[i];
    for (i = 0; i != 25; ++i)
        vgrain->ar_coeffs_cb_plus_128[i] = fg->ar_coeffs_cb_plus_128[i];
    for (i = 0; i != 25; ++i)
        vgrain->ar_coeffs_cr_plus_128[i] = fg->ar_coeffs_cr_plus_128[i];
    vgrain->ar_coeff_shift_minus_6 = fg->ar_coeff_shift_minus_6;
    vgrain->grain_scale_shift = fg->grain_scale_shift;
    vgrain->cb_mult = fg->cb_mult;
    vgrain->cb_luma_mult = fg->cb_luma_mult;
    vgrain->cr_luma_mult = fg->cr_luma_mult;
    vgrain->cb_offset = fg->cb_offset;
    vgrain->cr_offset = fg->cr_offset;
}

static void
fill_frame_ts(struct v4l2_ctrl_av1_frame * const vframe, const AV1DecContext * const h)
{
    unsigned int i;

    for (i = 0; i != AV1_NUM_REF_FRAMES; ++i) {
        const req_av1_frame_env_t * const rd = av1frame_rd(h->ref + i);
        vframe->reference_frame_ts[i] = rd == NULL ? 0 : rd->timestamp;
    }
}

static int frame_finish(req_av1_frame_env_t *const rd)
{
    int rv = 0;

    if (rd->qe_dst) {
        MediaBufsStatus stat = qent_dst_wait(rd->qe_dst);
        if (stat != MEDIABUFS_STATUS_SUCCESS)
            rv = -1;
    }
    {
        AVBufferRef **p = rd->refs;
        for (; *p != NULL; ++p) {
            av_log(NULL, AV_LOG_TRACE, "%s: %"PRId64" unref %"PRId64" Refs: %d\n", __func__, rd->timestamp,
                   ((req_av1_frame_env_t *)((*p)->data))->timestamp, av_buffer_get_ref_count(*p) - 1);
            av_buffer_unref(p);
        }
    }

    return rv;
}

// Called before finally returning the frame to the user
// Set corrupt flag here as this is actually the frame structure that
// is going to the user (in MT land each thread has its own pool)
static int frame_post_process(void *logctx, AVFrame *frame)
{
    req_av1_frame_env_t *rd = (req_av1_frame_env_t *)frame->data[0];

    frame->flags &= ~AV_FRAME_FLAG_CORRUPT;
    av_log(logctx, AV_LOG_TRACE, "%s: TS %"PRId64" wait\n", __func__, rd->timestamp);
    if (frame_finish(rd) != 0) {
        av_log(logctx, AV_LOG_ERROR, "%s: Decode fail\n", __func__);
        frame->flags |= AV_FRAME_FLAG_CORRUPT;
    }
    return 0;
}

static int
req_av1_probe(AVCodecContext * const avctx, V4L2RequestContextHEVC * const ctx)
{
    const AV1DecContext * const s = avctx->priv_data;
    const AV1RawSequenceHeader * const seq = s->raw_seq;
    struct v4l2_ctrl_av1_sequence vseq;

    fill_sequence(&vseq, seq);

    if (mediabufs_set_ext_ctrl(ctx->mbufs, NULL, V4L2_CID_STATELESS_AV1_SEQUENCE, &vseq, sizeof(vseq))) {
        av_log(avctx, AV_LOG_ERROR, "Failed to set initial SPS\n");
        return AVERROR(EINVAL);
    }

    return 0;
}

static int
req_av1_set_controls(AVCodecContext * const avctx, V4L2RequestContextHEVC * const ctx)
{
    if ((ctx->decode_ctx = av_mallocz(sizeof(req_av1_dec_env_t))) == NULL)
        return AVERROR(ENOMEM);
    return 0;
}

static void
req_av1_uninit(V4L2RequestContextHEVC *const ctx)
{
    av_freep(&ctx->decode_ctx);
}

static int
req_av1_start_frame(AVCodecContext *avctx,
                                         V4L2RequestContextHEVC *const ctx,
                                         av_unused const uint8_t *buffer,
                                         av_unused uint32_t size)
{
    const AV1DecContext * const h = avctx->priv_data;
    req_av1_frame_env_t *const rd = av1ctx_rd(h);

    decode_q_add(&ctx->decode_q, &rd->decode_ent);

    av_assert0(rd->tile_groups == NULL);
    rd->num_tiles = 0;
    rd->buffer0 = NULL;
    ctx->timestamp++;
    rd->timestamp = cvt_timestamp_to_dpb(ctx->timestamp);
    av_log(avctx, AV_LOG_DEBUG, "%s: TS %"PRId64"\n", __func__, rd->timestamp);

    {
        FrameDecodeData * const fdd = (FrameDecodeData*)h->cur_frame.f->private_ref->data;
        fdd->post_process = frame_post_process;
    }

    ff_thread_finish_setup(avctx); // Allow next thread to enter rpi_hevc_start_frame

    return 0;
}

static int
req_av1_decode_slice(AVCodecContext *avctx, V4L2RequestContextHEVC *const ctx, const uint8_t *buffer, uint32_t size)
{
    const AV1DecContext * const h = avctx->priv_data;
    req_av1_frame_env_t *const rd = av1ctx_rd(h);

    if (rd->tile_groups == NULL) {
        rd->num_tiles = h->raw_frame_header->tile_cols * h->raw_frame_header->tile_rows;
        rd->tile_groups = calloc(rd->num_tiles, sizeof(*rd->tile_groups));
        if (rd->tile_groups == NULL) {
            av_log(avctx, AV_LOG_ERROR, "Failed to allocate %zd tile groups\n", rd->num_tiles);
            return AVERROR(ENOMEM);
        }
    }

    if (h->tg_end >= rd->num_tiles) {
        av_log(avctx, AV_LOG_ERROR, "Tile group end > total tile count\n");
        return AVERROR_INVALIDDATA;
    }

    if (rd->buffer0 == NULL)
        rd->buffer0 = buffer;

    // tg_end is number of final tile not the one after
    for (unsigned int i = h->tg_start; i <= h->tg_end; ++i) {
        struct v4l2_ctrl_av1_tile_group_entry *tg = rd->tile_groups + i;
        tg->tile_offset = h->tile_group_info[i].tile_offset + buffer - rd->buffer0;
        tg->tile_size   = h->tile_group_info[i].tile_size;
        tg->tile_row    = h->tile_group_info[i].tile_row;
        tg->tile_col    = h->tile_group_info[i].tile_column;
    }

    return 0;
}

static int
req_av1_end_frame(AVCodecContext *avctx, V4L2RequestContextHEVC *const ctx)
{
    const AV1DecContext * const h = avctx->priv_data;
    const AV1RawSequenceHeader * const seq = h->raw_seq;
    req_av1_dec_env_t *const de = ctx->decode_ctx;
    req_av1_frame_env_t *const rd = av1ctx_rd(h);
    struct media_request * mreq = NULL;
    struct qent_src * src = NULL;
    size_t buf_size;
    struct timeval tv;
    int rv;
    MediaBufsStatus stat;

    struct v4l2_ctrl_av1_sequence vseq;
    struct v4l2_ctrl_av1_frame vframe;
    struct v4l2_ctrl_av1_film_grain vgrain;

    struct v4l2_ext_control control[4] = {
        {
            .id = V4L2_CID_STATELESS_AV1_SEQUENCE,
            .ptr = &vseq,
            .size = sizeof(vseq),
        },
        {
            .id = V4L2_CID_STATELESS_AV1_FRAME,
            .ptr = &vframe,
            .size = sizeof(vframe),
        },
        {
            .id = V4L2_CID_STATELESS_AV1_FILM_GRAIN,
            .ptr = &vgrain,
            .size = sizeof(vgrain),
        },
        {
            .id = V4L2_CID_STATELESS_AV1_TILE_GROUP_ENTRY,
            .ptr = rd->tile_groups,
            .size = rd->num_tiles * sizeof(*rd->tile_groups),
        },
    };

    av_log(avctx, AV_LOG_DEBUG, "rpi end frame\n");

    // It is possible, though maybe a bug, to get an end_frame without
    // a previous start_frame.  If we do then give up.
    if (!decode_q_in_q(&rd->decode_ent)) {
        av_log(avctx, AV_LOG_DEBUG, "%s: Frame not in decode Q\n", __func__);
        return AVERROR_INVALIDDATA;
    }

    fill_sequence(&vseq, seq);

    if (!de->seq_set) {
        de->last_seq = vseq;
        de->seq_set = true;
    }
    else if (memcmp(&vseq, &de->last_seq, sizeof(vseq)) != 0) {
        av_log(avctx, AV_LOG_WARNING, "SEQ changed without restart?\n");
        de->last_seq = vseq;
        exit(1);
//        return AVERROR_INVALIDDATA;
    }

    fill_frame(&vframe, seq, h->raw_frame_header, &h->cur_frame, h->ref);
    fill_film_grain(&vgrain, &h->cur_frame.film_grain);

    fill_frame_ts(&vframe, h);

    {
        req_av1_frame_env_t * const rd = av1frame_rd(&h->cur_frame);
        AVBufferRef **r = rd->refs;
        unsigned int i;
        for (i = 0; i != V4L2_AV1_REFS_PER_FRAME; ++i) {
            const AVFrame * const f = h->ref[vframe.ref_frame_idx[i]].f;
            if (f != NULL && f->buf[0] != NULL) {
                av_assert0(*r == NULL);
                *r++ = av_buffer_ref(f->buf[0]);
                av_log(avctx, AV_LOG_TRACE, "Frame %"PRId64"[%d]: Ref TS %"PRId64" Refs: %d\n", rd->timestamp, i,
                       av1frame_rd(&h->ref[vframe.ref_frame_idx[i]])->timestamp, av_buffer_get_ref_count(r[-1]));
            }
        }
    }

    decode_q_wait(&ctx->decode_q, &rd->decode_ent);

    av_assert0(rd->qe_dst == NULL);
    if ((rd->qe_dst = mediabufs_dst_qent_alloc(ctx->mbufs, ctx->dbufs)) == NULL) {
        av_log(avctx, AV_LOG_ERROR, "%s: Failed to get dst buffer\n", __func__);
        rv = AVERROR(ENOMEM);
        goto fail1;
    }

    if ((mreq = media_request_get(ctx->mpool)) == NULL) {
        av_log(avctx, AV_LOG_ERROR, "%s: Failed to alloc media request\n", __func__);
        return AVERROR(ENOMEM);
    }

    if ((rv = mediabufs_ctl_set_ext_ctrls(ctx->mbufs, mreq, control, 4)) != 0)
        goto fail2;

    if ((src = mediabufs_src_qent_get(ctx->mbufs)) == NULL) {
        av_log(avctx, AV_LOG_ERROR, "%s: Failed to get src buffer\n", __func__);
        goto fail1;
    }

    buf_size = rd->tile_groups[rd->num_tiles - 1].tile_offset + rd->tile_groups[rd->num_tiles - 1].tile_size;
    if (qent_src_data_copy(src, 0, rd->buffer0, buf_size, ctx->dbufs) != 0) {
        av_log(avctx, AV_LOG_ERROR, "%s: Failed data copy\n", __func__);
        goto fail2;
    }

    tv = cvt_dpb_to_tv(rd->timestamp);
    if (qent_src_params_set(src, &tv)) {
        av_log(avctx, AV_LOG_ERROR, "%s: Failed src param set\n", __func__);
        goto fail2;
    }

    stat = mediabufs_start_request(ctx->mbufs, &mreq, &src, rd->qe_dst, true);

    if (stat != MEDIABUFS_STATUS_SUCCESS) {
        av_log(avctx, AV_LOG_ERROR, "%s: Failed to start request\n", __func__);
        return AVERROR_UNKNOWN;
    }

    // Set the drm_prime desriptor
    if (drm_from_format(&rd->drm, mediabufs_dst_fmt(ctx->mbufs)) != 0) {
        av_log(avctx, AV_LOG_ERROR, "Unknown format\n");
        return AVERROR_UNKNOWN;
    }
    rd->drm.objects[0].fd = dmabuf_fd(qent_dst_dmabuf(rd->qe_dst, 0));
    rd->drm.objects[0].size = dmabuf_size(qent_dst_dmabuf(rd->qe_dst, 0));
#if 0
    {
        static int frame_no = 0;
        AVFrame * const cur_frame = h->cur_frame.f;
        char fname[64] = {0};
        FILE * f = NULL;
        int i, j;

        FrameDecodeData *fdd = (FrameDecodeData*)cur_frame->private_ref->data;
        const AVPixFmtDescriptor *pd = av_pix_fmt_desc_get(avctx->sw_pix_fmt);
        int last_plane = -1;

        struct dmabuf_h * const dh = (struct dmabuf_h *)qent_dst_dmabuf(rd->qe_dst, 0);
        const uint8_t * pbase;

        dmabuf_read_start(dh);
        pbase = dmabuf_map(dh);

        fdd->post_process(avctx, cur_frame);
        snprintf(fname, 63, "frame_%s_%dx%d_%03d", 
                 pd == NULL ? "----" : pd->name,
                 cur_frame->width, cur_frame->height, frame_no++);
        if (pd == NULL) {
            av_log(avctx, AV_LOG_ERROR, "Failed to find pix desc %d\n", cur_frame->format);
        }
        else if ((f = fopen(fname, "wb")) == NULL) {
            av_log(avctx, AV_LOG_ERROR, "Failed to open %s\n", fname);
        }
        else {
            for (j = 0; j != pd->nb_components; ++j) {
                const unsigned int sw = (j == 1 || j == 2) ? pd->log2_chroma_w : 0;
                const unsigned int sh = (j == 1 || j == 2) ? pd->log2_chroma_h : 0;
                if (pd->comp[j].plane == last_plane)
                    continue;
                last_plane = pd->comp[j].plane;
                for (i = 0; i != cur_frame->height >> sh; ++i) {
                    fwrite(pbase + rd->drm.layers[0].planes[last_plane].offset +
                                rd->drm.layers[0].planes[last_plane].pitch * i,
                           cur_frame->width * pd->comp[j].step >> sw, 1, f);
                }
            }
            fclose(f);
        }

        dmabuf_read_end(dh);
    }
#endif
    decode_q_remove(&ctx->decode_q, &rd->decode_ent);
    return 0;

fail2:
    mediabufs_src_qent_abort(ctx->mbufs, &src);
fail1:
    media_request_abort(&mreq);
    decode_q_remove(&ctx->decode_q, &rd->decode_ent);
    return AVERROR_UNKNOWN;
}

static void
req_av1_abort_frame(AVCodecContext * const avctx, V4L2RequestContextHEVC *const ctx)
{
    const AV1DecContext * const h = avctx->priv_data;
    req_av1_frame_env_t *const rd = av1ctx_rd(h);

    fprintf(stderr, "<<< %s\n", __func__);
    if (rd != NULL) {
        media_request_abort(&rd->req);
        mediabufs_src_qent_abort(ctx->mbufs, &rd->qe_src);

        decode_q_remove(&ctx->decode_q, &rd->decode_ent);
    }
}

static int
req_av1_frame_params(AVCodecContext *avctx, V4L2RequestContextHEVC *const ctx, AVBufferRef *hw_frames_ctx)
{
    AVHWFramesContext *hwfc = (AVHWFramesContext*)hw_frames_ctx->data;
    const struct v4l2_format *vfmt = mediabufs_dst_fmt(ctx->mbufs);
    const uint32_t pixfmt = V4L2_TYPE_IS_MULTIPLANAR(vfmt->type) ?
            vfmt->fmt.pix_mp.pixelformat : vfmt->fmt.pix.pixelformat;

    hwfc->format = AV_PIX_FMT_DRM_PRIME;
    hwfc->sw_format = ff_v4l2_format_v4l2_to_avfmt(pixfmt, AV_CODEC_ID_RAWVIDEO);
    if (V4L2_TYPE_IS_MULTIPLANAR(vfmt->type)) {
        hwfc->width = vfmt->fmt.pix_mp.width;
        hwfc->height = vfmt->fmt.pix_mp.height;
    } else {
        hwfc->width = vfmt->fmt.pix.width;
        hwfc->height = vfmt->fmt.pix.height;
    }
    av_log(avctx, AV_LOG_DEBUG, "%s: avctx=%p ctx=%p hw_frames_ctx=%p hwfc=%p pool=%p width=%d height=%d initial_pool_size=%d\n", __func__, avctx, ctx, hw_frames_ctx, hwfc, hwfc->pool, hwfc->width, hwfc->height, hwfc->initial_pool_size);
    return 0;
}

static void
free_av1_buf0(void *opaque, uint8_t *data)
{
    AVCodecContext *avctx = opaque;
    req_av1_frame_env_t * const rd = (req_av1_frame_env_t *)data;

    av_log(NULL, AV_LOG_DEBUG, "%s: avctx=%p data=%p, TS %"PRId64"\n", __func__, avctx, data, rd->timestamp);

    frame_finish(rd);

    qent_dst_unref(&rd->qe_dst);

    // We don't expect req or qe_src to be set
    if (rd->req || rd->qe_src)
        av_log(NULL, AV_LOG_ERROR, "%s: qe_src %p or req %p not NULL\n", __func__, rd->req, rd->qe_src);

    av_freep(&rd->tile_groups);

    av_free(rd);
}

static AVBufferRef *
alloc_av1_buf0(AVCodecContext *avctx, int size)
{
    AVBufferRef *ref;
    uint8_t *data;

    data = av_mallocz(size);
    if (!data)
        return NULL;

    av_log(avctx, AV_LOG_DEBUG, "%s: avctx=%p size=%d data=%p\n", __func__, avctx, size, data);
    ref = av_buffer_create(data, size, free_av1_buf0, avctx, 0);
    if (!ref) {
        av_freep(&data);
        return NULL;
    }
    return ref;
}

static int
req_av1_alloc_frame(AVCodecContext * avctx, V4L2RequestContextHEVC *const ctx, AVFrame *frame)
{
    int rv;

    frame->buf[0] = alloc_av1_buf0(avctx, sizeof(req_av1_frame_env_t));
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


static const struct v4l2_req_decode_fns req_av1_fns = {
    .src_pix_fmt_v4l2 = V4L2_PIX_FMT_AV1_FRAME,
    .name = "V4L2 AV1 stateless",
    .probe = req_av1_probe,
    .set_controls = req_av1_set_controls,
    .uninit = req_av1_uninit,

    .start_frame    = req_av1_start_frame,
    .decode_slice   = req_av1_decode_slice,
    .end_frame      = req_av1_end_frame,
    .abort_frame    = req_av1_abort_frame,
    .frame_params   = req_av1_frame_params,
    .alloc_frame    = req_av1_alloc_frame,
};

static int v4l2_request_av1_init(AVCodecContext *avctx)
{
    const AV1DecContext * const s = avctx->priv_data;
    const AV1RawSequenceHeader * const seq = s->raw_seq;

    const struct v4l2_req_decode_fns * const try_fns[] = {
        &req_av1_fns,
        NULL
    };

    return ff_v4l2_request_init(avctx, try_fns, seq->max_frame_width_minus_1 + 1, seq->max_frame_height_minus_1 + 1,
                             get_bit_depth_from_seq(seq), AV1_TOTAL_REFS_PER_FRAME);
}


const FFHWAccel ff_av1_v4l2request_hwaccel = {
    .p = {
        .name           = "av1_v4l2request",
        .type           = AVMEDIA_TYPE_VIDEO,
        .id             = AV_CODEC_ID_HEVC,
        .pix_fmt        = AV_PIX_FMT_DRM_PRIME,
    },
    .alloc_frame    = ff_v4l2_request_alloc_frame,
    .start_frame    = ff_v4l2_request_start_frame,
    .decode_slice   = ff_v4l2_request_decode_slice,
    .end_frame      = ff_v4l2_request_end_frame,
    .abort_frame    = ff_v4l2_request_abort_frame,
    .init           = v4l2_request_av1_init,
    .uninit         = ff_v4l2_request_uninit,
    .free_frame_priv = ff_v4l2_request_free_frame_priv,
    .frame_priv_data_size  = 128,
    .update_thread_context = ff_v4l2_request_update_thread_context,
    .priv_data_size = sizeof(V4L2RequestPrivHEVC),
    .frame_params   = ff_v4l2_request_frame_params,
    .caps_internal  = HWACCEL_CAP_ASYNC_SAFE | HWACCEL_CAP_THREAD_SAFE,
};
