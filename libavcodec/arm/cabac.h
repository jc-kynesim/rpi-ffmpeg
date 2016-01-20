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

#ifndef AVCODEC_ARM_CABAC_H
#define AVCODEC_ARM_CABAC_H

#include "config.h"
#if HAVE_ARMV6T2_INLINE

#include "libavutil/attributes.h"
#include "libavutil/internal.h"
#include "libavcodec/cabac.h"


#if UNCHECKED_BITSTREAM_READER
#define LOAD_16BITS_BEHI\
        "ldrh       %[tmp]        , [%[ptr]]    , #2            \n\t"\
        "rev        %[tmp]        , %[tmp]                      \n\t"
#elif CONFIG_THUMB
#define LOAD_16BITS_BEHI\
        "ldr        %[tmp]        , [%[c], %[end]]              \n\t"\
        "cmp        %[tmp]        , %[ptr]                      \n\t"\
        "it         cs                                          \n\t"\
        "ldrhcs     %[tmp]        , [%[ptr]]    , #2            \n\t"\
        "rev        %[tmp]        , %[tmp]                      \n\t"
#else
#define LOAD_16BITS_BEHI\
        "ldr        %[tmp]        , [%[c], %[end]]              \n\t"\
        "cmp        %[tmp]        , %[ptr]                      \n\t"\
        "ldrcsh     %[tmp]        , [%[ptr]]    , #2            \n\t"\
        "rev        %[tmp]        , %[tmp]                      \n\t"
#endif


#define get_cabac_inline get_cabac_inline_arm
static av_always_inline int get_cabac_inline_arm(CABACContext *c,
                                                 uint8_t *const state)
{
    int bit;
#if 0
    void *reg_b, *reg_c, *tmp;
    __asm__ volatile(
        "ldrb       %[bit]        , [%[state]]                  \n\t"
        "add        %[r_b]        , %[tables]   , %[lps_off]    \n\t"
        "mov        %[tmp]        , %[range]                    \n\t"
        "and        %[range]      , %[range]    , #0xC0         \n\t"
        "add        %[r_b]        , %[r_b]      , %[bit]        \n\t"
        "ldrb       %[range]      , [%[r_b], %[range], lsl #1]  \n\t"
        "add        %[r_b]        , %[tables]   , %[norm_off]   \n\t"
        "sub        %[r_c]        , %[tmp]      , %[range]      \n\t"
        "lsl        %[tmp]        , %[r_c]      , #17           \n\t"
        "cmp        %[tmp]        , %[low]                      \n\t"
        "it         gt                                          \n\t"
        "movgt      %[range]      , %[r_c]                      \n\t"
        "itt        cc                                          \n\t"
        "mvncc      %[bit]        , %[bit]                      \n\t"
        "subcc      %[low]        , %[low]      , %[tmp]        \n\t"
        "add        %[r_c]        , %[tables]   , %[mlps_off]   \n\t"
        "ldrb       %[tmp]        , [%[r_b], %[range]]          \n\t"
        "ldrb       %[r_b]        , [%[r_c], %[bit]]            \n\t"
        "lsl        %[low]        , %[low]      , %[tmp]        \n\t"
        "lsl        %[range]      , %[range]    , %[tmp]        \n\t"
        "uxth       %[r_c]        , %[low]                      \n\t"
        "strb       %[r_b]        , [%[state]]                  \n\t"
        "tst        %[r_c]        , %[r_c]                      \n\t"
        "bne        2f                                          \n\t"
        "ldr        %[r_c]        , [%[c], %[byte]]             \n\t"
#if UNCHECKED_BITSTREAM_READER
        "ldrh       %[tmp]        , [%[r_c]]                    \n\t"
        "add        %[r_c]        , %[r_c]      , #2            \n\t"
        "str        %[r_c]        , [%[c], %[byte]]             \n\t"
#else
        "ldr        %[r_b]        , [%[c], %[end]]              \n\t"
        "ldrh       %[tmp]        , [%[r_c]]                    \n\t"
        "cmp        %[r_c]        , %[r_b]                      \n\t"
        "itt        lt                                          \n\t"
        "addlt      %[r_c]        , %[r_c]      , #2            \n\t"
        "strlt      %[r_c]        , [%[c], %[byte]]             \n\t"
#endif
        "sub        %[r_c]        , %[low]      , #1            \n\t"
        "add        %[r_b]        , %[tables]   , %[norm_off]   \n\t"
        "eor        %[r_c]        , %[low]      , %[r_c]        \n\t"
        "rev        %[tmp]        , %[tmp]                      \n\t"
        "lsr        %[r_c]        , %[r_c]      , #15           \n\t"
        "lsr        %[tmp]        , %[tmp]      , #15           \n\t"
        "ldrb       %[r_c]        , [%[r_b], %[r_c]]            \n\t"
        "movw       %[r_b]        , #0xFFFF                     \n\t"
        "sub        %[tmp]        , %[tmp]      , %[r_b]        \n\t"
        "rsb        %[r_c]        , %[r_c]      , #7            \n\t"
        "lsl        %[tmp]        , %[tmp]      , %[r_c]        \n\t"
        "add        %[low]        , %[low]      , %[tmp]        \n\t"
        "2:                                                     \n\t"
        :    [bit]"=&r"(bit),
             [low]"+&r"(c->low),
           [range]"+&r"(c->range),
             [r_b]"=&r"(reg_b),
             [r_c]"=&r"(reg_c),
             [tmp]"=&r"(tmp)
        :        [c]"r"(c),
             [state]"r"(state),
            [tables]"r"(ff_h264_cabac_tables),
              [byte]"M"(offsetof(CABACContext, bytestream)),
               [end]"M"(offsetof(CABACContext, bytestream_end)),
          [norm_off]"I"(H264_NORM_SHIFT_OFFSET),
           [lps_off]"I"(H264_LPS_RANGE_OFFSET),
          [mlps_off]"I"(H264_MLPS_STATE_OFFSET + 128)
        : "memory", "cc"
        );
#else
   // *** Not thumb compatible yet
   unsigned int reg_b, tmp;
    __asm__ (
        "ldrb       %[bit]        , [%[state]]                  \n\t"
        "sub        %[r_b]        , %[mlps_tables], %[lps_off]  \n\t"
        "and        %[tmp]        , %[range]    , #0xC0         \n\t"
        "add        %[r_b]        , %[r_b]      , %[bit]        \n\t"
        "ldrb       %[tmp]        , [%[r_b]     , %[tmp], lsl #1] \n\t"
// %bit = *state
// %range = range
// %tmp = RangeLPS
        "sub        %[range]      , %[range]    , %[tmp]        \n\t"

        "cmp        %[low]        , %[range]    , lsl #17       \n\t"
        "ittt       ge                                          \n\t"
        "subge      %[low]        , %[low]      , %[range], lsl #17 \n\t"
        "mvnge      %[bit]        , %[bit]                      \n\t"
        "movge      %[range]      , %[tmp]                      \n\t"

        "clz        %[tmp]        , %[range]                    \n\t"
        "sub        %[tmp]        , #23                         \n\t"

        "ldrb       %[r_b]        , [%[mlps_tables], %[bit]]    \n\t"
        "lsl        %[low]        , %[low]      , %[tmp]        \n\t"
        "lsl        %[range]      , %[range]    , %[tmp]        \n\t"

        "strb       %[r_b]        , [%[state]]                  \n\t"
        "lsls       %[tmp]        , %[low]      , #16           \n\t"

        "bne        2f                                          \n\t"
        LOAD_16BITS_BEHI
        "lsr        %[tmp]        , %[tmp]      , #15           \n\t"
        "movw       %[r_b]        , #0xFFFF                     \n\t"
        "sub        %[tmp]        , %[tmp]      , %[r_b]        \n\t"

        "rbit       %[r_b]        , %[low]                      \n\t"
        "clz        %[r_b]        , %[r_b]                      \n\t"
        "sub        %[r_b]        , %[r_b]      , #16           \n\t"
#if CONFIG_THUMB
        "lsl        %[tmp]        , %[tmp]      , %[r_b]        \n\t"
        "add        %[low]        , %[low]      , %[tmp]        \n\t"
#else
        "add        %[low]        , %[low]      , %[tmp], lsl %[r_b] \n\t"
#endif
        "2:                                                     \n\t"
        :    [bit]"=&r"(bit),
             [low]"+&r"(c->low),
           [range]"+&r"(c->range),
             [r_b]"=&r"(reg_b),
             [ptr]"+&r"(c->bytestream),
             [tmp]"=&r"(tmp)
          :  [state]"r"(state),
            [mlps_tables]"r"(ff_h264_cabac_tables + H264_MLPS_STATE_OFFSET + 128),
              [byte]"M"(offsetof(CABACContext, bytestream)),
#if !UNCHECKED_BITSTREAM_READER
                 [c]"r"(c),
               [end]"M"(offsetof(CABACContext, bytestream_end)),
#endif
           [lps_off]"I"((H264_MLPS_STATE_OFFSET + 128) - H264_LPS_RANGE_OFFSET)
        : "memory", "cc"
        );
#endif

    return bit & 1;
}

#define get_cabac_bypass get_cabac_bypass_arm
static inline int get_cabac_bypass_arm(CABACContext * const c)
{
    int rv = 0;
    unsigned int tmp;
    __asm (
        "lsl        %[low]        , #1                          \n\t"
        "cmp        %[low]        , %[range]    , lsl #17       \n\t"
        "adc        %[rv]         , %[rv]       , #0            \n\t"
        "it         cs                                          \n\t"
        "subcs      %[low]        , %[low]      , %[range], lsl #17 \n\t"
        "lsls       %[tmp]        , %[low]      , #16           \n\t"
        "bne        1f                                          \n\t"
        LOAD_16BITS_BEHI
        "add        %[low]        , %[low]      , %[tmp], lsr #15 \n\t"
        "movw       %[tmp]        , #0xFFFF                     \n\t"
        "sub        %[low]        , %[low]      , %[tmp]        \n\t"
        "1:                                                     \n\t"
        : // Outputs
              [rv]"+&r"(rv),
             [low]"+&r"(c->low),
             [tmp]"=&r"(tmp),
             [ptr]"+&r"(c->bytestream)
        : // Inputs
#if !UNCHECKED_BITSTREAM_READER
                 [c]"r"(c),
               [end]"M"(offsetof(CABACContext, bytestream_end)),
#endif
             [range]"r"(c->range)
        : "cc"
    );
    return rv;
}


#define get_cabac_bypass_sign get_cabac_bypass_sign_arm
static inline int get_cabac_bypass_sign_arm(CABACContext * const c, int rv)
{
    unsigned int tmp;
    __asm (
        "lsl        %[low]        , #1                          \n\t"
        "cmp        %[low]        , %[range]    , lsl #17       \n\t"
        "ite        cc                                          \n\t"
        "rsbcc      %[rv]         , %[rv]       , #0            \n\t"
        "subcs      %[low]        , %[low]      , %[range], lsl #17 \n\t"
        "lsls       %[tmp]        , %[low]      , #16           \n\t"
        "bne        1f                                          \n\t"
        LOAD_16BITS_BEHI
        "add        %[low]        , %[low]      , %[tmp], lsr #15 \n\t"
        "movw       %[tmp]        , #0xFFFF                     \n\t"
        "sub        %[low]        , %[low]      , %[tmp]        \n\t"
        "1:                                                     \n\t"
        : // Outputs
              [rv]"+&r"(rv),
             [low]"+&r"(c->low),
             [tmp]"=&r"(tmp),
             [ptr]"+&r"(c->bytestream)
        : // Inputs
#if !UNCHECKED_BITSTREAM_READER
                 [c]"r"(c),
               [end]"M"(offsetof(CABACContext, bytestream_end)),
#endif
             [range]"r"(c->range)
        : "cc"
    );
    return rv;
}

#endif /* HAVE_ARMV6T2_INLINE */

#endif /* AVCODEC_ARM_CABAC_H */
