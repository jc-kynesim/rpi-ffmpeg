#include "libavutil/arm/asm.S"
#include "neon.S"

#define BIT_DEPTH 10

.macro clip16_4 Q0, Q1, Q2, Q3, Q_MIN, Q_MAX
        vmax.s16  \Q0, \Q_MIN
        vmax.s16  \Q1, \Q_MIN
        vmax.s16  \Q2, \Q_MIN
        vmax.s16  \Q3, \Q_MIN
        vmin.s16  \Q0, \Q_MAX
        vmin.s16  \Q1, \Q_MAX
        vmin.s16  \Q2, \Q_MAX
        vmin.s16  \Q3, \Q_MAX
.endm

@ add_residual4x4(
@  uint8_t *_dst,     [r0]
@  int16_t *res,      [r1]
@  ptrdiff_t stride)  [r2]

function JOIN(ff_hevc_add_residual_4x4_neon_, BIT_DEPTH), export=1
        vld1.16     {q10, q11}, [r1]
        movw        r3,  #(1 << BIT_DEPTH) - 1
        vld1.16     {d0}, [r0, :64], r2
        vld1.16     {d1}, [r0, :64], r2
        vld1.16     {d2}, [r0, :64], r2
        vld1.16     {d3}, [r0, :64], r2
        vmov.i64    q8,  #0
        vdup.i16    q9,  r3
        vqadd.s16   q0,  q10
        vqadd.s16   q1,  q11
        sub         r0,  r0,  r2, lsl #2
        vmax.s16    q0,  q0,  q8
        vmax.s16    q1,  q1,  q8
        vmin.s16    q0,  q0,  q9
        vmin.s16    q1,  q1,  q9
        vst1.16     {d0}, [r0, :64], r2
        vst1.16     {d1}, [r0, :64], r2
        vst1.16     {d2}, [r0, :64], r2
        vst1.16     {d3}, [r0, :64], r2
        bx          lr

endfunc

@ add_residual4x4(
@  uint8_t *_dst,     [r0]
@  ptrdiff_t stride,  [r1]
@  int dc)            [r2]

function JOIN(ff_hevc_add_residual_4x4_dc_neon_, BIT_DEPTH), export=1
        movw        r3,  #(1 << BIT_DEPTH) - 1
        vdup.i16    q9,  r3
        vld1.16     {d0}, [r0, :64], r1
        vld1.16     {d1}, [r0, :64], r1
        vdup.16     q15, r2
        vld1.16     {d2}, [r0, :64], r1
        vld1.16     {d3}, [r0, :64], r1
        vmov.i64    q8,  #0
        vdup.i16    q9,  r3
        vqadd.s16   q0,  q15
        vqadd.s16   q1,  q15
        sub         r0,  r0,  r1, lsl #2
        vmax.s16    q0,  q0,  q8
        vmax.s16    q1,  q1,  q8
        vmin.s16    q0,  q0,  q9
        vmin.s16    q1,  q1,  q9
        vst1.16     {d0}, [r0, :64], r1
        vst1.16     {d1}, [r0, :64], r1
        vst1.16     {d2}, [r0, :64], r1
        vst1.16     {d3}, [r0, :64], r1
        bx          lr

endfunc


@ add_residual8x8(
@  uint8_t *_dst,     [r0]
@  int16_t *res,      [r1]
@  ptrdiff_t stride)  [r2]

function JOIN(ff_hevc_add_residual_8x8_neon_, BIT_DEPTH), export=1
        movw        r3,  #(1 << BIT_DEPTH) - 1
        vmov.i64    q8,  #0
        vdup.i16    q9,  r3
        mov         r12, #2
1:
        vldm        r1!, {q10-q13}
        vld1.16     {q0}, [r0, :128], r2
        subs        r12, #1
        vld1.16     {q1}, [r0, :128], r2
        vqadd.s16   q0,  q10
        vld1.16     {q2}, [r0, :128], r2
        vqadd.s16   q1,  q11
        vld1.16     {q3}, [r0, :128], r2
        vqadd.s16   q2,  q12
        vqadd.s16   q3,  q13
        sub         r0,  r0,  r2, lsl #2
        vmax.s16    q0,  q0,  q8
        vmax.s16    q1,  q1,  q8
        vmax.s16    q2,  q2,  q8
        vmax.s16    q3,  q3,  q8
        vmin.s16    q0,  q0,  q9
        vmin.s16    q1,  q1,  q9
        vst1.16     {q0}, [r0, :128], r2
        vmin.s16    q2,  q2,  q9
        vst1.16     {q1}, [r0, :128], r2
        vmin.s16    q3,  q3,  q9
        vst1.16     {q2}, [r0, :128], r2
        vst1.16     {q3}, [r0, :128], r2
        bne         1b
        bx          lr

endfunc

@ add_residual4x4_dc_c(
@  uint8_t *_dst,     [r0]
@  ptrdiff_t stride,  [r1]
@  int dc_uv)         [r2]

function JOIN(ff_hevc_add_residual_4x4_dc_c_neon_, BIT_DEPTH), export=1
        mov         r12, #1
        vdup.32     q15, r2
        b           9f
endfunc

@ add_residual8x8_dc(
@  uint8_t *_dst,     [r0]
@  ptrdiff_t stride,  [r1]
@  int dc)            [r2]

function JOIN(ff_hevc_add_residual_8x8_dc_neon_, BIT_DEPTH), export=1
        mov         r12, #2
        vdup.16     q15, r2
9:
        movw        r3,  #(1 << BIT_DEPTH) - 1
        vmov.i64    q8,  #0
        vdup.i16    q9,  r3
1:
        vld1.16     {q0}, [r0, :128], r1
        subs        r12, #1
        vld1.16     {q1}, [r0, :128], r1
        vqadd.s16   q0,  q15
        vld1.16     {q2}, [r0, :128], r1
        vqadd.s16   q1,  q15
        vld1.16     {q3}, [r0, :128], r1
        vqadd.s16   q2,  q15
        vqadd.s16   q3,  q15
        sub         r0,  r0,  r1, lsl #2
        vmax.s16    q0,  q8
        vmax.s16    q1,  q8
        vmax.s16    q2,  q8
        vmax.s16    q3,  q8
        vmin.s16    q0,  q9
        vmin.s16    q1,  q9
        vst1.16     {q0}, [r0, :128], r1
        vmin.s16    q2,  q9
        vst1.16     {q1}, [r0, :128], r1
        vmin.s16    q3,  q9
        vst1.16     {q2}, [r0, :128], r1
        vst1.16     {q3}, [r0, :128], r1
        bne         1b
        bx          lr

endfunc

@ add_residual16x16(
@  uint8_t *_dst,     [r0]
@  int16_t *res,      [r1]
@  ptrdiff_t stride)  [r2]

function JOIN(ff_hevc_add_residual_16x16_neon_, BIT_DEPTH), export=1
        movw        r3,  #(1 << BIT_DEPTH) - 1
        vmov.i64    q8,  #0
        vdup.i16    q9,  r3
        mov         r12, #8
1:
        vldm        r1!, {q10-q13}
        @ For RPI Sand we could guarantee :256 but not for general
        @ non-RPI allocation. :128 is as good as we can claim
        vld1.16     {q0, q1}, [r0, :128], r2
        subs        r12, #1
        vld1.16     {q2, q3}, [r0, :128]
        vqadd.s16   q0,  q10
        vqadd.s16   q1,  q11
        vqadd.s16   q2,  q12
        vqadd.s16   q3,  q13
        sub         r0,  r2
        vmax.s16    q0,  q0,  q8
        vmax.s16    q1,  q1,  q8
        vmax.s16    q2,  q2,  q8
        vmax.s16    q3,  q3,  q8
        vmin.s16    q0,  q0,  q9
        vmin.s16    q1,  q1,  q9
        vmin.s16    q2,  q2,  q9
        vmin.s16    q3,  q3,  q9
        vst1.16     {q0, q1}, [r0, :128], r2
        vst1.16     {q2, q3}, [r0, :128], r2
        bne         1b
        bx          lr
endfunc

@ add_residual8x8_dc_c(
@  uint8_t *_dst,     [r0]
@  ptrdiff_t stride,  [r1]
@  int dc_uv)         [r2]

function JOIN(ff_hevc_add_residual_8x8_dc_c_neon_, BIT_DEPTH), export=1
        mov         r12, #4
        vdup.32     q15, r2
        b           9f
endfunc

@ add_residual16x16_dc(
@  uint8_t *_dst,     [r0]
@  ptrdiff_t stride,  [r1]
@  int dc)            [r2]

function JOIN(ff_hevc_add_residual_16x16_dc_neon_, BIT_DEPTH), export=1
        vdup.i16    q15, r2
        mov         r12, #8
9:
        movw        r3,  #(1 << BIT_DEPTH) - 1
        vmov.i64    q8,  #0
        vdup.i16    q9,  r3
1:
        @ For RPI Sand we could guarantee :256 but not for general
        @ non-RPI allocation. :128 is as good as we can claim
        vld1.16     {q0, q1}, [r0, :128], r1
        subs        r12, #1
        vld1.16     {q2, q3}, [r0, :128]
        vqadd.s16   q0,  q15
        vqadd.s16   q1,  q15
        vqadd.s16   q2,  q15
        vqadd.s16   q3,  q15
        sub         r0,  r1
        clip16_4 q0, q1, q2, q3, q8, q9
        vst1.16     {q0, q1}, [r0, :128], r1
        vst1.16     {q2, q3}, [r0, :128], r1
        bne         1b
        bx          lr

endfunc


@ add_residual32x32(
@  uint8_t *_dst,     [r0]
@  int16_t *res,      [r1]
@  ptrdiff_t stride)  [r2]

function JOIN(ff_hevc_add_residual_32x32_neon_, BIT_DEPTH), export=1
        movw        r3,  #(1 << BIT_DEPTH) - 1
        vmov.i64    q8,  #0
        vdup.i16    q9,  r3
        mov         r12, #32
1:
        vldm        r1!, {q10-q13}
        vldm        r0,  {q0-q3}
        subs        r12, #1
        vqadd.s16   q0,  q10
        vqadd.s16   q1,  q11
        vqadd.s16   q2,  q12
        vqadd.s16   q3,  q13
        clip16_4 q0, q1, q2, q3, q8, q9
        vstm        r0,  {q0-q3}
        add         r0,  r2
        bne         1b
        bx          lr

endfunc

@ add_residual8x8_dc_c(
@  uint8_t *_dst,     [r0]
@  ptrdiff_t stride,  [r1]
@  int dc_uv)         [r2]

function JOIN(ff_hevc_add_residual_16x16_dc_c_neon_, BIT_DEPTH), export=1
        mov         r12, #16
        vdup.32     q15, r2
        b           9f
endfunc

@ add_residual32x32_dc(
@  uint8_t *_dst,     [r0]
@  ptrdiff_t stride,  [r1]
@  int dc)            [r2]

function JOIN(ff_hevc_add_residual_32x32_dc_neon_, BIT_DEPTH), export=1
        vdup.i16    q15, r2
        mov         r12, #32
9:
        movw        r3,  #(1 << BIT_DEPTH) - 1
        vmov.i64    q8,  #0
        vdup.i16    q9,  r3
1:
        vldm        r0,  {q0-q3}
        subs        r12, #1
        vqadd.s16   q0,  q15
        vqadd.s16   q1,  q15
        vqadd.s16   q2,  q15
        vqadd.s16   q3,  q15
        clip16_4 q0, q1, q2, q3, q8, q9
        vstm        r0,  {q0-q3}
        add         r0,  r1
        bne         1b
        bx          lr

endfunc

@ ============================================================================
@ U add

@ add_residual4x4_u(
@   uint8_t *_dst,        [r0]
@   const int16_t *res,   [r1]
@   ptrdiff_t stride,     [r2]
@   int dc)               [r3]

function JOIN(ff_hevc_add_residual_4x4_u_neon_, BIT_DEPTH), export=1
        vld1.16     {q10, q11}, [r1, :256]
        vdup.16     q15, r3
        movw        r3,  #(1 << BIT_DEPTH) - 1
        vmov.i64    q8,  #0
        vdup.i16    q9,  r3

        vld2.16     {d0, d2}, [r0, :128], r2
        vld2.16     {d1, d3}, [r0, :128], r2
        vld2.16     {d4, d6}, [r0, :128], r2
        vld2.16     {d5, d7}, [r0, :128], r2

        vqadd.s16   q0,  q10
        vqadd.s16   q1,  q15
        vqadd.s16   q2,  q11
        vqadd.s16   q3,  q15
        sub         r0,  r0,  r2, lsl #2
        clip16_4 q0, q1, q2, q3, q8, q9

        vst2.16     {d0, d2}, [r0, :128], r2
        vst2.16     {d1, d3}, [r0, :128], r2
        vst2.16     {d4, d6}, [r0, :128], r2
        vst2.16     {d5, d7}, [r0, :128]
        bx          lr
endfunc

@ add_residual8x8_u(
@   uint8_t *_dst,        [r0]
@   const int16_t *res,   [r1]
@   ptrdiff_t stride,     [r2]
@   int dc)               [r3]

function JOIN(ff_hevc_add_residual_8x8_u_neon_, BIT_DEPTH), export=1
        vdup.16     q15, r3
        movw        r3,  #(1 << BIT_DEPTH) - 1
        vmov.i64    q8,  #0
        mov         r12, #4
        vdup.i16    q9,  r3
1:
        vld2.16     {q0, q1}, [r0, :256], r2
        vld2.16     {q2, q3}, [r0, :256]
        vld1.16     {q10, q11}, [r1, :256]!
        subs        r12, #1
        vqadd.s16   q0,  q10
        vqadd.s16   q1,  q15
        vqadd.s16   q2,  q11
        vqadd.s16   q3,  q15
        sub         r0,  r2
        clip16_4 q0, q1, q2, q3, q8, q9
        vst2.16     {q0, q1}, [r0, :256], r2
        vst2.16     {q2, q3}, [r0, :256], r2
        bne         1b
        bx          lr
endfunc

@ add_residual16x16_u(
@   uint8_t *_dst,        [r0]
@   const int16_t *res,   [r1]
@   ptrdiff_t stride,     [r2]
@   int dc)               [r3]

function JOIN(ff_hevc_add_residual_16x16_u_neon_, BIT_DEPTH), export=1
        vdup.16     q15, r3
        movw        r3,  #(1 << BIT_DEPTH) - 1
        vmov.i64    q8,  #0
        mov         r12, #16
        vdup.i16    q9,  r3
        sub         r2,  #32
1:
        vld2.16     {q0, q1}, [r0, :256]!
        vld2.16     {q2, q3}, [r0, :256]
        vld1.16     {q10, q11}, [r1, :256]!
        subs        r12, #1
        vqadd.s16   q0,  q10
        vqadd.s16   q1,  q15
        vqadd.s16   q2,  q11
        vqadd.s16   q3,  q15
        sub         r0,  #32
        clip16_4 q0, q1, q2, q3, q8, q9
        vst2.16     {q0, q1}, [r0, :256]!
        vst2.16     {q2, q3}, [r0, :256], r2
        bne         1b
        bx          lr
endfunc

@ ============================================================================
@ V add

@ add_residual4x4_v(
@   uint8_t *_dst,        [r0]
@   const int16_t *res,   [r1]
@   ptrdiff_t stride,     [r2]
@   int dc)               [r3]

function JOIN(ff_hevc_add_residual_4x4_v_neon_, BIT_DEPTH), export=1
        vld1.16     {q10, q11}, [r1, :256]
        vdup.16     q15, r3
        movw        r3,  #(1 << BIT_DEPTH) - 1
        vmov.i64    q8,  #0
        vdup.i16    q9,  r3

        vld2.16     {d0, d2}, [r0, :128], r2
        vld2.16     {d1, d3}, [r0, :128], r2
        vld2.16     {d4, d6}, [r0, :128], r2
        vld2.16     {d5, d7}, [r0, :128], r2

        vqadd.s16   q0,  q15
        vqadd.s16   q1,  q10
        vqadd.s16   q2,  q15
        vqadd.s16   q3,  q11
        sub         r0,  r0,  r2, lsl #2
        clip16_4 q0, q1, q2, q3, q8, q9

        vst2.16     {d0, d2}, [r0, :128], r2
        vst2.16     {d1, d3}, [r0, :128], r2
        vst2.16     {d4, d6}, [r0, :128], r2
        vst2.16     {d5, d7}, [r0, :128]
        bx          lr
endfunc

@ add_residual8x8_v(
@   uint8_t *_dst,        [r0]
@   const int16_t *res,   [r1]
@   ptrdiff_t stride,     [r2]
@   int dc)               [r3]

function JOIN(ff_hevc_add_residual_8x8_v_neon_, BIT_DEPTH), export=1
        vdup.16     q15, r3
        movw        r3,  #(1 << BIT_DEPTH) - 1
        vmov.i64    q8,  #0
        mov         r12, #4
        vdup.i16    q9,  r3
1:
        vld2.16     {q0, q1}, [r0, :256], r2
        vld2.16     {q2, q3}, [r0, :256]
        vld1.16     {q10, q11}, [r1, :256]!
        subs        r12, #1
        vqadd.s16   q0,  q15
        vqadd.s16   q1,  q10
        vqadd.s16   q2,  q15
        vqadd.s16   q3,  q11
        sub         r0,  r2
        clip16_4 q0, q1, q2, q3, q8, q9
        vst2.16     {q0, q1}, [r0, :256], r2
        vst2.16     {q2, q3}, [r0, :256], r2
        bne         1b
        bx          lr
endfunc

@ add_residual16x16_v(
@   uint8_t *_dst,        [r0]
@   const int16_t *res,   [r1]
@   ptrdiff_t stride,     [r2]
@   int dc)               [r3]

function JOIN(ff_hevc_add_residual_16x16_v_neon_, BIT_DEPTH), export=1
        vdup.16     q15, r3
        movw        r3,  #(1 << BIT_DEPTH) - 1
        vmov.i64    q8,  #0
        mov         r12, #16
        vdup.i16    q9,  r3
        sub         r2,  #32
1:
        vld2.16     {q0, q1}, [r0, :256]!
        vld2.16     {q2, q3}, [r0, :256]
        vld1.16     {q10, q11}, [r1, :256]!
        subs        r12, #1
        vqadd.s16   q0,  q15
        vqadd.s16   q1,  q10
        vqadd.s16   q2,  q15
        vqadd.s16   q3,  q11
        sub         r0,  #32
        clip16_4 q0, q1, q2, q3, q8, q9
        vst2.16     {q0, q1}, [r0, :256]!
        vst2.16     {q2, q3}, [r0, :256], r2
        bne         1b
        bx          lr
endfunc

@ ============================================================================
@ U & V add

@ add_residual4x4_c(
@   uint8_t *_dst,        [r0]
@   const int16_t *res,   [r1]
@   ptrdiff_t stride)     [r2]

function JOIN(ff_hevc_add_residual_4x4_c_neon_, BIT_DEPTH), export=1
        vldm        r1, {q10-q13}
        movw        r3,  #(1 << BIT_DEPTH) - 1
        vmov.i64    q8,  #0
        vdup.i16    q9,  r3

        vld2.16     {d0, d2}, [r0, :128], r2
        vld2.16     {d1, d3}, [r0, :128], r2
        vld2.16     {d4, d6}, [r0, :128], r2
        vld2.16     {d5, d7}, [r0, :128], r2

        vqadd.s16   q0,  q10
        vqadd.s16   q2,  q11
        vqadd.s16   q1,  q12
        vqadd.s16   q3,  q13
        sub         r0,  r0,  r2, lsl #2
        vmax.s16    q0,  q0,  q8
        vmax.s16    q1,  q1,  q8
        vmax.s16    q2,  q2,  q8
        vmax.s16    q3,  q3,  q8
        vmin.s16    q0,  q0,  q9
        vmin.s16    q1,  q1,  q9
        vmin.s16    q2,  q2,  q9
        vmin.s16    q3,  q3,  q9

        vst2.16     {d0, d2}, [r0, :128], r2
        vst2.16     {d1, d3}, [r0, :128], r2
        vst2.16     {d4, d6}, [r0, :128], r2
        vst2.16     {d5, d7}, [r0, :128]
        bx          lr
endfunc

@ add_residual8x8_c(
@   uint8_t *_dst,        [r0]
@   const int16_t *res,   [r1]
@   ptrdiff_t stride)     [r2]

function JOIN(ff_hevc_add_residual_8x8_c_neon_, BIT_DEPTH), export=1
        movw        r3,  #(1 << BIT_DEPTH) - 1
        vmov.i64    q8,  #0
        mov         r12, #4
        vdup.i16    q9,  r3
        add         r3, r1, #(8*8*2)  @ Offset to V
1:
        vld2.16     {q0, q1}, [r0, :256], r2
        vld2.16     {q2, q3}, [r0, :256]
        vld1.16     {q10, q11}, [r1, :256]!
        vld1.16     {q12, q13}, [r3, :256]!
        subs        r12, #1
        vqadd.s16   q0,  q10
        vqadd.s16   q2,  q11
        vqadd.s16   q1,  q12
        vqadd.s16   q3,  q13
        sub         r0,  r2
        vmax.s16    q0,  q0,  q8
        vmax.s16    q1,  q1,  q8
        vmax.s16    q2,  q2,  q8
        vmax.s16    q3,  q3,  q8
        vmin.s16    q0,  q0,  q9
        vmin.s16    q1,  q1,  q9
        vmin.s16    q2,  q2,  q9
        vmin.s16    q3,  q3,  q9
        vst2.16     {q0, q1}, [r0, :256], r2
        vst2.16     {q2, q3}, [r0, :256], r2
        bne         1b
        bx          lr
endfunc

@ add_residual16x16_c(
@   uint8_t *_dst,        [r0]
@   const int16_t *res,   [r1]
@   ptrdiff_t stride)     [r2]

function JOIN(ff_hevc_add_residual_16x16_c_neon_, BIT_DEPTH), export=1
        movw        r3,  #(1 << BIT_DEPTH) - 1
        vmov.i64    q8,  #0
        mov         r12, #16
        vdup.i16    q9,  r3
        add         r3,  r1, #(16*16*2)  @ Offset to V
        sub         r2,  #32
1:
        vld2.16     {q0, q1}, [r0, :256]!
        vld2.16     {q2, q3}, [r0, :256]
        vld1.16     {q10, q11}, [r1, :256]!
        vld1.16     {q12, q13}, [r3, :256]!
        subs        r12, #1
        vqadd.s16   q0,  q10
        vqadd.s16   q2,  q11
        vqadd.s16   q1,  q12
        vqadd.s16   q3,  q13
        sub         r0,  #32
        vmax.s16    q0,  q0,  q8
        vmax.s16    q1,  q1,  q8
        vmax.s16    q2,  q2,  q8
        vmax.s16    q3,  q3,  q8
        vmin.s16    q0,  q0,  q9
        vmin.s16    q1,  q1,  q9
        vmin.s16    q2,  q2,  q9
        vmin.s16    q3,  q3,  q9
        vst2.16     {q0, q1}, [r0, :256]!
        vst2.16     {q2, q3}, [r0, :256], r2
        bne         1b
        bx          lr
endfunc

