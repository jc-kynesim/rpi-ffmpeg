// v4l2_phase.c

#include <stdio.h>
#include <semaphore.h>
#include <pthread.h>

#include "libavutil/log.h"
#include "v4l2_phase.h"

typedef struct phase_envss {
    unsigned int last_order;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} phase_env;

struct V4L2PhaseControl {
    unsigned int order;
    unsigned int phase_count;
    phase_env p[V4L2PHASE_PHASE_COUNT];
};


unsigned int ff_v4l2_phase_order_next(V4L2PhaseControl * const pc)
{
    return ++pc->order;
}

// Phase isn't required but it acts as a check that we know what we are doing
int
ff_v4l2_phase_claim(V4L2PhaseInfo * const pi, unsigned int phase)
{
    V4L2PhaseControl *const pc = pi->ctrl;
    phase_env * const p = pc->p + phase;

    if (pi->n2 != phase * 2) {
        av_log(NULL, AV_LOG_ERROR, "%s: Unexpected phase: req=%d, cur=%d/%d\n", __func__, phase, pi->n2 >> 1, pi->n2 & 1);
        return -1;
    }

    pthread_mutex_lock(&p->lock);

    while (pi->order != p->last_order + 1) {
        pthread_cond_wait(&p->cond, &p->lock);
    }

    pi->n2++;
    pthread_mutex_unlock(&p->lock);
    return 0;
}

int
ff_v4l2_phase_release(V4L2PhaseInfo * const pi, unsigned int phase)
{
    V4L2PhaseControl *const pc = pi->ctrl;
    phase_env * const p = pc->p + phase;

    if (pi->n2 != ((phase << 1) | 1)) {
        av_log(NULL, AV_LOG_ERROR, "%s: Unexpected phase: req=%d, cur=%d/%d\n", __func__, phase, pi->n2 >> 1, pi->n2 & 1);
        return -1;
    }

    if (pi->order != p->last_order + 1) {
        av_log(NULL, AV_LOG_ERROR, "%s: order_mismatch\n", __func__);
        return -1;
    }

    pthread_mutex_lock(&p->lock);
    p->last_order = pi->order;
    pi->n2++;
    pthread_cond_broadcast(&p->cond);
    pthread_mutex_unlock(&p->lock);
    return 0;
}

// Init the PhaseInfo, assign a new order, claim phase 0
int
ff_v4l2_phase_start(V4L2PhaseInfo * const pi, V4L2PhaseControl * const pc)
{
    pi->n2 = 0;
    pi->ctrl = pc;
    pi->order = ff_v4l2_phase_order_next(pc);
    return ff_v4l2_phase_claim(pi, 0);
}

// Release any claimed phase and claim+release all remaining phases
void ff_v4l2_phase_abort(V4L2PhaseInfo * const pi)
{
    V4L2PhaseControl *const pc = pi->ctrl;

    // Nothing to do
    if (pi->n2 == 0 || pi->n2 >= pc->phase_count * 2)
        return;

    // Run through all remaining phases
    do {
        if ((pi->n2 & 1) == 0)
            ff_v4l2_phase_claim(pi, pi->n2 >> 1);
        else
            ff_v4l2_phase_release(pi, pi->n2 >> 1);
    } while (pi->n2 < pc->phase_count * 2);
}


V4L2PhaseControl *
ff_v4l2_phase_control_new(unsigned int phase_count)
{
    V4L2PhaseControl * pc;
    unsigned int i;
    if (phase_count > V4L2PHASE_PHASE_COUNT)
        return NULL;
    if ((pc = av_mallocz(sizeof(*pc))) == NULL)
        return NULL;
    pc->phase_count = phase_count;
    for (i = 0; i != phase_count; ++i) {
        phase_env * const p = pc->p + i;
        p->last_order = 0;
        pthread_mutex_init(&p->lock, NULL);
        pthread_cond_init(&p->cond, NULL);
    }
    return pc;
}

void
ff_v4l2_phase_control_deletez(V4L2PhaseControl ** const ppc)
{
    V4L2PhaseControl * const pc = *ppc;
    unsigned int i;

    if (pc == NULL)
        return;
    *ppc = NULL;

    for (i = 0; i != pc->phase_count; ++i) {
        phase_env * const p = pc->p + i;
        pthread_mutex_destroy(&p->lock);
        pthread_cond_destroy(&p->cond);
    }
}


