// FFMPEG HEVC decoder hardware accelerator
// Andrew Holme, Argon Design Ltd
// Copyright (c) June 2017 Raspberry Pi Ltd

#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>

#include "hevc.h"
#include "hevcdec.h"
#include "rpi_mem.h"

#define MAX_THREADS 50
#define NUM_SCALING_FACTORS 4064

#define AXI_BASE64 0

#define PROB_BACKUP ((20<<12) + (20<<6) + (0<<0))
#define PROB_RELOAD ((20<<12) + (20<<0) + (0<<6))

#define RPIVID_COL_PICS 17  // 16 ref & current

#define RPIVID_BITBUFS          2          // Bit + Cmd bufs (phase 0 & 1)
#define RPIVID_BITBUF_SIZE      (4 << 20)  // Bit + Cmd buf size

#define RPIVID_COEFFBUFS        3          // PU + Coeff bufs (phase 1 & 2)
#define RPIVID_COEFFBUF_SIZE    (16 << 20) // PU + Coeff buf size

//////////////////////////////////////////////////////////////////////////////

#define RPI_SPS0         0
#define RPI_SPS1         4
#define RPI_PPS          8
#define RPI_SLICE        12
#define RPI_TILESTART    16
#define RPI_TILEEND      20
#define RPI_SLICESTART   24
#define RPI_MODE         28
#define RPI_LEFT0        32
#define RPI_LEFT1        36
#define RPI_LEFT2        40
#define RPI_LEFT3        44
#define RPI_QP           48
#define RPI_CONTROL      52
#define RPI_STATUS       56
#define RPI_VERSION      60
#define RPI_BFBASE       64
#define RPI_BFNUM        68
#define RPI_BFCONTROL    72
#define RPI_BFSTATUS     76
#define RPI_PUWBASE      80
#define RPI_PUWSTRIDE    84
#define RPI_COEFFWBASE   88
#define RPI_COEFFWSTRIDE 92
#define RPI_SLICECMDS    96
#define RPI_BEGINTILEEND 100
#define RPI_TRANSFER     104
#define RPI_CFBASE       108
#define RPI_CFNUM        112
#define RPI_CFSTATUS     116

#define RPI_PURBASE       0x8000
#define RPI_PURSTRIDE     0x8004
#define RPI_COEFFRBASE    0x8008
#define RPI_COEFFRSTRIDE  0x800C
#define RPI_NUMROWS       0x8010
#define RPI_CONFIG2       0x8014
#define RPI_OUTYBASE      0x8018
#define RPI_OUTYSTRIDE    0x801C
#define RPI_OUTCBASE      0x8020
#define RPI_OUTCSTRIDE    0x8024
#define RPI_STATUS2       0x8028
#define RPI_FRAMESIZE     0x802C
#define RPI_MVBASE        0x8030
#define RPI_MVSTRIDE      0x8034
#define RPI_COLBASE       0x8038
#define RPI_COLSTRIDE     0x803C
#define RPI_CURRPOC       0x8040

//////////////////////////////////////////////////////////////////////////////

struct FFM_PROB {
    uint8_t  sao_merge_flag                   [ 1];
    uint8_t  sao_type_idx                     [ 1];
    uint8_t  split_coding_unit_flag           [ 3];
    uint8_t  cu_transquant_bypass_flag        [ 1];
    uint8_t  skip_flag                        [ 3];
    uint8_t  cu_qp_delta                      [ 3];
    uint8_t  pred_mode_flag                   [ 1];
    uint8_t  part_mode                        [ 4];
    uint8_t  prev_intra_luma_pred_flag        [ 1];
    uint8_t  intra_chroma_pred_mode           [ 2];
    uint8_t  merge_flag                       [ 1];
    uint8_t  merge_idx                        [ 1];
    uint8_t  inter_pred_idc                   [ 5];
    uint8_t  ref_idx_l0                       [ 2];
    uint8_t  ref_idx_l1                       [ 2];
    uint8_t  abs_mvd_greater0_flag            [ 2];
    uint8_t  abs_mvd_greater1_flag            [ 2];
    uint8_t  mvp_lx_flag                      [ 1];
    uint8_t  no_residual_data_flag            [ 1];
    uint8_t  split_transform_flag             [ 3];
    uint8_t  cbf_luma                         [ 2];
    uint8_t  cbf_cb_cr                        [ 4];
    uint8_t  transform_skip_flag/*[][]*/      [ 2];
    uint8_t  explicit_rdpcm_flag/*[][]*/      [ 2];
    uint8_t  explicit_rdpcm_dir_flag/*[][]*/  [ 2];
    uint8_t  last_significant_coeff_x_prefix  [18];
    uint8_t  last_significant_coeff_y_prefix  [18];
    uint8_t  significant_coeff_group_flag     [ 4];
    uint8_t  significant_coeff_flag           [44];
    uint8_t  coeff_abs_level_greater1_flag    [24];
    uint8_t  coeff_abs_level_greater2_flag    [ 6];
    uint8_t  log2_res_scale_abs               [ 8];
    uint8_t  res_scale_sign_flag              [ 2];
    uint8_t  cu_chroma_qp_offset_flag         [ 1];
    uint8_t  cu_chroma_qp_offset_idx          [ 1];
} __attribute__((packed));

//////////////////////////////////////////////////////////////////////////////

struct RPI_PROB {
    uint8_t  SAO_MERGE_FLAG             [ 1];
    uint8_t  SAO_TYPE_IDX               [ 1];
    uint8_t  SPLIT_FLAG                 [ 3];
    uint8_t  CU_SKIP_FLAG               [ 3];
    uint8_t  CU_TRANSQUANT_BYPASS_FLAG  [ 1];
    uint8_t  PRED_MODE                  [ 1];
    uint8_t  PART_SIZE                  [ 4];
    uint8_t  INTRA_PRED_MODE            [ 1];
    uint8_t  CHROMA_PRED_MODE           [ 1];
    uint8_t  MERGE_FLAG_EXT             [ 1];
    uint8_t  MERGE_IDX_EXT              [ 1];
    uint8_t  INTER_DIR                  [ 5];
    uint8_t  REF_PIC                    [ 2];
    uint8_t  MVP_IDX                    [ 1];
    uint8_t  MVD                        [ 2];
    uint8_t  QT_ROOT_CBF                [ 1];
    uint8_t  TRANS_SUBDIV_FLAG          [ 3];
    uint8_t  QT_CBF                     [ 6];
    uint8_t  DQP                        [ 2];
    uint8_t  ONE_FLAG                   [24];
    uint8_t  LASTX                      [18];
    uint8_t  LASTY                      [18];
    uint8_t  SIG_CG_FLAG                [ 4];
    uint8_t  ABS_FLAG                   [ 6];
    uint8_t  TRANSFORMSKIP_FLAG         [ 2];
    uint8_t  SIG_FLAG                   [42];
    uint8_t  SIG_FLAG_unused            [ 2];
} __attribute__((packed));

//////////////////////////////////////////////////////////////////////////////

struct RPI_CMD {
    uint32_t addr;
    uint32_t data;
} __attribute__((packed));

struct RPI_BIT {
    int         cmd;
    const void *ptr;
    int         len;
};

//////////////////////////////////////////////////////////////////////////////

struct RPI_T;

// Actual addressability is 38bits but we can only alloc in the bottom 32
// currently - when passed to rpivid h/w the address is always >> 6 so will
// fit in 32 bit there
// At some point we may weant to make this uint64_t
typedef uint32_t vid_vc_addr_t;

typedef enum rpivid_decode_state_e {
    RPIVID_DECODE_NEW = 0,
    RPIVID_DECODE_START,
    RPIVID_DECODE_SLICE,
    RPIVID_DECODE_END,
} rpivid_decode_state_t;

typedef struct dec_env_s {
    const AVCodecContext * avctx;

    rpivid_decode_state_t state;
    int phase_no;
    struct dec_env_s * phase_next;
    sem_t phase_wait;
    struct RPI_BIT *bit_fifo;
    struct RPI_CMD *cmd_fifo;
    unsigned int bit_len, bit_max;
    int         cmd_len, cmd_max;
    int         max_pu_msgs;
    int         max_coeff64;
    int         decode_order;
    uint8_t     scaling_factors[NUM_SCALING_FACTORS];
struct RPI_PROB probabilities;
    int         num_slice_msgs;
    uint16_t    slice_msgs[2*HEVC_MAX_REFS*8+3];
    unsigned int PicWidthInCtbsY;
    unsigned int PicHeightInCtbsY;
    unsigned int dpbno_col;
    uint32_t    reg_slicestart;
    int         collocated_from_l0_flag;
    int         max_num_merge_cand;
    int         RefPicList[2][HEVC_MAX_REFS];
    int         collocated_ref_idx;
    int         wpp_entry_x;
    int         wpp_entry_y;
} dec_env_t;

#define RPIVID_PHASES 3
#define RPIVID_PHASE_NEW (RPIVID_PHASES) // Phase before we have inced decode order
#define RPIVID_PHASE_START (-1)          // Phase after we have inced decode_order

typedef struct phase_wait_env_s {
    unsigned int last_seq;
    dec_env_t * q;
} phase_wait_env_t;

typedef struct RPI_T {
    atomic_int ref_count;
    sem_t ref_zero;

    dec_env_t ** dec_envs;

    pthread_mutex_t phase_lock;
    phase_wait_env_t phase_reqs[RPIVID_PHASES];

    volatile uint32_t * regs;
    volatile uint32_t * ints;

    GPU_MEM_PTR_T gcolbuf;
    unsigned int col_stride;
    size_t      col_picsize;

    unsigned int bitbuf_no;
    sem_t       bitbuf_sem;
    GPU_MEM_PTR_T gbitbufs[RPIVID_BITBUFS];

    unsigned int coeffbuf_no;
    sem_t       coeffbuf_sem;
    GPU_MEM_PTR_T gcoeffbufs[RPIVID_COEFFBUFS];

    int         decode_order;
} RPI_T;
