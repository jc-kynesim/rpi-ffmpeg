#include "libavutil/arm/asm.S"
#include "neon.S"

@ General notes:
@
@ Residual is only guaranteed to be cliped to 16 bits
@ This means that we do need to do movul, qadd, qmovun
@ rather than addw, qmovun (if we were clipped to 15 then we could get away
@ with this)

@ ============================================================================
@ U add

@ add_residual4x4_c(
@   uint8_t *_dst,        [r0]
@   const int16_t *res,   [r1]
@   ptrdiff_t stride,     [r2]
@   int dc_v)             [r3]

function ff_hevc_rpi_add_residual_4x4_u_neon_8, export=1
        vld1.8      {d16}, [r0, :64], r2
        vld1.8      {d17}, [r0, :64], r2
        vld1.8      {d18}, [r0, :64], r2
        vld1.8      {d19}, [r0, :64], r2
        vld1.16     {q0, q1}, [r1]
        vdup.16     q2, r3
        vdup.16     q3, r3
        vmovl.u8    q10, d16
        sub         r0, r0, r2, lsl #2
        vmovl.u8    q11, d17
        vmovl.u8    q12, d18
        vmovl.u8    q13, d19
        vzip.16     q0, q2
        vzip.16     q1, q3
        vqadd.s16   q0,  q10
        vqadd.s16   q2,  q11
        vqadd.s16   q1,  q12
        vqadd.s16   q3,  q13
        vqmovun.s16 d0,  q0
        vqmovun.s16 d1,  q2
        vqmovun.s16 d2,  q1
        vqmovun.s16 d3,  q3
        vst1.8      {d0}, [r0, :64], r2
        vst1.8      {d1}, [r0, :64], r2
        vst1.8      {d2}, [r0, :64], r2
        vst1.8      {d3}, [r0, :64]
        bx          lr
endfunc

@ add_residual8x8_c(
@   uint8_t *_dst,        [r0]
@   const int16_t *res,   [r1]
@   ptrdiff_t stride)     [r2]
@   int dc_v)             [r3]

function ff_hevc_rpi_add_residual_8x8_u_neon_8, export=1
        mov         r12,    #4
        vdup.16     q15, r3
1:
        vld2.8      {d16, d17}, [r0, :128], r2
        vld2.8      {d18, d19}, [r0, :128]
        vld1.16     {q0, q1}, [r1, :256]!
        subs        r12, #1
        vmovl.u8    q10, d16
        sub         r0, r2
        vmovl.u8    q11, d18
        vqadd.s16   q0,  q10
        vaddw.u8    q2,  q15, d17
        vqadd.s16   q1,  q11
        vaddw.u8    q3,  q15, d19
        vqmovun.s16 d16,  q0
        vqmovun.s16 d17,  q2
        vqmovun.s16 d18,  q1
        vqmovun.s16 d19,  q3
        vst2.8      {d16, d17}, [r0, :128], r2
        vst2.8      {d18, d19}, [r0, :128], r2
        bne         1b
        bx          lr
endfunc

@ add_residual16x16_u(
@   uint8_t *_dst,        [r0]
@   const int16_t *res,   [r1]
@   ptrdiff_t stride)     [r2]
@   int dc_v)             [r3]

function ff_hevc_rpi_add_residual_16x16_u_neon_8, export=1
        mov         r12,    #16
        vdup.16     q15, r3
1:
        vld2.8      {q8, q9}, [r0, :256]
        vld1.16     {q0, q1}, [r1, :256]!
        subs        r12,   #1
        vmovl.u8    q10, d16
        vmovl.u8    q11, d17
        vqadd.s16   q0,  q10
        vqadd.s16   q1,  q11
        vaddw.u8    q2,  q15, d18
        vaddw.u8    q3,  q15, d19
        vqmovun.s16 d16, q0
        vqmovun.s16 d17, q1
        vqmovun.s16 d18, q2
        vqmovun.s16 d19, q3
        vst2.8      {q8, q9}, [r0, :256], r2
        bne         1b
        bx          lr
endfunc

@ ============================================================================
@ V add

@ add_residual4x4_v(
@   uint8_t *_dst,        [r0]
@   const int16_t *res,   [r1]
@   ptrdiff_t stride)     [r2]

function ff_hevc_rpi_add_residual_4x4_v_neon_8, export=1
        vld1.8      {d16}, [r0, :64], r2
        vld1.8      {d17}, [r0, :64], r2
        vld1.8      {d18}, [r0, :64], r2
        vld1.8      {d19}, [r0, :64], r2
        vld1.16     {q2, q3}, [r1]
        vdup.16     q0, r3
        vdup.16     q1, r3
        vmovl.u8    q10, d16
        sub         r0, r0, r2, lsl #2
        vmovl.u8    q11, d17
        vmovl.u8    q12, d18
        vmovl.u8    q13, d19
        vzip.16     q0, q2
        vzip.16     q1, q3
        vqadd.s16   q0,  q10
        vqadd.s16   q2,  q11
        vqadd.s16   q1,  q12
        vqadd.s16   q3,  q13
        vqmovun.s16 d0,  q0
        vqmovun.s16 d1,  q2
        vqmovun.s16 d2,  q1
        vqmovun.s16 d3,  q3
        vst1.8      {d0}, [r0, :64], r2
        vst1.8      {d1}, [r0, :64], r2
        vst1.8      {d2}, [r0, :64], r2
        vst1.8      {d3}, [r0, :64]
        bx          lr
endfunc

@ add_residual8x8_v(
@   uint8_t *_dst,        [r0]
@   const int16_t *res,   [r1]
@   ptrdiff_t stride)     [r2]

function ff_hevc_rpi_add_residual_8x8_v_neon_8, export=1
        mov         r12,    #4
        vdup.16     q15, r3
1:
        vld2.8      {d16, d17}, [r0, :128], r2
        vld2.8      {d18, d19}, [r0, :128]
        vld1.16     {q0, q1}, [r1, :256]!
        subs        r12, #1
        vmovl.u8    q10, d17
        sub         r0, r2
        vmovl.u8    q11, d19
        vqadd.s16   q0,  q10
        vqadd.s16   q1,  q11
        vaddw.u8    q2,  q15, d16
        vaddw.u8    q3,  q15, d18
        vqmovun.s16 d17,  q0
        vqmovun.s16 d16,  q2
        vqmovun.s16 d19,  q1
        vqmovun.s16 d18,  q3
        vst2.8      {d16, d17}, [r0, :128], r2
        vst2.8      {d18, d19}, [r0, :128], r2
        bne         1b
        bx          lr
endfunc

@ add_residual16x16_v(
@   uint8_t *_dst,        [r0]
@   const int16_t *res,   [r1]
@   ptrdiff_t stride)     [r2]

function ff_hevc_rpi_add_residual_16x16_v_neon_8, export=1
        mov         r12,    #16
        vdup.16     q15, r3
1:
        vld2.8      {q8, q9}, [r0, :256]
        vld1.16     {q0, q1}, [r1, :256]!
        subs        r12,   #1
        vmovl.u8    q10, d18
        vmovl.u8    q11, d19
        vaddw.u8    q2,  q15, d16
        vaddw.u8    q3,  q15, d17
        vqadd.s16   q0,  q10
        vqadd.s16   q1,  q11
        vqmovun.s16 d16, q2
        vqmovun.s16 d17, q3
        vqmovun.s16 d18, q0
        vqmovun.s16 d19, q1
        vst2.8      {q8, q9}, [r0, :256], r2
        bne         1b
        bx          lr
endfunc

@ ============================================================================
@ U & V add

@ add_residual4x4_c(
@   uint8_t *_dst,        [r0]
@   const int16_t *res,   [r1]
@   ptrdiff_t stride)     [r2]

function ff_hevc_rpi_add_residual_4x4_c_neon_8, export=1
        vld1.8      {d16}, [r0, :64], r2
        vld1.8      {d17}, [r0, :64], r2
        vld1.8      {d18}, [r0, :64], r2
        vld1.8      {d19}, [r0, :64], r2
        vldm        r1, {q0-q3}           @ Q0/1 gets all of U, Q2/3 gets all of V
        vmovl.u8    q10, d16
        sub         r0, r0, r2, lsl #2
        vmovl.u8    q11, d17
        vmovl.u8    q12, d18
        vmovl.u8    q13, d19
        vzip.16     q0, q2
        vzip.16     q1, q3
        vqadd.s16   q0,  q10
        vqadd.s16   q2,  q11
        vqadd.s16   q1,  q12
        vqadd.s16   q3,  q13
        vqmovun.s16 d0,  q0
        vqmovun.s16 d1,  q2
        vqmovun.s16 d2,  q1
        vqmovun.s16 d3,  q3
        vst1.8      {d0}, [r0, :64], r2
        vst1.8      {d1}, [r0, :64], r2
        vst1.8      {d2}, [r0, :64], r2
        vst1.8      {d3}, [r0, :64]
        bx          lr
endfunc

@ add_residual8x8_c(
@   uint8_t *_dst,        [r0]
@   const int16_t *res,   [r1]
@   ptrdiff_t stride)     [r2]

function ff_hevc_rpi_add_residual_8x8_c_neon_8, export=1
        mov         r12,    #8
        add         r3, r1, #(8*8*2)  @ Offset to V
1:
        vld2.8      {d16, d17}, [r0, :128]
        vld1.16     {q0}, [r1, :128]!
        vld1.16     {q1}, [r3, :128]!
        subs        r12, #1
        vmovl.u8    q10, d16
        vmovl.u8    q11, d17
        vqadd.s16   q0,  q10
        vqadd.s16   q1,  q11
        vqmovun.s16 d0,  q0
        vqmovun.s16 d1,  q1
        vst2.8      {d0, d1}, [r0, :128], r2
        bne         1b
        bx          lr
endfunc

@ add_residual16x16_c(
@   uint8_t *_dst,        [r0]
@   const int16_t *res,   [r1]
@   ptrdiff_t stride)     [r2]

function ff_hevc_rpi_add_residual_16x16_c_neon_8, export=1
        mov         r12,    #16
        add         r3, r1, #(16*16*2)  @ Offset to V
1:
        vld2.8      {q8, q9}, [r0, :256]
        vld1.16     {q0, q1}, [r1, :256]!
        vld1.16     {q2, q3}, [r3, :256]!
        subs        r12,   #1
        vmovl.u8    q10, d16
        vmovl.u8    q11, d17
        vmovl.u8    q12, d18
        vmovl.u8    q13, d19
        vqadd.s16   q0,  q10
        vqadd.s16   q1,  q11
        vqadd.s16   q2,  q12
        vqadd.s16   q3,  q13
        vqmovun.s16 d0,  q0
        vqmovun.s16 d1,  q1
        vqmovun.s16 d2,  q2
        vqmovun.s16 d3,  q3
        vst2.8      {q0, q1}, [r0, :256], r2
        bne         1b
        bx          lr
endfunc

@ 32x32 chroma never occurs so NIF

@ ============================================================================
