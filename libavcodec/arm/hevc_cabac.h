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

#ifndef AVCODEC_ARM_HEVC_CABAC_H
#define AVCODEC_ARM_HEVC_CABAC_H

#include "config.h"
#if HAVE_ARMV6T2_INLINE

#define hevc_mem_bits32 hevc_mem_bits32_arm
static inline uint32_t hevc_mem_bits32_arm(const void * p, const unsigned int bits)
{
    unsigned int n;
    __asm__ (
        "rev        %[n], %[x]                     \n\t"
        : [n]"=r"(n)
        : [x]"r"(*(const uint32_t *)((const uint8_t *)p + (bits >> 3)))
        :
        );
    return n << (bits & 7);
}


// ---------------------------------------------------------------------------
//
// Helper fns - little bits of code where ARM has an instraction that the
// compiler doesn't know about / use

#define trans_scale_sat trans_scale_sat_arm
static inline int trans_scale_sat_arm(const int level, const unsigned int scale, const unsigned int scale_m, const unsigned int shift)
{
    int rv;
    int t = ((level * (int)(scale * scale_m)) >> shift) + 1;

    __asm__ (
    "ssat %[rv], #16, %[t], ASR #1 \n\t"
    : [rv]"=r"(rv)
    : [t]"r"(t)
    :
    );
    return rv;
}

#define update_rice update_rice_arm
static inline void update_rice_arm(uint8_t * const stat_coeff,
    const unsigned int last_coeff_abs_level_remaining,
    const unsigned int c_rice_param)
{
    int t;
    __asm__ (
    "lsl   %[t], %[coeff], #1               \n\t"
    "lsrs  %[t], %[t], %[shift]             \n\t"
    "it    eq                               \n\t"
    "subeq %[stat], %[stat], #1             \n\t"
    "cmp   %[t], #6                         \n\t"
    "adc   %[stat], %[stat], #0             \n\t"
    "usat  %[stat], #8, %[stat]             \n\t"
    : [stat]"+&r"(*stat_coeff),
         [t]"=&r"(t)
    :  [coeff]"r"(last_coeff_abs_level_remaining),
       [shift]"r"(c_rice_param)
    : "cc"
    );
}

// ---------------------------------------------------------------------------
//
// CABAC get loops
//
// Where the loop is simple enough we can normally do 10-30% better than the
// compiler

// Get the residual greater than 1 bits

#define get_cabac_greater1_bits get_cabac_greater1_bits_arm
static inline unsigned int get_cabac_greater1_bits_arm(CABACContext * const c, const unsigned int n,
    uint8_t * const state0)
{
    unsigned int i, reg_b, st, tmp, bit, rv;
     __asm__ (
         "mov        %[i]          , #0                          \n\t"
         "mov        %[rv]         , #0                          \n\t"
         "1:                                                     \n\t"
         "add        %[i]          , %[i]        , #1            \n\t"
         "cmp        %[rv]         , #0                          \n\t"
         "ite        eq                                          \n\t"
         "usateq     %[st]         , #2          , %[i]          \n\t"
         "movne      %[st]         , #0                          \n\t"

         "ldrb       %[bit]        , [%[state0], %[st]]          \n\t"
         "sub        %[r_b]        , %[mlps_tables], %[lps_off]  \n\t"
         "and        %[tmp]        , %[range]    , #0xC0         \n\t"
         "add        %[r_b]        , %[r_b]      , %[bit]        \n\t"
         "ldrb       %[tmp]        , [%[r_b], %[tmp], lsl #1]    \n\t"
         "sub        %[range]      , %[range]    , %[tmp]        \n\t"

         "cmp        %[low]        , %[range], lsl #17           \n\t"
         "ittt       ge                                          \n\t"
         "subge      %[low]        , %[low]      , %[range], lsl #17 \n\t"
         "mvnge      %[bit]        , %[bit]                      \n\t"
         "movge      %[range]      , %[tmp]                      \n\t"

         "ldrb       %[r_b]        , [%[mlps_tables], %[bit]]    \n\t"
         "and        %[bit]        , %[bit]      , #1            \n\t"
         "orr        %[rv]         , %[bit]      , %[rv], lsl #1 \n\t"

         "clz        %[tmp]        , %[range]                    \n\t"
         "sub        %[tmp]        , #23                         \n\t"

         "lsl        %[low]        , %[low]      , %[tmp]        \n\t"
         "lsl        %[range]      , %[range]    , %[tmp]        \n\t"

         "strb       %[r_b]        , [%[state0], %[st]]          \n\t"
// There is a small speed gain from combining both conditions, using a single
// branch and then working out what that meant later
         "lsls       %[tmp]        , %[low]      , #16           \n\t"
         "it         ne                                          \n\t"
         "cmpne      %[n]          , %[i]                        \n\t"
         "bne        1b                                          \n\t"

// If reload is not required then we must have run out of flags to decode
         "tst        %[tmp]        , %[tmp]                      \n\t"
         "bne        2f                                          \n\t"

// Do reload
         "ldrh       %[tmp]        , [%[bptr]]   , #2            \n\t"
         "movw       %[r_b]        , #0xFFFF                     \n\t"
         "rev        %[tmp]        , %[tmp]                      \n\t"
         "rsb        %[tmp]        , %[r_b]      , %[tmp], lsr #15 \n\t"

         "rbit       %[r_b]        , %[low]                      \n\t"
         "clz        %[r_b]        , %[r_b]                      \n\t"
         "sub        %[r_b]        , %[r_b]      , #16           \n\t"

#if CONFIG_THUMB
         "lsl        %[tmp]        , %[tmp]      , %[r_b]        \n\t"
         "add        %[low]        , %[low]      , %[tmp]        \n\t"
#else
         "add        %[low]        , %[low]      , %[tmp], lsl %[r_b] \n\t"
#endif

         "cmp        %[n]          , %[i]                        \n\t"
         "bne        1b                                          \n\t"
         "2:                                                     \n\t"
         :    [bit]"=&r"(bit),
              [low]"+&r"(c->low),
            [range]"+&r"(c->range),
              [r_b]"=&r"(reg_b),
             [bptr]"+&r"(c->bytestream),
                [i]"=&r"(i),
              [tmp]"=&r"(tmp),
               [st]"=&r"(st),
               [rv]"=&r"(rv)
          :  [state0]"r"(state0),
                  [n]"r"(n),
        [mlps_tables]"r"(ff_h264_cabac_tables + H264_MLPS_STATE_OFFSET + 128),
               [byte]"M"(offsetof(CABACContext, bytestream)),
            [lps_off]"I"((H264_MLPS_STATE_OFFSET + 128) - H264_LPS_RANGE_OFFSET)
         : "memory", "cc"
    );
    return rv;
}


// n must be > 0 on entry
#define get_cabac_sig_coeff_flag_idxs get_cabac_sig_coeff_flag_idxs_arm
static inline uint8_t * get_cabac_sig_coeff_flag_idxs_arm(CABACContext * const c, uint8_t * const state0,
    unsigned int n,
    const uint8_t const * ctx_map,
    uint8_t * p)
{
    unsigned int reg_b, tmp, st, bit;
     __asm__ (
         "1:                                                     \n\t"
// Get bin from map
         "ldrb       %[st]         , [%[ctx_map], %[n]]          \n\t"

// Load state & ranges
         "sub        %[r_b]        , %[mlps_tables], %[lps_off]  \n\t"
         "ldrb       %[bit]        , [%[state0], %[st]]          \n\t"
         "and        %[tmp]        , %[range]    , #0xC0         \n\t"
         "add        %[r_b]        , %[r_b]      , %[tmp], lsl #1 \n\t"
         "ldrb       %[tmp]        , [%[r_b], %[bit]]            \n\t"
         "sub        %[range]      , %[range]    , %[tmp]        \n\t"

         "cmp        %[low]        , %[range], lsl #17           \n\t"
         "ittt       ge                                          \n\t"
         "subge      %[low]        , %[low]      , %[range], lsl #17 \n\t"
         "mvnge      %[bit]        , %[bit]                      \n\t"
         "movge      %[range]      , %[tmp]                      \n\t"

         "ldrb       %[r_b]        , [%[mlps_tables], %[bit]]    \n\t"
         "tst        %[bit]        , #1                          \n\t"
// GCC asm seems to need strbne written differently for thumb and arm
#if CONFIG_THUMB
         "it         ne                                          \n\t"
         "strbne     %[n]          , [%[idx]]    , #1            \n\t"
#else
         "strneb     %[n]          , [%[idx]]    , #1            \n\t"
#endif

// Renorm
         "clz        %[tmp]        , %[range]                    \n\t"
         "sub        %[tmp]        , #23                         \n\t"
         "lsl        %[low]        , %[low]      , %[tmp]        \n\t"
         "lsl        %[range]      , %[range]    , %[tmp]        \n\t"

         "strb       %[r_b]        , [%[state0], %[st]]          \n\t"
// There is a small speed gain from combining both conditions, using a single
// branch and then working out what that meant later
         "subs       %[n]          , %[n]        , #1            \n\t"
#if CONFIG_THUMB
         "itt        ne                                          \n\t"
         "lslsne     %[tmp]        , %[low]      , #16           \n\t"
         "bne        1b                                          \n\t"
#else
         "lslnes     %[tmp]        , %[low]      , #16           \n\t"
         "bne        1b                                          \n\t"
#endif

// If we have bits left then n must be 0 so give up now
         "lsls       %[tmp]        , %[low]      , #16           \n\t"
         "bne        2f                                          \n\t"

// Do reload
         "ldrh       %[tmp]        , [%[bptr]]   , #2            \n\t"
         "movw       %[r_b]        , #0xFFFF                     \n\t"
         "rev        %[tmp]        , %[tmp]                      \n\t"
         "rsb        %[tmp]        , %[r_b]      , %[tmp], lsr #15 \n\t"

         "rbit       %[r_b]        , %[low]                      \n\t"
         "clz        %[r_b]        , %[r_b]                      \n\t"
         "sub        %[r_b]        , %[r_b]      , #16           \n\t"

#if CONFIG_THUMB
         "lsl        %[tmp]        , %[tmp]      , %[r_b]        \n\t"
         "add        %[low]        , %[low]      , %[tmp]        \n\t"
#else
         "add        %[low]        , %[low]      , %[tmp], lsl %[r_b] \n\t"
#endif

// Check to see if we still have more to do
         "cmp        %[n]          , #0                          \n\t"
         "bne        1b                                          \n\t"
         "2:                                                     \n\t"
         :    [bit]"=&r"(bit),
              [low]"+&r"(c->low),
            [range]"+&r"(c->range),
              [r_b]"=&r"(reg_b),
             [bptr]"+&r"(c->bytestream),
              [idx]"+&r"(p),
                [n]"+&r"(n),
              [tmp]"=&r"(tmp),
               [st]"=&r"(st)
          :  [state0]"r"(state0),
            [ctx_map]"r"(ctx_map),
        [mlps_tables]"r"(ff_h264_cabac_tables + H264_MLPS_STATE_OFFSET + 128),
               [byte]"M"(offsetof(CABACContext, bytestream)),
            [lps_off]"I"((H264_MLPS_STATE_OFFSET + 128) - H264_LPS_RANGE_OFFSET)
         : "memory", "cc"
    );

    return p;
}

// ---------------------------------------------------------------------------
//
// CABAC_BY22 functions
//
// By and large these are (at best) no faster than their C equivalents - the
// only one worth having is _peek where we do a slightly better job than the
// compiler
//
// The others have been stashed here for reference in case larger scale asm
// is attempted in which case they might be a useful base


#define get_cabac_by22_peek get_cabac_by22_peek_arm
static inline uint32_t get_cabac_by22_peek_arm(const CABACContext *const c)
{
    uint32_t rv, tmp;
    __asm__ (
        "bic      %[rv]  , %[low], #1            \n\t"
        "cmp      %[inv] , #0                    \n\t"
        "it       ne                             \n\t"
        "umullne  %[tmp] , %[rv] , %[inv], %[rv] \n\t"
        :  // Outputs
             [rv]"=&r"(rv),
             [tmp]"=r"(tmp)
        :  // Inputs
             [low]"r"(c->low),
             [inv]"r"(c->range)
        :  // Clobbers
                "cc"
    );
    return rv << 1;
}

#if 0

// ***** Slower than the C  :-(
#define get_cabac_by22_flush get_cabac_by22_flush_arm
static inline void get_cabac_by22_flush_arm(CABACContext *const c, const unsigned int n, const uint32_t val)
{
    uint32_t m, tmp;
    __asm__ (
    "add    %[bits], %[bits], %[n]   \n\t"
    "ldr    %[m], [%[ptr], %[bits], lsr #3]  \n\t"

    "rsb    %[tmp], %[n], #32        \n\t"
    "lsr    %[tmp], %[val], %[tmp]   \n\t"
    "mul    %[tmp], %[range], %[tmp] \n\t"

    "rev    %[m], %[m]               \n\t"

    "lsl    %[tmp], %[tmp], #23      \n\t"
    "rsb    %[low], %[tmp], %[low], lsl %[n] \n\t"

    "and    %[tmp], %[bits], #7         \n\t"
    "lsl    %[m], %[m], %[tmp]          \n\t"

    "orr    %[low], %[low], %[m], lsr #9      \n\t"
        :  // Outputs
             [m]"=&r"(m),
           [tmp]"=&r"(tmp),
          [bits]"+&r"(c->by22.bits),
           [low]"+&r"(c->low)
        :  // Inputs
               [n]"r"(n),
             [val]"r"(val),
             [inv]"r"(c->range),
           [range]"r"(c->by22.range),
             [ptr]"r"(c->bytestream)
        :  // Clobbers
    );
}


// Works but slower than C
#define coeff_abs_level_remaining_decode_by22(c,r) coeff_abs_level_remaining_decode_by22_arm(c, r)
static int coeff_abs_level_remaining_decode_by22_arm(CABACContext * const c, const unsigned int c_rice_param)
{
    uint32_t n, val, tmp, level;

//    PROFILE_START();

    __asm__ (
            // Peek
            "bic    %[val],  %[low],   #1  \n\t"
            "cmp    %[inv], #0          \n\t"
            "umullne  %[tmp], %[val], %[inv], %[val] \n\t"
            "lsl    %[val], %[val], #1  \n\t"

            // Count bits (n = prefix)
            "mvn    %[n], %[val] \n\t"
            "clz    %[n], %[n]   \n\t"

            "lsl    %[level], %[val], %[n] \n\t"
            "subs   %[tmp], %[n], #3 \n\t"
            "blo    2f \n\t"

            // prefix >= 3
            // < tmp = prefix - 3
            // > tmp = prefix + rice - 3
            "add    %[tmp], %[tmp], %[rice] \n\t"
            // > n = prefix * 2 + rice - 3
            "add    %[n], %[tmp], %[n] \n\t"
            "cmp    %[n], #21 \n\t"
            "bhi    3f \n\t"

            "orr    %[level], %[level], #0x80000000 \n\t"
            "rsb    %[tmp], %[tmp], #31 \n\t"
            "lsr    %[level], %[level], %[tmp] \n\t"

            "mov    %[tmp], #2 \n\t"
            "add    %[level], %[level], %[tmp], lsl %[rice] \n\t"
            "b      1f \n\t"

            // > 22 bits used in total - need reload
            "3:  \n\t"

            // Stash prefix + rice - 3 in level (only spare reg)
            "mov    %[level], %[tmp] \n\t"
            // Restore n to flush value (prefix)
            "sub    %[n], %[n], %[tmp] \n\t"

            // Flush + reload

//          "rsb    %[tmp], %[n], #32        \n\t"
//          "lsr    %[tmp], %[val], %[tmp]   \n\t"
//          "mul    %[tmp], %[range], %[tmp] \n\t"

            // As it happens we know that all the bits we are flushing are 1
            // so we can cheat slightly
            "rsb    %[tmp], %[range], %[range], lsl %[n] \n\t"
            "lsl    %[tmp], %[tmp], #23      \n\t"
            "rsb    %[low], %[tmp], %[low], lsl %[n] \n\t"

            "add    %[bits], %[bits], %[n]   \n\t"
            "ldr    %[n], [%[ptr], %[bits], lsr #3]  \n\t"
            "rev    %[n], %[n]               \n\t"
            "and    %[tmp], %[bits], #7         \n\t"
            "lsl    %[n], %[n], %[tmp]          \n\t"

            "orr    %[low], %[low], %[n], lsr #9      \n\t"

            // (reload)

            "bic    %[val],  %[low],   #1  \n\t"
            "cmp    %[inv], #0          \n\t"
            "umullne  %[tmp], %[val], %[inv], %[val] \n\t"
            "lsl    %[val], %[val], #1  \n\t"

            // Build value

            "mov    %[n], %[level] \n\t"

            "orr     %[tmp], %[val], #0x80000000 \n\t"
            "rsb     %[level], %[level], #31 \n\t"
            "lsr     %[level], %[tmp], %[level] \n\t"

            "mov    %[tmp], #2 \n\t"
            "add    %[level], %[level], %[tmp], lsl %[rice] \n\t"
            "b      1f \n\t"

            // prefix < 3
            "2:  \n\t"
            "rsb    %[tmp], %[rice], #31 \n\t"
            "lsr    %[level], %[level], %[tmp] \n\t"
            "orr    %[level], %[level], %[n], lsl %[rice] \n\t"
            "add    %[n], %[n], %[rice] \n\t"

            "1:  \n\t"
            // Flush
            "add    %[n], %[n], #1 \n\t"

            "rsb    %[tmp], %[n], #32        \n\t"
            "lsr    %[tmp], %[val], %[tmp]   \n\t"

            "add    %[bits], %[bits], %[n]   \n\t"
            "ldr    %[val], [%[ptr], %[bits], lsr #3]  \n\t"

            "mul    %[tmp], %[range], %[tmp] \n\t"
            "lsl    %[tmp], %[tmp], #23      \n\t"
            "rsb    %[low], %[tmp], %[low], lsl %[n] \n\t"

            "rev    %[val], %[val]               \n\t"
            "and    %[tmp], %[bits], #7         \n\t"
            "lsl    %[val], %[val], %[tmp]          \n\t"

            "orr    %[low], %[low], %[val], lsr #9      \n\t"
        :  // Outputs
         [level]"=&r"(level),
             [n]"=&r"(n),
           [val]"=&r"(val),
           [tmp]"=&r"(tmp),
          [bits]"+&r"(c->by22.bits),
           [low]"+&r"(c->low)
        :  // Inputs
            [rice]"r"(c_rice_param),
             [inv]"r"(c->range),
           [range]"r"(c->by22.range),
             [ptr]"r"(c->bytestream)
        :  // Clobbers
                "cc"
    );

//    PROFILE_ACC(residual_abs);

    return level;
}
#endif

#endif /* HAVE_ARMV6T2_INLINE */

#endif /* AVCODEC_ARM_HEVC_CABAC_H */
