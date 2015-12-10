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


#if JC_CABAC
#define I(x) (uint32_t)((0x10000000000ULL / (uint64_t)(x)) + 1ULL)

static const uint32_t inv_range[256] = {
                                                    0,      I(257), I(258), I(259),
    I(260), I(261), I(262), I(263), I(264), I(265), I(266), I(267), I(268), I(269),
    I(270), I(271), I(272), I(273), I(274), I(275), I(276), I(277), I(278), I(279),
    I(280), I(281), I(282), I(283), I(284), I(285), I(286), I(287), I(288), I(289),
    I(290), I(291), I(292), I(293), I(294), I(295), I(296), I(297), I(298), I(299),
    I(300), I(301), I(302), I(303), I(304), I(305), I(306), I(307), I(308), I(309),
    I(310), I(311), I(312), I(313), I(314), I(315), I(316), I(317), I(318), I(319),
    I(320), I(321), I(322), I(323), I(324), I(325), I(326), I(327), I(328), I(329),
    I(330), I(331), I(332), I(333), I(334), I(335), I(336), I(337), I(338), I(339),
    I(340), I(341), I(342), I(343), I(344), I(345), I(346), I(347), I(348), I(349),
    I(350), I(351), I(352), I(353), I(354), I(355), I(356), I(357), I(358), I(359),
    I(360), I(361), I(362), I(363), I(364), I(365), I(366), I(367), I(368), I(369),
    I(370), I(371), I(372), I(373), I(374), I(375), I(376), I(377), I(378), I(379),
    I(380), I(381), I(382), I(383), I(384), I(385), I(386), I(387), I(388), I(389),
    I(390), I(391), I(392), I(393), I(394), I(395), I(396), I(397), I(398), I(399),
    I(400), I(401), I(402), I(403), I(404), I(405), I(406), I(407), I(408), I(409),
    I(410), I(411), I(412), I(413), I(414), I(415), I(416), I(417), I(418), I(419),
    I(420), I(421), I(422), I(423), I(424), I(425), I(426), I(427), I(428), I(429),
    I(430), I(431), I(432), I(433), I(434), I(435), I(436), I(437), I(438), I(439),
    I(440), I(441), I(442), I(443), I(444), I(445), I(446), I(447), I(448), I(449),
    I(450), I(451), I(452), I(453), I(454), I(455), I(456), I(457), I(458), I(459),
    I(460), I(461), I(462), I(463), I(464), I(465), I(466), I(467), I(468), I(469),
    I(470), I(471), I(472), I(473), I(474), I(475), I(476), I(477), I(478), I(479),
    I(480), I(481), I(482), I(483), I(484), I(485), I(486), I(487), I(488), I(489),
    I(490), I(491), I(492), I(493), I(494), I(495), I(496), I(497), I(498), I(499),
    I(500), I(501), I(502), I(503), I(504), I(505), I(506), I(507), I(508), I(509),
    I(510), I(511)
};
#undef I
#endif




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
    c->range<<= shift;,
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

//    printf("bit=%d, n=%d, range=%d, offset=%d, state=%d\n", bit, lps_mask, c->range, c->low >> (CABAC_BITS+1), *state);

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
static inline int get_cabac_bypass(CABACContext *c){
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

static inline uint32_t get_cabac_bypeek22(CABACContext * c, uint32_t * pX)
{
    const uint32_t nb = bmem_peek4(c->bytestream_start, c->b_offset) << (c->b_offset & 7);
    uint32_t x = (c->codIOffset << 23) | ((nb >> 9) & ~1U);
    const uint32_t y = (inv_range - 256)[c->codIRange];
    *pX = x;

    if (c->codIRange != 256) {
//        printf("x=%08x, y=%08x, r=%d\n", x, y, c->codIRange);
        x = (uint32_t)(((uint64_t)x * (uint64_t)y) >> 32);
    }
    x <<= 1;
#if 0
    {
        char bits[33];
        unsigned int i;
        for (i = 0; i != 23; ++i) {
            bits[i] = '0' + ((x >> (31 - i)) & 1);
        }
        bits[i] = 0;
        printf("---- %s\n", bits);
    }
#endif
    return x;
}

static inline void get_cabac_byflush(CABACContext * c, const unsigned int n, const uint32_t val, const uint32_t x)
{
    c->b_offset += n;
    c->codIOffset =
        ((x >> (23 - n)) -
         (((val >> (32 - n)) & 0x1ff) * c->codIRange)) & 0x1ff;
}

#else
static int av_unused get_cabac_bypass(CABACContext *c){
    int range;
    c->low += c->low;

    if(!(c->low & CABAC_MASK))
        refill(c);

    range= c->range<<(CABAC_BITS+1);

    if(c->low < range){
//        printf("bypass 0: o=%u, r=%u\n", c->low >> (CABAC_BITS + 1), c->range);
        return 0;
    }else{
        c->low -= range;
//        printf("bypass 1: o=%u, r=%u\n", c->low >> (CABAC_BITS + 1), c->range);
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
    const uint8_t *ptr = c->bytestream_start + ((c->b_offset + 7) >> 3);

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
