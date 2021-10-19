/*
Copyright (c) 2017 Raspberry Pi (Trading) Ltd.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Written by John Cox, Ben Avison
*/

#ifndef AVCODEC_ARM_RPI_HEVC_MV_H
#define AVCODEC_ARM_RPI_HEVC_MV_H

#if HAVE_ARMV6T2_INLINE
static inline MvXY mvxy_add_arm(const MvXY a, const MvXY b)
{
    MvXY r;
    __asm__ (
        "sadd16    %[r], %[a], %[b]        \n\t"
        : [r]"=r"(r)
        : [a]"r"(a),
          [b]"r"(b)
        :
        );
    return r;
}
#define mvxy_add mvxy_add_arm
#endif

#if HAVE_ARMV6T2_INLINE
#if (defined(__ARM_ARCH_EXT_IDIV__) || defined (__ARM_FEATURE_IDIV))
static inline int32_t mv_scale_xy_arm(int32_t xy, int td, int tb)
{
    int t;
    __asm__ (
    "ssat   %[td], #8,    %[td]          \n\t"
    "ssat   %[tb], #8,    %[tb]          \n\t"
    "eor    %[t],  %[td], %[td], asr #31 \n\t"
    "adds   %[t],  %[t],  %[td], lsr #31 \n\t"
    "asr    %[t],  #1                    \n\t"
    "add    %[t],  #0x4000               \n\t"
    "it ne                               \n\t"
    "sdivne %[t],  %[t],  %[td]          \n\t"
    "mov    %[td], #32                   \n\t"
    "smlabb %[td], %[t],  %[tb], %[td]   \n\t"
    "ssat   %[td], #13,   %[td], asr #6  \n\t"
    "mov    %[tb], #127                  \n\t"
    "smlatb %[t],  %[xy], %[td], %[tb]   \n\t"
    "smlabb %[tb], %[xy], %[td], %[tb]   \n\t"
// This takes the sign of x & y for rounding at the "wrong" point
// (i.e. after adding 127) but for the range of values (-1,-127)
// where it does the wrong thing you get the right answer (0) anyway
    "add    %[t],  %[t],  %[t],  lsr #31 \n\t"
    "add    %[xy], %[tb], %[tb], lsr #31 \n\t"
    "ssat   %[t],  #16,   %[t],  asr #8  \n\t"
    "ssat   %[xy], #16,   %[xy], asr #8  \n\t"
    "pkhbt  %[xy], %[xy], %[t],  lsl #16 \n\t"
    :
         [t]"=&r"(t),
        [xy]"+r"(xy),
        [td]"+r"(td),
        [tb]"+r"(tb)
    :
    :
        "cc"
    );
    return xy;
}
#define mv_scale_xy mv_scale_xy_arm
#endif
#endif

#endif // AVCODEC_ARM_RPI_HEVC_MV_H

