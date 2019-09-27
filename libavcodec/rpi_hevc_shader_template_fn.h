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
*/

#define STRCAT(x,y) x##y

#if PW == 1
#define pixel uint8_t
#define FUNC(f) STRCAT(f, 8)
#elif PW == 2
#define pixel uint16_t
#define FUNC(f) STRCAT(f, 16)
#else
#error Unexpected PW
#endif

#define PATCH_STRIDE (16 * PW)

static void FUNC(dup_lr)(uint8_t * dst, const uint8_t * src, unsigned int w, unsigned int h, unsigned int stride)
{
    for (unsigned int i = 0; i != h; ++i, dst += stride, src += stride) {
        const pixel s = *(const pixel *)src;
        pixel * d = (pixel *)dst;
        for (unsigned int j = 0; j < w; j += PW) {
            *d++ = s;
        }
    }
}

static void FUNC(dup_tb)(uint8_t * dst, const uint8_t * src, unsigned int w, unsigned int h, unsigned int stride)
{
    for (unsigned int i = 0; i != h; ++i, dst += stride) {
        memcpy(dst, src, w);
    }
}

static void FUNC(get_patch_y)(const shader_track_t * const st,
                         uint8_t * dst, const unsigned int dst_stride,
                         const qpu_mc_src_t *src,
                         unsigned int _w, unsigned int _h)
{
    int x = src->x * PW;
    int y = src->y;
    int w = _w * PW;
    int h = _h;
    int dl = 0;
    int dr = 0;
    int dt = 0;
    int db = 0;

    if (x < 0) {
        if (-x >= w)
            x = PW - w;
        dl = -x;
        w += x;
        x = 0;
    }
    if (x + w > st->width) {
        if (x >= st->width)
            x = st->width - PW;
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
    FUNC(av_rpi_sand_to_planar_y)(dst, dst_stride, (const uint8_t *)src->base, st->stride1, st->stride2, x, y, w, h);

    // Edge dup
    if (dl != 0)
        FUNC(dup_lr)(dst - dl, dst, dl, h, dst_stride);
    if (dr != 0)
        FUNC(dup_lr)(dst + w, dst + w - PW, dr, h, dst_stride);
    w += dl + dr;
    dst -= dl;

    if (dt != 0)
        FUNC(dup_tb)(dst - dt * dst_stride, dst, w, dt, dst_stride);
    if (db != 0)
        FUNC(dup_tb)(dst + h * dst_stride, dst + (h - 1) * dst_stride, w, db, dst_stride);
}



static void FUNC(get_patch_c)(const shader_track_t * const st,
                         uint8_t * dst_u, uint8_t * dst_v, const unsigned int dst_stride,
                         const qpu_mc_src_t *src,
                         unsigned int _w, unsigned int _h)
{
    int x = src->x * PW;
    int y = src->y;
    int w = _w * PW;
    int h = _h;
    int dl = 0;
    int dr = 0;
    int dt = 0;
    int db = 0;
    const int width = st->width;
    const int height = st->height;

    if (x < 0) {
        if (-x >= w)
            x = PW - w;
        dl = -x;
        w += x;
        x = 0;
    }
    if (x + w > width) {
        if (x >= width)
            x = width - PW;
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
    FUNC(av_rpi_sand_to_planar_c)(dst_u, dst_stride, dst_v, dst_stride, (const uint8_t *)src->base, st->stride1, st->stride2, x, y, w, h);

    // Edge dup
    if (dl != 0)
    {
        FUNC(dup_lr)(dst_u - dl, dst_u, dl, h, dst_stride);
        FUNC(dup_lr)(dst_v - dl, dst_v, dl, h, dst_stride);
    }
    if (dr != 0)
    {
        FUNC(dup_lr)(dst_u + w, dst_u + w - PW, dr, h, dst_stride);
        FUNC(dup_lr)(dst_v + w, dst_v + w - PW, dr, h, dst_stride);
    }
    w += dl + dr;
    dst_u -= dl;
    dst_v -= dl;

    if (dt != 0)
    {
        FUNC(dup_tb)(dst_u - dt * dst_stride, dst_u, w, dt, dst_stride);
        FUNC(dup_tb)(dst_v - dt * dst_stride, dst_v, w, dt, dst_stride);
    }
    if (db != 0)
    {
        FUNC(dup_tb)(dst_u + h * dst_stride, dst_u + (h - 1) * dst_stride, w, db, dst_stride);
        FUNC(dup_tb)(dst_v + h * dst_stride, dst_v + (h - 1) * dst_stride, w, db, dst_stride);
    }
}

// w, y, w, h in pixels
// stride1, stride2 in bytes
void FUNC(rpi_sand_dump)(const char * const name,
                         const uint8_t * const base, const int stride1, const int stride2, int x, int y, int w, int h, const int is_c)
{
    const int mask = stride2 == 0 ? ~0 : stride1 - 1;

    printf("%s (%d,%d) %dx%d\n", name, x, y, w, h);

    if (is_c) {
        x *= 2;
        w *= 2;
    }

    for (int i = y; i != y + h; ++i) {
        for (int j = x; j != x + w; ++j) {
            const uint8_t * p = base + ((j*PW) & mask) + i * stride1 + ((j*PW) & ~mask) * stride2;
            char sep = is_c && (j & 1) == 0 ? ':' : ' ';
#if PW == 1
            if (j < 0 || i < 0)
                printf("..%c", sep);
            else
                printf("%02x%c", *(const pixel*)p, sep);
#else
            if (j < 0 || i < 0)
                printf("...%c", sep);
            else
                printf("%03x%c", *(const pixel*)p, sep);
#endif
        }
        printf("\n");
    }
}


void FUNC(ff_hevc_rpi_shader_c)(HEVCRpiContext *const s,
                  const HEVCRpiInterPredEnv *const ipe_y,
                  const HEVCRpiInterPredEnv *const ipe_c)
{
    for (int c_idx = 0; c_idx < 2; ++c_idx)
    {
        const HEVCRpiInterPredEnv *const ipe = c_idx == 0 ? ipe_y : ipe_c;
        shader_track_t tracka[QPU_N_MAX] = {{NULL}};
        unsigned int exit_n = 0;

        if (ipe == NULL || !ipe->used) {
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
                            st->width = c->pic_w * PW;
                            st->stride1 = c->stride1;
                            st->stride2 = c->stride2;
                            st->last_l0 = &c->next_src1;
                            st->last_l1 = &c->next_src2;
                            cmd = (const qpu_mc_pred_cmd_t *)(c + 1);
                        }
                        else {
                            // Chroma
                            const qpu_mc_pred_c_s_t *const c = &cmd->c.s;

                            st->height = c->pic_ch;
                            st->width = c->pic_cw * PW;
                            st->stride1 = c->stride1;
                            st->stride2 = c->stride2;
                            st->last_l0 = &c->next_src1;
                            st->last_l1 = &c->next_src2;
                            cmd = (const qpu_mc_pred_cmd_t *)(c + 1);
                        }
                    }
                    else if (link == s->qpu.y_pxx) {
                        const qpu_mc_pred_y_p_t *const c = &cmd->y.p;
                        const int w1 = FFMIN(c->w, 8);
                        const int w2 = c->w - w1;

                        uint8_t patch_y1[PATCH_STRIDE * 72]; // (Max width + 8) * (max height + 8)
                        uint8_t patch_y2[PATCH_STRIDE * 72]; // (Max width + 8) * (max height + 8)

                        FUNC(get_patch_y)(st,
                                    patch_y1, PATCH_STRIDE,
                                    st->last_l0,
                                    16, c->h + 7);
                        if (w2 > 0) {
                            FUNC(get_patch_y)(st,
                                        patch_y2, PATCH_STRIDE,
                                        st->last_l1,
                                        16, c->h + 7);
                        }

                        // wo[offset] = offset*2+1
                        s->hevcdsp.put_hevc_qpel_uni_w[wtoidx(w1)][(c->mymx21 & 0xff00) != 0][(c->mymx21 & 0xff) != 0](
                            (uint8_t *)c->dst_addr, st->stride1, patch_y1 + 3 * (PATCH_STRIDE + PW), PATCH_STRIDE,
                            c->h, QPU_MC_DENOM, wweight(c->wo1), woff_p(s, c->wo1), (c->mymx21 & 0xff), ((c->mymx21 >> 8) & 0xff), w1);
                        if (w2 > 0) {
                            s->hevcdsp.put_hevc_qpel_uni_w[wtoidx(w2)][(c->mymx21 & 0xff000000) != 0][(c->mymx21 & 0xff0000) != 0](
                                (uint8_t *)c->dst_addr + 8 * PW, st->stride1, patch_y2 + 3 * (PATCH_STRIDE + PW), PATCH_STRIDE,
                                c->h, QPU_MC_DENOM, wweight(c->wo2), woff_p(s, c->wo2), ((c->mymx21 >> 16) & 0xff), ((c->mymx21 >> 24) & 0xff), w2);
                        }
                        st->last_l0 = &c->next_src1;
                        st->last_l1 = &c->next_src2;
                        cmd = (const qpu_mc_pred_cmd_t *)(c + 1);
                    }
                    else if (link == s->qpu.y_bxx) {
                        const qpu_mc_pred_y_p_t *const c = &cmd->y.p;

                        uint8_t patch_y1[PATCH_STRIDE * 72]; // (Max width + 8) * (max height + 8)
                        uint8_t patch_y2[PATCH_STRIDE * 72]; // (Max width + 8) * (max height + 8)
                        int16_t patch_y3[MAX_PB_SIZE * MAX_PB_SIZE];

                        FUNC(get_patch_y)(st,
                                    patch_y1, PATCH_STRIDE,
                                    st->last_l0,
                                    16, c->h + 7);
                        FUNC(get_patch_y)(st,
                                    patch_y2, PATCH_STRIDE,
                                    st->last_l1,
                                    16, c->h + 7);

                        s->hevcdsp.put_hevc_qpel[wtoidx(c->w)][(c->mymx21 & 0xff00) != 0][(c->mymx21 & 0xff) != 0](
                           patch_y3, patch_y1+ 3 * (PATCH_STRIDE + PW), PATCH_STRIDE,
                           c->h, (c->mymx21 & 0xff), ((c->mymx21 >> 8) & 0xff), c->w);

                        s->hevcdsp.put_hevc_qpel_bi_w[wtoidx(c->w)][(c->mymx21 & 0xff000000) != 0][(c->mymx21 & 0xff0000) != 0](
                            (uint8_t *)c->dst_addr, st->stride1, patch_y2 + 3 * (PATCH_STRIDE + PW), PATCH_STRIDE, patch_y3,
                            c->h, QPU_MC_DENOM, wweight(c->wo1), wweight(c->wo2),
                            0, woff_b(s, c->wo2), ((c->mymx21 >> 16) & 0xff), ((c->mymx21 >> 24) & 0xff), c->w);
                        st->last_l0 = &c->next_src1;
                        st->last_l1 = &c->next_src2;
                        cmd = (const qpu_mc_pred_cmd_t *)(c + 1);
                    }
                    else if (link == s->qpu.y_p00) {
                        const qpu_mc_pred_y_p00_t *const c = &cmd->y.p00;

                        uint8_t patch_y1[PATCH_STRIDE * 72]; // (Max width + 8) * (max height + 8)

                        FUNC(get_patch_y)(st,
                                    patch_y1, PATCH_STRIDE,
                                    st->last_l0,
                                    16, c->h + 7);

                        // wo[offset] = offset*2+1
                        s->hevcdsp.put_hevc_qpel_uni_w[wtoidx(c->w)][0][0](
                            (uint8_t *)c->dst_addr, st->stride1, patch_y1, PATCH_STRIDE,
                            c->h, QPU_MC_DENOM, wweight(c->wo1), woff_p(s, c->wo1), 0, 0, c->w);

                        st->last_l0 = &c->next_src1;
                        cmd = (const qpu_mc_pred_cmd_t *)(c + 1);
                    }
                    else if (link == s->qpu.y_b00) {
                        const qpu_mc_pred_y_p_t *const c = &cmd->y.p;

                        uint8_t patch_y1[PATCH_STRIDE * 72]; // (Max width + 8) * (max height + 8)
                        uint8_t patch_y2[PATCH_STRIDE * 72]; // (Max width + 8) * (max height + 8)
                        int16_t patch_y3[MAX_PB_SIZE * MAX_PB_SIZE];

                        av_assert0(c->w <= 16 && c->h <= 64);

                        FUNC(get_patch_y)(st,
                                    patch_y1, PATCH_STRIDE,
                                    st->last_l0,
                                    16, c->h);
                        FUNC(get_patch_y)(st,
                                    patch_y2, PATCH_STRIDE,
                                    st->last_l1,
                                    16, c->h);

                        s->hevcdsp.put_hevc_qpel[wtoidx(c->w)][0][0](
                           patch_y3, patch_y1, PATCH_STRIDE,
                           c->h, 0, 0, c->w);

                        s->hevcdsp.put_hevc_qpel_bi_w[wtoidx(c->w)][0][0](
                            (uint8_t *)c->dst_addr, st->stride1, patch_y2, PATCH_STRIDE, patch_y3,
                            c->h, QPU_MC_DENOM, wweight(c->wo1), wweight(c->wo2),
                            0, woff_b(s, c->wo2), 0, 0, c->w);
                        st->last_l0 = &c->next_src1;
                        st->last_l1 = &c->next_src2;
                        cmd = (const qpu_mc_pred_cmd_t *)(c + 1);
                    }
                    else if (link == s->qpu.c_pxx) {
                        const qpu_mc_pred_c_p_t *const c = &cmd->c.p;
                        const int mx = fctom(c->coeffs_x);
                        const int my = fctom(c->coeffs_y);

                        uint8_t patch_u1[PATCH_STRIDE * 72]; // (Max width + 8) * (max height + 8)
                        uint8_t patch_v1[PATCH_STRIDE * 72]; // (Max width + 8) * (max height + 8)
                        uint8_t patch_u3[8 * 16 * PW];
                        uint8_t patch_v3[8 * 16 * PW];

                        FUNC(get_patch_c)(st, patch_u1, patch_v1, PATCH_STRIDE, st->last_l0, 8+3, c->h + 3);

                        s->hevcdsp.put_hevc_epel_uni_w[wtoidx(c->w)][my != 0][mx != 0](
                            patch_u3, 8 * PW, patch_u1 + PATCH_STRIDE + PW, PATCH_STRIDE,
                            c->h, QPU_MC_DENOM, wweight(c->wo_u), woff_p(s, c->wo_u), mx, my, c->w);
                        s->hevcdsp.put_hevc_epel_uni_w[wtoidx(c->w)][my != 0][mx != 0](
                            patch_v3, 8 * PW, patch_v1 + PATCH_STRIDE + PW, PATCH_STRIDE,
                            c->h, QPU_MC_DENOM, wweight(c->wo_v), woff_p(s, c->wo_v), mx, my, c->w);

                        FUNC(av_rpi_planar_to_sand_c)((uint8_t *)c->dst_addr_c, st->stride1, st->stride2, patch_u3, 8 * PW, patch_v3, 8 * PW, 0, 0, c->w * PW, c->h);

                        st->last_l0 = &c->next_src;
                        cmd = (const qpu_mc_pred_cmd_t *)(c + 1);
                    }
                    else if (link == s->qpu.c_pxx_l1) {
                        const qpu_mc_pred_c_p_t *const c = &cmd->c.p;
                        const int mx = fctom(c->coeffs_x);
                        const int my = fctom(c->coeffs_y);

                        uint8_t patch_u1[PATCH_STRIDE * 72]; // (Max width + 8) * (max height + 8)
                        uint8_t patch_v1[PATCH_STRIDE * 72]; // (Max width + 8) * (max height + 8)
                        uint8_t patch_u3[8 * 16 * PW];
                        uint8_t patch_v3[8 * 16 * PW];

                        FUNC(get_patch_c)(st, patch_u1, patch_v1, PATCH_STRIDE, st->last_l1, 8+3, c->h + 3);

                        s->hevcdsp.put_hevc_epel_uni_w[wtoidx(c->w)][my != 0][mx != 0](
                            patch_u3, 8 * PW, patch_u1 + PATCH_STRIDE + PW, PATCH_STRIDE,
                            c->h, QPU_MC_DENOM, wweight(c->wo_u), woff_p(s, c->wo_u), mx, my, c->w);
                        s->hevcdsp.put_hevc_epel_uni_w[wtoidx(c->w)][my != 0][mx != 0](
                            patch_v3, 8 * PW, patch_v1 + PATCH_STRIDE + PW, PATCH_STRIDE,
                            c->h, QPU_MC_DENOM, wweight(c->wo_v), woff_p(s, c->wo_v), mx, my, c->w);

                        FUNC(av_rpi_planar_to_sand_c)((uint8_t *)c->dst_addr_c, st->stride1, st->stride2, patch_u3, 8 * PW, patch_v3, 8 * PW, 0, 0, c->w * PW, c->h);

                        st->last_l1 = &c->next_src;
                        cmd = (const qpu_mc_pred_cmd_t *)(c + 1);
                    }
                    else if (link == s->qpu.c_bxx) {
                        const qpu_mc_pred_c_b_t *const c = &cmd->c.b;
                        const int mx1 = fctom(c->coeffs_x1);
                        const int my1 = fctom(c->coeffs_y1);
                        const int mx2 = fctom(c->coeffs_x2);
                        const int my2 = fctom(c->coeffs_y2);

                        uint8_t patch_u1[PATCH_STRIDE * 72];
                        uint8_t patch_v1[PATCH_STRIDE * 72];
                        uint8_t patch_u2[PATCH_STRIDE * 72];
                        uint8_t patch_v2[PATCH_STRIDE * 72];
                        uint8_t patch_u3[8 * 16 * PW];
                        uint8_t patch_v3[8 * 16 * PW];
                        uint16_t patch_u4[MAX_PB_SIZE * MAX_PB_SIZE];
                        uint16_t patch_v4[MAX_PB_SIZE * MAX_PB_SIZE];

                        FUNC(get_patch_c)(st, patch_u1, patch_v1, PATCH_STRIDE, st->last_l0, 8+3, c->h + 3);
                        FUNC(get_patch_c)(st, patch_u2, patch_v2, PATCH_STRIDE, st->last_l1, 8+3, c->h + 3);

                        s->hevcdsp.put_hevc_epel[wtoidx(c->w)][my1 != 0][mx1 != 0](
                           patch_u4, patch_u1 + PATCH_STRIDE + PW, PATCH_STRIDE,
                           c->h, mx1, my1, c->w);
                        s->hevcdsp.put_hevc_epel[wtoidx(c->w)][my1 != 0][mx1 != 0](
                           patch_v4, patch_v1 + PATCH_STRIDE + PW, PATCH_STRIDE,
                           c->h, mx1, my1, c->w);

                        s->hevcdsp.put_hevc_epel_bi_w[wtoidx(c->w)][my2 != 0][mx2 != 0](
                            patch_u3, 8 * PW, patch_u2 + PATCH_STRIDE + PW, PATCH_STRIDE, patch_u4,
                            c->h, QPU_MC_DENOM, c->weight_u1, wweight(c->wo_u2),
                            0, woff_b(s, c->wo_u2), mx2, my2, c->w);
                        s->hevcdsp.put_hevc_epel_bi_w[wtoidx(c->w)][my2 != 0][mx2 != 0](
                            patch_v3, 8 * PW, patch_v2 + PATCH_STRIDE + PW, PATCH_STRIDE, patch_v4,
                            c->h, QPU_MC_DENOM, c->weight_v1, wweight(c->wo_v2),
                            0, woff_b(s, c->wo_v2), mx2, my2, c->w);

                        FUNC(av_rpi_planar_to_sand_c)((uint8_t *)c->dst_addr_c, st->stride1, st->stride2, patch_u3, 8 * PW, patch_v3, 8 * PW, 0, 0, c->w * PW, c->h);

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

#undef FUNC
#undef pixel

