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


#define get_cabac_inline get_cabac_inline_arm
static av_always_inline int get_cabac_inline_arm(CABACContext *c,
                                                 uint8_t *state)
{
    const uint8_t *mlps_tables = ff_h264_cabac_tables + H264_MLPS_STATE_OFFSET + 128;
    int bit, ptr, low, tmp1, tmp2;
    __asm__ volatile (
        "ldr     %[bit], [%[c], %[range_off]]             \n\t"
        "ldrb    %[ptr], [%[state]]                       \n\t"
        "sub     %[tmp1], %[mlps_tables], %[lps_off]      \n\t"
        "and     %[tmp2], %[bit], #0xc0                   \n\t"
        "add     %[tmp1], %[tmp1], %[ptr]                 \n\t"
        "ldr     %[low], [%[c], %[low_off]]               \n\t"
        "ldrb    %[tmp2], [%[tmp1], %[tmp2], lsl #1]      \n\t"
        "sub     %[bit], %[bit], %[tmp2]                  \n\t"
        "mov     %[tmp1], %[bit]                          \n\t"
        "cmp     %[low], %[bit], lsl #17                  \n\t"
        "itt     ge                                       \n\t"
        "movge   %[tmp1], %[tmp2]                         \n\t"
        "mvnge   %[ptr], %[ptr]                           \n\t"
        "clz     %[tmp2], %[tmp1]                         \n\t"
        "it      ge                                       \n\t"
        "subge   %[low], %[low], %[bit], lsl #17          \n\t"
        "sub     %[tmp2], %[tmp2], #23                    \n\t"
        "and     %[bit], %[ptr], #1                       \n\t"
        "ldrb    %[mlps_tables], [%[mlps_tables], %[ptr]] \n\t"
        "lsl     %[low], %[low], %[tmp2]                  \n\t"
        "lsls    %[ptr], %[low], #16                      \n\t"
        "bne     1f                                       \n\t"
        "ldr     %[ptr], [%[c], %[ptr_off]]               \n\t"
        "lsl     %[tmp2], %[tmp1], %[tmp2]                \n\t"
#if UNCHECKED_BITSTREAM_READER
        "strb    %[mlps_tables], [%[state]]               \n\t"
        "rbit    %[state], %[low]                         \n\t"
        "ldrh    %[tmp1], [%[ptr]], #2                    \n\t"
#else
        "ldr     %[tmp1], [%[c], %[end_off]]              \n\t"
        "strb    %[mlps_tables], [%[state]]               \n\t"
        "rbit    %[state], %[low]                         \n\t"
        "cmp     %[tmp1], %[ptr]                          \n\t"
#if CONFIG_THUMB
        "it      cs                                       \n\t"
        "ldrhcs  %[tmp1], [%[ptr]], #2                    \n\t"
#else
        "ldrcsh  %[tmp1], [%[ptr]], #2                    \n\t"
#endif
#endif
        "clz     %[state], %[state]                       \n\t"
        "movw    %[mlps_tables], #0xffff                  \n\t"
        "sub     %[state], %[state], #16                  \n\t"
        "str     %[tmp2], [%[c], %[range_off]]            \n\t"
        "rev     %[tmp1], %[tmp1]                         \n\t"
        "str     %[ptr], [%[c], %[ptr_off]]               \n\t"
        "lsr     %[tmp1], %[tmp1], #15                    \n\t"
        "sub     %[tmp1], %[tmp1], %[mlps_tables]         \n\t"
#if CONFIG_THUMB
        "lsl     %[tmp1], %[tmp1], %[state]               \n\t"
        "add     %[low], %[low], %[tmp1]                  \n\t"
#else
        "add     %[low], %[low], %[tmp1], lsl %[state]    \n\t"
#endif
        "str     %[low], [%[c], %[low_off]]               \n\t"
        "b       2f                                       \n\t"
        "1:                                               \n\t"
        "strb    %[mlps_tables], [%[state]]               \n\t"
        "lsl     %[tmp1], %[tmp1], %[tmp2]                \n\t"
        "str     %[low], [%[c], %[low_off]]               \n\t"
        "str     %[tmp1], [%[c], %[range_off]]            \n\t"
        "2:                                               \n\t"
    :  // Outputs
             [state]"+r"(state),
       [mlps_tables]"+r"(mlps_tables),
               [bit]"=&r"(bit),
               [ptr]"=&r"(ptr),
               [low]"=&r"(low),
              [tmp1]"=&r"(tmp1),
              [tmp2]"=&r"(tmp2)
    :  // Inputs
               [c]"r"(c),
         [low_off]"J"(offsetof(CABACContext, low)),
       [range_off]"J"(offsetof(CABACContext, range)),
         [ptr_off]"J"(offsetof(CABACContext, bytestream)),
         [end_off]"J"(offsetof(CABACContext, bytestream_end)),
         [lps_off]"I"((H264_MLPS_STATE_OFFSET + 128) - H264_LPS_RANGE_OFFSET)
    :  // Clobbers
       "cc", "memory"
    );
    return bit;
}

#define get_cabac_bypass get_cabac_bypass_arm
static inline int get_cabac_bypass_arm(CABACContext * const c)
{
    uint32_t low = c->low, range, ptr, tmp;
    int rv;
    __asm volatile (
        "ldr        %[range] , [%[c], %[range_off]] \n\t"
        "mov        %[rv]    , #0                   \n\t"
        "ldr        %[ptr]   , [%[c], %[ptr_off]]   \n\t"
        "lsl        %[low]   , #1                   \n\t"
#if !UNCHECKED_BITSTREAM_READER
        "ldr        %[tmp]   , [%[c], %[end_off]]   \n\t"
#endif
        "cmp        %[low]   , %[range], lsl #17    \n\t"
        "itt         cs                              \n\t"
        "subcs      %[low]   , %[low], %[range], lsl #17 \n\t"
        "movcs      %[rv]    , #1                   \n\t"
#if UNCHECKED_BITSTREAM_READER
        "ldrh       %[tmp]   , [%[ptr]], #2         \n\t"
#else
        "cmp        %[tmp]   , %[ptr]               \n\t"
#if CONFIG_THUMB
        "it         cs                              \n\t"
        "ldrhcs     %[tmp]   , [%[ptr]], #2         \n\t"
#else
        "ldrcsh     %[tmp]   , [%[ptr]], #2         \n\t"
#endif
#endif
        "lsls       %[range] , %[low], #16          \n\t"
        "bne        1f                              \n\t"

        "str        %[ptr]   , [%[c], %[ptr_off]]   \n\t"
        "rev        %[tmp]   , %[tmp]               \n\t"
        "add        %[low]   , %[low], %[tmp], lsr #15 \n\t"
        "movw       %[tmp]   , 0xFFFF               \n\t"
        "sub        %[low]   , %[tmp]               \n\t"
        "1:                                         \n\t"
        "str        %[low]   , [%[c], %[low_off]]   \n\t"
        : // Outputs
               [rv]"=&r"(rv),
              [low]"+r"(low),
            [range]"=&r"(range),
              [ptr]"=&r"(ptr),
              [tmp]"=&r"(tmp)
        : // Inputs
                    [c]"r"(c),
              [low_off]"J"(offsetof(CABACContext, low)),
            [range_off]"J"(offsetof(CABACContext, range)),
              [ptr_off]"J"(offsetof(CABACContext, bytestream)),
              [end_off]"J"(offsetof(CABACContext, bytestream_end))
        : // Clobbers
            "memory", "cc"
    );
    return rv;
}


#define get_cabac_bypass_sign get_cabac_bypass_sign_arm
static inline int get_cabac_bypass_sign_arm(CABACContext * const c, int rv)
{
    uint32_t low = c->low, range, ptr, tmp;
    __asm volatile (
        "ldr        %[range] , [%[c], %[range_off]] \n\t"
        "ldr        %[ptr]   , [%[c], %[ptr_off]]   \n\t"
        "lsl        %[low]   , #1                   \n\t"
#if !UNCHECKED_BITSTREAM_READER
        "ldr        %[tmp]   , [%[c], %[end_off]]   \n\t"
#endif
        "cmp        %[low]   , %[range], lsl #17    \n\t"
        "it         cs                              \n\t"
        "subcs      %[low]   , %[low], %[range], lsl #17 \n\t"
        "it         cc                              \n\t"
        "rsbcc      %[rv]    , %[rv], #0            \n\t"
#if UNCHECKED_BITSTREAM_READER
        "ldrh       %[tmp]   , [%[ptr]], #2         \n\t"
#else
        "cmp        %[tmp]   , %[ptr]               \n\t"
#if CONFIG_THUMB
        "it         cs                              \n\t"
        "ldrhcs     %[tmp]   , [%[ptr]], #2         \n\t"
#else
        "ldrcsh     %[tmp]   , [%[ptr]], #2         \n\t"
#endif
#endif
        "lsls       %[range] , %[low], #16          \n\t"
        "bne        1f                              \n\t"

        "str        %[ptr]   , [%[c], %[ptr_off]]   \n\t"
        "rev        %[tmp]   , %[tmp]               \n\t"
        "add        %[low]   , %[low], %[tmp], lsr #15 \n\t"
        "movw       %[tmp]   , 0xFFFF               \n\t"
        "sub        %[low]   , %[tmp]               \n\t"
        "1:                                         \n\t"
        "str        %[low]   , [%[c], %[low_off]]   \n\t"
        : // Outputs
               [rv]"+r"(rv),
              [low]"+r"(low),
            [range]"=&r"(range),
              [ptr]"=&r"(ptr),
              [tmp]"=&r"(tmp)
        : // Inputs
                    [c]"r"(c),
              [low_off]"J"(offsetof(CABACContext, low)),
            [range_off]"J"(offsetof(CABACContext, range)),
              [ptr_off]"J"(offsetof(CABACContext, bytestream)),
              [end_off]"J"(offsetof(CABACContext, bytestream_end))
        : // Clobbers
            "memory", "cc"
    );
    return rv;
}

#endif /* HAVE_ARMV6T2_INLINE */

#endif /* AVCODEC_ARM_CABAC_H */
