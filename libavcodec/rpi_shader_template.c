#include "hevc.h"
#include "hevcdec.h"
#include "rpi_shader_cmd.h"
#include "rpi_shader_template.h"

#include "rpi_zc.h"

#define PATCH_STRIDE 16

typedef struct shader_track_s
{
    const union qpu_mc_pred_cmd_u *qpu_mc_curr;
    const struct qpu_mc_src_s *last_l0;
    const struct qpu_mc_src_s *last_l1;
    uint32_t width;
    uint32_t height;
    uint32_t stride2;
    uint32_t stride1;
    uint32_t wdenom;
} shader_track_t;

const unsigned int pw = 1; // Pixel width

// Fetches a single patch - offscreen fixup not done here
// w <= stride1
// unclipped
static void sand_to_planar_y(uint8_t * dst, const unsigned int dst_stride,
                             const uint8_t * src,
                             unsigned int stride1, unsigned int stride2,
                             unsigned int _x, unsigned int y,
                             unsigned int _w, unsigned int h)
{
    const unsigned int x = _x;
    const unsigned int w = _w;
    const unsigned int mask = stride1 - 1;

    if ((x & ~mask) == ((x + w) & ~mask)) {
        // All in one sand stripe
        const uint8_t * p = src + (x & mask) + y * stride1 + (x & ~mask) * stride2;
        for (unsigned int i = 0; i != h; ++i, dst += dst_stride, p += stride1) {
            memcpy(dst, p, w);
        }
    }
    else
    {
        // Two+ stripe
        const unsigned int sstride = stride1 * stride2;
        const uint8_t * p1 = src + (x & mask) + y * stride1 + (x & ~mask) * stride2;
        const uint8_t * p2 = p1 + sstride - (x & mask);
        const unsigned int w1 = stride1 - (x & mask);
        const unsigned int w3 = (x + w) & mask;
        const unsigned int w2 = w - (w1 + w3);

        for (unsigned int i = 0; i != h; ++i, dst += dst_stride, p1 += stride1, p2 += stride1) {
            unsigned int j;
            const uint8_t * p = p2;
            uint8_t * d = dst;
            memcpy(d, p1, w1);
            d += w1;
            for (j = stride1; j < w2; j += stride1, d += stride1, p += sstride) {
                memcpy(d, p, stride1);
            }
            memcpy(d, p, w3);
        }
    }
}

// x & w in bytes but not of interleave (i.e. offset = x*2 for U&V)

typedef uint8_t pixel; //*********
static void sand_to_planar_c(uint8_t * dst_u, const unsigned int dst_stride_u,
                             uint8_t * dst_v, const unsigned int dst_stride_v,
                             const uint8_t * src,
                             unsigned int stride1, unsigned int stride2,
                             unsigned int _x, unsigned int y,
                             unsigned int _w, unsigned int h)
{
    const unsigned int x = _x * 2;
    const unsigned int w = _w * 2;
    const unsigned int mask = stride1 - 1;

    if ((x & ~mask) == ((x + w) & ~mask)) {
        // All in one sand stripe
        const uint8_t * p1 = src + (x & mask) + y * stride1 + (x & ~mask) * stride2;
        for (unsigned int i = 0; i != h; ++i, dst_u += dst_stride_u, dst_v += dst_stride_v, p1 += stride1) {
            pixel * du = (pixel *)dst_u;
            pixel * dv = (pixel *)dst_v;
            const pixel * p = (const pixel *)p1;
            for (unsigned int k = 0; k < w; k += 2 * pw) {
                *du++ = *p++;
                *dv++ = *p++;
            }
        }
    }
    else
    {
        // Two+ stripe
        const unsigned int sstride = stride1 * stride2;
        const unsigned int sstride_p = (sstride - stride1) / pw;

        const uint8_t * p1 = src + (x & mask) + y * stride1 + (x & ~mask) * stride2;
        const uint8_t * p2 = p1 + sstride - (x & mask);
        const unsigned int w1 = stride1 - (x & mask);
        const unsigned int w3 = (x + w) & mask;
        const unsigned int w2 = w - (w1 + w3);

        for (unsigned int i = 0; i != h; ++i, dst_u += dst_stride_u, dst_v += dst_stride_v, p1 += stride1, p2 += stride1) {
            unsigned int j;
            const pixel * p = (const pixel *)p1;
            pixel * du = (pixel *)dst_u;
            pixel * dv = (pixel *)dst_v;
            for (unsigned int k = 0; k < w1; k += 2 * pw) {
                *du++ = *p++;
                *dv++ = *p++;
            }
            for (j = stride1, p = (const pixel *)p2; j < w2; j += stride1, p += sstride_p) {
                for (unsigned int k = 0; k < stride1; k += 2 * pw) {
                    *du++ = *p++;
                    *dv++ = *p++;
                }
            }
            for (unsigned int k = 0; k < w3; k += 2 * pw) {
                *du++ = *p++;
                *dv++ = *p++;
            }
        }
    }
}

static void planar_to_sand_c(uint8_t * dst_c,
                             unsigned int stride1, unsigned int stride2,
                             const uint8_t * src_u, const unsigned int src_stride_u,
                             const uint8_t * src_v, const unsigned int src_stride_v,
                             unsigned int _x, unsigned int y,
                             unsigned int _w, unsigned int h)
{
    const unsigned int x = _x * 2;
    const unsigned int w = _w * 2;
    const unsigned int mask = stride1 - 1;
    if ((x & ~mask) == ((x + w) & ~mask)) {
        // All in one sand stripe
        uint8_t * p1 = dst_c + (x & mask) + y * stride1 + (x & ~mask) * stride2;
        for (unsigned int i = 0; i != h; ++i, src_u += src_stride_u, src_v += src_stride_v, p1 += stride1) {
            const pixel * su = (const pixel *)src_u;
            const pixel * sv = (const pixel *)src_v;
            pixel * p = (pixel *)p1;
            for (unsigned int k = 0; k < w; k += 2 * pw) {
                *p++ = *su++;
                *p++ = *sv++;
            }
        }
    }
    else
    {
        // Two+ stripe
        const unsigned int sstride = stride1 * stride2;
        const unsigned int sstride_p = (sstride - stride1) / pw;

        const uint8_t * p1 = dst_c + (x & mask) + y * stride1 + (x & ~mask) * stride2;
        const uint8_t * p2 = p1 + sstride - (x & mask);
        const unsigned int w1 = stride1 - (x & mask);
        const unsigned int w2 = w - w1;

        for (unsigned int i = 0; i != h; ++i, src_u += src_stride_u, src_v += src_stride_v, p1 += stride1, p2 += stride1) {
            unsigned int j;
            const pixel * su = (const pixel *)src_u;
            const pixel * sv = (const pixel *)src_v;
            pixel * p = (pixel *)p1;
            for (unsigned int k = 0; k < w1; k += 2 * pw) {
                *p++ = *su++;
                *p++ = *sv++;
            }
            for (j = stride1, p = (pixel *)p2; j < w2; j += stride1, p += sstride_p) {
                for (unsigned int k = 0; k < stride1; k += 2 * pw) {
                    *p++ = *su++;
                    *p++ = *sv++;
                }
            }
            for (unsigned int k = 0; k < w2; k += 2 * pw) {
                *p++ = *su++;
                *p++ = *sv++;
            }
        }
    }
}

static void dup_lr(uint8_t * dst, const uint8_t * src, unsigned int w, unsigned int h, unsigned int stride)
{
    for (unsigned int i = 0; i != h; ++i, dst += stride, src += stride) {
        const pixel s = *(const pixel *)src;
        pixel * d = (pixel *)dst;
        for (unsigned int j = 0; j < w; j += pw) {
            *d++ = s;
        }
    }
}

static void dup_tb(uint8_t * dst, const uint8_t * src, unsigned int w, unsigned int h, unsigned int stride)
{
    for (unsigned int i = 0; i != h; ++i, dst += stride) {
        memcpy(dst, src, w);
    }
}

static void get_patch_y(const shader_track_t * const st,
                         uint8_t * dst, const unsigned int dst_stride,
                         const qpu_mc_src_t *src,
                         unsigned int _w, unsigned int _h)
{
    int x = src->x * pw;
    int y = src->y;
    int w = _w * pw;
    int h = _h;
    int dl = 0;
    int dr = 0;
    int dt = 0;
    int db = 0;

    if (x < 0) {
        if (-x >= w)
            x = pw - w;
        dl = -x;
        w += x;
        x = 0;
    }
    if (x + w > st->width) {  // ******* width*pw?? or maybe st->width already like that
        if (x >= st->width)
            x = st->width - pw;
        dr = (x + w) - st->width;
        w = st->width - x;
    }

    // Y
    if (y < 0) {
        if (-y >= h)
            y = 1 - h;
        dt = -y;
        h += y;
        y = 0;
    }
    if (y + h > st->height) {
        if (y >= st->height)
            y = st->height - 1;
        db = (y + h) - st->height;
        h = st->height - y;
    }

    dst += dl + dt * dst_stride;
    sand_to_planar_y(dst, dst_stride, (const uint8_t *)src->base, st->stride1, st->stride2, x, y, w, h);

    // Edge dup
    if (dl != 0)
        dup_lr(dst - dl, dst, dl, h, dst_stride);
    if (dr != 0)
        dup_lr(dst + w, dst + w - pw, dr, h, dst_stride);
    w += dl + dr;
    dst -= dl;

    if (dt != 0)
        dup_tb(dst - dt * dst_stride, dst, w, dt, dst_stride);
    if (db != 0)
        dup_tb(dst + h * dst_stride, dst + (h - 1) * dst_stride, w, db, dst_stride);
}



static void get_patch_c(const shader_track_t * const st,
                         uint8_t * dst_u, uint8_t * dst_v, const unsigned int dst_stride,
                         const qpu_mc_src_t *src,
                         unsigned int _w, unsigned int _h)
{
    int x = src->x * pw;
    int y = src->y;
    int w = _w * pw;
    int h = _h;
    int dl = 0;
    int dr = 0;
    int dt = 0;
    int db = 0;
    const int width = st->width;  // ?????? *pw??
    const int height = st->height;

    if (x < 0) {
        if (-x >= w)
            x = pw - w;
        dl = -x;
        w += x;
        x = 0;
    }
    if (x + w > width) {
        if (x >= width)
            x = width - pw;
        dr = (x + w) - width;
        w = width - x;
    }

    // Y
    if (y < 0) {
        if (-y >= h)
            y = 1 - h;
        dt = -y;
        h += y;
        y = 0;
    }
    if (y + h > height) {
        if (y >= height)
            y = height - 1;
        db = (y + h) - height;
        h = height - y;
    }

    dst_u += dl + dt * dst_stride;
    dst_v += dl + dt * dst_stride;
    sand_to_planar_c(dst_u, dst_stride, dst_v, dst_stride, (const uint8_t *)src->base, st->stride1, st->stride2, x, y, w, h);

    // Edge dup
    if (dl != 0)
    {
        dup_lr(dst_u - dl, dst_u, dl, h, dst_stride);
        dup_lr(dst_v - dl, dst_v, dl, h, dst_stride);
    }
    if (dr != 0)
    {
        dup_lr(dst_u + w, dst_u + w - pw, dr, h, dst_stride);
        dup_lr(dst_v + w, dst_v + w - pw, dr, h, dst_stride);
    }
    w += dl + dr;
    dst_u -= dl;
    dst_v -= dl;

    if (dt != 0)
    {
        dup_tb(dst_u - dt * dst_stride, dst_u, w, dt, dst_stride);
        dup_tb(dst_v - dt * dst_stride, dst_v, w, dt, dst_stride);
    }
    if (db != 0)
    {
        dup_tb(dst_u + h * dst_stride, dst_u + (h - 1) * dst_stride, w, db, dst_stride);
        dup_tb(dst_v + h * dst_stride, dst_v + (h - 1) * dst_stride, w, db, dst_stride);
    }
}

static int wtoidx(const unsigned int w)
{
    static const uint8_t pel_weight[65] = { [2] = 0, [4] = 1, [6] = 2, [8] = 3, [12] = 4, [16] = 5, [24] = 6, [32] = 7, [48] = 8, [64] = 9 };
    return pel_weight[w];
}

static const int fctom(uint32_t x)
{
    int rv;
    // As it happens we can take the 2nd filter term & divide it by 8
    // (dropping fractions) to get the fractional move
    rv = 8 - ((x >> 11) & 0xf);
    av_assert0(rv >= 0 && rv <= 7);
    return rv;
}

#if 0
// w, y, w, h in pixels
// stried1, stride2 in bytes
static void dump_y(const pixel * const base, const int stride1, const int stride2, int x, int y, int w, int h, const int is_c)
{
    const int mask = stride2 == 0 ? ~0 : stride1 - 1;
    if (is_c) {
        x *= 2;
        w *= 2;
    }
    for (int i = y; i != y + h; ++i) {
        for (int j = x; j != x + w; ++j) {
            const pixel * p = base + ((j*pw) & mask) + i * stride1 + ((j*pw) & ~mask) * stride2;
            char sep = is_c && (j & 1) == 0 ? ':' : ' ';
            if (j < 0 || i < 0)
                printf("..%c", sep);
            else
                printf("%02x%c", *p, sep);
        }
        printf("\n");
    }
}
#endif

static inline int32_t ext(int32_t x, unsigned int shl, unsigned int shr)
{
    return (x << shl) >> shr;
}

static inline int woff_p(int32_t x)
{
    return ext(x, 0, 17);
}

static inline int woff_b(int32_t x)
{
    return ext(x, 0, 16) - 1;
}

static inline int wweight(int32_t x)
{
    return ext(x, 16, 16);
}


void rpi_shader_c(HEVCContext *const s,
                  const HEVCRpiInterPredEnv *const ipe_y,
                  const HEVCRpiInterPredEnv *const ipe_c)
{
    for (int c_idx = 0; c_idx < 2; ++c_idx)
    {
        const HEVCRpiInterPredEnv *const ipe = c_idx == 0 ? ipe_y : ipe_c;
        shader_track_t tracka[QPU_N_MAX] = {{NULL}};
        unsigned int exit_n = 0;

        if (!ipe->used) {
            continue;
        }

        do {
            for (unsigned int i = 0; i != ipe->n; ++i) {
                const HEVCRpiInterPredQ * const q = ipe->q + i;
                shader_track_t * const st = tracka + i;
                const qpu_mc_pred_cmd_t * cmd = st->qpu_mc_curr == NULL ? q->qpu_mc_base : st->qpu_mc_curr;

                for (;;) {
                    const uint32_t link = (cmd == q->qpu_mc_base) ? q->code_setup : ((uint32_t *)cmd)[-1];

                    if (link == q->code_setup) {
                        if (c_idx == 0) {
                            // Luma
                            const qpu_mc_pred_y_s_t *const c = &cmd->y.s;

                            st->height = c->pic_h;
                            st->width = c->pic_w;
                            st->stride1 = c->stride1;
                            st->stride2 = c->stride2;
                            st->wdenom = c->wdenom - 6; // QPU uses denom + 6
                            st->last_l0 = &c->next_src1;
                            st->last_l1 = &c->next_src2;
                            cmd = (const qpu_mc_pred_cmd_t *)(c + 1);
                        }
                        else {
                            // Chroma
                            const qpu_mc_pred_c_s_t *const c = &cmd->c.s;

                            st->height = c->pic_ch;
                            st->width = c->pic_cw;
                            st->stride1 = c->stride1;
                            st->stride2 = c->stride2;
                            st->wdenom = c->wdenom - 6; // QPU uses denom + 6
                            st->last_l0 = &c->next_src1;
                            st->last_l1 = &c->next_src2;
                            cmd = (const qpu_mc_pred_cmd_t *)(c + 1);
                        }
                    }
                    else if (link == s->qpu_filter_y_pxx) {
                        const qpu_mc_pred_y_p_t *const c = &cmd->y.p;
                        const int w1 = FFMIN(c->w, 8);
                        const int w2 = c->w - w1;

                        uint8_t patch_y1[PATCH_STRIDE * 72]; // (Max width + 8) * (max height + 8)
                        uint8_t patch_y2[PATCH_STRIDE * 72]; // (Max width + 8) * (max height + 8)

                        get_patch_y(st,
                                    patch_y1, PATCH_STRIDE,
                                    st->last_l0,
                                    16, c->h + 7);
                        if (w2 > 0) {
                            get_patch_y(st,
                                        patch_y2, PATCH_STRIDE,
                                        st->last_l1,
                                        16, c->h + 7);
                        }

                        // wo[offset] = offset*2+1
                        s->hevcdsp.put_hevc_qpel_uni_w[wtoidx(w1)][(c->mymx21 & 0xff00) != 0][(c->mymx21 & 0xff) != 0](
                            c->dst_addr, st->stride1, patch_y1 + 3 * (PATCH_STRIDE + 1), PATCH_STRIDE,
                            c->h, st->wdenom, wweight(c->wo1), woff_p(c->wo1), (c->mymx21 & 0xff), ((c->mymx21 >> 8) & 0xff), w1);
                        if (w2 > 0) {
                            s->hevcdsp.put_hevc_qpel_uni_w[wtoidx(w2)][(c->mymx21 & 0xff000000) != 0][(c->mymx21 & 0xff0000) != 0](
                                c->dst_addr + 8 * pw, st->stride1, patch_y2 + 3 * (PATCH_STRIDE + 1), PATCH_STRIDE,
                                c->h, st->wdenom, wweight(c->wo2), woff_p(c->wo2), ((c->mymx21 >> 16) & 0xff), ((c->mymx21 >> 24) & 0xff), w2);
                        }
                        st->last_l0 = &c->next_src1;
                        st->last_l1 = &c->next_src2;
                        cmd = (const qpu_mc_pred_cmd_t *)(c + 1);
                    }
                    else if (link == s->qpu_filter_y_bxx) {
                        const qpu_mc_pred_y_p_t *const c = &cmd->y.p;

                        uint8_t patch_y1[PATCH_STRIDE * 72]; // (Max width + 8) * (max height + 8)
                        uint8_t patch_y2[PATCH_STRIDE * 72]; // (Max width + 8) * (max height + 8)
                        int16_t patch_y3[MAX_PB_SIZE * MAX_PB_SIZE];

                        get_patch_y(st,
                                    patch_y1, PATCH_STRIDE,
                                    st->last_l0,
                                    16, c->h + 7);
                        get_patch_y(st,
                                    patch_y2, PATCH_STRIDE,
                                    st->last_l1,
                                    16, c->h + 7);

                        s->hevcdsp.put_hevc_qpel[wtoidx(c->w)][(c->mymx21 & 0xff00) != 0][(c->mymx21 & 0xff) != 0](
                           patch_y3, patch_y1+ 3 * (PATCH_STRIDE + 1), PATCH_STRIDE,
                           c->h, (c->mymx21 & 0xff), ((c->mymx21 >> 8) & 0xff), c->w);

                        s->hevcdsp.put_hevc_qpel_bi_w[wtoidx(c->w)][(c->mymx21 & 0xff000000) != 0][(c->mymx21 & 0xff0000) != 0](
                            c->dst_addr, st->stride1, patch_y2 + 3 * (PATCH_STRIDE + 1), PATCH_STRIDE, patch_y3,
                            c->h, st->wdenom, wweight(c->wo1), wweight(c->wo2),
                            0, woff_b(c->wo2), ((c->mymx21 >> 16) & 0xff), ((c->mymx21 >> 24) & 0xff), c->w);
                        st->last_l0 = &c->next_src1;
                        st->last_l1 = &c->next_src2;
                        cmd = (const qpu_mc_pred_cmd_t *)(c + 1);
                    }
                    else if (link == s->qpu_filter_y_p00) {
                        const qpu_mc_pred_y_p00_t *const c = &cmd->y.p00;

                        uint8_t patch_y1[PATCH_STRIDE * 72]; // (Max width + 8) * (max height + 8)

                        get_patch_y(st,
                                    patch_y1, PATCH_STRIDE,
                                    st->last_l0,
                                    16, c->h + 7);

                        // wo[offset] = offset*2+1
                        s->hevcdsp.put_hevc_qpel_uni_w[wtoidx(c->w)][0][0](
                            c->dst_addr, st->stride1, patch_y1, PATCH_STRIDE,
                            c->h, st->wdenom, wweight(c->wo1), woff_p(c->wo1), 0, 0, c->w);

                        st->last_l0 = &c->next_src1;
                        cmd = (const qpu_mc_pred_cmd_t *)(c + 1);
                    }
                    else if (link == s->qpu_filter_y_b00) {
                        const qpu_mc_pred_y_p_t *const c = &cmd->y.p;

                        uint8_t patch_y1[PATCH_STRIDE * 72]; // (Max width + 8) * (max height + 8)
                        uint8_t patch_y2[PATCH_STRIDE * 72]; // (Max width + 8) * (max height + 8)
                        int16_t patch_y3[MAX_PB_SIZE * MAX_PB_SIZE];

                        av_assert0(c->w <= 16 && c->h <= 64);

                        get_patch_y(st,
                                    patch_y1, PATCH_STRIDE,
                                    st->last_l0,
                                    16, c->h);
                        get_patch_y(st,
                                    patch_y2, PATCH_STRIDE,
                                    st->last_l1,
                                    16, c->h);

                        s->hevcdsp.put_hevc_qpel[wtoidx(c->w)][0][0](
                           patch_y3, patch_y1, PATCH_STRIDE,
                           c->h, 0, 0, c->w);

                        s->hevcdsp.put_hevc_qpel_bi_w[wtoidx(c->w)][0][0](
                            c->dst_addr, st->stride1, patch_y2, PATCH_STRIDE, patch_y3,
                            c->h, st->wdenom, wweight(c->wo1), wweight(c->wo2),
                            0, woff_b(c->wo2), 0, 0, c->w);
                        st->last_l0 = &c->next_src1;
                        st->last_l1 = &c->next_src2;
                        cmd = (const qpu_mc_pred_cmd_t *)(c + 1);
                    }
                    else if (link == s->qpu_filter_uv_pxx) {
                        const qpu_mc_pred_c_p_t *const c = &cmd->c.p;
                        const int mx = fctom(c->coeffs_x);
                        const int my = fctom(c->coeffs_y);

                        uint8_t patch_u1[PATCH_STRIDE * 72]; // (Max width + 8) * (max height + 8)
                        uint8_t patch_v1[PATCH_STRIDE * 72]; // (Max width + 8) * (max height + 8)
                        pixel patch_u3[16 * 16];
                        pixel patch_v3[16 * 16];

                        get_patch_c(st, patch_u1, patch_v1, PATCH_STRIDE, st->last_l0, 8+3, c->h + 3);

                        s->hevcdsp.put_hevc_epel_uni_w[wtoidx(c->w)][my != 0][mx != 0](
                            patch_u3, 16 * pw, patch_u1 + PATCH_STRIDE + 1, PATCH_STRIDE,
                            c->h, st->wdenom, wweight(c->wo_u), woff_p(c->wo_u), mx, my, c->w);
                        s->hevcdsp.put_hevc_epel_uni_w[wtoidx(c->w)][my != 0][mx != 0](
                            patch_v3, 16 * pw, patch_v1 + PATCH_STRIDE + 1, PATCH_STRIDE,
                            c->h, st->wdenom, wweight(c->wo_v), woff_p(c->wo_v), mx, my, c->w);

                        planar_to_sand_c(c->dst_addr_c, st->stride1, st->stride2, patch_u3, 16, patch_v3, 16, 0, 0, c->w, c->h);

                        st->last_l0 = &c->next_src;
                        cmd = (const qpu_mc_pred_cmd_t *)(c + 1);
                    }
                    else if (link == s->qpu_filter_uv_bxx) {
                        const qpu_mc_pred_c_b_t *const c = &cmd->c.b;
                        const int mx1 = fctom(c->coeffs_x1);
                        const int my1 = fctom(c->coeffs_y1);
                        const int mx2 = fctom(c->coeffs_x2);
                        const int my2 = fctom(c->coeffs_y2);

                        uint8_t patch_u1[PATCH_STRIDE * 72];
                        uint8_t patch_v1[PATCH_STRIDE * 72];
                        uint8_t patch_u2[PATCH_STRIDE * 72];
                        uint8_t patch_v2[PATCH_STRIDE * 72];
                        pixel patch_u3[16 * 16];
                        pixel patch_v3[16 * 16];
                        uint16_t patch_u4[MAX_PB_SIZE * MAX_PB_SIZE];
                        uint16_t patch_v4[MAX_PB_SIZE * MAX_PB_SIZE];

                        get_patch_c(st, patch_u1, patch_v1, PATCH_STRIDE, st->last_l0, 8+3, c->h + 3);
                        get_patch_c(st, patch_u2, patch_v2, PATCH_STRIDE, st->last_l1, 8+3, c->h + 3);

                        s->hevcdsp.put_hevc_epel[wtoidx(c->w)][my1 != 0][mx1 != 0](
                           patch_u4, patch_u1 + PATCH_STRIDE + 1, PATCH_STRIDE,
                           c->h, mx1, my1, c->w);
                        s->hevcdsp.put_hevc_epel[wtoidx(c->w)][my1 != 0][mx1 != 0](
                           patch_v4, patch_v1 + PATCH_STRIDE + 1, PATCH_STRIDE,
                           c->h, mx1, my1, c->w);

                        s->hevcdsp.put_hevc_epel_bi_w[wtoidx(c->w)][my2 != 0][mx2 != 0](
                            patch_u3, 16 * pw, patch_u2 + PATCH_STRIDE + 1, PATCH_STRIDE, patch_u4,
                            c->h, st->wdenom, c->weight_u1, wweight(c->wo_u2),
                            0, woff_b(c->wo_u2), mx2, my2, c->w);
                        s->hevcdsp.put_hevc_epel_bi_w[wtoidx(c->w)][my2 != 0][mx2 != 0](
                            patch_v3, 16 * pw, patch_v2 + PATCH_STRIDE + 1, PATCH_STRIDE, patch_v4,
                            c->h, st->wdenom, c->weight_v1, wweight(c->wo_v2),
                            0, woff_b(c->wo_v2), mx2, my2, c->w);

                        planar_to_sand_c(c->dst_addr_c, st->stride1, st->stride2, patch_u3, 16, patch_v3, 16, 0, 0, c->w, c->h);

                        st->last_l0 = &c->next_src1;
                        st->last_l1 = &c->next_src2;
                        cmd = (const qpu_mc_pred_cmd_t *)(c + 1);
                    }
                    else if (link == q->code_sync) {
                        cmd = (const qpu_mc_pred_cmd_t *)((uint32_t *)cmd + 1);
                        break;
                    }
                    else if (link == q->code_exit) {
                        // We expect exit to occur without other sync
                        av_assert0(i == exit_n);
                        ++exit_n;
                        break;
                    }
                    else {
                        av_assert0(0);
                    }
                }

                st->qpu_mc_curr = cmd;
            }
        } while (exit_n == 0);
    }
}

