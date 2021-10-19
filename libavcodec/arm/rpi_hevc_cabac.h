/*
 * This file is part of FFmpeg.
 *
 * Copyright (C) 2018 John Cox, Ben Avison for Raspberry Pi (Trading)
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
    int t = last_coeff_abs_level_remaining << 1;
    __asm__ (
    "lsrs  %[t], %[t], %[shift]             \n\t"

    "it    eq                               \n\t"
    "subeq %[stat], %[stat], #1             \n\t"
    "cmp   %[t], #6                         \n\t"
    "adc   %[stat], %[stat], #0             \n\t"
    "usat  %[stat], #8, %[stat]             \n\t"
    : [stat]"+r"(*stat_coeff),
         [t]"+r"(t)
    :  [shift]"r"(c_rice_param)
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
         "sub        %[r_b]        , %[mlps_tables], %[lps_off]  \n\t"
         "and        %[tmp]        , %[range]    , #0xC0         \n\t"

         "ldrb       %[bit]        , [%[state0], %[st]]          \n\t"
         "add        %[r_b]        , %[r_b]      , %[bit]        \n\t"
         "ldrb       %[tmp]        , [%[r_b], %[tmp], lsl #1]    \n\t"
         "sub        %[range]      , %[range]    , %[tmp]        \n\t"

         "cmp        %[low]        , %[range], lsl #17           \n\t"
         "ittt       ge                                          \n\t"
         "subge      %[low]        , %[low]      , %[range], lsl #17 \n\t"
         "movge      %[range]      , %[tmp]                      \n\t"
         "mvnge      %[bit]        , %[bit]                      \n\t"

         "clz        %[tmp]        , %[range]                    \n\t"
         "sub        %[tmp]        , #23                         \n\t"
         "ldrb       %[r_b]        , [%[mlps_tables], %[bit]]    \n\t"
         "and        %[bit]        , %[bit]      , #1            \n\t"
         "strb       %[r_b]        , [%[state0], %[st]]          \n\t"
         "lsl        %[low]        , %[low]      , %[tmp]        \n\t"
         "orr        %[rv]         , %[bit]      , %[rv], lsl #1 \n\t"
         "lsl        %[range]      , %[range]    , %[tmp]        \n\t"

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
         "rbit       %[bit]        , %[low]                      \n\t"
         "movw       %[r_b]        , #0xFFFF                     \n\t"
         "clz        %[bit]        , %[bit]                      \n\t"
         "rev        %[tmp]        , %[tmp]                      \n\t"
         "sub        %[bit]        , %[bit]      , #16           \n\t"
         "cmp        %[n]          , %[i]                        \n\t"
         "rsb        %[tmp]        , %[r_b]      , %[tmp], lsr #15 \n\t"

#if CONFIG_THUMB
         "lsl        %[tmp]        , %[tmp]      , %[bit]        \n\t"
         "add        %[low]        , %[low]      , %[tmp]        \n\t"
#else
         "add        %[low]        , %[low]      , %[tmp], lsl %[bit] \n\t"
#endif

         "bne        1b                                          \n\t"
         "2:                                                     \n\t"
         :    [bit]"=&r"(bit),
              [low]"+r"(c->low),
            [range]"+r"(c->range),
              [r_b]"=&r"(reg_b),
             [bptr]"+r"(c->bytestream),
                [i]"=&r"(i),
              [tmp]"=&r"(tmp),
               [st]"=&r"(st),
               [rv]"=&r"(rv)
          :  [state0]"r"(state0),
                  [n]"r"(n),
        [mlps_tables]"r"(ff_h264_cabac_tables + H264_MLPS_STATE_OFFSET + 128),
            [lps_off]"I"((H264_MLPS_STATE_OFFSET + 128) - H264_LPS_RANGE_OFFSET)
         : "memory", "cc"
    );
    return rv;
}


// n must be > 0 on entry
#define get_cabac_sig_coeff_flag_idxs get_cabac_sig_coeff_flag_idxs_arm
static inline uint8_t * get_cabac_sig_coeff_flag_idxs_arm(CABACContext * const c, uint8_t * const state0,
    unsigned int n,
    const uint8_t * ctx_map,
    uint8_t * p)
{
    unsigned int reg_b, tmp, st, bit;
     __asm__ (
// Get bin from map
#if CONFIG_THUMB
         "add        %[ctx_map]    , %[n]                        \n\t"
         "ldrb       %[st]         , [%[ctx_map]]                \n\t"
#else
         "ldrb       %[st]         , [%[ctx_map], %[n]]!         \n\t"
#endif
         "1:                                                     \n\t"

// Load state & ranges
         "ldrb       %[bit]        , [%[state0], %[st]]          \n\t"
         "and        %[tmp]        , %[range]    , #0xC0         \n\t"
         "sub        %[r_b]        , %[mlps_tables], %[lps_off]  \n\t"
         "add        %[r_b]        , %[r_b]      , %[tmp], lsl #1 \n\t"
         "ldrb       %[tmp]        , [%[r_b], %[bit]]            \n\t"
         "sub        %[range]      , %[range]    , %[tmp]        \n\t"

         "cmp        %[low]        , %[range], lsl #17           \n\t"
         "ittt       ge                                          \n\t"
         "mvnge      %[bit]        , %[bit]                      \n\t"
         "subge      %[low]        , %[low]      , %[range], lsl #17 \n\t"
         "movge      %[range]      , %[tmp]                      \n\t"

// Renorm
         "clz        %[tmp]        , %[range]                    \n\t"
         "ldrb       %[r_b]        , [%[mlps_tables], %[bit]]    \n\t"
         "sub        %[tmp]        , #23                         \n\t"
         "strb       %[r_b]        , [%[state0], %[st]]          \n\t"
         "tst        %[bit]        , #1                          \n\t"
         "ldrb       %[st]         , [%[ctx_map], #-1]!          \n\t"
         "lsl        %[low]        , %[low]      , %[tmp]        \n\t"
// GCC asm seems to need strbne written differently for thumb and arm
#if CONFIG_THUMB
         "it         ne                                          \n\t"
         "strbne     %[n]          , [%[idx]]    , #1            \n\t"
#else
         "strneb     %[n]          , [%[idx]]    , #1            \n\t"
#endif

// There is a small speed gain from combining both conditions, using a single
// branch and then working out what that meant later
         "subs       %[n]          , %[n]        , #1            \n\t"
         "lsl        %[range]      , %[range]    , %[tmp]        \n\t"
#if CONFIG_THUMB
         "itt        ne                                          \n\t"
         "lslsne     %[tmp]        , %[low]      , #16           \n\t"
#else
         "lslnes     %[tmp]        , %[low]      , #16           \n\t"
#endif
         "bne        1b                                          \n\t"

// If we have bits left then n must be 0 so give up now
         "lsls       %[tmp]        , %[low]      , #16           \n\t"
         "bne        2f                                          \n\t"

// Do reload
         "ldrh       %[tmp]        , [%[bptr]]   , #2            \n\t"
         "rbit       %[bit]        , %[low]                      \n\t"
         "movw       %[r_b]        , #0xFFFF                     \n\t"
         "clz        %[bit]        , %[bit]                      \n\t"
         "cmp        %[n]          , #0                          \n\t"
         "rev        %[tmp]        , %[tmp]                      \n\t"
         "sub        %[bit]        , %[bit]      , #16           \n\t"
         "rsb        %[tmp]        , %[r_b]      , %[tmp], lsr #15 \n\t"

#if CONFIG_THUMB
         "lsl        %[tmp]        , %[tmp]      , %[bit]        \n\t"
         "add        %[low]        , %[low]      , %[tmp]        \n\t"
#else
         "add        %[low]        , %[low]      , %[tmp], lsl %[bit] \n\t"
#endif

// Check to see if we still have more to do
         "bne        1b                                          \n\t"
         "2:                                                     \n\t"
         :    [bit]"=&r"(bit),
              [low]"+r"(c->low),
            [range]"+r"(c->range),
              [r_b]"=&r"(reg_b),
             [bptr]"+r"(c->bytestream),
              [idx]"+r"(p),
                [n]"+r"(n),
              [tmp]"=&r"(tmp),
               [st]"=&r"(st),
          [ctx_map]"+r"(ctx_map)
          :  [state0]"r"(state0),
        [mlps_tables]"r"(ff_h264_cabac_tables + H264_MLPS_STATE_OFFSET + 128),
            [lps_off]"I"((H264_MLPS_STATE_OFFSET + 128) - H264_LPS_RANGE_OFFSET)
         : "memory", "cc"
    );

    return p;
}

// ---------------------------------------------------------------------------
//
// CABAC_BY22 functions


#define get_cabac_by22_start get_cabac_by22_start_arm
static inline void get_cabac_by22_start_arm(CABACContext * const c)
{
    const uint8_t *ptr = c->bytestream;
    register uint32_t low __asm__("r1"), range __asm__("r2");
    uint32_t m, range8, bits;
#if !USE_BY22_DIV
    uintptr_t inv;
#endif

    av_assert2(offsetof (CABACContext, low) == 0);
    av_assert2(offsetof (CABACContext, range) == 4);
    av_assert2(offsetof (CABACContext, by22.range) == offsetof (CABACContext, by22.bits) + 2);
    __asm__ volatile (
        "ldmia   %[c], {%[low], %[range]}                         \n\t"
        : // Outputs
               [low]"=r"(low),
             [range]"=r"(range)
        : // Inputs
                 [c]"r"(c)
        : // Clobbers
    );
#if !USE_BY22_DIV
    inv = (uintptr_t)cabac_by22_inv_range;
#endif
    __asm__ volatile (
        "ldr     %[m], [%[ptr]], #-("AV_STRINGIFY(CABAC_BITS)"/8) \n\t"
#if !USE_BY22_DIV
        "uxtb    %[range8], %[range]                              \n\t"
#endif
        "rbit    %[bits], %[low]                                  \n\t"
        "lsl     %[low], %[low], #22 - "AV_STRINGIFY(CABAC_BITS)" \n\t"
        "clz     %[bits], %[bits]                                 \n\t"
        "str     %[ptr], [%[c], %[ptr_off]]                       \n\t"
        "rev     %[m], %[m]                                       \n\t"
        "rsb     %[ptr], %[bits], #9 + "AV_STRINGIFY(CABAC_BITS)" \n\t"
        "eor     %[m], %[m], #0x80000000                          \n\t"
#if !USE_BY22_DIV
        "ldr     %[inv], [%[inv], %[range8], lsl #2]              \n\t"
        "pkhbt   %[range], %[bits], %[range], lsl #16             \n\t"
        "str     %[range], [%[c], %[bits_off]]                    \n\t"
#else
        "strh    %[bits], [%[c], %[bits_off]]                     \n\t"
#endif
#if CONFIG_THUMB
        "lsr     %[m], %[ptr]                                     \n\t"
        "eor     %[range], %[low], %[m]                           \n\t"
#else
        "eor     %[range], %[low], %[m], lsr %[ptr]               \n\t"
#endif
        : // Outputs
               [ptr]"+&r"(ptr),
               [low]"+&r"(low),
             [range]"+&r"(range),
#if !USE_BY22_DIV
               [inv]"+&r"(inv),
#endif
                 [m]"=&r"(m),
            [range8]"=&r"(range8),
              [bits]"=&r"(bits)
        : // Inputs
                   [c]"r"(c),
            [bits_off]"J"(offsetof (CABACContext, by22.bits)),
             [ptr_off]"J"(offsetof (CABACContext, bytestream))
        : // Clobbers
            "memory"
    );
    c->low = range;
#if !USE_BY22_DIV
    c->range = inv;
#endif
}

#define get_cabac_by22_peek get_cabac_by22_peek_arm
static inline uint32_t get_cabac_by22_peek_arm(const CABACContext *const c)
{
    uint32_t rv = c->low &~ 1, tmp;
    __asm__ (
        "cmp      %[inv] , #0                    \n\t"
        "it       ne                             \n\t"
        "umullne  %[tmp] , %[rv] , %[inv], %[rv] \n\t"
        :  // Outputs
             [rv]"+r"(rv),
             [tmp]"=r"(tmp)
        :  // Inputs
             [inv]"r"(c->range)
        :  // Clobbers
                "cc"
    );
    return rv << 1;
}

#define get_cabac_by22_flush get_cabac_by22_flush_arm
static inline void get_cabac_by22_flush_arm(CABACContext *const c, const unsigned int n, uint32_t val)
{
    uint32_t bits, ptr, tmp1, tmp2;
    __asm__ volatile (
        "ldrh    %[bits], [%[cc], %[bits_off]]     \n\t"
        "ldr     %[ptr], [%[cc], %[ptr_off]]       \n\t"
        "rsb     %[tmp1], %[n], #32                \n\t"
        "add     %[bits], %[bits], %[n]            \n\t"
        "ldrh    %[tmp2], [%[cc], %[range_off]]    \n\t"
        "lsr     %[tmp1], %[val], %[tmp1]          \n\t"
        "ldr     %[val], [%[cc], %[low_off]]       \n\t"
#if CONFIG_THUMB
        "add     %[ptr], %[ptr], %[bits], lsr #3   \n\t"
        "ldr     %[ptr], [%[ptr]]                  \n\t"
#else
        "ldr     %[ptr], [%[ptr], %[bits], lsr #3] \n\t"
#endif
        "mul     %[tmp1], %[tmp2], %[tmp1]         \n\t"
        "and     %[tmp2], %[bits], #7              \n\t"
        "strh    %[bits], [%[cc], %[bits_off]]     \n\t"
        "rev     %[ptr], %[ptr]                    \n\t"
        "lsl     %[tmp1], %[tmp1], #23             \n\t"
#if CONFIG_THUMB
        "lsl     %[val], %[n]                      \n\t"
        "sub     %[val], %[tmp1]                   \n\t"
#else
        "rsb     %[val], %[tmp1], %[val], lsl %[n] \n\t"
#endif
        "lsl     %[ptr], %[ptr], %[tmp2]           \n\t"
        "orr     %[val], %[val], %[ptr], lsr #9    \n\t"
        "str     %[val], [%[cc], %[low_off]]       \n\t"
        :  // Outputs
            [val]"+r"(val),
           [bits]"=&r"(bits),
            [ptr]"=&r"(ptr),
           [tmp1]"=&r"(tmp1),
           [tmp2]"=&r"(tmp2)
        :  // Inputs
                  [cc]"r"(c),
                   [n]"r"(n),
            [bits_off]"J"(offsetof(CABACContext, by22.bits)),
             [ptr_off]"J"(offsetof(CABACContext, bytestream)),
           [range_off]"J"(offsetof(CABACContext, by22.range)),
             [low_off]"J"(offsetof(CABACContext, low))
        :  // Clobbers
           "memory"
    );
}

#define coeff_abs_level_remaining_decode_bypass coeff_abs_level_remaining_decode_bypass_arm
static inline int coeff_abs_level_remaining_decode_bypass_arm(CABACContext *const c, unsigned int rice_param)
{
    uint32_t last_coeff_abs_level_remaining;
    uint32_t prefix, n1, range, n2, ptr, tmp1, tmp2;
    __asm__ volatile (
        "ldr     %[remain], [%[cc], %[low_off]]               \n\t"
        "ldr     %[prefix], [%[cc], %[range_off]]             \n\t"
        "bic     %[remain], %[remain], #1                     \n\t"
        "ldrh    %[tmp2], [%[cc], %[by22_bits_off]]           \n\t"
        "ldr     %[ptr], [%[cc], %[ptr_off]]                  \n\t"
        "cmp     %[prefix], #0                                \n\t"
        "it      ne                                           \n\t"
        "umullne %[prefix], %[remain], %[prefix], %[remain]   \n\t"
        "ldrh    %[range], [%[cc], %[by22_range_off]]         \n\t"
        "lsl     %[remain], %[remain], #1                     \n\t"
        "mvn     %[prefix], %[remain]                         \n\t"
        "clz     %[prefix], %[prefix]                         \n\t"
        "rsbs    %[n1], %[prefix], #2                         \n\t"
        "bcc     1f                                           \n\t"
        "adc     %[n1], %[rice], %[prefix]                    \n\t"
        "add     %[tmp2], %[tmp2], %[n1]                      \n\t"
        "rsb     %[n2], %[n1], #32                            \n\t"
        "and     %[tmp1], %[tmp2], #7                         \n\t"
        "strh    %[tmp2], [%[cc], %[by22_bits_off]]           \n\t"
        "lsr     %[tmp2], %[tmp2], #3                         \n\t"
        "lsr     %[n2], %[remain], %[n2]                      \n\t"
        "mul     %[n2], %[range], %[n2]                       \n\t"
        "ldr     %[range], [%[cc], %[low_off]]                \n\t"
        "ldr     %[ptr], [%[ptr], %[tmp2]]                    \n\t"
        "rsb     %[tmp2], %[rice], #31                        \n\t"
        "lsl     %[remain], %[remain], %[prefix]              \n\t"
        "lsl     %[n2], %[n2], #23                            \n\t"
#if CONFIG_THUMB
        "lsl     %[range], %[n1]                              \n\t"
        "sub     %[range], %[n2]                              \n\t"
#else
        "rsb     %[range], %[n2], %[range], lsl %[n1]         \n\t"
#endif
        "rev     %[ptr], %[ptr]                               \n\t"
        "lsl     %[n2], %[prefix], %[rice]                    \n\t"
#if CONFIG_THUMB
        "lsr     %[remain], %[tmp2]                           \n\t"
        "add     %[remain], %[n2]                             \n\t"
#else
        "add     %[remain], %[n2], %[remain], lsr %[tmp2]     \n\t"
#endif
        "b       3f                                           \n\t"
        "1:                                                   \n\t"
        "add     %[n2], %[rice], %[prefix], lsl #1            \n\t"
        "cmp     %[n2], %[peek_bits_plus_2]                   \n\t"
        "bhi     2f                                           \n\t"
        "sub     %[n1], %[n2], #2                             \n\t"
        "add     %[tmp2], %[tmp2], %[n1]                      \n\t"
        "rsb     %[n2], %[n1], #32                            \n\t"
        "strh    %[tmp2], [%[cc], %[by22_bits_off]]           \n\t"
        "lsr     %[tmp1], %[tmp2], #3                         \n\t"
        "lsr     %[n2], %[remain], %[n2]                      \n\t"
        "mul     %[n2], %[range], %[n2]                       \n\t"
        "rsb     %[range], %[rice], #34                       \n\t"
        "ldr     %[ptr], [%[ptr], %[tmp1]]                    \n\t"
        "and     %[tmp1], %[tmp2], #7                         \n\t"
        "lsl     %[remain], %[remain], %[prefix]              \n\t"
        "ldr     %[tmp2], [%[cc], %[low_off]]                 \n\t"
        "rsb     %[prefix], %[prefix], %[range]               \n\t"
        "orr     %[remain], %[remain], #0x80000000            \n\t"
        "rev     %[ptr], %[ptr]                               \n\t"
        "lsl     %[n2], %[n2], #23                            \n\t"
        "mov     %[range], #2                                 \n\t"
#if CONFIG_THUMB
        "lsl     %[tmp2], %[n1]                               \n\t"
        "sub     %[tmp2], %[n2]                               \n\t"
#else
        "rsb     %[tmp2], %[n2], %[tmp2], lsl %[n1]           \n\t"
#endif
        "lsl     %[ptr], %[ptr], %[tmp1]                      \n\t"
        "lsl     %[rice], %[range], %[rice]                   \n\t"
        "orr     %[range], %[tmp2], %[ptr], lsr #9            \n\t"
#if CONFIG_THUMB
        "lsr     %[remain], %[prefix]                         \n\t"
        "add     %[remain], %[rice]                           \n\t"
#else
        "add     %[remain], %[rice], %[remain], lsr %[prefix] \n\t"
#endif
        "b       4f                                           \n\t"
        "2:                                                   \n\t"
        "add     %[n1], %[tmp2], %[prefix]                    \n\t"
#if CONFIG_THUMB
        "add     %[tmp2], %[ptr], %[n1], lsr #3               \n\t"
        "ldr     %[tmp2], [%[tmp2]]                           \n\t"
#else
        "ldr     %[tmp2], [%[ptr], %[n1], lsr #3]             \n\t"
#endif
        "rsb     %[tmp1], %[prefix], #32                      \n\t"
        "push    {%[rice]}                                    \n\t"
        "and     %[rice], %[n1], #7                           \n\t"
        "lsr     %[tmp1], %[remain], %[tmp1]                  \n\t"
        "ldr     %[ptr], [%[cc], %[low_off]]                  \n\t"
        "mul     %[remain], %[range], %[tmp1]                 \n\t"
        "rev     %[tmp2], %[tmp2]                             \n\t"
        "rsb     %[n2], %[prefix], %[n2]                      \n\t"
        "ldr     %[tmp1], [%[cc], %[range_off]]               \n\t"
        "lsl     %[rice], %[tmp2], %[rice]                    \n\t"
        "sub     %[tmp2], %[n2], #2                           \n\t"
        "lsl     %[remain], %[remain], #23                    \n\t"
#if CONFIG_THUMB
        "lsl     %[ptr], %[prefix]                            \n\t"
        "rsb     %[remain], %[ptr]                            \n\t"
#else
        "rsb     %[remain], %[remain], %[ptr], lsl %[prefix]  \n\t"
#endif
        "orr     %[remain], %[remain], %[rice], lsr #9        \n\t"
        "add     %[prefix], %[n1], %[tmp2]                    \n\t"
        "bic     %[n1], %[remain], #1                         \n\t"
        "ldr     %[ptr], [%[cc], %[ptr_off]]                  \n\t"
        "cmp     %[tmp1], #0                                  \n\t"
        "rsb     %[rice], %[tmp2], #32                        \n\t"
        "it      ne                                           \n\t"
        "umullne %[tmp1], %[n1], %[tmp1], %[n1]               \n\t"
        "and     %[tmp1], %[prefix], #7                       \n\t"
#if CONFIG_THUMB
        "add     %[ptr], %[ptr], %[prefix], lsr #3            \n\t"
        "ldr     %[ptr], [%[ptr]]                             \n\t"
#else
        "ldr     %[ptr], [%[ptr], %[prefix], lsr #3]          \n\t"
#endif
        "lsl     %[n1], %[n1], #1                             \n\t"
        "lsr     %[rice], %[n1], %[rice]                      \n\t"
        "rsb     %[n2], %[n2], #34                            \n\t"
        "mul     %[range], %[range], %[rice]                  \n\t"
        "pop     {%[rice]}                                    \n\t"
        "rev     %[ptr], %[ptr]                               \n\t"
        "orr     %[n1], %[n1], #0x80000000                    \n\t"
        "strh    %[prefix], [%[cc], %[by22_bits_off]]         \n\t"
        "mov     %[prefix], #2                                \n\t"
        "lsl     %[range], %[range], #23                      \n\t"
#if CONFIG_THUMB
        "lsl     %[remain], %[tmp2]                           \n\t"
        "rsb     %[range], %[remain]                          \n\t"
#else
        "rsb     %[range], %[range], %[remain], lsl %[tmp2]   \n\t"
#endif
        "lsl     %[remain], %[prefix], %[rice]                \n\t"
#if CONFIG_THUMB
        "lsr     %[n1], %[n2]                                 \n\t"
        "add     %[remain], %[n1]                             \n\t"
#else
        "add     %[remain], %[remain], %[n1], lsr %[n2]       \n\t"
#endif
        "3:                                                   \n\t"
        "lsl     %[ptr], %[ptr], %[tmp1]                      \n\t"
        "orr     %[range], %[range], %[ptr], lsr #9           \n\t"
        "4:                                                   \n\t"
        "str     %[range], [%[cc], %[low_off]]                \n\t"
        :  // Outputs
            [remain]"=&r"(last_coeff_abs_level_remaining),
              [rice]"+r"(rice_param),
            [prefix]"=&r"(prefix),
                [n1]"=&r"(n1),
             [range]"=&r"(range),
                [n2]"=&r"(n2),
               [ptr]"=&r"(ptr),
              [tmp1]"=&r"(tmp1),
              [tmp2]"=&r"(tmp2)
        :  // Inputs
                          [cc]"r"(c),
            [peek_bits_plus_2]"I"(CABAC_BY22_PEEK_BITS + 2),
                     [low_off]"J"(offsetof(CABACContext, low)),
                   [range_off]"J"(offsetof(CABACContext, range)),
               [by22_bits_off]"J"(offsetof(CABACContext, by22.bits)),
              [by22_range_off]"J"(offsetof(CABACContext, by22.range)),
                     [ptr_off]"J"(offsetof(CABACContext, bytestream))
        :  // Clobbers
           "cc", "memory"
    );
    return last_coeff_abs_level_remaining;
}

#endif /* HAVE_ARMV6T2_INLINE */

#endif /* AVCODEC_ARM_HEVC_CABAC_H */
