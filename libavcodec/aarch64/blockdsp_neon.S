/*
 * AArch64 NEON optimised block operations
 *
 * Copyright (c) 2022 Ben Avison <bavison@riscosopen.org>
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

function ff_clear_block_neon, export=1
        movi            v0.16b, #0
        movi            v1.16b, #0
        st1             {v0.16b, v1.16b}, [x0], #32
        st1             {v0.16b, v1.16b}, [x0], #32
        st1             {v0.16b, v1.16b}, [x0], #32
        st1             {v0.16b, v1.16b}, [x0]
        ret
endfunc

function ff_clear_blocks_neon, export=1
        movi            v0.16b, #0
        movi            v1.16b, #0
        .rept           23
        st1             {v0.16b, v1.16b}, [x0], #32
        .endr
        st1             {v0.16b, v1.16b}, [x0]
        ret
endfunc
