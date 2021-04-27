/*
 * HEVC video decoder
 *
 * Copyright (C) 2012 - 2013 Guillaume Martres
 * Copyright (C) 2013 - 2014 Pierre-Edouard Lepere
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

#include "rpi_hevcdsp.h"
#include "rpi_hevc_mv.h"

static const int8_t transform[32][32] = {
    { 64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,
      64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64 },
    { 90,  90,  88,  85,  82,  78,  73,  67,  61,  54,  46,  38,  31,  22,  13,   4,
      -4, -13, -22, -31, -38, -46, -54, -61, -67, -73, -78, -82, -85, -88, -90, -90 },
    { 90,  87,  80,  70,  57,  43,  25,   9,  -9, -25, -43, -57, -70, -80, -87, -90,
     -90, -87, -80, -70, -57, -43, -25,  -9,   9,  25,  43,  57,  70,  80,  87,  90 },
    { 90,  82,  67,  46,  22,  -4, -31, -54, -73, -85, -90, -88, -78, -61, -38, -13,
      13,  38,  61,  78,  88,  90,  85,  73,  54,  31,   4, -22, -46, -67, -82, -90 },
    { 89,  75,  50,  18, -18, -50, -75, -89, -89, -75, -50, -18,  18,  50,  75,  89,
      89,  75,  50,  18, -18, -50, -75, -89, -89, -75, -50, -18,  18,  50,  75,  89 },
    { 88,  67,  31, -13, -54, -82, -90, -78, -46, -4,   38,  73,  90,  85,  61,  22,
     -22, -61, -85, -90, -73, -38,   4,  46,  78,  90,  82,  54,  13, -31, -67, -88 },
    { 87,  57,   9, -43, -80, -90, -70, -25,  25,  70,  90,  80,  43,  -9, -57, -87,
     -87, -57,  -9,  43,  80,  90,  70,  25, -25, -70, -90, -80, -43,   9,  57,  87 },
    { 85,  46, -13, -67, -90, -73, -22,  38,  82,  88,  54,  -4, -61, -90, -78, -31,
      31,  78,  90,  61,   4, -54, -88, -82, -38,  22,  73,  90,  67,  13, -46, -85 },
    { 83,  36, -36, -83, -83, -36,  36,  83,  83,  36, -36, -83, -83, -36,  36,  83,
      83,  36, -36, -83, -83, -36,  36,  83,  83,  36, -36, -83, -83, -36,  36,  83 },
    { 82,  22, -54, -90, -61,  13,  78,  85,  31, -46, -90, -67,   4,  73,  88,  38,
     -38, -88, -73,  -4,  67,  90,  46, -31, -85, -78, -13,  61,  90,  54, -22, -82 },
    { 80,   9, -70, -87, -25,  57,  90,  43, -43, -90, -57,  25,  87,  70,  -9, -80,
     -80,  -9,  70,  87,  25, -57, -90, -43,  43,  90,  57, -25, -87, -70,   9,  80 },
    { 78,  -4, -82, -73,  13,  85,  67, -22, -88, -61,  31,  90,  54, -38, -90, -46,
      46,  90,  38, -54, -90, -31,  61,  88,  22, -67, -85, -13,  73,  82,   4, -78 },
    { 75, -18, -89, -50,  50,  89,  18, -75, -75,  18,  89,  50, -50, -89, -18,  75,
      75, -18, -89, -50,  50,  89,  18, -75, -75,  18,  89,  50, -50, -89, -18,  75 },
    { 73, -31, -90, -22,  78,  67, -38, -90, -13,  82,  61, -46, -88,  -4,  85,  54,
     -54, -85,   4,  88,  46, -61, -82,  13,  90,  38, -67, -78,  22,  90,  31, -73 },
    { 70, -43, -87,   9,  90,  25, -80, -57,  57,  80, -25, -90,  -9,  87,  43, -70,
     -70,  43,  87,  -9, -90, -25,  80,  57, -57, -80,  25,  90,   9, -87, -43,  70 },
    { 67, -54, -78,  38,  85, -22, -90,   4,  90,  13, -88, -31,  82,  46, -73, -61,
      61,  73, -46, -82,  31,  88, -13, -90,  -4,  90,  22, -85, -38,  78,  54, -67 },
    { 64, -64, -64,  64,  64, -64, -64,  64,  64, -64, -64,  64,  64, -64, -64,  64,
      64, -64, -64,  64,  64, -64, -64,  64,  64, -64, -64,  64,  64, -64, -64,  64 },
    { 61, -73, -46,  82,  31, -88, -13,  90,  -4, -90,  22,  85, -38, -78,  54,  67,
     -67, -54,  78,  38, -85, -22,  90,   4, -90,  13,  88, -31, -82,  46,  73, -61 },
    { 57, -80, -25,  90,  -9, -87,  43,  70, -70, -43,  87,   9, -90,  25,  80, -57,
     -57,  80,  25, -90,   9,  87, -43, -70,  70,  43, -87,  -9,  90, -25, -80,  57 },
    { 54, -85,  -4,  88, -46, -61,  82,  13, -90,  38,  67, -78, -22,  90, -31, -73,
      73,  31, -90,  22,  78, -67, -38,  90, -13, -82,  61,  46, -88,   4,  85, -54 },
    { 50, -89,  18,  75, -75, -18,  89, -50, -50,  89, -18, -75,  75,  18, -89,  50,
      50, -89,  18,  75, -75, -18,  89, -50, -50,  89, -18, -75,  75,  18, -89,  50 },
    { 46, -90,  38,  54, -90,  31,  61, -88,  22,  67, -85,  13,  73, -82,   4,  78,
     -78,  -4,  82, -73, -13,  85, -67, -22,  88, -61, -31,  90, -54, -38,  90, -46 },
    { 43, -90,  57,  25, -87,  70,   9, -80,  80,  -9, -70,  87, -25, -57,  90, -43,
     -43,  90, -57, -25,  87, -70,  -9,  80, -80,   9,  70, -87,  25,  57, -90,  43 },
    { 38, -88,  73,  -4, -67,  90, -46, -31,  85, -78,  13,  61, -90,  54,  22, -82,
      82, -22, -54,  90, -61, -13,  78, -85,  31,  46, -90,  67,   4, -73,  88, -38 },
    { 36, -83,  83, -36, -36,  83, -83,  36,  36, -83,  83, -36, -36,  83, -83,  36,
      36, -83,  83, -36, -36,  83, -83,  36,  36, -83,  83, -36, -36,  83, -83,  36 },
    { 31, -78,  90, -61,   4,  54, -88,  82, -38, -22,  73, -90,  67, -13, -46,  85,
     -85,  46,  13, -67,  90, -73,  22,  38, -82,  88, -54,  -4,  61, -90,  78, -31 },
    { 25, -70,  90, -80,  43,   9, -57,  87, -87,  57,  -9, -43,  80, -90,  70, -25,
     -25,  70, -90,  80, -43,  -9,  57, -87,  87, -57,   9,  43, -80,  90, -70,  25 },
    { 22, -61,  85, -90,  73, -38,  -4,  46, -78,  90, -82,  54, -13, -31,  67, -88,
      88, -67,  31,  13, -54,  82, -90,  78, -46,   4,  38, -73,  90, -85,  61, -22 },
    { 18, -50,  75, -89,  89, -75,  50, -18, -18,  50, -75,  89, -89,  75, -50,  18,
      18, -50,  75, -89,  89, -75,  50, -18, -18,  50, -75,  89, -89,  75, -50,  18 },
    { 13, -38,  61, -78,  88, -90,  85, -73,  54, -31,   4,  22, -46,  67, -82,  90,
     -90,  82, -67,  46, -22,  -4,  31, -54,  73, -85,  90, -88,  78, -61,  38, -13 },
    {  9, -25,  43, -57,  70, -80,  87, -90,  90, -87,  80, -70,  57, -43,  25, -9,
      -9,  25, -43,  57, -70,  80, -87,  90, -90,  87, -80,  70, -57,  43, -25,   9 },
    {  4, -13,  22, -31,  38, -46,  54, -61,  67, -73,  78, -82,  85, -88,  90, -90,
      90, -90,  88, -85,  82, -78,  73, -67,  61, -54,  46, -38,  31, -22,  13,  -4 },
};

DECLARE_ALIGNED(16, const int8_t, ff_hevc_rpi_epel_filters[7][4]) = {
    { -2, 58, 10, -2},
    { -4, 54, 16, -2},
    { -6, 46, 28, -4},
    { -4, 36, 36, -4},
    { -4, 28, 46, -6},
    { -2, 16, 54, -4},
    { -2, 10, 58, -2},
};

DECLARE_ALIGNED(16, const int8_t, ff_hevc_rpi_qpel_filters[3][16]) = {
    { -1,  4,-10, 58, 17, -5,  1,  0, -1,  4,-10, 58, 17, -5,  1,  0},
    { -1,  4,-11, 40, 40,-11,  4, -1, -1,  4,-11, 40, 40,-11,  4, -1},
    {  0,  1, -5, 17, 58,-10,  4, -1,  0,  1, -5, 17, 58,-10,  4, -1}
};

#define BIT_DEPTH 8
#include "rpi_hevcdsp_template.c"
#undef BIT_DEPTH

#define BIT_DEPTH 9
#include "rpi_hevcdsp_template.c"
#undef BIT_DEPTH

#define BIT_DEPTH 10
#include "rpi_hevcdsp_template.c"
#undef BIT_DEPTH

#define BIT_DEPTH 12
#include "rpi_hevcdsp_template.c"
#undef BIT_DEPTH

static uint32_t hevc_deblocking_boundary_strengths(int pus, int dup, const HEVCRpiMvField *curr, const HEVCRpiMvField *neigh,
                                               const int *curr_rpl0, const int *curr_rpl1, const int *neigh_rpl0, const int *neigh_rpl1,
                                               int in_inc0, int in_inc1)
{
    int shift = 32;
    uint32_t bs = 0;
    for (; pus > 0; pus--) {
        int strength, out;
        int curr_refL0 = curr_rpl0[curr->ref_idx[0]];
        int curr_refL1 = curr_rpl1[curr->ref_idx[1]];
        int nr_idx0 = neigh->ref_idx[0];
        int nr_idx1 = neigh->ref_idx[1];
        int neigh_refL0 = neigh_rpl0[nr_idx0];
        int neigh_refL1 = neigh_rpl1[nr_idx1];

        av_assert0(nr_idx0 >= 0 && nr_idx0 <=31);
        av_assert0(nr_idx1 >= 0 && nr_idx1 <=31);

#if 1 // This more directly matches the original implementation
        if (curr->pred_flag == PF_BI &&  neigh->pred_flag == PF_BI) {
            // same L0 and L1
            if (curr_refL0 == neigh_refL0 &&
                curr_refL0 == curr_refL1 &&
                neigh_refL0 == neigh_refL1) {
                if ((FFABS(MV_X(neigh->xy[0]) - MV_X(curr->xy[0])) >= 4 || FFABS(MV_Y(neigh->xy[0]) - MV_Y(curr->xy[0])) >= 4 ||
                     FFABS(MV_X(neigh->xy[1]) - MV_X(curr->xy[1])) >= 4 || FFABS(MV_Y(neigh->xy[1]) - MV_Y(curr->xy[1])) >= 4) &&
                    (FFABS(MV_X(neigh->xy[1]) - MV_X(curr->xy[0])) >= 4 || FFABS(MV_Y(neigh->xy[1]) - MV_Y(curr->xy[0])) >= 4 ||
                     FFABS(MV_X(neigh->xy[0]) - MV_X(curr->xy[1])) >= 4 || FFABS(MV_Y(neigh->xy[0]) - MV_Y(curr->xy[1])) >= 4))
                    strength = 1;
                else
                    strength = 0;
            } else if (neigh_refL0 == curr_refL0 &&
                       neigh_refL1 == curr_refL1) {
                if (FFABS(MV_X(neigh->xy[0]) - MV_X(curr->xy[0])) >= 4 || FFABS(MV_Y(neigh->xy[0]) - MV_Y(curr->xy[0])) >= 4 ||
                    FFABS(MV_X(neigh->xy[1]) - MV_X(curr->xy[1])) >= 4 || FFABS(MV_Y(neigh->xy[1]) - MV_Y(curr->xy[1])) >= 4)
                    strength = 1;
                else
                    strength = 0;
            } else if (neigh_refL1 == curr_refL0 &&
                       neigh_refL0 == curr_refL1) {
                if (FFABS(MV_X(neigh->xy[1]) - MV_X(curr->xy[0])) >= 4 || FFABS(MV_Y(neigh->xy[1]) - MV_Y(curr->xy[0])) >= 4 ||
                    FFABS(MV_X(neigh->xy[0]) - MV_X(curr->xy[1])) >= 4 || FFABS(MV_Y(neigh->xy[0]) - MV_Y(curr->xy[1])) >= 4)
                    strength = 1;
                else
                    strength = 0;
            } else {
                strength = 1;
            }
        } else if ((curr->pred_flag != PF_BI) && (neigh->pred_flag != PF_BI)){ // 1 MV
            MvXY curr_mv0, neigh_mv0;

            if (curr->pred_flag & 1) {
                curr_mv0   = curr->xy[0];
            } else {
                curr_mv0   = curr->xy[1];
                curr_refL0 = curr_refL1;
            }

            if (neigh->pred_flag & 1) {
                neigh_mv0   = neigh->xy[0];
            } else {
                neigh_mv0   = neigh->xy[1];
                neigh_refL0 = neigh_refL1;
            }

            if (curr_refL0 == neigh_refL0) {
                if (FFABS(MV_X(curr_mv0) - MV_X(neigh_mv0)) >= 4 || FFABS(MV_Y(curr_mv0) - MV_Y(neigh_mv0)) >= 4)
                    strength = 1;
                else
                    strength = 0;
            } else
                strength = 1;
        } else
            strength = 1;
#else // This has exactly the same effect, but is more suitable for vectorisation
        MvXY curr_mv[2];
        MvXY neigh_mv[2];
        memcpy(curr_mv, curr->xy, sizeof curr_mv);
        memcpy(neigh_mv, neigh->xy, sizeof neigh_mv);

        if (!(curr->pred_flag & 2)) {
            curr_mv[1] = curr_mv[0];
            curr_refL1 = curr_refL0;
        }
        if (!(neigh->pred_flag & 2)) {
            neigh_mv[1] = neigh_mv[0];
            neigh_refL1 = neigh_refL0;
        }
        if (!(curr->pred_flag & 1)) {
            curr_mv[0] = curr_mv[1];
            curr_refL0 = curr_refL1;
        }
        if (!(neigh->pred_flag & 1)) {
            neigh_mv[0] = neigh_mv[1];
            neigh_refL0 = neigh_refL1;
        }

        strength = 1;

        strength &= (neigh_refL0 != curr_refL0) | (neigh_refL1 != curr_refL1) |
                (FFABS(MV_X(neigh_mv[0]) - MV_X(curr_mv[0])) >= 4) | (FFABS(MV_Y(neigh_mv[0]) - MV_Y(curr_mv[0])) >= 4) |
                (FFABS(MV_X(neigh_mv[1]) - MV_X(curr_mv[1])) >= 4) | (FFABS(MV_Y(neigh_mv[1]) - MV_Y(curr_mv[1])) >= 4);

        strength &= (neigh_refL1 != curr_refL0) | (neigh_refL0 != curr_refL1) |
                (FFABS(MV_X(neigh_mv[1]) - MV_X(curr_mv[0])) >= 4) | (FFABS(MV_Y(neigh_mv[1]) - MV_Y(curr_mv[0])) >= 4) |
                (FFABS(MV_X(neigh_mv[0]) - MV_X(curr_mv[1])) >= 4) | (FFABS(MV_Y(neigh_mv[0]) - MV_Y(curr_mv[1])) >= 4);

        strength |= (((curr->pred_flag + 1) ^ (neigh->pred_flag + 1)) >> 2);
#endif

        curr += in_inc0 / sizeof (HEVCRpiMvField);
        neigh += in_inc1 / sizeof (HEVCRpiMvField);

        for (out = dup; out > 0; out--)
        {
            bs = (bs >> 2) | (strength << 30);
            shift -= 2;
        }
    }
    return bs >> shift;
}


static void cpy_blk(uint8_t *dst, unsigned int stride_dst, const uint8_t *src, unsigned stride_src, unsigned int width, unsigned int height)
{
    unsigned int i, j;

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



void ff_hevc_rpi_dsp_init(HEVCDSPContext *hevcdsp, int bit_depth)
{
#undef FUNC
#define FUNC(a, depth) a ## _ ## depth

#undef PEL_FUNC
#define PEL_FUNC(dst1, idx1, idx2, a, depth)                                   \
    for(i = 0 ; i < 10 ; i++)                                                  \
{                                                                              \
    hevcdsp->dst1[i][idx1][idx2] = a ## _ ## depth;                            \
}

#undef EPEL_FUNCS
#define EPEL_FUNCS(depth)                                                     \
    PEL_FUNC(put_hevc_epel, 0, 0, put_hevc_pel_pixels, depth);                \
    PEL_FUNC(put_hevc_epel, 0, 1, put_hevc_epel_h, depth);                    \
    PEL_FUNC(put_hevc_epel, 1, 0, put_hevc_epel_v, depth);                    \
    PEL_FUNC(put_hevc_epel, 1, 1, put_hevc_epel_hv, depth)

#undef EPEL_UNI_FUNCS
#define EPEL_UNI_FUNCS(depth)                                                 \
    PEL_FUNC(put_hevc_epel_uni, 0, 0, put_hevc_pel_uni_pixels, depth);        \
    PEL_FUNC(put_hevc_epel_uni, 0, 1, put_hevc_epel_uni_h, depth);            \
    PEL_FUNC(put_hevc_epel_uni, 1, 0, put_hevc_epel_uni_v, depth);            \
    PEL_FUNC(put_hevc_epel_uni, 1, 1, put_hevc_epel_uni_hv, depth);           \
    PEL_FUNC(put_hevc_epel_uni_w, 0, 0, put_hevc_pel_uni_w_pixels, depth);    \
    PEL_FUNC(put_hevc_epel_uni_w, 0, 1, put_hevc_epel_uni_w_h, depth);        \
    PEL_FUNC(put_hevc_epel_uni_w, 1, 0, put_hevc_epel_uni_w_v, depth);        \
    PEL_FUNC(put_hevc_epel_uni_w, 1, 1, put_hevc_epel_uni_w_hv, depth)

#undef EPEL_BI_FUNCS
#define EPEL_BI_FUNCS(depth)                                                \
    PEL_FUNC(put_hevc_epel_bi, 0, 0, put_hevc_pel_bi_pixels, depth);        \
    PEL_FUNC(put_hevc_epel_bi, 0, 1, put_hevc_epel_bi_h, depth);            \
    PEL_FUNC(put_hevc_epel_bi, 1, 0, put_hevc_epel_bi_v, depth);            \
    PEL_FUNC(put_hevc_epel_bi, 1, 1, put_hevc_epel_bi_hv, depth);           \
    PEL_FUNC(put_hevc_epel_bi_w, 0, 0, put_hevc_pel_bi_w_pixels, depth);    \
    PEL_FUNC(put_hevc_epel_bi_w, 0, 1, put_hevc_epel_bi_w_h, depth);        \
    PEL_FUNC(put_hevc_epel_bi_w, 1, 0, put_hevc_epel_bi_w_v, depth);        \
    PEL_FUNC(put_hevc_epel_bi_w, 1, 1, put_hevc_epel_bi_w_hv, depth)

#undef QPEL_FUNCS
#define QPEL_FUNCS(depth)                                                     \
    PEL_FUNC(put_hevc_qpel, 0, 0, put_hevc_pel_pixels, depth);                \
    PEL_FUNC(put_hevc_qpel, 0, 1, put_hevc_qpel_h, depth);                    \
    PEL_FUNC(put_hevc_qpel, 1, 0, put_hevc_qpel_v, depth);                    \
    PEL_FUNC(put_hevc_qpel, 1, 1, put_hevc_qpel_hv, depth)

#undef QPEL_UNI_FUNCS
#define QPEL_UNI_FUNCS(depth)                                                 \
    PEL_FUNC(put_hevc_qpel_uni, 0, 0, put_hevc_pel_uni_pixels, depth);        \
    PEL_FUNC(put_hevc_qpel_uni, 0, 1, put_hevc_qpel_uni_h, depth);            \
    PEL_FUNC(put_hevc_qpel_uni, 1, 0, put_hevc_qpel_uni_v, depth);            \
    PEL_FUNC(put_hevc_qpel_uni, 1, 1, put_hevc_qpel_uni_hv, depth);           \
    PEL_FUNC(put_hevc_qpel_uni_w, 0, 0, put_hevc_pel_uni_w_pixels, depth);    \
    PEL_FUNC(put_hevc_qpel_uni_w, 0, 1, put_hevc_qpel_uni_w_h, depth);        \
    PEL_FUNC(put_hevc_qpel_uni_w, 1, 0, put_hevc_qpel_uni_w_v, depth);        \
    PEL_FUNC(put_hevc_qpel_uni_w, 1, 1, put_hevc_qpel_uni_w_hv, depth)

#undef QPEL_BI_FUNCS
#define QPEL_BI_FUNCS(depth)                                                  \
    PEL_FUNC(put_hevc_qpel_bi, 0, 0, put_hevc_pel_bi_pixels, depth);          \
    PEL_FUNC(put_hevc_qpel_bi, 0, 1, put_hevc_qpel_bi_h, depth);              \
    PEL_FUNC(put_hevc_qpel_bi, 1, 0, put_hevc_qpel_bi_v, depth);              \
    PEL_FUNC(put_hevc_qpel_bi, 1, 1, put_hevc_qpel_bi_hv, depth);             \
    PEL_FUNC(put_hevc_qpel_bi_w, 0, 0, put_hevc_pel_bi_w_pixels, depth);      \
    PEL_FUNC(put_hevc_qpel_bi_w, 0, 1, put_hevc_qpel_bi_w_h, depth);          \
    PEL_FUNC(put_hevc_qpel_bi_w, 1, 0, put_hevc_qpel_bi_w_v, depth);          \
    PEL_FUNC(put_hevc_qpel_bi_w, 1, 1, put_hevc_qpel_bi_w_hv, depth)

#define SLICED_ADD_RESIDUAL(depth)\
    hevcdsp->add_residual_u[0]      = FUNC(add_residual4x4_u, depth);         \
    hevcdsp->add_residual_u[1]      = FUNC(add_residual8x8_u, depth);         \
    hevcdsp->add_residual_u[2]      = FUNC(add_residual16x16_u, depth);       \
    hevcdsp->add_residual_u[3]      = FUNC(add_residual32x32_u, depth);       \
    hevcdsp->add_residual_v[0]      = FUNC(add_residual4x4_v, depth);         \
    hevcdsp->add_residual_v[1]      = FUNC(add_residual8x8_v, depth);         \
    hevcdsp->add_residual_v[2]      = FUNC(add_residual16x16_v, depth);       \
    hevcdsp->add_residual_v[3]      = FUNC(add_residual32x32_v, depth);       \
    hevcdsp->add_residual_c[0]      = FUNC(add_residual4x4_c, depth);         \
    hevcdsp->add_residual_c[1]      = FUNC(add_residual8x8_c, depth);         \
    hevcdsp->add_residual_c[2]      = FUNC(add_residual16x16_c, depth);       \
    hevcdsp->add_residual_c[3]      = FUNC(add_residual32x32_c, depth);       \
    hevcdsp->add_residual_dc_c[0]   = FUNC(add_residual4x4_dc_c, depth);         \
    hevcdsp->add_residual_dc_c[1]   = FUNC(add_residual8x8_dc_c, depth);         \
    hevcdsp->add_residual_dc_c[2]   = FUNC(add_residual16x16_dc_c, depth);       \
    hevcdsp->add_residual_dc_c[3]   = FUNC(add_residual32x32_dc_c, depth);       \
    hevcdsp->put_pcm_c              = FUNC(put_pcm_c, depth)
#define SLICED_LOOP_FILTERS(depth)\
    hevcdsp->hevc_h_loop_filter_luma2 = FUNC(hevc_h_loop_filter_luma2, depth); \
    hevcdsp->hevc_v_loop_filter_luma2 = FUNC(hevc_v_loop_filter_luma2, depth); \
    hevcdsp->hevc_h_loop_filter_uv    = FUNC(hevc_h_loop_filter_uv, depth);    \
    hevcdsp->hevc_v_loop_filter_uv2   = FUNC(hevc_v_loop_filter_uv2, depth)
#define SLICED_SAO(depth)\
    for (i = 0; i != SAO_FILTER_N; ++i) {                                     \
        hevcdsp->sao_band_filter_c[i] = FUNC(sao_band_filter_c, depth);       \
        hevcdsp->sao_edge_filter_c[i] = FUNC(sao_edge_filter_c, depth);       \
    }                                                                         \
    hevcdsp->sao_edge_restore_c[0] = FUNC(sao_edge_restore_c_0, depth);       \
    hevcdsp->sao_edge_restore_c[1] = FUNC(sao_edge_restore_c_1, depth)

#define HEVC_DSP(depth)                                                     \
    hevcdsp->put_pcm                = FUNC(put_pcm, depth);                 \
    hevcdsp->add_residual[0]        = FUNC(add_residual4x4, depth);         \
    hevcdsp->add_residual[1]        = FUNC(add_residual8x8, depth);         \
    hevcdsp->add_residual[2]        = FUNC(add_residual16x16, depth);       \
    hevcdsp->add_residual[3]        = FUNC(add_residual32x32, depth);       \
    hevcdsp->add_residual_dc[0]     = FUNC(add_residual4x4_dc, depth);         \
    hevcdsp->add_residual_dc[1]     = FUNC(add_residual8x8_dc, depth);         \
    hevcdsp->add_residual_dc[2]     = FUNC(add_residual16x16_dc, depth);       \
    hevcdsp->add_residual_dc[3]     = FUNC(add_residual32x32_dc, depth);       \
    SLICED_ADD_RESIDUAL(depth);                                             \
    hevcdsp->dequant                = FUNC(dequant, depth);                 \
    hevcdsp->transform_rdpcm        = FUNC(transform_rdpcm, depth);         \
    hevcdsp->transform_4x4_luma     = FUNC(transform_4x4_luma, depth);      \
    hevcdsp->idct[0]                = FUNC(idct_4x4, depth);                \
    hevcdsp->idct[1]                = FUNC(idct_8x8, depth);                \
    hevcdsp->idct[2]                = FUNC(idct_16x16, depth);              \
    hevcdsp->idct[3]                = FUNC(idct_32x32, depth);              \
                                                                            \
    hevcdsp->idct_dc[0]             = FUNC(idct_4x4_dc, depth);             \
    hevcdsp->idct_dc[1]             = FUNC(idct_8x8_dc, depth);             \
    hevcdsp->idct_dc[2]             = FUNC(idct_16x16_dc, depth);           \
    hevcdsp->idct_dc[3]             = FUNC(idct_32x32_dc, depth);           \
                                                                            \
    for (i = 0; i != SAO_FILTER_N; ++i) {                                   \
        hevcdsp->sao_band_filter[i] = FUNC(sao_band_filter, depth);         \
        hevcdsp->sao_edge_filter[i] = FUNC(sao_edge_filter, depth);         \
    }                                                                       \
    hevcdsp->sao_edge_restore[0] = FUNC(sao_edge_restore_0, depth);            \
    hevcdsp->sao_edge_restore[1] = FUNC(sao_edge_restore_1, depth);            \
    SLICED_SAO(depth);                                                         \
                                                                               \
    QPEL_FUNCS(depth);                                                         \
    QPEL_UNI_FUNCS(depth);                                                     \
    QPEL_BI_FUNCS(depth);                                                      \
    EPEL_FUNCS(depth);                                                         \
    EPEL_UNI_FUNCS(depth);                                                     \
    EPEL_BI_FUNCS(depth);                                                      \
                                                                               \
    SLICED_LOOP_FILTERS(depth);                                                \
    hevcdsp->hevc_h_loop_filter_luma     = FUNC(hevc_h_loop_filter_luma, depth);   \
    hevcdsp->hevc_v_loop_filter_luma     = FUNC(hevc_v_loop_filter_luma, depth);   \
    hevcdsp->hevc_h_loop_filter_chroma   = FUNC(hevc_h_loop_filter_chroma, depth); \
    hevcdsp->hevc_v_loop_filter_chroma   = FUNC(hevc_v_loop_filter_chroma, depth); \
    hevcdsp->hevc_h_loop_filter_luma_c   = FUNC(hevc_h_loop_filter_luma, depth);   \
    hevcdsp->hevc_v_loop_filter_luma_c   = FUNC(hevc_v_loop_filter_luma, depth);   \
    hevcdsp->hevc_h_loop_filter_chroma_c = FUNC(hevc_h_loop_filter_chroma, depth); \
    hevcdsp->hevc_v_loop_filter_chroma_c = FUNC(hevc_v_loop_filter_chroma, depth)
int i = 0;

    switch (bit_depth) {
    case 9:
        HEVC_DSP(9);
        break;
    case 10:
        HEVC_DSP(10);
        break;
    case 12:
        HEVC_DSP(12);
        break;
    default:
        HEVC_DSP(8);
        break;
    }

    hevcdsp->hevc_deblocking_boundary_strengths = hevc_deblocking_boundary_strengths;
    hevcdsp->cpy_blk = cpy_blk;

    if (ARCH_PPC)
        ff_hevc_rpi_dsp_init_ppc(hevcdsp, bit_depth);
    if (ARCH_X86)
        ff_hevc_rpi_dsp_init_x86(hevcdsp, bit_depth);
    if (ARCH_ARM)
        ff_hevcdsp_rpi_init_arm(hevcdsp, bit_depth);
    if (ARCH_MIPS)
        ff_hevc_rpi_dsp_init_mips(hevcdsp, bit_depth);
}
