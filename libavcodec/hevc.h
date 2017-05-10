/*
 * HEVC shared code
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

#ifndef AVCODEC_HEVC_H
#define AVCODEC_HEVC_H

// define RPI to split the CABAC/prediction/transform into separate stages
#ifndef RPI

  #define RPI_INTER          0

#else

  #include "rpi_qpu.h"
  #define RPI_INTER          1          // 0 use ARM for UV inter-pred, 1 use QPU

  // Define RPI_WORKER to launch a worker thread for pixel processing tasks
  #define RPI_WORKER
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

  #define RPI_FRAME_INVALID      1
#endif

/**
 * Table 7-3: NAL unit type codes
 */
enum HEVCNALUnitType {
    HEVC_NAL_TRAIL_N    = 0,
    HEVC_NAL_TRAIL_R    = 1,
    HEVC_NAL_TSA_N      = 2,
    HEVC_NAL_TSA_R      = 3,
    HEVC_NAL_STSA_N     = 4,
    HEVC_NAL_STSA_R     = 5,
    HEVC_NAL_RADL_N     = 6,
    HEVC_NAL_RADL_R     = 7,
    HEVC_NAL_RASL_N     = 8,
    HEVC_NAL_RASL_R     = 9,
    HEVC_NAL_BLA_W_LP   = 16,
    HEVC_NAL_BLA_W_RADL = 17,
    HEVC_NAL_BLA_N_LP   = 18,
    HEVC_NAL_IDR_W_RADL = 19,
    HEVC_NAL_IDR_N_LP   = 20,
    HEVC_NAL_CRA_NUT    = 21,
    HEVC_NAL_VPS        = 32,
    HEVC_NAL_SPS        = 33,
    HEVC_NAL_PPS        = 34,
    HEVC_NAL_AUD        = 35,
    HEVC_NAL_EOS_NUT    = 36,
    HEVC_NAL_EOB_NUT    = 37,
    HEVC_NAL_FD_NUT     = 38,
    HEVC_NAL_SEI_PREFIX = 39,
    HEVC_NAL_SEI_SUFFIX = 40,
};

enum HEVCSliceType {
    HEVC_SLICE_B = 0,
    HEVC_SLICE_P = 1,
    HEVC_SLICE_I = 2,
};

/**
 * 7.4.2.1
 */
#define HEVC_MAX_SUB_LAYERS 7
#define HEVC_MAX_VPS_COUNT 16
#define HEVC_MAX_SPS_COUNT 32
#define HEVC_MAX_PPS_COUNT 256
#define HEVC_MAX_SHORT_TERM_RPS_COUNT 64
#define HEVC_MAX_CU_SIZE 128

#define HEVC_MAX_REFS 16
#define HEVC_MAX_DPB_SIZE 16 // A.4.1

#define HEVC_MAX_LOG2_CTB_SIZE 6

#endif /* AVCODEC_HEVC_H */
