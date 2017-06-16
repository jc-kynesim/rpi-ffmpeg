#include "hevc.h"
#include "hevcdec.h"
#include "rpi_shader_cmd.h"
#include "rpi_shader_template.h"

#include "rpi_zc.h"

typedef struct shader_track_s
{
    union qpu_mc_pred_cmd_u *qpu_mc_curr;
    struct qpu_mc_src_s *last_l0;
    struct qpu_mc_src_s *last_l1;
} shader_track_t;

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
                    }
                    else if (link == s->qpu_filter) {
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

