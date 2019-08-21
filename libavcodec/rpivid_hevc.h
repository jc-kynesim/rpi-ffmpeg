// FFMPEG HEVC decoder hardware accelerator
// Andrew Holme, Argon Design Ltd
// Copyright (c) June 2017 Raspberry Pi Ltd

#include <stdio.h>
#include <pthread.h>

#include "hevc.h"
#include "hevcdec.h"

#define MAX_THREADS 50
#define NUM_SCALING_FACTORS 4064

#define AXI_BASE64 0

#define PROB_BACKUP ((20<<12) + (20<<6) + (0<<0))
#define PROB_RELOAD ((20<<12) + (20<<0) + (0<<6))

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

typedef struct RPI_T {
struct RPI_BIT *bit_fifo;
struct RPI_CMD *cmd_fifo;
    int         bit_len, bit_max;
    int         cmd_len, cmd_max;
    int         max_pu_msgs;
    int         max_coeff64;
AVCodecContext *thread_avctx[MAX_THREADS];
    int         thread_order[MAX_THREADS];
    int         decode_order;
    int         phase1_order;
    int         phase2_order;
pthread_mutex_t mutex_phase1;
pthread_mutex_t mutex_phase2;
    uint8_t     scaling_factors[NUM_SCALING_FACTORS];
struct RPI_PROB probabilities;
    int         num_slice_msgs;
    uint16_t    slice_msgs[2*HEVC_MAX_REFS*8+3];
    int         pubase64[MAX_THREADS];
    int         pustep64;
    int         coeffbase64[MAX_THREADS];
    int         coeffstep64;
    int         PicWidthInCtbsY;
    int         PicHeightInCtbsY;
#ifdef AXI_BUFFERS
    int         lumabytes64;
    int         framebytes64;
    int         lumastride64;
    int         chromastride64;
#endif
    int         mvframebytes64;
    int         mvstorage64;
    int         colstride64;
    int         mvstride64;
    int         colbase64[MAX_THREADS];
    int         mvbase64[MAX_THREADS];
    uint32_t    reg_slicestart;
    int         collocated_from_l0_flag;
    int         max_num_merge_cand;
    int         RefPicList[2][HEVC_MAX_REFS];
    int         collocated_ref_idx;
    int         wpp_entry_x;
    int         wpp_entry_y;

    void *      dl_handle;
    void *      id;
    char *   (* ctrl_ffmpeg_init) (const char *hwaccel_device, void **id);
    void     (* apb_write)        (void *id, uint16_t addr, uint32_t data);
    void     (* apb_write_addr)   (void *id, uint16_t addr, uint32_t data);
    uint32_t (* apb_read)         (void *id, uint16_t addr);
    void     (* apb_read_drop)    (void *id, uint16_t addr);
    void     (* axi_write)        (void *id, uint64_t addr, uint32_t size, const void *buf);
    void     (* axi_read_alloc)   (void *id, uint32_t size);
    void     (* axi_read_tx)      (void *id, uint64_t addr, uint32_t size);
    void     (* axi_read_rx)      (void *id, uint32_t size, void *buf);
    uint64_t (* axi_get_addr)     (void *id);
    void     (* apb_dump_regs)    (void *id, uint16_t addr, int num);
    void     (* axi_dump)         (void *id, uint64_t addr, uint32_t size);
    void     (* axi_flush)        (void *id, int mode);
    void     (* wait_interrupt)   (void *id, int phase);
    void     (* ctrl_ffmpeg_free) (void *id);

} RPI_T;
