@ Included multiple times from hevc_idct_neon.S
@ Macros defined there

#define DC_SHIFT  (15 - BIT_DEPTH)
#define DC_ADD    (1 | (1 << (14 - BIT_DEPTH)))
#define TRN_SHIFT (20 - BIT_DEPTH)

function JOIN(ff_hevc_rpi_idct_4x4_dc_neon_, BIT_DEPTH), export=1
        ldrsh       r1, [r0]
        add         r1, #DC_ADD
        asr         r1, #DC_SHIFT
        vdup.16     q0, r1
        vdup.16     q1, r1
        vst1.16     {q0, q1}, [r0]
        bx lr
endfunc

function JOIN(ff_hevc_rpi_idct_8x8_dc_neon_, BIT_DEPTH), export=1
        ldrsh       r1, [r0]
        add         r1, #DC_ADD
        asr         r1, #DC_SHIFT
        vdup.16     q8, r1
        vdup.16     q9, r1
        vmov.16     q10, q8
        vmov.16     q11, q8
        vmov.16     q12, q8
        vmov.16     q13, q8
        vmov.16     q14, q8
        vmov.16     q15, q8
        vstm        r0, {q8-q15}
        bx lr
endfunc

function JOIN(ff_hevc_rpi_idct_16x16_dc_neon_, BIT_DEPTH), export=1
        ldrsh       r1, [r0]
        add         r1, #DC_ADD
        asr         r1, #DC_SHIFT
        vdup.16     q8, r1
        vdup.16     q9, r1
        vmov.16     q10, q8
        vmov.16     q11, q8
        vmov.16     q12, q8
        vmov.16     q13, q8
        vmov.16     q14, q8
        vmov.16     q15, q8
        vstm        r0!, {q8-q15}
        vstm        r0!, {q8-q15}
        vstm        r0!, {q8-q15}
        vstm        r0, {q8-q15}
        bx lr
endfunc

function JOIN(ff_hevc_rpi_idct_32x32_dc_neon_, BIT_DEPTH), export=1
        ldrsh       r1, [r0]
        add         r1, #DC_ADD
        asr         r1, #DC_SHIFT
        mov         r3, #16
        vdup.16     q8, r1
        vdup.16     q9, r1
        vmov.16     q10, q8
        vmov.16     q11, q8
        vmov.16     q12, q8
        vmov.16     q13, q8
        vmov.16     q14, q8
        vmov.16     q15, q8
1:      subs        r3, #1
        vstm        r0!, {q8-q15}
        bne         1b
        bx lr
endfunc


function JOIN(ff_hevc_rpi_transform_4x4_neon_, BIT_DEPTH), export=1
        vpush       {d8-d15}
        vld1.16     {q14, q15}, [r0]  // coeffs
        ldr         r3, =0x00240053 // 36 and 83
        vmov.32     d0[0], r3

        tr4_shift d28, d29, d30, d31, #7

        vtrn.16     d28, d29
        vtrn.16     d30, d31
        vtrn.32     q14, q15

        tr4_shift d28, d29, d30, d31, #(TRN_SHIFT)

        vtrn.16     d28, d29
        vtrn.16     d30, d31
        vtrn.32     q14, q15

        vst1.16     {q14, q15}, [r0]
        vpop        {d8-d15}
        bx lr
endfunc



function JOIN(ff_hevc_rpi_transform_luma_4x4_neon_, BIT_DEPTH), export=1
        vpush       {d8-d15}
        vld1.16     {q14, q15}, [r0]  // coeffs
        ldr         r3, =0x4a  // 74
        vmov.32     d0[0], r3
        ldr         r3, =0x1d  // 29
        vmov.32     d0[1], r3
        ldr         r3, =0x37  // 55
        vmov.32     d1[0], r3

        tr4_luma_shift d28, d29, d30, d31, #7

        vtrn.16     d28, d29
        vtrn.16     d30, d31
        vtrn.32     q14, q15

        tr4_luma_shift d28, d29, d30, d31, #(TRN_SHIFT)

        vtrn.16     d28, d29
        vtrn.16     d30, d31
        vtrn.32     q14, q15
        vst1.16     {q14, q15}, [r0]
        vpop        {d8-d15}
        bx lr
endfunc



function JOIN(ff_hevc_rpi_transform_8x8_neon_, BIT_DEPTH), export=1
        push   {r4-r8}
        vpush {d8-d15}
        mov    r5, #16

        adrl      r3, tr4f
        vld1.16   {d0, d1}, [r3]

        // left half
        vld1.16 {d24}, [r0], r5
        vld1.16 {d25}, [r0], r5
        vld1.16 {d26}, [r0], r5
        vld1.16 {d27}, [r0], r5
        vld1.16 {d28}, [r0], r5
        vld1.16 {d29}, [r0], r5
        vld1.16 {d30}, [r0], r5
        vld1.16 {d31}, [r0], r5
        sub      r0, #128
        tr8_begin d25, d27, d29, d31
        tr4       d24, d26, d28, d30
        tr8_end   #7
        vst1.16 {d2}, [r0], r5
        vst1.16 {d3}, [r0], r5
        vst1.16 {d4}, [r0], r5
        vst1.16 {d5}, [r0], r5
        vst1.16 {d6}, [r0], r5
        vst1.16 {d7}, [r0], r5
        vst1.16 {d8}, [r0], r5
        vst1.16 {d9}, [r0], r5
        sub      r0, #128
        //skip right half if col_limit in r1 is less than 4
        cmp      r1, #4
        blt      1f
        //right half
        add      r0, #8
        vld1.16 {d24}, [r0], r5
        vld1.16 {d25}, [r0], r5
        vld1.16 {d26}, [r0], r5
        vld1.16 {d27}, [r0], r5
        vld1.16 {d28}, [r0], r5
        vld1.16 {d29}, [r0], r5
        vld1.16 {d30}, [r0], r5
        vld1.16 {d31}, [r0], r5
        sub      r0, #128
        tr8_begin d25, d27, d29, d31
        tr4       d24, d26, d28, d30
        tr8_end   #7
        vst1.16 {d2}, [r0], r5
        vst1.16 {d3}, [r0], r5
        vst1.16 {d4}, [r0], r5
        vst1.16 {d5}, [r0], r5
        vst1.16 {d6}, [r0], r5
        vst1.16 {d7}, [r0], r5
        vst1.16 {d8}, [r0], r5
        vst1.16 {d9}, [r0], r5
        sub      r0, #136
1:
        // top half
        vldm r0, {q12-q15} // coeffs
        transpose_16b_4x4 d24, d26, d28, d30
        transpose_16b_4x4 d25, d27, d29, d31
        tr8_begin d26, d30, d27, d31
        tr4 d24, d28, d25, d29
        tr8_end #(TRN_SHIFT)
        transpose_16b_4x4 d2, d3, d4, d5
        transpose_16b_4x4 d6, d7, d8, d9
        vswp     d7, d5
        vswp     d7, d8
        vswp     d3, d6
        vswp     d6, d4
        vstm r0!, {q1-q4}

        // bottom half
        vldm r0, {q12-q15} // coeffs
        transpose_16b_4x4 d24, d26, d28, d30
        transpose_16b_4x4 d25, d27, d29, d31
        tr8_begin d26, d30, d27, d31
        tr4 d24, d28, d25, d29
        tr8_end #(TRN_SHIFT)
        transpose_16b_4x4 d2, d3, d4, d5
        transpose_16b_4x4 d6, d7, d8, d9
        vswp     d7, d5
        vswp     d7, d8
        vswp     d3, d6
        vswp     d6, d4
        //vstm     r0, {q1-q4}
        vst1.16 {q1-q2}, [r0]
        add     r0, #32
        vst1.16 {q3-q4}, [r0]
        sub     r0, #32
        vpop {d8-d15}
        pop {r4-r8}
        bx lr
endfunc

#undef DC_SHIFT
#undef DC_ADD
#undef TRN_SHIFT

