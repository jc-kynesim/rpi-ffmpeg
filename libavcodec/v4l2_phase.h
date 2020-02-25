// v4l2_phase.h
#ifndef AVCODEC_V4L2_PHASE_H
#define AVCODEC_V4L2_PHASE_H

#define V4L2PHASE_PHASE_COUNT 2

struct V4L2PhaseControl;
typedef struct V4L2PhaseControl V4L2PhaseControl;

typedef struct V4L2PhaseInfo {
    unsigned int n2;  // (phase << 1) | (claimed)
    unsigned int order;
    V4L2PhaseControl * ctrl;
} V4L2PhaseInfo;

unsigned int ff_v4l2_phase_order_next(V4L2PhaseControl * const pc);

static inline int ff_v4l2_phase_started(const V4L2PhaseInfo * const pi)
{
    return pi->n2 != 0;
}

// Init the PhaseInfo, assign a new order, claim phase 0
int ff_v4l2_phase_start(V4L2PhaseInfo * const pi, V4L2PhaseControl * const pc);

// Phase isn't required but it acts as a check that we know what we are doing
int ff_v4l2_phase_claim(V4L2PhaseInfo * const pi, unsigned int phase);
int ff_v4l2_phase_release(V4L2PhaseInfo * const pi, unsigned int phase);

// Release any claimed phase and claim+release all remaining phases
void ff_v4l2_phase_abort(V4L2PhaseInfo * const pi);


V4L2PhaseControl * ff_v4l2_phase_control_new(unsigned int phase_count);
void ff_v4l2_phase_control_deletez(V4L2PhaseControl ** const ppc);

#endif
