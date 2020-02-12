/*
 * HEVC video decoder
 *
 * Copyright (C) 2012 - 2013 Guillaume Martres
 * Copyright (C) 2012 - 2013 Gildas Cocherel
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

#include "libavutil/avassert.h"
#include "libavutil/pixdesc.h"
#include "libavutil/rpi_sand_fns.h"
#include "internal.h"
#include "thread.h"
#include "hevc.h"
#include "rpi_hevcdec.h"

void ff_hevc_rpi_unref_frame(HEVCRpiContext *s, HEVCRpiFrame *frame, int flags)
{
    /* frame->frame can be NULL if context init failed */
    if (!frame->frame || !frame->frame->buf[0])
        return;

    frame->flags &= ~flags;
    if (!frame->flags) {
        ff_thread_release_buffer(s->avctx, &frame->tf);

        av_buffer_unref(&frame->col_mvf_buf);  // OK if already NULL
        frame->col_mvf = NULL;

        frame->collocated_ref = NULL;
    }
}

void ff_hevc_rpi_clear_refs(HEVCRpiContext *s)
{
    int i;
    for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++)
        ff_hevc_rpi_unref_frame(s, &s->DPB[i],
                            HEVC_FRAME_FLAG_SHORT_REF |
                            HEVC_FRAME_FLAG_LONG_REF);
}

void ff_hevc_rpi_flush_dpb(HEVCRpiContext *s)
{
    int i;
    for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++)
        ff_hevc_rpi_unref_frame(s, &s->DPB[i], ~0);
}

static HEVCRpiFrame *alloc_frame(HEVCRpiContext * const s)
{
    int i, ret;
    for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++) {
        HEVCRpiFrame * const frame = &s->DPB[i];
        if (frame->frame->buf[0])
            continue;

        ret = ff_thread_get_buffer(s->avctx, &frame->tf,
                                   AV_GET_BUFFER_FLAG_REF);
        if (ret < 0)
            return NULL;

        frame->col_mvf = NULL;
        frame->col_mvf_buf = NULL;
        if (s->used_for_ref && !s->is_irap)
        {
            frame->col_mvf_buf = av_buffer_pool_get(s->col_mvf_pool);
            if (!frame->col_mvf_buf)
                goto fail;
            frame->col_mvf = (ColMvField *)frame->col_mvf_buf->data;
        }

        frame->frame->top_field_first  = s->sei.picture_timing.picture_struct == AV_PICTURE_STRUCTURE_TOP_FIELD;
        frame->frame->interlaced_frame = (s->sei.picture_timing.picture_struct == AV_PICTURE_STRUCTURE_TOP_FIELD) || (s->sei.picture_timing.picture_struct == AV_PICTURE_STRUCTURE_BOTTOM_FIELD);

        return frame;

fail:
        ff_hevc_rpi_unref_frame(s, frame, ~0);
        return NULL;
    }
    av_log(s->avctx, AV_LOG_ERROR, "Error allocating frame, DPB full.\n");
    return NULL;
}

int ff_hevc_rpi_set_new_ref(HEVCRpiContext *s, AVFrame **frame, int poc)
{
    HEVCRpiFrame *ref;
    int i;

    /* check that this POC doesn't already exist */
    for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++) {
        HEVCRpiFrame *frame = &s->DPB[i];

        if (frame->frame->buf[0] && frame->sequence == s->seq_decode &&
            frame->poc == poc) {
            av_log(s->avctx, AV_LOG_ERROR, "Duplicate POC in a sequence: %d.\n",
                   poc);
            return AVERROR_INVALIDDATA;
        }
    }

    ref = alloc_frame(s);
    if (!ref)
        return AVERROR(ENOMEM);

    *frame = ref->frame;
    s->ref = ref;

    if (s->sh.pic_output_flag)
        ref->flags = HEVC_FRAME_FLAG_OUTPUT | HEVC_FRAME_FLAG_SHORT_REF;
    else
        ref->flags = HEVC_FRAME_FLAG_SHORT_REF;

    ref->poc      = poc;
    ref->sequence = s->seq_decode;
    ref->frame->crop_left   = s->ps.sps->output_window.left_offset;
    ref->frame->crop_right  = s->ps.sps->output_window.right_offset;
    ref->frame->crop_top    = s->ps.sps->output_window.top_offset;
    ref->frame->crop_bottom = s->ps.sps->output_window.bottom_offset;

    return 0;
}

int ff_hevc_rpi_output_frame(HEVCRpiContext *s, AVFrame *out, int flush)
{
    do {
        int nb_output = 0;
        int min_poc   = INT_MAX;
        int i, min_idx, ret;

        if (s->sh.no_output_of_prior_pics_flag == 1 && s->no_rasl_output_flag == 1) {
            for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++) {
                HEVCRpiFrame *frame = &s->DPB[i];
                if (!(frame->flags & HEVC_FRAME_FLAG_BUMPING) && frame->poc != s->poc &&
                        frame->sequence == s->seq_output) {
                    ff_hevc_rpi_unref_frame(s, frame, HEVC_FRAME_FLAG_OUTPUT);
                }
            }
        }

        for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++) {
            HEVCRpiFrame *frame = &s->DPB[i];
            if ((frame->flags & HEVC_FRAME_FLAG_OUTPUT) &&
                frame->sequence == s->seq_output) {
                nb_output++;
                if (frame->poc < min_poc || nb_output == 1) {
                    min_poc = frame->poc;
                    min_idx = i;
                }
            }
        }

        /* wait for more frames before output */
        if (!flush && s->seq_output == s->seq_decode && s->ps.sps &&
            nb_output <= s->ps.sps->temporal_layer[s->ps.sps->max_sub_layers - 1].num_reorder_pics)
            return 0;

        if (nb_output) {
            HEVCRpiFrame *frame = &s->DPB[min_idx];
            if (frame->frame->format == AV_PIX_FMT_VIDEOTOOLBOX && frame->frame->buf[0]->size == 1)
                return 0;

            ret = av_frame_ref(out, frame->frame);
            if (frame->flags & HEVC_FRAME_FLAG_BUMPING)
                ff_hevc_rpi_unref_frame(s, frame, HEVC_FRAME_FLAG_OUTPUT | HEVC_FRAME_FLAG_BUMPING);
            else
                ff_hevc_rpi_unref_frame(s, frame, HEVC_FRAME_FLAG_OUTPUT);
            if (ret < 0)
                return ret;
            av_log(s->avctx, AV_LOG_DEBUG,
                   "Output frame with POC %d.\n", frame->poc);
            return 1;
        }

        if (s->seq_output != s->seq_decode)
            s->seq_output = (s->seq_output + 1) & 0xff;
        else
            break;
    } while (1);

    return 0;
}

void ff_hevc_rpi_bump_frame(HEVCRpiContext *s)
{
    int dpb = 0;
    int min_poc = INT_MAX;
    int i;

    for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++) {
        HEVCRpiFrame *frame = &s->DPB[i];
        if ((frame->flags) &&
            frame->sequence == s->seq_output &&
            frame->poc != s->poc) {
            dpb++;
        }
    }

    if (s->ps.sps && dpb >= s->ps.sps->temporal_layer[s->ps.sps->max_sub_layers - 1].max_dec_pic_buffering) {
        for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++) {
            HEVCRpiFrame *frame = &s->DPB[i];
            if ((frame->flags) &&
                frame->sequence == s->seq_output &&
                frame->poc != s->poc) {
                if (frame->flags == HEVC_FRAME_FLAG_OUTPUT && frame->poc < min_poc) {
                    min_poc = frame->poc;
                }
            }
        }

        for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++) {
            HEVCRpiFrame *frame = &s->DPB[i];
            if (frame->flags & HEVC_FRAME_FLAG_OUTPUT &&
                frame->sequence == s->seq_output &&
                frame->poc <= min_poc) {
                frame->flags |= HEVC_FRAME_FLAG_BUMPING;
            }
        }

        dpb--;
    }
}

static int init_slice_rpl(HEVCRpiContext *s)
{
    if (s->slice_idx >= s->rpl_tab_size)
        return AVERROR_INVALIDDATA;

    s->refPicList = s->rpl_tab[s->slice_idx].refPicList + 0;
    return 0;
}

int ff_hevc_rpi_slice_rpl(HEVCRpiContext *s)
{
    RpiSliceHeader *sh = &s->sh;

    uint8_t nb_list = sh->slice_type == HEVC_SLICE_B ? 2 : 1;
    uint8_t list_idx;
    int i, j, ret;

    ret = init_slice_rpl(s);
    if (ret < 0)
        return ret;

    if (!(s->rps[ST_CURR_BEF].nb_refs + s->rps[ST_CURR_AFT].nb_refs +
          s->rps[LT_CURR].nb_refs)) {
        av_log(s->avctx, AV_LOG_ERROR, "Zero refs in the frame RPS.\n");
        return AVERROR_INVALIDDATA;
    }

    for (list_idx = 0; list_idx < nb_list; list_idx++) {
        RefPicList  rpl_tmp = { { 0 } };
        RefPicList *rpl     = &s->refPicList[list_idx];

        /* The order of the elements is
         * ST_CURR_BEF - ST_CURR_AFT - LT_CURR for the L0 and
         * ST_CURR_AFT - ST_CURR_BEF - LT_CURR for the L1 */
        int cand_lists[3] = { list_idx ? ST_CURR_AFT : ST_CURR_BEF,
                              list_idx ? ST_CURR_BEF : ST_CURR_AFT,
                              LT_CURR };

        /* concatenate the candidate lists for the current frame */
        while (rpl_tmp.nb_refs < sh->nb_refs[list_idx]) {
            for (i = 0; i < FF_ARRAY_ELEMS(cand_lists); i++) {
                RefPicList *rps = &s->rps[cand_lists[i]];
                for (j = 0; j < rps->nb_refs && rpl_tmp.nb_refs < HEVC_MAX_REFS; j++) {
                    rpl_tmp.list[rpl_tmp.nb_refs]       = rps->list[j];
                    rpl_tmp.ref[rpl_tmp.nb_refs]        = rps->ref[j];
                    rpl_tmp.isLongTerm[rpl_tmp.nb_refs] = i == 2;
                    rpl_tmp.nb_refs++;
                }
            }
        }

        /* reorder the references if necessary */
        if (sh->rpl_modification_flag[list_idx]) {
            for (i = 0; i < sh->nb_refs[list_idx]; i++) {
                int idx = sh->list_entry_lx[list_idx][i];

                if (idx >= rpl_tmp.nb_refs) {
                    av_log(s->avctx, AV_LOG_ERROR, "Invalid reference index.\n");
                    return AVERROR_INVALIDDATA;
                }

                rpl->list[i]       = rpl_tmp.list[idx];
                rpl->ref[i]        = rpl_tmp.ref[idx];
                rpl->isLongTerm[i] = rpl_tmp.isLongTerm[idx];
                rpl->nb_refs++;
            }
        } else {
            memcpy(rpl, &rpl_tmp, sizeof(*rpl));
            rpl->nb_refs = FFMIN(rpl->nb_refs, sh->nb_refs[list_idx]);
        }

        if (sh->collocated_list == list_idx &&
            sh->collocated_ref_idx < rpl->nb_refs)
            s->ref->collocated_ref = rpl->ref[sh->collocated_ref_idx];
    }

    return 0;
}

static HEVCRpiFrame *find_ref_idx(HEVCRpiContext *s, int poc)
{
    int i;
    int LtMask = (1 << s->ps.sps->log2_max_poc_lsb) - 1;

    for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++) {
        HEVCRpiFrame *ref = &s->DPB[i];
        if (ref->frame->buf[0] && (ref->sequence == s->seq_decode)) {
            if ((ref->poc & LtMask) == poc)
                return ref;
        }
    }

    for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++) {
        HEVCRpiFrame *ref = &s->DPB[i];
        if (ref->frame->buf[0] && ref->sequence == s->seq_decode) {
            if (ref->poc == poc || (ref->poc & LtMask) == poc)
                return ref;
        }
    }

    if (s->nal_unit_type != HEVC_NAL_CRA_NUT && !IS_BLA(s))
        av_log(s->avctx, AV_LOG_ERROR,
               "Could not find ref with POC %d\n", poc);
    return NULL;
}

static void mark_ref(HEVCRpiFrame *frame, int flag)
{
    frame->flags &= ~(HEVC_FRAME_FLAG_LONG_REF | HEVC_FRAME_FLAG_SHORT_REF);
    frame->flags |= flag;
}

static HEVCRpiFrame *generate_missing_ref(HEVCRpiContext *s, int poc)
{
    HEVCRpiFrame *frame;
    int i, x, y;

    frame = alloc_frame(s);
    if (!frame)
        return NULL;

    if (!s->ps.sps->pixel_shift) {
        for (i = 0; frame->frame->buf[i]; i++)
            memset(frame->frame->buf[i]->data, 1 << (s->ps.sps->bit_depth - 1),
                   frame->frame->buf[i]->size);
    } else {
        for (i = 0; frame->frame->data[i]; i++)
            for (y = 0; y < (s->ps.sps->height >> s->ps.sps->vshift[i]); y++)
                for (x = 0; x < (s->ps.sps->width >> s->ps.sps->hshift[i]); x++) {
                    AV_WN16(frame->frame->data[i] + y * frame_stride1(frame->frame, 1) + 2 * x,
                            1 << (s->ps.sps->bit_depth - 1));
                }
    }

    frame->poc      = poc;
    frame->sequence = s->seq_decode;
    frame->flags    = 0;

    ff_hevc_rpi_progress_set_all_done(frame);

    return frame;
}

/* add a reference with the given poc to the list and mark it as used in DPB */
static int add_candidate_ref(HEVCRpiContext *s, RefPicList *list,
                             int poc, int ref_flag)
{
    HEVCRpiFrame *ref = find_ref_idx(s, poc);

    if (ref == s->ref || list->nb_refs >= HEVC_MAX_REFS)
        return AVERROR_INVALIDDATA;

    if (!ref) {
        ref = generate_missing_ref(s, poc);
        if (!ref)
            return AVERROR(ENOMEM);
    }

    list->list[list->nb_refs] = ref->poc;
    list->ref[list->nb_refs]  = ref;
    list->nb_refs++;

    mark_ref(ref, ref_flag);
    return 0;
}

int ff_hevc_rpi_frame_rps(HEVCRpiContext *s)
{
    const ShortTermRPS *short_rps = s->sh.short_term_rps;
    const LongTermRPS  *long_rps  = &s->sh.long_term_rps;
    RefPicList               *rps = s->rps;
    int i, ret = 0;

    if (!short_rps) {
        rps[0].nb_refs = rps[1].nb_refs = 0;
        return 0;
    }

    /* clear the reference flags on all frames except the current one */
    for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++) {
        HEVCRpiFrame *frame = &s->DPB[i];

        if (frame == s->ref)
            continue;

        mark_ref(frame, 0);
    }

    for (i = 0; i < NB_RPS_TYPE; i++)
        rps[i].nb_refs = 0;

    /* add the short refs */
    for (i = 0; i < short_rps->num_delta_pocs; i++) {
        int poc = s->poc + short_rps->delta_poc[i];
        int list;

        if (!short_rps->used[i])
            list = ST_FOLL;
        else if (i < short_rps->num_negative_pics)
            list = ST_CURR_BEF;
        else
            list = ST_CURR_AFT;

        ret = add_candidate_ref(s, &rps[list], poc, HEVC_FRAME_FLAG_SHORT_REF);
        if (ret < 0)
            goto fail;
    }

    /* add the long refs */
    for (i = 0; i < long_rps->nb_refs; i++) {
        int poc  = long_rps->poc[i];
        int list = long_rps->used[i] ? LT_CURR : LT_FOLL;

        ret = add_candidate_ref(s, &rps[list], poc, HEVC_FRAME_FLAG_LONG_REF);
        if (ret < 0)
            goto fail;
    }

fail:
    /* release any frames that are now unused */
    for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++)
        ff_hevc_rpi_unref_frame(s, &s->DPB[i], 0);

    return ret;
}

int ff_hevc_rpi_frame_nb_refs(HEVCRpiContext *s)
{
    int ret = 0;
    int i;
    const ShortTermRPS *rps = s->sh.short_term_rps;
    LongTermRPS *long_rps   = &s->sh.long_term_rps;

    if (rps) {
        for (i = 0; i < rps->num_negative_pics; i++)
            ret += !!rps->used[i];
        for (; i < rps->num_delta_pocs; i++)
            ret += !!rps->used[i];
    }

    if (long_rps) {
        for (i = 0; i < long_rps->nb_refs; i++)
            ret += !!long_rps->used[i];
    }
    return ret;
}
