/*
 * H.26L/H.264/AVC/JVT/14496-10/... encoder/decoder
 * Copyright (c) 2003 Michael Niedermayer <michaelni@gmx.at>
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

/**
 * @file
 * Context Adaptive Binary Arithmetic Coder inline functions
 */

#ifndef AVCODEC_CABAC_FUNCTIONS_H
#define AVCODEC_CABAC_FUNCTIONS_H

#include <stdint.h>

#include "cabac.h"
#include "config.h"

#ifndef UNCHECKED_BITSTREAM_READER
#define UNCHECKED_BITSTREAM_READER !CONFIG_SAFE_BITSTREAM_READER
#endif

#if ARCH_AARCH64
#   include "aarch64/cabac.h"
#endif
#if ARCH_ARM
#   include "arm/cabac.h"
#endif
#if ARCH_X86
#   include "x86/cabac.h"
#endif

static CABAC_TABLE_CONST uint8_t * const ff_h264_norm_shift = ff_h264_cabac_tables + H264_NORM_SHIFT_OFFSET;
static CABAC_TABLE_CONST uint8_t * const ff_h264_lps_range = ff_h264_cabac_tables + H264_LPS_RANGE_OFFSET;
static CABAC_TABLE_CONST uint8_t * const ff_h264_mlps_state = ff_h264_cabac_tables + H264_MLPS_STATE_OFFSET;
static CABAC_TABLE_CONST uint8_t * const ff_h264_last_coeff_flag_offset_8x8 = ff_h264_cabac_tables + H264_LAST_COEFF_FLAG_OFFSET_8x8_OFFSET;

#define JC_CABAC 1


#if !JC_CABAC
static void refill(CABACContext *c){
#if CABAC_BITS == 16
        c->low+= (c->bytestream[0]<<9) + (c->bytestream[1]<<1);
#else
        c->low+= c->bytestream[0]<<1;
#endif
    c->low -= CABAC_MASK;
#if !UNCHECKED_BITSTREAM_READER
    if (c->bytestream < c->bytestream_end)
#endif
        c->bytestream += CABAC_BITS / 8;
}

static inline void renorm_cabac_decoder_once(CABACContext *c){
    int shift= (uint32_t)(c->range - 0x100)>>31;
    c->range<<= shift;
    c->low  <<= shift;
    c->b_offset += shift;
    if(!(c->low & CABAC_MASK))
        refill(c);
}

#ifndef get_cabac_inline
static void refill2(CABACContext *c){
    int i, x;

    x= c->low ^ (c->low-1);
    i= 7 - ff_h264_norm_shift[x>>(CABAC_BITS-1)];

    x= -CABAC_MASK;

#if CABAC_BITS == 16
        x+= (c->bytestream[0]<<9) + (c->bytestream[1]<<1);
#else
        x+= c->bytestream[0]<<1;
#endif

    c->low += x<<i;
#if !UNCHECKED_BITSTREAM_READER
    if (c->bytestream < c->bytestream_end)
#endif
        c->bytestream += CABAC_BITS/8;
}
#endif
#endif


#if JC_CABAC
static av_always_inline int get_cabac_inline(CABACContext *c, uint8_t * const state){
    int s = *state;
    unsigned int RangeLPS= ff_h264_lps_range[2*(c->codIRange&0xC0) + s];
    int bit, lps_mask;
    const unsigned int next_bits = bmem_peek4(c->bytestream_start, c->b_offset);

    c->codIRange -= RangeLPS;
    lps_mask= (int)(c->codIRange - (c->codIOffset + 1))>>31;

    c->codIOffset -= c->codIRange & lps_mask;
    c->codIRange += (RangeLPS - c->codIRange) & lps_mask;

    s^=lps_mask;
    *state= (ff_h264_mlps_state+128)[s];
    bit= s&1;

    {
        unsigned int n = lmbd1(c->codIRange) - 23;
        if (n != 0) {
            c->codIRange = (c->codIRange << n);
            c->codIOffset = (c->codIOffset << n) | ((next_bits << (c->b_offset & 7)) >> (32 - n));
            c->b_offset += n;
        }

//        printf("bit=%d, n=%d, range=%d, offset=%d, state=%d, nxt=%08x\n", bit, n, c->codIRange, c->codIOffset, *state, next_bits);
    }

    return bit;
}
#else
static av_always_inline int get_cabac_inline(CABACContext *c, uint8_t * const state){
    int s = *state;
    int RangeLPS= ff_h264_lps_range[2*(c->range&0xC0) + s];
    int bit, lps_mask;

    c->range -= RangeLPS;
    lps_mask= ((c->range<<(CABAC_BITS+1)) - c->low)>>31;

    c->low -= (c->range<<(CABAC_BITS+1)) & lps_mask;
    c->range += (RangeLPS - c->range) & lps_mask;

    s^=lps_mask;
    *state= (ff_h264_mlps_state+128)[s];
    bit= s&1;

//    lps_mask= ff_h264_norm_shift[c->range];
    lps_mask= lmbd1(c->range) - 23;
    c->range<<= lps_mask;
    c->low  <<= lps_mask;

    c->b_offset += lps_mask;

    printf("bit=%d, n=%d, range=%d, offset=%d, state=%d\n", bit, lps_mask, c->range, c->low >> (CABAC_BITS+1), *state);

    if(!(c->low & CABAC_MASK))
        refill2(c);
    return bit;
}
#endif

static int av_noinline av_unused get_cabac_noinline(CABACContext *c, uint8_t * const state){
    return get_cabac_inline(c,state);
}

static int av_unused get_cabac(CABACContext *c, uint8_t * const state){
    return get_cabac_inline(c,state);
}

#ifndef get_cabac_bypass
#if JC_CABAC
static int av_unused get_cabac_bypass(CABACContext *c){
    c->codIOffset = (c->codIOffset << 1) |
        ((c->bytestream_start[c->b_offset >> 3] >> (~c->b_offset & 7)) & 1);
    ++c->b_offset;
    if (c->codIOffset < c->codIRange) {
//        printf("bypass 0: o=%u, r=%u\n", c->codIOffset, c->codIRange);
        return 0;
    }
    c->codIOffset -= c->codIRange;
//    printf("bypass 1: o=%u, r=%u\n", c->codIOffset, c->codIRange);
    return 1;
}
#else
static int av_unused get_cabac_bypass(CABACContext *c){
    int range;
    c->low += c->low;
    ++c->b_offset;

    if(!(c->low & CABAC_MASK))
        refill(c);

    range= c->range<<(CABAC_BITS+1);

    if(c->low < range){
        printf("bypass 0: o=%u, r=%u\n", c->low >> (CABAC_BITS + 1), c->range);
        return 0;
    }else{
        c->low -= range;
        printf("bypass 1: o=%u, r=%u\n", c->low >> (CABAC_BITS + 1), c->range);
        return 1;
    }
}
#endif
#endif

#ifndef get_cabac_bypass_sign
#if 1
//#if JC_CABAC
static av_always_inline int get_cabac_bypass_sign(CABACContext *c, int val){
    return get_cabac_bypass(c) ? val : -val;
}
#else
static av_always_inline int get_cabac_bypass_sign(CABACContext *c, int val){
    int range, mask;
    c->low += c->low;
    ++c->b_offset;

    if(!(c->low & CABAC_MASK))
        refill(c);

    range= c->range<<(CABAC_BITS+1);
    c->low -= range;
    mask= c->low >> 31;
    range &= mask;
    c->low += range;
    return (val^mask)-mask;
}
#endif
#endif

/**
 *
 * @return the number of bytes read or 0 if no end
 */
#if JC_CABAC
static int av_unused get_cabac_terminate(CABACContext *c){
    c->codIRange -= 2;
    if (c->codIOffset >= c->codIRange) {
        return (c->b_offset + 7) >> 3;
    }

    // renorm
    {
        int n = (int)lmbd1(c->codIRange) - 23;
        if (n > 0)
        {
            const unsigned int next_bits = bmem_peek4(c->bytestream_start, c->b_offset);
            c->codIRange <<= n;
            c->codIOffset = (c->codIOffset << n) | ((next_bits << (c->b_offset & 7)) >> (32 - n));
            c->b_offset += n;
        }
    }
    return 0;
}
#else
static int av_unused get_cabac_terminate(CABACContext *c){
    c->range -= 2;
    if(c->low < c->range<<(CABAC_BITS+1)){
        renorm_cabac_decoder_once(c);
        return 0;
    }else{
        printf("b1=%d/%d, b_jc=%d\n", c->bytestream - c->bytestream_start, c->bytestream_end - c->bytestream_start, (c->b_offset + 7) >> 3);
        return c->bytestream - c->bytestream_start;
    }
}
#endif

/**
 * Skip @p n bytes and reset the decoder.
 * @return the address of the first skipped byte or NULL if there's less than @p n bytes left
 */
#if JC_CABAC
static av_unused const uint8_t* skip_bytes(CABACContext *c, int n) {
    const uint8_t *ptr = c->bytestream + ((c->b_offset + 7) >> 3);

    if ((int) (c->bytestream_end - ptr) < n)
        return NULL;
    ff_init_cabac_decoder(c, ptr + n, c->bytestream_end - ptr - n);

    return ptr;
}
#else
static av_unused const uint8_t* skip_bytes(CABACContext *c, int n) {
    const uint8_t *ptr = c->bytestream;

    if (c->low & 0x1)
        ptr--;
#if CABAC_BITS == 16
    if (c->low & 0x1FF)
        ptr--;
#endif
    if ((int) (c->bytestream_end - ptr) < n)
        return NULL;
    ff_init_cabac_decoder(c, ptr + n, c->bytestream_end - ptr - n);

    return ptr;
}
#endif

#endif /* AVCODEC_CABAC_FUNCTIONS_H */
