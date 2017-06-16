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
    const unsigned int x = _x * pw;
    const unsigned int w = _w * pw;
    const unsigned int mask = stride1 - 1;
    if ((x & mask) == ((x + w) & mask)) {
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
        const unsigned int w2 = w - w1;

        for (unsigned int i = 0; i != h; ++i, dst += dst_stride, p1 += stride1, p2 += stride1) {
            unsigned int j;
            const uint8_t * p = p2;
            uint8_t * d = dst;
            memcpy(d, p1, w1);
            d += w1;
            for (j = stride1; j < w2; j += stride1, d += stride1, p += sstride) {
                memcpy(d, p, stride1);
            }
            memcpy(d, p, w2);
        }
    }
}

typedef uint8_t pixel; //*********
static void sand_to_planar_c(uint8_t * dst_u, const unsigned int dst_stride_u,
                             uint8_t * dst_v, const unsigned int dst_stride_v,
                             const uint8_t * src,
                             unsigned int stride1, unsigned int stride2,
                             unsigned int _x, unsigned int y,
                             unsigned int _w, unsigned int h)
{
    const unsigned int x = _x * pw;
    const unsigned int w = _w * pw;
    const unsigned int mask = stride1 - 1;
    if ((x & mask) == ((x + w) & mask)) {
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
        const unsigned int w2 = w - w1;

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
            for (unsigned int k = 0; k < w2; k += 2 * pw) {
                *du++ = *p++;
                *dv++ = *p++;
            }
        }
    }
}

static void planar_to_sand_c(uint8_t * dst,
                             unsigned int stride1, unsigned int stride2,
                             const uint8_t * src_u, const unsigned int src_stride_u,
                             const uint8_t * src_v, const unsigned int src_stride_v,
                             unsigned int _x, unsigned int y,
                             unsigned int _w, unsigned int h)
{
    av_assert0(0);
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
    for (unsigned int i = 0; i != h; ++i, dst += stride, src += stride) {
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
        {
            dl = w - pw;
            w = pw;
        }
        else
        {
            dl = -x;
            w += x;
        }
        x = 0;
    }
    if (x + w > st->width) {
        if (x >= st->width)
        {
            dr = w - pw;
            w = pw;
            x = 0;
        }
        else
        {
            dr = (x + w) - st->width;
            w = st->width - x;
        }
    }

    // Y
    if (y < 0) {
        if (-y >= h)
        {
            dt = h - 1;
            h = 1;
        }
        else
        {
            dt = -y;
            h += y;
        }
        y = 0;
    }
    if (y + h > st->height) {
        if (y >= st->height)
        {
            db = h - 1;
            h = 1;
            y = 0;
        }
        else
        {
            db = (y + h) - st->height;
            h = st->height - y;
        }
    }

    dst += dl + dt * dst_stride;
    sand_to_planar_y(dst, dst_stride, (const uint8_t *)src->base, st->stride1, st->stride2, x, y, w, h);

    // Edge dup
    if (dl != 0)
        dup_lr(dst - dl, dst, w, h, dst_stride);
    if (dr != 0)
        dup_lr(dst + w, dst + w - pw, w, h, dst_stride);
    w += dl + dr;
    dst -= dl;

    if (dt != 0)
        dup_tb(dst - dt * dst_stride, dst, w, h, dst_stride);
    if (db != 0)
        dup_tb(dst + h * dst_stride, dst + (h - 1) * dst_stride, w, h, dst_stride);
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

        do {
            for (unsigned int i = 0; i != ipe->n; ++i) {
                const HEVCRpiInterPredQ * const q = ipe->q + i;
                shader_track_t * const st = tracka + i;

                const uint32_t link = st->qpu_mc_curr == NULL ? q->code_setup : ((uint32_t *)st->qpu_mc_curr)[-1];
                const qpu_mc_pred_cmd_t * cmd = st->qpu_mc_curr == NULL ? q->qpu_mc_base : st->qpu_mc_curr;

                for (;;) {
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
                    else if (link == s->qpu_filter) {
                        const qpu_mc_pred_y_p_t *const c = &cmd->y.p;

                        uint8_t patch_y1[PATCH_STRIDE * 72]; // (Max width + 8) * (max height + 8)
                        uint8_t patch_y2[PATCH_STRIDE * 72]; // (Max width + 8) * (max height + 8)
                        get_patch_y(st,
                                    patch_y1, PATCH_STRIDE,
                                    st->last_l0,
                                    16, c->h + 7);
                        get_patch_y(st,
                                    patch_y2, PATCH_STRIDE,
                                    st->last_l1,
                                    16, c->h + 7);
#if 0
                        s->hevcdsp.put_hevc_qpel_uni_w[idx][!!my][!!mx](dst, dststride, src, srcstride,
                                                                        block_h, s->sh.luma_log2_weight_denom,
                                                                        luma_weight, luma_offset, mx, my, block_w);
#endif

                        st->last_l0 = &c->next_src1;
                        st->last_l1 = &c->next_src2;
                        cmd = (const qpu_mc_pred_cmd_t *)(c + 1);
                    }
                    else if (link == s->qpu_filter_b) {
                    }
                    else if (link == s->qpu_filter_y_p00) {
                    }
                    else if (link == s->qpu_filter_y_b00) {
                    }
                    else if (link == q->code_sync) {
                        cmd = (const qpu_mc_pred_cmd_t *)((uint32_t *)cmd + 1);
                        break;
                    }
                    else if (link == q->code_exit) {
                        // We expect exit to occur without other sync
                        av_assert0(i == exit_n);
                        ++exit_n;
                    }
                    else {
                        av_assert0(0);
                    }
                }
            }
        } while (exit_n == 0);
    }
}

