/*
 * HEVC video Decoder
 *
 * Copyright (C) 2012 - 2013 Guillaume Martres
 * Copyright (C) 2012 - 2013 Mickael Raulet
 * Copyright (C) 2012 - 2013 Gildas Cocherel
 * Copyright (C) 2012 - 2013 Wassim Hamidouche
 * Copyright (C) 2018 John Cox, Ben Avison, Peter de Rivaz for Raspberry Pi (Trading)
 *
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

#include "libavutil/attributes.h"
#include "libavutil/common.h"
#include "libavutil/display.h"
#include "libavutil/internal.h"
#include "libavutil/mastering_display_metadata.h"
#include "libavutil/md5.h"
#include "libavutil/opt.h"
#include "libavutil/pixdesc.h"
#include "libavutil/stereo3d.h"

#include "decode.h"
#include "bswapdsp.h"
#include "bytestream.h"
#include "golomb.h"
#include "hevc.h"
#include "rpi_hevc_data.h"
#include "rpi_hevc_parse.h"
#include "rpi_hevcdec.h"
#include "rpi_hevc_cabac_fns.h"
#include "profiles.h"
#include "hwconfig.h"

#include "rpi_zc_frames.h"
#include "rpi_qpu.h"
#include "rpi_hevc_shader.h"
#include "rpi_hevc_shader_cmd.h"
#include "rpi_hevc_shader_template.h"
#include "rpi_zc.h"
#include "libavutil/rpi_sand_fns.h"

#include "pthread.h"
#include <stdatomic.h>

#define DEBUG_DECODE_N 0   // 0 = do all, n = frames idr onwards

#define PACK2(hi,lo) (((hi) << 16) | ((lo) & 0xffff))

#ifndef av_mod_uintp2
static av_always_inline av_const unsigned av_mod_uintp2_c(unsigned a, unsigned p)
{
    return a & ((1 << p) - 1);
}
#   define av_mod_uintp2   av_mod_uintp2_c
#endif

const uint8_t ff_hevc_rpi_pel_weight[65] = { [2] = 0, [4] = 1, [6] = 2, [8] = 3, [12] = 4, [16] = 5, [24] = 6, [32] = 7, [48] = 8, [64] = 9 };
static void rpi_begin(const HEVCRpiContext * const s, HEVCRpiJob * const jb, const unsigned int ctu_ts_first);

#define MC_DUMMY_X (-32)
#define MC_DUMMY_Y (-32)

// UV & Y both have min 4x4 pred (no 2x2 chroma)
// Allow for even spread +1 for setup, +1 for rounding
// As we have load sharing this can (in theory) be exceeded so we have to
// check after each CTU, but it is a good base size

// Worst case (all 4x4) commands per CTU
#define QPU_Y_CMD_PER_CTU_MAX (16 * 16)
#define QPU_C_CMD_PER_CTU_MAX (8 * 8)

#define QPU_MAX_CTU_PER_LINE ((HEVC_RPI_MAX_WIDTH + 63) / 64)

#define QPU_GRPS (QPU_N_MAX / QPU_N_GRP)
#define QPU_CTU_PER_GRP ((QPU_MAX_CTU_PER_LINE + QPU_GRPS - 1) / QPU_GRPS)

#define QPU_Y_CMD_SLACK_PER_Q (QPU_Y_CMD_PER_CTU_MAX / 2)
#define QPU_C_CMD_SLACK_PER_Q (QPU_C_CMD_PER_CTU_MAX / 2)

// Total cmds to allocate - allow for slack & setup
#define QPU_Y_COMMANDS (QPU_CTU_PER_GRP * QPU_GRPS * QPU_Y_CMD_PER_CTU_MAX + (1 + QPU_Y_CMD_SLACK_PER_Q) * QPU_N_MAX)
#define QPU_C_COMMANDS (QPU_CTU_PER_GRP * QPU_GRPS * QPU_C_CMD_PER_CTU_MAX + (1 + QPU_C_CMD_SLACK_PER_Q) * QPU_N_MAX)

#define QPU_Y_SYNCS (QPU_N_MAX * (16 + 2))
#define QPU_C_SYNCS (QPU_N_MAX * (8 + 2))

// The QPU code for UV blocks only works up to a block width of 8
#define RPI_CHROMA_BLOCK_WIDTH 8

#define ENCODE_COEFFS(c0, c1, c2, c3) (((c0) & 0xff) | ((c1) & 0xff) << 8 | ((c2) & 0xff) << 16 | ((c3) & 0xff) << 24)


// Actual filter goes -ve, +ve, +ve, -ve using these values
static const uint32_t rpi_filter_coefs[8] = {
        ENCODE_COEFFS(  0,  64,   0,  0),
        ENCODE_COEFFS(  2,  58,  10,  2),
        ENCODE_COEFFS(  4,  54,  16,  2),
        ENCODE_COEFFS(  6,  46,  28,  4),
        ENCODE_COEFFS(  4,  36,  36,  4),
        ENCODE_COEFFS(  4,  28,  46,  6),
        ENCODE_COEFFS(  2,  16,  54,  4),
        ENCODE_COEFFS(  2,  10,  58,  2)
};

// Function arrays by QPU

static const int * const inter_pred_setup_c_qpu[12] = {
    mc_setup_c_q0, mc_setup_c_qn, mc_setup_c_qn, mc_setup_c_qn,
    mc_setup_c_qn, mc_setup_c_qn, mc_setup_c_qn, mc_setup_c_qn,
    mc_setup_c_qn, mc_setup_c_qn, mc_setup_c_qn, mc_setup_c_qn
};

static const int * const inter_pred_setup_c10_qpu[12] = {
    mc_setup_c10_q0, mc_setup_c10_qn, mc_setup_c10_qn, mc_setup_c10_qn,
    mc_setup_c10_qn, mc_setup_c10_qn, mc_setup_c10_qn, mc_setup_c10_qn,
    mc_setup_c10_qn, mc_setup_c10_qn, mc_setup_c10_qn, mc_setup_c10_qn
};

static const int * const inter_pred_setup_y_qpu[12] = {
    mc_setup_y_q0, mc_setup_y_qn, mc_setup_y_qn, mc_setup_y_qn,
    mc_setup_y_qn, mc_setup_y_qn, mc_setup_y_qn, mc_setup_y_qn,
    mc_setup_y_qn, mc_setup_y_qn, mc_setup_y_qn, mc_setup_y_qn
};

static const int * const inter_pred_setup_y10_qpu[12] = {
    mc_setup_y10_q0, mc_setup_y10_qn, mc_setup_y10_qn, mc_setup_y10_qn,
    mc_setup_y10_qn, mc_setup_y10_qn, mc_setup_y10_qn, mc_setup_y10_qn,
    mc_setup_y10_qn, mc_setup_y10_qn, mc_setup_y10_qn, mc_setup_y10_qn
};

static const int * const inter_pred_sync_qpu[12] = {
    mc_sync_q0, mc_sync_q1, mc_sync_q2, mc_sync_q3,
    mc_sync_q4, mc_sync_q5, mc_sync_q6, mc_sync_q7,
    mc_sync_q8, mc_sync_q9, mc_sync_q10, mc_sync_q11
};

static const int * const inter_pred_sync10_qpu[12] = {
    mc_sync10_q0, mc_sync10_q1, mc_sync10_q2, mc_sync10_q3,
    mc_sync10_q4, mc_sync10_q5, mc_sync10_q6, mc_sync10_q7,
    mc_sync10_q8, mc_sync10_q9, mc_sync10_q10, mc_sync10_q11
};

static const int * const inter_pred_exit_c_qpu[12] = {
    mc_exit_c_q0, mc_exit_c_qn, mc_exit_c_qn, mc_exit_c_qn,
    mc_exit_c_qn, mc_exit_c_qn, mc_exit_c_qn, mc_exit_c_qn,
    mc_exit_c_qn, mc_exit_c_qn, mc_exit_c_qn, mc_exit_c_qn
};

static const int * const inter_pred_exit_c10_qpu[12] = {
    mc_exit_c10_q0, mc_exit_c10_qn, mc_exit_c10_qn, mc_exit_c10_qn,
    mc_exit_c10_qn, mc_exit_c10_qn, mc_exit_c10_qn, mc_exit_c10_qn,
    mc_exit_c10_qn, mc_exit_c10_qn, mc_exit_c10_qn, mc_exit_c10_qn
};

static const int * const inter_pred_exit_y_qpu[12] = {
    mc_exit_y_q0, mc_exit_y_qn, mc_exit_y_qn, mc_exit_y_qn,
    mc_exit_y_qn, mc_exit_y_qn, mc_exit_y_qn, mc_exit_y_qn,
    mc_exit_y_qn, mc_exit_y_qn, mc_exit_y_qn, mc_exit_y_qn
};

static const int * const inter_pred_exit_y10_qpu[12] = {
    mc_exit_y10_q0, mc_exit_y10_qn, mc_exit_y10_qn, mc_exit_y10_qn,
    mc_exit_y10_qn, mc_exit_y10_qn, mc_exit_y10_qn, mc_exit_y10_qn,
    mc_exit_y10_qn, mc_exit_y10_qn, mc_exit_y10_qn, mc_exit_y10_qn
};

typedef struct ipe_chan_info_s
{
    const uint8_t bit_depth;
    const uint8_t n;
    const int * const * setup_fns;
    const int * const * sync_fns;
    const int * const * exit_fns;
} ipe_chan_info_t;

typedef struct ipe_init_info_s
{
    ipe_chan_info_t luma;
    ipe_chan_info_t chroma;
} ipe_init_info_t;

static void set_bytes(uint8_t * b, const unsigned int stride, const int ln, unsigned int a)
{
    switch (ln)
    {
        default:  // normally 0
            *b = a;
            break;
        case 1:
            a |= a << 8;
            *(uint16_t *)b = a;
            b += stride;
            *(uint16_t *)b = a;
            break;
        case 2:
            a |= a << 8;
            a |= a << 16;
            *(uint32_t *)b = a;
            b += stride;
            *(uint32_t *)b = a;
            b += stride;
            *(uint32_t *)b = a;
            b += stride;
            *(uint32_t *)b = a;
            break;
        case 3:
        {
            unsigned int i;
            uint64_t d;
            a |= a << 8;
            a |= a << 16;
            d = ((uint64_t)a << 32) | a;
            for (i = 0; i != 8; ++i, b += stride)
                *(uint64_t *)b = d;
            break;
        }
        case 4:
        {
            unsigned int i;
            uint64_t d;
            a |= a << 8;
            a |= a << 16;
            d = ((uint64_t)a << 32) | a;
            for (i = 0; i != 16; ++i, b += stride)
            {
                *(uint64_t *)b = d;
                *(uint64_t *)(b + 8) = d;
            }
            break;
        }
    }
}

// We expect this to be called with ln = (log2_cb_size - 3) so range =  -1..3
// (4 not required)
static void set_stash2(uint8_t * b_u, uint8_t * b_l, const int ln, unsigned int a)
{
    switch (ln)
    {
        default:  // 0 or -1
            *b_u = a;
            *b_l = a;
            break;
        case 1:
            a |= a << 8;
            *(uint16_t *)b_u = a;
            *(uint16_t *)b_l = a;
            break;
        case 2:
            a |= a << 8;
            a |= a << 16;
            *(uint32_t *)b_u = a;
            *(uint32_t *)b_l = a;
            break;
        case 3:
            a |= a << 8;
            a |= a << 16;
            *(uint32_t *)b_u = a;
            *(uint32_t *)(b_u + 4) = a;
            *(uint32_t *)b_l = a;
            *(uint32_t *)(b_l + 4) = a;
            break;
        case 4:
            a |= a << 8;
            a |= a << 16;
            *(uint32_t *)b_u = a;
            *(uint32_t *)(b_u + 4) = a;
            *(uint32_t *)(b_u + 8) = a;
            *(uint32_t *)(b_u + 12) = a;
            *(uint32_t *)b_l = a;
            *(uint32_t *)(b_l + 4) = a;
            *(uint32_t *)(b_l + 8) = a;
            *(uint32_t *)(b_l + 12) = a;
            break;
    }
}

static void zap_cabac_stash(uint8_t * b, const int ln)
{
    switch (ln)
    {
        default:  // 0
            *b = 0;
            break;
        case 1:
            *(uint16_t *)b = 0;
            break;
        case 2:
            *(uint32_t *)b = 0;
            break;
        case 3:
            *(uint32_t *)b = 0;
            *(uint32_t *)(b + 4) = 0;
            break;
    }
}



// Set a small square block of bits in a bitmap
// Bits must be aligned on their size boundry (which will be true of all split CBs)
static void set_bits(uint8_t * f, const unsigned int x, const unsigned int stride, const unsigned int ln)
{
    unsigned int n;
    const unsigned int sh = (x & 7);

    f += (x >> 3);

    av_assert2(ln <= 3);
    av_assert2((x & ((1 << ln) - 1)) == 0);

    switch (ln)
    {
        default:  // 1
            f[0] |= 1 << sh;
            break;
        case 1:  // 3 * 2
            n = 3 << sh;
            f[0] |= n;
            f[stride] |= n;
            break;
        case 2:  // 0xf * 4
            n = 0xf << sh;
            f[0] |= n;
            f[stride] |= n;
            f[stride * 2] |= n;
            f[stride * 3] |= n;
            break;
        case 3:  // 0xff * 8
            for (n = 0; n != 8; ++n, f += stride)
                *f = 0xff;
            break;
    }
}

static const ipe_init_info_t ipe_init_infos[9] = {  // Alloc for bit depths of 8-16
   {  // 8
      .luma =   {8, QPU_MC_PRED_N_Y8, inter_pred_setup_y_qpu, inter_pred_sync_qpu, inter_pred_exit_y_qpu},
      .chroma = {8, QPU_MC_PRED_N_C8, inter_pred_setup_c_qpu, inter_pred_sync_qpu, inter_pred_exit_c_qpu}
   },
   {  // 9
      .luma =   {0},
      .chroma = {0}
   },
   {  // 10
      .luma =   {10, QPU_MC_PRED_N_Y10, inter_pred_setup_y10_qpu, inter_pred_sync10_qpu, inter_pred_exit_y10_qpu},
      .chroma = {10, QPU_MC_PRED_N_C10, inter_pred_setup_c10_qpu, inter_pred_sync10_qpu, inter_pred_exit_c10_qpu}
   }

};

static void set_ipe_from_ici(HEVCRpiInterPredEnv * const ipe, const ipe_chan_info_t * const ici)
{
    const unsigned int n = ici->n;
    const unsigned int q1_size = (ipe->gptr.numbytes / n) & ~3;  // Round down to word

    ipe->n = n;
    ipe->max_fill = q1_size - ipe->min_gap;
    for(unsigned int i = 0; i < n; i++) {
        HEVCRpiInterPredQ * const q = ipe->q + i;
        q->qpu_mc_curr = q->qpu_mc_base =
            (qpu_mc_pred_cmd_t *)(ipe->gptr.arm + i * q1_size);
        q->code_setup = qpu_fn(ici->setup_fns[i]);
        q->code_sync = qpu_fn(ici->sync_fns[i]);
        q->code_exit = qpu_fn(ici->exit_fns[i]);
    }
}

static void rpi_hevc_qpu_set_fns(HEVCRpiContext * const s, const unsigned int bit_depth)
{
    av_assert0(bit_depth >= 8 && bit_depth <= 16);

    rpi_hevc_qpu_init_fn(&s->qpu, bit_depth);
}

// Unsigned Trivial MOD
static inline unsigned int utmod(const unsigned int x, const unsigned int n)
{
    return x >= n ? x - n : x;
}

// returns pq->job_n++
static inline unsigned int pass_queue_inc_job_n(HEVCRpiPassQueue * const pq)
{
    unsigned int const x2 = pq->job_n;
    pq->job_n = utmod(x2 + 1, RPI_MAX_JOBS);
    return x2;
}

static void pass_queue_init(HEVCRpiPassQueue * const pq, HEVCRpiContext * const s, HEVCRpiWorkerFn * const worker, sem_t * const psem_out, const int n)
{
    pq->terminate = 0;
    pq->job_n = 0;
    pq->context = s;
    pq->worker = worker;
    pq->psem_out = psem_out;
    pq->pass_n = n;
    pq->started = 0;
    sem_init(&pq->sem_in, 0, 0);
}

static void pass_queue_kill(HEVCRpiPassQueue * const pq)
{
    sem_destroy(&pq->sem_in);
}

static inline void rpi_sem_wait(sem_t * const sem)
{
    while (sem_wait(sem) != 0) {
        av_assert0(errno == EINTR);
    }
}

static void pass_queue_submit_job(HEVCRpiPassQueue * const pq)
{
    sem_post(&pq->sem_in);
}

static inline void pass_queue_do_all(HEVCRpiContext * const s, HEVCRpiJob * const jb)
{
    // Do the various passes - common with the worker code
    for (unsigned int i = 0; i != RPI_PASSES; ++i) {
        s->passq[i].worker(s, jb);
    }
}


#if 0
static void dump_jbc(const HEVCRpiJobCtl *const jbc, const char * const func)
{
    int x;
    sem_getvalue((sem_t *)&jbc->sem_out, &x);
    printf("%s: jbc: in=%d, out=%d, sum=%d\n", func, jbc->offload_in, jbc->offload_out, x);
}
#endif


static HEVCRpiJob * job_alloc(HEVCRpiJobCtl * const jbc, HEVCRpiLocalContext * const lc)
{
    HEVCRpiJob * jb;
    HEVCRpiJobGlobal * const jbg = jbc->jbg;

    pthread_mutex_lock(&jbg->lock);
    // Check local 1st
    if ((jb = jbc->jb1) != NULL)
    {
        // Only 1 - very easy :-)
        jbc->jb1 = NULL;
    }
    else
    {
        // Now look for global free chain
        if ((jb = jbg->free1) != NULL)
        {
            // Found one - unlink it
            jbg->free1 = jb->next;
            jb->next = NULL;
        }
        else
        {
            // Out of places to look - wait for one to become free - add to Qs

            // Global
            // If "good" lc then add after the last "good" el in the chain
            // otherwise add to the tail
            if (jbg->wait_tail == NULL || jbg->wait_tail->last_progress_good || !lc->last_progress_good)
            {
                // Add to end as we had to wait last time or wait Q empty
                if ((lc->jw_prev = jbg->wait_tail) == NULL)
                    jbg->wait_head = lc;
                else
                    lc->jw_prev->jw_next = lc;
                lc->jw_next = NULL;
                jbg->wait_tail = lc;
            }
            else
            {
                // This is a "good" lc that we need to poke into the middle
                // of the Q
                // We know that the Q isn't empty and there is at least one
                // !last_progess_good el in it from the previous test

                HEVCRpiLocalContext * const p = jbg->wait_good; // Insert after

                if (p == NULL)
                {
                    // No current good els - add to head
                    lc->jw_next = jbg->wait_head;
                    jbg->wait_head = lc;
                }
                else
                {
                    lc->jw_next = p->jw_next;
                    p->jw_next = lc;
                }

                lc->jw_next->jw_prev = lc;
                lc->jw_prev = p;
            }

            // If "good" then we are now the last good waiting el
            if (lc->last_progress_good)
                jbg->wait_good = lc;

            // Local
            if ((lc->ljw_prev = jbc->lcw_tail) == NULL)
                jbc->lcw_head = lc;
            else
                lc->ljw_prev->ljw_next = lc;
            lc->ljw_next = NULL;
            jbc->lcw_tail = lc;
        }
    }

    pthread_mutex_unlock(&jbg->lock);

    if (jb == NULL)  // Need to wait
    {
        rpi_sem_wait(&lc->jw_sem);
        jb = lc->jw_job;  // Set by free code
    }

    return jb;
}


static void job_free(HEVCRpiJobCtl * const jbc0, HEVCRpiJob * const jb)
{
    HEVCRpiJobGlobal * const jbg = jbc0->jbg;  // This jbc only used to find jbg so we can get the lock
    HEVCRpiJobCtl * jbc = jb->jbc_local;
    HEVCRpiLocalContext * lc = NULL;

    pthread_mutex_lock(&jbg->lock);

    if (jbc != NULL)
    {
        av_assert1(jbc->jb1 == NULL);

        // Release to Local if nothing waiting there
        if ((lc = jbc->lcw_head) == NULL)
            jbc->jb1 = jb;
    }
    else
    {
        // Release to global if nothing waiting there
        if ((lc = jbg->wait_head) == NULL)
        {
            jb->next = jbg->free1;
            jbg->free1 = jb;
        }
        else
        {
            // ? seems somehow mildy ugly...
            jbc = lc->context->jbc;
        }
    }

    if (lc != NULL)
    {
        // Something was waiting

        // Unlink
        // Global
        if (lc->jw_next == NULL)
            jbg->wait_tail = lc->jw_prev;
        else
            lc->jw_next->jw_prev = lc->jw_prev;

        if (lc->jw_prev == NULL)
            jbg->wait_head = lc->jw_next;
        else
            lc->jw_prev->jw_next = lc->jw_next;

        // Local
        if (lc->ljw_next == NULL)
            jbc->lcw_tail = lc->ljw_prev;
        else
            lc->ljw_next->ljw_prev = lc->ljw_prev;

        if (lc->ljw_prev == NULL)
            jbc->lcw_head = lc->ljw_next;
        else
            lc->ljw_prev->ljw_next = lc->ljw_next;

        // Update good if required
        if (jbg->wait_good == lc)
            jbg->wait_good = lc->jw_prev;

        // Prod
        lc->jw_job = jb;
        sem_post(&lc->jw_sem);
    }

    pthread_mutex_unlock(&jbg->lock);
}

static void job_lc_kill(HEVCRpiLocalContext * const lc)
{
    sem_destroy(&lc->jw_sem);
}

static void job_lc_init(HEVCRpiLocalContext * const lc)
{
    lc->jw_next = NULL;
    lc->jw_prev = NULL;
    lc->ljw_next = NULL;
    lc->ljw_prev = NULL;
    lc->jw_job = NULL;
    sem_init(&lc->jw_sem,  0, 0);
}

// Returns:
//  0 if we have waited for MV or expect to wait for recon
//  1 if we haven't waited for MV & do not need to wait for recon
static int progress_good(const HEVCRpiContext *const s, const HEVCRpiJob * const jb)
{
    if (jb->waited) // reset by rpi_begin
        return 0;
    for (unsigned int i = 0; i != FF_ARRAY_ELEMS(jb->progress_req); ++i)
    {
        if (jb->progress_req[i] >= 0 && s->DPB[i].tf.progress != NULL &&
                ((volatile int *)(s->DPB[i].tf.progress->data))[0] < jb->progress_req[i])
            return 0;
    }
    return 1;
}

// Submit job if it is full (indicated by having ctu_ts_last set >= 0)
static inline void worker_submit_job(HEVCRpiContext *const s, HEVCRpiLocalContext * const lc)
{
    HEVCRpiJobCtl *const jbc = s->jbc;
    HEVCRpiJob * const jb = lc->jb0;

    av_assert1(jb != NULL);

    if (jb->ctu_ts_last < 0) {
        return;
    }

    lc->last_progress_good = progress_good(s, jb);
    jb->waited = !lc->last_progress_good;
    lc->jb0 = NULL;

    if (s->offload_recon)
    {
        pthread_mutex_lock(&jbc->in_lock);
        jbc->offloadq[jbc->offload_in] = jb;
        jbc->offload_in = utmod(jbc->offload_in + 1, RPI_MAX_JOBS);
        pthread_mutex_unlock(&jbc->in_lock);

        pass_queue_submit_job(s->passq + 0);  // Consumes job eventually
    }
    else
    {
        pass_queue_do_all(s, jb);  // Consumes job before return
    }
}


// Call worker_pass0_ready to wait until the s->pass0_job slot becomes
// available to receive the next job.
//
// Now safe against multiple callers - needed for tiles
// "normal" and WPP will only call here one at a time
static inline void worker_pass0_ready(const HEVCRpiContext * const s, HEVCRpiLocalContext * const lc)
{
    HEVCRpiJobCtl * const jbc = s->jbc;

    // It is legit for us to already have a job allocated - do nothing in this case
    if (lc->jb0 != NULL)
        return;

    if (s->offload_recon)
        rpi_sem_wait(&jbc->sem_out);  // This sem will stop this frame grabbing too much

    lc->jb0 = job_alloc(jbc, lc);

    rpi_begin(s, lc->jb0, lc->ts);
}

// Free up a job without submission
static void worker_free(const HEVCRpiContext * const s, HEVCRpiLocalContext * const lc)
{
    HEVCRpiJobCtl * const jbc = s->jbc;
    HEVCRpiJob * const jb = lc->jb0;

    if (jb == NULL) {
        return;
    }

    lc->jb0 = NULL;

    job_free(jbc, jb);

    // If offload then poke sem_out too
    if (s->offload_recon) {
        sem_post(&jbc->sem_out);
    }
}


// Call this to wait for all jobs to have completed at the end of a frame
// Slightly icky as there is no clean way to wait for a sem to count up
// Not reentrant - call on main thread only
static void worker_wait(const HEVCRpiContext * const s, HEVCRpiLocalContext * const lc)
{
    HEVCRpiJobCtl * const jbc = s->jbc;
    int i = 0;

    // We shouldn't reach here with an unsubmitted job
    av_assert1(lc->jb0 == NULL);

    // If no offload then there can't be anything to wait for
    if (!s->offload_recon) {
        return;
    }

    if (sem_getvalue(&jbc->sem_out, &i) == 0 && i < RPI_MAX_JOBS)
    {
        for (i = 0; i != RPI_MAX_JOBS; ++i) {
            rpi_sem_wait(&jbc->sem_out);
        }
        for (i = 0; i != RPI_MAX_JOBS; ++i) {
            sem_post(&jbc->sem_out);
        }
    }
}

static void * pass_worker(void *arg)
{
    HEVCRpiPassQueue *const pq = (HEVCRpiPassQueue *)arg;
    HEVCRpiContext *const s = pq->context;

    for (;;)
    {
        rpi_sem_wait(&pq->sem_in);

        if (pq->terminate)
            break;

        pq->worker(s, s->jbc->offloadq[pass_queue_inc_job_n(pq)]);
        // * should really set jb->passes_done here

        sem_post(pq->psem_out);
    }
    return NULL;
}

static void pass_queues_start_all(HEVCRpiContext *const s)
{
    unsigned int i;
    HEVCRpiPassQueue * const pqs = s->passq;

    for (i = 0; i != RPI_PASSES; ++i)
    {
        av_assert0(pthread_create(&pqs[i].thread, NULL, pass_worker, pqs + i) == 0);
        pqs[i].started = 1;
    }
}

static void pass_queues_term_all(HEVCRpiContext *const s)
{
    unsigned int i;
    HEVCRpiPassQueue * const pqs = s->passq;

    for (i = 0; i != RPI_PASSES; ++i)
        pqs[i].terminate = 1;
    for (i = 0; i != RPI_PASSES; ++i)
    {
        if (pqs[i].started)
            sem_post(&pqs[i].sem_in);
    }
    for (i = 0; i != RPI_PASSES; ++i)
    {
        if (pqs[i].started) {
            pthread_join(pqs[i].thread, NULL);
            pqs[i].started = 0;
        }
    }
}

static void pass_queues_kill_all(HEVCRpiContext *const s)
{
    unsigned int i;
    HEVCRpiPassQueue * const pqs = s->passq;

    for (i = 0; i != RPI_PASSES; ++i)
        pass_queue_kill(pqs + i);
}


static void worker_pic_free_one(HEVCRpiJob * const jb)
{
    // Free coeff stuff - allocation not the same for all buffers
    HEVCRpiCoeffsEnv * const cf = &jb->coeffs;

    if (cf->s[0].buf != NULL)
        av_freep(&cf->mptr);
    if (cf->s[2].buf != NULL)
        gpu_free(&cf->gptr);
    memset(cf, 0, sizeof(*cf));
}

static int worker_pic_alloc_one(HEVCRpiJob * const jb, const unsigned int coeff_count)
{
    HEVCRpiCoeffsEnv * const cf = &jb->coeffs;

    if (gpu_malloc_cached((coeff_count + 32*32) * sizeof(cf->s[2].buf[0]), &cf->gptr) != 0)
        goto fail;
    cf->s[2].buf = (int16_t *)cf->gptr.arm;
    cf->s[3].buf = cf->s[2].buf + coeff_count;

    // Must be 64 byte aligned for our zero zapping code so over-allocate &
    // round
    if ((cf->mptr = av_malloc(coeff_count * sizeof(cf->s[0].buf[0]) + 63)) == NULL)
        goto fail;
    cf->s[0].buf = (void *)(((intptr_t)cf->mptr + 63) & ~63);
    return 0;

fail:
    av_log(NULL, AV_LOG_ERROR, "%s: Allocation failed\n", __func__);
    worker_pic_free_one(jb);
    return -1;
}

static void worker_pic_reset(HEVCRpiCoeffsEnv * const cf)
{
    unsigned int i;
    for (i = 0; i != 4; ++i) {
        cf->s[i].n = 0;
#if RPI_COMPRESS_COEFFS        
        cf->s[i].packed = 1;
        cf->s[i].packed_n = 0;
#endif
    }
}

int16_t * rpi_alloc_coeff_buf(HEVCRpiJob * const jb, const int buf_no, const int n)
{
    HEVCRpiCoeffEnv *const cfe = jb->coeffs.s + buf_no;
    int16_t * const coeffs = (buf_no != 3) ? cfe->buf + cfe->n : cfe->buf - (cfe->n + n);
    cfe->n += n;
    return coeffs;
}

void ff_hevc_rpi_progress_wait_field(const HEVCRpiContext * const s, HEVCRpiJob * const jb,
                                     const HEVCRpiFrame * const ref, const int val, const int field)
{
    if (ref->tf.progress != NULL && ((int *)ref->tf.progress->data)[field] < val) {
        HEVCRpiContext *const fs = ref->tf.owner[field]->priv_data;
        HEVCRpiFrameProgressState * const pstate = fs->progress_states + field;
        sem_t * sem = NULL;

        av_assert0(pthread_mutex_lock(&pstate->lock) == 0);
        if (((volatile int *)ref->tf.progress->data)[field] < val) {
            HEVCRpiFrameProgressWait * const pwait = &jb->progress_wait;

            av_assert1(pwait->req == -1 && pwait->next == NULL);
            jb->waited = 1;  // Remember that we had to wait for later scheduling

            pwait->req = val;
            pwait->next = NULL;
            if (pstate->first == NULL)
                pstate->first = pwait;
            else
                pstate->last->next = pwait;
            pstate->last = pwait;
            sem = &pwait->sem;
        }
        pthread_mutex_unlock(&pstate->lock);

        if (sem != NULL) {
            rpi_sem_wait(sem);
        }
    }
}

void ff_hevc_rpi_progress_signal_field(HEVCRpiContext * const s, const int val, const int field)
{
    HEVCRpiFrameProgressState *const pstate = s->progress_states + field;

    ((int *)s->ref->tf.progress->data)[field] = val;

    av_assert0(pthread_mutex_lock(&pstate->lock) == 0);
    {
        HEVCRpiFrameProgressWait ** ppwait = &pstate->first;
        HEVCRpiFrameProgressWait * pwait;

        while ((pwait = *ppwait) != NULL) {
            if (pwait->req > val)
            {
                ppwait = &pwait->next;
                pstate->last = pwait;
            }
            else
            {
                *ppwait = pwait->next;
                pwait->req = -1;
                pwait->next = NULL;
                sem_post(&pwait->sem);
            }
        }
    }
    pthread_mutex_unlock(&pstate->lock);
}

static void ff_hevc_rpi_progress_init_state(HEVCRpiFrameProgressState * const pstate)
{
    pstate->first = NULL;
    pstate->last = NULL;
    pthread_mutex_init(&pstate->lock, NULL);
}

static void ff_hevc_rpi_progress_init_wait(HEVCRpiFrameProgressWait * const pwait)
{
    pwait->req = -1;
    pwait->next = NULL;
    sem_init(&pwait->sem, 0, 0);
}

static void ff_hevc_rpi_progress_kill_state(HEVCRpiFrameProgressState * const pstate)
{
    av_assert1(pstate->first == NULL);
    pthread_mutex_destroy(&pstate->lock);
}

static void ff_hevc_rpi_progress_kill_wait(HEVCRpiFrameProgressWait * const pwait)
{
    sem_destroy(&pwait->sem);
}


/**
 * NOTE: Each function hls_foo correspond to the function foo in the
 * specification (HLS stands for High Level Syntax).
 */

/**
 * Section 5.7
 */

// Realloc the entry point arrays
static int alloc_entry_points(RpiSliceHeader * const sh, const int n)
{
    if (sh->entry_point_offset == NULL || n > sh->offsets_allocated || n == 0)
    {
        // Round up alloc to multiple of 32
        int a = (n + 31) & ~31;

        // We don't care about the previous contents so probably fastest to simply discard
        av_freep(&sh->entry_point_offset);
        av_freep(&sh->offset);
        av_freep(&sh->size);

        if (a != 0)
        {
            sh->entry_point_offset = av_malloc_array(a, sizeof(unsigned));
            sh->offset = av_malloc_array(a, sizeof(int));
            sh->size = av_malloc_array(a, sizeof(int));

            if (!sh->entry_point_offset || !sh->offset || !sh->size) {
                sh->num_entry_point_offsets = 0;
                sh->offsets_allocated = 0;
                return AVERROR(ENOMEM);
            }
        }

        sh->offsets_allocated = a;
    }

    return 0;
}

/* free everything allocated  by pic_arrays_init() */
static void pic_arrays_free(HEVCRpiContext *s)
{
    av_freep(&s->sao);
    av_freep(&s->deblock);

    av_freep(&s->cabac_stash_up);
    s->cabac_stash_left = NULL;  // freed with _up

    av_freep(&s->mvf_up);
    av_freep(&s->mvf_left);

    av_freep(&s->is_pcm);
    av_freep(&s->is_intra_store);
    s->is_intra = NULL;
    av_freep(&s->rpl_tab);
    s->rpl_tab_size = 0;

    av_freep(&s->qp_y_tab);
    av_freep(&s->tab_slice_address);
    av_freep(&s->filter_slice_edges);

    av_freep(&s->bs_horizontal);
    s->bs_vertical = NULL;  // freed with H
    av_freep(&s->bsf_stash_left);
    av_freep(&s->bsf_stash_up);

    av_freep(&s->rpl_up);
    av_freep(&s->rpl_left);

    alloc_entry_points(&s->sh, 0);

    av_buffer_pool_uninit(&s->col_mvf_pool);
}

/* allocate arrays that depend on frame dimensions */
static int pic_arrays_init(HEVCRpiContext * const s, const HEVCRpiSPS * const sps)
{
    const unsigned int log2_min_cb_size = sps->log2_min_cb_size;
    const unsigned int width            = sps->width;
    const unsigned int height           = sps->height;
    const unsigned int pic_size_in_cb   = ((width  >> log2_min_cb_size) + 1) *
                           ((height >> log2_min_cb_size) + 1);
    const unsigned int ctb_count        = sps->ctb_size;

    {
        unsigned int w = ((width + HEVC_RPI_BS_STRIDE1_PEL_MASK) & ~HEVC_RPI_BS_STRIDE1_PEL_MASK);
        unsigned int h = ((height + 15) & ~15);

        s->bs_stride2 = h >> HEVC_RPI_BS_COL_BYTES_SHR; // Column size
        s->bs_size = s->bs_stride2 * (w >> HEVC_RPI_BS_STRIDE1_PEL_SHIFT); // col size * cols
    }

    s->sao           = av_mallocz(ctb_count * sizeof(*s->sao) + 8); // Our sao code overreads this array slightly
    s->deblock       = av_mallocz_array(ctb_count, sizeof(*s->deblock));
    if (!s->sao || !s->deblock)
        goto fail;

    s->cabac_stash_up  = av_malloc((((width + 63) & ~63) >> 3) + (((height + 63) & ~63) >> 3));
    s->cabac_stash_left = s->cabac_stash_up + (((width + 63) & ~63) >> 3);
    if (s->cabac_stash_up == NULL)
        goto fail;

    // Round width up to max ctb size
    s->mvf_up = av_malloc((((width + 63) & ~63) >> LOG2_MIN_PU_SIZE) * sizeof(*s->mvf_up));
    // * Only needed if we have H tiles
    s->mvf_left = av_malloc((((height + 63) & ~63) >> LOG2_MIN_PU_SIZE) * sizeof(*s->mvf_up));

    // We can overread by 1 line & one byte in deblock so alloc & zero
    // We don't need to zero the extra @ start of frame as it will never be
    // written
    s->is_pcm   = av_mallocz(sps->pcm_width * (sps->pcm_height + 1) + 1);
    s->is_intra_store = av_mallocz(sps->pcm_width * (sps->pcm_height + 1) + 1);
    if (s->is_pcm == NULL || s->is_intra_store == NULL)
        goto fail;

    s->filter_slice_edges = av_mallocz(ctb_count);
    s->tab_slice_address  = av_malloc_array(ctb_count,
                                      sizeof(*s->tab_slice_address));
    s->qp_y_tab           = av_malloc_array(pic_size_in_cb,
                                      sizeof(*s->qp_y_tab));
    if (!s->qp_y_tab || !s->filter_slice_edges || !s->tab_slice_address)
        goto fail;

    s->bs_horizontal = av_mallocz(s->bs_size * 2);
    s->bs_vertical   = s->bs_horizontal + s->bs_size;
    if (s->bs_horizontal == NULL)
        goto fail;

    s->rpl_up = av_mallocz(sps->ctb_width * sizeof(*s->rpl_up));
    s->rpl_left = av_mallocz(sps->ctb_height * sizeof(*s->rpl_left));
    if (s->rpl_left == NULL || s->rpl_up == NULL)
        goto fail;

    if ((s->bsf_stash_left = av_mallocz(((height + 63) & ~63) >> 4)) == NULL ||
        (s->bsf_stash_up   = av_mallocz(((width + 63) & ~63) >> 4)) == NULL)
        goto fail;

    s->col_mvf_stride = (width + 15) >> 4;
    s->col_mvf_pool = av_buffer_pool_init(((height + 15) >> 4) * s->col_mvf_stride * sizeof(ColMvField),
                                          av_buffer_allocz);
    if (s->col_mvf_pool == NULL)
        goto fail;

    return 0;

fail:
    pic_arrays_free(s);
    return AVERROR(ENOMEM);
}

static void default_pred_weight_table(HEVCRpiContext * const s)
{
  unsigned int i;
  const unsigned int wt = 1 << QPU_MC_DENOM;
  s->sh.luma_log2_weight_denom = 0;
  s->sh.chroma_log2_weight_denom = 0;
  for (i = 0; i < s->sh.nb_refs[L0]; i++) {
      s->sh.luma_weight_l0[i] = wt;
      s->sh.luma_offset_l0[i] = 0;
      s->sh.chroma_weight_l0[i][0] = wt;
      s->sh.chroma_weight_l0[i][1] = wt;
      s->sh.chroma_offset_l0[i][0] = 0;
      s->sh.chroma_offset_l0[i][1] = 0;
  }
  for (i = 0; i < s->sh.nb_refs[L1]; i++) {
      s->sh.luma_weight_l1[i] = wt;
      s->sh.luma_offset_l1[i] = 0;
      s->sh.chroma_weight_l1[i][0] = wt;
      s->sh.chroma_weight_l1[i][1] = wt;
      s->sh.chroma_offset_l1[i][0] = 0;
      s->sh.chroma_offset_l1[i][1] = 0;
  }
}

static int get_weights(HEVCRpiContext * const s, GetBitContext * const gb,
                       const unsigned int refs,
                       int16_t * luma_weight,   int16_t * luma_offset,
                       int16_t * chroma_weight, int16_t * chroma_offset)
{
    unsigned int luma_flags;
    unsigned int chroma_flags;
    unsigned int i;
    const unsigned int wp_offset_bd_shift = s->ps.sps->high_precision_offsets_enabled_flag ? 0 : (s->ps.sps->bit_depth - 8);
    const int wp_offset_half_range = s->ps.sps->wp_offset_half_range;
    const unsigned int luma_weight_base    = 1 << QPU_MC_DENOM;
    const unsigned int chroma_weight_base  = 1 << QPU_MC_DENOM;
    const unsigned int luma_weight_shift   = (QPU_MC_DENOM - s->sh.luma_log2_weight_denom);
    const unsigned int chroma_weight_shift = (QPU_MC_DENOM - s->sh.chroma_log2_weight_denom);

    if (refs == 0)
        return 0;

    luma_flags = get_bits(gb, refs);
    chroma_flags = ctx_cfmt(s) == 0 ? 0 : get_bits(gb, refs);
    i = 1 << (refs - 1);

    do
    {
        if ((luma_flags & i) != 0)
        {
            const int delta_weight = get_se_golomb(gb);
            const int offset = get_se_golomb(gb);
            if (delta_weight < -128 || delta_weight > 127 ||
                offset < -wp_offset_half_range || offset >= wp_offset_half_range)
            {
                return AVERROR_INVALIDDATA;
            }
            *luma_weight++ = luma_weight_base + (delta_weight << luma_weight_shift);
            *luma_offset++ = offset << wp_offset_bd_shift;
        }
        else
        {
            *luma_weight++ = luma_weight_base;
            *luma_offset++ = 0;
        }

        if ((chroma_flags & i) != 0)
        {
            unsigned int j;
            for (j = 0; j != 2; ++j)
            {
                const int delta_weight = get_se_golomb(gb);
                const int delta_offset = get_se_golomb(gb);

                if (delta_weight < -128 || delta_weight > 127 ||
                    delta_offset < -4 * wp_offset_half_range || delta_offset >= 4 * wp_offset_half_range)
                {
                    return AVERROR_INVALIDDATA;
                }

                *chroma_weight++ = chroma_weight_base + (delta_weight << chroma_weight_shift);
                *chroma_offset++ = av_clip(
                    wp_offset_half_range + delta_offset -
                        ((wp_offset_half_range * ((1 << s->sh.chroma_log2_weight_denom) + delta_weight)) >> s->sh.chroma_log2_weight_denom),
                    -wp_offset_half_range, wp_offset_half_range - 1) << wp_offset_bd_shift;
            }
        }
        else
        {
            *chroma_weight++ = chroma_weight_base;
            *chroma_weight++ = chroma_weight_base;
            *chroma_offset++ = 0;
            *chroma_offset++ = 0;
        }
    } while ((i >>= 1) != 0);

    return 0;
}

static int pred_weight_table(HEVCRpiContext *s, GetBitContext *gb)
{
    int err;
    const unsigned int luma_log2_weight_denom = get_ue_golomb_long(gb);
    const unsigned int chroma_log2_weight_denom = (ctx_cfmt(s) == 0) ? 0 : luma_log2_weight_denom + get_se_golomb(gb);

    if (luma_log2_weight_denom > 7 ||
        chroma_log2_weight_denom > 7)
    {
        av_log(s->avctx, AV_LOG_ERROR, "Invalid prediction weight denom: luma=%d, chroma=%d\n",
               luma_log2_weight_denom, chroma_log2_weight_denom);
        return AVERROR_INVALIDDATA;
    }

    s->sh.luma_log2_weight_denom = luma_log2_weight_denom;
    s->sh.chroma_log2_weight_denom = chroma_log2_weight_denom;

    if ((err = get_weights(s, gb, s->sh.nb_refs[L0],
                s->sh.luma_weight_l0,      s->sh.luma_offset_l0,
                s->sh.chroma_weight_l0[0], s->sh.chroma_offset_l0[0])) != 0 ||
        (err = get_weights(s, gb, s->sh.nb_refs[L1],
                s->sh.luma_weight_l1,      s->sh.luma_offset_l1,
                s->sh.chroma_weight_l1[0], s->sh.chroma_offset_l1[0])) != 0)
    {
        av_log(s->avctx, AV_LOG_ERROR, "Invalid prediction weight or offset\n");
        return err;
    }

    return 0;
}

static int decode_lt_rps(HEVCRpiContext *s, LongTermRPS *rps, GetBitContext *gb)
{
    const HEVCRpiSPS *sps = s->ps.sps;
    int max_poc_lsb    = 1 << sps->log2_max_poc_lsb;
    int prev_delta_msb = 0;
    unsigned int nb_sps = 0, nb_sh;
    int i;

    rps->nb_refs = 0;
    if (!sps->long_term_ref_pics_present_flag)
        return 0;

    if (sps->num_long_term_ref_pics_sps > 0)
        nb_sps = get_ue_golomb_long(gb);
    nb_sh = get_ue_golomb_long(gb);

    if (nb_sps > sps->num_long_term_ref_pics_sps)
        return AVERROR_INVALIDDATA;
    if (nb_sh + (uint64_t)nb_sps > FF_ARRAY_ELEMS(rps->poc))
        return AVERROR_INVALIDDATA;

    rps->nb_refs = nb_sh + nb_sps;

    for (i = 0; i < rps->nb_refs; i++) {
        uint8_t delta_poc_msb_present;

        if (i < nb_sps) {
            uint8_t lt_idx_sps = 0;

            if (sps->num_long_term_ref_pics_sps > 1)
                lt_idx_sps = get_bits(gb, av_ceil_log2(sps->num_long_term_ref_pics_sps));

            rps->poc[i]  = sps->lt_ref_pic_poc_lsb_sps[lt_idx_sps];
            rps->used[i] = sps->used_by_curr_pic_lt_sps_flag[lt_idx_sps];
        } else {
            rps->poc[i]  = get_bits(gb, sps->log2_max_poc_lsb);
            rps->used[i] = get_bits1(gb);
        }

        delta_poc_msb_present = get_bits1(gb);
        if (delta_poc_msb_present) {
            int64_t delta = get_ue_golomb_long(gb);
            int64_t poc;

            if (i && i != nb_sps)
                delta += prev_delta_msb;

            poc = rps->poc[i] + s->poc - delta * max_poc_lsb - s->sh.pic_order_cnt_lsb;
            if (poc != (int32_t)poc)
                return AVERROR_INVALIDDATA;
            rps->poc[i] = poc;
            prev_delta_msb = delta;
        }
    }

    return 0;
}

static void export_stream_params(AVCodecContext *avctx, const HEVCRpiParamSets *ps,
                                 const HEVCRpiSPS *sps)
{
    const HEVCRpiVPS *vps = (const HEVCRpiVPS*)ps->vps_list[sps->vps_id]->data;
    const HEVCRpiWindow *ow = &sps->output_window;
    unsigned int num = 0, den = 0;

    avctx->pix_fmt             = sps->pix_fmt;
    avctx->coded_width         = sps->width;
    avctx->coded_height        = sps->height;
    avctx->width               = sps->width  - ow->left_offset - ow->right_offset;
    avctx->height              = sps->height - ow->top_offset  - ow->bottom_offset;
    avctx->has_b_frames        = sps->temporal_layer[sps->max_sub_layers - 1].num_reorder_pics;
    avctx->profile             = sps->ptl.general_ptl.profile_idc;
    avctx->level               = sps->ptl.general_ptl.level_idc;

    ff_set_sar(avctx, sps->vui.sar);

    if (sps->vui.video_signal_type_present_flag)
        avctx->color_range = sps->vui.video_full_range_flag ? AVCOL_RANGE_JPEG
                                                            : AVCOL_RANGE_MPEG;
    else
        avctx->color_range = AVCOL_RANGE_MPEG;

    if (sps->vui.colour_description_present_flag) {
        avctx->color_primaries = sps->vui.colour_primaries;
        avctx->color_trc       = sps->vui.transfer_characteristic;
        avctx->colorspace      = sps->vui.matrix_coeffs;
    } else {
        avctx->color_primaries = AVCOL_PRI_UNSPECIFIED;
        avctx->color_trc       = AVCOL_TRC_UNSPECIFIED;
        avctx->colorspace      = AVCOL_SPC_UNSPECIFIED;
    }

    if (vps->vps_timing_info_present_flag) {
        num = vps->vps_num_units_in_tick;
        den = vps->vps_time_scale;
    } else if (sps->vui.vui_timing_info_present_flag) {
        num = sps->vui.vui_num_units_in_tick;
        den = sps->vui.vui_time_scale;
    }

    if (num != 0 && den != 0)
        av_reduce(&avctx->framerate.den, &avctx->framerate.num,
                  num, den, 1 << 30);
}

static enum AVPixelFormat get_format(HEVCRpiContext *s, const HEVCRpiSPS *sps)
{
    enum AVPixelFormat pix_fmts[4], *fmt = pix_fmts;

    // Admit to no h/w formats

    *fmt++ = sps->pix_fmt;
    *fmt = AV_PIX_FMT_NONE;

    return pix_fmts[0] == AV_PIX_FMT_NONE ? AV_PIX_FMT_NONE: ff_thread_get_format(s->avctx, pix_fmts);
}

static int is_sps_supported(const HEVCRpiSPS * const sps)
{
    return av_rpi_is_sand_format(sps->pix_fmt) &&
           sps->width <= HEVC_RPI_MAX_WIDTH &&
           sps->height <= HEVC_RPI_MAX_HEIGHT;
}

static int set_sps(HEVCRpiContext * const s, const HEVCRpiSPS * const sps,
                   const enum AVPixelFormat pix_fmt)
{
    int ret;

    pic_arrays_free(s);
    s->ps.sps = NULL;
    s->ps.vps = NULL;

    if (sps == NULL)
        return 0;

    if (!is_sps_supported(sps))
        return AVERROR_DECODER_NOT_FOUND;

    ret = pic_arrays_init(s, sps);
    if (ret < 0)
        goto fail;

    export_stream_params(s->avctx, &s->ps, sps);

    s->avctx->pix_fmt = pix_fmt;

    ff_hevc_rpi_pred_init(&s->hpc,     sps->bit_depth);
    ff_hevc_rpi_dsp_init (&s->hevcdsp, sps->bit_depth);

    // * We don't support cross_component_prediction_enabled_flag but as that
    //   must be 0 unless we have 4:4:4 there is no point testing for it as we
    //   only deal with sand which is never 4:4:4
    //   [support wouldn't be hard]

    rpi_hevc_qpu_set_fns(s, sps->bit_depth);

    av_freep(&s->sao_pixel_buffer_h[0]);
    av_freep(&s->sao_pixel_buffer_v[0]);

    if (sps->sao_enabled)
    {
        const unsigned int c_count = (ctx_cfmt(s) != 0) ? 3 : 1;
        unsigned int c_idx;
        size_t vsize[3] = {0};
        size_t hsize[3] = {0};

        for(c_idx = 0; c_idx < c_count; c_idx++) {
            int w = sps->width >> ctx_hshift(s, c_idx);
            int h = sps->height >> ctx_vshift(s, c_idx);
            // ctb height & width are a min of 8 so this must a multiple of 16
            // so no point rounding up!
            hsize[c_idx] = (w * 2 * sps->ctb_height) << sps->pixel_shift;
            vsize[c_idx] = (h * 2 * sps->ctb_width) << sps->pixel_shift;
        }

        // Allocate as a single lump so we can extend h[1] & v[1] into h[2] & v[2]
        // when we have plaited chroma
        s->sao_pixel_buffer_h[0] = av_malloc(hsize[0] + hsize[1] + hsize[2]);
        s->sao_pixel_buffer_v[0] = av_malloc(vsize[0] + vsize[1] + vsize[2]);
        s->sao_pixel_buffer_h[1] = s->sao_pixel_buffer_h[0] + hsize[0];
        s->sao_pixel_buffer_h[2] = s->sao_pixel_buffer_h[1] + hsize[1];
        s->sao_pixel_buffer_v[1] = s->sao_pixel_buffer_v[0] + vsize[0];
        s->sao_pixel_buffer_v[2] = s->sao_pixel_buffer_v[1] + vsize[1];
    }

    s->ps.sps = sps;
    s->ps.vps = (HEVCRpiVPS*) s->ps.vps_list[s->ps.sps->vps_id]->data;

    return 0;

fail:
    pic_arrays_free(s);
    s->ps.sps = NULL;
    return ret;
}

static inline int qp_offset_valid(const int qp_offset)
{
    return qp_offset >= -12 && qp_offset <= 12;
}

static int hls_slice_header(HEVCRpiContext * const s)
{
    GetBitContext * const gb = &s->HEVClc->gb;
    RpiSliceHeader * const sh   = &s->sh;
    int i, ret;

    // Coded parameters
    sh->first_slice_in_pic_flag = get_bits1(gb);
    if ((IS_IDR(s) || IS_BLA(s)) && sh->first_slice_in_pic_flag) {
        s->seq_decode = (s->seq_decode + 1) & 0xff;
        s->max_ra     = INT_MAX;
        if (IS_IDR(s))
            ff_hevc_rpi_clear_refs(s);
    }
    sh->no_output_of_prior_pics_flag = 0;
    if (IS_IRAP(s))
        sh->no_output_of_prior_pics_flag = get_bits1(gb);

    sh->pps_id = get_ue_golomb_long(gb);
    if (sh->pps_id >= HEVC_MAX_PPS_COUNT || !s->ps.pps_list[sh->pps_id]) {
        av_log(s->avctx, AV_LOG_ERROR, "PPS id out of range: %d\n", sh->pps_id);
        return AVERROR_INVALIDDATA;
    }
    if (!sh->first_slice_in_pic_flag &&
        s->ps.pps != (HEVCRpiPPS*)s->ps.pps_list[sh->pps_id]->data) {
        av_log(s->avctx, AV_LOG_ERROR, "PPS changed between slices.\n");
        return AVERROR_INVALIDDATA;
    }
    s->ps.pps = (HEVCRpiPPS*)s->ps.pps_list[sh->pps_id]->data;
    if (s->nal_unit_type == HEVC_NAL_CRA_NUT && s->last_eos == 1)
        sh->no_output_of_prior_pics_flag = 1;

    if (s->ps.sps != (HEVCRpiSPS*)s->ps.sps_list[s->ps.pps->sps_id]->data) {
        const HEVCRpiSPS *sps = (HEVCRpiSPS*)s->ps.sps_list[s->ps.pps->sps_id]->data;
        const HEVCRpiSPS *last_sps = s->ps.sps;
        enum AVPixelFormat pix_fmt;

        if (last_sps && IS_IRAP(s) && s->nal_unit_type != HEVC_NAL_CRA_NUT) {
            if (sps->width != last_sps->width || sps->height != last_sps->height ||
                sps->temporal_layer[sps->max_sub_layers - 1].max_dec_pic_buffering !=
                last_sps->temporal_layer[last_sps->max_sub_layers - 1].max_dec_pic_buffering)
                sh->no_output_of_prior_pics_flag = 0;
        }
        ff_hevc_rpi_clear_refs(s);

        ret = set_sps(s, sps, sps->pix_fmt);
        if (ret < 0)
            return ret;

        pix_fmt = get_format(s, sps);
        if (pix_fmt < 0)
            return pix_fmt;

//        ret = set_sps(s, sps, pix_fmt);
//        if (ret < 0)
//            return ret;

        s->avctx->pix_fmt = pix_fmt;

        s->seq_decode = (s->seq_decode + 1) & 0xff;
        s->max_ra     = INT_MAX;
    }

    sh->dependent_slice_segment_flag = 0;
    if (!sh->first_slice_in_pic_flag) {
        int slice_address_length;

        if (s->ps.pps->dependent_slice_segments_enabled_flag)
            sh->dependent_slice_segment_flag = get_bits1(gb);

        slice_address_length = av_ceil_log2(s->ps.sps->ctb_size);
        sh->slice_segment_addr = get_bitsz(gb, slice_address_length);
        if (sh->slice_segment_addr >= s->ps.sps->ctb_size) {
            av_log(s->avctx, AV_LOG_ERROR,
                   "Invalid slice segment address: %u.\n",
                   sh->slice_segment_addr);
            return AVERROR_INVALIDDATA;
        }

        if (!sh->dependent_slice_segment_flag) {
            sh->slice_addr = sh->slice_segment_addr;
            s->slice_idx++;
        }
    } else {
        sh->slice_segment_addr = sh->slice_addr = 0;
        s->slice_idx           = 0;
        s->slice_initialized   = 0;
    }

    if (!sh->dependent_slice_segment_flag) {
        s->slice_initialized = 0;

        for (i = 0; i < s->ps.pps->num_extra_slice_header_bits; i++)
            skip_bits(gb, 1);  // slice_reserved_undetermined_flag[]

        sh->slice_type = get_ue_golomb_long(gb);
        if (!(sh->slice_type == HEVC_SLICE_I ||
              sh->slice_type == HEVC_SLICE_P ||
              sh->slice_type == HEVC_SLICE_B)) {
            av_log(s->avctx, AV_LOG_ERROR, "Unknown slice type: %d.\n",
                   sh->slice_type);
            return AVERROR_INVALIDDATA;
        }
        if (IS_IRAP(s) && sh->slice_type != HEVC_SLICE_I) {
            av_log(s->avctx, AV_LOG_ERROR, "Inter slices in an IRAP frame.\n");
            return AVERROR_INVALIDDATA;
        }

        // when flag is not present, picture is inferred to be output
        sh->pic_output_flag = 1;
        if (s->ps.pps->output_flag_present_flag)
            sh->pic_output_flag = get_bits1(gb);

        if (s->ps.sps->separate_colour_plane_flag)
            sh->colour_plane_id = get_bits(gb, 2);

        if (!IS_IDR(s)) {
            int poc, pos;

            sh->pic_order_cnt_lsb = get_bits(gb, s->ps.sps->log2_max_poc_lsb);
            poc = ff_hevc_rpi_compute_poc(s->ps.sps, s->pocTid0, sh->pic_order_cnt_lsb, s->nal_unit_type);
            if (!sh->first_slice_in_pic_flag && poc != s->poc) {
                av_log(s->avctx, AV_LOG_WARNING,
                       "Ignoring POC change between slices: %d -> %d\n", s->poc, poc);
                if (s->avctx->err_recognition & AV_EF_EXPLODE)
                    return AVERROR_INVALIDDATA;
                poc = s->poc;
            }
            s->poc = poc;

            sh->short_term_ref_pic_set_sps_flag = get_bits1(gb);
            pos = get_bits_left(gb);
            if (!sh->short_term_ref_pic_set_sps_flag) {
                ret = ff_hevc_rpi_decode_short_term_rps(gb, s->avctx, &sh->slice_rps, s->ps.sps, 1);
                if (ret < 0)
                    return ret;

                sh->short_term_rps = &sh->slice_rps;
            } else {
                int numbits, rps_idx;

                if (!s->ps.sps->nb_st_rps) {
                    av_log(s->avctx, AV_LOG_ERROR, "No ref lists in the SPS.\n");
                    return AVERROR_INVALIDDATA;
                }

                numbits = av_ceil_log2(s->ps.sps->nb_st_rps);
                rps_idx = numbits > 0 ? get_bits(gb, numbits) : 0;
                sh->short_term_rps = &s->ps.sps->st_rps[rps_idx];
            }
            sh->short_term_ref_pic_set_size = pos - get_bits_left(gb);

            pos = get_bits_left(gb);
            ret = decode_lt_rps(s, &sh->long_term_rps, gb);
            if (ret < 0) {
                av_log(s->avctx, AV_LOG_WARNING, "Invalid long term RPS.\n");
                if (s->avctx->err_recognition & AV_EF_EXPLODE)
                    return AVERROR_INVALIDDATA;
            }
            sh->long_term_ref_pic_set_size = pos - get_bits_left(gb);

            if (s->ps.sps->sps_temporal_mvp_enabled_flag)
                sh->slice_temporal_mvp_enabled_flag = get_bits1(gb);
            else
                sh->slice_temporal_mvp_enabled_flag = 0;
        } else {
            s->sh.short_term_rps = NULL;
            s->poc               = 0;
        }

        /* 8.3.1 */
        if (sh->first_slice_in_pic_flag && s->temporal_id == 0 &&
            s->nal_unit_type != HEVC_NAL_TRAIL_N &&
            s->nal_unit_type != HEVC_NAL_TSA_N   &&
            s->nal_unit_type != HEVC_NAL_STSA_N  &&
            s->nal_unit_type != HEVC_NAL_RADL_N  &&
            s->nal_unit_type != HEVC_NAL_RADL_R  &&
            s->nal_unit_type != HEVC_NAL_RASL_N  &&
            s->nal_unit_type != HEVC_NAL_RASL_R)
            s->pocTid0 = s->poc;

        if (s->ps.sps->sao_enabled) {
            sh->slice_sample_adaptive_offset_flag[0] = get_bits1(gb);
            if (ctx_cfmt(s) != 0) {
                sh->slice_sample_adaptive_offset_flag[1] =
                sh->slice_sample_adaptive_offset_flag[2] = get_bits1(gb);
            }
        } else {
            sh->slice_sample_adaptive_offset_flag[0] = 0;
            sh->slice_sample_adaptive_offset_flag[1] = 0;
            sh->slice_sample_adaptive_offset_flag[2] = 0;
        }

        sh->nb_refs[L0] = sh->nb_refs[L1] = 0;
        if (sh->slice_type == HEVC_SLICE_P || sh->slice_type == HEVC_SLICE_B) {
            int nb_refs;

            sh->nb_refs[L0] = s->ps.pps->num_ref_idx_l0_default_active;
            if (sh->slice_type == HEVC_SLICE_B)
                sh->nb_refs[L1] = s->ps.pps->num_ref_idx_l1_default_active;

            if (get_bits1(gb)) { // num_ref_idx_active_override_flag
                sh->nb_refs[L0] = get_ue_golomb_long(gb) + 1;
                if (sh->slice_type == HEVC_SLICE_B)
                    sh->nb_refs[L1] = get_ue_golomb_long(gb) + 1;
            }
            if (sh->nb_refs[L0] > HEVC_MAX_REFS || sh->nb_refs[L1] > HEVC_MAX_REFS) {
                av_log(s->avctx, AV_LOG_ERROR, "Too many refs: %d/%d.\n",
                       sh->nb_refs[L0], sh->nb_refs[L1]);
                return AVERROR_INVALIDDATA;
            }

            sh->rpl_modification_flag[0] = 0;
            sh->rpl_modification_flag[1] = 0;
            nb_refs = ff_hevc_rpi_frame_nb_refs(s);
            if (!nb_refs) {
                av_log(s->avctx, AV_LOG_ERROR, "Zero refs for a frame with P or B slices.\n");
                return AVERROR_INVALIDDATA;
            }

            if (s->ps.pps->lists_modification_present_flag && nb_refs > 1) {
                sh->rpl_modification_flag[0] = get_bits1(gb);
                if (sh->rpl_modification_flag[0]) {
                    for (i = 0; i < sh->nb_refs[L0]; i++)
                        sh->list_entry_lx[0][i] = get_bits(gb, av_ceil_log2(nb_refs));
                }

                if (sh->slice_type == HEVC_SLICE_B) {
                    sh->rpl_modification_flag[1] = get_bits1(gb);
                    if (sh->rpl_modification_flag[1] == 1)
                        for (i = 0; i < sh->nb_refs[L1]; i++)
                            sh->list_entry_lx[1][i] = get_bits(gb, av_ceil_log2(nb_refs));
                }
            }

            if (sh->slice_type == HEVC_SLICE_B)
                sh->mvd_l1_zero_flag = get_bits1(gb);

            if (s->ps.pps->cabac_init_present_flag)
                sh->cabac_init_flag = get_bits1(gb);
            else
                sh->cabac_init_flag = 0;

            sh->collocated_ref_idx = 0;
            if (sh->slice_temporal_mvp_enabled_flag) {
                sh->collocated_list = L0;
                if (sh->slice_type == HEVC_SLICE_B)
                    sh->collocated_list = !get_bits1(gb);

                if (sh->nb_refs[sh->collocated_list] > 1) {
                    sh->collocated_ref_idx = get_ue_golomb_long(gb);
                    if (sh->collocated_ref_idx >= sh->nb_refs[sh->collocated_list]) {
                        av_log(s->avctx, AV_LOG_ERROR,
                               "Invalid collocated_ref_idx: %d.\n",
                               sh->collocated_ref_idx);
                        return AVERROR_INVALIDDATA;
                    }
                }
            }

            if ((s->ps.pps->weighted_pred_flag   && sh->slice_type == HEVC_SLICE_P) ||
                (s->ps.pps->weighted_bipred_flag && sh->slice_type == HEVC_SLICE_B))
            {
                if ((ret = pred_weight_table(s, gb)) != 0)
                    return ret;
            }
            else
            {
                // Give us unit weights
                default_pred_weight_table(s);
            }

            sh->max_num_merge_cand = 5 - get_ue_golomb_long(gb);
            if (sh->max_num_merge_cand < 1 || sh->max_num_merge_cand > 5) {
                av_log(s->avctx, AV_LOG_ERROR,
                       "Invalid number of merging MVP candidates: %d.\n",
                       sh->max_num_merge_cand);
                return AVERROR_INVALIDDATA;
            }
        }

        sh->slice_qp_delta = get_se_golomb(gb);

        if (s->ps.pps->pic_slice_level_chroma_qp_offsets_present_flag) {
            sh->slice_cb_qp_offset = get_se_golomb(gb);
            sh->slice_cr_qp_offset = get_se_golomb(gb);
            if (!qp_offset_valid(sh->slice_cb_qp_offset) ||
                !qp_offset_valid(s->ps.pps->cb_qp_offset + sh->slice_cb_qp_offset) ||
                !qp_offset_valid(sh->slice_cr_qp_offset) ||
                !qp_offset_valid(s->ps.pps->cr_qp_offset + sh->slice_cr_qp_offset))
            {
                av_log(s->avctx, AV_LOG_ERROR, "Bad chroma offset (pps:%d/%d; slice=%d/%d\n",
                       sh->slice_cr_qp_offset, sh->slice_cr_qp_offset,
                       s->ps.pps->cb_qp_offset, s->ps.pps->cr_qp_offset);
                return AVERROR_INVALIDDATA;
            }
        } else
        {
            sh->slice_cb_qp_offset = 0;
            sh->slice_cr_qp_offset = 0;
        }

        if (s->ps.pps->chroma_qp_offset_list_enabled_flag)
            sh->cu_chroma_qp_offset_enabled_flag = get_bits1(gb);
        else
            sh->cu_chroma_qp_offset_enabled_flag = 0;

        if (s->ps.pps->deblocking_filter_control_present_flag) {
            int deblocking_filter_override_flag = 0;

            if (s->ps.pps->deblocking_filter_override_enabled_flag)
                deblocking_filter_override_flag = get_bits1(gb);

            if (deblocking_filter_override_flag) {
                sh->disable_deblocking_filter_flag = get_bits1(gb);
                if (!sh->disable_deblocking_filter_flag) {
                    int beta_offset_div2 = get_se_golomb(gb);
                    int tc_offset_div2   = get_se_golomb(gb) ;
                    if (beta_offset_div2 < -6 || beta_offset_div2 > 6 ||
                        tc_offset_div2   < -6 || tc_offset_div2   > 6) {
                        av_log(s->avctx, AV_LOG_ERROR,
                            "Invalid deblock filter offsets: %d, %d\n",
                            beta_offset_div2, tc_offset_div2);
                        return AVERROR_INVALIDDATA;
                    }
                    sh->beta_offset = beta_offset_div2 * 2;
                    sh->tc_offset   =   tc_offset_div2 * 2;
                }
            } else {
                sh->disable_deblocking_filter_flag = s->ps.pps->disable_dbf;
                sh->beta_offset                    = s->ps.pps->beta_offset;
                sh->tc_offset                      = s->ps.pps->tc_offset;
            }
        } else {
            sh->disable_deblocking_filter_flag = 0;
            sh->beta_offset                    = 0;
            sh->tc_offset                      = 0;
        }

        if (s->ps.pps->seq_loop_filter_across_slices_enabled_flag &&
            (sh->slice_sample_adaptive_offset_flag[0] ||
             sh->slice_sample_adaptive_offset_flag[1] ||
             !sh->disable_deblocking_filter_flag)) {
            sh->slice_loop_filter_across_slices_enabled_flag = get_bits1(gb);
        } else {
            sh->slice_loop_filter_across_slices_enabled_flag = s->ps.pps->seq_loop_filter_across_slices_enabled_flag;
        }
        sh->no_dblk_boundary_flags =
            (sh->slice_loop_filter_across_slices_enabled_flag ? 0 :
                BOUNDARY_UPPER_SLICE | BOUNDARY_LEFT_SLICE) |
            (s->ps.pps->loop_filter_across_tiles_enabled_flag ? 0 :
                BOUNDARY_UPPER_TILE | BOUNDARY_LEFT_TILE);


    } else if (!s->slice_initialized) {
        av_log(s->avctx, AV_LOG_ERROR, "Independent slice segment missing.\n");
        return AVERROR_INVALIDDATA;
    }

    sh->num_entry_point_offsets = 0;
    sh->offload_wpp = 0;
    sh->offload_tiles = 0;

    if (s->ps.pps->tiles_enabled_flag || s->ps.pps->entropy_coding_sync_enabled_flag) {
        unsigned num_entry_point_offsets = get_ue_golomb_long(gb);
        // It would be possible to bound this tighter but this here is simpler
        if (num_entry_point_offsets > get_bits_left(gb)) {
            av_log(s->avctx, AV_LOG_ERROR, "num_entry_point_offsets %d is invalid\n", num_entry_point_offsets);
            return AVERROR_INVALIDDATA;
        }

        sh->num_entry_point_offsets = num_entry_point_offsets;
        if (sh->num_entry_point_offsets > 0) {
            int offset_len = get_ue_golomb_long(gb) + 1;

            if (offset_len < 1 || offset_len > 32) {
                sh->num_entry_point_offsets = 0;
                av_log(s->avctx, AV_LOG_ERROR, "offset_len %d is invalid\n", offset_len);
                return AVERROR_INVALIDDATA;
            }

            if ((ret = alloc_entry_points(sh, sh->num_entry_point_offsets)) < 0)
            {
                av_log(s->avctx, AV_LOG_ERROR, "Failed to allocate memory\n");
                return ret;
            }

            for (i = 0; i < sh->num_entry_point_offsets; i++) {
                uint32_t val_minus1 = get_bits_long(gb, offset_len);
                if (val_minus1 > (1 << 28))
                {
                    // We can declare offsets of > 2^28 bad without loss of generality
                    // Will check actual bounds wrt NAL later, but this keeps
                    // the values within bounds we can deal with easily
                    av_log(s->avctx, AV_LOG_ERROR, "entry_point_offset_minus1 %d invalid\n", val_minus1);
                    return AVERROR_INVALIDDATA;
                }
                sh->entry_point_offset[i] = val_minus1 + 1; // +1 to get the size
            }

            // Do we want to offload this
            if (s->threads_type != 0)
            {
                sh->offload_tiles = (!s->ps.pps->tile_wpp_inter_disable || sh->slice_type == HEVC_SLICE_I) &&
                    s->ps.pps->num_tile_columns > 1;
                // * We only cope with WPP in a single column
                //   Probably want to deal with that case as tiles rather than WPP anyway
                // ?? Not actually sure that the main code deals with WPP + multi-col correctly
                sh->offload_wpp = s->ps.pps->entropy_coding_sync_enabled_flag &&
                    s->ps.pps->num_tile_columns == 1;
            }
        }
    }

    if (s->ps.pps->slice_header_extension_present_flag) {
        unsigned int length = get_ue_golomb_long(gb);
        if (length*8LL > get_bits_left(gb)) {
            av_log(s->avctx, AV_LOG_ERROR, "too many slice_header_extension_data_bytes\n");
            return AVERROR_INVALIDDATA;
        }
        for (i = 0; i < length; i++)
            skip_bits(gb, 8);  // slice_header_extension_data_byte
    }

    // Inferred parameters
    sh->slice_qp = 26U + s->ps.pps->pic_init_qp_minus26 + sh->slice_qp_delta;
    if (sh->slice_qp > 51 ||
        sh->slice_qp < -s->ps.sps->qp_bd_offset) {
        av_log(s->avctx, AV_LOG_ERROR,
               "The slice_qp %d is outside the valid range "
               "[%d, 51].\n",
               sh->slice_qp,
               -s->ps.sps->qp_bd_offset);
        return AVERROR_INVALIDDATA;
    }

    if (get_bits_left(gb) < 0) {
        av_log(s->avctx, AV_LOG_ERROR,
               "Overread slice header by %d bits\n", -get_bits_left(gb));
        return AVERROR_INVALIDDATA;
    }

    s->slice_initialized = 1;
    return 0;
}

static void hls_sao_param(const HEVCRpiContext *s, HEVCRpiLocalContext * const lc, const int rx, const int ry)
{
    RpiSAOParams * const sao = s->sao + rx + ry * s->ps.sps->ctb_width;
    int c_idx, i;

    if (s->sh.slice_sample_adaptive_offset_flag[0] ||
        s->sh.slice_sample_adaptive_offset_flag[1]) {
        if ((lc->ctb_avail & AVAIL_L) != 0)
        {
            const int sao_merge_left_flag = ff_hevc_rpi_sao_merge_flag_decode(lc);
            if (sao_merge_left_flag) {
                *sao = sao[-1];
                return;
            }
        }
        if ((lc->ctb_avail & AVAIL_U) != 0)
        {
            const int sao_merge_up_flag = ff_hevc_rpi_sao_merge_flag_decode(lc);
            if (sao_merge_up_flag) {
                *sao = sao[-(int)s->ps.sps->ctb_width];
                return;
            }
        }
    }

    for (c_idx = 0; c_idx < (ctx_cfmt(s) != 0 ? 3 : 1); c_idx++) {
        const unsigned int log2_sao_offset_scale = c_idx == 0 ? s->ps.pps->log2_sao_offset_scale_luma :
                                                 s->ps.pps->log2_sao_offset_scale_chroma;
        int offset_abs[4];
        char offset_sign[4] = {0};

        if (!s->sh.slice_sample_adaptive_offset_flag[c_idx]) {
            sao->type_idx[c_idx] = SAO_NOT_APPLIED;
            continue;
        }

        if (c_idx == 2) {
            sao->type_idx[2] = sao->type_idx[1];
            sao->eo_class[2] = sao->eo_class[1];
        } else {
            sao->type_idx[c_idx] = ff_hevc_rpi_sao_type_idx_decode(lc);
        }

        // ** Could use BY22 here quite plausibly - this is all bypass stuff
        //    though only per CTB so not very timing critical

        if (sao->type_idx[c_idx] == SAO_NOT_APPLIED)
            continue;

        for (i = 0; i < 4; i++)
            offset_abs[i] = ff_hevc_rpi_sao_offset_abs_decode(s, lc);

        if (sao->type_idx[c_idx] == SAO_BAND) {
            for (i = 0; i < 4; i++) {
                if (offset_abs[i] != 0)
                    offset_sign[i] = ff_hevc_rpi_sao_offset_sign_decode(lc);
            }
            sao->band_position[c_idx] = ff_hevc_rpi_sao_band_position_decode(lc);
        } else if (c_idx != 2) {
            sao->eo_class[c_idx] = ff_hevc_rpi_sao_eo_class_decode(lc);
        }

        // Inferred parameters
        sao->offset_val[c_idx][0] = 0;
        for (i = 0; i < 4; i++) {
            sao->offset_val[c_idx][i + 1] = offset_abs[i] << log2_sao_offset_scale;
            if (sao->type_idx[c_idx] == SAO_EDGE) {
                if (i > 1)
                    sao->offset_val[c_idx][i + 1] = -sao->offset_val[c_idx][i + 1];
            } else if (offset_sign[i]) {
                sao->offset_val[c_idx][i + 1] = -sao->offset_val[c_idx][i + 1];
            }
        }
    }
}

#if 0
static int hls_cross_component_pred(HEVCRpiLocalContext * const lc, const int idx) {
    int log2_res_scale_abs_plus1 = ff_hevc_rpi_log2_res_scale_abs(lc, idx);  // 0..4

    if (log2_res_scale_abs_plus1 !=  0) {
        int res_scale_sign_flag = ff_hevc_rpi_res_scale_sign_flag(lc, idx);
        lc->tu.res_scale_val = (1 << (log2_res_scale_abs_plus1 - 1)) *
                               (1 - 2 * res_scale_sign_flag);
    } else {
        lc->tu.res_scale_val = 0;
    }


    return 0;
}
#endif

static inline HEVCPredCmd * rpi_new_intra_cmd(HEVCRpiJob * const jb)
{
    return jb->intra.cmds + jb->intra.n++;
}

#define A0(x, y, U, L, UL, UR, DL) \
    [(x)+(y)*16] = (((U) ? AVAIL_U : 0) | ((L) ? AVAIL_L : 0) | ((UL) ? AVAIL_UL : 0) | ((UR) ? AVAIL_UR : 0) | ((DL) ? AVAIL_DL : 0))

#define A1(x, y, U, L, UL, UR, DL) \
    A0((x) + 0, (y) + 0, (U),  (L),  (UL), (U),  (L) ),  A0((x) + 1, (y) + 0, (U),   1,   (U),  (UR),  0  ),\
    A0((x) + 0, (y) + 1,  1,   (L),  (L),   1,   (DL)),  A0((x) + 1, (y) + 1,  1,    1,    1,    0,    0  )

#define A2(x, y, U, L, UL, UR, DL) \
    A1((x) + 0, (y) + 0, (U),  (L),  (UL), (U),  (L) ),  A1((x) + 2, (y) + 0, (U),   1,   (U),  (UR),  0  ),\
    A1((x) + 0, (y) + 2,  1,   (L),  (L),   1,   (DL)),  A1((x) + 2, (y) + 2,  1,    1,    1,    0,    0  )

#define A3(x, y, U, L, UL, UR, DL) \
    A2((x) + 0, (y) + 0, (U),  (L),  (UL), (U),  (L) ),  A2((x) + 4, (y) + 0, (U),   1,   (U),  (UR),  0  ),\
    A2((x) + 0, (y) + 4,  1,   (L),  (L),   1,   (DL)),  A2((x) + 4, (y) + 4,  1,    1,    1,    0,    0  )

#define A4(x, y, U, L, UL, UR, DL) \
    A3((x) + 0, (y) + 0, (U),  (L),  (UL), (U),  (L) ),  A3((x) + 8, (y) + 0, (U),   1,   (U),  (UR),  0  ),\
    A3((x) + 0, (y) + 8,  1,   (L),  (L),   1,   (DL)),  A3((x) + 8, (y) + 8,  1,    1,    1,    0,    0  )

static const uint8_t tb_flags[16 * 16] = {A4(0, 0, 0, 0, 0, 0, 0)};

unsigned int ff_hevc_rpi_tb_avail_flags(
    const HEVCRpiContext * const s, const HEVCRpiLocalContext * const lc,
    const unsigned int x, const unsigned int y, const unsigned int w, const unsigned int h)
{
    const unsigned int ctb_mask = ~0U << s->ps.sps->log2_ctb_size;
    const unsigned int tb_x = x & ~ctb_mask;
    const unsigned int tb_y = y & ~ctb_mask;
    const unsigned int ctb_avail = lc->ctb_avail;

    const uint8_t * const tb_f = tb_flags + (tb_x >> 2) + (tb_y >> 2) * 16;

    unsigned int f = (ctb_avail | tb_f[0]) & (AVAIL_L | AVAIL_U | AVAIL_UL);

    // This deals with both the U & L edges
    if ((tb_x | tb_y) != 0 && (~f & (AVAIL_L | AVAIL_U)) == 0)
        f |= AVAIL_UL;

    if (x + w < lc->end_of_ctb_x)
        f |= (tb_y == 0 ? ctb_avail >> (AVAIL_S_U - AVAIL_S_UR) : tb_f[(w - 1) >> 2]) & AVAIL_UR;
    else if (tb_y == 0)
        f |= (ctb_avail & AVAIL_UR);
#if AVAIL_S_U - AVAIL_S_UR < 0
#error Shift problem
#endif

    // Never any D if Y beyond eoctb
    if (y + h < lc->end_of_ctb_y)
        f |= (tb_x == 0 ? ctb_avail << (AVAIL_S_DL - AVAIL_S_L) : tb_f[((h - 1) >> 2) * 16]) & AVAIL_DL;
#if AVAIL_S_DL - AVAIL_S_L < 0
#error Shift problem
#endif

//    printf("(%#x, %#x): %dx%d ca=%02x, ful=%02x, ftr=%02x, fdl=%02x, eox=%#x, eoy=%#x\n", x, y, w, h,
//           lc->ctb_avail, tb_f[0], tb_f[(w - 1) >> 2], tb_f[((h - 1) >> 2) * 16],
//           lc->end_of_ctb_x, lc->end_of_ctb_y);

    return f;
}

#undef A0
#undef A1
#undef A2
#undef A3
#undef A4

static void do_intra_pred(const HEVCRpiContext * const s, HEVCRpiLocalContext * const lc, int log2_trafo_size, int x0, int y0, int c_idx,
                          unsigned int avail)
{
    // If rpi_enabled then sand - U & V done on U call
    if (c_idx <= 1)
    {
        HEVCPredCmd *const cmd = rpi_new_intra_cmd(lc->jb0);
        cmd->type = RPI_PRED_INTRA + c_idx;
        cmd->size = log2_trafo_size;
        cmd->avail = avail;
        cmd->i_pred.x = x0;
        cmd->i_pred.y = y0;
        cmd->i_pred.mode = c_idx ? lc->tu.intra_pred_mode_c :  lc->tu.intra_pred_mode;

//        printf("(%#x, %#x) c_idx=%d, s=%d, a=%#x\n", x0, y0, c_idx, 1 << log2_trafo_size, avail);
    }
}

#define CBF_CB0_S 0
#define CBF_CB1_S 1 // CB1 must be CB0 + 1
#define CBF_CR0_S 2
#define CBF_CR1_S 3

#define CBF_CB0 (1 << CBF_CB0_S)
#define CBF_CR0 (1 << CBF_CR0_S)
#define CBF_CB1 (1 << CBF_CB1_S)
#define CBF_CR1 (1 << CBF_CR1_S)

// * Only good for chroma_idx == 1
static int hls_transform_unit(const HEVCRpiContext * const s, HEVCRpiLocalContext * const lc,
                              const unsigned int x0, const unsigned int y0,
                              const unsigned int log2_cb_size, const unsigned int log2_trafo_size,
                              const unsigned int blk_idx, const int cbf_luma,
                              const unsigned int cbf_chroma)
{
    const unsigned int log2_trafo_size_c = FFMAX(2, log2_trafo_size - 1);
    const unsigned int x0_c = x0 & ~7;
    const unsigned int y0_c = y0 & ~7;

    enum ScanType scan_idx   = SCAN_DIAG;
    enum ScanType scan_idx_c = SCAN_DIAG;

    if (lc->cu.pred_mode == MODE_INTRA)
    {
        const unsigned int trafo_size = 1 << log2_trafo_size;
        const unsigned int avail = ff_hevc_rpi_tb_avail_flags(s, lc, x0, y0, trafo_size, trafo_size);

        do_intra_pred(s, lc, log2_trafo_size, x0, y0, 0, avail);

        if (log2_trafo_size > 2)
            do_intra_pred(s, lc, log2_trafo_size_c, x0_c, y0_c, 1, avail);
        else if (blk_idx == 3)
            do_intra_pred(s, lc, log2_trafo_size_c, x0_c, y0_c, 1,
                          ff_hevc_rpi_tb_avail_flags(s, lc, x0_c, y0_c, 8, 8));

        if (log2_trafo_size < 4) {
            if (lc->tu.intra_pred_mode >= 6 &&
                lc->tu.intra_pred_mode <= 14) {
                scan_idx = SCAN_VERT;
            } else if (lc->tu.intra_pred_mode >= 22 &&
                       lc->tu.intra_pred_mode <= 30) {
                scan_idx = SCAN_HORIZ;
            }

            if (lc->tu.intra_pred_mode_c >=  6 &&
                lc->tu.intra_pred_mode_c <= 14) {
                scan_idx_c = SCAN_VERT;
            } else if (lc->tu.intra_pred_mode_c >= 22 &&
                       lc->tu.intra_pred_mode_c <= 30) {
                scan_idx_c = SCAN_HORIZ;
            }
        }
    }

    if (!cbf_luma && cbf_chroma == 0)
        return 0;

    if (lc->tu.is_cu_qp_delta_wanted)
    {
        const int qp_delta = ff_hevc_rpi_cu_qp_delta(lc);
        const unsigned int cb_mask = ~0U << log2_cb_size;

        if (qp_delta < -(26 + (s->ps.sps->qp_bd_offset >> 1)) ||
            qp_delta >  (25 + (s->ps.sps->qp_bd_offset >> 1)))
        {
            av_log(s->avctx, AV_LOG_ERROR,
                   "The cu_qp_delta %d is outside the valid range "
                   "[%d, %d].\n",
                   qp_delta,
                   -(26 + (s->ps.sps->qp_bd_offset >> 1)),
                    (25 + (s->ps.sps->qp_bd_offset >> 1)));
            return AVERROR_INVALIDDATA;
        }

        lc->tu.is_cu_qp_delta_wanted = 0;
        lc->tu.cu_qp_delta = qp_delta;
        ff_hevc_rpi_set_qPy(s, lc, x0 & cb_mask, y0 & cb_mask);
    }

    // * Not main profile & untested due to no conform streams
    if (lc->tu.cu_chroma_qp_offset_wanted && cbf_chroma &&
        !lc->cu.cu_transquant_bypass_flag) {
        int cu_chroma_qp_offset_flag = ff_hevc_rpi_cu_chroma_qp_offset_flag(lc);
        if (cu_chroma_qp_offset_flag) {
            int cu_chroma_qp_offset_idx  = 0;
            if (s->ps.pps->chroma_qp_offset_list_len_minus1 > 0) {
                cu_chroma_qp_offset_idx = ff_hevc_rpi_cu_chroma_qp_offset_idx(s, lc);
            }
            lc->tu.qp_divmod6[1] += s->ps.pps->cb_qp_offset_list[cu_chroma_qp_offset_idx];
            lc->tu.qp_divmod6[2] += s->ps.pps->cr_qp_offset_list[cu_chroma_qp_offset_idx];
        }
        lc->tu.cu_chroma_qp_offset_wanted = 0;
    }

    if (cbf_luma)
        ff_hevc_rpi_hls_residual_coding(s, lc, x0, y0, log2_trafo_size, scan_idx, 0);

    if (log2_trafo_size > 2 || blk_idx == 3)
    {
        if ((cbf_chroma & CBF_CB0) != 0)
            ff_hevc_rpi_hls_residual_coding(s, lc, x0_c, y0_c,
                                        log2_trafo_size_c, scan_idx_c, 1);
        if ((cbf_chroma & CBF_CR0) != 0)
            ff_hevc_rpi_hls_residual_coding(s, lc, x0_c, y0_c,
                                        log2_trafo_size_c, scan_idx_c, 2);
    }

    return 0;
}

static inline void set_deblocking_bypass(const HEVCRpiContext * const s, const int x0, const int y0, const int log2_cb_size)
{
    set_bits(s->is_pcm + (y0 >> 3) * s->ps.sps->pcm_width, x0 >> 3, s->ps.sps->pcm_width, log2_cb_size - 3);
}


static int hls_transform_tree(const HEVCRpiContext * const s, HEVCRpiLocalContext * const lc,
                              const unsigned int x0, const unsigned int y0,
                              const unsigned int log2_trafo_size,
                              const unsigned int trafo_depth, const unsigned int blk_idx,
                              const unsigned int cbf_c0)
{
    // When trafo_size == 2 hls_transform_unit uses c0 so put in c1
    unsigned int cbf_c1 = cbf_c0;
    int split_transform_flag;
    int ret;

    if (lc->cu.intra_split_flag) {
        if (trafo_depth == 1) {
            lc->tu.intra_pred_mode   = lc->pu.intra_pred_mode[blk_idx];
            if (ctx_cfmt(s) == 3) {
                lc->tu.intra_pred_mode_c = lc->pu.intra_pred_mode_c[blk_idx];
                lc->tu.chroma_mode_c     = lc->pu.chroma_mode_c[blk_idx];
            } else {
                lc->tu.intra_pred_mode_c = lc->pu.intra_pred_mode_c[0];
                lc->tu.chroma_mode_c     = lc->pu.chroma_mode_c[0];
            }
        }
    } else {
        lc->tu.intra_pred_mode   = lc->pu.intra_pred_mode[0];
        lc->tu.intra_pred_mode_c = lc->pu.intra_pred_mode_c[0];
        lc->tu.chroma_mode_c     = lc->pu.chroma_mode_c[0];
    }

    if (log2_trafo_size <= s->ps.sps->log2_max_trafo_size &&
        log2_trafo_size >  s->ps.sps->log2_min_tb_size    &&
        trafo_depth     < lc->cu.max_trafo_depth       &&
        !(lc->cu.intra_split_flag && trafo_depth == 0))
    {
        split_transform_flag = ff_hevc_rpi_split_transform_flag_decode(lc, log2_trafo_size);
    } else {
        int inter_split = s->ps.sps->max_transform_hierarchy_depth_inter == 0 &&
                          lc->cu.pred_mode == MODE_INTER &&
                          lc->cu.part_mode != PART_2Nx2N &&
                          trafo_depth == 0;

        split_transform_flag = log2_trafo_size > s->ps.sps->log2_max_trafo_size ||
                               (lc->cu.intra_split_flag && trafo_depth == 0) ||
                               inter_split;
    }

    if (log2_trafo_size > 2 || ctx_cfmt(s) == 3)
    {
        const int wants_c1 = ctx_cfmt(s) == 2 && (!split_transform_flag || log2_trafo_size == 3);
        cbf_c1 = 0;

        if ((cbf_c0 & CBF_CB0) != 0)
        {
            cbf_c1 = ff_hevc_rpi_cbf_cb_cr_decode(lc, trafo_depth) << CBF_CB0_S;
            if (wants_c1)
                cbf_c1 |= ff_hevc_rpi_cbf_cb_cr_decode(lc, trafo_depth) << CBF_CB1_S;
        }

        if ((cbf_c0 & CBF_CR0) != 0)
        {
            cbf_c1 |= ff_hevc_rpi_cbf_cb_cr_decode(lc, trafo_depth) << CBF_CR0_S;
            if (wants_c1)
                cbf_c1 |= ff_hevc_rpi_cbf_cb_cr_decode(lc, trafo_depth) << CBF_CR1_S;
        }
    }

    if (split_transform_flag) {
        const int trafo_size_split = 1 << (log2_trafo_size - 1);
        const int x1 = x0 + trafo_size_split;
        const int y1 = y0 + trafo_size_split;

#define SUBDIVIDE(x, y, idx)                                                    \
do {                                                                            \
    ret = hls_transform_tree(s, lc, x, y,                                       \
                             log2_trafo_size - 1, trafo_depth + 1, idx,         \
                             cbf_c1);                                           \
    if (ret < 0)                                                                \
        return ret;                                                             \
} while (0)

        SUBDIVIDE(x0, y0, 0);
        SUBDIVIDE(x1, y0, 1);
        SUBDIVIDE(x0, y1, 2);
        SUBDIVIDE(x1, y1, 3);

#undef SUBDIVIDE
    } else {
        // If trafo_size == 2 then we should have cbf_c == 0 here but as we can't have
        // trafo_size == 2 with depth == 0 the issue is moot
        const int cbf_luma = ((lc->cu.pred_mode != MODE_INTRA && trafo_depth == 0 && cbf_c1 == 0) ||
            ff_hevc_rpi_cbf_luma_decode(lc, trafo_depth));

        ret = hls_transform_unit(s, lc, x0, y0,
                                 log2_trafo_size + trafo_depth, log2_trafo_size,
                                 blk_idx, cbf_luma, cbf_c1);
        if (ret < 0)
            return ret;

        if (!s->sh.disable_deblocking_filter_flag) {
            ff_hevc_rpi_deblocking_boundary_strengths(s, lc, x0, y0, log2_trafo_size, cbf_luma);
        }
    }
    return 0;
}


static int pcm_extract(const HEVCRpiContext * const s, const uint8_t * pcm, const int length, const int x0, const int y0, const int cb_size)
{
    GetBitContext gb;
    int ret;

    ret = init_get_bits(&gb, pcm, length);
    if (ret < 0)
        return ret;

    s->hevcdsp.put_pcm(av_rpi_sand_frame_pos_y(s->frame, x0, y0),
                       frame_stride1(s->frame, 0),
                       cb_size, cb_size, &gb, s->ps.sps->pcm.bit_depth);

    s->hevcdsp.put_pcm_c(av_rpi_sand_frame_pos_c(s->frame, x0 >> ctx_hshift(s, 1), y0 >> ctx_vshift(s, 1)),
                       s->frame->linesize[1],
                       cb_size >> ctx_hshift(s, 1),
                       cb_size >> ctx_vshift(s, 1),
                       &gb, s->ps.sps->pcm.bit_depth_chroma);

    return 0;
}


// x * 2^(y*2)
static inline unsigned int xyexp2(const unsigned int x, const unsigned int y)
{
    return x << (y * 2);
}

static int hls_pcm_sample(const HEVCRpiContext * const s, HEVCRpiLocalContext * const lc, const int x0, const int y0, unsigned int log2_cb_size)
{
    // Length in bits
    const unsigned int length = xyexp2(s->ps.sps->pcm.bit_depth, log2_cb_size) +
        xyexp2(s->ps.sps->pcm.bit_depth_chroma, log2_cb_size - ctx_vshift(s, 1)) +
        xyexp2(s->ps.sps->pcm.bit_depth_chroma, log2_cb_size - ctx_vshift(s, 2));

    const uint8_t * const pcm = ff_hevc_rpi_cabac_skip_bytes(&lc->cc, (length + 7) >> 3);

    if (!s->sh.disable_deblocking_filter_flag)
        ff_hevc_rpi_deblocking_boundary_strengths(s, lc, x0, y0, log2_cb_size, 0);

    // Copy coeffs
    {
        const int blen = (length + 7) >> 3;
        // Round allocated bytes up to nearest 32 to avoid alignment confusion
        // Allocation is in int16_t s
        // As we are only using 1 byte per sample and the coeff buffer allows 2 per
        // sample this rounding doesn't affect the total size we need to allocate for
        // the coeff buffer
        int16_t * const coeffs = rpi_alloc_coeff_buf(lc->jb0, 0, ((blen + 31) & ~31) >> 1);
        memcpy(coeffs, pcm, blen);

        // Our coeff stash assumes that any partially allocated 64byte lump
        // is zeroed so make that true.
        {
            uint8_t * const eopcm = (uint8_t *)coeffs + blen;
            if ((-(intptr_t)eopcm & 63) != 0)
                memset(eopcm, 0, -(intptr_t)eopcm & 63);
        }

        // Add command
        {
            HEVCPredCmd *const cmd = rpi_new_intra_cmd(lc->jb0);
            cmd->type = RPI_PRED_I_PCM;
            cmd->size = log2_cb_size;
            cmd->i_pcm.src = coeffs;
            cmd->i_pcm.x = x0;
            cmd->i_pcm.y = y0;
            cmd->i_pcm.src_len = length;
        }
        return 0;
    }
}


static void hevc_await_progress(const HEVCRpiContext * const s, HEVCRpiLocalContext * const lc, const HEVCRpiFrame * const ref,
                                const MvXY xy, const int y0, const int height)
{
    if (s->threads_type != 0) {
        const int y = FFMAX(0, (MV_Y(xy) >> 2) + y0 + height + 9);

        // Progress has to be attached to current job as the actual wait
        // is in worker_core which can't use lc
        int16_t *const pr = lc->jb0->progress_req + ref->dpb_no;
        if (*pr < y) {
            *pr = y;
        }
    }
}

static void hevc_luma_mv_mvp_mode(const HEVCRpiContext * const s, HEVCRpiLocalContext * const lc,
                                  const int x0, const int y0, const int nPbW,
                                  const int nPbH,
                                  HEVCRpiMvField * const mv)
{
    enum InterPredIdc inter_pred_idc = PRED_L0;
    int mvp_flag;
    const unsigned int avail = ff_hevc_rpi_tb_avail_flags(s, lc, x0, y0, nPbW, nPbH);

    mv->pred_flag = 0;
    if (s->sh.slice_type == HEVC_SLICE_B)
        inter_pred_idc = ff_hevc_rpi_inter_pred_idc_decode(lc, nPbW, nPbH);

    if (inter_pred_idc != PRED_L1) {
        MvXY mvd;

        if (s->sh.nb_refs[L0])
            mv->ref_idx[0]= ff_hevc_rpi_ref_idx_lx_decode(lc, s->sh.nb_refs[L0]);

        mv->pred_flag = PF_L0;
        mvd = ff_hevc_rpi_hls_mvd_coding(lc);
        mvp_flag = ff_hevc_rpi_mvp_lx_flag_decode(lc);
        ff_hevc_rpi_luma_mv_mvp_mode(s, lc, x0, y0, nPbW, nPbH, avail,
                                 mv, mvp_flag, 0);
        mv->xy[0] = mvxy_add(mv->xy[0], mvd);
    }

    if (inter_pred_idc != PRED_L0) {
        MvXY mvd = 0;

        if (s->sh.nb_refs[L1])
            mv->ref_idx[1] = ff_hevc_rpi_ref_idx_lx_decode(lc, s->sh.nb_refs[L1]);

        if (s->sh.mvd_l1_zero_flag != 1 || inter_pred_idc != PRED_BI)
            mvd = ff_hevc_rpi_hls_mvd_coding(lc);

        mv->pred_flag += PF_L1;
        mvp_flag = ff_hevc_rpi_mvp_lx_flag_decode(lc);
        ff_hevc_rpi_luma_mv_mvp_mode(s, lc, x0, y0, nPbW, nPbH, avail,
                                 mv, mvp_flag, 1);
        mv->xy[1] = mvxy_add(mv->xy[1], mvd);
    }
}


static HEVCRpiInterPredQ *
rpi_nxt_pred(HEVCRpiInterPredEnv * const ipe, const unsigned int load_val, const uint32_t fn)
{
    HEVCRpiInterPredQ * yp = NULL;
    HEVCRpiInterPredQ * ypt = ipe->q + ipe->curr;
    const unsigned int max_fill = ipe->max_fill;
    unsigned int load = UINT_MAX;

    for (unsigned int i = 0; i != ipe->n_grp; ++i, ++ypt) {
        // We will always have enough room between the Qs but if we are
        // running critically low due to poor scheduling then use fill size
        // rather than load to determine QPU.  This has obvious dire
        // performance implications but (a) it is better than crashing
        // and (b) it should (almost) never happen
        const unsigned int tfill = (char *)ypt->qpu_mc_curr - (char *)ypt->qpu_mc_base;
        const unsigned int tload = tfill > max_fill ? tfill + 0x1000000 : ypt->load;

        if (tload < load)
        {
            yp = ypt;
            load = tload;
        }
    }

    yp->load += load_val;
    ipe->used_grp = 1;
    qpu_mc_link_set(yp->qpu_mc_curr, fn);

    return yp;
}


static void rpi_inter_pred_sync(HEVCRpiInterPredEnv * const ipe)
{
    for (unsigned int i = 0; i != ipe->n; ++i) {
        HEVCRpiInterPredQ * const q = ipe->q + i;
        const unsigned int qfill = (char *)q->qpu_mc_curr - (char *)q->qpu_mc_base;

        qpu_mc_link_set(q->qpu_mc_curr, q->code_sync);
        q->qpu_mc_curr = (qpu_mc_pred_cmd_t *)(&q->qpu_mc_curr->sync + 1);
        q->load = (qfill >> 7); // Have a mild preference for emptier Qs to balance memory usage
    }
}

// Returns 0 on success
// We no longer check for Q fullness as wew have emergncy code in ctu alloc
// * However it might be an idea to have some means of spotting that we've used it
static int rpi_inter_pred_next_ctu(HEVCRpiInterPredEnv * const ipe)
{
    if (!ipe->used_grp)
        return 0;

    if ((ipe->curr += ipe->n_grp) >= ipe->n)
    {
        ipe->curr = 0;
        rpi_inter_pred_sync(ipe);
    }
    ipe->used = 1;
    ipe->used_grp = 0;

    return 0;
}

static void rpi_inter_pred_reset(HEVCRpiInterPredEnv * const ipe)
{
    unsigned int i;

    ipe->curr = 0;
    ipe->used = 0;
    ipe->used_grp = 0;
    for (i = 0; i != ipe->n; ++i) {
        HEVCRpiInterPredQ * const q = ipe->q + i;
        q->qpu_mc_curr = q->qpu_mc_base;
        q->load = 0;
        q->last_l0 = NULL;
        q->last_l1 = NULL;
    }
}

static int rpi_inter_pred_alloc(HEVCRpiInterPredEnv * const ipe,
                                 const unsigned int n_max, const unsigned int n_grp,
                                 const unsigned int total_size, const unsigned int min_gap)
{
    int rv;

    memset(ipe, 0, sizeof(*ipe));
    if ((ipe->q = av_mallocz(n_max * sizeof(*ipe->q))) == NULL)
        return AVERROR(ENOMEM);

    ipe->n_grp = n_grp;
    ipe->min_gap = min_gap;

    if ((rv = gpu_malloc_cached(total_size, &ipe->gptr)) != 0)
        av_freep(&ipe->q);
    return rv;
}


#if RPI_QPU_EMU_Y
#define get_mc_address_y(f) ((f)->data[0])
#else
#define get_mc_address_y(f) get_vc_address_y(f)
#endif
#if RPI_QPU_EMU_C
#define get_mc_address_u(f) ((f)->data[1])
#else
#define get_mc_address_u(f) get_vc_address_u(f)
#endif

static inline uint32_t pack_wo_p(const int off, const int mul)
{
    return PACK2(off * 2 + 1, mul);
}

static inline uint32_t pack_wo_b(const int off0, const int off1, const int mul)
{
    return PACK2(off0 + off1 + 1, mul);
}


static void
rpi_pred_y(const HEVCRpiContext *const s, HEVCRpiJob * const jb,
           const int x0, const int y0,
           const int nPbW, const int nPbH,
           const MvXY mv_xy,
           const int weight_mul,
           const int weight_offset,
           AVFrame *const src_frame)
{
    const unsigned int y_off = av_rpi_sand_frame_off_y(s->frame, x0, y0);
    const unsigned int mx          = MV_X(mv_xy) & 3;
    const unsigned int my          = MV_Y(mv_xy) & 3;
    const unsigned int my_mx       = (my << 8) | mx;
    const uint32_t     my2_mx2_my_mx = (my_mx << 16) | my_mx;
    const qpu_mc_src_addr_t src_vc_address_y = get_mc_address_y(src_frame);
    qpu_mc_dst_addr_t dst_addr = get_mc_address_y(s->frame) + y_off;
    const uint32_t wo = pack_wo_p(weight_offset, weight_mul);
    HEVCRpiInterPredEnv * const ipe = &jb->luma_ip;
    const unsigned int xshl = av_rpi_sand_frame_xshl(s->frame);

    if (my_mx == 0)
    {
        const int x1 = x0 + (MV_X(mv_xy) >> 2);
        const int y1 = y0 + (MV_Y(mv_xy) >> 2);
        const int bh = nPbH;

        for (int start_x = 0; start_x < nPbW; start_x += 16)
        {
            const int bw = FFMIN(nPbW - start_x, 16);
            HEVCRpiInterPredQ *const yp = rpi_nxt_pred(ipe, bh, s->qpu.y_p00);
            qpu_mc_src_t *const src1 = yp->last_l0;
            qpu_mc_pred_y_p00_t *const cmd_y = &yp->qpu_mc_curr->y.p00;

#if RPI_TSTATS
            {
                HEVCRpiStats *const ts = (HEVCRpiStats *)&s->tstats;
                ++ts->y_pred1_x0y0;

                if (nPbW > 8)
                    ++ts->y_pred1_wgt8;
                else
                    ++ts->y_pred1_wle8;

                if (nPbH > 16)
                    ++ts->y_pred1_hgt16;
                else
                    ++ts->y_pred1_hle16;
            }
#endif

            src1->x = x1 + start_x;
            src1->y = y1;
            src1->base = src_vc_address_y;
            cmd_y->w = bw;
            cmd_y->h = bh;
            cmd_y->wo1 = wo;
            cmd_y->dst_addr =  dst_addr + (start_x << xshl);
            yp->last_l0 = &cmd_y->next_src1;
            yp->qpu_mc_curr = (qpu_mc_pred_cmd_t *)(cmd_y + 1);
        }
    }
    else
    {
        const int x1_m3 = x0 + (MV_X(mv_xy) >> 2) - 3;
        const int y1_m3 = y0 + (MV_Y(mv_xy) >> 2) - 3;
        const unsigned int bh = nPbH;
        int start_x = 0;

#if 1
        // As Y-pred operates on two independant 8-wide src blocks we can merge
        // this pred with the previous one if it the previous one is 8 pel wide,
        // the same height as the current block, immediately to the left of our
        // current dest block and mono-pred.

        qpu_mc_pred_y_p_t *const last_y8_p = jb->last_y8_p;
        if (last_y8_p != NULL && last_y8_p->h == bh && last_y8_p->dst_addr + (8 << xshl) == dst_addr)
        {
            const int bw = FFMIN(nPbW, 8);
            qpu_mc_src_t *const last_y8_src2 = jb->last_y8_l1;

            last_y8_src2->x = x1_m3;
            last_y8_src2->y = y1_m3;
            last_y8_src2->base = src_vc_address_y;
            last_y8_p->w += bw;
            last_y8_p->mymx21 = PACK2(my2_mx2_my_mx, last_y8_p->mymx21);
            last_y8_p->wo2 = wo;

            jb->last_y8_p = NULL;
            jb->last_y8_l1 = NULL;
            start_x = bw;
#if RPI_TSTATS
            ++((HEVCRpiStats *)&s->tstats)->y_pred1_y8_merge;
#endif
        }
#endif

        for (; start_x < nPbW; start_x += 16)
        {
            const int bw = FFMIN(nPbW - start_x, 16);
            HEVCRpiInterPredQ *const yp = rpi_nxt_pred(ipe, bh + 7, s->qpu.y_pxx);
            qpu_mc_src_t *const src1 = yp->last_l0;
            qpu_mc_src_t *const src2 = yp->last_l1;
            qpu_mc_pred_y_p_t *const cmd_y = &yp->qpu_mc_curr->y.p;
#if RPI_TSTATS
            {
                HEVCRpiStats *const ts = (HEVCRpiStats *)&s->tstats;
                if (mx == 0 && my == 0)
                    ++ts->y_pred1_x0y0;
                else if (mx == 0)
                    ++ts->y_pred1_x0;
                else if (my == 0)
                    ++ts->y_pred1_y0;
                else
                    ++ts->y_pred1_xy;

                if (nPbW > 8)
                    ++ts->y_pred1_wgt8;
                else
                    ++ts->y_pred1_wle8;

                if (nPbH > 16)
                    ++ts->y_pred1_hgt16;
                else
                    ++ts->y_pred1_hle16;
            }
#endif
            src1->x = x1_m3 + start_x;
            src1->y = y1_m3;
            src1->base = src_vc_address_y;
            if (bw <= 8)
            {
                src2->x = MC_DUMMY_X;
                src2->y = MC_DUMMY_Y;
#if RPI_QPU_EMU_Y
                src2->base = s->qpu_dummy_frame_emu;
#else
                src2->base = s->qpu_dummy_frame_qpu;
#endif
            }
            else
            {
                src2->x = x1_m3 + start_x + 8;
                src2->y = y1_m3;
                src2->base = src_vc_address_y;
            }
            cmd_y->w = bw;
            cmd_y->h = bh;
            cmd_y->mymx21 = my2_mx2_my_mx;
            cmd_y->wo1 = wo;
            cmd_y->wo2 = wo;
            cmd_y->dst_addr =  dst_addr + (start_x << xshl);
            yp->last_l0 = &cmd_y->next_src1;
            yp->last_l1 = &cmd_y->next_src2;
            yp->qpu_mc_curr = (qpu_mc_pred_cmd_t *)(cmd_y + 1);

            if (bw == 8) {
                jb->last_y8_l1 = src2;
                jb->last_y8_p = cmd_y;
            }
        }
    }
}

static void
rpi_pred_y_b(const HEVCRpiContext * const s, HEVCRpiJob * const jb,
           const int x0, const int y0,
           const int nPbW, const int nPbH,
           const struct HEVCRpiMvField *const mv_field,
           const AVFrame *const src_frame,
           const AVFrame *const src_frame2)
{
    const unsigned int y_off = av_rpi_sand_frame_off_y(s->frame, x0, y0);
    const MvXY mv  = mv_field->xy[0];
    const MvXY mv2 = mv_field->xy[1];

    const unsigned int mx          = MV_X(mv) & 3;
    const unsigned int my          = MV_Y(mv) & 3;
    const unsigned int my_mx = (my<<8) | mx;
    const unsigned int mx2          = MV_X(mv2) & 3;
    const unsigned int my2          = MV_Y(mv2) & 3;
    const unsigned int my2_mx2 = (my2<<8) | mx2;
    const uint32_t     my2_mx2_my_mx = (my2_mx2 << 16) | my_mx;
    const unsigned int ref_idx0 = mv_field->ref_idx[0];
    const unsigned int ref_idx1 = mv_field->ref_idx[1];
    const uint32_t wo1 = pack_wo_b(s->sh.luma_offset_l0[ref_idx0], s->sh.luma_offset_l1[ref_idx1], s->sh.luma_weight_l0[ref_idx0]);
    const uint32_t wo2 = pack_wo_b(s->sh.luma_offset_l0[ref_idx0], s->sh.luma_offset_l1[ref_idx1], s->sh.luma_weight_l1[ref_idx1]);

    const unsigned int xshl = av_rpi_sand_frame_xshl(s->frame);
    qpu_mc_dst_addr_t dst = get_mc_address_y(s->frame) + y_off;
    const qpu_mc_src_addr_t src1_base = get_mc_address_y(src_frame);
    const qpu_mc_src_addr_t src2_base = get_mc_address_y(src_frame2);
    HEVCRpiInterPredEnv * const ipe = &jb->luma_ip;

    if (my2_mx2_my_mx == 0)
    {
        const int x1 = x0 + (MV_X(mv) >> 2);
        const int y1 = y0 + (MV_Y(mv) >> 2);
        const int x2 = x0 + (MV_X(mv2) >> 2);
        const int y2 = y0 + (MV_Y(mv2) >> 2);
        const int bh = nPbH;

        // Can do chunks a full 16 wide if we don't want the H filter
        for (int start_x=0; start_x < nPbW; start_x += 16)
        {
            HEVCRpiInterPredQ *const yp = rpi_nxt_pred(ipe, bh, s->qpu.y_b00);
            qpu_mc_src_t *const src1 = yp->last_l0;
            qpu_mc_src_t *const src2 = yp->last_l1;
            qpu_mc_pred_y_p_t *const cmd_y = &yp->qpu_mc_curr->y.p;
#if RPI_TSTATS
            {
                HEVCRpiStats *const ts = (HEVCRpiStats *)&s->tstats;
                ++ts->y_pred2_x0y0;

                if (nPbH > 16)
                    ++ts->y_pred2_hgt16;
                else
                    ++ts->y_pred2_hle16;
            }
#endif
            src1->x = x1 + start_x;
            src1->y = y1;
            src1->base = src1_base;
            src2->x = x2 + start_x;
            src2->y = y2;
            src2->base = src2_base;
            cmd_y->w = FFMIN(nPbW - start_x, 16);
            cmd_y->h = bh;
            cmd_y->mymx21 = 0;
            cmd_y->wo1 = wo1;
            cmd_y->wo2 = wo2;
            cmd_y->dst_addr =  dst + (start_x << xshl);
            yp->last_l0 = &cmd_y->next_src1;
            yp->last_l1 = &cmd_y->next_src2;
            yp->qpu_mc_curr = (qpu_mc_pred_cmd_t *)(cmd_y + 1);
        }
    }
    else
    {
        // Filter requires a run-up of 3
        const int x1 = x0 + (MV_X(mv) >> 2) - 3;
        const int y1 = y0 + (MV_Y(mv) >> 2) - 3;
        const int x2 = x0 + (MV_X(mv2) >> 2) - 3;
        const int y2 = y0 + (MV_Y(mv2) >> 2) - 3;
        const int bh = nPbH;

        for (int start_x=0; start_x < nPbW; start_x += 8)
        { // B blocks work 8 at a time
            // B weights aren't doubled as the QPU code does the same
            // amount of work as it does for P
            HEVCRpiInterPredQ *const yp = rpi_nxt_pred(ipe, bh + 7, s->qpu.y_bxx);
            qpu_mc_src_t *const src1 = yp->last_l0;
            qpu_mc_src_t *const src2 = yp->last_l1;
            qpu_mc_pred_y_p_t *const cmd_y = &yp->qpu_mc_curr->y.p;
#if RPI_TSTATS
            {
                HEVCRpiStats *const ts = (HEVCRpiStats *)&s->tstats;
                const unsigned int mmx = mx | mx2;
                const unsigned int mmy = my | my2;
                if (mmx == 0 && mmy == 0)
                    ++ts->y_pred2_x0y0;
                else if (mmx == 0)
                    ++ts->y_pred2_x0;
                else if (mmy == 0)
                    ++ts->y_pred2_y0;
                else
                    ++ts->y_pred2_xy;

                if (nPbH > 16)
                    ++ts->y_pred2_hgt16;
                else
                    ++ts->y_pred2_hle16;
            }
#endif
            src1->x = x1 + start_x;
            src1->y = y1;
            src1->base = src1_base;
            src2->x = x2 + start_x;
            src2->y = y2;
            src2->base = src2_base;
            cmd_y->w = FFMIN(nPbW - start_x, 8);
            cmd_y->h = bh;
            cmd_y->mymx21 = my2_mx2_my_mx;
            cmd_y->wo1 = wo1;
            cmd_y->wo2 = wo2;
            cmd_y->dst_addr =  dst + (start_x << xshl);
            yp->last_l0 = &cmd_y->next_src1;
            yp->last_l1 = &cmd_y->next_src2;
            yp->qpu_mc_curr = (qpu_mc_pred_cmd_t *)(cmd_y + 1);
        }
    }
}

// h/v shifts fixed at one as that is all the qasm copes with
static void
rpi_pred_c(const HEVCRpiContext * const s, HEVCRpiJob * const jb,
  const unsigned int lx, const int x0_c, const int y0_c,
  const int nPbW_c, const int nPbH_c,
  const MvXY mv,
  const int16_t * const c_weights,
  const int16_t * const c_offsets,
  AVFrame * const src_frame)
{
    const unsigned int c_off = av_rpi_sand_frame_off_c(s->frame, x0_c, y0_c);
    const int hshift = 1; // = s->ps.sps->hshift[1];
    const int vshift = 1; // = s->ps.sps->vshift[1];

    const int x1_c = x0_c + (MV_X(mv) >> (2 + hshift)) - 1;
    const int y1_c = y0_c + (MV_Y(mv) >> (2 + hshift)) - 1;
    const qpu_mc_src_addr_t src_base_u = get_mc_address_u(src_frame);
    const uint32_t x_coeffs = rpi_filter_coefs[av_mod_uintp2(MV_X(mv), 2 + hshift) << (1 - hshift)];
    const uint32_t y_coeffs = rpi_filter_coefs[av_mod_uintp2(MV_Y(mv), 2 + vshift) << (1 - vshift)];
    const uint32_t wo_u = pack_wo_p(c_offsets[0], c_weights[0]);
    const uint32_t wo_v = pack_wo_p(c_offsets[1], c_weights[1]);
    qpu_mc_dst_addr_t dst_base_u = get_mc_address_u(s->frame) + c_off;
    HEVCRpiInterPredEnv * const ipe = &jb->chroma_ip;
    const unsigned int xshl = av_rpi_sand_frame_xshl(s->frame) + 1;
    const unsigned int bh = nPbH_c;
    const uint32_t qfn = lx == 0 ? s->qpu.c_pxx : s->qpu.c_pxx_l1;

    for(int start_x=0; start_x < nPbW_c; start_x+=RPI_CHROMA_BLOCK_WIDTH)
    {
        HEVCRpiInterPredQ * const cp = rpi_nxt_pred(ipe, bh + 3, qfn);
        qpu_mc_pred_c_p_t * const cmd_c = &cp->qpu_mc_curr->c.p;
        qpu_mc_src_t ** const plast_lx = (lx == 0) ? &cp->last_l0 : &cp->last_l1;
        qpu_mc_src_t * const last_lx = *plast_lx;
        const int bw = FFMIN(nPbW_c-start_x, RPI_CHROMA_BLOCK_WIDTH);

        last_lx->x = x1_c + start_x;
        last_lx->y = y1_c;
        last_lx->base = src_base_u;
        cmd_c->h = bh;
        cmd_c->w = bw;
        cmd_c->coeffs_x = x_coeffs;
        cmd_c->coeffs_y = y_coeffs;
        cmd_c->wo_u = wo_u;
        cmd_c->wo_v = wo_v;
        cmd_c->dst_addr_c = dst_base_u + (start_x << xshl);
        *plast_lx = &cmd_c->next_src;
        cp->qpu_mc_curr = (qpu_mc_pred_cmd_t *)(cmd_c + 1);
    }
    return;
}

// h/v shifts fixed at one as that is all the qasm copes with
static void
rpi_pred_c_b(const HEVCRpiContext * const s, HEVCRpiJob * const jb,
  const int x0_c, const int y0_c,
  const int nPbW_c, const int nPbH_c,
  const struct HEVCRpiMvField * const mv_field,
  const int16_t * const c_weights,
  const int16_t * const c_offsets,
  const int16_t * const c_weights2,
  const int16_t * const c_offsets2,
  AVFrame * const src_frame,
  AVFrame * const src_frame2)
{
    const unsigned int c_off = av_rpi_sand_frame_off_c(s->frame, x0_c, y0_c);
    const int hshift = 1; // s->ps.sps->hshift[1];
    const int vshift = 1; // s->ps.sps->vshift[1];
    const MvXY mv = mv_field->xy[0];
    const MvXY mv2 = mv_field->xy[1];

    const unsigned int mx = av_mod_uintp2(MV_X(mv), 2 + hshift);
    const unsigned int my = av_mod_uintp2(MV_Y(mv), 2 + vshift);
    const uint32_t coefs0_x = rpi_filter_coefs[mx << (1 - hshift)];
    const uint32_t coefs0_y = rpi_filter_coefs[my << (1 - vshift)]; // Fractional part of motion vector
    const int x1_c = x0_c + (MV_X(mv) >> (2 + hshift)) - 1;
    const int y1_c = y0_c + (MV_Y(mv) >> (2 + hshift)) - 1;

    const unsigned int mx2 = av_mod_uintp2(MV_X(mv2), 2 + hshift);
    const unsigned int my2 = av_mod_uintp2(MV_Y(mv2), 2 + vshift);
    const uint32_t coefs1_x = rpi_filter_coefs[mx2 << (1 - hshift)];
    const uint32_t coefs1_y = rpi_filter_coefs[my2 << (1 - vshift)]; // Fractional part of motion vector

    const int x2_c = x0_c + (MV_X(mv2) >> (2 + hshift)) - 1;
    const int y2_c = y0_c + (MV_Y(mv2) >> (2 + hshift)) - 1;

    const uint32_t wo_u2 = pack_wo_b(c_offsets[0], c_offsets2[0], c_weights2[0]);
    const uint32_t wo_v2 = pack_wo_b(c_offsets[1], c_offsets2[1], c_weights2[1]);

    const qpu_mc_dst_addr_t dst_base_u = get_mc_address_u(s->frame) + c_off;
    const qpu_mc_src_addr_t src1_base = get_mc_address_u(src_frame);
    const qpu_mc_src_addr_t src2_base = get_mc_address_u(src_frame2);
    HEVCRpiInterPredEnv * const ipe = &jb->chroma_ip;
    const unsigned int xshl = av_rpi_sand_frame_xshl(s->frame) + 1;
    const unsigned int bh = nPbH_c;

    for (int start_x=0; start_x < nPbW_c; start_x += RPI_CHROMA_BLOCK_WIDTH)
    {
        const unsigned int bw = FFMIN(nPbW_c-start_x, RPI_CHROMA_BLOCK_WIDTH);

        HEVCRpiInterPredQ * const cp = rpi_nxt_pred(ipe, bh * 2 + 3, s->qpu.c_bxx);
        qpu_mc_pred_c_b_t * const u = &cp->qpu_mc_curr->c.b;
        qpu_mc_src_t * const src_l0 = cp->last_l0;
        qpu_mc_src_t * const src_l1 = cp->last_l1;

        src_l0->x = x1_c + start_x;
        src_l0->y = y1_c;
        src_l0->base = src1_base;
        src_l1->x = x2_c + start_x;
        src_l1->y = y2_c;
        src_l1->base = src2_base;

        u[0].h = bh;
        u[0].w = bw;
        u[0].coeffs_x1 = coefs0_x;
        u[0].coeffs_y1 = coefs0_y;
        u[0].weight_u1 = c_weights[0]; // Weight L0 U
        u[0].weight_v1 = c_weights[1]; // Weight L0 V
        u[0].coeffs_x2 = coefs1_x;
        u[0].coeffs_y2 = coefs1_y;
        u[0].wo_u2 = wo_u2;
        u[0].wo_v2 = wo_v2;
        u[0].dst_addr_c = dst_base_u + (start_x << xshl);

        cp->last_l0 = &u[0].next_src1;
        cp->last_l1 = &u[0].next_src2;
        cp->qpu_mc_curr = (qpu_mc_pred_cmd_t *)(u + 1);
    }
}


static inline void
col_stash(const HEVCRpiContext * const s,
          const unsigned int x0, const unsigned int y0, const unsigned int w0, const unsigned int h0,
          const HEVCRpiMvField * const mvf)
{
    ColMvField * const col_mvf = s->ref->col_mvf;
    const unsigned int x = (x0 + 15) >> 4;
    const unsigned int y = (y0 + 15) >> 4;
    const unsigned int w = ((x0 + 15 + w0) >> 4) - x;
    const unsigned int h = ((y0 + 15 + h0) >> 4) - y;

    if (col_mvf != NULL && w != 0 && h != 0)
    {
        // Only record MV from the top left of the 16x16 block

        const RefPicList * const rpl = s->refPicList;
        const ColMvField cmv = {
            .L = {
                {
                    .poc = (mvf->pred_flag & PF_L0) == 0 ?
                            COL_POC_INTRA :
                            COL_POC_MAKE_INTER(rpl[0].isLongTerm[mvf->ref_idx[0]], rpl[0].list[mvf->ref_idx[0]]),
                    .xy = mvf->xy[0]
                },
                {
                    .poc = (mvf->pred_flag & PF_L1) == 0 ?
                            COL_POC_INTRA :
                            COL_POC_MAKE_INTER(rpl[1].isLongTerm[mvf->ref_idx[1]], rpl[1].list[mvf->ref_idx[1]]),
                    .xy = mvf->xy[1]
                }
            }
        };

        ColMvField * p = col_mvf + y * s->col_mvf_stride + x;
        const unsigned int stride = s->col_mvf_stride - w;
        unsigned int j = h;

        do
        {
            unsigned int k = w;
            do
            {
                *p++ = cmv;
            } while (--k != 0);
            p += stride;
        } while (--j != 0);
    }
}

static void hls_prediction_unit(const HEVCRpiContext * const s, HEVCRpiLocalContext * const lc,
                                const unsigned int x0, const unsigned int y0,
                                const unsigned int nPbW, const unsigned int nPbH,
                                const unsigned int log2_cb_size, const unsigned int partIdx, const unsigned int idx)
{
    HEVCRpiJob * const jb = lc->jb0;

    struct HEVCRpiMvField current_mv = {{0}};
    const RefPicList  *const refPicList = s->refPicList;
    const HEVCRpiFrame *ref0 = NULL, *ref1 = NULL;

    if (lc->cu.pred_mode != MODE_SKIP)
        lc->pu.merge_flag = ff_hevc_rpi_merge_flag_decode(lc);

    if (lc->cu.pred_mode == MODE_SKIP || lc->pu.merge_flag) {
        const unsigned int merge_idx = s->sh.max_num_merge_cand <= 1 ? 0 :
            ff_hevc_rpi_merge_idx_decode(s, lc);

        ff_hevc_rpi_luma_mv_merge_mode(s, lc, x0, y0, nPbW, nPbH, log2_cb_size,
                                   partIdx, merge_idx, &current_mv);
    } else {
        hevc_luma_mv_mvp_mode(s, lc, x0, y0, nPbW, nPbH, &current_mv);
    }

    {
        HEVCRpiMvField * p = mvf_stash_ptr(s, lc, x0, y0);
        unsigned int i, j;

        for (j = 0; j < nPbH >> LOG2_MIN_PU_SIZE; j++)
        {
            for (i = 0; i < nPbW >> LOG2_MIN_PU_SIZE; i++)
                p[i] = current_mv;
            p += MVF_STASH_WIDTH_PU;
        }
    }

    col_stash(s, x0, y0, nPbW, nPbH, &current_mv);

    if (current_mv.pred_flag & PF_L0) {
        ref0 = refPicList[0].ref[current_mv.ref_idx[0]];
        if (!ref0)
            return;
        hevc_await_progress(s, lc, ref0, current_mv.xy[0], y0, nPbH);
    }
    if (current_mv.pred_flag & PF_L1) {
        ref1 = refPicList[1].ref[current_mv.ref_idx[1]];
        if (!ref1)
            return;
        hevc_await_progress(s, lc, ref1, current_mv.xy[1], y0, nPbH);
    }

    if (current_mv.pred_flag == PF_L0) {
        const int x0_c = x0 >> ctx_hshift(s, 1);
        const int y0_c = y0 >> ctx_vshift(s, 1);
        const int nPbW_c = nPbW >> ctx_hshift(s, 1);
        const int nPbH_c = nPbH >> ctx_vshift(s, 1);

        rpi_pred_y(s, jb, x0, y0, nPbW, nPbH, current_mv.xy[0],
          s->sh.luma_weight_l0[current_mv.ref_idx[0]], s->sh.luma_offset_l0[current_mv.ref_idx[0]],
          ref0->frame);

        if (ctx_cfmt(s) != 0) {
            rpi_pred_c(s, jb, 0, x0_c, y0_c, nPbW_c, nPbH_c, current_mv.xy[0],
              s->sh.chroma_weight_l0[current_mv.ref_idx[0]], s->sh.chroma_offset_l0[current_mv.ref_idx[0]],
              ref0->frame);
            return;
        }
    } else if (current_mv.pred_flag == PF_L1) {
        const int x0_c = x0 >> ctx_hshift(s, 1);
        const int y0_c = y0 >> ctx_vshift(s, 1);
        const int nPbW_c = nPbW >> ctx_hshift(s, 1);
        const int nPbH_c = nPbH >> ctx_vshift(s, 1);

        rpi_pred_y(s, jb, x0, y0, nPbW, nPbH, current_mv.xy[1],
          s->sh.luma_weight_l1[current_mv.ref_idx[1]], s->sh.luma_offset_l1[current_mv.ref_idx[1]],
          ref1->frame);

        if (ctx_cfmt(s) != 0) {
            rpi_pred_c(s, jb, 1, x0_c, y0_c, nPbW_c, nPbH_c, current_mv.xy[1],
              s->sh.chroma_weight_l1[current_mv.ref_idx[1]], s->sh.chroma_offset_l1[current_mv.ref_idx[1]],
              ref1->frame);
            return;
        }
    } else if (current_mv.pred_flag == PF_BI) {
        const int x0_c = x0 >> ctx_hshift(s, 1);
        const int y0_c = y0 >> ctx_vshift(s, 1);
        const int nPbW_c = nPbW >> ctx_hshift(s, 1);
        const int nPbH_c = nPbH >> ctx_vshift(s, 1);

        rpi_pred_y_b(s, jb, x0, y0, nPbW, nPbH, &current_mv, ref0->frame, ref1->frame);

        if (ctx_cfmt(s) != 0) {
          rpi_pred_c_b(s, jb, x0_c, y0_c, nPbW_c, nPbH_c,
                       &current_mv,
                       s->sh.chroma_weight_l0[current_mv.ref_idx[0]],
                       s->sh.chroma_offset_l0[current_mv.ref_idx[0]],
                       s->sh.chroma_weight_l1[current_mv.ref_idx[1]],
                       s->sh.chroma_offset_l1[current_mv.ref_idx[1]],
                       ref0->frame,
                       ref1->frame);
            return;
        }
    }
}

static void set_ipm(const HEVCRpiContext * const s, HEVCRpiLocalContext * const lc,
                    const unsigned int x0, const unsigned int y0,
                    const unsigned int log2_cb_size,
                    const unsigned int ipm)
{
    const unsigned int x_pu = x0 >> LOG2_MIN_PU_SIZE;
    const unsigned int y_pu = y0 >> LOG2_MIN_PU_SIZE;

    {
        const unsigned int ctb_mask = ~(~0U << (s->ps.sps->log2_ctb_size - LOG2_MIN_PU_SIZE));
        set_stash2(lc->ipm_left + (y_pu & ctb_mask), lc->ipm_up + (x_pu & ctb_mask), log2_cb_size - LOG2_MIN_PU_SIZE, ipm);
    }

    // If IRAP then everything is Intra & we avoid ever looking at these
    // stashes so don't bother setting them
    if (!s->is_irap && lc->cu.pred_mode == MODE_INTRA)
    {
        if (s->is_intra != NULL)
        {
            set_bits(s->is_intra + (y0 >> LOG2_MIN_CU_SIZE) * s->ps.sps->pcm_width, x0 >> LOG2_MIN_CU_SIZE, s->ps.sps->pcm_width, log2_cb_size - LOG2_MIN_CU_SIZE);
        }

        {
            HEVCRpiMvField * p = mvf_stash_ptr(s, lc, x0, y0);
            const unsigned int size_in_pus = (1 << log2_cb_size) >> LOG2_MIN_PU_SIZE; // min_pu <= log2_cb so >= 1
            unsigned int n = size_in_pus;

            do
            {
                memset(p, 0, size_in_pus * sizeof(*p));
                p += MVF_STASH_WIDTH_PU;
            } while (--n != 0);
        }


        if (s->ref->col_mvf != NULL && ((x0 | y0) & 0xf) == 0)
        {
            // Only record top left stuff
            // Blocks should always be alinged on size boundries
            // so cannot have overflow from a small block

            ColMvField * p = s->ref->col_mvf + (y0 >> 4) * s->col_mvf_stride + (x0 >> 4);
            const unsigned int size_in_col = log2_cb_size < 4 ? 1 : (1 << (log2_cb_size - 4));
            const unsigned int stride = s->col_mvf_stride - size_in_col;
            unsigned int j = size_in_col;

            do
            {
                unsigned int k = size_in_col;
                do
                {
                    p->L[0].poc = COL_POC_INTRA;
                    p->L[0].xy = 0;
                    p->L[1].poc = COL_POC_INTRA;
                    p->L[1].xy = 0;
                    ++p;
                } while (--k != 0);
                p += stride;
            } while (--j != 0);
        }
    }
}

static inline void intra_prediction_unit_default_value(const HEVCRpiContext * const s, HEVCRpiLocalContext * const lc,
                                                const unsigned int x0, const unsigned int y0,
                                                const unsigned int log2_cb_size)
{
    set_ipm(s, lc, x0, y0, log2_cb_size, INTRA_DC);
}


/**
 * 8.4.1
 */
static int luma_intra_pred_mode(const HEVCRpiContext * const s, HEVCRpiLocalContext * const lc,
                                int x0, int y0, int log2_pu_size,
                                int prev_intra_luma_pred_flag,
                                const unsigned int idx)
{
    const unsigned int ctb_mask = ~(~0U << s->ps.sps->log2_ctb_size);
    const unsigned int xb_pu = (x0 & ctb_mask) >> LOG2_MIN_PU_SIZE;
    const unsigned int yb_pu = (y0 & ctb_mask) >> LOG2_MIN_PU_SIZE;

    // Up does not cross boundries so as we always scan 1 slice-tile-line in an
    // lc we can just keep 1 CTB lR stashes
    // Left is reset to DC @ Start of Line/Tile/Slice in fill_job
    const unsigned int cand_up   = yb_pu == 0 ? INTRA_DC : lc->ipm_up[xb_pu];
    const unsigned int cand_left = lc->ipm_left[yb_pu];

    unsigned int intra_pred_mode;
    unsigned int a, b, c;

    if (cand_left == cand_up) {
        if (cand_left < 2) {
            a = INTRA_PLANAR;
            b = INTRA_DC;
            c = INTRA_ANGULAR_26;
        } else {
            a = cand_left;
            b = 2 + ((cand_left - 2 - 1 + 32) & 31);
            c = 2 + ((cand_left - 2 + 1) & 31);
        }
    } else {
        a = cand_left;
        b = cand_up;
        c = (cand_left != INTRA_PLANAR && cand_up != INTRA_PLANAR) ?
                INTRA_PLANAR :
            (cand_left != INTRA_DC && cand_up != INTRA_DC) ?
                INTRA_DC :
                INTRA_ANGULAR_26;
    }

    if (prev_intra_luma_pred_flag) {
        intra_pred_mode = idx == 0 ? a : idx == 1 ? b : c;
    } else {
        // Sort lowest 1st
        if (a > b)
            FFSWAP(int, a, b);
        if (a > c)
            FFSWAP(int, a, c);
        if (b > c)
            FFSWAP(int, b, c);

        intra_pred_mode = idx;
        if (intra_pred_mode >= a)
            intra_pred_mode++;
        if (intra_pred_mode >= b)
            intra_pred_mode++;
        if (intra_pred_mode >= c)
            intra_pred_mode++;
    }

    /* write the intra prediction units into the mv array */
    set_ipm(s, lc, x0, y0, log2_pu_size, intra_pred_mode);
    return intra_pred_mode;
}

static const uint8_t tab_mode_idx[] = {
     0,  1,  2,  2,  2,  2,  3,  5,  7,  8, 10, 12, 13, 15, 17, 18, 19, 20,
    21, 22, 23, 23, 24, 24, 25, 25, 26, 27, 27, 28, 28, 29, 29, 30, 31};

static void intra_prediction_unit(const HEVCRpiContext * const s, HEVCRpiLocalContext * const lc,
                                  const unsigned int x0, const unsigned int y0,
                                  const unsigned int log2_cb_size)
{
    static const uint8_t intra_chroma_table[4] = { 0, 26, 10, 1 };
    uint8_t prev_intra_luma_pred_flag[4];
    int split   = lc->cu.part_mode == PART_NxN;
    const unsigned int split_size = (1 << (log2_cb_size - 1));
    int chroma_mode;
    const unsigned int n = split ? 4 : 1;
    unsigned int i;

    for (i = 0; i != n; i++)
        prev_intra_luma_pred_flag[i] = ff_hevc_rpi_prev_intra_luma_pred_flag_decode(lc);

    for (i = 0; i < n; i++) {
        // depending on mode idx is mpm or luma_pred_mode
        const unsigned int idx = prev_intra_luma_pred_flag[i] ?
            ff_hevc_rpi_mpm_idx_decode(lc) :
            ff_hevc_rpi_rem_intra_luma_pred_mode_decode(lc);

        lc->pu.intra_pred_mode[i] =
            luma_intra_pred_mode(s, lc,
                                 x0 + ((i & 1) == 0 ? 0 : split_size),
                                 y0 + ((i & 2) == 0 ? 0 : split_size),
                                 log2_cb_size - split,
                                 prev_intra_luma_pred_flag[i], idx);
    }

    if (ctx_cfmt(s) == 3) {
        for (i = 0; i < n; i++) {
            lc->pu.chroma_mode_c[i] = chroma_mode = ff_hevc_rpi_intra_chroma_pred_mode_decode(lc);
            if (chroma_mode != 4) {
                if (lc->pu.intra_pred_mode[i] == intra_chroma_table[chroma_mode])
                    lc->pu.intra_pred_mode_c[i] = 34;
                else
                    lc->pu.intra_pred_mode_c[i] = intra_chroma_table[chroma_mode];
            } else {
                lc->pu.intra_pred_mode_c[i] = lc->pu.intra_pred_mode[i];
            }
        }
    } else if (ctx_cfmt(s) == 2) {
        int mode_idx;
        lc->pu.chroma_mode_c[0] = chroma_mode = ff_hevc_rpi_intra_chroma_pred_mode_decode(lc);
        if (chroma_mode != 4) {
            if (lc->pu.intra_pred_mode[0] == intra_chroma_table[chroma_mode])
                mode_idx = 34;
            else
                mode_idx = intra_chroma_table[chroma_mode];
        } else {
            mode_idx = lc->pu.intra_pred_mode[0];
        }
        lc->pu.intra_pred_mode_c[0] = tab_mode_idx[mode_idx];
    } else if (ctx_cfmt(s) != 0) {
        chroma_mode = ff_hevc_rpi_intra_chroma_pred_mode_decode(lc);
        if (chroma_mode != 4) {
            if (lc->pu.intra_pred_mode[0] == intra_chroma_table[chroma_mode])
                lc->pu.intra_pred_mode_c[0] = 34;
            else
                lc->pu.intra_pred_mode_c[0] = intra_chroma_table[chroma_mode];
        } else {
            lc->pu.intra_pred_mode_c[0] = lc->pu.intra_pred_mode[0];
        }
    }
}

static int hls_coding_unit(const HEVCRpiContext * const s, HEVCRpiLocalContext * const lc,
                           const unsigned int x0, const unsigned int y0, const unsigned int log2_cb_size)
{
    const unsigned int cb_size          = 1 << log2_cb_size;
    const unsigned int log2_min_cb_size = s->ps.sps->log2_min_cb_size;
    const unsigned int min_cb_width     = s->ps.sps->min_cb_width;
    const unsigned int x_cb             = x0 >> log2_min_cb_size;
    const unsigned int y_cb             = y0 >> log2_min_cb_size;
    const unsigned int idx              = log2_cb_size - 2;
    const unsigned int qp_block_mask    = (1 << s->ps.pps->log2_min_cu_qp_delta_size) - 1;
    int skip_flag = 0;

    lc->cu.x                = x0;
    lc->cu.y                = y0;
    lc->cu.x_split          = x0;
    lc->cu.y_split          = y0;

    lc->cu.pred_mode        = MODE_INTRA;
    lc->cu.part_mode        = PART_2Nx2N;
    lc->cu.intra_split_flag = 0;
    lc->cu.cu_transquant_bypass_flag = 0;
    lc->pu.intra_pred_mode[0] = 1;
    lc->pu.intra_pred_mode[1] = 1;
    lc->pu.intra_pred_mode[2] = 1;
    lc->pu.intra_pred_mode[3] = 1;

    if (s->ps.pps->transquant_bypass_enable_flag) {
        lc->cu.cu_transquant_bypass_flag = ff_hevc_rpi_cu_transquant_bypass_flag_decode(lc);
        if (lc->cu.cu_transquant_bypass_flag)
            set_deblocking_bypass(s, x0, y0, log2_cb_size);
    }

    if (s->sh.slice_type != HEVC_SLICE_I) {
        lc->cu.pred_mode = MODE_INTER;
        skip_flag = ff_hevc_rpi_skip_flag_decode(s, lc, x0, y0, x_cb, y_cb);
    }

    if (skip_flag) {
        lc->cu.pred_mode = MODE_SKIP;

        hls_prediction_unit(s, lc, x0, y0, cb_size, cb_size, log2_cb_size, 0, idx);
        intra_prediction_unit_default_value(s, lc, x0, y0, log2_cb_size);

        if (!s->sh.disable_deblocking_filter_flag)
            ff_hevc_rpi_deblocking_boundary_strengths(s, lc, x0, y0, log2_cb_size, 0);
    } else {
        int pcm_flag = 0;

        if (s->sh.slice_type != HEVC_SLICE_I)
            lc->cu.pred_mode = ff_hevc_rpi_pred_mode_decode(lc);
        if (lc->cu.pred_mode != MODE_INTRA ||
            log2_cb_size == s->ps.sps->log2_min_cb_size) {
            lc->cu.part_mode        = ff_hevc_rpi_part_mode_decode(s, lc, log2_cb_size);
            lc->cu.intra_split_flag = lc->cu.part_mode == PART_NxN &&
                                      lc->cu.pred_mode == MODE_INTRA;
        }

        if (lc->cu.pred_mode == MODE_INTRA) {
            if (lc->cu.part_mode == PART_2Nx2N &&
                log2_cb_size <= s->ps.sps->pcm.log2_max_pcm_cb_size &&  // 0 if not enabled
                log2_cb_size >= s->ps.sps->pcm.log2_min_pcm_cb_size &&
                ff_hevc_rpi_pcm_flag_decode(lc) != 0)
            {
                int ret;
                pcm_flag = 1;
                intra_prediction_unit_default_value(s, lc, x0, y0, log2_cb_size);
                if ((ret = hls_pcm_sample(s, lc, x0, y0, log2_cb_size)) < 0)
                    return ret;

                if (s->ps.sps->pcm.loop_filter_disable_flag)
                    set_deblocking_bypass(s, x0, y0, log2_cb_size);
            } else {
                intra_prediction_unit(s, lc, x0, y0, log2_cb_size);
            }
        } else {
            intra_prediction_unit_default_value(s, lc, x0, y0, log2_cb_size);
            switch (lc->cu.part_mode) {
            case PART_2Nx2N:
                hls_prediction_unit(s, lc, x0, y0, cb_size, cb_size, log2_cb_size, 0, idx);
                break;
            case PART_2NxN:
                hls_prediction_unit(s, lc, x0, y0,               cb_size, cb_size / 2, log2_cb_size, 0, idx);
                lc->cu.y_split = y0 + cb_size / 2;
                hls_prediction_unit(s, lc, x0, y0 + cb_size / 2, cb_size, cb_size / 2, log2_cb_size, 1, idx);
                break;
            case PART_Nx2N:
                hls_prediction_unit(s, lc, x0,               y0, cb_size / 2, cb_size, log2_cb_size, 0, idx - 1);
                lc->cu.x_split = x0 + cb_size / 2;
                hls_prediction_unit(s, lc, x0 + cb_size / 2, y0, cb_size / 2, cb_size, log2_cb_size, 1, idx - 1);
                break;
            case PART_2NxnU:
                hls_prediction_unit(s, lc, x0, y0,               cb_size, cb_size     / 4, log2_cb_size, 0, idx);
                lc->cu.y_split = y0 + cb_size / 4;
                hls_prediction_unit(s, lc, x0, y0 + cb_size / 4, cb_size, cb_size / 4 * 3, log2_cb_size, 1, idx);
                break;
            case PART_2NxnD:
                hls_prediction_unit(s, lc, x0, y0,                   cb_size, cb_size / 4 * 3, log2_cb_size, 0, idx);
                lc->cu.y_split = y0 + cb_size / 4 * 3;
                hls_prediction_unit(s, lc, x0, y0 + cb_size / 4 * 3, cb_size, cb_size     / 4, log2_cb_size, 1, idx);
                break;
            case PART_nLx2N:
                hls_prediction_unit(s, lc, x0,               y0, cb_size     / 4, cb_size, log2_cb_size, 0, idx - 2);
                lc->cu.x_split = x0 + cb_size / 4;
                hls_prediction_unit(s, lc, x0 + cb_size / 4, y0, cb_size * 3 / 4, cb_size, log2_cb_size, 1, idx - 2);
                break;
            case PART_nRx2N:
                hls_prediction_unit(s, lc, x0,                   y0, cb_size / 4 * 3, cb_size, log2_cb_size, 0, idx - 2);
                lc->cu.x_split = x0 + cb_size / 4 * 3;
                hls_prediction_unit(s, lc, x0 + cb_size / 4 * 3, y0, cb_size     / 4, cb_size, log2_cb_size, 1, idx - 2);
                break;
            case PART_NxN:
                hls_prediction_unit(s, lc, x0,               y0,               cb_size / 2, cb_size / 2, log2_cb_size, 0, idx - 1);
                lc->cu.x_split = x0 + cb_size / 2;
                hls_prediction_unit(s, lc, x0 + cb_size / 2, y0,               cb_size / 2, cb_size / 2, log2_cb_size, 1, idx - 1);
                lc->cu.y_split = y0 + cb_size / 2;
                hls_prediction_unit(s, lc, x0,               y0 + cb_size / 2, cb_size / 2, cb_size / 2, log2_cb_size, 2, idx - 1);
                hls_prediction_unit(s, lc, x0 + cb_size / 2, y0 + cb_size / 2, cb_size / 2, cb_size / 2, log2_cb_size, 3, idx - 1);
                break;
            }
        }

        if (!pcm_flag) {
            int rqt_root_cbf = 1;

            if (lc->cu.pred_mode != MODE_INTRA &&
                !(lc->cu.part_mode == PART_2Nx2N && lc->pu.merge_flag)) {
                rqt_root_cbf = ff_hevc_rpi_no_residual_syntax_flag_decode(lc);
            }
            if (rqt_root_cbf) {
                const unsigned int cbf_c = ctx_cfmt(s) == 0 ? 0 : (CBF_CR0 | CBF_CB0);
                int ret;

                lc->cu.max_trafo_depth = lc->cu.pred_mode == MODE_INTRA ?
                                         s->ps.sps->max_transform_hierarchy_depth_intra + lc->cu.intra_split_flag :
                                         s->ps.sps->max_transform_hierarchy_depth_inter;
                // transform_tree does deblock_boundary_strengths
                ret = hls_transform_tree(s, lc, x0, y0,
                                         log2_cb_size, 0, 0, cbf_c);
                if (ret < 0)
                    return ret;
            } else {
                if (!s->sh.disable_deblocking_filter_flag)
                    ff_hevc_rpi_deblocking_boundary_strengths(s, lc, x0, y0, log2_cb_size, 0);
            }
        }
    }

    // If the delta is still wanted then we haven't read the delta & therefore need to set qp here
    if (lc->tu.is_cu_qp_delta_wanted)
        ff_hevc_rpi_set_qPy(s, lc, x0, y0);

    if(((x0 + (1<<log2_cb_size)) & qp_block_mask) == 0 &&
       ((y0 + (1<<log2_cb_size)) & qp_block_mask) == 0) {
        lc->qPy_pred = lc->qp_y;
    }

    set_bytes(s->qp_y_tab + y_cb * min_cb_width + x_cb, min_cb_width, log2_cb_size - log2_min_cb_size, lc->qp_y & 0xff);

    set_stash2(s->cabac_stash_up + (x0 >> 3), s->cabac_stash_left + (y0 >> 3), log2_cb_size - 3, (lc->ct_depth << 1) | skip_flag);

    return 0;
}

// Returns:
//  < 0  Error
//  0    More data wanted
//  1    EoSlice / EoPicture
static int hls_coding_quadtree(const HEVCRpiContext * const s, HEVCRpiLocalContext * const lc, const int x0, const int y0,
                               const int log2_cb_size, const unsigned int cb_depth)
{
    const int cb_size    = 1 << log2_cb_size;
    int ret;
    int split_cu;

    lc->ct_depth = cb_depth;
    split_cu = (log2_cb_size > s->ps.sps->log2_min_cb_size);
    if (x0 + cb_size <= s->ps.sps->width  &&
        y0 + cb_size <= s->ps.sps->height &&
        split_cu)
    {
        split_cu = ff_hevc_rpi_split_coding_unit_flag_decode(s, lc, cb_depth, x0, y0);
    }

    // Qp delta (and offset) need to remain wanted if cb_size < min until
    // a coded block is found so we still initial state at depth 0 (outside
    // this fn) and only reset here
    if (s->ps.pps->cu_qp_delta_enabled_flag &&
        log2_cb_size >= s->ps.pps->log2_min_cu_qp_delta_size)
    {
        lc->tu.is_cu_qp_delta_wanted = 1;
        lc->tu.cu_qp_delta          = 0;
    }
    if (s->sh.cu_chroma_qp_offset_enabled_flag &&
        log2_cb_size >= s->ps.pps->log2_min_cu_qp_delta_size)
    {
        lc->tu.cu_chroma_qp_offset_wanted = 1;
    }

    lc->tu.qp_divmod6[0] = s->ps.pps->qp_bd_x[0];
    lc->tu.qp_divmod6[1] = s->ps.pps->qp_bd_x[1] + s->sh.slice_cb_qp_offset;
    lc->tu.qp_divmod6[2] = s->ps.pps->qp_bd_x[2] + s->sh.slice_cr_qp_offset;

    if (split_cu) {
        int qp_block_mask = (1 << s->ps.pps->log2_min_cu_qp_delta_size) - 1;
        const int cb_size_split = cb_size >> 1;
        const int x1 = x0 + cb_size_split;
        const int y1 = y0 + cb_size_split;

        int more_data = 0;

        more_data = hls_coding_quadtree(s, lc, x0, y0, log2_cb_size - 1, cb_depth + 1);
        if (more_data < 0)
            return more_data;

        if (more_data && x1 < s->ps.sps->width) {
            more_data = hls_coding_quadtree(s, lc, x1, y0, log2_cb_size - 1, cb_depth + 1);
            if (more_data < 0)
                return more_data;
        }
        if (more_data && y1 < s->ps.sps->height) {
            more_data = hls_coding_quadtree(s, lc, x0, y1, log2_cb_size - 1, cb_depth + 1);
            if (more_data < 0)
                return more_data;
        }
        if (more_data && x1 < s->ps.sps->width &&
            y1 < s->ps.sps->height) {
            more_data = hls_coding_quadtree(s, lc, x1, y1, log2_cb_size - 1, cb_depth + 1);
            if (more_data < 0)
                return more_data;
        }

        if(((x0 + (1<<log2_cb_size)) & qp_block_mask) == 0 &&
            ((y0 + (1<<log2_cb_size)) & qp_block_mask) == 0)
            lc->qPy_pred = lc->qp_y;

        if (more_data)
            return ((x1 + cb_size_split) < s->ps.sps->width ||
                    (y1 + cb_size_split) < s->ps.sps->height);
        else
            return 0;
    } else {
        ret = hls_coding_unit(s, lc, x0, y0, log2_cb_size);
        if (ret < 0)
            return ret;
        if ((!((x0 + cb_size) %
               (1 << (s->ps.sps->log2_ctb_size))) ||
             (x0 + cb_size >= s->ps.sps->width)) &&
            (!((y0 + cb_size) %
               (1 << (s->ps.sps->log2_ctb_size))) ||
             (y0 + cb_size >= s->ps.sps->height))) {
            int end_of_slice_flag = ff_hevc_rpi_get_cabac_terminate(&lc->cc);
            return !end_of_slice_flag;
        } else {
            return 1;
        }
    }

    return 0;  // NEVER
}

static void hls_decode_neighbour(const HEVCRpiContext * const s, HEVCRpiLocalContext * const lc,
                                 const int x_ctb, const int y_ctb, const int ctb_addr_ts)
{
    const unsigned int ctb_size          = 1 << s->ps.sps->log2_ctb_size;
    const unsigned int ctb_addr_rs       = s->ps.pps->ctb_addr_ts_to_rs[ctb_addr_ts];
    const unsigned int ctb_addr_rs_in_slice = ctb_addr_rs - s->sh.slice_addr;  // slice_addr = RS addr of start of slice
    const unsigned int ctb_flags = s->ps.pps->ctb_ts_flags[ctb_addr_ts];
    const unsigned int line_w = s->ps.sps->ctb_width;

    s->tab_slice_address[ctb_addr_rs] = s->sh.slice_addr;

    lc->end_of_ctb_x = FFMIN(x_ctb + ctb_size, s->ps.sps->width);
    lc->end_of_ctb_y = FFMIN(y_ctb + ctb_size, s->ps.sps->height);

    lc->boundary_flags = 0;

    if ((ctb_flags & CTB_TS_FLAGS_SOTL) != 0)
        lc->boundary_flags |= BOUNDARY_LEFT_TILE;
    if (x_ctb > 0 && s->tab_slice_address[ctb_addr_rs] != s->tab_slice_address[ctb_addr_rs - 1])
        lc->boundary_flags |= BOUNDARY_LEFT_SLICE;
    if ((ctb_flags & CTB_TS_FLAGS_TOT) != 0)
        lc->boundary_flags |= BOUNDARY_UPPER_TILE;
    if (y_ctb > 0 && s->tab_slice_address[ctb_addr_rs] != s->tab_slice_address[ctb_addr_rs - line_w])
        lc->boundary_flags |= BOUNDARY_UPPER_SLICE;

    // Use line width rather than tile width for addr_in_slice test as
    // addr_in_slice is in raster units

    lc->ctb_avail =
        ((lc->boundary_flags & (BOUNDARY_LEFT_SLICE | BOUNDARY_LEFT_TILE)) == 0 ? AVAIL_L : 0) |
        ((lc->boundary_flags & (BOUNDARY_UPPER_SLICE | BOUNDARY_UPPER_TILE)) == 0 ? AVAIL_U : 0) |
        ((lc->boundary_flags & (BOUNDARY_LEFT_TILE | BOUNDARY_UPPER_TILE)) == 0 &&
            (ctb_addr_rs_in_slice > line_w) ? AVAIL_UL : 0) |
        ((ctb_flags & (CTB_TS_FLAGS_EOTL | CTB_TS_FLAGS_TOT)) == 0 &&
            (ctb_addr_rs_in_slice + 1 >= line_w) ? AVAIL_UR : 0);
    // Down-left never avail at CTB level
}


static void rpi_execute_dblk_cmds(const HEVCRpiContext * const s, HEVCRpiJob * const jb)
{
    int y = ff_hevc_rpi_hls_filter_blk(s, jb->bounds,
        (s->ps.pps->ctb_ts_flags[jb->ctu_ts_last] & CTB_TS_FLAGS_EOT) != 0);

    // Signal
    if (y > 0) {
        // Cast away const as progress is held in s, but this really shouldn't confuse anything
        ff_hevc_rpi_progress_signal_recon((HEVCRpiContext *)s, y - 1);
    }

    // Job done now
    // ? Move outside this fn
    job_free(s->jbc, jb);
}

// I-pred, transform_and_add for all blocks types done here
// All ARM
static void rpi_execute_pred_cmds(const HEVCRpiContext * const s, HEVCRpiJob * const jb)
{
    unsigned int i;
    HEVCRpiIntraPredEnv * const iap = &jb->intra;
    const HEVCPredCmd *cmd = iap->cmds;

#if !RPI_WORKER_WAIT_PASS_0
    rpi_sem_wait(&jb->sem);
    rpi_cache_flush_execute(jb->rfe);  // Invalidate data set up in pass1
#endif

    for (i = iap->n; i > 0; i--, cmd++)
    {
        switch (cmd->type)
        {
            case RPI_PRED_INTRA:
                s->hpc.intra_pred(s, cmd->i_pred.mode, cmd->i_pred.x, cmd->i_pred.y, cmd->avail, cmd->size);
                break;
            case RPI_PRED_INTRA_C:
                s->hpc.intra_pred_c(s, cmd->i_pred.mode, cmd->i_pred.x, cmd->i_pred.y, cmd->avail, cmd->size);
                break;
            case RPI_PRED_ADD_RESIDUAL:
                s->hevcdsp.add_residual[cmd->size - 2](cmd->ta.dst, (int16_t *)cmd->ta.buf, cmd->ta.stride);
                break;
            case RPI_PRED_ADD_DC:
                s->hevcdsp.add_residual_dc[cmd->size - 2](cmd->dc.dst, cmd->dc.stride, cmd->dc.dc);
                break;
            case RPI_PRED_ADD_RESIDUAL_U:
                s->hevcdsp.add_residual_u[cmd->size - 2](cmd->ta.dst, (int16_t *)cmd->ta.buf, cmd->ta.stride, cmd->ta.dc);
                break;
            case RPI_PRED_ADD_RESIDUAL_V:
                s->hevcdsp.add_residual_v[cmd->size - 2](cmd->ta.dst, (int16_t *)cmd->ta.buf, cmd->ta.stride, cmd->ta.dc);
                break;
            case RPI_PRED_ADD_RESIDUAL_C:
                s->hevcdsp.add_residual_c[cmd->size - 2](cmd->ta.dst, (int16_t *)cmd->ta.buf, cmd->ta.stride);
                break;
            case RPI_PRED_ADD_DC_U:
            case RPI_PRED_ADD_DC_V:
                s->hevcdsp.add_residual_dc_c[cmd->size - 2](cmd->dc.dst, cmd->dc.stride, cmd->dc.dc);
                break;

            case RPI_PRED_I_PCM:
                pcm_extract(s, cmd->i_pcm.src, cmd->i_pcm.src_len, cmd->i_pcm.x, cmd->i_pcm.y, 1 << cmd->size);
                break;

            default:
                av_log(s->avctx, AV_LOG_PANIC, "Bad command %d in worker pred Q\n", cmd->type);
                abort();
        }
    }

    // Mark done
    iap->n = 0;
}


// Set initial uniform job values & zero ctu_count
static void rpi_begin(const HEVCRpiContext * const s, HEVCRpiJob * const jb, const unsigned int ctu_ts_first)
{
    unsigned int i;
    HEVCRpiInterPredEnv *const cipe = &jb->chroma_ip;
    HEVCRpiInterPredEnv *const yipe = &jb->luma_ip;
    const HEVCRpiSPS * const sps = s->ps.sps;

    const uint16_t pic_width_y   = sps->width;
    const uint16_t pic_height_y  = sps->height;

    const uint16_t pic_width_c   = sps->width >> ctx_hshift(s, 1);
    const uint16_t pic_height_c  = sps->height >> ctx_vshift(s, 1);

    // We expect the pointer to change if we use another sps
    if (sps != jb->sps)
    {
        worker_pic_free_one(jb);

        set_ipe_from_ici(cipe, &ipe_init_infos[s->ps.sps->bit_depth - 8].chroma);
        set_ipe_from_ici(yipe, &ipe_init_infos[s->ps.sps->bit_depth - 8].luma);

        {
            const int coefs_per_luma = HEVC_MAX_CTB_SIZE * HEVC_RPI_MAX_WIDTH;
            const int coefs_per_chroma = (coefs_per_luma * 2) >> (ctx_vshift(s, 1) + ctx_hshift(s, 1));
            worker_pic_alloc_one(jb, coefs_per_luma + coefs_per_chroma);
        }

        jb->sps = sps;
    }

    jb->waited = 0;
    jb->ctu_ts_first = ctu_ts_first;
    jb->ctu_ts_last = -1;

    rpi_inter_pred_reset(cipe);
    for (i = 0; i < cipe->n; i++) {
        HEVCRpiInterPredQ * const cp = cipe->q + i;
        qpu_mc_pred_c_s_t * const u = &cp->qpu_mc_base->c.s;

        u->next_src1.x = 0;
        u->next_src1.y = 0;
        u->next_src1.base = 0;
        u->pic_cw = pic_width_c;
        u->pic_ch = pic_height_c;
        u->stride2 = av_rpi_sand_frame_stride2(s->frame);
        u->stride1 = av_rpi_sand_frame_stride1(s->frame);
        cp->last_l0 = &u->next_src1;

        u->next_fn = 0;
        u->next_src2.x = 0;
        u->next_src2.y = 0;
        u->next_src2.base = 0;
        cp->last_l1 = &u->next_src2;

        cp->qpu_mc_curr = (qpu_mc_pred_cmd_t *)(u + 1);
    }

    rpi_inter_pred_reset(yipe);
    for (i = 0; i < yipe->n; i++) {
        HEVCRpiInterPredQ * const yp = yipe->q + i;
        qpu_mc_pred_y_s_t * const y = &yp->qpu_mc_base->y.s;

        y->next_src1.x = 0;
        y->next_src1.y = 0;
        y->next_src1.base = 0;
        y->next_src2.x = 0;
        y->next_src2.y = 0;
        y->next_src2.base = 0;
        y->pic_h = pic_height_y;
        y->pic_w = pic_width_y;
        y->stride2 = av_rpi_sand_frame_stride2(s->frame);
        y->stride1 = av_rpi_sand_frame_stride1(s->frame);
        y->next_fn = 0;
        yp->last_l0 = &y->next_src1;
        yp->last_l1 = &y->next_src2;

        yp->qpu_mc_curr = (qpu_mc_pred_cmd_t *)(y + 1);
    }

    jb->last_y8_p = NULL;
    jb->last_y8_l1 = NULL;

    for (i = 0; i != FF_ARRAY_ELEMS(jb->progress_req); ++i) {
        jb->progress_req[i] = -1;
    }

    worker_pic_reset(&jb->coeffs);
}


#if !RPI_QPU_EMU_Y || !RPI_QPU_EMU_C
static unsigned int mc_terminate_add_qpu(const HEVCRpiContext * const s,
                                     const vpu_qpu_job_h vqj,
                                     rpi_cache_flush_env_t * const rfe,
                                     HEVCRpiInterPredEnv * const ipe)
{
    unsigned int i;
    uint32_t mail[QPU_N_MAX][QPU_MAIL_EL_VALS];
    unsigned int max_block = 0;

    if (!ipe->used) {
        return 0;
    }

    if (ipe->curr != 0) {
        rpi_inter_pred_sync(ipe);
    }

    // Add final commands to Q
    for(i = 0; i != ipe->n; ++i) {
        HEVCRpiInterPredQ * const yp = ipe->q + i;
        qpu_mc_src_t *const p0 = yp->last_l0;
        qpu_mc_src_t *const p1 = yp->last_l1;
        const unsigned int block_size = (char *)yp->qpu_mc_curr - (char *)yp->qpu_mc_base;

        if (block_size > max_block)
            max_block = block_size;

        qpu_mc_link_set(yp->qpu_mc_curr, yp->code_exit);

        // Need to set the srcs for L0 & L1 to something that can be (pointlessly) prefetched
        p0->x = MC_DUMMY_X;
        p0->y = MC_DUMMY_Y;
        p0->base = s->qpu_dummy_frame_qpu;
        p1->x = MC_DUMMY_X;
        p1->y = MC_DUMMY_Y;
        p1->base = s->qpu_dummy_frame_qpu;

        yp->last_l0 = NULL;
        yp->last_l1 = NULL;

        // Add to mailbox list
        mail[i][0] = ipe->gptr.vc + ((uint8_t *)yp->qpu_mc_base - ipe->gptr.arm);
        mail[i][1] = yp->code_setup;
    }

    // We don't need invalidate here as the uniforms aren't changed by the QPU
    // and leaving them in ARM cache avoids (pointless) pre-reads when writing
    // new values which seems to give us a small performance advantage
    //
    // In most cases we will not have a completely packed set of uniforms and as
    // we have a 2d invalidate we writeback all uniform Qs to the depth of the
    // fullest
    rpi_cache_flush_add_gm_blocks(rfe, &ipe->gptr, RPI_CACHE_FLUSH_MODE_WRITEBACK,
                                  (uint8_t *)ipe->q[0].qpu_mc_base - ipe->gptr.arm, max_block,
                                  ipe->n, ipe->max_fill + ipe->min_gap);
    vpu_qpu_job_add_qpu(vqj, ipe->n, (uint32_t *)mail);

    return 1;
}
#endif

#if RPI_QPU_EMU_Y || RPI_QPU_EMU_C
static unsigned int mc_terminate_add_emu(const HEVCRpiContext * const s,
                                     const vpu_qpu_job_h vqj,
                                     rpi_cache_flush_env_t * const rfe,
                                     HEVCRpiInterPredEnv * const ipe)
{
    unsigned int i;
    if (!ipe->used) {
        return 0;
    }

    if (ipe->curr != 0) {
        rpi_inter_pred_sync(ipe);
    }

    // Add final commands to Q
    for(i = 0; i != ipe->n; ++i) {
        HEVCRpiInterPredQ * const yp = ipe->q + i;
        qpu_mc_src_t *const p0 = yp->last_l0;
        qpu_mc_src_t *const p1 = yp->last_l1;

        yp->qpu_mc_curr->data[-1] = yp->code_exit;

        // Need to set the srcs for L0 & L1 to something that can be (pointlessly) prefetched
        p0->x = MC_DUMMY_X;
        p0->y = MC_DUMMY_Y;
        p0->base = s->qpu_dummy_frame_emu;
        p1->x = MC_DUMMY_X;
        p1->y = MC_DUMMY_Y;
        p1->base = s->qpu_dummy_frame_emu;

        yp->last_l0 = NULL;
        yp->last_l1 = NULL;
    }

    return 1;
}
#endif


#if RPI_QPU_EMU_Y
#define mc_terminate_add_y mc_terminate_add_emu
#else
#define mc_terminate_add_y mc_terminate_add_qpu
#endif
#if RPI_QPU_EMU_C
#define mc_terminate_add_c mc_terminate_add_emu
#else
#define mc_terminate_add_c mc_terminate_add_qpu
#endif


static void flush_frame(HEVCRpiContext *s,AVFrame *frame)
{
    rpi_cache_buf_t cbuf;
    rpi_cache_flush_env_t * rfe = rpi_cache_flush_init(&cbuf);
    rpi_cache_flush_add_frame(rfe, frame, RPI_CACHE_FLUSH_MODE_WB_INVALIDATE);
    rpi_cache_flush_finish(rfe);
}

static void job_gen_bounds(const HEVCRpiContext * const s, HEVCRpiJob * const jb)
{
    const unsigned int rs0 = s->ps.pps->ctb_addr_ts_to_rs[jb->ctu_ts_first];
    const unsigned int rs1 = s->ps.pps->ctb_addr_ts_to_rs[jb->ctu_ts_last];
    const unsigned int ctb_width = s->ps.sps->ctb_width;
    RpiBlk *const bounds = &jb->bounds;
    av_assert1(jb->ctu_ts_first <= jb->ctu_ts_last);
    bounds->x = (rs0 % ctb_width) << s->ps.sps->log2_ctb_size;
    bounds->y = (rs0 / ctb_width) << s->ps.sps->log2_ctb_size;
    bounds->w = ((rs1 - rs0) % ctb_width + 1) << s->ps.sps->log2_ctb_size;
    bounds->h = ((rs1 - rs0) / ctb_width + 1) << s->ps.sps->log2_ctb_size;

    bounds->w = FFMIN(bounds->w, s->ps.sps->width - bounds->x);
    bounds->h = FFMIN(bounds->h, s->ps.sps->height - bounds->y);
}

#if RPI_PASSES == 2
static void worker_core2(HEVCRpiContext * const s, HEVCRpiJob * const jb)
{
    // Perform intra prediction and residual reconstruction
    rpi_execute_pred_cmds(s, jb);

    // Perform deblocking for CTBs in this row
    rpi_execute_dblk_cmds(s, jb);
}
#endif

// Core execution tasks
static void worker_core(const HEVCRpiContext * const s, HEVCRpiJob * const jb)
{
    int pred_y, pred_c;
    vpu_qpu_job_env_t qvbuf;
    const vpu_qpu_job_h vqj = vpu_qpu_job_init(&qvbuf);
#if RPI_WORKER_WAIT_PASS_0
    int do_wait;
#endif

    {
        const HEVCRpiCoeffsEnv * const cf = &jb->coeffs;
        if (cf->s[3].n + cf->s[2].n != 0)
        {
            const unsigned int csize = sizeof(cf->s[3].buf[0]);
            const unsigned int offset32 = ((cf->s[3].buf - cf->s[2].buf) - cf->s[3].n) * csize;
            unsigned int n16 = (cf->s[2].n >> 8);
            unsigned int n32 = (cf->s[3].n >> 10);
#if RPI_COMPRESS_COEFFS
            if (cf->s[2].packed) {
                n16 = n16 | (n16<<16);
            } else {
                const unsigned int npack16 = (cf->s[2].packed_n>>8);
                n16 = n16 | (npack16<<16);
            }
            if (cf->s[3].packed) {
                n32 = n32 | (n32<<16);
            } else {
                const unsigned int npack32 = (cf->s[3].packed_n>>10);
                n32 = n32 | (npack32<<16);
            }
#endif
            vpu_qpu_job_add_vpu(vqj,
                vpu_get_fn(s->ps.sps->bit_depth),
                vpu_get_constants(),
                cf->gptr.vc,
                n16,
                cf->gptr.vc + offset32,
                n32,
                0);

            rpi_cache_flush_add_gm_range(jb->rfe, &cf->gptr, RPI_CACHE_FLUSH_MODE_WB_INVALIDATE, 0, cf->s[2].n * csize);
            rpi_cache_flush_add_gm_range(jb->rfe, &cf->gptr, RPI_CACHE_FLUSH_MODE_WB_INVALIDATE, offset32, cf->s[3].n * csize);
        }
    }

    pred_c = mc_terminate_add_c(s, vqj, jb->rfe, &jb->chroma_ip);

// We could take a sync here and try to locally overlap QPU processing with ARM
// but testing showed a slightly negative benefit with noticable extra complexity

    pred_y = mc_terminate_add_y(s, vqj, jb->rfe, &jb->luma_ip);

    // Returns 0 if nothing to do, 1 if sync added
#if RPI_WORKER_WAIT_PASS_0
    do_wait = vpu_qpu_job_add_sync_sem(vqj, &jb->sem);
#else
    if (vpu_qpu_job_add_sync_sem(vqj, &jb->sem) == 0)
        sem_post(&jb->sem);
#endif

    rpi_cache_flush_execute(jb->rfe);

    // Await progress as required
    // jb->waited will only be clear if we have already tested the progress values
    // (in worker_submit_job) and found we don't have to wait
    if (jb->waited)
    {
        unsigned int i;
        for (i = 0; i != FF_ARRAY_ELEMS(jb->progress_req); ++i) {
            if (jb->progress_req[i] >= 0) {
                ff_hevc_rpi_progress_wait_recon(s, jb, s->DPB + i, jb->progress_req[i]);
            }
        }
    }

    vpu_qpu_job_finish(vqj);

    // We always work on a rectangular block
    if (pred_y || pred_c)
    {
        rpi_cache_flush_add_frame_block(jb->rfe, s->frame, RPI_CACHE_FLUSH_MODE_INVALIDATE,
                                        jb->bounds.x, jb->bounds.y, jb->bounds.w, jb->bounds.h,
                                        ctx_vshift(s, 1), pred_y, pred_c);
    }

    // If we have emulated VPU ops - do it here
#if RPI_QPU_EMU_Y || RPI_QPU_EMU_C
    if (av_rpi_is_sand8_frame(s->frame))
    {
#if RPI_QPU_EMU_Y && RPI_QPU_EMU_C
        ff_hevc_rpi_shader_c8(s, &jb->luma_ip, &jb->chroma_ip);
#elif RPI_QPU_EMU_Y
        ff_hevc_rpi_shader_c8(s, &jb->luma_ip, NULL);
#else
        ff_hevc_rpi_shader_c8(s, NULL, &jb->chroma_ip);
#endif
    }
    else
    {
#if RPI_QPU_EMU_Y && RPI_QPU_EMU_C
        ff_hevc_rpi_shader_c16(s, &jb->luma_ip, &jb->chroma_ip);
#elif RPI_QPU_EMU_Y
        ff_hevc_rpi_shader_c16(s, &jb->luma_ip, NULL);
#else
        ff_hevc_rpi_shader_c16(s, NULL, &jb->chroma_ip);
#endif
    }
#endif

#if RPI_WORKER_WAIT_PASS_0
    if (do_wait)
        rpi_sem_wait(&jb->sem);
    rpi_cache_flush_execute(jb->rfe);
#endif
}


static void rpi_free_inter_pred(HEVCRpiInterPredEnv * const ipe)
{
    av_freep(&ipe->q);
    gpu_free(&ipe->gptr);
}

static HEVCRpiJob * job_new(void)
{
    HEVCRpiJob * const jb = av_mallocz(sizeof(HEVCRpiJob));

    if (jb == NULL)
        return NULL;

    sem_init(&jb->sem, 0, 0);
    jb->rfe = rpi_cache_flush_init(&jb->flush_buf);
    ff_hevc_rpi_progress_init_wait(&jb->progress_wait);

    jb->intra.n = 0;
    if ((jb->intra.cmds = av_mallocz(sizeof(HEVCPredCmd) * RPI_MAX_PRED_CMDS)) == NULL)
        goto fail1;

    // * Sizeof the union structure might be overkill but at the moment it
    //   is correct (it certainly isn't going to be too small)
    // Set max fill to slack/2 from the end of the Q
    // If we exceed this in any Q then we will schedule by size (which should
    // mean that we never use that Q again part from syncs)
    // * Given how agressive the overflow resonse is we could maybe put the
    //   threshold even nearer the end, but I don't expect us to ever hit
    //   it on any real stream anyway.

    if (rpi_inter_pred_alloc(&jb->chroma_ip,
                         QPU_N_MAX, QPU_N_GRP,
                         QPU_C_COMMANDS * sizeof(qpu_mc_pred_c_t) + QPU_C_SYNCS * sizeof(uint32_t),
                         QPU_C_CMD_SLACK_PER_Q * sizeof(qpu_mc_pred_c_t) / 2) != 0)
        goto fail2;
    if (rpi_inter_pred_alloc(&jb->luma_ip,
                         QPU_N_MAX,  QPU_N_GRP,
                         QPU_Y_COMMANDS * sizeof(qpu_mc_pred_y_t) + QPU_Y_SYNCS * sizeof(uint32_t),
                         QPU_Y_CMD_SLACK_PER_Q * sizeof(qpu_mc_pred_y_t) / 2) != 0)
        goto fail3;

    return jb;

fail3:
    rpi_free_inter_pred(&jb->luma_ip);
fail2:
    av_freep(&jb->intra.cmds);
fail1:
    ff_hevc_rpi_progress_kill_wait(&jb->progress_wait);
    rpi_cache_flush_finish(jb->rfe);
    sem_destroy(&jb->sem);
    return NULL;
}

static void job_delete(HEVCRpiJob * const jb)
{
    worker_pic_free_one(jb);
    ff_hevc_rpi_progress_kill_wait(&jb->progress_wait);
    rpi_free_inter_pred(&jb->chroma_ip);
    rpi_free_inter_pred(&jb->luma_ip);
    av_freep(&jb->intra.cmds);
    rpi_cache_flush_finish(jb->rfe);  // Not really needed - should do nothing
    sem_destroy(&jb->sem);
    av_free(jb);
}

static void jbg_delete(HEVCRpiJobGlobal * const jbg)
{
    HEVCRpiJob * jb;

    if (jbg == NULL)
        return;

    jb = jbg->free1;
    while (jb != NULL)
    {
        HEVCRpiJob * const jb2 = jb;
        jb = jb2->next;
        job_delete(jb2);
    }

    pthread_mutex_destroy(&jbg->lock);
    av_free(jbg);
}

static HEVCRpiJobGlobal * jbg_new(unsigned int job_count)
{
    HEVCRpiJobGlobal * const jbg = av_mallocz(sizeof(HEVCRpiJobGlobal));
    if (jbg == NULL)
        return NULL;

    pthread_mutex_init(&jbg->lock, NULL);

    while (job_count-- != 0)
    {
        HEVCRpiJob * const jb = job_new();
        if (jb == NULL)
            goto fail;

        jb->next = jbg->free1;
        jbg->free1 = jb;
    }

    return jbg;

fail:
    jbg_delete(jbg);
    return NULL;
}

static void rpi_job_ctl_delete(HEVCRpiJobCtl * const jbc)
{
    HEVCRpiJobGlobal * jbg;

    if (jbc == NULL)
        return;

    jbg = jbc->jbg;

    if (jbc->jb1 != NULL)
        job_delete(jbc->jb1);

    pthread_mutex_destroy(&jbc->in_lock);
    sem_destroy(&jbc->sem_out);
    av_free(jbc);

    // Deref the global job context
    if (jbg != NULL && atomic_fetch_add(&jbg->ref_count, -1) == 1)
        jbg_delete(jbg);
}

static HEVCRpiJobCtl * rpi_job_ctl_new(HEVCRpiJobGlobal *const jbg)
{
    HEVCRpiJobCtl * const jbc = av_mallocz(sizeof(HEVCRpiJobCtl));

    if (jbc == NULL)
        return NULL;

    jbc->jbg = jbg;
    atomic_fetch_add(&jbg->ref_count, 1);

    sem_init(&jbc->sem_out, 0, RPI_MAX_JOBS);
    pthread_mutex_init(&jbc->in_lock, NULL);

    if ((jbc->jb1 = job_new()) == NULL)
        goto fail;
    jbc->jb1->jbc_local = jbc;

    return jbc;

fail:
    rpi_job_ctl_delete(jbc);
    return NULL;
}



static av_cold void hevc_init_worker(HEVCRpiContext * const s)
{
#if RPI_PASSES == 2
    pass_queue_init(s->passq + 1, s, worker_core2, &s->jbc->sem_out, 1);
#elif RPI_PASSES == 3
    pass_queue_init(s->passq + 2, s, rpi_execute_dblk_cmds, &s->jbc->sem_out, 2);
    pass_queue_init(s->passq + 1, s, rpi_execute_pred_cmds, &s->passq[2].sem_in, 1);
#else
#error Passes confused
#endif
    pass_queue_init(s->passq + 0, s, worker_core, &s->passq[1].sem_in, 0);

    pass_queues_start_all(s);
}

static av_cold void hevc_exit_worker(HEVCRpiContext *s)
{
    pass_queues_term_all(s);

    pass_queues_kill_all(s);

    rpi_job_ctl_delete(s->jbc);
    s->jbc = NULL;
}


static int slice_start(const HEVCRpiContext * const s, HEVCRpiLocalContext *const lc)
{
    const int ctb_addr_ts = s->ps.pps->ctb_addr_rs_to_ts[s->sh.slice_segment_addr];
    const int tiles = s->ps.pps->num_tile_rows * s->ps.pps->num_tile_columns;
    const unsigned int tile_id = s->ps.pps->tile_id[ctb_addr_ts];

    // Check for obvious disasters
    if (ctb_addr_ts == 0 && s->sh.dependent_slice_segment_flag) {
        av_log(s->avctx, AV_LOG_ERROR, "Impossible initial tile.\n");
        return AVERROR_INVALIDDATA;
    }

    // If dependant then ctb_addr_ts != 0 from previous check
    if (s->sh.dependent_slice_segment_flag) {
        int prev_rs = s->ps.pps->ctb_addr_ts_to_rs[ctb_addr_ts - 1];
        if (s->tab_slice_address[prev_rs] != s->sh.slice_addr) {
            av_log(s->avctx, AV_LOG_ERROR, "Previous slice segment missing\n");
            return AVERROR_INVALIDDATA;
        }
    }

    if (!s->ps.pps->entropy_coding_sync_enabled_flag &&
        tile_id + s->sh.num_entry_point_offsets >= tiles)
    {
        av_log(s->avctx, AV_LOG_ERROR, "Entry points exceed tiles\n");
        return AVERROR_INVALIDDATA;
    }

    // Tiled stuff must start at start of tile if it has multiple entry points
    if (!s->ps.pps->entropy_coding_sync_enabled_flag &&
        s->sh.num_entry_point_offsets != 0 &&
        ctb_addr_ts != s->ps.pps->tile_pos_ts[tile_id])
    {
        av_log(s->avctx, AV_LOG_ERROR, "Multiple tiles in slice; slice start != tile start\n");
        return AVERROR_INVALIDDATA;
    }

    ff_hevc_rpi_cabac_init_decoder(lc);

    // Setup any required decode vars
    lc->cabac_init_req = !s->sh.dependent_slice_segment_flag;

//    printf("SS: req=%d, sol=%d, sot=%d\n", lc->cabac_init_req, sol, sot);
    lc->qp_y = s->sh.slice_qp;

    // General setup
    lc->bt_line_no = 0;
    lc->ts = ctb_addr_ts;
    return 0;
}

static int gen_entry_points(HEVCRpiContext * const s, const H2645NAL * const nal)
{
    const GetBitContext * const gb = &s->HEVClc->gb;
    RpiSliceHeader * const sh = &s->sh;
    int i, j;

    const unsigned int length = nal->size;
    unsigned int offset = ((gb->index) >> 3) + 1;  // We have a bit & align still to come = +1 byte
    unsigned int cmpt;
    unsigned int startheader;

    if (sh->num_entry_point_offsets == 0) {
        s->data = NULL;
        return 0;
    }

    // offset in slice header includes emulation prevention bytes.
    // Unfortunately those have been removed by the time we get here so we
    // have to compensate.  The nal layer keeps a track of where they were.
    for (j = 0, cmpt = 0, startheader = offset + sh->entry_point_offset[0]; j < nal->skipped_bytes; j++) {
        if (nal->skipped_bytes_pos[j] >= offset && nal->skipped_bytes_pos[j] < startheader) {
            startheader--;
            cmpt++;
        }
    }

    for (i = 1; i < sh->num_entry_point_offsets; i++) {
        offset += (sh->entry_point_offset[i - 1] - cmpt);
        for (j = 0, cmpt = 0, startheader = offset + sh->entry_point_offset[i]; j < nal->skipped_bytes; j++) {
            if (nal->skipped_bytes_pos[j] >= offset && nal->skipped_bytes_pos[j] < startheader) {
                startheader--;
                cmpt++;
            }
        }
        if (sh->entry_point_offset[i] <= cmpt) {
            av_log(s->avctx, AV_LOG_ERROR, "entry point offset <= skipped bytes\n");
            return AVERROR_INVALIDDATA;
        }
        sh->size[i - 1] = sh->entry_point_offset[i] - cmpt;
        sh->offset[i - 1] = offset;
    }

    offset += sh->entry_point_offset[sh->num_entry_point_offsets - 1] - cmpt;
    if (length < offset) {
        av_log(s->avctx, AV_LOG_ERROR, "entry_point_offset table is corrupted\n");
        return AVERROR_INVALIDDATA;
    }
    sh->size[sh->num_entry_point_offsets - 1] = length - offset;
    sh->offset[sh->num_entry_point_offsets - 1] = offset;

    // Remember data start pointer as we won't have nal later
    s->data = nal->data;
    return 0;
}


// Return
// < 0   Error
// 0     OK
//
// jb->ctu_ts_last < 0       Job still filling
// jb->ctu_ts_last >= 0      Job ready

static int fill_job(HEVCRpiContext * const s, HEVCRpiLocalContext *const lc, unsigned int max_blocks)
{
    const unsigned int log2_ctb_size = s->ps.sps->log2_ctb_size;
    const unsigned int ctb_size = (1 << log2_ctb_size);
    HEVCRpiJob * const jb = lc->jb0;
    int more_data = 1;
    unsigned int ctb_addr_ts = lc->ts;
    unsigned int ctb_addr_rs = s->ps.pps->ctb_addr_ts_to_rs[ctb_addr_ts];
    unsigned int x_ctb = (ctb_addr_rs % s->ps.sps->ctb_width) << log2_ctb_size;
    const unsigned int y_ctb = (ctb_addr_rs / s->ps.sps->ctb_width) << log2_ctb_size;

    lc->unit_done = 0;

    while (more_data && ctb_addr_ts < s->ps.sps->ctb_size)
    {
        int q_full;
        const unsigned int ctb_flags = s->ps.pps->ctb_ts_flags[ctb_addr_ts];

        hls_decode_neighbour(s, lc, x_ctb, y_ctb, ctb_addr_ts);

        ff_hevc_rpi_cabac_init(s, lc, ctb_flags);

        hls_sao_param(s, lc, x_ctb >> log2_ctb_size, y_ctb >> log2_ctb_size);

        s->deblock[ctb_addr_rs].beta_offset = s->sh.beta_offset;
        s->deblock[ctb_addr_rs].tc_offset   = s->sh.tc_offset;
        s->filter_slice_edges[ctb_addr_rs]  = s->sh.slice_loop_filter_across_slices_enabled_flag;

        // Zap stashes if navail
        if ((lc->ctb_avail & AVAIL_U) == 0)
            zap_cabac_stash(s->cabac_stash_up + (x_ctb >> 3), log2_ctb_size - 3);
        if ((lc->ctb_avail & AVAIL_L) == 0)
        {
            memset(lc->ipm_left, INTRA_DC, IPM_TAB_SIZE);
            zap_cabac_stash(s->cabac_stash_left + (y_ctb >> 3), log2_ctb_size - 3);
        }
#if MVF_STASH_WIDTH > 64
        // Restore left mvf stash at start of tile if not at start of line
        if ((ctb_flags & CTB_TS_FLAGS_SOTL) != 0 && x_ctb != 0 && !s->is_irap)
        {
            unsigned int i;
            HEVCRpiMvField * dst = mvf_stash_ptr(s, lc, x_ctb - 1, 0);
            const HEVCRpiMvField * src = s->mvf_left + (y_ctb >> LOG2_MIN_PU_SIZE);
            for (i = 0; i != ctb_size >> LOG2_MIN_PU_SIZE; ++i)
            {
                *dst = *src++;
                dst += MVF_STASH_WIDTH_PU;
            }
        }
#endif

        // Set initial tu states
        lc->tu.cu_qp_delta = 0;
        lc->tu.is_cu_qp_delta_wanted = 0;
        lc->tu.cu_chroma_qp_offset_wanted = 0;

        // Decode
        more_data = hls_coding_quadtree(s, lc, x_ctb, y_ctb, log2_ctb_size, 0);

        if (ff_hevc_rpi_cabac_overflow(lc))
        {
            av_log(s->avctx, AV_LOG_ERROR, "Quadtree bitstream overread\n ");
            more_data = AVERROR_INVALIDDATA;
        }

        if (more_data < 0) {
            s->tab_slice_address[ctb_addr_rs] = TAB_SLICE_ADDR_BROKEN;  // Mark slice as broken
            return more_data;
        }

        if (more_data && ((ctb_flags & CTB_TS_FLAGS_EOT) != 0 ||
             (s->ps.pps->entropy_coding_sync_enabled_flag && (ctb_flags & CTB_TS_FLAGS_EOTL) != 0)))
        {
            if (ff_hevc_rpi_get_cabac_terminate(&lc->cc) < 0 ||
                ff_hevc_rpi_cabac_skip_bytes(&lc->cc, 0) == NULL)
            {
                av_log(s->avctx, AV_LOG_ERROR, "Error reading terminate el\n ");
                return -1;
            }
        }

        // --- Post CTB processing

        // Stash rpl top/left for deblock that needs to remember such things cross-slice
        s->rpl_up[x_ctb >> log2_ctb_size] = s->refPicList;
        s->rpl_left[y_ctb >> log2_ctb_size] = s->refPicList;

        if (!s->is_irap)
        {
            // Copy MVF up to up-left & stash to up
            {
                const HEVCRpiMvField * src = mvf_stash_ptr(s, lc, x_ctb, ctb_size - 1);
                HEVCRpiMvField * dst = s->mvf_up + (x_ctb >> LOG2_MIN_PU_SIZE);

    //            printf("Stash: %d,%d, ctb_size=%d, %p->%p\n", x_ctb, y_ctb, ctb_size, src, dst);

                lc->mvf_ul[0] = dst[(ctb_size - 1) >> LOG2_MIN_PU_SIZE];
                memcpy(dst, src, (sizeof(*src)*ctb_size) >> LOG2_MIN_PU_SIZE);
            }
            // Stash sideways if end of tile line but not end of line (no point)
            // ** Could/should do this @ end of fn
#if MVF_STASH_WIDTH > 64
            if ((ctb_flags & (CTB_TS_FLAGS_EOTL | CTB_TS_FLAGS_EOL)) == CTB_TS_FLAGS_EOTL)
#endif
            {
                unsigned int i;
                const HEVCRpiMvField * src = mvf_stash_ptr(s, lc, x_ctb + ctb_size - 1, 0);
                HEVCRpiMvField * dst = s->mvf_left + (y_ctb >> LOG2_MIN_PU_SIZE);
                for (i = 0; i != ctb_size >> LOG2_MIN_PU_SIZE; ++i)
                {
                    *dst++ = *src;
                    src += MVF_STASH_WIDTH_PU;
                }
            }
        }

        if ((ctb_flags & CTB_TS_FLAGS_CSAVE) != 0)
            ff_hevc_rpi_save_states(s, lc);

        // Report progress so we can use our MVs in other frames
        if ((ctb_flags & CTB_TS_FLAGS_EOL) != 0)
            ff_hevc_rpi_progress_signal_mv(s, y_ctb + ctb_size - 1);

        // End of line || End of tile line || End of tile
        // (EoL covers end of frame for our purposes here)
        q_full = ((ctb_flags & CTB_TS_FLAGS_EOTL) != 0);

        // Allocate QPU chunks on fixed size 64 pel boundries rather than
        // whatever ctb_size is today.
        // * We might quite like to continue to 64 pel vertical too but that
        //   currently confuses WPP
        if (((x_ctb + ctb_size) & 63) == 0 || q_full)
        {
            int overflow = 0;
            if (rpi_inter_pred_next_ctu(&jb->luma_ip) != 0)
                overflow = 1;
            if (rpi_inter_pred_next_ctu(&jb->chroma_ip) != 0)
                overflow = 1;
            if (overflow)
            {
                // * This is very annoying (and slow) to cope with in WPP so
                //   we treat it as an error there (no known stream triggers this
                //   with the current buffer sizes).  Non-wpp should cope fine.
                av_log(s->avctx, AV_LOG_WARNING,  "%s: Q full before EoL\n", __func__);
                q_full = 1;
            }
        }

        // Inc TS to next.
        ctb_addr_ts++;
        ctb_addr_rs++;
        x_ctb += ctb_size;

        if (q_full)
        {
            // Do job
            // Prep for submission
            jb->ctu_ts_last = ctb_addr_ts - 1;  // Was pre-inced
            job_gen_bounds(s, jb);
            break;
        }

        // If max_blocks started as 0 then this will never be true
        if (--max_blocks == 0)
            break;
    }

    lc->unit_done = (more_data <= 0);
    lc->ts = ctb_addr_ts;
    return 0;
}

static void bt_lc_init(HEVCRpiContext * const s, HEVCRpiLocalContext * const lc, const unsigned int n)
{
    lc->context = s;
    lc->jb0 = NULL;
    lc->lc_n = n;
    lc->bt_terminate = 0;
    lc->bt_psem_out = NULL;
    sem_init(&lc->bt_sem_in, 0, 0);
}

#define TRACE_WPP 0
#if RPI_EXTRA_BIT_THREADS > 0
static inline unsigned int line_ts_width(const HEVCRpiContext * const s, unsigned int ts)
{
    unsigned int rs = s->ps.pps->ctb_addr_ts_to_rs[ts];
    return s->ps.pps->column_width[s->ps.pps->col_idxX[rs % s->ps.sps->ctb_width]];
}

// Move local context parameters from an aux bit thread back to the main
// thread at the end of a slice as processing is going to continue there.
static void movlc(HEVCRpiLocalContext *const dst_lc, HEVCRpiLocalContext *const src_lc, const int is_dep)
{
    if (src_lc == dst_lc) {
        return;
    }

    // Move the job
    // We will still have an active job if the final line terminates early
    // Dest should always be null by now
    av_assert1(dst_lc->jb0 == NULL);
    dst_lc->jb0 = src_lc->jb0;
    src_lc->jb0 = NULL;

    // Always need to store where we are in the bitstream
    dst_lc->ts = src_lc->ts;
    dst_lc->gb = src_lc->gb;
    // Cabac init request will be built at start of next slice

    // Need to store context if we might have a dependent seg
    if (is_dep)
    {
        dst_lc->qPy_pred = src_lc->qPy_pred;
        memcpy(dst_lc->ipm_left, src_lc->ipm_left, sizeof(src_lc->ipm_left));
        memcpy(dst_lc->cabac_state, src_lc->cabac_state, sizeof(src_lc->cabac_state));
        memcpy(dst_lc->stat_coeff, src_lc->stat_coeff, sizeof(src_lc->stat_coeff));
    }
}

static inline int wait_bt_sem_in(HEVCRpiLocalContext * const lc)
{
    rpi_sem_wait(&lc->bt_sem_in);
    return lc->bt_terminate;
}

// Do one WPP line
// Will not work correctly over horizontal tile boundries - vertical should be OK
static int rpi_run_one_line(HEVCRpiContext *const s, HEVCRpiLocalContext * const lc, const int is_first)
{
    const int is_tile = lc->bt_is_tile;
    const unsigned int tile_id = s->ps.pps->tile_id[lc->ts];
    const unsigned int line = lc->bt_line_no;
    const unsigned int line_inc = lc->bt_line_inc;
    const int is_last = (line >= lc->bt_last_line);

    const unsigned int ts_eol = lc->ts + (is_tile ? s->ps.pps->tile_size[tile_id] : lc->bt_line_width);
    const unsigned int ts_next =
        line + line_inc > (unsigned int)s->sh.num_entry_point_offsets ?
            INT_MAX :
        is_tile ?
            s->ps.pps->tile_pos_ts[tile_id + line_inc] :
            lc->ts + lc->bt_line_width * line_inc;
    // Tile wants line, WPP a few CTUs (must be >= 2 for cabac context to work)
    const unsigned int partial_size = is_tile ? line_ts_width(s, lc->ts) : 2;
    unsigned int ts_prev;
    int loop_n = 0;
    int err = 0;

    av_assert1(line <= s->sh.num_entry_point_offsets);

#if TRACE_WPP
    printf("%s[%d]: Start %s: tile=%d, line=%d/%d/%d, ts=%d/%d/%d, width=%d, jb=%p\n", __func__,
           lc->lc_n,  is_tile ? "Tile" : "WPP", tile_id,
           line, lc->bt_last_line, s->sh.num_entry_point_offsets,
           lc->ts, ts_eol, ts_next, partial_size, lc->jb0);
#endif
    if (line != 0)
    {
        const uint8_t * const data = s->data + s->sh.offset[line - 1];
        const unsigned int len = s->sh.size[line - 1];
        if ((err = init_get_bits8(&lc->gb, data, len)) < 0)
            return err;

        ff_init_cabac_decoder(&lc->cc, data, len);
    }

    // We should never be processing a dependent slice here so reset is good
    // ?? These probably shouldn't be needed (as they should be set by later
    //    logic) but do seem to be required
    lc->qp_y = s->sh.slice_qp;

    do
    {
        if (!is_last && loop_n > 1) {
#if TRACE_WPP
            printf("%s[%d]: %sPoke %p\n", __func__, lc->lc_n, err == 0 ? "" : "ERR: ", lc->bt_psem_out);
#endif
            sem_post(lc->bt_psem_out);
        }
        // The wait for loop_n == 0 has been done in bit_thread
        if (!is_first && loop_n != 0)
        {
#if TRACE_WPP
            printf("%s[%d]: %sWait %p\n", __func__, lc->lc_n, err == 0 ? "" : "ERR: ", &lc->bt_sem_in);
#endif
            if (wait_bt_sem_in(lc) != 0)
                return AVERROR_EXIT;
        }

#if TRACE_WPP
        {
            int n;
            sem_getvalue(&lc->bt_sem_in, &n);
            printf("%s[%d]: ts=%d, sem=%d %p\n", __func__, lc->lc_n, lc->ts, n, &lc->bt_sem_in);
        }
#endif

        ts_prev = lc->ts;

        // If we have had an error - do no further decode but do continue
        // moving signals around so the other threads continue to operate
        // correctly (or at least as correctly as they can with this line missing)
        //
        // Errors in WPP/Tile are less fatal than normal as we have a good idea
        // of how to restart on the next line so there is no need to give up totally
        if (err != 0)
        {
            lc->unit_done = 0;
            lc->ts += partial_size;
        }
        else
        {
            worker_pass0_ready(s, lc);

            if ((err = fill_job(s, lc, partial_size)) < 0 ||
                (lc->ts < ts_eol && !is_last && (lc->ts != ts_prev + partial_size || lc->unit_done)))
            {
                if (err == 0) {
                    av_log(s->avctx, AV_LOG_ERROR, "Unexpected end of tile/wpp section\n");
                    err = AVERROR_INVALIDDATA;
                }
                worker_free(s, lc);
                lc->ts = ts_prev + partial_size;  // Pretend we did all that
                lc->unit_done = 0;
            }
            else if (is_tile)
            {
                worker_submit_job(s, lc);
            }
        }

        ++loop_n;
    } while (lc->ts < ts_eol && !lc->unit_done);

    // If we are on the last line & we didn't get a whole line we must wait for
    // and sink the sem_posts from the line above / tile to the left.
    while ((ts_prev += partial_size) < ts_eol)
    {
#if TRACE_WPP
        printf("%s[%d]: EOL Wait: ts=%d %p\n", __func__, lc->lc_n, ts_prev, &lc->bt_sem_in);
#endif
        if (wait_bt_sem_in(lc) != 0)
            return AVERROR_EXIT;
    }

    lc->bt_line_no += line_inc;

    if (!is_tile && err == 0)
        worker_submit_job(s, lc);

    if (!is_last) {
        lc->ts = ts_next;

#if TRACE_WPP
        printf("%s[%d]: Poke post submit %p\n", __func__, lc->lc_n, lc->bt_psem_out);
#endif
        sem_post(lc->bt_psem_out);
        if (loop_n > 1) {
#if TRACE_WPP
            printf("%s[%d]: Poke post submit2 %p\n", __func__, lc->lc_n, lc->bt_psem_out);
#endif
            sem_post(lc->bt_psem_out);
        }
    }
    else
    {
        movlc(s->HEVClcList[0], lc, s->ps.pps->dependent_slice_segments_enabled_flag);  // * & not EoT
#if MVF_STASH_WIDTH > 64
        // Horrid calculations to work out what we want but luckily this should almost never execute
        // **** Move to movlc
        if (!s->is_irap)
        {
            const unsigned int ctb_flags = s->ps.pps->ctb_ts_flags[lc->ts];
            if ((ctb_flags & CTB_TS_FLAGS_EOTL) == 0) // If EOTL then we have already stashed mvf
            {
                const unsigned int x_ctb = ((s->ps.pps->ctb_addr_ts_to_rs[lc->ts] % s->ps.sps->ctb_width) << s->ps.sps->log2_ctb_size) - 1;
                unsigned int i;
                const HEVCRpiMvField *s_mvf = lc->mvf_stash + ((x_ctb >> LOG2_MIN_PU_SIZE) & (MVF_STASH_WIDTH_PU - 1));
                HEVCRpiMvField *d_mvf = s->HEVClcList[0]->mvf_stash + ((x_ctb >> LOG2_MIN_PU_SIZE) & (MVF_STASH_WIDTH_PU - 1));

                for (i = 0; i != MVF_STASH_HEIGHT_PU; ++i)
                {
                    *d_mvf = *s_mvf;
                    d_mvf += MVF_STASH_WIDTH_PU;
                    s_mvf += MVF_STASH_WIDTH_PU;
                }

            }
        }
#endif
        // When all done poke the thread 0 sem_in one final time
#if TRACE_WPP
        printf("%s[%d]: Poke final %p\n", __func__, lc->lc_n, &s->HEVClcList[0]->bt_sem_in);
#endif
        sem_post(&s->HEVClcList[0]->bt_sem_in);
    }

#if TRACE_WPP
    printf("%s[%d]: End. dep=%d\n", __func__, lc->lc_n, s->ps.pps->dependent_slice_segments_enabled_flag);
#endif
    return err;
}

static void wpp_setup_lcs(HEVCRpiContext * const s)
{
    unsigned int ts = s->ps.pps->ctb_addr_rs_to_ts[s->sh.slice_segment_addr];
    const unsigned int line_width = line_ts_width(s, ts);

    for (int i = 0; i <= s->sh.num_entry_point_offsets && i < RPI_BIT_THREADS; ++i)
    {
        HEVCRpiLocalContext * const lc = s->HEVClcList[i];
        lc->ts = ts;
        lc->bt_is_tile = 0;
        lc->bt_line_no = i;
        lc->bt_line_width = line_width;
        lc->bt_last_line = s->sh.num_entry_point_offsets;
        lc->bt_line_inc = RPI_BIT_THREADS;
        ts += line_width;
    }
}


// Can only process tile single row at once
static void tile_one_row_setup_lcs(HEVCRpiContext * const s, unsigned int slice_row)
{
    const HEVCRpiPPS * const pps = s->ps.pps;
    const unsigned int ts0 = pps->ctb_addr_rs_to_ts[s->sh.slice_segment_addr];
    const unsigned int tile0 = pps->tile_id[ts0];
    const unsigned int col0 = tile0 % pps->num_tile_columns;

    const unsigned int col = (slice_row == 0) ? col0 : 0;
    unsigned int line = slice_row * pps->num_tile_columns - col0 + col;
    const unsigned int last_line = FFMIN(
        line + pps->num_tile_columns - 1 - col, s->sh.num_entry_point_offsets);

    const unsigned int par =
        FFMIN(RPI_BIT_THREADS, last_line + 1 - line);
#if TRACE_WPP
    printf("ts0=%d, ents=%d, row=%d, tiles=%dx%d, col=%d, par=%d, line=%d/%d\n", ts0, s->sh.num_entry_point_offsets, slice_row,
           pps->num_tile_columns, pps->num_tile_rows, col, par, line, last_line);
#endif
    for (unsigned int i = 0; i != par; ++i, ++line)
    {
        HEVCRpiLocalContext * const lc = s->HEVClcList[i];
        const unsigned int tile = tile0 + line;

        lc->ts = pps->tile_pos_ts[tile];
        lc->bt_line_no = line;
        lc->bt_is_tile = 1;
        lc->bt_line_width = line_ts_width(s, lc->ts);
        lc->bt_last_line = last_line;
        lc->bt_line_inc = par;
    }
}


static void * bit_thread(void * v)
{
    HEVCRpiLocalContext * const lc = v;
    HEVCRpiContext *const s = lc->context;

    while (wait_bt_sem_in(lc) == 0)
    {
        int err;

        if ((err = rpi_run_one_line(s, lc, 0)) < 0) {  // Never first tile/wpp
            if (lc->bt_terminate) {
                av_log(s->avctx, AV_LOG_ERROR, "%s: Unexpected termination\n", __func__);
                break;
            }
            av_log(s->avctx, AV_LOG_WARNING, "%s: Decode failure: %d\n", __func__, err);
        }
    }

    return NULL;
}

static int bit_threads_start(HEVCRpiContext * const s)
{
    if (s->bt_started)
        return 0;

    for (int i = 1; i < RPI_BIT_THREADS; ++i)
    {
        // lc[0] belongs to the main thread - this sets up lc[1..RPI_BIT_THREADS]
        if (s->HEVClcList[i] == NULL) {
            if ((s->HEVClcList[i] = av_mallocz(sizeof(*s->HEVClcList[0]))) == NULL)
                return -1;
        }

        bt_lc_init(s, s->HEVClcList[i], i);
        job_lc_init(s->HEVClcList[i]);
    }

    // Link the sems in a circle
    for (int i = 0; i < RPI_BIT_THREADS - 1; ++i)
        s->HEVClcList[i]->bt_psem_out = &s->HEVClcList[i + 1]->bt_sem_in;
    s->HEVClcList[RPI_BIT_THREADS - 1]->bt_psem_out = &s->HEVClcList[0]->bt_sem_in;

    // Init all lc before starting any threads
    for (int i = 0; i < RPI_EXTRA_BIT_THREADS; ++i)
    {
        if (pthread_create(s->bit_threads + i, NULL, bit_thread, s->HEVClcList[i + 1]) < 0)
            return -1;
    }

    s->bt_started = 1;
    return 0;
}

static int bit_threads_kill(HEVCRpiContext * const s)
{
    if (!s->bt_started)
        return 0;
    s->bt_started = 0;

    for (int i = 0; i < RPI_EXTRA_BIT_THREADS; ++i)
    {
        HEVCRpiLocalContext *const lc = s->HEVClcList[i + 1];
        if (lc == NULL)
            break;

        lc->bt_terminate = 1;
        sem_post(&lc->bt_sem_in);
        pthread_join(s->bit_threads[i], NULL);

        sem_destroy(&lc->bt_sem_in);
        job_lc_kill(lc);
    }
    return 0;
}
#endif


// If we are at EoT and the row is shorter than the number of jobs
// we can Q we have to wait for it finish otherwise we risk cache/QPU
// disasters
static inline int tile_needs_wait(const HEVCRpiContext * const s, const int n)
{
    return
        s->ps.pps->tile_wpp_inter_disable >= 2 &&
        s->sh.slice_type != HEVC_SLICE_I &&
        n >= 0 &&
        (s->ps.pps->ctb_ts_flags[n] & (CTB_TS_FLAGS_EOT | CTB_TS_FLAGS_EOL)) == CTB_TS_FLAGS_EOT;
}

static int rpi_decode_entry(AVCodecContext *avctxt, void *isFilterThread)
{
    HEVCRpiContext * const s  = avctxt->priv_data;
    HEVCRpiLocalContext * const lc = s->HEVClc;
    int err;

    // Start of slice
    if ((err = slice_start(s, lc)) != 0)
        return err;

#if RPI_EXTRA_BIT_THREADS > 0

    if (s->sh.offload_tiles)
    {
        unsigned int slice_row = 0;

#if TRACE_WPP
        printf("%s: Do Tiles\n", __func__);
#endif
        // Generate & start extra bit threads if they aren't already running
        bit_threads_start(s);

        do
        {
            // Reset lc lines etc.
            tile_one_row_setup_lcs(s, slice_row);

#if TRACE_WPP
            printf("%s: Row %d: Do 1st: line=%d/%d/%d\n",
                   __func__, slice_row, lc->bt_line_no, lc->bt_last_line, s->sh.num_entry_point_offsets);
#endif

            rpi_run_one_line(s, lc, 1);  // Kicks off the other threads
#if TRACE_WPP
            printf("%s: Row %d: Done 1st: line=%d/%d/%d\n",
                   __func__, slice_row, lc->bt_line_no, lc->bt_last_line, s->sh.num_entry_point_offsets);
#endif

            while (lc->bt_line_no <= lc->bt_last_line) {
                rpi_sem_wait(&lc->bt_sem_in);
                rpi_run_one_line(s, lc, 0);
            }
#if TRACE_WPP
            printf("%s: Done body\n", __func__);
#endif

            // Wait for everything else to finish
            rpi_sem_wait(&lc->bt_sem_in);

            ++slice_row;
        } while (lc->bt_last_line < s->sh.num_entry_point_offsets);


#if TRACE_WPP
        printf("%s: Done wait: ts=%d\n", __func__, lc->ts);
#endif
    }
    else if (s->sh.offload_wpp)
    {
#if TRACE_WPP
        printf("%s: Do WPP\n", __func__);
#endif
        // Generate & start extra bit threads if they aren't already running
        bit_threads_start(s);

        // Reset lc lines etc.
        wpp_setup_lcs(s);

        rpi_run_one_line(s, lc, 1);  // Kicks off the other threads
#if TRACE_WPP
        printf("%s: Done 1st\n", __func__);
#endif

        while (lc->bt_line_no <= s->sh.num_entry_point_offsets) {
            rpi_sem_wait(&lc->bt_sem_in);
            rpi_run_one_line(s, lc, 0);
        }
#if TRACE_WPP
        printf("%s: Done body\n", __func__);
#endif

        // Wait for everything else to finish
        rpi_sem_wait(&lc->bt_sem_in);

#if TRACE_WPP
        printf("%s: Done wait: ts=%d\n", __func__, lc->ts);
#endif
    }
    else
#endif
    {
#if TRACE_WPP
        printf("%s: Single start: ts=%d\n", __func__, lc->ts);
#endif
        // Single bit thread
        do {
            // Make sure we have space to prepare the next job
            worker_pass0_ready(s, lc);

            if ((err = fill_job(s, lc, 0)) < 0)
                goto fail;

            worker_submit_job(s, lc);

            if (tile_needs_wait(s, lc->ts - 1))
                worker_wait(s, lc);

        } while (!lc->unit_done);

#if TRACE_WPP
        printf("%s: Single end: ts=%d\n", __func__, lc->ts);
#endif
    }

    // If we have reached the end of the frame or
    // then wait for the worker to finish all its jobs
    if (lc->ts >= s->ps.sps->ctb_size)
        worker_wait(s, lc);

#if RPI_TSTATS
    {
        HEVCRpiStats *const ts = &s->tstats;

        printf("=== P: xy00:%5d/%5d/%5d/%5d h16gl:%5d/%5d w8gl:%5d/%5d y8m:%d\n    B: xy00:%5d/%5d/%5d/%5d h16gl:%5d/%5d\n",
               ts->y_pred1_xy, ts->y_pred1_x0, ts->y_pred1_y0, ts->y_pred1_x0y0,
               ts->y_pred1_hgt16, ts->y_pred1_hle16, ts->y_pred1_wgt8, ts->y_pred1_wle8, ts->y_pred1_y8_merge,
               ts->y_pred2_xy, ts->y_pred2_x0, ts->y_pred2_y0, ts->y_pred2_x0y0,
               ts->y_pred2_hgt16, ts->y_pred2_hle16);
        memset(ts, 0, sizeof(*ts));
    }
#endif

    return lc->ts;

fail:
    // Cleanup
    av_log(s->avctx, AV_LOG_ERROR, "%s failed: err=%d\n", __func__, err);
    // Free our job & wait for temination
    worker_free(s, lc);
    worker_wait(s, lc);
    return err;
}


static void set_no_backward_pred(HEVCRpiContext * const s)
{
    int i, j;
    const RefPicList *const refPicList = s->refPicList;

    s->no_backward_pred_flag = 0;
    if (s->sh.slice_type != HEVC_SLICE_B || !s->sh.slice_temporal_mvp_enabled_flag)
        return;

    for (j = 0; j < 2; j++) {
        for (i = 0; i < refPicList[j].nb_refs; i++) {
            if (refPicList[j].list[i] > s->poc) {
                s->no_backward_pred_flag = 1;
                return;
            }
        }
    }
}

static int hls_slice_data(HEVCRpiContext * const s, const H2645NAL * const nal)
{
    int err;
    if ((err = gen_entry_points(s, nal)) < 0)
        return err;

    set_no_backward_pred(s);

    return rpi_decode_entry(s->avctx, NULL);
}

static int set_side_data(HEVCRpiContext *s)
{
    AVFrame *out = s->ref->frame;

    if (s->sei.frame_packing.present &&
        s->sei.frame_packing.arrangement_type >= 3 &&
        s->sei.frame_packing.arrangement_type <= 5 &&
        s->sei.frame_packing.content_interpretation_type > 0 &&
        s->sei.frame_packing.content_interpretation_type < 3) {
        AVStereo3D *stereo = av_stereo3d_create_side_data(out);
        if (!stereo)
            return AVERROR(ENOMEM);

        switch (s->sei.frame_packing.arrangement_type) {
        case 3:
            if (s->sei.frame_packing.quincunx_subsampling)
                stereo->type = AV_STEREO3D_SIDEBYSIDE_QUINCUNX;
            else
                stereo->type = AV_STEREO3D_SIDEBYSIDE;
            break;
        case 4:
            stereo->type = AV_STEREO3D_TOPBOTTOM;
            break;
        case 5:
            stereo->type = AV_STEREO3D_FRAMESEQUENCE;
            break;
        }

        if (s->sei.frame_packing.content_interpretation_type == 2)
            stereo->flags = AV_STEREO3D_FLAG_INVERT;

        if (s->sei.frame_packing.arrangement_type == 5) {
            if (s->sei.frame_packing.current_frame_is_frame0_flag)
                stereo->view = AV_STEREO3D_VIEW_LEFT;
            else
                stereo->view = AV_STEREO3D_VIEW_RIGHT;
        }
    }

    if (s->sei.display_orientation.present &&
        (s->sei.display_orientation.anticlockwise_rotation ||
         s->sei.display_orientation.hflip || s->sei.display_orientation.vflip)) {
        double angle = s->sei.display_orientation.anticlockwise_rotation * 360 / (double) (1 << 16);
        AVFrameSideData *rotation = av_frame_new_side_data(out,
                                                           AV_FRAME_DATA_DISPLAYMATRIX,
                                                           sizeof(int32_t) * 9);
        if (!rotation)
            return AVERROR(ENOMEM);

        av_display_rotation_set((int32_t *)rotation->data, angle);
        av_display_matrix_flip((int32_t *)rotation->data,
                               s->sei.display_orientation.hflip,
                               s->sei.display_orientation.vflip);
    }

    // Decrement the mastering display flag when IRAP frame has no_rasl_output_flag=1
    // so the side data persists for the entire coded video sequence.
    if (s->sei.mastering_display.present > 0 &&
        IS_IRAP(s) && s->no_rasl_output_flag) {
        s->sei.mastering_display.present--;
    }
    if (s->sei.mastering_display.present) {
        // HEVC uses a g,b,r ordering, which we convert to a more natural r,g,b
        const int mapping[3] = {2, 0, 1};
        const int chroma_den = 50000;
        const int luma_den = 10000;
        int i;
        AVMasteringDisplayMetadata *metadata =
            av_mastering_display_metadata_create_side_data(out);
        if (!metadata)
            return AVERROR(ENOMEM);

        for (i = 0; i < 3; i++) {
            const int j = mapping[i];
            metadata->display_primaries[i][0].num = s->sei.mastering_display.display_primaries[j][0];
            metadata->display_primaries[i][0].den = chroma_den;
            metadata->display_primaries[i][1].num = s->sei.mastering_display.display_primaries[j][1];
            metadata->display_primaries[i][1].den = chroma_den;
        }
        metadata->white_point[0].num = s->sei.mastering_display.white_point[0];
        metadata->white_point[0].den = chroma_den;
        metadata->white_point[1].num = s->sei.mastering_display.white_point[1];
        metadata->white_point[1].den = chroma_den;

        metadata->max_luminance.num = s->sei.mastering_display.max_luminance;
        metadata->max_luminance.den = luma_den;
        metadata->min_luminance.num = s->sei.mastering_display.min_luminance;
        metadata->min_luminance.den = luma_den;
        metadata->has_luminance = 1;
        metadata->has_primaries = 1;

        av_log(s->avctx, AV_LOG_DEBUG, "Mastering Display Metadata:\n");
        av_log(s->avctx, AV_LOG_DEBUG,
               "r(%5.4f,%5.4f) g(%5.4f,%5.4f) b(%5.4f %5.4f) wp(%5.4f, %5.4f)\n",
               av_q2d(metadata->display_primaries[0][0]),
               av_q2d(metadata->display_primaries[0][1]),
               av_q2d(metadata->display_primaries[1][0]),
               av_q2d(metadata->display_primaries[1][1]),
               av_q2d(metadata->display_primaries[2][0]),
               av_q2d(metadata->display_primaries[2][1]),
               av_q2d(metadata->white_point[0]), av_q2d(metadata->white_point[1]));
        av_log(s->avctx, AV_LOG_DEBUG,
               "min_luminance=%f, max_luminance=%f\n",
               av_q2d(metadata->min_luminance), av_q2d(metadata->max_luminance));
    }
    // Decrement the mastering display flag when IRAP frame has no_rasl_output_flag=1
    // so the side data persists for the entire coded video sequence.
    if (s->sei.content_light.present > 0 &&
        IS_IRAP(s) && s->no_rasl_output_flag) {
        s->sei.content_light.present--;
    }
    if (s->sei.content_light.present) {
        AVContentLightMetadata *metadata =
            av_content_light_metadata_create_side_data(out);
        if (!metadata)
            return AVERROR(ENOMEM);
        metadata->MaxCLL  = s->sei.content_light.max_content_light_level;
        metadata->MaxFALL = s->sei.content_light.max_pic_average_light_level;

        av_log(s->avctx, AV_LOG_DEBUG, "Content Light Level Metadata:\n");
        av_log(s->avctx, AV_LOG_DEBUG, "MaxCLL=%d, MaxFALL=%d\n",
               metadata->MaxCLL, metadata->MaxFALL);
    }

    if (s->sei.a53_caption.a53_caption) {
        AVFrameSideData* sd = av_frame_new_side_data(out,
                                                     AV_FRAME_DATA_A53_CC,
                                                     s->sei.a53_caption.a53_caption_size);
        if (sd)
            memcpy(sd->data, s->sei.a53_caption.a53_caption, s->sei.a53_caption.a53_caption_size);
        av_freep(&s->sei.a53_caption.a53_caption);
        s->sei.a53_caption.a53_caption_size = 0;
        s->avctx->properties |= FF_CODEC_PROPERTY_CLOSED_CAPTIONS;
    }

    if (s->sei.alternative_transfer.present &&
        av_color_transfer_name(s->sei.alternative_transfer.preferred_transfer_characteristics) &&
        s->sei.alternative_transfer.preferred_transfer_characteristics != AVCOL_TRC_UNSPECIFIED) {
        s->avctx->color_trc = out->color_trc = s->sei.alternative_transfer.preferred_transfer_characteristics;
    }

    return 0;
}

static int hevc_frame_start(HEVCRpiContext * const s)
{
    int ret;

    memset(s->bs_horizontal, 0, s->bs_size * 2);  // Does V too
    memset(s->is_pcm,        0, s->ps.sps->pcm_width * s->ps.sps->pcm_height);
    memset(s->tab_slice_address, -1, s->ps.sps->ctb_size * sizeof(*s->tab_slice_address));

    // Only need to remember intra for CIP
    if (!s->ps.pps->constrained_intra_pred_flag || s->is_irap)
        s->is_intra = NULL;
    else
    {
        s->is_intra = s->is_intra_store;
        memset(s->is_intra, 0, s->ps.sps->pcm_width * s->ps.sps->pcm_height);
    }

    s->is_decoded        = 0;
    s->first_nal_type    = s->nal_unit_type;

    s->no_rasl_output_flag = IS_IDR(s) || IS_BLA(s) || (s->nal_unit_type == HEVC_NAL_CRA_NUT && s->last_eos);

    if (s->pkt.nb_nals > s->rpl_tab_size)
    {
        // In most cases it will be faster to free & realloc as that doesn't
        // require (an unwanted) copy
        av_freep(&s->rpl_tab);
        s->rpl_tab_size = 0;
        if ((s->rpl_tab = av_malloc(s->pkt.nb_nals * sizeof(*s->rpl_tab))) == NULL)
            goto fail;
        s->rpl_tab_size = s->pkt.nb_nals;
    }
    memset(s->rpl_tab, 0, s->pkt.nb_nals * sizeof(*s->rpl_tab));

    ret = ff_hevc_rpi_set_new_ref(s, &s->frame, s->poc);
    if (ret < 0)
        goto fail;

    // Resize rpl_tab to max that we might want
    ret = ff_hevc_rpi_frame_rps(s);
    if (ret < 0) {
        av_log(s->avctx, AV_LOG_ERROR, "Error constructing the frame RPS.\n");
        goto fail;
    }

    s->ref->frame->key_frame = IS_IRAP(s);

    ret = set_side_data(s);
    if (ret < 0)
        goto fail;

    s->frame->pict_type = 3 - s->sh.slice_type;

    if (!IS_IRAP(s))
        ff_hevc_rpi_bump_frame(s);

    av_frame_unref(s->output_frame);
    ret = ff_hevc_rpi_output_frame(s, s->output_frame, 0);
    if (ret < 0)
        goto fail;

    ff_thread_finish_setup(s->avctx);

    return 0;

fail:
    if (s->ref)
        ff_hevc_rpi_unref_frame(s, s->ref, ~0);
    s->ref = NULL;
    return ret;
}

static inline int is_non_ref_unit_type(const unsigned int nal_unit_type)
{
    // From Table 7-1
    return (nal_unit_type & ~0xe) == 0;  // True for 0, 2, 4, 6, 8, 10, 12, 14
}

static int decode_nal_unit(HEVCRpiContext *s, const H2645NAL *nal)
{
    GetBitContext * const gb    = &s->HEVClc->gb;
    int ctb_addr_ts, ret;

    *gb              = nal->gb;
    s->nal_unit_type = nal->type;
    s->temporal_id   = nal->temporal_id;

    switch (s->nal_unit_type) {
    case HEVC_NAL_VPS:
        ret = ff_hevc_rpi_decode_nal_vps(gb, s->avctx, &s->ps);
        if (ret < 0)
            goto fail;
        break;
    case HEVC_NAL_SPS:
        ret = ff_hevc_rpi_decode_nal_sps(gb, s->avctx, &s->ps,
                                     s->apply_defdispwin);
        if (ret < 0)
            goto fail;
        break;
    case HEVC_NAL_PPS:
        ret = ff_hevc_rpi_decode_nal_pps(gb, s->avctx, &s->ps);
        if (ret < 0)
            goto fail;
        break;
    case HEVC_NAL_SEI_PREFIX:
    case HEVC_NAL_SEI_SUFFIX:
        ret = ff_hevc_rpi_decode_nal_sei(gb, s->avctx, &s->sei, &s->ps, s->nal_unit_type);
        if (ret < 0)
            goto fail;
        break;
    case HEVC_NAL_TRAIL_R:
    case HEVC_NAL_TRAIL_N:
    case HEVC_NAL_TSA_N:
    case HEVC_NAL_TSA_R:
    case HEVC_NAL_STSA_N:
    case HEVC_NAL_STSA_R:
    case HEVC_NAL_BLA_W_LP:
    case HEVC_NAL_BLA_W_RADL:
    case HEVC_NAL_BLA_N_LP:
    case HEVC_NAL_IDR_W_RADL:
    case HEVC_NAL_IDR_N_LP:
    case HEVC_NAL_CRA_NUT:
    case HEVC_NAL_RADL_N:
    case HEVC_NAL_RADL_R:
    case HEVC_NAL_RASL_N:
    case HEVC_NAL_RASL_R:
        ret = hls_slice_header(s);
        if (ret < 0)
            return ret;

        // The definition of _N unit types is "non-reference for other frames
        // with the same temporal_id" so they may/will be ref frames for pics
        // with a higher temporal_id.
        s->used_for_ref = s->ps.sps->max_sub_layers > s->temporal_id + 1 ||
            !is_non_ref_unit_type(s->nal_unit_type);
        s->offload_recon = s->threads_type != 0 && s->used_for_ref;
        s->is_irap = IS_IRAP(s);

#if DEBUG_DECODE_N
        {
            static int z = 0;
            if (IS_IDR(s)) {
                z = 1;
            }
            if (z != 0 && z++ > DEBUG_DECODE_N) {
                s->is_decoded = 0;
                break;
            }
        }
#endif
        if (
            (s->avctx->skip_frame >= AVDISCARD_NONREF && !s->used_for_ref) ||
            (s->avctx->skip_frame >= AVDISCARD_BIDIR && s->sh.slice_type == HEVC_SLICE_B) ||
            (s->avctx->skip_frame >= AVDISCARD_NONINTRA && s->sh.slice_type != HEVC_SLICE_I) ||
            (s->avctx->skip_frame >= AVDISCARD_NONKEY && !IS_IRAP(s)))
        {
            s->is_decoded = 0;
            break;
        }

        if (s->sh.first_slice_in_pic_flag) {
            if (s->max_ra == INT_MAX) {
                if (s->nal_unit_type == HEVC_NAL_CRA_NUT || IS_BLA(s)) {
                    s->max_ra = s->poc;
                } else {
                    if (IS_IDR(s))
                        s->max_ra = INT_MIN;
                }
            }

            if ((s->nal_unit_type == HEVC_NAL_RASL_R || s->nal_unit_type == HEVC_NAL_RASL_N) &&
                s->poc <= s->max_ra) {
                s->is_decoded = 0;
                break;
            } else {
                if (s->nal_unit_type == HEVC_NAL_RASL_R && s->poc > s->max_ra)
                    s->max_ra = INT_MIN;
            }

            ret = hevc_frame_start(s);
            if (ret < 0)
                return ret;
        } else if (!s->ref) {
            av_log(s->avctx, AV_LOG_ERROR, "First slice in a frame missing.\n");
            goto fail;
        }

        if (s->nal_unit_type != s->first_nal_type) {
            av_log(s->avctx, AV_LOG_ERROR,
                   "Non-matching NAL types of the VCL NALUs: %d %d\n",
                   s->first_nal_type, s->nal_unit_type);
            return AVERROR_INVALIDDATA;
        }

        if (!s->sh.dependent_slice_segment_flag &&
            s->sh.slice_type != HEVC_SLICE_I) {
            ret = ff_hevc_rpi_slice_rpl(s);
            if (ret < 0) {
                av_log(s->avctx, AV_LOG_WARNING,
                       "Error constructing the reference lists for the current slice.\n");
                goto fail;
            }
        }

        ctb_addr_ts = hls_slice_data(s, nal);
        if (ctb_addr_ts >= s->ps.sps->ctb_size) {
            s->is_decoded = 1;
        }

        if (ctb_addr_ts < 0) {
            ret = ctb_addr_ts;
            goto fail;
        }
        break;
    case HEVC_NAL_EOS_NUT:
    case HEVC_NAL_EOB_NUT:
        s->seq_decode = (s->seq_decode + 1) & 0xff;
        s->max_ra     = INT_MAX;
        break;
    case HEVC_NAL_AUD:
    case HEVC_NAL_FD_NUT:
        break;
    default:
        av_log(s->avctx, AV_LOG_INFO,
               "Skipping NAL unit %d\n", s->nal_unit_type);
    }

    return 0;
fail:
    if (s->avctx->err_recognition & AV_EF_EXPLODE)
        return ret;
    return 0;
}

static int decode_nal_units(HEVCRpiContext *s, const uint8_t *buf, int length)
{
    int i, ret = 0;
    int eos_at_start = 1;

    s->ref = NULL;
    s->last_eos = s->eos;
    s->eos = 0;

    /* split the input packet into NAL units, so we know the upper bound on the
     * number of slices in the frame */
    ret = ff_h2645_packet_split(&s->pkt, buf, length, s->avctx, s->is_nalff,
                                s->nal_length_size, s->avctx->codec_id, 0, 0);
    if (ret < 0) {
        av_log(s->avctx, AV_LOG_ERROR,
               "Error splitting the input into NAL units.\n");
        return ret;
    }

    for (i = 0; i < s->pkt.nb_nals; i++) {
        if (s->pkt.nals[i].type == HEVC_NAL_EOB_NUT ||
            s->pkt.nals[i].type == HEVC_NAL_EOS_NUT) {
            if (eos_at_start) {
                s->last_eos = 1;
            } else {
                s->eos = 1;
            }
        } else {
            eos_at_start = 0;
        }
    }

    /* decode the NAL units */
    for (i = 0; i < s->pkt.nb_nals; i++) {
        ret = decode_nal_unit(s, &s->pkt.nals[i]);
        if (ret < 0) {
            av_log(s->avctx, AV_LOG_WARNING,
                   "Error parsing NAL unit #%d.\n", i);
            goto fail;
        }
    }

fail:  // Also success path
    if (s->ref != NULL) {
        if (s->used_for_ref && s->threads_type != 0) {
            ff_hevc_rpi_progress_signal_all_done(s);
        }
        else {
            // Flush frame to real memory as we expect to be able to pass
            // it straight on to mmal
            flush_frame(s, s->frame);
        }
    }
    return ret;
}

static void print_md5(void *log_ctx, int level, uint8_t md5[16])
{
    int i;
    for (i = 0; i < 16; i++)
        av_log(log_ctx, level, "%02"PRIx8, md5[i]);
}

static int verify_md5(HEVCRpiContext *s, AVFrame *frame)
{
    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(frame->format);
    int pixel_shift;
    int i, j;

    if (!desc)
        return AVERROR(EINVAL);

    pixel_shift = desc->comp[0].depth > 8;

    av_log(s->avctx, AV_LOG_DEBUG, "Verifying checksum for frame with POC %d: ",
           s->poc);

    /* the checksums are LE, so we have to byteswap for >8bpp formats
     * on BE arches */
#if HAVE_BIGENDIAN
    if (pixel_shift && !s->checksum_buf) {
        av_fast_malloc(&s->checksum_buf, &s->checksum_buf_size,
                       FFMAX3(frame->linesize[0], frame->linesize[1],
                              frame->linesize[2]));
        if (!s->checksum_buf)
            return AVERROR(ENOMEM);
    }
#endif

    for (i = 0; frame->data[i]; i++) {
        int width  = s->avctx->coded_width;
        int height = s->avctx->coded_height;
        int w = (i == 1 || i == 2) ? (width  >> desc->log2_chroma_w) : width;
        int h = (i == 1 || i == 2) ? (height >> desc->log2_chroma_h) : height;
        uint8_t md5[16];

        av_md5_init(s->md5_ctx);
        for (j = 0; j < h; j++) {
            const uint8_t *src = frame->data[i] + j * frame_stride1(frame, 1);
#if HAVE_BIGENDIAN
            if (pixel_shift) {
                s->bdsp.bswap16_buf((uint16_t *) s->checksum_buf,
                                    (const uint16_t *) src, w);
                src = s->checksum_buf;
            }
#endif
            av_md5_update(s->md5_ctx, src, w << pixel_shift);
        }
        av_md5_final(s->md5_ctx, md5);

        if (!memcmp(md5, s->sei.picture_hash.md5[i], 16)) {
            av_log   (s->avctx, AV_LOG_DEBUG, "plane %d - correct ", i);
            print_md5(s->avctx, AV_LOG_DEBUG, md5);
            av_log   (s->avctx, AV_LOG_DEBUG, "; ");
        } else {
            av_log   (s->avctx, AV_LOG_ERROR, "mismatching checksum of plane %d - ", i);
            print_md5(s->avctx, AV_LOG_ERROR, md5);
            av_log   (s->avctx, AV_LOG_ERROR, " != ");
            print_md5(s->avctx, AV_LOG_ERROR, s->sei.picture_hash.md5[i]);
            av_log   (s->avctx, AV_LOG_ERROR, "\n");
            return AVERROR_INVALIDDATA;
        }
    }

    av_log(s->avctx, AV_LOG_DEBUG, "\n");

    return 0;
}

static int all_sps_supported(const HEVCRpiContext * const s)
{
    for (unsigned int i = 0; i < FF_ARRAY_ELEMS(s->ps.sps_list); i++) {
        if (s->ps.sps_list[i] != NULL)
        {
            const HEVCRpiSPS * const sps = (const HEVCRpiSPS*)s->ps.sps_list[i]->data;
            if (!is_sps_supported(sps))
                return 0;
        }
    }
    return 1;
}

static int hevc_rpi_decode_extradata(HEVCRpiContext *s, uint8_t *buf, int length, int first)
{
    int ret, i;

    ret = ff_hevc_rpi_decode_extradata(buf, length, &s->ps, &s->sei, &s->is_nalff,
                                   &s->nal_length_size, s->avctx->err_recognition,
                                   s->apply_defdispwin, s->avctx);
    if (ret < 0)
        return ret;

    /* export stream parameters from the first SPS */
    for (i = 0; i < FF_ARRAY_ELEMS(s->ps.sps_list); i++) {
        if (first && s->ps.sps_list[i]) {
            const HEVCRpiSPS *sps = (const HEVCRpiSPS*)s->ps.sps_list[i]->data;
            export_stream_params(s->avctx, &s->ps, sps);
            break;
        }
    }

    return 0;
}

static int hevc_rpi_decode_frame(AVCodecContext *avctx, void *data, int *got_output,
                             AVPacket *avpkt)
{
    int ret;
    int new_extradata_size;
    uint8_t *new_extradata;
    HEVCRpiContext *s = avctx->priv_data;

    if (!avpkt->size) {
        ret = ff_hevc_rpi_output_frame(s, data, 1);
        if (ret < 0)
            return ret;

        *got_output = ret;
        return 0;
    }

    new_extradata = av_packet_get_side_data(avpkt, AV_PKT_DATA_NEW_EXTRADATA,
                                            &new_extradata_size);
    if (new_extradata && new_extradata_size > 0) {
        ret = hevc_rpi_decode_extradata(s, new_extradata, new_extradata_size, 0);
        if (ret < 0)
            return ret;
    }

    s->ref = NULL;
    ret    = decode_nal_units(s, avpkt->data, avpkt->size);
    if (ret < 0)
        return ret;

    /* verify the SEI checksum */
    if (avctx->err_recognition & AV_EF_CRCCHECK && s->is_decoded &&
        s->sei.picture_hash.is_md5) {
        ret = verify_md5(s, s->ref->frame);
        if (ret < 0 && avctx->err_recognition & AV_EF_EXPLODE) {
            ff_hevc_rpi_unref_frame(s, s->ref, ~0);
            return ret;
        }
    }
    s->sei.picture_hash.is_md5 = 0;

    if (s->is_decoded) {
        av_log(avctx, AV_LOG_DEBUG, "Decoded frame with POC %d.\n", s->poc);
        s->is_decoded = 0;
    }

    if (s->output_frame->buf[0]) {
        av_frame_move_ref(data, s->output_frame);
        *got_output = 1;
    }

    return avpkt->size;
}

static int hevc_ref_frame(HEVCRpiContext *s, HEVCRpiFrame *dst, HEVCRpiFrame *src)
{
    int ret;

    ret = ff_thread_ref_frame(&dst->tf, &src->tf);
    if (ret < 0)
        return ret;

    if (src->col_mvf_buf != NULL)
    {
        dst->col_mvf_buf = av_buffer_ref(src->col_mvf_buf);
        if (!dst->col_mvf_buf)
            goto fail;
    }
    dst->col_mvf = src->col_mvf;

    dst->poc        = src->poc;
    dst->flags      = src->flags;
    dst->sequence   = src->sequence;
    return 0;

fail:
    ff_hevc_rpi_unref_frame(s, dst, ~0);
    return AVERROR(ENOMEM);
}


static av_cold int hevc_decode_free(AVCodecContext *avctx)
{
    HEVCRpiContext * const s = avctx->priv_data;
    int i;

    pic_arrays_free(s);

    av_freep(&s->md5_ctx);

    av_freep(&s->cabac_save);

#if RPI_EXTRA_BIT_THREADS
    bit_threads_kill(s);
#endif

    hevc_exit_worker(s);
    for (i = 0; i != 2; ++i) {
        ff_hevc_rpi_progress_kill_state(s->progress_states + i);
    }
    job_lc_kill(s->HEVClc);

    av_freep(&s->sao_pixel_buffer_h[0]);  // [1] & [2] allocated with [0]
    av_freep(&s->sao_pixel_buffer_v[0]);
    av_frame_free(&s->output_frame);

    for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++) {
        ff_hevc_rpi_unref_frame(s, &s->DPB[i], ~0);
        av_frame_free(&s->DPB[i].frame);
    }

    for (i = 0; i < FF_ARRAY_ELEMS(s->ps.vps_list); i++)
        av_buffer_unref(&s->ps.vps_list[i]);
    for (i = 0; i < FF_ARRAY_ELEMS(s->ps.sps_list); i++)
        av_buffer_unref(&s->ps.sps_list[i]);
    for (i = 0; i < FF_ARRAY_ELEMS(s->ps.pps_list); i++)
        av_buffer_unref(&s->ps.pps_list[i]);
    s->ps.sps = NULL;
    s->ps.pps = NULL;
    s->ps.vps = NULL;

    // Free separately from sLists as used that way by RPI WPP
    for (i = 0; i < MAX_NB_THREADS && s->HEVClcList[i] != NULL; ++i) {
        av_freep(s->HEVClcList + i);
    }
    s->HEVClc = NULL;  // Allocated as part of HEVClcList

    ff_h2645_packet_uninit(&s->pkt);

    if (s->qpu_init_ok)
        vpu_qpu_term();
    s->qpu_init_ok = 0;

    return 0;
}


static av_cold int hevc_init_context(AVCodecContext *avctx)
{
    HEVCRpiContext *s = avctx->priv_data;
    int i;

    s->avctx = avctx;

    s->HEVClc = av_mallocz(sizeof(HEVCRpiLocalContext));
    if (!s->HEVClc)
        goto fail;
    s->HEVClcList[0] = s->HEVClc;

    if (vpu_qpu_init() != 0)
        goto fail;
    s->qpu_init_ok = 1;

#if RPI_QPU_EMU_Y || RPI_QPU_EMU_C
    {
        static const uint32_t dframe[1] = {0x80808080};
        s->qpu_dummy_frame_emu = (const uint8_t *)dframe;
    }
#endif
#if !RPI_QPU_EMU_Y || !RPI_QPU_EMU_C
    s->qpu_dummy_frame_qpu = qpu_dummy();
#endif

    bt_lc_init(s, s->HEVClc, 0);
    job_lc_init(s->HEVClc);

    for (i = 0; i != 2; ++i) {
        ff_hevc_rpi_progress_init_state(s->progress_states + i);
    }

    if ((s->cabac_save = av_malloc(sizeof(*s->cabac_save))) == NULL)
        goto fail;

     if ((s->output_frame = av_frame_alloc()) == NULL)
        goto fail;

    for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++) {
        s->DPB[i].frame = av_frame_alloc();
        if (!s->DPB[i].frame)
            goto fail;
        s->DPB[i].tf.f = s->DPB[i].frame;
        s->DPB[i].dpb_no = i;
    }

    s->max_ra = INT_MAX;

    if ((s->md5_ctx = av_md5_alloc()) == NULL)
        goto fail;

    s->context_initialized = 1;
    s->eos = 0;

    ff_hevc_rpi_reset_sei(&s->sei);

    return 0;

fail:
    av_log(s->avctx, AV_LOG_ERROR, "%s: Failed\n", __func__);
    hevc_decode_free(avctx);
    return AVERROR(ENOMEM);
}

#if HAVE_THREADS
static int hevc_update_thread_context(AVCodecContext *dst,
                                      const AVCodecContext *src)
{
    HEVCRpiContext *s  = dst->priv_data;
    HEVCRpiContext *s0 = src->priv_data;
    int i, ret;

    av_assert0(s->context_initialized);

    // dst == src can happen according to the comments and in that case
    // there is nothing to do here
    if (dst == src)
        return 0;

    for (i = 0; i < FF_ARRAY_ELEMS(s->DPB); i++) {
        ff_hevc_rpi_unref_frame(s, &s->DPB[i], ~0);
        if (s0->DPB[i].frame->buf[0]) {
            ret = hevc_ref_frame(s, &s->DPB[i], &s0->DPB[i]);
            if (ret < 0)
                return ret;
        }
    }

    if (s->ps.sps != s0->ps.sps)
        s->ps.sps = NULL;
    for (i = 0; i < FF_ARRAY_ELEMS(s->ps.vps_list); i++) {
        av_buffer_unref(&s->ps.vps_list[i]);
        if (s0->ps.vps_list[i]) {
            s->ps.vps_list[i] = av_buffer_ref(s0->ps.vps_list[i]);
            if (!s->ps.vps_list[i])
                return AVERROR(ENOMEM);
        }
    }

    for (i = 0; i < FF_ARRAY_ELEMS(s->ps.sps_list); i++) {
        av_buffer_unref(&s->ps.sps_list[i]);
        if (s0->ps.sps_list[i]) {
            s->ps.sps_list[i] = av_buffer_ref(s0->ps.sps_list[i]);
            if (!s->ps.sps_list[i])
                return AVERROR(ENOMEM);
        }
    }

    for (i = 0; i < FF_ARRAY_ELEMS(s->ps.pps_list); i++) {
        av_buffer_unref(&s->ps.pps_list[i]);
        if (s0->ps.pps_list[i]) {
            s->ps.pps_list[i] = av_buffer_ref(s0->ps.pps_list[i]);
            if (!s->ps.pps_list[i])
                return AVERROR(ENOMEM);
        }
    }

    if (s->ps.sps != s0->ps.sps)
        if ((ret = set_sps(s, s0->ps.sps, src->pix_fmt)) < 0)
            return ret;

    s->seq_decode = s0->seq_decode;
    s->seq_output = s0->seq_output;
    s->pocTid0    = s0->pocTid0;
    s->max_ra     = s0->max_ra;
    s->eos        = s0->eos;
    s->no_rasl_output_flag = s0->no_rasl_output_flag;

    s->is_nalff        = s0->is_nalff;
    s->nal_length_size = s0->nal_length_size;

    s->threads_type        = s0->threads_type;

    if (s0->eos) {
        s->seq_decode = (s->seq_decode + 1) & 0xff;
        s->max_ra = INT_MAX;
    }

    s->sei.frame_packing        = s0->sei.frame_packing;
    s->sei.display_orientation  = s0->sei.display_orientation;
    s->sei.mastering_display    = s0->sei.mastering_display;
    s->sei.content_light        = s0->sei.content_light;
    s->sei.alternative_transfer = s0->sei.alternative_transfer;

    // * We do this here as it allows us to easily locate our parents
    //   global job pool, but there really should be a less nasty way
    if (s->jbc == NULL)
    {
        av_assert0((s->jbc = rpi_job_ctl_new(s0->jbc->jbg)) != NULL);
        hevc_init_worker(s);
    }

    return 0;
}
#endif

#include <sys/stat.h>
static int qpu_ok(void)
{
    static int is_pi3 = -1;
    if (is_pi3 == -1)
    {
        struct stat sb;
        is_pi3 = (stat("/dev/rpivid-intcmem", &sb) != 0);
    }
    return is_pi3;
}

static av_cold int hevc_decode_init(AVCodecContext *avctx)
{
    HEVCRpiContext *s = avctx->priv_data;
    int ret;

    if (!qpu_ok())
        return AVERROR_DECODER_NOT_FOUND;

    if ((ret = hevc_init_context(avctx)) < 0)
        return ret;

    // If we are a child context then stop now
    // Everything after this point is either 1st decode setup or global alloc
    // that must not be repeated
    // Global info will be copied into children in update_thread_context (we
    // can't do it here as we have no way of finding the parent context)
    if (avctx->internal->is_copy)
        return 0;

    // Job allocation requires VCSM alloc to work so ensure that we have it
    // initialised by this point
    {
        HEVCRpiJobGlobal * const jbg = jbg_new(FFMAX(avctx->thread_count * 3, 5));
        if (jbg == NULL) {
            av_log(s->avctx, AV_LOG_ERROR, "%s: Job global init failed\n", __func__);
            ret = AVERROR(ENOMEM);
            goto fail;
        }

        if ((s->jbc = rpi_job_ctl_new(jbg)) == NULL) {
            av_log(s->avctx, AV_LOG_ERROR, "%s: Job ctl init failed\n", __func__);
            ret = AVERROR(ENOMEM);
            goto fail;
        }
    }

    hevc_init_worker(s);

    s->eos = 1;

    if (avctx->extradata_size > 0 && avctx->extradata) {
        if ((ret = hevc_rpi_decode_extradata(s, avctx->extradata, avctx->extradata_size, 1)) < 0)
            goto fail;

        if (!all_sps_supported(s)) {
            ret = AVERROR_DECODER_NOT_FOUND;
            goto fail;
        }
    }

    if((avctx->active_thread_type & FF_THREAD_FRAME) && avctx->thread_count > 1)
        s->threads_type = FF_THREAD_FRAME;
    else
        s->threads_type = 0;

    return 0;

fail:
    hevc_decode_free(avctx);
    return ret;
}

static void hevc_decode_flush(AVCodecContext *avctx)
{
    HEVCRpiContext *s = avctx->priv_data;
    ff_hevc_rpi_flush_dpb(s);
    s->max_ra = INT_MAX;
    s->eos = 1;
}

typedef struct  hwaccel_rpi3_qpu_env_s {
    const AVClass *av_class;
    AVZcEnvPtr zc;
} hwaccel_rpi3_qpu_env_t;

static int hwaccel_alloc_frame(AVCodecContext *s, AVFrame *frame)
{
    hwaccel_rpi3_qpu_env_t * const r3 = s->internal->hwaccel_priv_data;
    int rv;

    if (av_rpi_zc_in_use(s))
    {
        rv = s->get_buffer2(s, frame, 0);
    }
    else
    {
        rv = av_rpi_zc_get_buffer(r3->zc, frame);
        if (rv == 0)
            rv = av_rpi_zc_resolve_frame(frame, ZC_RESOLVE_ALLOC_VALID);  // actually do the alloc
    }

    if (rv == 0 &&
        (rv = ff_attach_decode_data(frame)) < 0)
    {
        av_frame_unref(frame);
    }

    return rv;
}

static int hwaccel_rpi3_qpu_free(AVCodecContext *avctx)
{
    hwaccel_rpi3_qpu_env_t * const r3 = avctx->internal->hwaccel_priv_data;
    av_rpi_zc_int_env_freep(&r3->zc);
    return 0;
}

static int hwaccel_rpi3_qpu_init(AVCodecContext *avctx)
{
    hwaccel_rpi3_qpu_env_t * const r3 = avctx->internal->hwaccel_priv_data;

    if ((r3->zc = av_rpi_zc_int_env_alloc(avctx)) == NULL)
        goto fail;

    return 0;

fail:
    av_log(avctx, AV_LOG_ERROR, "Rpi3 QPU init failed\n");
    hwaccel_rpi3_qpu_free(avctx);
    return AVERROR(ENOMEM);
}


#define OFFSET(x) offsetof(HEVCRpiContext, x)
#define PAR (AV_OPT_FLAG_DECODING_PARAM | AV_OPT_FLAG_VIDEO_PARAM)


static const AVOption options[] = {
    { "apply_defdispwin", "Apply default display window from VUI", OFFSET(apply_defdispwin),
        AV_OPT_TYPE_BOOL, {.i64 = 0}, 0, 1, PAR },
    { "strict-displaywin", "stricly apply default display window size", OFFSET(apply_defdispwin),
        AV_OPT_TYPE_BOOL, {.i64 = 0}, 0, 1, PAR },
    { NULL },
};

static const AVClass hevc_rpi_decoder_class = {
    .class_name = "HEVC RPI decoder",
    .item_name  = av_default_item_name,
    .option     = options,
    .version    = LIBAVUTIL_VERSION_INT,
};

static const enum AVPixelFormat hevc_rpi_pix_fmts[] = {
    AV_PIX_FMT_SAND128,
    AV_PIX_FMT_SAND64_10,
    AV_PIX_FMT_NONE
};


static const AVHWAccel hwaccel_rpi3_qpu = {
    .name           = "Pi3 QPU Hwaccel",
    .alloc_frame    = hwaccel_alloc_frame,
    .init           = hwaccel_rpi3_qpu_init,
    .uninit         = hwaccel_rpi3_qpu_free,
    .priv_data_size = sizeof(hwaccel_rpi3_qpu_env_t),
    .caps_internal  = HWACCEL_CAP_ASYNC_SAFE | HWACCEL_CAP_MT_SAFE,
};

static const AVCodecHWConfigInternal hevc_rpi_hw_config_sand128 =
{
    .public = {
        .pix_fmt = AV_PIX_FMT_SAND128,
        .methods = AV_CODEC_HW_CONFIG_METHOD_AD_HOC,
        .device_type = AV_HWDEVICE_TYPE_NONE,
    },
    .hwaccel = &hwaccel_rpi3_qpu
};
static const AVCodecHWConfigInternal hevc_rpi_hw_config_sand64_10 =
{
    .public = {
        .pix_fmt = AV_PIX_FMT_SAND64_10,
        .methods = AV_CODEC_HW_CONFIG_METHOD_AD_HOC,
        .device_type = AV_HWDEVICE_TYPE_NONE,
    },
    .hwaccel = &hwaccel_rpi3_qpu
};


static const AVCodecHWConfigInternal *hevc_rpi_hw_configs[] = {
    &hevc_rpi_hw_config_sand128,
    &hevc_rpi_hw_config_sand64_10,
    NULL
};


AVCodec ff_hevc_rpi_decoder = {
    .name                  = "hevc_rpi",
    .long_name             = NULL_IF_CONFIG_SMALL("HEVC (rpi)"),
    .type                  = AVMEDIA_TYPE_VIDEO,
    .id                    = AV_CODEC_ID_HEVC,
    .priv_data_size        = sizeof(HEVCRpiContext),
    .priv_class            = &hevc_rpi_decoder_class,
    .init                  = hevc_decode_init,
    .close                 = hevc_decode_free,
    .decode                = hevc_rpi_decode_frame,
    .flush                 = hevc_decode_flush,
    .update_thread_context = ONLY_IF_THREADS_ENABLED(hevc_update_thread_context),
    .capabilities          = AV_CODEC_CAP_DR1 | AV_CODEC_CAP_DELAY |
                             AV_CODEC_CAP_HARDWARE |
                             AV_CODEC_CAP_AVOID_PROBING |
#if 0
    // Debugging is often easier without threads getting in the way
                            0,
#warning H265 threading turned off
#else
    // We only have decent optimisation for frame - so only admit to that
                             AV_CODEC_CAP_FRAME_THREADS,
#endif
    .caps_internal         = FF_CODEC_CAP_INIT_THREADSAFE |
                             FF_CODEC_CAP_EXPORTS_CROPPING |
                             FF_CODEC_CAP_ALLOCATE_PROGRESS,
    .pix_fmts              = hevc_rpi_pix_fmts,
    .profiles              = NULL_IF_CONFIG_SMALL(ff_hevc_profiles),
    .hw_configs            = hevc_rpi_hw_configs,
//    .wrapper_name          = "hevc_rpi",
};

