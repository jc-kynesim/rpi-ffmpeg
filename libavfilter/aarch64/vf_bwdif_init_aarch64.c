#include "libavutil/common.h"
#include "libavutil/aarch64/cpu.h"
#include "../avfilter.h"
#include "../bwdif.h"
#include "vf_bwdif_aarch64.h"

/*
 * Filter coefficients coef_lf and coef_hf taken from BBC PH-2071 (Weston 3 Field Deinterlacer).
 * Used when there is spatial and temporal interpolation.
 * Filter coefficients coef_sp are used when there is spatial interpolation only.
 * Adjusted for matching visual sharpness impression of spatial and temporal interpolation.
 */
static const uint16_t coef_lf[2] = { 4309, 213 };
static const uint16_t coef_hf[3] = { 5570, 3801, 1016 };
static const uint16_t coef_sp[2] = { 5077, 981 };

#define NEXT_LINE()\
    dst += d_stride; \
    prev += prefs; \
    cur  += prefs; \
    next += prefs;

static void filter_line4_check(void *restrict dst1, int d_stride,
                          const void *restrict prev1, const void *restrict cur1, const void *restrict next1, int prefs,
                          int w, int parity, int clip_max)
{
    uint8_t * restrict dst  = dst1;
    const uint8_t * restrict prev = prev1;
    const uint8_t * restrict cur  = cur1;
    const uint8_t * restrict next = next1;

    const int mrefs = -prefs;
    const int mrefs2 = mrefs * 2;
    const int prefs2 = prefs * 2;
    const int mrefs3 = mrefs * 3;
    const int prefs3 = prefs * 3;
    const int mrefs4 = mrefs * 4;
    const int prefs4 = prefs * 4;

    static int n = 0;
    uint64_t buf[2048*4/sizeof(uint64_t)];
    int i, j;
    static int fail_count = 0;

    memset(dst, 0xba, d_stride * 3);
    memset(buf, 0xba, d_stride * 3);

    ff_bwdif_filter_line4_aarch64(dst, d_stride, prev, cur, next, prefs, w, parity, clip_max);

    dst  = (uint8_t*)buf;
    prev = prev1;
    cur  = cur1;
    next = next1;

    ff_bwdif_filter_line_c(dst, (void*)prev, (void*)cur, (void*)next, w,
                           prefs, mrefs, prefs2, mrefs2, prefs3, mrefs3, prefs4, mrefs4, parity, clip_max);
    NEXT_LINE();
    memcpy(dst, cur, w);
    NEXT_LINE();
    ff_bwdif_filter_line_c(dst, (void*)prev, (void*)cur, (void*)next, w,
                           prefs, mrefs, prefs2, mrefs2, prefs3, mrefs3, prefs4, mrefs4, parity, clip_max);

    for (j = 0; j != 3; ++j)
    {
        const uint8_t * ref = (uint8_t*)buf + j * d_stride;
        const uint8_t * tst = (uint8_t*)dst1 + j * d_stride;
        for (i = 0; i != w; ++i)
        {
            if (ref[i] != tst[i])
            {
                printf("n=%d, (%d,%d): Ref: %02x, Tst: %02x\n", n, i, j, ref[i], tst[i]);
                if (fail_count++ > 16)
                    exit(1);
            }
        }
    }

    ++n;
}

static void __attribute__((optimize("tree-vectorize"))) filter_line4_debug(void *restrict dst1, int d_stride,
                          const void *restrict prev1, const void *restrict cur1, const void *restrict next1, int prefs,
                          int w, int parity, int clip_max)
{
    uint8_t * restrict dst  = dst1;
    const uint8_t * restrict prev = prev1;
    const uint8_t * restrict cur  = cur1;
    const uint8_t * restrict next = next1;

    const int mrefs = -prefs;
    const int mrefs2 = mrefs * 2;
    const int prefs2 = prefs * 2;
    const int mrefs3 = mrefs * 3;
    const int prefs3 = prefs * 3;
    const int mrefs4 = mrefs * 4;
    const int prefs4 = prefs * 4;

    static int n = 0;
    static int itt = -1;

    {
        int x;
#define prev2 cur
        const uint8_t * restrict next2 = parity ? prev : next;

        for (x = 0; x < w; x++) {
            int diff0, diff2;
            int d0, d2;
            int temporal_diff0, temporal_diff2;

            int i1, i2;
            int j1, j2;
            int p6, p5, p4, p3, p2, p1, c0, m1, m2, m3, m4;

            if ((x & 15) == 0)
                ++itt;

//            printf("======= n=%d x=%d [iteration %d.%d] =======\n", n, x, itt, x & 15);
            c0 = prev2[0] + next2[0];            // c0 = v20,v26
            d0  = c0 >> 1;                       // d0 = v21
            temporal_diff0 = FFABS(prev2[0] - next2[0]); // td0 = v9
//            printf("c0=%d, d0=%d, temporal_diff0=%d\n", c0, d0, temporal_diff0);
            i1 = coef_hf[0] * c0;                // -
//            printf("i1=%d\n", i1);
            m4 = prev2[mrefs4] + next2[mrefs4];  // m4 = v3,v4
            p4 = prev2[prefs4] + next2[prefs4];  // p4 = v5,v6, (p4 >> 1) = v23
            j1 = -coef_hf[1] * (c0 + p4);        // (-c0:v20,v26*)
//            printf("m4=%d, p4=%d, j1=%d\n", m4, p4, j1);
            i1 += coef_hf[2] * (m4 + p4);        // (-m4:v3,v4) (-p4:v5,v6) i1 = v3,v4,v7,v8
//            printf("hf2 i1=%d\n", i1);
            m3 = cur[mrefs3];                    // m3 = v5
            p3 = cur[prefs3];                    // p3 = v10, [f2=v23]
            i1 -= coef_lf[1] * 4 * (m3 + p3);   // -
//            printf("lf1 i1=%d\n", i1);
            m2 = prev2[mrefs2] + next2[mrefs2];  // m2 = v11,v12, (m2 >> 1) = v22
            p6 = prev2[prefs4 + prefs2] + next2[prefs4 + prefs2];  // p6=v0,v1
            j1 += coef_hf[2] * (m2 + p6);        // (-p6:v0*,v1*), j1 = v13,v14,v15,v16
//            printf("hf2 j1=%d\n", j1);
            p2 = prev2[prefs2] + next2[prefs2];  // p2 = v17,v18
            temporal_diff2 = FFABS(prev2[prefs2] - next2[prefs2]); // td2 = v6
            j1 += coef_hf[0] * p2;               // -
            d2  = p2 >> 1;                       // d2 = v19
            i1 -= coef_hf[1] * (m2 + p2);        // (-m2:v11,v12)
//            printf("hf1 i1=%d\n", i1);
            m1 = cur[mrefs];                     // m1 = v11, [b0=v22]
            p5 = cur[prefs3 + prefs2];           // p5=v2
            j1 -= coef_lf[1] * 4 * (m1 + p5);    // -
            p1 = cur[prefs];                     // p1 = v12
            dst[d_stride] = p1;
            j2 = (coef_sp[0] * (p1 + p3) - coef_sp[1] * (m1 + p5)) >> 13; // (-p5:v2) j2=v2
            i2 = (coef_sp[0] * (m1 + p1) - coef_sp[1] * (m3 + p3)) >> 13; // (-m3:v5) i2=v5
            {
                int t1 =(FFABS(prev[mrefs] - m1) + FFABS(prev[prefs] - p1)) >> 1;
                int t2 =(FFABS(next[mrefs] - m1) + FFABS(next[prefs] - p1)) >> 1;
                diff0 = FFMAX3(temporal_diff0 >> 1, t1, t2); // diff0=v24
//                printf("tdiff0=%d, t1=%d, t2=%d\n", temporal_diff0, t1, t2);
            }
            {
                int t1 =(FFABS(prev[prefs] - p1) + FFABS(prev[prefs3] - p3)) >> 1;
                int t2 =(FFABS(next[prefs] - p1) + FFABS(next[prefs3] - p3)) >> 1;
                diff2 = FFMAX3(temporal_diff2 >> 1, t1, t2); // diff2=v25
//                printf("tdiff2=%d, t1=%d, t2=%d\n", temporal_diff2, t1, t2);
            }
            i1 += coef_lf[0] * 4 * (m1 + p1);    // -
            j1 += coef_lf[0] * 4 * (p1 + p3);    // -
//            printf("lf0 i1=%d, j1=%d, diff0=%d, diff2=%d\n", i1, j1, diff0, diff2);
            {
                int b = (m2 >> 1) - m1;           // [v22]
                int f = d2 - p1;                  // 1
                int dc = d0 - m1;
                int de = d0 - p1;
                int sp_max = FFMIN(-de, -dc);
                int sp_min = FFMIN(de, dc);
                sp_max = FFMIN(sp_max, FFMAX(-b,-f));
                sp_min = FFMIN(sp_min, FFMAX(b,f));
//                printf("spmax0=%d, spmin0=%d, b=%d, f=%d, dc=%d, de=%d\n", sp_max, sp_min, b, f, dc, de);
                diff0 = FFMAX3(diff0, sp_min, sp_max);
            }
            {
                int b = d0 - p1;                  // 1
                int f = (p4 >> 1) - p3;           // [v23]
                int dc = d2 - p1;
                int de = d2 - p3;
                int sp_max = FFMIN(-de, -dc);
                int sp_min = FFMIN(de, dc);
                sp_max = FFMIN(sp_max, FFMAX(-b,-f));
                sp_min = FFMIN(sp_min, FFMAX(b,f));
//                printf("spmax2=%d, spmin2=%d, b=%d, f=%d, dc=%d, de=%d\n", sp_max, sp_min, b, f, dc, de);
                diff2 = FFMAX3(diff2, sp_min, sp_max);
            }

            i1 >>= 15;
            j1 >>= 15;

//            printf("Final i1=%d, i2=%d, j1=%d, j2=%d\n", i1, i2, j1, j2);


            {
                int interpol = FFABS(p1 - p3) > temporal_diff2 ? j1:j2;

//                printf("diff2=%d, interpol=%d, d2=%d\n", diff2, interpol, d2);

                if (interpol > d2 + diff2)
                    interpol = d2 + diff2;
                else if (interpol < d2 - diff2)
                    interpol = d2 - diff2;
                dst[d_stride * 2] = av_clip_uint8(interpol);
            }
            {
                int interpol = FFABS(m1 - p1) > temporal_diff0 ? i1:i2;

//                printf("diff0=%d, interpol=%d, d0=%d\n", diff0, interpol, d0);

                if (interpol > d0 + diff0)
                    interpol = d0 + diff0;
                else if (interpol < d0 - diff0)
                    interpol = d0 - diff0;

                dst[0] = av_clip_uint8(interpol);
            }
//            printf("dst[0]=%d, dst[2]=%d\n", dst[0], dst[d_stride*2]);

            dst++;
            cur++;
            prev++;
            next++;
            next2++;
//            if (n >= 513 && x >= 719)
//            {
//                exit(99);
//            }
        }
#undef prev2

//        NEXT_LINE();
//        memcpy(dst, cur, w);
        ++n;
    }
}


void
ff_bwdif_init_aarch64(AVFilterContext *ctx)
{
    const int cpu_flags = av_get_cpu_flags();
    BWDIFContext *s = ctx->priv;
    YADIFContext *yadif = &s->yadif;

    if ((ctx->inputs[0]->w & 31) != 0)
    {
        av_log(ctx, AV_LOG_DEBUG, "Cannot use aarch64 optimization: w=%d, (needs multiple of 32)\n", ctx->inputs[0]->w);
        return;
    }
    if (yadif->csp->comp[0].depth != 8)
    {
        av_log(ctx, AV_LOG_DEBUG, "Cannot use aarch64 optimization: bits=%d, (only 8 supported)\n", yadif->csp->comp[0].depth);
        return;
    }

    if (!have_neon(cpu_flags))
    {
        av_log(ctx, AV_LOG_DEBUG, "Cannot use aarch64 optimization: no NEON!\n");
        return;
    }

    if (yadif->useasm == 3)
        s->filter_line4 = filter_line4_check;
    else if (yadif->useasm == 2)
        s->filter_line4 = filter_line4_debug;
    else
        s->filter_line4 = ff_bwdif_filter_line4_aarch64;
}

