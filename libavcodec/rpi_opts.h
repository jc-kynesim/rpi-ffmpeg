#ifndef AVCODEC_RPI_OPTS_H
#define AVCODEC_RPI_OPTS_H

// define RPI to split the CABAC/prediction/transform into separate stages
#ifndef RPI

  #define RPI_INTER          0
  #define RPI_TSTATS         0
  #define RPI_HEVC_SAND      0

#else
  #include "config.h"

  #define RPI_INTER          1          // 0 use ARM for UV inter-pred, 1 use QPU

  // By passing jobs to a worker thread we hope to be able to catch up during slow frames
  // This has no effect unless RPI_WORKER is defined
  // N.B. The extra thread count is effectively RPI_MAX_JOBS - 1 as
  // RPI_MAX_JOBS defines the number of worker parameter sets and we must have one
  // free for the foreground to fill in.
  #define RPI_MAX_JOBS 2

  // Define RPI_DEBLOCK_VPU to perform deblocking on the VPUs
  // As it stands there is something mildy broken in VPU deblock - looks mostly OK
  // but reliably fails some conformance tests (e.g. DBLK_A/B/C_)
  // With VPU luma & chroma pred it is much the same speed to deblock on the ARM
//  #define RPI_DEBLOCK_VPU

  #define RPI_VPU_DEBLOCK_CACHED 1

  #if HAVE_NEON
  #define RPI_HEVC_SAND      1
  #else
  // Sand bust on Pi1 currently - reasons unknown
  #define RPI_HEVC_SAND      0
  #endif


  #define RPI_QPU_EMU_Y      0
  #define RPI_QPU_EMU_C      0

  #define RPI_TSTATS 0
#endif

#endif

