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
#include "vp8.h"
#include "vp8-ctrls.h"

typedef struct V4L2RequestControlsVP8 {
    struct v4l2_ctrl_vp8_frame_header ctrl;
} V4L2RequestControlsVP8;

static int v4l2_request_vp8_start_frame(AVCodecContext          *avctx,
                                        av_unused const uint8_t *buffer,
                                        av_unused uint32_t       size)
{
    const VP8Context *s = avctx->priv_data;
    V4L2RequestControlsVP8 *controls = s->framep[VP56_FRAME_CURRENT]->hwaccel_picture_private;

    memset(&controls->ctrl, 0, sizeof(controls->ctrl));
    return ff_v4l2_request_reset_frame(avctx, s->framep[VP56_FRAME_CURRENT]->tf.f);
}

static int v4l2_request_vp8_end_frame(AVCodecContext *avctx)
{
    const VP8Context *s = avctx->priv_data;
    V4L2RequestControlsVP8 *controls = s->framep[VP56_FRAME_CURRENT]->hwaccel_picture_private;
    struct v4l2_ext_control control[] = {
        {
            .id = V4L2_CID_MPEG_VIDEO_VP8_FRAME_HEADER,
            .ptr = &controls->ctrl,
            .size = sizeof(controls->ctrl),
        },
    };

    return ff_v4l2_request_decode_frame(avctx, s->framep[VP56_FRAME_CURRENT]->tf.f,
                                        control, FF_ARRAY_ELEMS(control));
}

static int v4l2_request_vp8_decode_slice(AVCodecContext *avctx,
                                         const uint8_t *buffer,
                                         uint32_t size)
{
    const VP8Context *s = avctx->priv_data;
    V4L2RequestControlsVP8 *controls = s->framep[VP56_FRAME_CURRENT]->hwaccel_picture_private;
    struct v4l2_ctrl_vp8_frame_header *hdr = &controls->ctrl;
    const uint8_t *data = buffer + 3 + 7 * s->keyframe;
    unsigned int i, j, k;

    hdr->version = s->profile & 0x3;
    hdr->width = avctx->width;
    hdr->height = avctx->height;
    /* FIXME: set ->xx_scale */
    hdr->prob_skip_false = s->prob->mbskip;
    hdr->prob_intra = s->prob->intra;
    hdr->prob_gf = s->prob->golden;
    hdr->prob_last = s->prob->last;
    hdr->first_part_size = s->header_partition_size;
    hdr->first_part_header_bits = (8 * (s->coder_state_at_header_end.input - data) -
                                   s->coder_state_at_header_end.bit_count - 8);
    hdr->num_dct_parts = s->num_coeff_partitions;
    for (i = 0; i < 8; i++)
        hdr->dct_part_sizes[i] = s->coeff_partition_size[i];

    hdr->coder_state.range = s->coder_state_at_header_end.range;
    hdr->coder_state.value = s->coder_state_at_header_end.value;
    hdr->coder_state.bit_count = s->coder_state_at_header_end.bit_count;
    if (s->framep[VP56_FRAME_PREVIOUS])
        hdr->last_frame_ts = ff_v4l2_request_get_capture_timestamp(s->framep[VP56_FRAME_PREVIOUS]->tf.f);
    if (s->framep[VP56_FRAME_GOLDEN])
        hdr->golden_frame_ts = ff_v4l2_request_get_capture_timestamp(s->framep[VP56_FRAME_GOLDEN]->tf.f);
    if (s->framep[VP56_FRAME_GOLDEN2])
        hdr->alt_frame_ts = ff_v4l2_request_get_capture_timestamp(s->framep[VP56_FRAME_GOLDEN2]->tf.f);
    hdr->flags |= s->invisible ? 0 : V4L2_VP8_FRAME_HEADER_FLAG_SHOW_FRAME;
    hdr->flags |= s->mbskip_enabled ? V4L2_VP8_FRAME_HEADER_FLAG_MB_NO_SKIP_COEFF : 0;
    hdr->flags |= (s->profile & 0x4) ? V4L2_VP8_FRAME_HEADER_FLAG_EXPERIMENTAL : 0;
    hdr->flags |= s->keyframe ? V4L2_VP8_FRAME_HEADER_FLAG_KEY_FRAME : 0;
    hdr->flags |= s->sign_bias[VP56_FRAME_GOLDEN] ? V4L2_VP8_FRAME_HEADER_FLAG_SIGN_BIAS_GOLDEN : 0;
    hdr->flags |= s->sign_bias[VP56_FRAME_GOLDEN2] ? V4L2_VP8_FRAME_HEADER_FLAG_SIGN_BIAS_ALT : 0;
    hdr->segment_header.flags |= s->segmentation.enabled ? V4L2_VP8_SEGMENT_HEADER_FLAG_ENABLED : 0;
    hdr->segment_header.flags |= s->segmentation.update_map ? V4L2_VP8_SEGMENT_HEADER_FLAG_UPDATE_MAP : 0;
    hdr->segment_header.flags |= s->segmentation.update_feature_data ? V4L2_VP8_SEGMENT_HEADER_FLAG_UPDATE_FEATURE_DATA : 0;
    hdr->segment_header.flags |= s->segmentation.absolute_vals ? 0 : V4L2_VP8_SEGMENT_HEADER_FLAG_DELTA_VALUE_MODE;
    for (i = 0; i < 4; i++) {
        hdr->segment_header.quant_update[i] = s->segmentation.base_quant[i];
        hdr->segment_header.lf_update[i] = s->segmentation.filter_level[i];
    }

    for (i = 0; i < 3; i++)
        hdr->segment_header.segment_probs[i] = s->prob->segmentid[i];

    hdr->lf_header.level = s->filter.level;
    hdr->lf_header.sharpness_level = s->filter.sharpness;
    hdr->lf_header.flags |= s->lf_delta.enabled ? V4L2_VP8_LF_HEADER_ADJ_ENABLE : 0;
    hdr->lf_header.flags |= s->lf_delta.update ? V4L2_VP8_LF_HEADER_DELTA_UPDATE : 0;
    hdr->lf_header.flags |= s->filter.simple ? V4L2_VP8_LF_FILTER_TYPE_SIMPLE : 0;
    for (i = 0; i < 4; i++) {
        hdr->lf_header.ref_frm_delta[i] = s->lf_delta.ref[i];
        hdr->lf_header.mb_mode_delta[i] = s->lf_delta.mode[i + MODE_I4x4];
    }

    // Probabilites
    if (s->keyframe) {
        static const uint8_t keyframe_y_mode_probs[4] = {
            145, 156, 163, 128
        };
        static const uint8_t keyframe_uv_mode_probs[3] = {
            142, 114, 183
        };

        memcpy(hdr->entropy_header.y_mode_probs, keyframe_y_mode_probs,  4);
        memcpy(hdr->entropy_header.uv_mode_probs, keyframe_uv_mode_probs, 3);
    } else {
        for (i = 0; i < 4; i++)
            hdr->entropy_header.y_mode_probs[i] = s->prob->pred16x16[i];
        for (i = 0; i < 3; i++)
            hdr->entropy_header.uv_mode_probs[i] = s->prob->pred8x8c[i];
    }
    for (i = 0; i < 2; i++)
        for (j = 0; j < 19; j++)
            hdr->entropy_header.mv_probs[i][j] = s->prob->mvc[i][j];

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 8; j++) {
            static const int coeff_bands_inverse[8] = {
                0, 1, 2, 3, 5, 6, 4, 15
            };
            int coeff_pos = coeff_bands_inverse[j];

            for (k = 0; k < 3; k++) {
                memcpy(hdr->entropy_header.coeff_probs[i][j][k],
                       s->prob->token[i][coeff_pos][k], 11);
            }
        }
    }

    hdr->quant_header.y_ac_qi = s->quant.yac_qi;
    hdr->quant_header.y_dc_delta = s->quant.ydc_delta;
    hdr->quant_header.y2_dc_delta = s->quant.y2dc_delta;
    hdr->quant_header.y2_ac_delta = s->quant.y2ac_delta;
    hdr->quant_header.uv_dc_delta = s->quant.uvdc_delta;
    hdr->quant_header.uv_ac_delta = s->quant.uvac_delta;

    return ff_v4l2_request_append_output_buffer(avctx, s->framep[VP56_FRAME_CURRENT]->tf.f, buffer, size);
}

static int v4l2_request_vp8_init(AVCodecContext *avctx)
{
    return ff_v4l2_request_init(avctx, V4L2_PIX_FMT_VP8_FRAME, 2 * 1024 * 1024, NULL, 0);
}

const AVHWAccel ff_vp8_v4l2request_hwaccel = {
    .name           = "vp8_v4l2request",
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_VP8,
    .pix_fmt        = AV_PIX_FMT_DRM_PRIME,
    .start_frame    = v4l2_request_vp8_start_frame,
    .decode_slice   = v4l2_request_vp8_decode_slice,
    .end_frame      = v4l2_request_vp8_end_frame,
    .frame_priv_data_size = sizeof(V4L2RequestControlsVP8),
    .init           = v4l2_request_vp8_init,
    .uninit         = ff_v4l2_request_uninit,
    .priv_data_size = sizeof(V4L2RequestContext),
    .frame_params   = ff_v4l2_request_frame_params,
    .caps_internal  = HWACCEL_CAP_ASYNC_SAFE,
};
