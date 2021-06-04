// FFMPEG HEVC decoder hardware accelerator
// Andrew Holme, Argon Design Ltd
// Copyright (c) June 2017 Raspberry Pi Ltd

#include <stdio.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/mman.h>

#include "fftools/ffmpeg.h"
#include "libavutil/avassert.h"
#include "libavutil/imgutils.h"
#include "avcodec.h"
#include "hwconfig.h"
#include "decode.h"

#include "hevc.h"
#include "hevcdec.h"
#include "rpi_zc.h"
#include "rpi_mem.h"
#include "rpi_zc_frames.h"
#include "rpi_mailbox.h"


#define OPT_PHASE_TIMING 0      // Generate stats for phase usage

#define OPT_EMU 0

#define TRACE_DEV 0
#define TRACE_ENTRY 0

#define NUM_SCALING_FACTORS 4064

#define AXI_BASE64 0

#define PROB_BACKUP ((20<<12) + (20<<6) + (0<<0))
#define PROB_RELOAD ((20<<12) + (20<<0) + (0<<6))

#define RPIVID_COL_PICS 17                 // 16 ref & current

#define RPIVID_BITBUFS          2          // Bit + Cmd bufs (phase 0 & 1)
#define RPIVID_BITBUF_SIZE      (4 << 20)  // Bit + Cmd buf size

#define RPIVID_COEFFBUFS        3          // PU + Coeff bufs (phase 1 & 2)
#define RPIVID_COEFFBUF_SIZE    (16 << 20) // PU + Coeff buf size

//////////////////////////////////////////////////////////////////////////////
//
// Register offsets

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

// Unused but left here to illustrate the diffrences between FFmpegs prob
// structure and the rpivid one

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

#define RPI_PROB_VALS 154U
#define RPI_PROB_ARRAY_SIZE ((154 + 3) & ~3)

typedef struct dec_env_s {
    const AVCodecContext * avctx;

    rpivid_decode_state_t state;
    unsigned int    decode_order;

    int             phase_no;           // Current phase (i.e. the last one we waited for)
    struct dec_env_s * phase_wait_q_next;
    sem_t           phase_wait;

    struct RPI_BIT *bit_fifo;
    struct RPI_CMD *cmd_fifo;
    unsigned int    bit_len, bit_max;
    unsigned int    cmd_len, cmd_max;
    unsigned int    num_slice_msgs;
    unsigned int    PicWidthInCtbsY;
    unsigned int    PicHeightInCtbsY;
    unsigned int    dpbno_col;
    uint32_t        reg_slicestart;
    unsigned int    wpp_entry_x;
    unsigned int    wpp_entry_y;

    const uint8_t * nal_buffer;
    size_t          nal_size;

    uint16_t        slice_msgs[2*HEVC_MAX_REFS*8+3];
    uint8_t         scaling_factors[NUM_SCALING_FACTORS];
//    unsigned int    RefPicList[2][HEVC_MAX_REFS];
} dec_env_t;

#define RPIVID_PHASES 3
#define RPIVID_PHASE_NEW (RPIVID_PHASES) // Phase before we have inced decode order
#define RPIVID_PHASE_START (-1)          // Phase after we have inced decode_order

#if OPT_PHASE_TIMING
static const unsigned int time_thresholds[8] = {
    10, 15, 20, 30, 45, 60, 75, 90
};
#endif

typedef struct phase_wait_env_s {
    unsigned int    last_order;
    dec_env_t *     q;
#if OPT_PHASE_TIMING
    uint64_t phase_time;
    uint64_t max_phase_time;
    uint64_t time_in_phase;
    uint64_t time_out_phase;
    unsigned int max_time_decode_order;
    unsigned int time_bins[9];
    unsigned int time_bins3[9];
    unsigned int time_bins5[9];
    uint64_t time_stash[16];
    unsigned int i3;
#endif
} phase_wait_env_t;                      // Single linked list of threads waiting for this phase

typedef struct RPI_T {
    atomic_int      ref_count;
    sem_t           ref_zero;

    dec_env_t **    dec_envs;
    AVZcEnvPtr      zc;

    pthread_mutex_t phase_lock;
    phase_wait_env_t phase_reqs[RPIVID_PHASES];

    volatile uint32_t * regs;
    volatile uint32_t * ints;

    GPU_MEM_PTR_T   gcolbuf;
    unsigned int    col_stride;
    size_t          col_picsize;

    unsigned int    bitbuf_no;
    sem_t           bitbuf_sem;
    GPU_MEM_PTR_T   gbitbufs[RPIVID_BITBUFS];

    unsigned int    max_pu_msgs;
    unsigned int    coeffbuf_no;
    sem_t           coeffbuf_sem;
    GPU_MEM_PTR_T   gcoeffbufs[RPIVID_COEFFBUFS];

    unsigned int    decode_order;
    int             mbox_fd;
    int             gpu_init_type;
} RPI_T;

#if OPT_PHASE_TIMING
static uint64_t tus64(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}
#endif

static inline unsigned int rnd64(unsigned int x)
{
    return (x + 63) & ~63;
}

static inline int rpi_sem_wait(sem_t * const sem)
{
    int rv;
    while ((rv = sem_wait(sem)) != 0 && errno == EINTR)
        /* Loop */;
    return rv;
}

//============================================================================

#define REGS_NAME "/dev/rpivid-hevcmem"
#define REGS_SIZE 0x10000
#define INTS_NAME "/dev/rpivid-intcmem"
#define INTS_SIZE 0x10000  // 4 is probably enough but we are going to alloc a page anyway

static volatile uint32_t * map_dev(AVCodecContext * const avctx, const char * const dev_name, size_t size)
{
    void *gpio_map;
    int  mem_fd;

    /* open /dev/mem */
    if ((mem_fd = open(dev_name, O_RDWR|O_SYNC) ) < 0) {
        av_log(avctx, AV_LOG_WARNING, "can't open %s\n", dev_name);
        return NULL;
    }

    // Now map it
    gpio_map = mmap(
       NULL,
       size,
       PROT_READ|PROT_WRITE,
       MAP_SHARED,
       mem_fd,
       0
    );

    close(mem_fd);  // No longer need the FD

    if (gpio_map == MAP_FAILED) {
        av_log(avctx, AV_LOG_WARNING, "GPIO mapping failed");
        return NULL;
    }

    return (volatile uint32_t *)gpio_map;
}

static void unmap_devp(volatile uint32_t ** const p_gpio_map, size_t size)
{
    volatile uint32_t * const gpio_map = *p_gpio_map;
    if (gpio_map != NULL) {
        *p_gpio_map = NULL;
        munmap((void *)gpio_map, size);
    }
}

#define MANGLE(x) ((x) &~0xc0000000)          // ** If x is ever a 64 bit thing this will need fixing!
#define MANGLE64(x) (uint32_t)(MANGLE(x)>>6)

static inline void apb_write_vc_addr(const RPI_T *const rpi, const uint32_t addr, const vid_vc_addr_t data)
{
#if TRACE_DEV
    printf("W %x %08x\n", addr, MANGLE64(data));
#endif

    rpi->regs[addr >> 2] = MANGLE64(data);
}

static inline void apb_write_vc_len(const RPI_T *const rpi, const uint32_t addr, const unsigned int data)
{
#if TRACE_DEV
    printf("W %x %08x\n", addr, data >> 6);
#endif

    rpi->regs[addr >> 2] = data >> 6;  // ?? rnd64 - but not currently needed
}

static inline void apb_write(const RPI_T * const rpi, const uint32_t addr, const uint32_t data)
{
#if TRACE_DEV
    printf("W %x %08x\n", addr, data);
#endif

    rpi->regs[addr >> 2] = data;
}

static inline uint32_t apb_read(const RPI_T * const rpi, const uint32_t addr)
{
    const uint32_t v = rpi->regs[addr >> 2];
#if TRACE_DEV
    printf("R %x (=%x)\n", addr, v);
#endif
    return v;
}

#define ARG_IC_ICTRL_ACTIVE1_INT_SET                   0x00000001
#define ARG_IC_ICTRL_ACTIVE1_EDGE_SET                  0x00000002
#define ARG_IC_ICTRL_ACTIVE1_EN_SET                    0x00000004
#define ARG_IC_ICTRL_ACTIVE1_STATUS_SET                0x00000008
#define ARG_IC_ICTRL_ACTIVE2_INT_SET                   0x00000010
#define ARG_IC_ICTRL_ACTIVE2_EDGE_SET                  0x00000020
#define ARG_IC_ICTRL_ACTIVE2_EN_SET                    0x00000040
#define ARG_IC_ICTRL_ACTIVE2_STATUS_SET                0x00000080

static inline void int_wait(const RPI_T * const rpi, const unsigned int phase)
{
    const uint32_t mask_reset = phase == 1 ? ~ARG_IC_ICTRL_ACTIVE2_INT_SET : ~ARG_IC_ICTRL_ACTIVE1_INT_SET;
    const uint32_t mask_done = phase == 1 ? ARG_IC_ICTRL_ACTIVE1_INT_SET : ARG_IC_ICTRL_ACTIVE2_INT_SET;
    uint32_t ival;
    while (((ival = rpi->ints[0]) & mask_done) == 0) {
        usleep(1000);
    }
    rpi->ints[0] = ival & mask_reset;
}

#if TRACE_DEV && 0
static void apb_dump_regs(const RPI_T * const rpi, uint16_t addr, int num) {
    int i;

    for (i=0; i<num; i++)
    {
        if ((i%4)==0)
          printf("%08x: ", 0x7eb00000 + addr + 4*i);

        printf("%08x", rpi->regs[(addr>>2)+i]);

        if ((i%4)==3 || i+1 == num)
            printf("\n");
        else
            printf(" ");
    }
}

static void axi_dump(const dec_env_t * const de, uint64_t addr, uint32_t size) {
    int i;

    for (i=0; i<size>>2; i++)
    {
        if ((i%4)==0)
            printf("%08x: ", MANGLE(de->gbuf.vc) + (uint32_t)addr + 4*i);

        printf("%08x", ((uint32_t*)de->gbuf.arm)[(addr>>2)+i]);

        if ((i%4)==3 || i+1 == size>>2)
            printf("\n");
        else
            printf(" ");
    }
}
#endif

//////////////////////////////////////////////////////////////////////////////

static inline size_t round_up_size(const size_t x)
{
    /* Admit no size < 256 */
    const unsigned int n = x < 256 ? 8 : av_log2(x) - 1;

    return x >= (3 << n) ? 4 << n : (3 << n);
}

//////////////////////////////////////////////////////////////////////////////
// Scaling factors

static void expand_scaling_list(
    const unsigned int sizeID,
    const unsigned int matrixID,
    uint8_t * const dst0,
    const uint8_t * const src0,
    uint8_t dc)
{
    switch (sizeID) {
        case 0:
            memcpy(dst0, src0, 16);
            break;
        case 1:
            memcpy(dst0, src0, 64);
            break;
        case 2:
        {
            uint8_t * d = dst0;
            for (unsigned int y=0; y != 16; y++) {
                const uint8_t * s = src0 + (y >> 1) * 8;
                for (unsigned int x = 0; x != 8; ++x) {
                    *d++ = *s;
                    *d++ = *s++;
                }
            }
            dst0[0] = dc;
            break;
        }
        default:
        {
            uint8_t * d = dst0;
            for (unsigned int y=0; y != 32; y++) {
                const uint8_t * s = src0 + (y >> 2) * 8;
                for (unsigned int x = 0; x != 8; ++x) {
                    *d++ = *s;
                    *d++ = *s;
                    *d++ = *s;
                    *d++ = *s++;
                }
            }
            dst0[0] = dc;
            break;
        }
    }
}

static void populate_scaling_factors(dec_env_t * const de, const HEVCContext * const s) {
    // Array of constants for scaling factors
    static const uint32_t scaling_factor_offsets[4][6] = {
        // MID0    MID1    MID2    MID3    MID4    MID5
        {0x0000, 0x0010, 0x0020, 0x0030, 0x0040, 0x0050},   // SID0 (4x4)
        {0x0060, 0x00A0, 0x00E0, 0x0120, 0x0160, 0x01A0},   // SID1 (8x8)
        {0x01E0, 0x02E0, 0x03E0, 0x04E0, 0x05E0, 0x06E0},   // SID2 (16x16)
        {0x07E0,      0,      0, 0x0BE0,      0,      0}};  // SID3 (32x32)

    // ffmpeg places SID3,MID1 where matrixID 3 normally is
    const ScalingList * const sl =
        s->ps.pps->scaling_list_data_present_flag ? &s->ps.pps->scaling_list
                                                  : &s->ps.sps->scaling_list;
    unsigned int mid;

    for (mid=0; mid<6; mid++)
        expand_scaling_list(0, mid,
            de->scaling_factors + scaling_factor_offsets[0][mid],
            sl->sl[0][mid], 0);
    for (mid=0; mid<6; mid++)
        expand_scaling_list(1, mid,
            de->scaling_factors + scaling_factor_offsets[1][mid],
            sl->sl[1][mid], 0);
    for (mid=0; mid<6; mid++)
        expand_scaling_list(2, mid,
            de->scaling_factors + scaling_factor_offsets[2][mid],
            sl->sl[2][mid],
            sl->sl_dc[0][mid]);
    // second scaling matrix for 32x32 is at matrixID 3 not 1 in ffmpeg
    for (mid=0; mid<6; mid += 3)
        expand_scaling_list(3, mid,
            de->scaling_factors + scaling_factor_offsets[3][mid],
            sl->sl[3][mid],
            sl->sl_dc[1][mid]);
}

//////////////////////////////////////////////////////////////////////////////
// Probabilities

static const uint8_t prob_init[3][156] = {
	{
		 153, 200, 139, 141, 157, 154, 154, 154,
		 154, 154, 184, 154, 154, 154, 184,  63,
		 154, 154, 154, 154, 154, 154, 154, 154,
		 154, 154, 154, 154, 154, 153, 138, 138,
		 111, 141,  94, 138, 182, 154, 154, 154,
		 140,  92, 137, 138, 140, 152, 138, 139,
		 153,  74, 149,  92, 139, 107, 122, 152,
		 140, 179, 166, 182, 140, 227, 122, 197,
		 110, 110, 124, 125, 140, 153, 125, 127,
		 140, 109, 111, 143, 127, 111,  79, 108,
		 123,  63, 110, 110, 124, 125, 140, 153,
		 125, 127, 140, 109, 111, 143, 127, 111,
		  79, 108, 123,  63,  91, 171, 134, 141,
		 138, 153, 136, 167, 152, 152, 139, 139,
		 111, 111, 125, 110, 110,  94, 124, 108,
		 124, 107, 125, 141, 179, 153, 125, 107,
		 125, 141, 179, 153, 125, 107, 125, 141,
		 179, 153, 125, 140, 139, 182, 182, 152,
		 136, 152, 136, 153, 136, 139, 111, 136,
		 139, 111,   0,   0,	},
	{
		 153, 185, 107, 139, 126, 197, 185, 201,
		 154, 149, 154, 139, 154, 154, 154, 152,
		 110, 122,  95,  79,  63,  31,  31, 153,
		 153, 168, 140, 198,  79, 124, 138,  94,
		 153, 111, 149, 107, 167, 154, 154, 154,
		 154, 196, 196, 167, 154, 152, 167, 182,
		 182, 134, 149, 136, 153, 121, 136, 137,
		 169, 194, 166, 167, 154, 167, 137, 182,
		 125, 110,  94, 110,  95,  79, 125, 111,
		 110,  78, 110, 111, 111,  95,  94, 108,
		 123, 108, 125, 110,  94, 110,  95,  79,
		 125, 111, 110,  78, 110, 111, 111,  95,
		  94, 108, 123, 108, 121, 140,  61, 154,
		 107, 167,  91, 122, 107, 167, 139, 139,
		 155, 154, 139, 153, 139, 123, 123,  63,
		 153, 166, 183, 140, 136, 153, 154, 166,
		 183, 140, 136, 153, 154, 166, 183, 140,
		 136, 153, 154, 170, 153, 123, 123, 107,
		 121, 107, 121, 167, 151, 183, 140, 151,
		 183, 140,   0,   0,	},
	{
		 153, 160, 107, 139, 126, 197, 185, 201,
		 154, 134, 154, 139, 154, 154, 183, 152,
		 154, 137,  95,  79,  63,  31,  31, 153,
		 153, 168, 169, 198,  79, 224, 167, 122,
		 153, 111, 149,  92, 167, 154, 154, 154,
		 154, 196, 167, 167, 154, 152, 167, 182,
		 182, 134, 149, 136, 153, 121, 136, 122,
		 169, 208, 166, 167, 154, 152, 167, 182,
		 125, 110, 124, 110,  95,  94, 125, 111,
		 111,  79, 125, 126, 111, 111,  79, 108,
		 123,  93, 125, 110, 124, 110,  95,  94,
		 125, 111, 111,  79, 125, 126, 111, 111,
		  79, 108, 123,  93, 121, 140,  61, 154,
		 107, 167,  91, 107, 107, 167, 139, 139,
		 170, 154, 139, 153, 139, 123, 123,  63,
		 124, 166, 183, 140, 136, 153, 154, 166,
		 183, 140, 136, 153, 154, 166, 183, 140,
		 136, 153, 154, 170, 153, 138, 138, 122,
		 121, 122, 121, 167, 151, 183, 140, 151,
		 183, 140,   0,   0,	},
};


//////////////////////////////////////////////////////////////////////////////
// Phase 1 command and bit FIFOs

// ???? uint16_t addr - put in uint32_t
static int p1_apb_write(dec_env_t * const de, const uint16_t addr, const uint32_t data) {
    if (de->cmd_len==de->cmd_max)
        av_assert0(de->cmd_fifo = realloc(de->cmd_fifo, (de->cmd_max*=2)*sizeof(struct RPI_CMD)));

#if TRACE_DEV
    printf("[%02x] %x %x\n", de->cmd_len, addr, data);
#endif

    de->cmd_fifo[de->cmd_len].addr = addr;
    de->cmd_fifo[de->cmd_len].data = data;
    return de->cmd_len++;
}

static void p1_axi_write(dec_env_t * const de, const uint32_t len, const void * const ptr, const int cmd_idx) {
    if (de->bit_len==de->bit_max)
        av_assert0(de->bit_fifo = realloc(de->bit_fifo, (de->bit_max*=2)*sizeof(struct RPI_BIT)));
    de->bit_fifo[de->bit_len].cmd = cmd_idx;
    de->bit_fifo[de->bit_len].ptr = ptr;
    de->bit_fifo[de->bit_len].len = len;
    de->bit_len++;
}

//////////////////////////////////////////////////////////////////////////////
// Write probability and scaling factor memories

#if 0
static void WriteProb(dec_env_t * const de) {
    int i;
    const uint8_t *p = (uint8_t *) &de->probabilities;
    for (i=0; i<sizeof(struct RPI_PROB); i+=4, p+=4)
        p1_apb_write(de, 0x1000+i, p[0] + (p[1]<<8) + (p[2]<<16) + (p[3]<<24));
}
#endif

static void WriteProb(dec_env_t * const de, const HEVCContext * const s) {
    uint8_t dst[RPI_PROB_ARRAY_SIZE];

    const unsigned int init_type = (s->sh.cabac_init_flag && s->sh.slice_type != HEVC_SLICE_I) ?
        s->sh.slice_type + 1 : 2 - s->sh.slice_type;
    const uint8_t * p = prob_init[init_type];
    const int q = av_clip(s->sh.slice_qp, 0, 51);
    unsigned int i;

    for (i = 0; i < RPI_PROB_VALS; i++) {
        int init_value = p[i];
        int m = (init_value >> 4) * 5 - 45;
        int n = ((init_value & 15) << 3) - 16;
        int pre = 2 * (((m * q) >> 4) + n) - 127;

        pre ^= pre >> 31;
        if (pre > 124)
            pre = 124 + (pre & 1);
        dst[i] = pre;
    }
    for (i = RPI_PROB_VALS; i != RPI_PROB_ARRAY_SIZE; ++i) {
        dst[i] = 0;
    }

    for (i=0; i < RPI_PROB_ARRAY_SIZE; i+=4)
        p1_apb_write(de, 0x1000+i, dst[i] + (dst[i+1]<<8) + (dst[i+2]<<16) + (dst[i+3]<<24));

}


static void WriteScalingFactors(dec_env_t * const de) {
    int i;
    const uint8_t *p = (uint8_t *) de->scaling_factors;
    for (i=0; i<NUM_SCALING_FACTORS; i+=4, p+=4)
        p1_apb_write(de, 0x2000+i, p[0] + (p[1]<<8) + (p[2]<<16) + (p[3]<<24));
}

//////////////////////////////////////////////////////////////////////////////

static int ctb_to_tile (unsigned int ctb, unsigned int *bd, int num) {
    int i;
    for (i=1; ctb >= bd[i]; i++); // bd[] has num+1 elements; bd[0]=0; see hevc_ps.c
    return i-1;
}

static int ctb_to_slice_w_h (unsigned int ctb, int ctb_size, int width, unsigned int *bd, int num) {
    if (ctb < bd[num-1]) return ctb_size;
    else if (width % ctb_size) return width % ctb_size;
    else return ctb_size;
}

//////////////////////////////////////////////////////////////////////////////
// Handle PU and COEFF stream overflow


// Returns:
// -2 Other error
// -1 Out of coeff space
//  0  OK
//  1  Out of PU space

static int check_status(const RPI_T * const rpi, dec_env_t * const de) {
    uint32_t status;

    // this is the definition of successful completion of phase 1
    // it assures that status register is zero and all blocks in each tile have completed
    if (apb_read(rpi, RPI_CFSTATUS) == apb_read(rpi, RPI_CFNUM))
        return 0;

    status = apb_read(rpi, RPI_STATUS);

    if ((status & 8) != 0)
        return -1;

    if ((status & 0x10) != 0)
        return 1;

    return -2;
}

//////////////////////////////////////////////////////////////////////////////
// Write STATUS register with expected end CTU address of previous slice

static void end_previous_slice(dec_env_t * const de, const HEVCContext * const s, const int ctb_addr_ts) {
    const HEVCPPS * const pps = s->ps.pps;
    int last_x = pps->ctb_addr_ts_to_rs[ctb_addr_ts-1] % de->PicWidthInCtbsY;
    int last_y = pps->ctb_addr_ts_to_rs[ctb_addr_ts-1] / de->PicWidthInCtbsY;
    p1_apb_write(de, RPI_STATUS, 1 + (last_x<<5) + (last_y<<18));
}

static void wpp_pause(dec_env_t * const de, int ctb_row) {
    p1_apb_write(de, RPI_STATUS, (ctb_row<<18) + 0x25);
    p1_apb_write(de, RPI_TRANSFER, PROB_BACKUP);
    p1_apb_write(de, RPI_MODE, ctb_row==de->PicHeightInCtbsY-1 ? 0x70000 : 0x30000);
    p1_apb_write(de, RPI_CONTROL, (ctb_row<<16) + 2);
}

static void wpp_end_previous_slice(dec_env_t * const de, const HEVCContext * const s, int ctb_addr_ts) {
    const HEVCPPS *pps = s->ps.pps;
    int new_x = s->sh.slice_ctb_addr_rs % de->PicWidthInCtbsY;
    int new_y = s->sh.slice_ctb_addr_rs / de->PicWidthInCtbsY;
    int last_x = pps->ctb_addr_ts_to_rs[ctb_addr_ts-1] % de->PicWidthInCtbsY;
    int last_y = pps->ctb_addr_ts_to_rs[ctb_addr_ts-1] / de->PicWidthInCtbsY;
    if (de->wpp_entry_x<2 && (de->wpp_entry_y<new_y || new_x>2) && de->PicWidthInCtbsY>2)
        wpp_pause(de, last_y);
    p1_apb_write(de, RPI_STATUS, 1 + (last_x<<5) + (last_y<<18));
    if (new_x==2 || de->PicWidthInCtbsY==2 && de->wpp_entry_y<new_y)
        p1_apb_write(de, RPI_TRANSFER, PROB_BACKUP);
}

//////////////////////////////////////////////////////////////////////////////

static void new_slice_segment(dec_env_t * const de, const HEVCContext * const s)
{
    const HEVCSPS *sps = s->ps.sps;
    const HEVCPPS *pps = s->ps.pps;

    p1_apb_write(de, RPI_SPS0,
        (sps->log2_min_cb_size                    <<  0) +
        (sps->log2_ctb_size                       <<  4) +
        (sps->log2_min_tb_size                    <<  8) +
        (sps->log2_max_trafo_size                 << 12) +
        (sps->bit_depth                           << 16) +
        (sps->bit_depth                           << 20) +
        (sps->max_transform_hierarchy_depth_intra << 24) +
        (sps->max_transform_hierarchy_depth_inter << 28));

    p1_apb_write(de, RPI_SPS1,
        (sps->pcm.bit_depth                                        <<  0) +
        (sps->pcm.bit_depth_chroma                                 <<  4) +
        (sps->pcm.log2_min_pcm_cb_size                             <<  8) +
        (sps->pcm.log2_max_pcm_cb_size                             << 12) +
        (sps->separate_colour_plane_flag? 0:sps->chroma_format_idc << 16) +
        (sps->amp_enabled_flag                                     << 18) +
        (sps->pcm_enabled_flag                                     << 19) +
        (sps->scaling_list_enable_flag                             << 20) +
        (sps->sps_strong_intra_smoothing_enable_flag               << 21));

    p1_apb_write(de, RPI_PPS,
        (sps->log2_ctb_size - pps->diff_cu_qp_delta_depth   <<  0) +
        (pps->cu_qp_delta_enabled_flag                      <<  4) +
        (pps->transquant_bypass_enable_flag                 <<  5) +
        (pps->transform_skip_enabled_flag                   <<  6) +
        (pps->sign_data_hiding_flag                         <<  7) +
      (((pps->cb_qp_offset + s->sh.slice_cb_qp_offset)&255) <<  8) +
      (((pps->cr_qp_offset + s->sh.slice_cr_qp_offset)&255) << 16) +
        (pps->constrained_intra_pred_flag                   << 24));

    if (s->ps.sps->scaling_list_enable_flag) WriteScalingFactors(de);

    if (!s->sh.dependent_slice_segment_flag) {
        int ctb_col = s->sh.slice_ctb_addr_rs % de->PicWidthInCtbsY;
        int ctb_row = s->sh.slice_ctb_addr_rs / de->PicWidthInCtbsY;
        de->reg_slicestart = (ctb_col<<0) + (ctb_row<<16);
    }

    p1_apb_write(de, RPI_SLICESTART, de->reg_slicestart);
}

//////////////////////////////////////////////////////////////////////////////

static void write_slice(dec_env_t * const de, const HEVCContext * const s,
                        const unsigned int slice_w, const unsigned int slice_h) {
    uint32_t u32 =
          (s->sh.slice_type                           << 12)
        + (s->sh.slice_sample_adaptive_offset_flag[0] << 14)
        + (s->sh.slice_sample_adaptive_offset_flag[1] << 15)
        + (slice_w                                    << 17)
        + (slice_h                                    << 24);

    if (s->sh.slice_type==HEVC_SLICE_B || s->sh.slice_type==HEVC_SLICE_P) u32 |=
          (s->sh.max_num_merge_cand << 0)
        + (s->sh.nb_refs[L0]        << 4)
        + (s->sh.nb_refs[L1]        << 8);

    if (s->sh.slice_type==HEVC_SLICE_B)
        u32 |= s->sh.mvd_l1_zero_flag<<16;
    p1_apb_write(de, RPI_SLICE, u32);
}

//////////////////////////////////////////////////////////////////////////////
// Wavefront mode

static void wpp_entry_point(dec_env_t * const de, const HEVCContext * const s,
                            const int do_bte, const int resetQPY, const int ctb_addr_ts) {
    const HEVCSPS * const sps = s->ps.sps;
    const HEVCPPS * const pps = s->ps.pps;

    int ctb_size = 1<<sps->log2_ctb_size;
    int ctb_addr_rs = pps->ctb_addr_ts_to_rs[ctb_addr_ts];

    int ctb_col = de->wpp_entry_x = ctb_addr_rs % de->PicWidthInCtbsY;
    int ctb_row = de->wpp_entry_y = ctb_addr_rs / de->PicWidthInCtbsY;

    int endx = de->PicWidthInCtbsY-1;
    int endy = ctb_row;

    uint8_t slice_w = ctb_to_slice_w_h(ctb_col, ctb_size, sps->width,  pps->col_bd, pps->num_tile_columns);
    uint8_t slice_h = ctb_to_slice_w_h(ctb_row, ctb_size, sps->height, pps->row_bd, pps->num_tile_rows);

    p1_apb_write(de, RPI_TILESTART, 0);
    p1_apb_write(de, RPI_TILEEND, endx + (endy<<16));

    if (do_bte)
        p1_apb_write(de, RPI_BEGINTILEEND, endx + (endy<<16));

    write_slice(de, s, slice_w, ctb_row==de->PicHeightInCtbsY-1? slice_h : ctb_size);

    if (resetQPY) p1_apb_write(de, RPI_QP, sps->qp_bd_offset + s->sh.slice_qp);

    p1_apb_write(de, RPI_MODE, ctb_row==de->PicHeightInCtbsY-1? 0x60001 : 0x20001);
    p1_apb_write(de, RPI_CONTROL, (ctb_col<<0) + (ctb_row<<16));
}

//////////////////////////////////////////////////////////////////////////////
// Tiles mode

static void new_entry_point(dec_env_t * const de, const HEVCContext * const s,
                            const int do_bte, const int resetQPY, const int ctb_addr_ts) {
    const HEVCSPS * const sps = s->ps.sps;
    const HEVCPPS * const pps = s->ps.pps;

    int ctb_col = pps->ctb_addr_ts_to_rs[ctb_addr_ts] % de->PicWidthInCtbsY;
    int ctb_row = pps->ctb_addr_ts_to_rs[ctb_addr_ts] / de->PicWidthInCtbsY;

    int tile_x = ctb_to_tile (ctb_col, pps->col_bd, pps->num_tile_columns);
    int tile_y = ctb_to_tile (ctb_row, pps->row_bd, pps->num_tile_rows);

    int endx = pps->col_bd[tile_x+1] - 1;
    int endy = pps->row_bd[tile_y+1] - 1;

    uint8_t slice_w = ctb_to_slice_w_h(ctb_col, 1<<sps->log2_ctb_size, sps->width,  pps->col_bd, pps->num_tile_columns);
    uint8_t slice_h = ctb_to_slice_w_h(ctb_row, 1<<sps->log2_ctb_size, sps->height, pps->row_bd, pps->num_tile_rows);

    p1_apb_write(de, RPI_TILESTART, pps->col_bd[tile_x] + (pps->row_bd[tile_y]<<16));
    p1_apb_write(de, RPI_TILEEND, endx + (endy<<16));

    if (do_bte)
        p1_apb_write(de, RPI_BEGINTILEEND, endx + (endy<<16));

    write_slice(de, s, slice_w, slice_h);

    if (resetQPY)
        p1_apb_write(de, RPI_QP, sps->qp_bd_offset + s->sh.slice_qp);

    p1_apb_write(de, RPI_MODE, (0xFFFF                            <<  0)
                              + (0x0                               << 16)
                              + ((tile_x==pps->num_tile_columns-1) << 17)
                              + ((tile_y==pps->num_tile_rows-1)    << 18));

    p1_apb_write(de, RPI_CONTROL, (ctb_col<<0) + (ctb_row<<16));
}

//////////////////////////////////////////////////////////////////////////////

// Doesn't attempt to remove from context as we should only do this at the end
// of time or on create error
static void
dec_env_delete(dec_env_t * const de)
{
//    gpu_free(&de->gbuf);

    av_freep(&de->cmd_fifo);
    av_freep(&de->bit_fifo);

    sem_destroy(&de->phase_wait);
    av_free(de);
}

static dec_env_t *
dec_env_new(AVCodecContext * const avctx, RPI_T * const rpi)
{
    dec_env_t * const de = av_mallocz(sizeof(*de));
    int i;

    if (de == NULL)
        return NULL;

    de->avctx = avctx;
    de->phase_no = RPIVID_PHASE_NEW;

    sem_init(&de->phase_wait, 0, 0);

    if ((de->cmd_fifo = malloc((de->cmd_max=1024)*sizeof(struct RPI_CMD))) == NULL)
        goto fail;

    if ((de->bit_fifo = malloc((de->bit_max=1024)*sizeof(struct RPI_BIT))) == NULL)
        goto fail;

    pthread_mutex_lock(&rpi->phase_lock); // Abuse - not worth creating a lock just for this
    for (i = 0; i != avctx->thread_count; ++i) {
        if (rpi->dec_envs[i] == NULL)
        {
            rpi->dec_envs[i] = de;
            break;
        }
    }
    pthread_mutex_unlock(&rpi->phase_lock);

    if (i == avctx->thread_count) {
        av_log(avctx, AV_LOG_ERROR, "Failed to find a slot for hw thread context\n");
        goto fail;
    }

    return de;

fail:
    dec_env_delete(de);
    return NULL;
}


static dec_env_t *
dec_env_get(AVCodecContext * const avctx, RPI_T * const rpi)
{
    dec_env_t * de = NULL;
    const int ref_count = atomic_fetch_add(&rpi->ref_count, 1);

    if (ref_count <= 0) {
        // Already dead
        av_log(avctx, AV_LOG_ERROR, "RPIVID called whilst dead\n");;
        return NULL;
    }

    for (int i = 0; i != avctx->thread_count; ++i) {
        if (rpi->dec_envs[i] == NULL)
        {
            de = dec_env_new(avctx, rpi);
            break;
        }
        if (rpi->dec_envs[i]->avctx == avctx)
        {
            de = rpi->dec_envs[i];
            break;
        }
    }
    return de;
}

// Call at end of fn
// Used to ensure we aren't in a worker thead when killed
static void
dec_env_release(RPI_T * const rpi, dec_env_t * const de)
{
    const int n = atomic_fetch_sub(&rpi->ref_count, 1);
    if (n == 1) {
        sem_post(&rpi->ref_zero);
    }
}

//----------------------------------------------------------------------------

// Wait for a slot in the given phase
// Any error return is probably fatal
static int
wait_phase(RPI_T * const rpi, dec_env_t * const de, const int phase_no)
{
    int needs_wait = 0;
    phase_wait_env_t *const p = rpi->phase_reqs + phase_no;

    pthread_mutex_lock(&rpi->phase_lock);
    if (p->last_order + 1 != de->decode_order) {
        de->phase_wait_q_next = p->q;
        p->q = de;
        needs_wait = 1;
    }
    pthread_mutex_unlock(&rpi->phase_lock);

    if (needs_wait) {
        while (sem_wait(&de->phase_wait) == -1)
        {
            int err;
            if ((err = errno) != EINTR)
                return AVERROR(err);
        }
    }

    de->phase_no = phase_no;
    return 0;
}

static void
post_phase(RPI_T * const rpi, dec_env_t * const de, const int phase_no)
{
    dec_env_t * next_de = NULL;
    phase_wait_env_t *const p = rpi->phase_reqs + phase_no;
    dec_env_t ** q = &p->q;

    pthread_mutex_lock(&rpi->phase_lock);

    p->last_order = de->decode_order;
    while (*q != NULL) {
        dec_env_t * const t_de = *q;

        if (t_de->decode_order == p->last_order + 1) {
            // This is us - remove from Q
            *q = t_de->phase_wait_q_next;
            t_de->phase_wait_q_next = NULL; // Tidy
            next_de = t_de;
            break;
        }
        q = &t_de->phase_wait_q_next;
    }

    pthread_mutex_unlock(&rpi->phase_lock);

    if (next_de != NULL)
        sem_post(&next_de->phase_wait);
}

// Wait & signal stuff s.t. threads in other phases can continue
static void
abort_phases(RPI_T * const rpi, dec_env_t * const de)
{
    for (int i = de->phase_no + 1; i < RPIVID_PHASE_NEW; ++i) {
        wait_phase(rpi, de, i);
        post_phase(rpi, de, i);
    }
    de->phase_no = RPIVID_PHASE_NEW;
}

// Start timing for phase
// Stats only - no actual effect
static inline void tstart_phase(RPI_T * const rpi, const int phase_no)
{
#if OPT_PHASE_TIMING
    phase_wait_env_t *const p = rpi->phase_reqs + phase_no;
    const int64_t now = tus64();
    if (p->phase_time != 0)
        p->time_out_phase += now - p->phase_time;
    p->phase_time = now;
#endif
}

#if OPT_PHASE_TIMING
static unsigned int tavg_bin_phase(phase_wait_env_t *const p, const unsigned int avg_n)
{
    uint64_t tsum = 0;
    unsigned int i;
    for (i = 0; i != avg_n; ++i)
        tsum += p->time_stash[(p->i3 - i) & 15];
    for (i = 0; i != 9; ++i) {
        if (time_thresholds[i] * 1000 * avg_n > tsum)
            break;
    }
    return i;
}
#endif

// End timing for phase
// Stats only - no actual effect
static inline void tend_phase(RPI_T * const rpi, const int phase_no)
{
#if OPT_PHASE_TIMING
    phase_wait_env_t *const p = rpi->phase_reqs + phase_no;
    const uint64_t now = tus64();
    const uint64_t in_time = now - p->phase_time;

    p->time_in_phase += in_time;
    p->phase_time = now;
    p->time_stash[p->i3] = in_time;
    if (in_time > p->max_phase_time) {
        p->max_phase_time = in_time;
        p->max_time_decode_order = p->last_order;
    }
    ++p->time_bins[tavg_bin_phase(p, 1)];
    ++p->time_bins3[tavg_bin_phase(p, 3)];
    ++p->time_bins5[tavg_bin_phase(p, 5)];

    p->i3 = (p->i3 + 1) & 15;
#endif
}

//////////////////////////////////////////////////////////////////////////////
// Start frame

static int rpi_hevc_start_frame(
    AVCodecContext * avctx,
    const uint8_t *buffer,
    uint32_t size) {

    RPI_T * const rpi = avctx->internal->hwaccel_priv_data;
    dec_env_t * const de = dec_env_get(avctx, rpi);
    const HEVCContext * const s = avctx->priv_data;
    const HEVCSPS * const sps = s->ps.sps;
    const unsigned int CtbSizeY = 1U << sps->log2_ctb_size;

#if TRACE_ENTRY
    printf("<<< %s[%p]\n", __func__, de);
#endif

    if (de == NULL) {
        av_log(avctx, AV_LOG_ERROR, "%s: Cannot find find context for thread\n", __func__);
        return -1;
    }

    de->phase_no = RPIVID_PHASE_START;
    de->decode_order = ++rpi->decode_order;  // *** atomic?

    ff_thread_finish_setup(avctx); // Allow next thread to enter rpi_hevc_start_frame

    if (de->state != RPIVID_DECODE_NEW && de->state != RPIVID_DECODE_END) {
        av_log(avctx, AV_LOG_ERROR, "%s: Unexpected state transition: %d", __func__, de->state);
        return -1;
    }
    de->state = RPIVID_DECODE_START;

    de->PicWidthInCtbsY  = (sps->width + CtbSizeY - 1) / CtbSizeY;  //7-15
    de->PicHeightInCtbsY = (sps->height + CtbSizeY - 1) / CtbSizeY;  //7-17
    de->bit_len = 0;
    de->cmd_len = 0;

#if TRACE_ENTRY
    printf(">>> %s[%p]\n", __func__, de);
#endif

    dec_env_release(rpi, de);
    return 0;
}

//////////////////////////////////////////////////////////////////////////////
// Slice messages

static void msg_slice(dec_env_t * const de, const uint16_t msg) {
    de->slice_msgs[de->num_slice_msgs++] = msg;
}

static void program_slicecmds(dec_env_t * const de, const int sliceid) {
    int i;
    p1_apb_write(de, RPI_SLICECMDS, de->num_slice_msgs+(sliceid<<8));
    for(i=0; i < de->num_slice_msgs; i++) {
        p1_apb_write(de, 0x4000+4*i, de->slice_msgs[i] & 0xffff);
    }
}

static void pre_slice_decode(dec_env_t * const de, const HEVCContext * const s) {
    const HEVCSPS * const sps = s->ps.sps;
    const HEVCPPS * const pps = s->ps.pps;
    const SliceHeader *sh = &s->sh;

    int weightedPredFlag, i, rIdx;
    uint16_t cmd_slice;
    unsigned int collocated_from_l0_flag;

    de->num_slice_msgs=0;
    de->dpbno_col = 0;
    cmd_slice = 0;
    if (sh->slice_type==HEVC_SLICE_I) cmd_slice = 1;
    if (sh->slice_type==HEVC_SLICE_P) cmd_slice = 2;
    if (sh->slice_type==HEVC_SLICE_B) cmd_slice = 3;

    if (sh->slice_type!=HEVC_SLICE_I) {
        cmd_slice += sh->nb_refs[L0]<<2;
        cmd_slice += sh->nb_refs[L1]<<6;
    }

    if (sh->slice_type==HEVC_SLICE_P ||  sh->slice_type==HEVC_SLICE_B)
        cmd_slice |= sh->max_num_merge_cand<<11;

    collocated_from_l0_flag =
        !sh->slice_temporal_mvp_enabled_flag ?
            0 :
        sh->slice_type == HEVC_SLICE_B ?
            (sh->collocated_list == L0) :
            (sh->slice_type==HEVC_SLICE_P);
    cmd_slice |= collocated_from_l0_flag<<14;

    if (sh->slice_type==HEVC_SLICE_P || sh->slice_type==HEVC_SLICE_B) {

        int NoBackwardPredFlag = 1; // Flag to say all reference pictures are from the past
        for(i=L0; i<=L1; i++) {
            for(rIdx=0; rIdx <sh->nb_refs[i]; rIdx++) {
                HEVCFrame *f = s->ref->refPicList[i].ref[rIdx];
                HEVCFrame *c = s->ref; // CurrentPicture
                if (c->poc < f->poc) NoBackwardPredFlag = 0;
            }
        }

        if (sps->sps_temporal_mvp_enabled_flag)
        {
            const RefPicList *rpl = (sh->slice_type != HEVC_SLICE_B || collocated_from_l0_flag) ?
                s->ref->refPicList + 0 :
                s->ref->refPicList + 1;
            de->dpbno_col = rpl->ref[sh->collocated_ref_idx] - s->DPB;
        }

        cmd_slice += NoBackwardPredFlag<<10;
        msg_slice(de, cmd_slice);

        // Write reference picture descriptions
        weightedPredFlag = sh->slice_type==HEVC_SLICE_P? pps->weighted_pred_flag : pps->weighted_bipred_flag;

        for(i=L0; i<=L1; i++)
            for(rIdx=0; rIdx <sh->nb_refs[i]; rIdx++) {
                HEVCFrame *f = s->ref->refPicList[i].ref[rIdx];
                HEVCFrame *c = s->ref; // CurrentPicture
                int pic = f - s->DPB;
                // Make sure pictures are in range 0 to 15
                int adjusted_pic = f<c? pic : pic-1;
                int lt = s->ref->refPicList[i].isLongTerm[rIdx];
                msg_slice(de, adjusted_pic+(lt<<4)+(weightedPredFlag<<5)+(weightedPredFlag<<6));
                msg_slice(de, f->poc);
                if (weightedPredFlag) {
                    msg_slice(de,   s->sh.luma_log2_weight_denom+(((i?s->  sh.luma_weight_l1:  s->sh.luma_weight_l0)[rIdx]   &0x1ff)<<3));
                    msg_slice(de,                                  (i?s->  sh.luma_offset_l1:  s->sh.luma_offset_l0)[rIdx]   & 0xff);
                    msg_slice(de, s->sh.chroma_log2_weight_denom+(((i?s->sh.chroma_weight_l1:s->sh.chroma_weight_l0)[rIdx][0]&0x1ff)<<3));
                    msg_slice(de,                                  (i?s->sh.chroma_offset_l1:s->sh.chroma_offset_l0)[rIdx][0]& 0xff);
                    msg_slice(de, s->sh.chroma_log2_weight_denom+(((i?s->sh.chroma_weight_l1:s->sh.chroma_weight_l0)[rIdx][1]&0x1ff)<<3));
                    msg_slice(de,                                  (i?s->sh.chroma_offset_l1:s->sh.chroma_offset_l0)[rIdx][1]& 0xff);
                }
            }
    }
    else
        msg_slice(de, cmd_slice);

    msg_slice(de, ((sh->beta_offset/2)&15)
        + (((sh->tc_offset/2)&15)                           <<  4)
        + (sh->disable_deblocking_filter_flag               <<  8)
        + (sh->slice_loop_filter_across_slices_enabled_flag <<  9)
        + (pps->loop_filter_across_tiles_enabled_flag       << 10)); // CMD_DEBLOCK

    msg_slice(de, ((sh->slice_cr_qp_offset&31)<<5) + (sh->slice_cb_qp_offset&31)); // CMD_QPOFF
}


//////////////////////////////////////////////////////////////////////////////

static void rpi_hevc_abort_frame(AVCodecContext * const avctx) {
    RPI_T * const rpi = avctx->internal->hwaccel_priv_data;
    dec_env_t * const de = dec_env_get(avctx,  rpi);

#if TRACE_ENTRY
    printf("<<< %s[%p]\n", __func__, de);
#endif

    if (de == NULL) {
        av_log(avctx, AV_LOG_ERROR, "%s: Cannot find find context for thread\n", __func__);
        return;
    }

    switch (de->state) {
        case RPIVID_DECODE_NEW:
        case RPIVID_DECODE_END:
            // Expected transition
            break;

        case RPIVID_DECODE_SLICE:
            // Error transition
            av_log(avctx, AV_LOG_INFO, "Error in decode - aborting\n");
            break;

        case RPIVID_DECODE_START:
        default:
            av_log(avctx, AV_LOG_ERROR, "%s: Unexpected state transition: %d", __func__, de->state);
            break;
    }

    abort_phases(rpi, de);
    de->state = RPIVID_DECODE_NEW;

    dec_env_release(rpi, de);
}

//////////////////////////////////////////////////////////////////////////////
// End frame

static int rpi_hevc_end_frame(AVCodecContext * const avctx) {
    RPI_T * const rpi = avctx->internal->hwaccel_priv_data;
    const HEVCContext * const s = avctx->priv_data;
    const HEVCPPS * const pps = s->ps.pps;
    const HEVCSPS * const sps = s->ps.sps;
    dec_env_t * const de = dec_env_get(avctx,  rpi);
    AVFrame * const f = s->ref->frame;
    const unsigned int dpbno_cur = s->ref - s->DPB;
    vid_vc_addr_t cmds_vc;
    vid_vc_addr_t pu_base_vc;
    unsigned int pu_stride;
    vid_vc_addr_t coeff_base_vc;
    unsigned int coeff_stride;
    unsigned int i;
    int rv = 0;
    int status = 0;
    int coeffbuf_sem_claimed = 0;

#if TRACE_ENTRY
    fprintf("<<< %s[%p]\n", __func__, de);
#endif

    if (de == NULL) {
        av_log(avctx, AV_LOG_ERROR, "%s: Cannot find find context for thread\n", __func__);
        return AVERROR_BUG;  // Should never happen
    }

    if (de->state != RPIVID_DECODE_SLICE) {
        av_log(avctx, AV_LOG_ERROR, "%s: Unexpected state: %d\n", __func__, de->state);
        rv = AVERROR_UNKNOWN;
        goto fail;
    }
    de->state = RPIVID_DECODE_END;

    // End of command compilation
    {
        const unsigned int last_x = pps->col_bd[pps->num_tile_columns]-1;
        const unsigned int last_y = pps->row_bd[pps->num_tile_rows]-1;
        if (pps->entropy_coding_sync_enabled_flag) {
            if (de->wpp_entry_x<2 && de->PicWidthInCtbsY>2)
                wpp_pause(de, last_y);
        }
        p1_apb_write(de, RPI_STATUS, 1 + (last_x<<5) + (last_y<<18));
    }

    // Phase 0 ---------------------------------------------------------------

    wait_phase(rpi, de, 0);
    rpi_sem_wait(&rpi->bitbuf_sem);
    tstart_phase(rpi, 0);

    // Copy cmds & bits into gpu side buffer
    // Layout: CMDS, BITS
    {
        uint8_t * const armbase = rpi->gbitbufs[rpi->bitbuf_no].arm;
        vid_vc_addr_t vcbase = rpi->gbitbufs[rpi->bitbuf_no].vc;
        unsigned int cmd_bytes = de->cmd_len * sizeof(struct RPI_CMD);

        uint8_t * p = armbase + rnd64(cmd_bytes);
        uint8_t * const eobits = armbase + rpi->gbitbufs[rpi->bitbuf_no].numbytes;

        cmds_vc = vcbase;

        // Copy all the bits & update bitstream cmds to point at the right bits
        for (i = 0; i < de->bit_len; ++i)
        {
            const unsigned int seg_len = de->bit_fifo[i].len;

            if (p + seg_len > eobits) {
                status = -1;
                break;
            }

            memcpy(p, de->bit_fifo[i].ptr, seg_len);
            de->cmd_fifo[de->bit_fifo[i].cmd].data = MANGLE64((p - armbase) + vcbase);

            p += rnd64(seg_len);
        }

        memcpy(armbase, de->cmd_fifo, cmd_bytes);
    }

    if (status == 0)
    {
        if (++rpi->bitbuf_no >= RPIVID_BITBUFS)
            rpi->bitbuf_no = 0;
    }
    else
    {
        sem_post(&rpi->bitbuf_sem);
        av_log(avctx, AV_LOG_ERROR, "Out of HEVC bit/cmd memory\n");
        rv = AVERROR_BUFFER_TOO_SMALL;
    }

    tend_phase(rpi, 0);
    post_phase(rpi, de, 0);

    if (status < 0)
        goto fail;

    // Phase 1 ---------------------------------------------------------------

    wait_phase(rpi, de, 1);
    rpi_sem_wait(&rpi->coeffbuf_sem);
    coeffbuf_sem_claimed = 1;
    tstart_phase(rpi, 1);

    status = 0;
    for (;;)
    {
        // (Re-)allocate PU/COEFF stream space
        const unsigned int total_size = rpi->gcoeffbufs[rpi->coeffbuf_no].numbytes;
        unsigned int pu_size;

        pu_base_vc = rpi->gcoeffbufs[rpi->coeffbuf_no].vc;
        pu_stride = rnd64(rpi->max_pu_msgs * 2 * de->PicWidthInCtbsY);
        pu_size = pu_stride * de->PicHeightInCtbsY;

        if (pu_size >= total_size || status == -1) {
            GPU_MEM_PTR_T newbuf;

            if (gpu_malloc_uncached(round_up_size(total_size + 1), &newbuf) != 0)
            {
                av_log(avctx, AV_LOG_ERROR, "Failed to reallocate coeffbuf\n");
                status = -1;
                break;
            }
            gpu_free(rpi->gcoeffbufs + rpi->coeffbuf_no);
            rpi->gcoeffbufs[rpi->coeffbuf_no] = newbuf;
            status = 0;
            continue;
        }

        // Allocate all remaining space to coeff
        coeff_base_vc = pu_base_vc + pu_size;
        coeff_stride = ((total_size - pu_size) / de->PicHeightInCtbsY) & ~63;  // Round down to multiple of 64

        apb_write_vc_addr(rpi, RPI_PUWBASE, pu_base_vc);
        apb_write_vc_len(rpi, RPI_PUWSTRIDE, pu_stride);
        apb_write_vc_addr(rpi, RPI_COEFFWBASE, coeff_base_vc);
        apb_write_vc_len(rpi, RPI_COEFFWSTRIDE, coeff_stride);

        // Trigger command FIFO
        apb_write(rpi, RPI_CFNUM, de->cmd_len);
#if TRACE_DEV && 0
        apb_dump_regs(rpi, 0x0, 32);
        apb_dump_regs(rpi, 0x8000, 24);
        axi_dump(de, ((uint64_t)a64)<<6, de->cmd_len * sizeof(struct RPI_CMD));
#endif
        apb_write_vc_addr(rpi, RPI_CFBASE, cmds_vc);

        int_wait(rpi, 1);

        status = check_status(rpi, de);

        if (status == -1)
            continue;
        else if (status != 1)
            break;

        // Status 1 means out of PU space so try again with more
        // If we ran out of Coeff space then we are out of memory - we could possibly realloc?
        rpi->max_pu_msgs += rpi->max_pu_msgs / 2;
    }

    // Inc inside the phase 1 lock, but only inc if we succeeded otherwise we
    // may reuse a live buffer when we kick the coeff sem
    if (status == 0)
    {
        if (++rpi->coeffbuf_no >= RPIVID_COEFFBUFS)
            rpi->coeffbuf_no = 0;
    }
    else
    {
        if (status == -1)
        {
            av_log(avctx, AV_LOG_ERROR, "Out of pu + coeff intermediate memory: pus=%d\n", rpi->max_pu_msgs);
            rv = AVERROR_BUFFER_TOO_SMALL;
        }
        else
        {
            av_log(avctx, AV_LOG_WARNING, "Phase 1 decode error\n");
            rv = AVERROR_INVALIDDATA;
        }
    }

    tend_phase(rpi, 1);
    sem_post(&rpi->bitbuf_sem);
    post_phase(rpi, de, 1);

    if (status != 0)
        goto fail;

    // Phase 2 ---------------------------------------------------------------

    wait_phase(rpi, de, 2);

    if ((rv = av_rpi_zc_resolve_frame(f, ZC_RESOLVE_ALLOC)) != 0)
    {
        // As we are in phase 2 already here we don't need to worry about
        // ceoffbuf_no despite the early exit
        post_phase(rpi, de, 2);
        av_log(avctx, AV_LOG_ERROR, "Failed to allocate output frame\n");
        goto fail;
    }

    tstart_phase(rpi, 2);

    apb_write_vc_addr(rpi, RPI_PURBASE, pu_base_vc);
    apb_write_vc_len(rpi, RPI_PURSTRIDE, pu_stride);
    apb_write_vc_addr(rpi, RPI_COEFFRBASE, coeff_base_vc);
    apb_write_vc_len(rpi, RPI_COEFFRSTRIDE, coeff_stride);

    apb_write_vc_addr(rpi, RPI_OUTYBASE, get_vc_address_y(f));
    apb_write_vc_addr(rpi, RPI_OUTCBASE, get_vc_address_u(f));
    apb_write_vc_len(rpi, RPI_OUTYSTRIDE, f->linesize[3] * 128);
    apb_write_vc_len(rpi, RPI_OUTCSTRIDE, f->linesize[3] * 128);

    // Keep the last thing we resolved as fallback for any ref we fail to
    // resolve.  As a final fallback use our current frame.  The pels might
    // not be there yet but at least the memory is valid.
    //
    // Attempt to resolve the entire DPB - we could note what we have used
    // in ref lists but probably simpler and more reliable to set the whole thing
    {
        AVFrame * fallback_frame = f;
        for (i = 0; i != 16; ++i) {
            // Avoid current frame
            const HEVCFrame * hevc_fr = (s->DPB + i >= s->ref) ? s->DPB + i + 1 : s->DPB + i;
            AVFrame * fr = hevc_fr->frame;

            if (fr != NULL &&
                av_rpi_zc_resolve_frame(fr, ZC_RESOLVE_FAIL) == 0)
            {
                fallback_frame = fr;
            }
            else
            {
                fr = fallback_frame;
            }

            apb_write_vc_addr(rpi, 0x9000+16*i, get_vc_address_y(fr));
            apb_write(rpi, 0x9004+16*i, 0);
            apb_write_vc_addr(rpi, 0x9008+16*i, get_vc_address_u(fr));
            apb_write(rpi, 0x900C+16*i, 0);
        }
    }

    apb_write(rpi, RPI_CONFIG2,
          (sps->bit_depth                             << 0) // BitDepthY
        + (sps->bit_depth                             << 4) // BitDepthC
       + ((sps->bit_depth>8)                          << 8) // BitDepthY
       + ((sps->bit_depth>8)                          << 9) // BitDepthC
        + (sps->log2_ctb_size                         <<10)
        + (pps->constrained_intra_pred_flag           <<13)
        + (sps->sps_strong_intra_smoothing_enable_flag<<14)
        + (sps->sps_temporal_mvp_enabled_flag         <<15)
        + (pps->log2_parallel_merge_level             <<16)
        + (s->sh.slice_temporal_mvp_enabled_flag      <<19)
        + (sps->pcm.loop_filter_disable_flag          <<20)
       + ((pps->cb_qp_offset&31)                      <<21)
       + ((pps->cr_qp_offset&31)                      <<26));

    apb_write(rpi, RPI_FRAMESIZE, (sps->height<<16) + sps->width);
    apb_write(rpi, RPI_CURRPOC, s->poc);

    // collocated reads/writes
    if (sps->sps_temporal_mvp_enabled_flag) {
        av_assert0(de->dpbno_col < RPIVID_COL_PICS);
        av_assert0(dpbno_cur < RPIVID_COL_PICS);

        apb_write_vc_len(rpi, RPI_COLSTRIDE, rpi->col_stride);
        apb_write_vc_len(rpi, RPI_MVSTRIDE,  rpi->col_stride);
        apb_write_vc_addr(rpi, RPI_MVBASE,  rpi->gcolbuf.vc + dpbno_cur * rpi->col_picsize);
        apb_write_vc_addr(rpi, RPI_COLBASE, rpi->gcolbuf.vc + de->dpbno_col * rpi->col_picsize);
    }

#if TRACE_DEV && 0
    apb_dump_regs(rpi, 0x0, 32);
    apb_dump_regs(rpi, 0x8000, 24);
#endif

    apb_write(rpi, RPI_NUMROWS, de->PicHeightInCtbsY);
    apb_read(rpi, RPI_NUMROWS); // Read back to confirm write has reached block

    int_wait(rpi, 2);

    tend_phase(rpi, 2);
    coeffbuf_sem_claimed = 0;
    sem_post(&rpi->coeffbuf_sem);
    // Set valid here to avoid race in resolving in any pending phase 2
    av_rpi_zc_set_valid_frame(f);

    post_phase(rpi, de, 2);

    // Flush frame for CPU access
    // Arguably the best place would be at the start of phase 2 but here
    // will overlap with the wait
    //
    // * Even better would be to have better lock/unlock control in ZC for external access
    if (rpi->gpu_init_type == GPU_INIT_GPU)  // * CMA is currently always uncached
    {
        rpi_cache_buf_t cbuf;
        rpi_cache_flush_env_t * const fe = rpi_cache_flush_init(&cbuf);
        rpi_cache_flush_add_frame(fe, f, RPI_CACHE_FLUSH_MODE_INVALIDATE);
        rpi_cache_flush_finish(fe);
    }

#if TRACE_ENTRY
    printf(">>> %s[%p] OK\n", __func__, de);
#endif

    dec_env_release(rpi, de);
    return 0;

fail:
    av_rpi_zc_set_broken_frame(f);
    if (coeffbuf_sem_claimed)
        sem_post(&rpi->coeffbuf_sem);
    abort_phases(rpi, de);  // Dummy any unresolved phases

#if TRACE_ENTRY
    printf(">>> %s[%p] FAIL\n", __func__, de);
#endif

    dec_env_release(rpi, de);
    return rv;
}

//////////////////////////////////////////////////////////////////////////////


#if TRACE_DEV
static void dump_data(const uint8_t * p, size_t len)
{
    size_t i;
    for (i = 0; i < len; i += 16) {
        size_t j;
        printf("%04x", i);
        for (j = 0; j != 16; ++j) {
            printf("%c%02x", i == 8 ? '-' : ' ', p[i+j]);
        }
        printf("\n");
    }
}
#endif

#if OPT_EMU
static const uint8_t * ptr_from_index(const uint8_t * b, unsigned int idx)
{
    unsigned int z = 0;
    while (idx--) {
        if (*b++ == 0) {
            ++z;
            if (z >= 2 && *b == 3) {
                ++b;
                z = 0;
            }
        }
        else {
            z = 0;
        }
    }
    return b;
}
#endif

static void WriteBitstream(dec_env_t * const de, const HEVCContext * const s) {
    const int rpi_use_emu = OPT_EMU; // FFmpeg removes emulation prevention bytes
    const int offset = 0; // Always 64-byte aligned in sim, need not be on real hardware
    const GetBitContext *gb = &s->HEVClc->gb;

#if OPT_EMU
    const uint8_t *ptr = ptr_from_index(de->nal_buffer, gb->index/8 + 1);
    const int len = de->nal_size - (ptr - de->nal_buffer);
#else
    const int len = 1 + gb->size_in_bits/8 - gb->index/8;
    const void *ptr = &gb->buffer[gb->index/8];
#endif

#if TRACE_DEV
    printf("Index=%d, /8=%#x\n", gb->index, gb->index/8);
    dump_data(de->nal_buffer, 128);
#endif

    p1_axi_write(de, len, ptr, p1_apb_write(de, RPI_BFBASE, 0)); // BFBASE set later
    p1_apb_write(de, RPI_BFNUM, len);
    p1_apb_write(de, RPI_BFCONTROL, offset + (1<<7)); // Stop
    p1_apb_write(de, RPI_BFCONTROL, offset + (rpi_use_emu<<6));
}

//////////////////////////////////////////////////////////////////////////////
// Wavefront mode

static void wpp_decode_slice(dec_env_t * const de, const HEVCContext * const s, int ctb_addr_ts)
{
    const HEVCPPS * const pps = s->ps.pps;

    int i, resetQPY=1;
    int indep = !s->sh.dependent_slice_segment_flag;
    int ctb_col = s->sh.slice_ctb_addr_rs % de->PicWidthInCtbsY;

    if (ctb_addr_ts)
        wpp_end_previous_slice(de, s, ctb_addr_ts);
    pre_slice_decode(de, s);
    WriteBitstream(de, s);
    if (ctb_addr_ts==0 || indep || de->PicWidthInCtbsY==1)
        WriteProb(de, s);
    else if (ctb_col==0)
        p1_apb_write(de, RPI_TRANSFER, PROB_RELOAD);
    else
        resetQPY=0;
    program_slicecmds(de, s->slice_idx);
    new_slice_segment(de, s);
    wpp_entry_point(de, s, indep, resetQPY, ctb_addr_ts);
    for (i=0; i<s->sh.num_entry_point_offsets; i++) {
        int ctb_addr_rs = pps->ctb_addr_ts_to_rs[ctb_addr_ts];
        int ctb_row = ctb_addr_rs / de->PicWidthInCtbsY;
        int last_x = de->PicWidthInCtbsY-1;
        if (de->PicWidthInCtbsY>2)
            wpp_pause(de, ctb_row);
        p1_apb_write(de, RPI_STATUS, (ctb_row<<18) + (last_x<<5) + 2);
        if (de->PicWidthInCtbsY==2)
            p1_apb_write(de, RPI_TRANSFER, PROB_BACKUP);
        if (de->PicWidthInCtbsY==1)
            WriteProb(de, s);
        else
            p1_apb_write(de, RPI_TRANSFER, PROB_RELOAD);
        ctb_addr_ts += pps->column_width[0];
        wpp_entry_point(de, s, 0, 1, ctb_addr_ts);
    }
}

//////////////////////////////////////////////////////////////////////////////
// Tiles mode

static void decode_slice(dec_env_t * const de, const HEVCContext * const s, int ctb_addr_ts) {
    const HEVCPPS * const pps = s->ps.pps;
    int i, resetQPY;

    if (ctb_addr_ts) end_previous_slice(de, s, ctb_addr_ts);
    pre_slice_decode(de, s);
    WriteBitstream(de, s);
    resetQPY = ctb_addr_ts==0
            || pps->tile_id[ctb_addr_ts]!=pps->tile_id[ctb_addr_ts-1]
            || !s->sh.dependent_slice_segment_flag;
    if (resetQPY) WriteProb(de, s);
    program_slicecmds(de, s->slice_idx);
    new_slice_segment(de, s);
    new_entry_point(de, s, !s->sh.dependent_slice_segment_flag, resetQPY, ctb_addr_ts);
    for (i=0; i<s->sh.num_entry_point_offsets; i++) {
        int ctb_addr_rs = pps->ctb_addr_ts_to_rs[ctb_addr_ts];
        int ctb_col = ctb_addr_rs % de->PicWidthInCtbsY;
        int ctb_row = ctb_addr_rs / de->PicWidthInCtbsY;
        int tile_x = ctb_to_tile (ctb_col, pps->col_bd, pps->num_tile_columns);
        int tile_y = ctb_to_tile (ctb_row, pps->row_bd, pps->num_tile_rows);
        int last_x = pps->col_bd[tile_x+1]-1;
        int last_y = pps->row_bd[tile_y+1]-1;
        p1_apb_write(de, RPI_STATUS, 2 + (last_x<<5) + (last_y<<18));
        WriteProb(de, s);
        ctb_addr_ts += pps->column_width[tile_x] * pps->row_height[tile_y];
        new_entry_point(de, s, 0, 1, ctb_addr_ts);
    }
}

//////////////////////////////////////////////////////////////////////////////

static int cabac_start_align(HEVCContext *s)
{
    GetBitContext *gb = &s->HEVClc->gb;
    skip_bits(gb, 1);
    align_get_bits(gb);
    // Should look at getting rid of this
    return ff_init_cabac_decoder(&s->HEVClc->cc,
                          gb->buffer + get_bits_count(gb) / 8,
                          (get_bits_left(gb) + 7) / 8);
}

static int rpi_hevc_decode_slice(
    AVCodecContext *avctx,
    const uint8_t *buffer,
    uint32_t size)
{
    RPI_T * const rpi = avctx->internal->hwaccel_priv_data;
    HEVCContext * const s = avctx->priv_data;
    dec_env_t * const de = dec_env_get(avctx, rpi);
    const HEVCPPS *pps = s->ps.pps;
    int ctb_addr_ts = pps->ctb_addr_rs_to_ts[s->sh.slice_ctb_addr_rs];

#if TRACE_ENTRY
    printf("<<< %s[%p]\n", __func__, de);
#endif
    if (de == NULL) {
        av_log(avctx, AV_LOG_ERROR, "%s: Cannot find find context for thread\n", __func__);
        return -1;
    }

    if (de->state != RPIVID_DECODE_START && de->state != RPIVID_DECODE_SLICE) {
        av_log(avctx, AV_LOG_ERROR, "%s: Unexpected state: %d\n", __func__, de->state);
        return -1;
    }
    de->state = RPIVID_DECODE_SLICE;

    de->nal_buffer = buffer;
    de->nal_size   = size;

#if !OPT_EMU
//    ff_hevc_cabac_init(s, ctb_addr_ts);
    cabac_start_align(s);
#endif
    if (s->ps.sps->scaling_list_enable_flag)
        populate_scaling_factors(de, s);
    pps->entropy_coding_sync_enabled_flag? wpp_decode_slice(de, s, ctb_addr_ts)
                                             : decode_slice(de, s, ctb_addr_ts);
#if TRACE_ENTRY
    printf(">>> %s[%p]\n", __func__, de);
#endif
    dec_env_release(rpi, de);
    return 0;
}

//////////////////////////////////////////////////////////////////////////////

static int rpivid_retrieve_data(void *logctx, AVFrame *frame)
{
    int rv;
    if ((rv = av_rpi_zc_resolve_frame(frame, ZC_RESOLVE_WAIT_VALID)) != 0)
        av_log(logctx, AV_LOG_ERROR, "Unable to resolve output frame\n");
    return rv;
}

static int rpivid_hevc_alloc_frame(AVCodecContext * avctx, AVFrame *frame)
{
    RPI_T * const rpi = avctx->internal->hwaccel_priv_data;
    HEVCContext * const s = avctx->priv_data;
    // Frame buffering + 1 output.  Would need thread_count extra but we now
    // alloc at the start of phase 2 so that is the only thread we need the
    // extra buffer for.
    const unsigned int pool_req = s->ps.sps->temporal_layer[s->ps.sps->max_sub_layers - 1].max_dec_pic_buffering + 1;
    int rv;

    if (av_rpi_zc_in_use(avctx))
    {
        const AVZcEnvPtr zc = avctx->opaque;
        av_rpi_zc_set_decoder_pool_size(zc, pool_req);
        rv = av_rpi_zc_get_buffer(zc, frame);   // get_buffer2 would alloc
    }
    else
    {
        if (rpi->zc == NULL) {
            pthread_mutex_lock(&rpi->phase_lock); // Abuse - not worth creating a lock just for this
            // Alloc inside lock to make sure we only ever alloc one
            if (rpi->zc == NULL) {
                rpi->zc = av_rpi_zc_int_env_alloc(s);
            }
            pthread_mutex_unlock(&rpi->phase_lock);
        }
        av_rpi_zc_set_decoder_pool_size(rpi->zc, pool_req); // Ignored by local allocator, but set anyway :-)
        rv = (rpi->zc == NULL) ? AVERROR(ENOMEM) :
            av_rpi_zc_get_buffer(rpi->zc, frame);
    }

    if (rv == 0 &&
        (rv = ff_attach_decode_data(frame)) < 0)
    {
        av_frame_unref(frame);
    }

    if (rv == 0)
    {
        FrameDecodeData *fdd = (FrameDecodeData*)frame->private_ref->data;
        fdd->post_process = rpivid_retrieve_data;
    }

    return rv;
}

#if OPT_PHASE_TIMING
static void log_bin_phase(AVCodecContext * const avctx, const unsigned int * const bins)
{
    av_log(avctx, AV_LOG_INFO, "%7d %7d %7d %7d %7d %7d %7d %7d %7d\n",
           bins[0],  bins[1], bins[2], bins[3],
           bins[4],  bins[5], bins[6], bins[7], bins[8]);
}
#endif

//////////////////////////////////////////////////////////////////////////////

static int rpi_hevc_free(AVCodecContext *avctx) {
    RPI_T * const rpi = avctx->internal->hwaccel_priv_data;

#if TRACE_ENTRY
    printf("<<< %s\n", __func__);
#endif

    dec_env_release(rpi, NULL);

    // Wait for everything else to stop
    {
        struct timespec tt;
        clock_gettime(CLOCK_REALTIME, &tt);
        tt.tv_sec += 2;
        while (sem_timedwait(&rpi->ref_zero, &tt) == -1) {
            const int err = errno;
            if (err == ETIMEDOUT) {
                av_log(avctx, AV_LOG_FATAL, "Rpivid worker threads still running\n");
                return -1;
            }
            if (err != EINTR) {
                av_log(avctx, AV_LOG_ERROR, "Unexpected error %d waiting for work thread to stop\n", err);
                break;
            }
        }
    }

#if OPT_PHASE_TIMING
    {
        unsigned int i;
        for (i = 0; i != RPIVID_PHASES; ++i) {
            const phase_wait_env_t * const p = rpi->phase_reqs + i;
            av_log(avctx, AV_LOG_INFO, "Phase %u: In %3u.%06u, Out %3u.%06u\n", i,
                   (unsigned int)(p->time_in_phase / 1000000), (unsigned int)(p->time_in_phase % 1000000),
                   (unsigned int)(p->time_out_phase / 1000000), (unsigned int)(p->time_out_phase % 1000000));
            av_log(avctx, AV_LOG_INFO, "%7d %7d %7d %7d %7d %7d %7d %7d        >\n",
                   time_thresholds[0], time_thresholds[1], time_thresholds[2], time_thresholds[3],
                   time_thresholds[4], time_thresholds[5], time_thresholds[6], time_thresholds[7]);
            log_bin_phase(avctx, p->time_bins);
            log_bin_phase(avctx, p->time_bins3);
            log_bin_phase(avctx, p->time_bins5);
            av_log(avctx, AV_LOG_INFO, "Longest duraction: %ums @ frame %u\n",
                   (unsigned int)(p->max_phase_time / 1000),
                   p->max_time_decode_order);
        }
        av_log(avctx, AV_LOG_INFO, "PU max=%d\n", rpi->max_pu_msgs);
    }
#endif

    if (rpi->dec_envs != NULL)
    {
        for (int i; i < avctx->thread_count && rpi->dec_envs[i] != NULL; ++i) {
            dec_env_delete(rpi->dec_envs[i]);
        }
        av_freep(&rpi->dec_envs);
    }

    av_rpi_zc_int_env_freep(&rpi->zc);

    gpu_free(&rpi->gcolbuf);

    for (unsigned int i = 0; i != RPIVID_BITBUFS; ++i) {
        gpu_free(rpi->gbitbufs + i);
    }
    for (unsigned int i = 0; i != RPIVID_COEFFBUFS; ++i) {
        gpu_free(rpi->gcoeffbufs + i);
    }

    unmap_devp(&rpi->regs, REGS_SIZE);
    unmap_devp(&rpi->ints, INTS_SIZE);

    if (rpi->gpu_init_type > 0)
        rpi_mem_gpu_uninit();

    if (rpi->mbox_fd >= 0) {
        mbox_release_clock(rpi->mbox_fd);
        mbox_close(rpi->mbox_fd);
    }

    sem_destroy(&rpi->ref_zero);
    sem_destroy(&rpi->coeffbuf_sem);
    sem_destroy(&rpi->bitbuf_sem);

#if TRACE_ENTRY
    printf(">>> %s\n", __func__);
#endif
    return 0;
}

//////////////////////////////////////////////////////////////////////////////

static int rpi_hevc_init(AVCodecContext *avctx) {
    RPI_T * const rpi = avctx->internal->hwaccel_priv_data;
//    const char *err;

#if TRACE_ENTRY
    printf("<<< %s\n", __func__);
#endif

    if (avctx->width>4096 || avctx->height>4096) {
        av_log(NULL, AV_LOG_FATAL, "Picture size %dx%d exceeds 4096x4096 maximum for HWAccel\n", avctx->width, avctx->height);
        return AVERROR(ENOTSUP);
    }

    memset(rpi, 0, sizeof(*rpi));

    rpi->mbox_fd = -1;
    rpi->decode_order = 0;

    // Initial PU/COEFF stream buffer split chosen as worst case seen so far
    rpi->max_pu_msgs = 768; // 7.2 says at most 1611 messages per CTU


    atomic_store(&rpi->ref_count, 1);
    sem_init(&rpi->ref_zero, 0, 0);

    sem_init(&rpi->bitbuf_sem,   0, RPIVID_BITBUFS);
    sem_init(&rpi->coeffbuf_sem, 0, RPIVID_COEFFBUFS);

    pthread_mutex_init(&rpi->phase_lock, NULL);

    if ((rpi->mbox_fd = mbox_open()) < 0)
    {
        av_log(avctx, AV_LOG_ERROR, "Failed to open mailbox\n");
        goto fail;
    }
    mbox_request_clock(rpi->mbox_fd);

    if ((rpi->regs = map_dev(avctx, REGS_NAME, REGS_SIZE)) == NULL ||
        (rpi->ints = map_dev(avctx, INTS_NAME, INTS_SIZE)) == NULL) {
        av_log(avctx, AV_LOG_ERROR, "Failed to open rpivid devices\n");
        goto fail;
    }

    if ((rpi->gpu_init_type = rpi_mem_gpu_init(0)) < 0) {
        av_log(avctx, AV_LOG_ERROR, "Failed to init GPU\n");
        goto fail;
    }

    if ((rpi->dec_envs = av_mallocz(sizeof(dec_env_t *) * avctx->thread_count)) == NULL) {
        av_log(avctx, AV_LOG_ERROR, "Failed to alloc %d dec envs\n", avctx->thread_count);
        goto fail;
    }

    rpi->col_stride = rnd64(avctx->width);
    rpi->col_picsize = rpi->col_stride * (((avctx->height + 63) & ~63) >> 4);
    if (gpu_malloc_uncached(rpi->col_picsize * RPIVID_COL_PICS, &rpi->gcolbuf) != 0)
    {
        av_log(avctx, AV_LOG_ERROR, "Failed to allocate col mv buffer\n");
        goto fail;
    }

    for (unsigned int i = 0; i != RPIVID_BITBUFS; ++i) {
        if (gpu_malloc_uncached(RPIVID_BITBUF_SIZE, rpi->gbitbufs + i) != 0)
        {
            av_log(avctx, AV_LOG_ERROR, "Failed to allocate bitbuf %d\n", i);
            goto fail;
        }
    }

    for (unsigned int i = 0; i != RPIVID_COEFFBUFS; ++i) {
        if (gpu_malloc_uncached(RPIVID_COEFFBUF_SIZE, rpi->gcoeffbufs + i) != 0)
        {
            av_log(avctx, AV_LOG_ERROR, "Failed to allocate coeffbuf %d\n", i);
            goto fail;
        }
    }

    av_log(avctx, AV_LOG_INFO, "RPI HEVC h/w accel init OK\n");

    return 0;

fail:
    rpi_hevc_free(avctx);
    return AVERROR_EXTERNAL;
}

//////////////////////////////////////////////////////////////////////////////

const AVHWAccel ff_hevc_rpi4_8_hwaccel = {
    .name           = "hevc_rpi4_8",
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_HEVC,
    .pix_fmt        = AV_PIX_FMT_RPI4_8,
    .alloc_frame    = rpivid_hevc_alloc_frame,
    .start_frame    = rpi_hevc_start_frame,
    .end_frame      = rpi_hevc_end_frame,
    .abort_frame    = rpi_hevc_abort_frame,
    .decode_slice   = rpi_hevc_decode_slice,
    .init           = rpi_hevc_init,
    .uninit         = rpi_hevc_free,
    .priv_data_size = sizeof(RPI_T),
    .caps_internal  = HWACCEL_CAP_ASYNC_SAFE | HWACCEL_CAP_MT_SAFE,
};

const AVHWAccel ff_hevc_rpi4_10_hwaccel = {
    .name           = "hevc_rpi4_10",
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_HEVC,
    .pix_fmt        = AV_PIX_FMT_RPI4_10,
    .alloc_frame    = rpivid_hevc_alloc_frame,
    .start_frame    = rpi_hevc_start_frame,
    .end_frame      = rpi_hevc_end_frame,
    .abort_frame    = rpi_hevc_abort_frame,
    .decode_slice   = rpi_hevc_decode_slice,
    .init           = rpi_hevc_init,
    .uninit         = rpi_hevc_free,
    .priv_data_size = sizeof(RPI_T),
    .caps_internal  = HWACCEL_CAP_ASYNC_SAFE | HWACCEL_CAP_MT_SAFE,
};

