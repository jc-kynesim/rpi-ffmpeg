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

#include "hwconfig.h"
#include "v4l2_request.h"
#include "vp9dec.h"
#include "vp9-ctrls.h"

typedef struct V4L2RequestControlsVP9 {
    struct v4l2_ctrl_vp9_frame_decode_params decode_params;
} V4L2RequestControlsVP9;

static const uint8_t ff_to_v4l2_intramode[] = {
    [VERT_PRED] = V4L2_VP9_INTRA_PRED_MODE_V,
    [HOR_PRED] = V4L2_VP9_INTRA_PRED_MODE_H,
    [DC_PRED] = V4L2_VP9_INTRA_PRED_MODE_DC,
    [DIAG_DOWN_LEFT_PRED] = V4L2_VP9_INTRA_PRED_MODE_D45,
    [DIAG_DOWN_RIGHT_PRED] = V4L2_VP9_INTRA_PRED_MODE_D135,
    [VERT_RIGHT_PRED] = V4L2_VP9_INTRA_PRED_MODE_D117,
    [HOR_DOWN_PRED] = V4L2_VP9_INTRA_PRED_MODE_D153,
    [VERT_LEFT_PRED] = V4L2_VP9_INTRA_PRED_MODE_D63,
    [HOR_UP_PRED] = V4L2_VP9_INTRA_PRED_MODE_D207,
    [TM_VP8_PRED] = V4L2_VP9_INTRA_PRED_MODE_TM,
};

static int v4l2_request_vp9_set_frame_ctx(AVCodecContext *avctx, unsigned int id)
{
    VP9Context *s = avctx->priv_data;
    struct v4l2_ctrl_vp9_frame_ctx fctx = {};
    struct v4l2_ext_control control[] = {
        {
            .id = V4L2_CID_MPEG_VIDEO_VP9_FRAME_CONTEXT(id),
            .ptr = &fctx,
            .size = sizeof(fctx),
        },
    };

    memcpy(fctx.probs.tx8, s->prob_ctx[id].p.tx8p, sizeof(s->prob_ctx[id].p.tx8p));
    memcpy(fctx.probs.tx16, s->prob_ctx[id].p.tx16p, sizeof(s->prob_ctx[id].p.tx16p));
    memcpy(fctx.probs.tx32, s->prob_ctx[id].p.tx32p, sizeof(s->prob_ctx[id].p.tx32p));
    memcpy(fctx.probs.coef, s->prob_ctx[id].coef, sizeof(s->prob_ctx[id].coef));
    memcpy(fctx.probs.skip, s->prob_ctx[id].p.skip, sizeof(s->prob_ctx[id].p.skip));
    memcpy(fctx.probs.inter_mode, s->prob_ctx[id].p.mv_mode, sizeof(s->prob_ctx[id].p.mv_mode));
    memcpy(fctx.probs.interp_filter, s->prob_ctx[id].p.filter, sizeof(s->prob_ctx[id].p.filter));
    memcpy(fctx.probs.is_inter, s->prob_ctx[id].p.intra, sizeof(s->prob_ctx[id].p.intra));
    memcpy(fctx.probs.comp_mode, s->prob_ctx[id].p.comp, sizeof(s->prob_ctx[id].p.comp));
    memcpy(fctx.probs.single_ref, s->prob_ctx[id].p.single_ref, sizeof(s->prob_ctx[id].p.single_ref));
    memcpy(fctx.probs.comp_ref, s->prob_ctx[id].p.comp_ref, sizeof(s->prob_ctx[id].p.comp_ref));
    memcpy(fctx.probs.y_mode, s->prob_ctx[id].p.y_mode, sizeof(s->prob_ctx[id].p.y_mode));
    for (unsigned i = 0; i < 10; i++)
        memcpy(fctx.probs.uv_mode[ff_to_v4l2_intramode[i]], s->prob_ctx[id].p.uv_mode[i], sizeof(s->prob_ctx[id].p.uv_mode[0]));
    for (unsigned i = 0; i < 4; i++)
        memcpy(fctx.probs.partition[i * 4], s->prob_ctx[id].p.partition[3 - i], sizeof(s->prob_ctx[id].p.partition[0]));
    memcpy(fctx.probs.mv.joint, s->prob_ctx[id].p.mv_joint, sizeof(s->prob_ctx[id].p.mv_joint));
    for (unsigned i = 0; i < 2; i++) {
         fctx.probs.mv.sign[i] = s->prob_ctx[id].p.mv_comp[i].sign;
         memcpy(fctx.probs.mv.class[i], s->prob_ctx[id].p.mv_comp[i].classes, sizeof(s->prob_ctx[id].p.mv_comp[0].classes));
         fctx.probs.mv.class0_bit[i] = s->prob_ctx[id].p.mv_comp[i].class0;
         memcpy(fctx.probs.mv.bits[i], s->prob_ctx[id].p.mv_comp[i].bits, sizeof(s->prob_ctx[id].p.mv_comp[0].bits));
         memcpy(fctx.probs.mv.class0_fr[i], s->prob_ctx[id].p.mv_comp[i].class0_fp, sizeof(s->prob_ctx[id].p.mv_comp[0].class0_fp));
         memcpy(fctx.probs.mv.fr[i], s->prob_ctx[id].p.mv_comp[i].fp, sizeof(s->prob_ctx[id].p.mv_comp[0].fp));
         fctx.probs.mv.class0_hp[i] = s->prob_ctx[id].p.mv_comp[i].class0_hp;
         fctx.probs.mv.hp[i] = s->prob_ctx[id].p.mv_comp[i].hp;
    }

    return ff_v4l2_request_set_controls(avctx, control, FF_ARRAY_ELEMS(control));
}

static int v4l2_request_vp9_get_frame_ctx(AVCodecContext *avctx, unsigned int id)
{
    VP9Context *s = avctx->priv_data;
    struct v4l2_ctrl_vp9_frame_ctx fctx = {};
    struct v4l2_ext_control control[] = {
        {
            .id = V4L2_CID_MPEG_VIDEO_VP9_FRAME_CONTEXT(id),
            .ptr = &fctx,
            .size = sizeof(fctx),
        },
    };

    int ret = ff_v4l2_request_get_controls(avctx, control, FF_ARRAY_ELEMS(control));
    if (ret)
        return ret;

    memcpy(s->prob_ctx[id].p.tx8p, fctx.probs.tx8, sizeof(s->prob_ctx[id].p.tx8p));
    memcpy(s->prob_ctx[id].p.tx16p, fctx.probs.tx16, sizeof(s->prob_ctx[id].p.tx16p));
    memcpy(s->prob_ctx[id].p.tx32p, fctx.probs.tx32, sizeof(s->prob_ctx[id].p.tx32p));
    memcpy(s->prob_ctx[id].coef, fctx.probs.coef, sizeof(s->prob_ctx[id].coef));
    memcpy(s->prob_ctx[id].p.skip, fctx.probs.skip, sizeof(s->prob_ctx[id].p.skip));
    memcpy(s->prob_ctx[id].p.mv_mode, fctx.probs.inter_mode, sizeof(s->prob_ctx[id].p.mv_mode));
    memcpy(s->prob_ctx[id].p.filter, fctx.probs.interp_filter, sizeof(s->prob_ctx[id].p.filter));
    memcpy(s->prob_ctx[id].p.intra, fctx.probs.is_inter, sizeof(s->prob_ctx[id].p.intra));
    memcpy(s->prob_ctx[id].p.comp, fctx.probs.comp_mode, sizeof(s->prob_ctx[id].p.comp));
    memcpy(s->prob_ctx[id].p.single_ref, fctx.probs.single_ref, sizeof(s->prob_ctx[id].p.single_ref));
    memcpy(s->prob_ctx[id].p.comp_ref, fctx.probs.comp_ref, sizeof(s->prob_ctx[id].p.comp_ref));
    memcpy(s->prob_ctx[id].p.y_mode, fctx.probs.y_mode, sizeof(s->prob_ctx[id].p.y_mode));
    for (unsigned i = 0; i < 10; i++)
        memcpy(s->prob_ctx[id].p.uv_mode[i], fctx.probs.uv_mode[ff_to_v4l2_intramode[i]], sizeof(s->prob_ctx[id].p.uv_mode[0]));
    for (unsigned i = 0; i < 4; i++)
        memcpy(s->prob_ctx[id].p.partition[3 - i], fctx.probs.partition[i * 4], sizeof(s->prob_ctx[id].p.partition[0]));
    memcpy(s->prob_ctx[id].p.mv_joint, fctx.probs.mv.joint, sizeof(s->prob_ctx[id].p.mv_joint));
    for (unsigned i = 0; i < 2; i++) {
         s->prob_ctx[id].p.mv_comp[i].sign = fctx.probs.mv.sign[i];
         memcpy(s->prob_ctx[id].p.mv_comp[i].classes, fctx.probs.mv.class[i], sizeof(s->prob_ctx[id].p.mv_comp[0].classes));
         s->prob_ctx[id].p.mv_comp[i].class0 = fctx.probs.mv.class0_bit[i];
         memcpy(s->prob_ctx[id].p.mv_comp[i].bits, fctx.probs.mv.bits[i], sizeof(s->prob_ctx[id].p.mv_comp[0].bits));
         memcpy(s->prob_ctx[id].p.mv_comp[i].class0_fp, fctx.probs.mv.class0_fr[i], sizeof(s->prob_ctx[id].p.mv_comp[0].class0_fp));
         memcpy(s->prob_ctx[id].p.mv_comp[i].fp, fctx.probs.mv.fr[i], sizeof(s->prob_ctx[id].p.mv_comp[0].fp));
         s->prob_ctx[id].p.mv_comp[i].class0_hp = fctx.probs.mv.class0_hp[i];
         s->prob_ctx[id].p.mv_comp[i].hp = fctx.probs.mv.hp[i];
    }

    return 0;
}

static int v4l2_request_vp9_start_frame(AVCodecContext *avctx,
                                        av_unused const uint8_t *buffer,
                                        av_unused uint32_t size)
{
    const VP9Context *s = avctx->priv_data;
    const VP9Frame *f = &s->s.frames[CUR_FRAME];
    V4L2RequestControlsVP9 *controls = f->hwaccel_picture_private;
    struct v4l2_ctrl_vp9_frame_decode_params *dec_params = &controls->decode_params;
    int ret;

    if (s->s.h.keyframe || s->s.h.errorres || (s->s.h.intraonly && s->s.h.resetctx == 3)) {
        for (unsigned i = 0; i < 4; i++) {
            ret = v4l2_request_vp9_set_frame_ctx(avctx, i);
            if (ret)
                return ret;
        }
    } else if (s->s.h.intraonly && s->s.h.resetctx == 2) {
        ret = v4l2_request_vp9_set_frame_ctx(avctx, s->s.h.framectxid);
        if (ret)
            return ret;
    }

    if (s->s.h.keyframe)
        dec_params->flags |= V4L2_VP9_FRAME_FLAG_KEY_FRAME;
    if (!s->s.h.invisible)
        dec_params->flags |= V4L2_VP9_FRAME_FLAG_SHOW_FRAME;
    if (s->s.h.errorres)
        dec_params->flags |= V4L2_VP9_FRAME_FLAG_ERROR_RESILIENT;
    if (s->s.h.intraonly)
        dec_params->flags |= V4L2_VP9_FRAME_FLAG_INTRA_ONLY;
    if (!s->s.h.keyframe && s->s.h.highprecisionmvs)
        dec_params->flags |= V4L2_VP9_FRAME_FLAG_ALLOW_HIGH_PREC_MV;
    if (s->s.h.refreshctx)
        dec_params->flags |= V4L2_VP9_FRAME_FLAG_REFRESH_FRAME_CTX;
    if (s->s.h.parallelmode)
        dec_params->flags |= V4L2_VP9_FRAME_FLAG_PARALLEL_DEC_MODE;
    if (s->ss_h)
        dec_params->flags |= V4L2_VP9_FRAME_FLAG_X_SUBSAMPLING;
    if (s->ss_v)
        dec_params->flags |= V4L2_VP9_FRAME_FLAG_Y_SUBSAMPLING;
    if (avctx->color_range == AVCOL_RANGE_JPEG)
        dec_params->flags |= V4L2_VP9_FRAME_FLAG_COLOR_RANGE_FULL_SWING;

    dec_params->compressed_header_size = s->s.h.compressed_header_size;
    dec_params->uncompressed_header_size = s->s.h.uncompressed_header_size;
    dec_params->profile = s->s.h.profile;
    dec_params->reset_frame_context = s->s.h.resetctx > 0 ? s->s.h.resetctx - 1 : 0;
    dec_params->frame_context_idx = s->s.h.framectxid;
    dec_params->bit_depth = s->s.h.bpp;

    dec_params->interpolation_filter = s->s.h.filtermode ^ (s->s.h.filtermode <= 1);
    dec_params->tile_cols_log2 = s->s.h.tiling.log2_tile_cols;
    dec_params->tile_rows_log2 = s->s.h.tiling.log2_tile_rows;
    dec_params->tx_mode = s->s.h.txfmmode;
    dec_params->reference_mode = s->s.h.comppredmode;
    dec_params->frame_width_minus_1 = s->w - 1;
    dec_params->frame_height_minus_1 = s->h - 1;
    //dec_params->render_width_minus_1 = avctx->width - 1;
    //dec_params->render_height_minus_1 = avctx->height - 1;

    for (unsigned i = 0; i < 3; i++) {
        const ThreadFrame *ref = &s->s.refs[s->s.h.refidx[i]];
        if (ref->f && ref->f->buf[0])
            dec_params->refs[i] = ff_v4l2_request_get_capture_timestamp(ref->f);
    }

    if (s->s.h.lf_delta.enabled)
        dec_params->lf.flags |= V4L2_VP9_LOOP_FILTER_FLAG_DELTA_ENABLED;
    if (s->s.h.lf_delta.updated)
        dec_params->lf.flags |= V4L2_VP9_LOOP_FILTER_FLAG_DELTA_UPDATE;

    dec_params->lf.level = s->s.h.filter.level;
    dec_params->lf.sharpness = s->s.h.filter.sharpness;
    for (unsigned i = 0; i < 4; i++)
        dec_params->lf.ref_deltas[i] = s->s.h.lf_delta.ref[i];
    for (unsigned i = 0; i < 2; i++)
        dec_params->lf.mode_deltas[i] = s->s.h.lf_delta.mode[i];
    for (unsigned i = 0; i < 8; i++) {
        for (unsigned j = 0; j < 4; j++)
            memcpy(dec_params->lf.level_lookup[i][j], s->s.h.segmentation.feat[i].lflvl[j], sizeof(dec_params->lf.level_lookup[0][0]));
    }

    dec_params->quant.base_q_idx = s->s.h.yac_qi;
    dec_params->quant.delta_q_y_dc = s->s.h.ydc_qdelta;
    dec_params->quant.delta_q_uv_dc = s->s.h.uvdc_qdelta;
    dec_params->quant.delta_q_uv_ac = s->s.h.uvac_qdelta;

    if (s->s.h.segmentation.enabled)
        dec_params->seg.flags |= V4L2_VP9_SEGMENTATION_FLAG_ENABLED;
    if (s->s.h.segmentation.update_map)
        dec_params->seg.flags |= V4L2_VP9_SEGMENTATION_FLAG_UPDATE_MAP;
    if (s->s.h.segmentation.temporal)
        dec_params->seg.flags |= V4L2_VP9_SEGMENTATION_FLAG_TEMPORAL_UPDATE;
    if (s->s.h.segmentation.update_data)
        dec_params->seg.flags |= V4L2_VP9_SEGMENTATION_FLAG_UPDATE_DATA;
    if (s->s.h.segmentation.absolute_vals)
        dec_params->seg.flags |= V4L2_VP9_SEGMENTATION_FLAG_ABS_OR_DELTA_UPDATE;

    for (unsigned i = 0; i < 7; i++)
        dec_params->seg.tree_probs[i] = s->s.h.segmentation.prob[i];

    if (s->s.h.segmentation.temporal) {
        for (unsigned i = 0; i < 3; i++)
            dec_params->seg.pred_probs[i] = s->s.h.segmentation.pred_prob[i];
    } else {
        memset(dec_params->seg.pred_probs, 255, sizeof(dec_params->seg.pred_probs));
    }

    for (unsigned i = 0; i < 8; i++) {
        if (s->s.h.segmentation.feat[i].q_enabled) {
            dec_params->seg.feature_enabled[i] |= 1 << V4L2_VP9_SEGMENT_FEATURE_QP_DELTA;
            dec_params->seg.feature_data[i][V4L2_VP9_SEGMENT_FEATURE_QP_DELTA] = s->s.h.segmentation.feat[i].q_val;
        }

        if (s->s.h.segmentation.feat[i].lf_enabled) {
            dec_params->seg.feature_enabled[i] |= 1 << V4L2_VP9_SEGMENT_FEATURE_LF;
            dec_params->seg.feature_data[i][V4L2_VP9_SEGMENT_FEATURE_LF] = s->s.h.segmentation.feat[i].lf_val;
        }

        if (s->s.h.segmentation.feat[i].ref_enabled) {
            dec_params->seg.feature_enabled[i] |= 1 << V4L2_VP9_SEGMENT_FEATURE_REF_FRAME;
            dec_params->seg.feature_data[i][V4L2_VP9_SEGMENT_FEATURE_REF_FRAME] = s->s.h.segmentation.feat[i].ref_val;
        }

        if (s->s.h.segmentation.feat[i].skip_enabled)
            dec_params->seg.feature_enabled[i] |= 1 << V4L2_VP9_SEGMENT_FEATURE_SKIP;
    }

    memcpy(dec_params->probs.tx8, s->prob.p.tx8p, sizeof(s->prob.p.tx8p));
    memcpy(dec_params->probs.tx16, s->prob.p.tx16p, sizeof(s->prob.p.tx16p));
    memcpy(dec_params->probs.tx32, s->prob.p.tx32p, sizeof(s->prob.p.tx32p));
    for (unsigned i = 0; i < 4; i++) {
        for (unsigned j = 0; j < 2; j++) {
            for (unsigned k = 0; k < 2; k++) {
                for (unsigned l = 0; l < 6; l++) {
                    for (unsigned m = 0; m < 6; m++) {
                        memcpy(dec_params->probs.coef[i][j][k][l][m], s->prob.coef[i][j][k][l][m], sizeof(dec_params->probs.coef[0][0][0][0][0]));
                    }
                }
            }
        }
    }
    memcpy(dec_params->probs.skip, s->prob.p.skip, sizeof(s->prob.p.skip));
    memcpy(dec_params->probs.inter_mode, s->prob.p.mv_mode, sizeof(s->prob.p.mv_mode));
    memcpy(dec_params->probs.interp_filter, s->prob.p.filter, sizeof(s->prob.p.filter));
    memcpy(dec_params->probs.is_inter, s->prob.p.intra, sizeof(s->prob.p.intra));
    memcpy(dec_params->probs.comp_mode, s->prob.p.comp, sizeof(s->prob.p.comp));
    memcpy(dec_params->probs.single_ref, s->prob.p.single_ref, sizeof(s->prob.p.single_ref));
    memcpy(dec_params->probs.comp_ref, s->prob.p.comp_ref, sizeof(s->prob.p.comp_ref));
    memcpy(dec_params->probs.y_mode, s->prob.p.y_mode, sizeof(s->prob.p.y_mode));
    for (unsigned i = 0; i < 10; i++)
        memcpy(dec_params->probs.uv_mode[ff_to_v4l2_intramode[i]], s->prob.p.uv_mode[i], sizeof(s->prob.p.uv_mode[0]));
    for (unsigned i = 0; i < 4; i++)
        memcpy(dec_params->probs.partition[i * 4], s->prob.p.partition[3 - i], sizeof(s->prob.p.partition[0]));
    memcpy(dec_params->probs.mv.joint, s->prob.p.mv_joint, sizeof(s->prob.p.mv_joint));
    for (unsigned i = 0; i < 2; i++) {
         dec_params->probs.mv.sign[i] = s->prob.p.mv_comp[i].sign;
         memcpy(dec_params->probs.mv.class[i], s->prob.p.mv_comp[i].classes, sizeof(s->prob.p.mv_comp[0].classes));
         dec_params->probs.mv.class0_bit[i] = s->prob.p.mv_comp[i].class0;
         memcpy(dec_params->probs.mv.bits[i], s->prob.p.mv_comp[i].bits, sizeof(s->prob.p.mv_comp[0].bits));
         memcpy(dec_params->probs.mv.class0_fr[i], s->prob.p.mv_comp[i].class0_fp, sizeof(s->prob.p.mv_comp[0].class0_fp));
         memcpy(dec_params->probs.mv.fr[i], s->prob.p.mv_comp[i].fp, sizeof(s->prob.p.mv_comp[0].fp));
         dec_params->probs.mv.class0_hp[i] = s->prob.p.mv_comp[i].class0_hp;
         dec_params->probs.mv.hp[i] = s->prob.p.mv_comp[i].hp;
    }

    return ff_v4l2_request_reset_frame(avctx, f->tf.f);
}

static int v4l2_request_vp9_decode_slice(AVCodecContext *avctx, const uint8_t *buffer, uint32_t size)
{
    const VP9Context *s = avctx->priv_data;
    const VP9Frame *f = &s->s.frames[CUR_FRAME];

    return ff_v4l2_request_append_output_buffer(avctx, f->tf.f, buffer, size);
}

static int v4l2_request_vp9_end_frame(AVCodecContext *avctx)
{
    const VP9Context *s = avctx->priv_data;
    const VP9Frame *f = &s->s.frames[CUR_FRAME];
    V4L2RequestControlsVP9 *controls = f->hwaccel_picture_private;
    int ret;

    struct v4l2_ext_control control[] = {
        {
            .id = V4L2_CID_MPEG_VIDEO_VP9_FRAME_DECODE_PARAMS,
            .ptr = &controls->decode_params,
            .size = sizeof(controls->decode_params),
        },
    };

    ret = ff_v4l2_request_decode_frame(avctx, f->tf.f, control, FF_ARRAY_ELEMS(control));
    if (ret)
        return ret;

    if (!s->s.h.refreshctx)
        return 0;

    return v4l2_request_vp9_get_frame_ctx(avctx, s->s.h.framectxid);
}

static int v4l2_request_vp9_init(AVCodecContext *avctx)
{
    // TODO: check V4L2_CID_MPEG_VIDEO_VP9_PROFILE
    return ff_v4l2_request_init(avctx, V4L2_PIX_FMT_VP9_FRAME, 3 * 1024 * 1024, NULL, 0);
}

const AVHWAccel ff_vp9_v4l2request_hwaccel = {
    .name           = "vp9_v4l2request",
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_VP9,
    .pix_fmt        = AV_PIX_FMT_DRM_PRIME,
    .start_frame    = v4l2_request_vp9_start_frame,
    .decode_slice   = v4l2_request_vp9_decode_slice,
    .end_frame      = v4l2_request_vp9_end_frame,
    .frame_priv_data_size = sizeof(V4L2RequestControlsVP9),
    .init           = v4l2_request_vp9_init,
    .uninit         = ff_v4l2_request_uninit,
    .priv_data_size = sizeof(V4L2RequestContext),
    .frame_params   = ff_v4l2_request_frame_params,
    .caps_internal  = HWACCEL_CAP_ASYNC_SAFE,
};
