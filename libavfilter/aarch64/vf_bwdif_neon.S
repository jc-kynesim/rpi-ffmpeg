/*
 * bwdif aarch64 NEON optimisations
 *
 * Copyright (c) 2023 John Cox <jc@kynesim.co.uk>
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


#include "libavutil/aarch64/asm.S"

.macro SQSHRUNN b, s0, s1, s2, s3, n
        sqshrun         \s0\().4h, \s0\().4s, #\n - 8
        sqshrun2        \s0\().8h, \s1\().4s, #\n - 8
        sqshrun         \s1\().4h, \s2\().4s, #\n - 8
        sqshrun2        \s1\().8h, \s3\().4s, #\n - 8
        uzp2            \b\().16b, \s0\().16b, \s1\().16b
.endm

.macro SMULL4K a0, a1, a2, a3, s0, s1, k
        smull           \a0\().4s, \s0\().4h, \k
        smull2          \a1\().4s, \s0\().8h, \k
        smull           \a2\().4s, \s1\().4h, \k
        smull2          \a3\().4s, \s1\().8h, \k
.endm

.macro UMULL4K a0, a1, a2, a3, s0, s1, k
        umull           \a0\().4s, \s0\().4h, \k
        umull2          \a1\().4s, \s0\().8h, \k
        umull           \a2\().4s, \s1\().4h, \k
        umull2          \a3\().4s, \s1\().8h, \k
.endm

.macro UMLAL4K a0, a1, a2, a3, s0, s1, k
        umlal           \a0\().4s, \s0\().4h, \k
        umlal2          \a1\().4s, \s0\().8h, \k
        umlal           \a2\().4s, \s1\().4h, \k
        umlal2          \a3\().4s, \s1\().8h, \k
.endm

.macro UMLSL4K a0, a1, a2, a3, s0, s1, k
        umlsl           \a0\().4s, \s0\().4h, \k
        umlsl2          \a1\().4s, \s0\().8h, \k
        umlsl           \a2\().4s, \s1\().4h, \k
        umlsl2          \a3\().4s, \s1\().8h, \k
.endm

// static const uint16_t coef_lf[2] = { 4309, 213 };
// static const uint16_t coef_hf[3] = { 5570, 3801, 1016 };
// static const uint16_t coef_sp[2] = { 5077, 981 };

        .align 16
coeffs:
        .hword          4309 * 4, 213 * 4               // lf[0]*4 = v0.h[0]
        .hword          5570, 3801, 1016, -3801         // hf[0] = v0.h[2], -hf[1] = v0.h[5]
        .hword          5077, 981                       // sp[0] = v0.h[6]

