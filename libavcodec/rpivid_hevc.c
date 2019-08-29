// FFMPEG HEVC decoder hardware accelerator
// Andrew Holme, Argon Design Ltd
// Copyright (c) June 2017 Raspberry Pi Ltd

#include <stdio.h>
//#include <dlfcn.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "fftools/ffmpeg.h"
#include "libavutil/avassert.h"
#include "libavutil/imgutils.h"
#include "avcodec.h"
#include "hwaccel.h"

#include "rpivid_hevc.h"
#include "rpi_zc.h"
#include "rpi_mem.h"
#include "rpi_zc_frames.h"

#include "rpivid_axi.h"

#define OPT_GBUF_CACHED 0

#define GBUF_SIZE (16 << 20)

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>

long syscall(long number, ...);

static inline int gettid(void)
{
    return syscall(SYS_gettid);
}

//============================================================================

#define TRACE_DEV 0

#define REGS_NAME "/dev/argon-hevcmem"
#define REGS_SIZE 0x10000
#define INTS_NAME "/dev/argon-intcmem"
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
    rpi->regs[addr >> 2] = MANGLE64(data);
}

static inline void apb_write_addr(const RPI_T * const rpi, const dec_env_t * const de, const uint32_t addr, const uint32_t data)
{
#if TRACE_DEV
    printf("P %x %08x\n", addr, data);
#endif

    rpi->regs[addr >> 2] = data + (MANGLE(de->gbuf.vc) >> 6);
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

static inline void axi_write(const dec_env_t * const de, uint32_t dst64, size_t len, const void * src)
{
#if TRACE_DEV
    printf("L %08x %08x\n", dst64 << 6, len);
#endif

    av_assert0((dst64 << 6) + len <= de->gbuf.numbytes);

    memcpy(de->gbuf.arm + (dst64 << 6), src, len);
}

static uint32_t axi_addr64(const dec_env_t * const de, const uint32_t a64)
{
    return a64 + (MANGLE(de->gbuf.vc) >> 6);
}

static inline void axi_flush(const dec_env_t * const de, size_t len)
{
    // ****
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

#if TRACE_DEV
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

// Array of constants for scaling factors
static const uint32_t scaling_factor_offsets[4][6] = {
    // MID0    MID1    MID2    MID3    MID4    MID5
    {0x0000, 0x0010, 0x0020, 0x0030, 0x0040, 0x0050},   // SID0 (4x4)
    {0x0060, 0x00A0, 0x00E0, 0x0120, 0x0160, 0x01A0},   // SID1 (8x8)
    {0x01E0, 0x02E0, 0x03E0, 0x04E0, 0x05E0, 0x06E0},   // SID2 (16x16)
    {0x07E0,      0,      0, 0x0BE0,      0,      0}};  // SID3 (32x32)

// ffmpeg places SID3,MID1 where matrixID 3 normally is

//////////////////////////////////////////////////////////////////////////////
// Scaling factors

static void expand_scaling_list(
    dec_env_t * const de,
    const ScalingList * const scaling_list, // scaling list structure from ffmpeg
    const unsigned int sizeID,
    const unsigned int matrixID)
{
    unsigned int x, y, i, blkSize = 4<<sizeID;
    const uint32_t index_offset = scaling_factor_offsets[sizeID][matrixID];

    for (x=0; x<blkSize; x++) {
        for (y=0; y<blkSize; y++) {
            uint32_t index = index_offset + x + y*blkSize;
            // Derivation of i to match indexing in ff_hevc_hls_residual_coding
            switch (sizeID) {
                case 0: i = (y<<2) + x;             break;
                case 1: i = (y<<3) + x;             break;
                case 2: i = ((y>>1)<<3) + (x>>1);   break;
                case 3: i = ((y>>2)<<3) + (x>>2);
            }
            de->scaling_factors[index] = scaling_list->sl[sizeID][matrixID][i];
        }
    }
    if (sizeID>1)
        de->scaling_factors[index_offset] =
            scaling_list->sl_dc[sizeID-2][matrixID];
}

static void populate_scaling_factors(dec_env_t * const de, const HEVCContext * const s) {
    const ScalingList * const sl =
        s->ps.pps->scaling_list_data_present_flag ? &s->ps.pps->scaling_list
                                                  : &s->ps.sps->scaling_list;
    int sid, mid;
    for (sid=0; sid<3; sid++)
        for (mid=0; mid<6; mid++)
            expand_scaling_list(de, sl, sid, mid);

    // second scaling matrix for 32x32 is at matrixID 3 not 1 in ffmpeg
    expand_scaling_list(de, sl, 3, 0);
    expand_scaling_list(de, sl, 3, 3);
}

//////////////////////////////////////////////////////////////////////////////
// Probabilities

// **** Horrid type punning here &I don't understand why it is wanted
static void populate_prob_tables(dec_env_t * const de, const HEVCContext * const s) {
    struct RPI_PROB * const dst = &de->probabilities;
    const struct FFM_PROB * const src = (struct FFM_PROB *) s->HEVClc->cabac_state;
    #define PROB_CPSZ(to, from, sz) memcpy(dst->to, src->from, sz)
    #define PROB_COPY(to, from)     memcpy(dst->to, src->from, sizeof(dst->to))
    memset(dst, 0, sizeof(*dst));
    PROB_COPY(SAO_MERGE_FLAG           , sao_merge_flag                 );
    PROB_COPY(SAO_TYPE_IDX             , sao_type_idx                   );
    PROB_COPY(SPLIT_FLAG               , split_coding_unit_flag         );
    PROB_COPY(CU_SKIP_FLAG             , skip_flag                      );
    PROB_COPY(CU_TRANSQUANT_BYPASS_FLAG, cu_transquant_bypass_flag      );
    PROB_COPY(PRED_MODE                , pred_mode_flag                 );
    PROB_COPY(PART_SIZE                , part_mode                      );
    PROB_COPY(INTRA_PRED_MODE          , prev_intra_luma_pred_flag      );
    PROB_COPY(CHROMA_PRED_MODE         , intra_chroma_pred_mode         );
    PROB_COPY(MERGE_FLAG_EXT           , merge_flag                     );
    PROB_COPY(MERGE_IDX_EXT            , merge_idx                      );
    PROB_COPY(INTER_DIR                , inter_pred_idc                 );
    PROB_COPY(REF_PIC                  , ref_idx_l0                     );
    PROB_COPY(MVP_IDX                  , mvp_lx_flag                    );
    PROB_CPSZ(MVD+0                    , abs_mvd_greater0_flag+0    ,  1); // ABS_MVD_GREATER0_FLAG[1] not used
    PROB_CPSZ(MVD+1                    , abs_mvd_greater1_flag+1    ,  1); // ABS_MVD_GREATER1_FLAG[0] not used
    PROB_COPY(QT_ROOT_CBF              , no_residual_data_flag          );
    PROB_COPY(TRANS_SUBDIV_FLAG        , split_transform_flag           );
    PROB_CPSZ(QT_CBF                   , cbf_luma                   ,  2);
    PROB_CPSZ(QT_CBF+2                 , cbf_cb_cr                  ,  4);
    PROB_COPY(DQP                      , cu_qp_delta                    );
    PROB_COPY(ONE_FLAG                 , coeff_abs_level_greater1_flag  );
    PROB_COPY(LASTX                    , last_significant_coeff_x_prefix);
    PROB_COPY(LASTY                    , last_significant_coeff_y_prefix);
    PROB_COPY(SIG_CG_FLAG              , significant_coeff_group_flag   );
    PROB_COPY(ABS_FLAG                 , coeff_abs_level_greater2_flag  );
    PROB_COPY(TRANSFORMSKIP_FLAG       , transform_skip_flag            );
    PROB_CPSZ(SIG_FLAG                 , significant_coeff_flag     , 42);
}

//////////////////////////////////////////////////////////////////////////////
// Phase 1 command and bit FIFOs

// ???? uint16_t addr - put in uint32_t
static int p1_apb_write(dec_env_t * const de, const uint16_t addr, const uint32_t data) {
    if (de->cmd_len==de->cmd_max)
        av_assert0(de->cmd_fifo = realloc(de->cmd_fifo, (de->cmd_max*=2)*sizeof(struct RPI_CMD)));
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

static void WriteProb(dec_env_t * const de) {
    int i;
    const uint8_t *p = (uint8_t *) &de->probabilities;
    for (i=0; i<sizeof(struct RPI_PROB); i+=4, p+=4)
        p1_apb_write(de, 0x1000+i, p[0] + (p[1]<<8) + (p[2]<<16) + (p[3]<<24));
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

static void alloc_picture_space(dec_env_t * const de, const HEVCContext * const s)
{
    const HEVCSPS * const sps = s->ps.sps;
    int CtbSizeY = 1<<sps->log2_ctb_size;
    int x64 = AXI_BASE64;

    de->PicWidthInCtbsY  = (sps->width + CtbSizeY - 1) / CtbSizeY;  //7-15
    de->PicHeightInCtbsY = (sps->height + CtbSizeY - 1) / CtbSizeY;  //7-17
#if 0
    // collocated reads/writes
    if (sps->sps_temporal_mvp_enabled_flag) {
        // 128 bits = 16 bytes per MV, one for every 16*16
        int collocatedStride64 = (de->PicWidthInCtbsY * (CtbSizeY/16) * 16 + 63)>>6;
        de->mvframebytes64 = de->PicHeightInCtbsY * (CtbSizeY/16) * collocatedStride64;
        de->mvstorage64 = x64;
        x64 += de->mvframebytes64 * 17; // Leave space for 17 reference pictures
        de->colstride64 = collocatedStride64;
        de->mvstride64 = collocatedStride64;
    }
#endif
    de->pubase64 = x64;
}

static int alloc_stream_space(dec_env_t * const de, const HEVCContext *s) {
    int stride64;
    int x64 = de->pubase64;

    stride64 = 1 + (de->max_pu_msgs*2*de->PicWidthInCtbsY)/64;
    de->pubase64 = x64;
    de->pustep64 = stride64;
    x64 += de->PicHeightInCtbsY*stride64 ;

    stride64 = de->max_coeff64;
    de->coeffbase64 = x64;
    de->coeffstep64 = stride64;
    x64 += de->PicHeightInCtbsY*stride64;

    return x64;
}

//////////////////////////////////////////////////////////////////////////////
// Start or restart phase 1

static void phase1_begin(const RPI_T * const rpi, const dec_env_t * const de) {
    apb_write_addr(rpi, de, RPI_PUWBASE, de->pubase64);
    apb_write(rpi, RPI_PUWSTRIDE, de->pustep64);
    apb_write_addr(rpi, de, RPI_COEFFWBASE, de->coeffbase64);
    apb_write(rpi, RPI_COEFFWSTRIDE, de->coeffstep64);
}

//////////////////////////////////////////////////////////////////////////////
// Handle PU and COEFF stream overflow

static int check_status(const RPI_T * const rpi, dec_env_t * const de) {
    int status, c, p;

    // this is the definition of successful completion of phase 1
    // it assures that status register is zero and all blocks in each tile have completed
    if (apb_read(rpi, RPI_CFSTATUS) == apb_read(rpi, RPI_CFNUM))
        return 0;

    status = apb_read(rpi, RPI_STATUS);

    p = (status>>4)&1;
    c = (status>>3)&1;
    if (p|c) { // overflow?
        if (p) de->max_pu_msgs += de->max_pu_msgs/2;
        if (c) de->max_coeff64 += de->max_coeff64/2;
        return 1;
    }
    return 2;
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
// Workaround for 3 December 2016 commit 8dfba25ce89b62c80ba83e2116d549176c376144
// https://github.com/libav/libav/commit/8dfba25ce89b62c80ba83e2116d549176c376144
// This commit prevents multi-threaded hardware acceleration by locking hwaccel_mutex
// around codec->decode() calls.  Workaround is to unlock and relock before returning.

static void hwaccel_mutex(AVCodecContext *avctx, int (*action) (pthread_mutex_t *)) {
    struct FrameThreadContext {
        void *foo1, *foo2; // must match struct layout in pthread_frame.c
        pthread_mutex_t foo3, hwaccel_mutex;
    };
    struct PerThreadContext {
        struct FrameThreadContext *parent;
    };
    struct PerThreadContext *p = avctx->internal->thread_ctx;
    if (avctx->thread_count>1) action(&p->parent->hwaccel_mutex);
}

//////////////////////////////////////////////////////////////////////////////

// Doesn't attempt to remove from context as we should only do this at the end
// of time or on create error
static void
dec_env_delete(dec_env_t * const de)
{
    gpu_free(&de->gbuf);

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
    int rv;

    if (de == NULL)
        return NULL;

#if OPT_GBUF_CACHED
    rv = gpu_malloc_cached(GBUF_SIZE, &de->gbuf);
#else
    rv = gpu_malloc_uncached(GBUF_SIZE, &de->gbuf);
#endif

#if TRACE_DEV
    printf("A %x arm=%p vc=%08x\n", de->gbuf.numbytes, de->gbuf.arm, de->gbuf.vc);
#endif

    if (rv != 0) {
        av_log(avctx, AV_LOG_ERROR, "Failed to allocate GPU mem (%d) for thread\n", GBUF_SIZE);
        av_free(de);
        return NULL;
    }

    de->rpi = rpi;
    de->avctx = avctx;
    sem_init(&de->phase_wait, 0, 0);

    // Initial PU/COEFF stream buffer sizes chosen so jellyfish40.265 requires 1 overflow/restart
    de->max_pu_msgs = 2+340; // 7.2 says at most 1611 messages per CTU
    de->max_coeff64 = 2+1404;

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

//----------------------------------------------------------------------------



static int
wait_phase(RPI_T * const rpi, dec_env_t * const de, dec_env_t ** const q)
{
    int rv = 0;

    pthread_mutex_lock(&rpi->phase_lock);
    de->phase_next = *q;
    *q = de;
    pthread_mutex_unlock(&rpi->phase_lock);

    if (de->phase_next != NULL) {
        while ((rv = sem_wait(&de->phase_wait)) == -1 && errno == EINTR)
            /* Loop */;
    }

    return rv;
}

static void
post_phase(RPI_T * const rpi, dec_env_t * const de, dec_env_t ** q)
{
    dec_env_t * next_de = NULL;
    int next_poc = INT_MAX;

    pthread_mutex_lock(&rpi->phase_lock);

    while (*q != NULL) {
        dec_env_t * const t_de = *q;

        if (t_de == de) {
            // This is us - remove from Q
            // Do not null out current phase_next yet
            *q = t_de->phase_next;
        }
        else {
            // Scan for lowest waiting POC to poke
            HEVCContext * const t_s = t_de->avctx->priv_data;
            if (t_s->poc <= next_poc) {
                next_poc = t_s->poc;
                next_de = t_de;
            }
        }
        q = &t_de->phase_next;
    }

    de->phase_next = NULL; // Tidy
    pthread_mutex_unlock(&rpi->phase_lock);

    if (next_de != NULL)
        sem_post(&next_de->phase_wait);
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

    printf("<<< %s[%p]\n", __func__, de);

    if (de == NULL) {
        av_log(avctx, AV_LOG_ERROR, "%s: Cannot find find context for thread\n", __func__);
        return -1;
    }

    de->decode_order = rpi->decode_order++;

    ff_thread_finish_setup(avctx); // Allow next thread to enter rpi_hevc_start_frame
    hwaccel_mutex(avctx, pthread_mutex_unlock);


    // Enforcing phase 1 order precludes busy waiting for phase 2
    for (;;) {
        pthread_mutex_lock  (&rpi->mutex_phase1);
        if (de->decode_order == rpi->phase1_order) break;
        pthread_mutex_unlock(&rpi->mutex_phase1);
    }
    rpi->phase1_order++;


    alloc_picture_space(de, s);
    de->bit_len = 0;
    de->cmd_len = 0;
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

    de->num_slice_msgs=0;
    cmd_slice = 0;
    if (sh->slice_type==HEVC_SLICE_I) cmd_slice = 1;
    if (sh->slice_type==HEVC_SLICE_P) cmd_slice = 2;
    if (sh->slice_type==HEVC_SLICE_B) cmd_slice = 3;

    if (sh->slice_type!=HEVC_SLICE_I) {
        cmd_slice += sh->nb_refs[L0]<<2;
        cmd_slice += sh->nb_refs[L1]<<6;
    }
    if (sh->slice_type==HEVC_SLICE_P
    ||  sh->slice_type==HEVC_SLICE_B) de->max_num_merge_cand = sh->max_num_merge_cand;

    cmd_slice += de->max_num_merge_cand<<11;

    if (sh->slice_temporal_mvp_enabled_flag) {
        if      (sh->slice_type==HEVC_SLICE_B) de->collocated_from_l0_flag = sh->collocated_list==L0;
        else if (sh->slice_type==HEVC_SLICE_P) de->collocated_from_l0_flag = 1;
    }
    cmd_slice += de->collocated_from_l0_flag<<14;

    if (sh->slice_type==HEVC_SLICE_P || sh->slice_type==HEVC_SLICE_B) {

        int NoBackwardPredFlag = 1; // Flag to say all reference pictures are from the past
        for(i=L0; i<=L1; i++) {
            for(rIdx=0; rIdx <sh->nb_refs[i]; rIdx++) {
                HEVCFrame *f = s->ref->refPicList[i].ref[rIdx];
                HEVCFrame *c = s->ref; // CurrentPicture
                if (c->poc < f->poc) NoBackwardPredFlag = 0;
            }
        }

        de->collocated_ref_idx = sh->collocated_ref_idx;
        if (s->ref->refPicList && s->ref->collocated_ref)
            for (i=0; i<HEVC_MAX_REFS; i++) {
                if (i<sh->nb_refs[L1]) de->RefPicList[1][i] = s->ref->refPicList[1].ref[i] - s->DPB;
                if (i<sh->nb_refs[L0]) de->RefPicList[0][i] = s->ref->refPicList[0].ref[i] - s->DPB;
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

    // collocated reads/writes
    if (sps->sps_temporal_mvp_enabled_flag) {
        de->dpbno_col = sh->slice_type == HEVC_SLICE_I ? 0 :
            de->RefPicList[sh->slice_type==HEVC_SLICE_B && de->collocated_from_l0_flag==0][de->collocated_ref_idx];
#if 0
        de->mvbase64 = de->mvstorage64 + CurrentPicture * de->mvframebytes64;
        if (sh->slice_type==HEVC_SLICE_I) {
            // Collocated picture not well defined here.  Use mvbase or previous value
            if (sh->first_slice_in_pic_flag)
                de->colbase64 = de->mvbase64; // Ensure we don't read garbage
        }
        else
            de->colbase64 = de->mvstorage64 + colPic * de->mvframebytes64;
#endif
    }
}

//////////////////////////////////////////////////////////////////////////////
// End frame

static int rpi_hevc_end_frame(AVCodecContext * const avctx) {
    RPI_T * const rpi = avctx->internal->hwaccel_priv_data;
    const HEVCContext * const s = avctx->priv_data;
    const HEVCPPS * const pps = s->ps.pps;
    const HEVCSPS * const sps = s->ps.sps;
    const SliceHeader * const sh = &s->sh;
    dec_env_t * const de = dec_env_get(avctx,  rpi);
//    int jump = sps->bit_depth>8?96:128;
//    int CurrentPicture = s->ref - s->DPB;
    AVFrame * const f = s->ref->frame;
    int last_x = pps->col_bd[pps->num_tile_columns]-1;
    int last_y = pps->row_bd[pps->num_tile_rows]-1;
    const unsigned int dpbno_cur = s->ref - s->DPB;

    int i, a64;
//    char *buf;
    int status = 1;

    printf("<<< %s[%p]\n", __func__, de);

    if (de == NULL) {
        av_log(avctx, AV_LOG_ERROR, "%s: Cannot find find context for thread\n", __func__);
        return -1;
    }

#if 0
    // Wait for frames
    // ***********************************

    {
        atomic_int *progress = s->ref->tf.progress ? (atomic_int*)s->ref->tf.progress->data : NULL;
        printf("POC=%d, Seq=%d, Our progress=%d\n",
               s->ref->poc, s->ref->sequence,
               progress == NULL ? -999 : atomic_load_explicit(&progress[1], memory_order_acquire));
    }

    printf("Pre-wait [%p]\n", de);
    for(i=L0; i<=L1; i++)
    {
        for (unsigned int rIdx=0; rIdx <sh->nb_refs[i]; rIdx++)
        {
            HEVCFrame *f1 = s->ref->refPicList[i].ref[rIdx];

            atomic_int *progress = f1->tf.progress ? (atomic_int*)f1->tf.progress->data : NULL;

            printf("Check L%d:R%d POC=%d,Seq=%d: progress=%d\n", i, rIdx, f1->poc, f1->sequence,
                   progress == NULL ? -999 : atomic_load_explicit(&progress[1], memory_order_acquire));

            ff_thread_await_progress(&f1->tf, 2, 1);
        }
    }
    printf("Post-wait [%p]\n", de);
#endif

    // End of phase 1 command compilation
    if (pps->entropy_coding_sync_enabled_flag) {
        if (de->wpp_entry_x<2 && de->PicWidthInCtbsY>2)
            wpp_pause(de, last_y);
    }
    p1_apb_write(de, RPI_STATUS, 1 + (last_x<<5) + (last_y<<18));

    // Phase 1 ...
//    wait_phase(rpi, de, &rpi->phase1_req);

    for (;;) {
        // (Re-)allocate PU/COEFF stream space
        a64 = alloc_stream_space(de, s);
        // Send bitstream data
        for (i=0; i < de->bit_len; i++) {
            if ((unsigned int)a64 + (((unsigned int)de->bit_fifo[i].len + 31) >> 6) > (1U << 20)) {
                av_log(avctx, AV_LOG_ERROR, "Out of HEVC intermediate memory - maybe too many threads");
                status = -1;
                break;
            }
            axi_write(de, a64, de->bit_fifo[i].len, de->bit_fifo[i].ptr);
            de->cmd_fifo[de->bit_fifo[i].cmd].data = axi_addr64(de, a64); // Set BFBASE
            a64 += (de->bit_fifo[i].len+63)/64;
        }
        // Send phase 1 commands (cache flush on real hardware)
        axi_write(de, a64, de->cmd_len * sizeof(struct RPI_CMD), de->cmd_fifo);

        axi_flush(de, (a64 << 6) + de->cmd_len * sizeof(struct RPI_CMD));

        phase1_begin(rpi, de);
        // Trigger command FIFO
        apb_write(rpi, RPI_CFNUM, de->cmd_len);
#if TRACE_DEV
        apb_dump_regs(rpi, 0x0, 32);
        apb_dump_regs(rpi, 0x8000, 24);
        axi_dump(de, ((uint64_t)a64)<<6, de->cmd_len * sizeof(struct RPI_CMD));
#endif
        apb_write_addr(rpi, de, RPI_CFBASE, a64);
        printf("P1 start\n");
        int_wait(rpi, 1);
        printf("P1 done\n");
        status = check_status(rpi, de);
        if (status != 1) break; // No PU/COEFF overflow?
        av_assert0(0);
    }

    pthread_mutex_unlock(&rpi->mutex_phase1);
//    post_phase(rpi, de, &rpi->phase1_req);

    if (status != 0) {
        av_log(avctx, AV_LOG_WARNING, "Phase 1 decode error\n");
        goto fail;
    }

//    wait_phase(rpi, de, &rpi->phase2_req);
    printf("Phase 2 start [%p]\n", de);
    for (;;) {
        pthread_mutex_lock  (&rpi->mutex_phase2);
        if (de->decode_order == rpi->phase2_order) break;
        pthread_mutex_unlock(&rpi->mutex_phase2);
    }
    rpi->phase2_order++;

    apb_write_addr(rpi, de, RPI_PURBASE, de->pubase64);
    apb_write(rpi, RPI_PURSTRIDE, de->pustep64);
    apb_write_addr(rpi, de, RPI_COEFFRBASE, de->coeffbase64);
    apb_write(rpi, RPI_COEFFRSTRIDE, de->coeffstep64);

    {
//        const AVRpiZcRefPtr fr_buf = f ? av_rpi_zc_ref(avctx, f, f->format, 0) : NULL;
//        uint32_t handle = fr_buf ? av_rpi_zc_vc_handle(fr_buf):0;
//    printf("%s cur:%d fr:%p handle:%d YUV:%x:%x ystride:%d ustride:%d ah:%d\n", __FUNCTION__, CurrentPicture, f, handle, get_vc_address_y(f), get_vc_address_u(f), f->linesize[0], f->linesize[1],  f->linesize[3]);
        apb_write_vc_addr(rpi, RPI_OUTYBASE, get_vc_address_y(f));
        apb_write_vc_addr(rpi, RPI_OUTCBASE, get_vc_address_u(f));
        apb_write(rpi, RPI_OUTYSTRIDE, f->linesize[3] * 128 / 64);
        apb_write(rpi, RPI_OUTCSTRIDE, f->linesize[3] * 128 / 64);
//        av_rpi_zc_unref(fr_buf);
    }

    for(i=0; i<16; i++) {
        apb_write(rpi, 0x9000+16*i, 0);
        apb_write(rpi, 0x9004+16*i, 0);
        apb_write(rpi, 0x9008+16*i, 0);
        apb_write(rpi, 0x900C+16*i, 0);
    }

    for(i=L0; i<=L1; i++)
    {
        for (unsigned int rIdx=0; rIdx <sh->nb_refs[i]; rIdx++)
        {
            const HEVCFrame *f1 = s->ref->refPicList[i].ref[rIdx];
            const HEVCFrame *c = s->ref; // CurrentPicture
            int pic = f1 - s->DPB;
            // Make sure pictures are in range 0 to 15
            int adjusted_pic = f1<c? pic : pic-1;
            const struct HEVCFrame *hevc = &s->DPB[pic];
            const AVFrame *fr = hevc ? hevc->frame : NULL;
//                const AVRpiZcRefPtr fr_buf = fr ? av_rpi_zc_ref(avctx, fr, fr->format, 0) : NULL;
//                uint32_t handle = fr_buf ? av_rpi_zc_vc_handle(fr_buf):0;
        //        printf("%s pic:%d (%d,%d,%d) fr:%p handle:%d YUV:%x:%x\n", __FUNCTION__, adjusted_pic, i, rIdx, pic, fr, handle, get_vc_address_y(fr), get_vc_address_u(fr));
            av_assert0(adjusted_pic >= 0 && adjusted_pic < 16);
            apb_write_vc_addr(rpi, 0x9000+16*adjusted_pic, get_vc_address_y(fr));
            apb_write_vc_addr(rpi, 0x9008+16*adjusted_pic, get_vc_address_u(fr));
//                apb_write(rpi, RPI_OUTYSTRIDE, fr->linesize[3] * 128 / 64);
//                apb_write(rpi, RPI_OUTCSTRIDE, fr->linesize[3] * 128 / 64);
//                av_rpi_zc_unref(fr_buf);
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

        apb_write(rpi, RPI_COLSTRIDE, rpi->col_stride64);
        apb_write(rpi, RPI_MVSTRIDE,  rpi->col_stride64);
        apb_write_vc_addr(rpi, RPI_MVBASE,  rpi->gcolbuf.vc + dpbno_cur * rpi->col_picsize);
        apb_write_vc_addr(rpi, RPI_COLBASE, rpi->gcolbuf.vc + de->dpbno_col * rpi->col_picsize);
    }

#if TRACE_DEV
    apb_dump_regs(rpi, 0x0, 32);
    apb_dump_regs(rpi, 0x8000, 24);
#endif

    apb_write(rpi, RPI_NUMROWS, de->PicHeightInCtbsY);
    apb_read(rpi, RPI_NUMROWS); // Read back to confirm write has reached block


    int_wait(rpi, 2);
    printf("Phase 2 done [%p]: POC=%d, Seq=%d\n", de, s->ref->poc, s->ref->sequence);

//    post_phase(rpi, de, &rpi->phase2_req);

    // Flush frame for CPU access
    // Arguably the best place would be at the start of phase 2 but here
    // will overlap with the wait
    //
    // * Even better would be to have better lock/unlock control in ZC for external access
    {
        rpi_cache_buf_t cbuf;
        rpi_cache_flush_env_t * const fe = rpi_cache_flush_init(&cbuf);
        rpi_cache_flush_add_frame(fe, f, RPI_CACHE_FLUSH_MODE_INVALIDATE);
        rpi_cache_flush_finish(fe);
    }

fail:
//    ff_thread_report_progress(&s->ref->tf, 2, 1);

    pthread_mutex_unlock(&rpi->mutex_phase2);
    hwaccel_mutex(avctx, pthread_mutex_lock);

    return 0;
}

//////////////////////////////////////////////////////////////////////////////

static void WriteBitstream(dec_env_t * const de, const HEVCContext * const s) {
    const int rpi_use_emu = 0; // FFmpeg removes emulation prevention bytes
    const int offset = 0; // Always 64-byte aligned in sim, need not be on real hardware
    const GetBitContext *gb = &s->HEVClc->gb;
    const int len = 1 + gb->size_in_bits/8 - gb->index/8;
    const void *ptr = &gb->buffer[gb->index/8];

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

    if (ctb_addr_ts) wpp_end_previous_slice(de, s, ctb_addr_ts);
    pre_slice_decode(de, s);
    WriteBitstream(de, s);
    if (ctb_addr_ts==0 || indep || de->PicWidthInCtbsY==1) WriteProb(de);
    else if (ctb_col==0) p1_apb_write(de, RPI_TRANSFER, PROB_RELOAD);
    else resetQPY=0;
    program_slicecmds(de, s->slice_idx);
    new_slice_segment(de, s);
    wpp_entry_point(de, s, indep, resetQPY, ctb_addr_ts);
    for (i=0; i<s->sh.num_entry_point_offsets; i++) {
        int ctb_addr_rs = pps->ctb_addr_ts_to_rs[ctb_addr_ts];
        int ctb_row = ctb_addr_rs / de->PicWidthInCtbsY;
        int last_x = de->PicWidthInCtbsY-1;
        if (de->PicWidthInCtbsY>2) wpp_pause(de, ctb_row);
        p1_apb_write(de, RPI_STATUS, (ctb_row<<18) + (last_x<<5) + 2);
        if (de->PicWidthInCtbsY==2) p1_apb_write(de, RPI_TRANSFER, PROB_BACKUP);
        if (de->PicWidthInCtbsY==1) WriteProb(de);
        else p1_apb_write(de, RPI_TRANSFER, PROB_RELOAD);
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
    if (resetQPY) WriteProb(de);
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
        WriteProb(de);
        ctb_addr_ts += pps->column_width[tile_x] * pps->row_height[tile_y];
        new_entry_point(de, s, 0, 1, ctb_addr_ts);
    }
}

//////////////////////////////////////////////////////////////////////////////

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

    printf("<<< %s[%p]\n", __func__, de);

    if (de == NULL) {
        av_log(avctx, AV_LOG_ERROR, "%s: Cannot find find context for thread\n", __func__);
        return -1;
    }

    ff_hevc_cabac_init(s, ctb_addr_ts);
    if (s->ps.sps->scaling_list_enable_flag)
        populate_scaling_factors(de, s);
    populate_prob_tables(de, s);
    pps->entropy_coding_sync_enabled_flag? wpp_decode_slice(de, s, ctb_addr_ts)
                                             : decode_slice(de, s, ctb_addr_ts);
    return 0;
}

//////////////////////////////////////////////////////////////////////////////
// Bind to socket client
#if 0
static int open_socket_client(RPI_T *rpi, const char *so) {
     *(void **) &rpi->ctrl_ffmpeg_init = rpi_ctrl_ffmpeg_init;
//     *(void **) &rpi->apb_write        = rpi_apb_write;
//     *(void **) &rpi->apb_write_addr   = rpi_apb_write_addr;
//     *(void **) &rpi->apb_read         = rpi_apb_read;
//     *(void **) &rpi->apb_read_drop    = rpi_apb_read_drop;
//     *(void **) &rpi->axi_write        = rpi_axi_write;
//     *(void **) &rpi->axi_read_alloc   = rpi_axi_read_alloc;
//     *(void **) &rpi->axi_read_tx      = rpi_axi_read_tx;
//     *(void **) &rpi->axi_read_rx      = rpi_axi_read_rx;
//     *(void **) &rpi->axi_get_addr     = rpi_axi_get_addr;
//     *(void **) &rpi->apb_dump_regs    = rpi_apb_dump_regs;
//     *(void **) &rpi->axi_dump         = rpi_axi_dump;
//     *(void **) &rpi->axi_flush        = rpi_axi_flush;
     *(void **) &rpi->wait_interrupt   = rpi_wait_interrupt;
     *(void **) &rpi->ctrl_ffmpeg_free = rpi_ctrl_ffmpeg_free;
    return 1;
}
#endif

//////////////////////////////////////////////////////////////////////////////

static int rpi_hevc_free(AVCodecContext *avctx) {
    RPI_T * const rpi = avctx->internal->hwaccel_priv_data;

    for (int i; i < avctx->thread_count && rpi->dec_envs[i] != NULL; ++i) {
        dec_env_delete(rpi->dec_envs[i]);
    }
    av_freep(&rpi->dec_envs);

    unmap_devp(&rpi->regs, REGS_SIZE);
    unmap_devp(&rpi->ints, INTS_SIZE);
#if 0
    if (rpi->id && rpi->ctrl_ffmpeg_free)
        rpi->ctrl_ffmpeg_free(rpi->id);
#endif
    av_rpi_zc_uninit_local(avctx);
    return 0;
}

//////////////////////////////////////////////////////////////////////////////

static int rpi_hevc_init(AVCodecContext *avctx) {
    RPI_T * const rpi = avctx->internal->hwaccel_priv_data;
//    const char *err;

    if (avctx->width>4096 || avctx->height>4096) {
        av_log(NULL, AV_LOG_FATAL, "Picture size %dx%d exceeds 4096x4096 maximum for HWAccel\n", avctx->width, avctx->height);
        return AVERROR(ENOTSUP);
    }
#if 0
    open_socket_client(rpi, NULL);

    err = rpi->ctrl_ffmpeg_init(NULL, &rpi->id);
    if (err) {
        av_log(NULL, AV_LOG_FATAL, "Could not connect to RPI server: %s\n", err);
        return AVERROR_EXTERNAL;
    }
#endif
    av_rpi_zc_init_local(avctx);

    printf("%s: threads=%d\n", __func__, avctx->thread_count);

    pthread_mutex_init(&rpi->mutex_phase1, NULL);
    pthread_mutex_init(&rpi->mutex_phase2, NULL);
    rpi->decode_order = 0;
    rpi->phase1_order = 0;
    rpi->phase2_order = 0;

    rpi->phase1_req = NULL;
    rpi->phase2_req = NULL;
    pthread_mutex_init(&rpi->phase_lock, NULL);

    if ((rpi->regs = map_dev(avctx, REGS_NAME, REGS_SIZE)) == NULL ||
        (rpi->ints = map_dev(avctx, INTS_NAME, INTS_SIZE)) == NULL) {
        av_log(avctx, AV_LOG_ERROR, "Failed to open rpivid devices\n");
        goto fail;
    }

    if ((rpi->dec_envs = av_mallocz(sizeof(dec_env_t *) * avctx->thread_count)) == NULL) {
        av_log(avctx, AV_LOG_ERROR, "Failed to alloc %d dec envs\n", avctx->thread_count);
        goto fail;
    }

    {
        const size_t colstride = ((avctx->width + 63) & ~63);
        rpi->col_picsize = colstride * (((avctx->height + 63) & ~63) >> 4);
        rpi->col_stride64 = colstride >> 6;
        gpu_malloc_uncached(rpi->col_picsize * RPIVID_COL_PICS, &rpi->gcolbuf);
    }

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
    //.alloc_frame    = rpi_hevc_alloc_frame,
    .start_frame    = rpi_hevc_start_frame,
    .end_frame      = rpi_hevc_end_frame,
    .decode_slice   = rpi_hevc_decode_slice,
    .init           = rpi_hevc_init,
    .uninit         = rpi_hevc_free,
    .priv_data_size = sizeof(RPI_T),
    .caps_internal  = HWACCEL_CAP_ASYNC_SAFE,
};

const AVHWAccel ff_hevc_rpi4_10_hwaccel = {
    .name           = "hevc_rpi4_10",
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_HEVC,
    .pix_fmt        = AV_PIX_FMT_RPI4_10,
    //.alloc_frame    = rpi_hevc_alloc_frame,
    .start_frame    = rpi_hevc_start_frame,
    .end_frame      = rpi_hevc_end_frame,
    .decode_slice   = rpi_hevc_decode_slice,
    .init           = rpi_hevc_init,
    .uninit         = rpi_hevc_free,
    .priv_data_size = sizeof(RPI_T),
    .caps_internal  = HWACCEL_CAP_ASYNC_SAFE,
};

