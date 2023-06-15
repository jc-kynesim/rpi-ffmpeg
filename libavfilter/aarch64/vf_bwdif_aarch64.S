#include "libavutil/aarch64/asm.S"

.macro SQSHRUNN b, s0, s1, s2, s3, n
        sqshrun         \s0\().4h, \s0\().4s, #\n - 8
        sqshrun2        \s0\().8h, \s1\().4s, #\n - 8
        sqshrun         \s1\().4h, \s2\().4s, #\n - 8
        sqshrun2        \s1\().8h, \s3\().4s, #\n - 8
        uzp2            \b\().16b, \s0\().16b, \s1\().16b
.endm

.macro SMULL4K a0, a1, a2, a3, s0, s1, k
        smull           \a0\().4s, \s0\().4h, \k
        smull2          \a1\().4s, \s0\().8h, \k
        smull           \a2\().4s, \s1\().4h, \k
        smull2          \a3\().4s, \s1\().8h, \k
.endm

.macro UMULL4K a0, a1, a2, a3, s0, s1, k
        umull           \a0\().4s, \s0\().4h, \k
        umull2          \a1\().4s, \s0\().8h, \k
        umull           \a2\().4s, \s1\().4h, \k
        umull2          \a3\().4s, \s1\().8h, \k
.endm

.macro UMLAL4K a0, a1, a2, a3, s0, s1, k
        umlal           \a0\().4s, \s0\().4h, \k
        umlal2          \a1\().4s, \s0\().8h, \k
        umlal           \a2\().4s, \s1\().4h, \k
        umlal2          \a3\().4s, \s1\().8h, \k
.endm

.macro UMLSL4K a0, a1, a2, a3, s0, s1, k
        umlsl           \a0\().4s, \s0\().4h, \k
        umlsl2          \a1\().4s, \s0\().8h, \k
        umlsl           \a2\().4s, \s1\().4h, \k
        umlsl2          \a3\().4s, \s1\().8h, \k
.endm


// void ff_bwdif_filter_line4_aarch64(
//         void * dst1,         // x0
//         int d_stride,        // w1
//         const void * prev1,  // x2
//         const void * cur1,   // x3
//         const void * next1,  // x4
//         int prefs,           // w5
//         int w,               // w6
//         int parity,          // w7
//         int clip_max);       // [sp, #0] (Ignored)

// static const uint16_t coef_lf[2] = { 4309, 213 };
// static const uint16_t coef_hf[3] = { 5570, 3801, 1016 };
// static const uint16_t coef_sp[2] = { 5077, 981 };

        .align 16

coeffs:
        .hword          4309 * 4, 213 * 4                    // lf[1]*4 = v0.h[1]
        .hword          5570, 3801, 1016, -3801              // hf[0] = v0.h[2], -hf[1] =v0.h[5]
        .hword          5077, 981

function ff_bwdif_filter_line4_aarch64, export=1
// #define prev2 cur
//        const uint8_t * restrict next2 = parity ? prev : next;
        cmp             w7, #0
        csel            x17, x2, x4, ne

        // We want all the V registers - save all the ones we must
        stp             d14, d15, [sp, #-64]!
        stp             d8,  d9,  [sp, #48]
        stp             d10, d11, [sp, #32]
        stp             d12, d13, [sp, #16]

        ldr             q0, coeffs

        // Some rearrangement of initial values for nice layout of refs
        mov             w10, w6                         // w10 = loop count
        neg             w9,  w5                         // w9  = mref
        lsl             w8,  w9,  #1                    // w8 =  mref2
        add             w7,  w9,  w9, LSL #1            // w7  = mref3
        lsl             w6,  w9,  #2                    // w6  = mref4
        mov             w11, w5                         // w11 = pref
        lsl             w12, w5,  #1                    // w12 = pref2
        add             w13, w5,  w5, LSL #1            // w13 = pref3
        lsl             w14, w5,  #2                    // w14 = pref4
        add             w15, w5,  w5, LSL #2            // w15 = pref5
        add             w16, w14, w12                   // w16 = pref6

        lsl             w5,  w1,  #1                    // w5 = d_stride * 2

//         for (x = 0; x < w; x++) {
//             int diff0, diff2;
//             int d0, d2;
//             int temporal_diff0, temporal_diff2;
//
//             int i1, i2;
//             int j1, j2;
//             int p6, p5, p4, p3, p2, p1, c0, m1, m2, m3, m4;

10:
//             c0 = prev2[0] + next2[0];            // c0 = v20, v21
//             d0  = c0 >> 1;                       // d0 = v10
//             temporal_diff0 = FFABS(prev2[0] - next2[0]); // td0 = v11
        ldr             q31, [x3]
        ldr             q21, [x17]
        uhadd           v10.16b, v31.16b, v21.16b
        uabd            v11.16b, v31.16b, v21.16b
        uaddl           v20.8h,  v21.8b,  v31.8b
        uaddl2          v21.8h,  v21.16b, v31.16b

        ldr             q31, [x3, w6, SXTW]
        ldr             q23, [x17, w6, SXTW]

//             i1 = coef_hf[0] * c0;                // i1 = v2-v5
        UMULL4K         v2, v3, v4, v5, v20, v21, v0.h[2]

        ldr             q30, [x3, w14, SXTW]
        ldr             q25, [x17, w14, SXTW]

//             m4 = prev2[mrefs4] + next2[mrefs4];  // m4 = v22,v23
        uaddl           v22.8h,  v23.8b,  v31.8b
        uaddl2          v23.8h,  v23.16b, v31.16b

//             p4 = prev2[prefs4] + next2[prefs4];  // p4 = v24,v25, (p4 >> 1) = v12
        uhadd           v12.16b, v25.16b, v30.16b
        uaddl           v24.8h,  v25.8b,  v30.8b
        uaddl2          v25.8h,  v25.16b, v30.16b

//             j1 = -coef_hf[1] * (c0 + p4);        // j1 = v6-v9  (-c0:v20,v21)
        add             v20.8h,  v20.8h,  v24.8h
        add             v21.8h,  v21.8h,  v25.8h
        SMULL4K         v6, v7, v8, v9, v20, v21, v0.h[5]

//             m3 = cur[mrefs3];                    // m3 = v20
        ldr             q20, [x3, w7, SXTW]

//             p3 = cur[prefs3];                    // p3 = v21
        ldr             q21, [x3, w13, SXTW]

//             i1 += coef_hf[2] * (m4 + p4);        // (-m4:v22,v23) (-p4:v24,v25)
        add             v22.8h,  v22.8h,  v24.8h
        add             v23.8h,  v23.8h,  v25.8h
        UMLAL4K         v2, v3, v4, v5, v22, v23, v0.h[4]

        ldr             q29, [x3, w8, SXTW]
        ldr             q23, [x17, w8, SXTW]

//             i1 -= coef_lf[1] * 4 * (m3 + p3);   // -
        uaddl           v30.8h,  v20.8b,  v21.8b
        uaddl2          v31.8h,  v20.16b, v21.16b

        ldr             q28, [x3, w16, SXTW]
        ldr             q25, [x17, w16, SXTW]

        UMLSL4K         v2, v3, v4, v5, v30, v31, v0.h[1]

//             m2 = prev2[mrefs2] + next2[mrefs2];  // m2 = v22,v23, (m2 >> 1) = v13
        uhadd           v13.16b, v23.16b, v29.16b
        uaddl           v22.8h,  v23.8b,  v29.8b
        uaddl2          v23.8h,  v23.16b, v29.16b

        ldr             q31, [x3, w12, SXTW]
        ldr             q27, [x17, w12, SXTW]

//             p6 = prev2[prefs6] + next2[prefs6];  // p6 = v24,v25
        uaddl           v24.8h,  v25.8b,  v28.8b
        uaddl2          v25.8h,  v25.16b, v28.16b

//             j1 += coef_hf[2] * (m2 + p6);        // (-p6:v24,v25)
        add             v24.8h,  v24.8h,  v22.8h
        add             v25.8h,  v25.8h,  v23.8h
        UMLAL4K         v6, v7, v8, v9, v24, v25, v0.h[4]

//             m1 = cur[mrefs];                     // m1 = v24
        ldr             q24, [x3, w9, SXTW]

//             p5 = cur[prefs5];                    // p5 = v25
        ldr             q25, [x3, w15, SXTW]


//             p2 = prev2[prefs2] + next2[prefs2];  // p2 = v26, v27
//             temporal_diff2 = FFABS(prev2[prefs2] - next2[prefs2]); // td2 = v14
//             d2  = p2 >> 1;                       // d2 = v15
        uabd            v14.16b, v31.16b, v27.16b
        uhadd           v15.16b, v31.16b, v27.16b
        uaddl           v26.8h,  v27.8b,  v31.8b
        uaddl2          v27.8h,  v27.16b, v31.16b

//             j1 += coef_hf[0] * p2;               // -
        UMLAL4K         v6, v7, v8, v9, v26, v27, v0.h[2]

//             i1 -= coef_hf[1] * (m2 + p2);        // (-m2:v22,v23*) (-p2:v26*,v27*)
        add             v22.8h,  v22.8h,  v26.8h
        add             v23.8h,  v23.8h,  v27.8h
        UMLSL4K         v2, v3, v4, v5, v22, v23, v0.h[3]

//             p1 = cur[prefs];                     // p1 = v22
        ldr             q22, [x3, w11, SXTW]

//             j1 -= coef_lf[1] * 4 * (m1 + p5);    // -
        uaddl           v26.8h,  v24.8b,  v25.8b
        uaddl2          v27.8h,  v24.16b, v25.16b
        UMLSL4K         v6, v7, v8, v9, v26, v27, v0.h[1]

//             j2 = (coef_sp[0] * (p1 + p3) - coef_sp[1]  * (m1 + p5)) >> 13; // (-p5:v25*) j2=v16
        uaddl           v18.8h,  v22.8b,  v21.8b
        uaddl2          v19.8h,  v22.16b, v21.16b
        UMULL4K         v28, v29, v30, v31, v18, v19, v0.h[6]

        uaddl           v18.8h,  v24.8b,  v25.8b
        uaddl2          v19.8h,  v24.16b, v25.16b
        UMLSL4K         v28, v29, v30, v31, v18, v19, v0.h[7]

        SQSHRUNN        v16, v28, v29, v30, v31, 13

//             i2 = (coef_sp[0] * (m1 + p1) - coef_sp[1] * (m3 + p3)) >> 13; // (-m3:v20*) i2=v17
        uaddl           v18.8h,  v22.8b,  v24.8b
        uaddl2          v19.8h,  v22.16b, v24.16b
        UMULL4K         v28, v29, v30, v31, v18, v19, v0.h[6]

        uaddl           v18.8h,  v20.8b,  v21.8b
        uaddl2          v19.8h,  v20.16b, v21.16b
        UMLSL4K         v28, v29, v30, v31, v18, v19, v0.h[7]

        SQSHRUNN        v17, v28, v29, v30, v31, 13

//             i1 += coef_lf[0] * 4 * (m1 + p1);    // p1 = v22, m1 = v24
        uaddl           v26.8h,  v24.8b,  v22.8b
        uaddl2          v27.8h,  v24.16b, v22.16b
        UMLAL4K         v2, v3, v4, v5, v26, v27, v0.h[0]

        ldr             q31, [x2, w9, SXTW]
        ldr             q29, [x4, w9, SXTW]

//             j1 += coef_lf[0] * 4 * (p1 + p3);    // p1 = v22, p3 = v21
        uaddl           v26.8h,  v21.8b,  v22.8b
        uaddl2          v27.8h,  v21.16b, v22.16b
        UMLAL4K         v6, v7, v8, v9, v26, v27, v0.h[0]

        ldr             q30, [x2, w11, SXTW]
        ldr             q28, [x4, w11, SXTW]

//             i1 >>= 15;                            // i1 = v2, -v3, -v4*, -v5*
        SQSHRUNN        v2, v2, v3, v4, v5, 15

//             j1 >>= 15;                            // j1 = v3, -v6*, -v7*, -v8*, -v9*
        SQSHRUNN        v3, v6, v7, v8, v9, 15

//             {
//                 int t1 =(FFABS(prev[mrefs] - m1) + FFABS(prev[prefs] - p1)) >> 1;
//                 int t2 =(FFABS(next[mrefs] - m1) + FFABS(next[prefs] - p1)) >> 1;
        uabd            v30.16b, v22.16b, v30.16b
        uabd            v31.16b, v24.16b, v31.16b
        uabd            v28.16b, v22.16b, v28.16b
        uabd            v29.16b, v24.16b, v29.16b
        uhadd           v31.16b, v31.16b, v30.16b
        uhadd           v29.16b, v29.16b, v28.16b

        ldr             q27, [x2, w13, SXTW]
        ldr             q26, [x4, w13, SXTW]

//                 diff0 = FFMAX3(temporal_diff0 >> 1, t1, t2); // diff0=v18
        ushr            v18.16b, v11.16b, #1
        umax            v18.16b, v18.16b, v31.16b
        umax            v18.16b, v18.16b, v29.16b

//             }                                   // v28, v30 preserved for next block
//             {  // tdiff2 = v14
//                 int t1 =(FFABS(prev[prefs] - p1) + FFABS(prev[prefs3] - p3)) >> 1;
//                 int t2 =(FFABS(next[prefs] - p1) + FFABS(next[prefs3] - p3)) >> 1;
        uabd            v31.16b, v21.16b, v27.16b
        uabd            v29.16b, v21.16b, v26.16b
        uhadd           v31.16b, v31.16b, v30.16b
        uhadd           v29.16b, v29.16b, v28.16b

//                 diff2 = FFMAX3(temporal_diff2 >> 1, t1, t2); // diff2=v19
        ushr            v19.16b, v14.16b, #1
        umax            v19.16b, v19.16b, v31.16b
        umax            v19.16b, v19.16b, v29.16b

//             }
//             {        // (m2 >> 1) = v13, m1 = v24, d0 = v10, d2 = v15, p1 = v22, diff0 = v18
//                 int b = (m2 >> 1) - m1;
//                 int f = d2 - p1;
//                 int dc = d0 - m1;
//                 int de = d0 - p1;
//                 int sp_max = FFMIN(p1 - d0, m1 - d0);
        uqsub           v31.16b, v22.16b, v10.16b
        uqsub           v29.16b, v24.16b, v10.16b
        umin            v29.16b, V31.16b, v29.16b

//                 sp_max = FFMIN(sp_max, FFMAX(-b,-f));
        uqsub           v30.16b, v24.16b, v13.16b
        uqsub           v28.16b, v22.16b, v15.16b
        umax            v28.16b, v28.16b, v30.16b
        umin            v27.16b, v29.16b, v28.16b

//                 int sp_min = FFMIN(d0 - p1, d0 - m1);
        uqsub           v31.16b, v10.16b, v22.16b
        uqsub           v29.16b, v10.16b, v24.16b
        umin            v29.16b, V31.16b, v29.16b

//                 sp_min = FFMIN(sp_min, FFMAX(b,f));
        uqsub           v30.16b, v13.16b, v24.16b
        uqsub           v28.16b, v15.16b, v22.16b
        umax            v28.16b, v28.16b, v30.16b
        umin            v26.16b, v29.16b, v28.16b

//                 diff0 = FFMAX3(diff0, sp_min, sp_max);  // diff0 = v18
        umax            v18.16b, v18.16b, v27.16b
        umax            v18.16b, v18.16b, v26.16b
//             }
//             {        // (p4 >> 1) = v12, p3 = v21, d0 = v10, d2 = v15, p1 = v22, diff2 = v19
//                 int b = d0 - p1;                  // 1
//                 int f = (p4 >> 1) - p3;           // [v23]
//                 int dc = d2 - p1;
//                 int de = d2 - p3;
//                 int sp_max = FFMIN(-de, -dc);
        uqsub           v31.16b, v21.16b, v15.16b
        uqsub           v29.16b, v22.16b, v15.16b
        umin            v29.16b, V31.16b, v29.16b

//                 sp_max = FFMIN(sp_max, FFMAX(-b,-f));
        uqsub           v30.16b, v22.16b, v10.16b
        uqsub           v28.16b, v21.16b, v12.16b
        umax            v28.16b, v28.16b, v30.16b
        umin            v27.16b, v29.16b, v28.16b

//                 int sp_min = FFMIN(de, dc);
        uqsub           v31.16b, v15.16b, v21.16b
        uqsub           v29.16b, v15.16b, v22.16b
        umin            v29.16b, V31.16b, v29.16b

//                 sp_min = FFMIN(sp_min, FFMAX(b,f));
        uqsub           v30.16b, v10.16b, v22.16b
        uqsub           v28.16b, v12.16b, v21.16b
        umax            v28.16b, v28.16b, v30.16b
        umin            v26.16b, v29.16b, v28.16b

//                 diff2 = FFMAX3(diff2, sp_min, sp_max);
        umax            v19.16b, v19.16b, v27.16b
        umax            v19.16b, v19.16b, v26.16b

//             }
//
//
//             {
//                 int interpol = FFABS(p1 - p3) > temporal_diff2 ? j1:j2;  // interpol = v6 (-j1:v6) (-j2=v16)
        uabd            v31.16b, v22.16b, v21.16b
        cmhi            v31.16b, v31.16b, v14.16b
        bif             v3.16b,  v16.16b, v31.16b

//                 if (interpol > d2 + diff2)
//                     interpol = d2 + diff2;
        uqadd           v30.16b, v15.16b, v19.16b
        umin            v3.16b,  v3.16b,  v30.16b

//                 else if (interpol < d2 - diff2)
//                     interpol = d2 - diff2;
        uqsub           v29.16b, v15.16b, v19.16b
        umax            v3.16b,  v3.16b,  v29.16b

//                 dst[d_stride * 2] = av_clip_uint8(interpol);
        str             q3,  [x0, w5, SXTW]

//             }

//             dst[d_stride] = p1;
        str             q22, [x0, w1, SXTW]


//             {
//                 int interpol = FFABS(m1 - p1) > temporal_diff0 ? i1:i2;
        uabd            v31.16b, v24.16b, v22.16b       // m1 = v24, p1 = v22
        cmhi            v31.16b, v31.16b, v11.16b       // td0 = v11
        bif             v2.16b,  v17.16b, v31.16b       // i1 = v2, i2 = v17

//                 if (interpol > d0 + diff0)
//                     interpol = d0 + diff0;
        uqadd           v30.16b, v10.16b, v18.16b       // diff0 = v18
        umin            v2.16b,  v2.16b,  v30.16b

//                 else if (interpol < d0 - diff0)
//                     interpol = d0 - diff0;
        uqsub           v29.16b, v10.16b, v18.16b
        umax            v2.16b,  v2.16b,  v29.16b
//
//                 dst[0] = av_clip_uint8(interpol);
        str             q2,  [x0], #16
//             }
//
//             dst++;
//             cur++;
//             prev++;
//             prev2++;
//             next++;
//         }

        subs            w10, w10, #16
        add             x2,  x2,  #16
        add             x3,  x3,  #16
        add             x4,  x4,  #16
        add             x17, x17, #16
        bgt             10b

        ldp             d12, d13, [sp, #16]
        ldp             d10, d11, [sp, #32]
        ldp             d8,  d9,  [sp, #48]
        ldp             d14, d15, [sp], #64
        ret
