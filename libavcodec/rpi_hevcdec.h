/*
 * HEVC video decoder
 *
 * Copyright (C) 2012 - 2013 Guillaume Martres
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

#ifndef AVCODEC_RPI_HEVCDEC_H
#define AVCODEC_RPI_HEVCDEC_H

#include "config.h"

#include <stdatomic.h>

#include "libavutil/buffer.h"

#include "avcodec.h"
#include "bswapdsp.h"
#include "cabac.h"
#include "get_bits.h"
#include "rpi_hevcpred.h"
#include "h2645_parse.h"
#include "hevc.h"
#include "rpi_hevc_ps.h"
#include "rpi_hevc_sei.h"
#include "rpi_hevcdsp.h"
#include "internal.h"
#include "thread.h"
#include "videodsp.h"

#define MAX_NB_THREADS 16
#define SHIFT_CTB_WPP 2

//TODO: check if this is really the maximum
#define MAX_TRANSFORM_DEPTH 5

#define MAX_TB_SIZE 32
#define MAX_QP 51
#define DEFAULT_INTRA_TC_OFFSET 2

#define HEVC_CONTEXTS 199

#define MRG_MAX_NUM_CANDS     5

#define HEVC_MAX_CTB_SIZE (1 << HEVC_MAX_LOG2_CTB_SIZE)  // 64

// Size of DPB array
#define HEVC_DPB_ELS            32

#define L0 0
#define L1 1

#define EPEL_EXTRA_BEFORE 1
#define EPEL_EXTRA_AFTER  2
#define EPEL_EXTRA        3
#define QPEL_EXTRA_BEFORE 3
#define QPEL_EXTRA_AFTER  4
#define QPEL_EXTRA        7

#define EDGE_EMU_BUFFER_STRIDE 80

#if !CONFIG_HEVC_RPI_DECODER
#define RPI_TSTATS         0
#else
#include <semaphore.h>
#include "rpi_qpu.h"

// Max jobs per frame thread. Actual usage will be limited by the size
// of the global job pool
// ?? Limits
#define RPI_MAX_JOBS            8

// This is the number of _extra_ bit threads - we will have
// RPI_EXTRA_BIT_THREADS+1 threads actually doing the processing
//
// 0 is legitimate and will disable our WPP processing
//#define RPI_EXTRA_BIT_THREADS 0
#define RPI_EXTRA_BIT_THREADS   2

// Number of separate threads/passes in worker
// 2 and 3 are the currently valid numbers
// At the moment 3 seems fractionally faster
//#define RPI_PASSES 2
#define RPI_PASSES              3

// Print out various usage stats
#define RPI_TSTATS              0

// Define RPI_DEBLOCK_VPU to perform deblocking on the VPUs
// (currently slower than deblocking on the ARM)
// #define RPI_DEBLOCK_VPU

#define RPI_VPU_DEBLOCK_CACHED 0

// Use ARM emulation of QPU pred
// These are for debug only as the emulation makes only limited
// effort to be fast
#define RPI_QPU_EMU_Y           0
#define RPI_QPU_EMU_C           0
#endif

/**
 * Value of the luma sample at position (x, y) in the 2D array tab.
 */
#define SAMPLE(tab, x, y) ((tab)[(y) * s->sps->width + (x)])
#define SAMPLE_CTB(tab, x, y) ((tab)[(y) * min_cb_width + (x)])

#define IS_IDR(s) ((s)->nal_unit_type == HEVC_NAL_IDR_W_RADL || (s)->nal_unit_type == HEVC_NAL_IDR_N_LP)
#define IS_BLA(s) ((s)->nal_unit_type == HEVC_NAL_BLA_W_RADL || (s)->nal_unit_type == HEVC_NAL_BLA_W_LP || \
                   (s)->nal_unit_type == HEVC_NAL_BLA_N_LP)
#define IS_IRAP(s) ((s)->nal_unit_type >= 16 && (s)->nal_unit_type <= 23)

enum RPSType {
    ST_CURR_BEF = 0,
    ST_CURR_AFT,
    ST_FOLL,
    LT_CURR,
    LT_FOLL,
    NB_RPS_TYPE,
};

enum SyntaxElement {
    SAO_MERGE_FLAG = 0,
    SAO_TYPE_IDX,
    SAO_EO_CLASS,
    SAO_BAND_POSITION,
    SAO_OFFSET_ABS,
    SAO_OFFSET_SIGN,
    END_OF_SLICE_FLAG,
    SPLIT_CODING_UNIT_FLAG,
    CU_TRANSQUANT_BYPASS_FLAG,
    SKIP_FLAG,
    CU_QP_DELTA,
    PRED_MODE_FLAG,
    PART_MODE,
    PCM_FLAG,
    PREV_INTRA_LUMA_PRED_FLAG,
    MPM_IDX,
    REM_INTRA_LUMA_PRED_MODE,
    INTRA_CHROMA_PRED_MODE,
    MERGE_FLAG,
    MERGE_IDX,
    INTER_PRED_IDC,
    REF_IDX_L0,
    REF_IDX_L1,
    ABS_MVD_GREATER0_FLAG,
    ABS_MVD_GREATER1_FLAG,
    ABS_MVD_MINUS2,
    MVD_SIGN_FLAG,
    MVP_LX_FLAG,
    NO_RESIDUAL_DATA_FLAG,
    SPLIT_TRANSFORM_FLAG,
    CBF_LUMA,
    CBF_CB_CR,
    TRANSFORM_SKIP_FLAG,
    EXPLICIT_RDPCM_FLAG,
    EXPLICIT_RDPCM_DIR_FLAG,
    LAST_SIGNIFICANT_COEFF_X_PREFIX,
    LAST_SIGNIFICANT_COEFF_Y_PREFIX,
    LAST_SIGNIFICANT_COEFF_X_SUFFIX,
    LAST_SIGNIFICANT_COEFF_Y_SUFFIX,
    SIGNIFICANT_COEFF_GROUP_FLAG,
    SIGNIFICANT_COEFF_FLAG,
    COEFF_ABS_LEVEL_GREATER1_FLAG,
    COEFF_ABS_LEVEL_GREATER2_FLAG,
    COEFF_ABS_LEVEL_REMAINING,
    COEFF_SIGN_FLAG,
    LOG2_RES_SCALE_ABS,
    RES_SCALE_SIGN_FLAG,
    CU_CHROMA_QP_OFFSET_FLAG,
    CU_CHROMA_QP_OFFSET_IDX,
};

enum PartMode {
    PART_2Nx2N = 0,
    PART_2NxN  = 1,
    PART_Nx2N  = 2,
    PART_NxN   = 3,
    PART_2NxnU = 4,
    PART_2NxnD = 5,
    PART_nLx2N = 6,
    PART_nRx2N = 7,
};

enum PredMode {
    MODE_INTER = 0,
    MODE_INTRA,
    MODE_SKIP,
};

enum InterPredIdc {
    PRED_L0 = 0,
    PRED_L1,
    PRED_BI,
};

enum PredFlag {
    PF_INTRA = 0,
    PF_L0,
    PF_L1,
    PF_BI,
};

enum IntraPredMode {
    INTRA_PLANAR = 0,
    INTRA_DC,
    INTRA_ANGULAR_2,
    INTRA_ANGULAR_3,
    INTRA_ANGULAR_4,
    INTRA_ANGULAR_5,
    INTRA_ANGULAR_6,
    INTRA_ANGULAR_7,
    INTRA_ANGULAR_8,
    INTRA_ANGULAR_9,
    INTRA_ANGULAR_10,
    INTRA_ANGULAR_11,
    INTRA_ANGULAR_12,
    INTRA_ANGULAR_13,
    INTRA_ANGULAR_14,
    INTRA_ANGULAR_15,
    INTRA_ANGULAR_16,
    INTRA_ANGULAR_17,
    INTRA_ANGULAR_18,
    INTRA_ANGULAR_19,
    INTRA_ANGULAR_20,
    INTRA_ANGULAR_21,
    INTRA_ANGULAR_22,
    INTRA_ANGULAR_23,
    INTRA_ANGULAR_24,
    INTRA_ANGULAR_25,
    INTRA_ANGULAR_26,
    INTRA_ANGULAR_27,
    INTRA_ANGULAR_28,
    INTRA_ANGULAR_29,
    INTRA_ANGULAR_30,
    INTRA_ANGULAR_31,
    INTRA_ANGULAR_32,
    INTRA_ANGULAR_33,
    INTRA_ANGULAR_34,
};

enum SAOType {
    SAO_NOT_APPLIED = 0,
    SAO_BAND,
    SAO_EDGE,
    SAO_APPLIED
};

enum SAOEOClass {
    SAO_EO_HORIZ = 0,
    SAO_EO_VERT,
    SAO_EO_135D,
    SAO_EO_45D,
};

enum ScanType {
    SCAN_DIAG = 0,
    SCAN_HORIZ,
    SCAN_VERT,
};

typedef struct RefPicList {
    struct HEVCFrame *ref[HEVC_MAX_REFS];
    int list[HEVC_MAX_REFS];
    int isLongTerm[HEVC_MAX_REFS];
    int nb_refs;
} RefPicList;

typedef struct RefPicListTab {
    RefPicList refPicList[2];
} RefPicListTab;

typedef struct CodingUnit {
    int x;
    int y;

    enum PredMode pred_mode;    ///< PredMode
    enum PartMode part_mode;    ///< PartMode

    // Inferred parameters
    uint8_t intra_split_flag;   ///< IntraSplitFlag
    uint8_t max_trafo_depth;    ///< MaxTrafoDepth
    uint8_t cu_transquant_bypass_flag;
} CodingUnit;

typedef struct NeighbourAvailable {
    int cand_bottom_left;
    int cand_left;
    int cand_up;
    int cand_up_left;
    int cand_up_right;
    int cand_up_right_sap;
} NeighbourAvailable;

typedef struct PredictionUnit {
    int mpm_idx;
    int rem_intra_luma_pred_mode;
    uint8_t intra_pred_mode[4];
    Mv mvd;
    uint8_t merge_flag;
    uint8_t intra_pred_mode_c[4];
    uint8_t chroma_mode_c[4];
} PredictionUnit;

typedef struct TransformUnit {
    int cu_qp_delta;

    int res_scale_val;

    // Inferred parameters;
    int intra_pred_mode;
    int intra_pred_mode_c;
    int chroma_mode_c;
    uint8_t is_cu_qp_delta_coded;
    uint8_t is_cu_chroma_qp_offset_coded;
    int8_t  cu_qp_offset_cb;
    int8_t  cu_qp_offset_cr;
    uint8_t cross_pf;
} TransformUnit;

typedef struct DBParams {
    int8_t beta_offset; // -12 to +12
    int8_t tc_offset;   // -12 to +12
} DBParams;

#define HEVC_FRAME_FLAG_OUTPUT    (1 << 0)
#define HEVC_FRAME_FLAG_SHORT_REF (1 << 1)
#define HEVC_FRAME_FLAG_LONG_REF  (1 << 2)
#define HEVC_FRAME_FLAG_BUMPING   (1 << 3)

struct HEVCRpiJob;

typedef struct HEVCFrame {
    AVFrame *frame;
    ThreadFrame tf;
    MvField *tab_mvf;
    RefPicList *refPicList;
    RefPicListTab **rpl_tab;
    int ctb_count;
    int poc;
    struct HEVCFrame *collocated_ref;

    AVBufferRef *tab_mvf_buf;
    AVBufferRef *rpl_tab_buf;
    AVBufferRef *rpl_buf;

    AVBufferRef *hwaccel_priv_buf;
    void *hwaccel_picture_private;

    /**
     * A sequence counter, so that old frames are output first
     * after a POC reset
     */
    uint16_t sequence;

    /**
     * A combination of HEVC_FRAME_FLAG_*
     */
    uint8_t flags;

    // Entry no in DPB - can be used as a small unique
    // frame identifier (within the current thread)
    uint8_t dpb_no;
} HEVCFrame;

#if CONFIG_HEVC_RPI_DECODER
typedef struct HEVCLocalContextIntra {
    TransformUnit tu;
    NeighbourAvailable na;
} HEVCLocalContextIntra;
#endif

typedef struct HEVCLocalContext {
    TransformUnit tu;  // Moved to start to match HEVCLocalContextIntra (yuk!)
    NeighbourAvailable na;

#if CONFIG_HEVC_RPI_DECODER
    // Vars that allow us to locate everything from just an lc
    struct HEVCContext * context;  // ??? make const ???
    unsigned int lc_n; // lc list el no

    // Job wait links
    struct HEVCLocalContext * jw_next;
    struct HEVCLocalContext * jw_prev;
    struct HEVCLocalContext * ljw_next;
    struct HEVCLocalContext * ljw_prev;
    struct HEVCRpiJob * volatile jw_job;
    sem_t jw_sem;

    // ?? Wrap in structure ??
    sem_t bt_sem_in;
    sem_t * bt_psem_out;
    volatile int bt_terminate;
    unsigned int ts;
    unsigned int bt_last_line;  // Last line in this bit_thread chunk
    unsigned int bt_line_no;
    unsigned int bt_line_width;
    unsigned int bt_line_inc;

    struct HEVCRpiJob * jb0;
    char unit_done;  // Set once we have dealt with this slice
//    char max_done;
    char bt_is_tile;
    char last_progress_good;
#endif
    char wpp_init;   // WPP/Tile bitstream init has happened

    uint8_t cabac_state[HEVC_CONTEXTS];

    uint8_t stat_coeff[4];

//    uint8_t first_qp_group;

    GetBitContext gb;
    CABACContext cc;

    int8_t qp_y;
    int8_t curr_qp_y;

    int qPy_pred;

    uint8_t ctb_left_flag;
    uint8_t ctb_up_flag;
    uint8_t ctb_up_right_flag;
    uint8_t ctb_up_left_flag;
    int     end_of_tiles_x;
    int     end_of_tiles_y;
    /* +7 is for subpixel interpolation, *2 for high bit depths */
    DECLARE_ALIGNED(32, uint8_t, edge_emu_buffer)[(MAX_PB_SIZE + 7) * EDGE_EMU_BUFFER_STRIDE * 2];
    /* The extended size between the new edge emu buffer is abused by SAO */
    DECLARE_ALIGNED(32, uint8_t, edge_emu_buffer2)[(MAX_PB_SIZE + 7) * EDGE_EMU_BUFFER_STRIDE * 2];
    DECLARE_ALIGNED(32, int16_t, tmp [MAX_PB_SIZE * MAX_PB_SIZE]);

    int ct_depth;
    CodingUnit cu;
    PredictionUnit pu;

#define BOUNDARY_LEFT_SLICE     (1 << 0)
#define BOUNDARY_LEFT_TILE      (1 << 1)
#define BOUNDARY_UPPER_SLICE    (1 << 2)
#define BOUNDARY_UPPER_TILE     (1 << 3)
    /* properties of the boundary of the current CTB for the purposes
     * of the deblocking filter */
    int boundary_flags;
} HEVCLocalContext;

#if CONFIG_HEVC_RPI_DECODER

// RPI_MAX_WIDTH is maximum width in pixels supported by the accelerated code
// Various buffer sizes depend on this so do not over allocate
#define RPI_MAX_WIDTH 2048

// Each block can have an intra prediction and an add_residual command
// noof-cmds(2) * max-ctu height(64) / min-transform(4) * planes(3) * MAX_WIDTH
#if CONFIG_HEVC_RPI_DECODER
// Sand only has 2 planes (Y/C)
#define RPI_MAX_PRED_CMDS (2*(HEVC_MAX_CTB_SIZE/4)*2*(RPI_MAX_WIDTH/4))
#else
#define RPI_MAX_PRED_CMDS (2*(HEVC_MAX_CTB_SIZE/4)*3*(RPI_MAX_WIDTH/4))
#endif

#ifdef RPI_DEBLOCK_VPU
// Worst case is 16x16 CTUs
#define RPI_MAX_DEBLOCK_CMDS (RPI_MAX_WIDTH*4/16)
#endif

// Command for intra prediction and transform_add of predictions to coefficients
enum rpi_pred_cmd_e
{
    RPI_PRED_ADD_RESIDUAL,
    RPI_PRED_ADD_RESIDUAL_U, // = RPI_PRED_TRANSFORM_ADD + c_idx
    RPI_PRED_ADD_RESIDUAL_V, // = RPI_PRED_TRANSFORM_ADD + c_idx
    RPI_PRED_ADD_RESIDUAL_C, // Merged U+V
    RPI_PRED_ADD_DC,
    RPI_PRED_ADD_DC_U,       // Both U & V are effectively C
    RPI_PRED_ADD_DC_V,
    RPI_PRED_INTRA,
    RPI_PRED_I_PCM,
    RPI_PRED_CMD_MAX
};

typedef struct HEVCPredCmd {
    uint8_t type;
    uint8_t size;  // log2 "size" used by all variants
    uint8_t na;    // i_pred - but left here as they pack well
    uint8_t c_idx; // i_pred
    union {
        struct {  // TRANSFORM_ADD
            uint8_t * dst;
            const int16_t * buf;
            uint16_t stride;  // Should be good enough for all pic fmts we use
            int16_t dc;
        } ta;
        struct {
            uint8_t * dst;
            uint32_t stride;
            int dc;
        } dc;
        struct {  // INTRA
            uint16_t x;
            uint16_t y;
            enum IntraPredMode mode;
        } i_pred;
        struct {  // I_PCM
            uint16_t x;
            uint16_t y;
            const void * src;
            uint32_t src_len;
        } i_pcm;
    };
} HEVCPredCmd;

union qpu_mc_pred_cmd_s;
struct qpu_mc_pred_y_p_s;
struct qpu_mc_src_s;

typedef struct HEVCRpiInterPredQ
{
    union qpu_mc_pred_cmd_u *qpu_mc_base;
    union qpu_mc_pred_cmd_u *qpu_mc_curr;
    struct qpu_mc_src_s *last_l0;
    struct qpu_mc_src_s *last_l1;
    unsigned int load;
    uint32_t code_setup;
    uint32_t code_sync;
    uint32_t code_exit;
} HEVCRpiInterPredQ;

typedef struct HEVCRpiInterPredEnv
{
    HEVCRpiInterPredQ * q;
    uint8_t n;                  // Number of Qs
    uint8_t n_grp;              // Number of Q in a group
    uint8_t curr;               // Current Q number (0..n-1)
    uint8_t used;               // 0 if nothing in any Q, 1 otherwise
    uint8_t used_grp;           // 0 if nothing in any Q in the current group
    unsigned int max_fill;
    unsigned int min_gap;
    GPU_MEM_PTR_T gptr;
} HEVCRpiInterPredEnv;

typedef struct HEVCRpiIntraPredEnv {
    unsigned int n;        // Number of commands
    HEVCPredCmd * cmds;
} HEVCRpiIntraPredEnv;

typedef struct HEVCRpiCoeffEnv {
    unsigned int n;
    int16_t * buf;
} HEVCRpiCoeffEnv;

typedef struct HEVCRpiCoeffsEnv {
    HEVCRpiCoeffEnv s[4];
    GPU_MEM_PTR_T gptr;
    void * mptr;
} HEVCRpiCoeffsEnv;

typedef struct HEVCRpiFrameProgressWait {
    int req;
    struct HEVCRpiFrameProgressWait * next;
    sem_t sem;
} HEVCRpiFrameProgressWait;

typedef struct HEVCRpiFrameProgressState {
    struct HEVCRpiFrameProgressWait * first;
    struct HEVCRpiFrameProgressWait * last;
    pthread_mutex_t lock;
} HEVCRpiFrameProgressState;

typedef struct RpiBlk
{
    unsigned int x;
    unsigned int y;
    unsigned int w;
    unsigned int h;
} RpiBlk;

typedef struct HEVCRpiJob {
    struct HEVCRpiJob * next;  // Free chain
    struct HEVCRpiJobCtl * jbc_local;
    const HEVCSPS * sps;       // sps used to set up this job

    int waited;
    int ctu_ts_first;
    int ctu_ts_last;
    RpiBlk bounds;  // Bounding box of job

    struct qpu_mc_pred_y_p_s * last_y8_p;
    struct qpu_mc_src_s * last_y8_l1;

    HEVCRpiInterPredEnv chroma_ip;
    HEVCRpiInterPredEnv luma_ip;
    int16_t progress_req[HEVC_DPB_ELS]; // index by dpb_no
    HEVCRpiIntraPredEnv intra;
    HEVCRpiCoeffsEnv coeffs;
    HEVCRpiFrameProgressWait progress_wait;
} HEVCRpiJob;

struct HEVCContext;

typedef void HEVCRpiWorkerFn(struct HEVCContext * const s, HEVCRpiJob * const jb);

typedef struct HEVCRpiPassQueue
{
//    int pending;
    volatile int terminate;
    sem_t sem_in;
    sem_t * psem_out;
    unsigned int job_n;
    struct HEVCContext * context; // Context pointer as we get to pass a single "void * this" to the thread
    HEVCRpiWorkerFn * worker;
    pthread_t thread;
    uint8_t pass_n;  // Pass number - debug
    uint8_t started;
} HEVCRpiPassQueue;


struct HEVCRpiJobGlobal;

typedef struct HEVCRpiJobCtl
{
    sem_t sem_out;

    HEVCRpiJob * volatile jb1;  // The job associated with this frame if unallocated - NULL if allocated
    struct HEVCRpiJobGlobal * jbg;

    HEVCLocalContext * lcw_head;
    HEVCLocalContext * lcw_tail;

    pthread_mutex_t in_lock;
    int offload_in;

    HEVCRpiJob *offloadq[RPI_MAX_JOBS];
} HEVCRpiJobCtl;


typedef struct HEVCRpiJobGlobal
{
    intptr_t ref_count;
    pthread_mutex_t lock;
    HEVCRpiJob * free1;                 // Singly linked list of free jobs
    HEVCLocalContext * wait_head;       // Double linked list of lcs waiting for a job
    HEVCLocalContext * wait_good;  // Last good tail
    HEVCLocalContext * wait_tail;

} HEVCRpiJobGlobal;

#define RPI_BIT_THREADS (RPI_EXTRA_BIT_THREADS + 1)

#if RPI_TSTATS
typedef struct HEVCRpiStats {
    int y_pred1_y8_merge;
    int y_pred1_xy;
    int y_pred1_x0;
    int y_pred1_y0;
    int y_pred1_x0y0;
    int y_pred1_wle8;
    int y_pred1_wgt8;
    int y_pred1_hle16;
    int y_pred1_hgt16;
    int y_pred2_xy;
    int y_pred2_x0;
    int y_pred2_y0;
    int y_pred2_x0y0;
    int y_pred2_hle16;
    int y_pred2_hgt16;
} HEVCRpiStats;
#endif

#endif

typedef struct HEVCContext {
    const AVClass *c;  // needed by private avoptions
    AVCodecContext *avctx;

    struct HEVCContext  *sList[MAX_NB_THREADS];

    HEVCLocalContext    *HEVClcList[MAX_NB_THREADS];
    HEVCLocalContext    *HEVClc;

    uint8_t             threads_type;
    uint8_t             threads_number;

    int                 width;
    int                 height;

    char used_for_ref;  // rpi
#if CONFIG_HEVC_RPI_DECODER
    char offload_recon;
    char enable_rpi;

    HEVCRpiJobCtl * jbc;

    // Function pointers
#if RPI_QPU_EMU_Y || RPI_QPU_EMU_C
    const uint8_t * qpu_dummy_frame_emu;
#endif
#if !RPI_QPU_EMU_Y || !RPI_QPU_EMU_C
    uint32_t qpu_dummy_frame_qpu;  // Not a frame - just a bit of memory
#endif
    HEVCRpiQpu qpu;

    HEVCRpiFrameProgressState progress_states[2];

#ifdef RPI_DEBLOCK_VPU
// With the new scheme of rpi_execute_dblk_cmds 
// it looks like ff_hevc_hls_filter is no longer called in raster order.
// This causes trouble if RPI_DEBLOCK_VPU_Q_COUNT > 1 because we prepare setup
// data for more than one row at a time before triggering the deblocker for one row.
// This means that the deblock of the final row can use the wrong setup buffer.
// 
// Also concerned that the thread progress and waiting for job completion is
// not done correctly with RPI_DEBLOCK_VPU at the end of the frame, or for small CTU sizes.
#define RPI_DEBLOCK_VPU_Q_COUNT 1

    int enable_rpi_deblock;

    int uv_setup_width;
    int uv_setup_height;
    int setup_width; // Number of 16x16 blocks across the image
    int setup_height; // Number of 16x16 blocks down the image

    struct dblk_vpu_q_s
    {
        GPU_MEM_PTR_T deblock_vpu_gmem;

        uint8_t (*y_setup_arm)[2][2][2][4];
        uint8_t (*y_setup_vc)[2][2][2][4];

        uint8_t (*uv_setup_arm)[2][2][2][4];
        uint8_t (*uv_setup_vc)[2][2][2][4];

        int (*vpu_cmds_arm)[6]; // r0-r5 for each command
        int vpu_cmds_vc;

        vpu_qpu_wait_h cmd_id;
    } dvq_ents[RPI_DEBLOCK_VPU_Q_COUNT];

    struct dblk_vpu_q_s * dvq;
    unsigned int dvq_n;
#endif
#endif  // RPI

    uint8_t *cabac_state;

    /** 1 if the independent slice segment header was successfully parsed */
    uint8_t slice_initialized;

    AVFrame *frame;
    AVFrame *output_frame;
    uint8_t *sao_pixel_buffer_h[3];
    uint8_t *sao_pixel_buffer_v[3];

    HEVCParamSets ps;

    AVBufferPool *tab_mvf_pool;
    AVBufferPool *rpl_tab_pool;

    ///< candidate references for the current frame
    RefPicList rps[5];

    SliceHeader sh;
    SAOParams *sao;
    DBParams *deblock;
    enum HEVCNALUnitType nal_unit_type;
    int temporal_id;  ///< temporal_id_plus1 - 1
    HEVCFrame *ref;
    HEVCFrame DPB[HEVC_DPB_ELS];
    int poc;
    int pocTid0;
    int slice_idx; ///< number of the slice being currently decoded
    int eos;       ///< current packet contains an EOS/EOB NAL
    int last_eos;  ///< last packet contains an EOS/EOB NAL
    int max_ra;
    int bs_width;
    int bs_height;

    int is_decoded;
    int no_rasl_output_flag;

    HEVCPredContext hpc;
    HEVCDSPContext hevcdsp;
    VideoDSPContext vdsp;
    BswapDSPContext bdsp;
    int8_t *qp_y_tab;
    uint8_t *horizontal_bs;
    uint8_t *vertical_bs;

    int32_t *tab_slice_address;

    //  CU
    uint8_t *skip_flag;
    uint8_t *tab_ct_depth;
    // PU
    uint8_t *tab_ipm;

    uint8_t *cbf_luma; // cbf_luma of colocated TU
    uint8_t *is_pcm;

    // CTB-level flags affecting loop filter operation
    uint8_t *filter_slice_edges;

    /** used on BE to byteswap the lines for checksumming */
    uint8_t *checksum_buf;
    int      checksum_buf_size;

    /**
     * Sequence counters for decoded and output frames, so that old
     * frames are output first after a POC reset
     */
    uint16_t seq_decode;
    uint16_t seq_output;

    int enable_parallel_tiles;
    atomic_int wpp_err;

    const uint8_t *data;

    H2645Packet pkt;
    // type of the first VCL NAL of the current frame
    enum HEVCNALUnitType first_nal_type;

    uint8_t context_initialized;
    int is_nalff;           ///< this flag is != 0 if bitstream is encapsulated
                            ///< as a format defined in 14496-15
    int apply_defdispwin;

    int nal_length_size;    ///< Number of bytes used for nal length (1, 2 or 4)
    int nuh_layer_id;

    HEVCSEIContext sei;

#if CONFIG_HEVC_RPI_DECODER
    // Put structures that allocate non-trivial storage at the end
    // These are mostly used indirectly so position in the structure doesn't matter
    HEVCLocalContextIntra HEVClcIntra;
    HEVCRpiPassQueue passq[RPI_PASSES];
#if RPI_EXTRA_BIT_THREADS > 0
    int bt_started;
    // This simply contains thread descriptors - task setup is held elsewhere
    pthread_t bit_threads[RPI_EXTRA_BIT_THREADS];
#endif
#if RPI_TSTATS
    HEVCRpiStats tstats;
#endif
#endif  // RPI
} HEVCContext;

/**
 * Mark all frames in DPB as unused for reference.
 */
void ff_hevc_clear_refs(HEVCContext *s);

/**
 * Drop all frames currently in DPB.
 */
void ff_hevc_flush_dpb(HEVCContext *s);

const RefPicList *ff_hevc_get_ref_list(const HEVCContext * const s, const HEVCFrame * const ref,
                                 int x0, int y0);

/**
 * Construct the reference picture sets for the current frame.
 */
int ff_hevc_frame_rps(HEVCContext *s);

/**
 * Construct the reference picture list(s) for the current slice.
 */
int ff_hevc_slice_rpl(HEVCContext *s);

void ff_hevc_save_states(HEVCContext *s, const HEVCLocalContext * const lc, int ctb_addr_ts);
int ff_hevc_cabac_init(const HEVCContext * const s, HEVCLocalContext *const lc, int ctb_addr_ts);
int ff_hevc_sao_merge_flag_decode(HEVCLocalContext * const lc);
int ff_hevc_sao_type_idx_decode(HEVCLocalContext * const lc);
int ff_hevc_sao_band_position_decode(HEVCLocalContext * const lc);
int ff_hevc_sao_offset_abs_decode(const HEVCContext * const s, HEVCLocalContext * const lc);
int ff_hevc_sao_offset_sign_decode(HEVCLocalContext * const lc);
int ff_hevc_sao_eo_class_decode(HEVCLocalContext * const lc);
int ff_hevc_end_of_slice_flag_decode(HEVCLocalContext * const lc);
int ff_hevc_cu_transquant_bypass_flag_decode(HEVCLocalContext * const lc);
int ff_hevc_skip_flag_decode(const HEVCContext * const s, HEVCLocalContext * const lc,
                             const int x0, const int y0, const int x_cb, const int y_cb);
int ff_hevc_pred_mode_decode(HEVCLocalContext * const lc);
int ff_hevc_split_coding_unit_flag_decode(const HEVCContext * const s, HEVCLocalContext * const lc, const int ct_depth,
                                          const int x0, const int y0);
int ff_hevc_part_mode_decode(const HEVCContext * const s, HEVCLocalContext * const lc, const int log2_cb_size);
int ff_hevc_pcm_flag_decode(HEVCLocalContext * const lc);
int ff_hevc_prev_intra_luma_pred_flag_decode(HEVCLocalContext * const lc);
int ff_hevc_mpm_idx_decode(HEVCLocalContext * const lc);
int ff_hevc_rem_intra_luma_pred_mode_decode(HEVCLocalContext * const lc);
int ff_hevc_intra_chroma_pred_mode_decode(HEVCLocalContext * const lc);
int ff_hevc_merge_idx_decode(const HEVCContext * const s, HEVCLocalContext * const lc);
int ff_hevc_merge_flag_decode(HEVCLocalContext * const lc);
int ff_hevc_inter_pred_idc_decode(HEVCLocalContext * const lc, int nPbW, int nPbH);
int ff_hevc_ref_idx_lx_decode(HEVCLocalContext * const lc, const int num_ref_idx_lx);
int ff_hevc_mvp_lx_flag_decode(HEVCLocalContext * const lc);
int ff_hevc_no_residual_syntax_flag_decode(HEVCLocalContext * const lc);
int ff_hevc_split_transform_flag_decode(HEVCLocalContext * const lc, const int log2_trafo_size);
int ff_hevc_cbf_cb_cr_decode(HEVCLocalContext * const lc, const int trafo_depth);
int ff_hevc_cbf_luma_decode(HEVCLocalContext * const lc, const int trafo_depth);
int ff_hevc_log2_res_scale_abs(HEVCLocalContext * const lc, const int idx);
int ff_hevc_res_scale_sign_flag(HEVCLocalContext *const lc, const int idx);

/**
 * Get the number of candidate references for the current frame.
 */
int ff_hevc_frame_nb_refs(HEVCContext *s);

int ff_hevc_set_new_ref(HEVCContext *s, AVFrame **frame, int poc);

/**
 * Find next frame in output order and put a reference to it in frame.
 * @return 1 if a frame was output, 0 otherwise
 */
int ff_hevc_output_frame(HEVCContext *s, AVFrame *frame, int flush);

void ff_hevc_bump_frame(HEVCContext *s);

void ff_hevc_unref_frame(HEVCContext *s, HEVCFrame *frame, int flags);

void ff_hevc_set_neighbour_available(const HEVCContext * const s, HEVCLocalContext * const lc, const int x0, const int y0,
                                     const int nPbW, const int nPbH);
void ff_hevc_luma_mv_merge_mode(const HEVCContext * const s, HEVCLocalContext * const lc, int x0, int y0, int nPbW,
                                int nPbH, int log2_cb_size, int part_idx,
                                int merge_idx, MvField * const mv);
void ff_hevc_luma_mv_mvp_mode(const HEVCContext * const s, HEVCLocalContext *lc, int x0, int y0, int nPbW,
                              int nPbH, int log2_cb_size, int part_idx,
                              int merge_idx, MvField * const mv,
                              int mvp_lx_flag, int LX);
void ff_hevc_set_qPy(const HEVCContext * const s, HEVCLocalContext * const lc, int xBase, int yBase, int log2_cb_size);
void ff_hevc_deblocking_boundary_strengths(const HEVCContext * const s, HEVCLocalContext * const lc, int x0, int y0,
                                           int log2_trafo_size);
int ff_hevc_cu_qp_delta_sign_flag(HEVCLocalContext * const lc);
int ff_hevc_cu_qp_delta_abs(HEVCLocalContext * const lc);
int ff_hevc_cu_chroma_qp_offset_flag(HEVCLocalContext * const lc);
int ff_hevc_cu_chroma_qp_offset_idx(const HEVCContext * const s, HEVCLocalContext * const lc);
void ff_hevc_hls_filter(HEVCContext * const s, const int x, const int y, const int ctb_size);
void ff_hevc_hls_filters(HEVCContext *s, int x_ctb, int y_ctb, int ctb_size);
void ff_hevc_hls_residual_coding(const HEVCContext * const s, HEVCLocalContext * const lc,
                                const int x0, const int y0,
                                const int log2_trafo_size, const enum ScanType scan_idx,
                                const int c_idx);

void ff_hevc_hls_mvd_coding(HEVCLocalContext * const lc);


extern const uint8_t ff_hevc_qpel_extra_before[4];
extern const uint8_t ff_hevc_qpel_extra_after[4];
extern const uint8_t ff_hevc_qpel_extra[4];

#if CONFIG_HEVC_RPI_DECODER
int16_t * rpi_alloc_coeff_buf(HEVCRpiJob * const jb, const int buf_no, const int n);

// arm/hevc_misc_neon.S
// Neon coeff zap fn
#if HAVE_NEON
extern void rpi_zap_coeff_vals_neon(int16_t * dst, unsigned int l2ts_m2);
#endif

void ff_hevc_rpi_progress_wait_field(const HEVCContext * const s, HEVCRpiJob * const jb,
                                     const HEVCFrame * const ref, const int val, const int field);

void ff_hevc_rpi_progress_signal_field(HEVCContext * const s, const int val, const int field);

// All of these expect that s->threads_type == FF_THREAD_FRAME

static inline void ff_hevc_progress_wait_mv(const HEVCContext * const s, HEVCRpiJob * const jb,
                                     const HEVCFrame * const ref, const int y)
{
    if (s->enable_rpi)
        ff_hevc_rpi_progress_wait_field(s, jb, ref, y, 1);
    else
        ff_thread_await_progress((ThreadFrame*)&ref->tf, y, 0);
}

static inline void ff_hevc_progress_signal_mv(HEVCContext * const s, const int y)
{
    if (s->enable_rpi && s->used_for_ref)
        ff_hevc_rpi_progress_signal_field(s, y, 1);
}

static inline void ff_hevc_progress_wait_recon(const HEVCContext * const s, HEVCRpiJob * const jb,
                                     const HEVCFrame * const ref, const int y)
{
    if (s->enable_rpi)
        ff_hevc_rpi_progress_wait_field(s, jb, ref, y, 0);
    else
        ff_thread_await_progress((ThreadFrame*)&ref->tf, y, 0);
}

static inline void ff_hevc_progress_signal_recon(HEVCContext * const s, const int y)
{
    if (s->used_for_ref)
    {
        if (s->enable_rpi)
            ff_hevc_rpi_progress_signal_field(s, y, 0);
        else
            ff_thread_report_progress(&s->ref->tf, y, 0);
    }
}

static inline void ff_hevc_progress_signal_all_done(HEVCContext * const s)
{
    if (s->enable_rpi)
    {
        ff_hevc_rpi_progress_signal_field(s, INT_MAX, 0);
        ff_hevc_rpi_progress_signal_field(s, INT_MAX, 1);
    }
    else
        ff_thread_report_progress(&s->ref->tf, INT_MAX, 0);
}

#else

// Use #define as that allows us to discard "jb" which won't exist in non-RPI world
#define ff_hevc_progress_wait_mv(s, jb, ref, y) ff_thread_await_progress((ThreadFrame *)&ref->tf, y, 0)
#define ff_hevc_progress_wait_recon(s, jb, ref, y) ff_thread_await_progress((ThreadFrame *)&ref->tf, y, 0)
#define ff_hevc_progress_signal_mv(s, y)
#define ff_hevc_progress_signal_recon(s, y) ff_thread_report_progress(&s->ref->tf, y, 0)
#define ff_hevc_progress_signal_all_done(s) ff_thread_report_progress(&s->ref->tf, INT_MAX, 0)

#endif

// Set all done - signal nothing (used in missing refs)
// Works for both rpi & non-rpi
static inline void ff_hevc_progress_set_all_done(HEVCFrame * const ref)
{
    if (ref->tf.progress != NULL)
    {
        int * const p = (int *)&ref->tf.progress->data;
        p[0] = INT_MAX;
        p[1] = INT_MAX;
    }
}

#endif /* AVCODEC_RPI_HEVCDEC_H */
