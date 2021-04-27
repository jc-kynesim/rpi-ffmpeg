/*
 * HEVC video Decoder
 *
 * Copyright (C) 2012 - 2013 Guillaume Martres
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

#ifndef AVCODEC_RPI_HEVCPRED_H
#define AVCODEC_RPI_HEVCPRED_H

#include <stddef.h>
#include <stdint.h>
#include "config.h"

struct HEVCRpiContext;
struct HEVCRpiLocalContext;

enum IntraPredMode {
    INTRA_PLANAR = 0,
    INTRA_DC,
    INTRA_ANGULAR_2,
    INTRA_ANGULAR_3,
    INTRA_ANGULAR_4,
    INTRA_ANGULAR_5,
    INTRA_ANGULAR_6,
    INTRA_ANGULAR_7,
    INTRA_ANGULAR_8,
    INTRA_ANGULAR_9,
    INTRA_ANGULAR_10,
    INTRA_ANGULAR_11,
    INTRA_ANGULAR_12,
    INTRA_ANGULAR_13,
    INTRA_ANGULAR_14,
    INTRA_ANGULAR_15,
    INTRA_ANGULAR_16,
    INTRA_ANGULAR_17,
    INTRA_ANGULAR_18,
    INTRA_ANGULAR_19,
    INTRA_ANGULAR_20,
    INTRA_ANGULAR_21,
    INTRA_ANGULAR_22,
    INTRA_ANGULAR_23,
    INTRA_ANGULAR_24,
    INTRA_ANGULAR_25,
    INTRA_ANGULAR_26,
    INTRA_ANGULAR_27,
    INTRA_ANGULAR_28,
    INTRA_ANGULAR_29,
    INTRA_ANGULAR_30,
    INTRA_ANGULAR_31,
    INTRA_ANGULAR_32,
    INTRA_ANGULAR_33,
    INTRA_ANGULAR_34,
};
#define INTRA_ANGULAR_HORIZONTAL INTRA_ANGULAR_10
#define INTRA_ANGULAR_VERTICAL   INTRA_ANGULAR_26

typedef void intra_filter_fn_t(
        uint8_t * const left, uint8_t * const top,
        const unsigned int req, const unsigned int avail,
        const uint8_t * const src_l, const uint8_t * const src_u, const uint8_t * const src_ur,
        const unsigned int stride,
        const unsigned int top_right_size, const unsigned int down_left_size);

typedef struct HEVCRpiPredContext {
    void (*intra_pred)(const struct HEVCRpiContext * const s,
                          const enum IntraPredMode mode, const unsigned int x0, const unsigned int y0,
                          const unsigned int avail, const unsigned int log2_size);

    intra_filter_fn_t *intra_filter[4];
    void (*pred_planar[4])(uint8_t *src, const uint8_t *top,
                           const uint8_t *left, ptrdiff_t stride);
    void (*pred_dc[4])(uint8_t *src, const uint8_t *top, const uint8_t *left,
                    ptrdiff_t stride);
    void (*pred_angular[4])(uint8_t *src, const uint8_t *top,
                            const uint8_t *left, ptrdiff_t stride,
                            int mode);
    void (*pred_vertical[4])(uint8_t *src, const uint8_t *top,
                            const uint8_t *left, ptrdiff_t stride,
                            int mode);
    void (*pred_horizontal[4])(uint8_t *src, const uint8_t *top,
                            const uint8_t *left, ptrdiff_t stride,
                            int mode);
    void (*pred_dc0[4])(uint8_t *src, ptrdiff_t stride);

    void (*intra_pred_c)(const struct HEVCRpiContext * const s,
                          const enum IntraPredMode mode, const unsigned int x0, const unsigned int y0,
                          const unsigned int avail, const unsigned int log2_size);
    intra_filter_fn_t *intra_filter_c[4];
    void (*pred_planar_c[4])(uint8_t *src, const uint8_t *top,
                           const uint8_t *left, ptrdiff_t stride);
    void (*pred_dc_c[4])(uint8_t *src, const uint8_t *top, const uint8_t *left,
                    ptrdiff_t stride);
    void (*pred_angular_c[4])(uint8_t *src, const uint8_t *top,
                            const uint8_t *left, ptrdiff_t stride,
                            int mode);
    void (*pred_vertical_c[4])(uint8_t *src, const uint8_t *top,
                            const uint8_t *left, ptrdiff_t stride,
                            int mode);
    void (*pred_horizontal_c[4])(uint8_t *src, const uint8_t *top,
                            const uint8_t *left, ptrdiff_t stride,
                            int mode);
    void (*pred_dc0_c[4])(uint8_t *src, ptrdiff_t stride);
} HEVCRpiPredContext;

void ff_hevc_rpi_pred_init(HEVCRpiPredContext *hpc, int bit_depth);

#endif /* AVCODEC_RPI_HEVCPRED_H */
