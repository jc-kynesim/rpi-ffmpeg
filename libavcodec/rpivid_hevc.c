// FFMPEG HEVC decoder hardware accelerator
// Andrew Holme, Argon Design Ltd
// Copyright (c) June 2017 Raspberry Pi Ltd

#include <stdio.h>
#include <dlfcn.h>

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

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>

long syscall(long number, ...);

static inline int gettid(void)
{
    return syscall(SYS_gettid);
}

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
    RPI_T *rpi,
    const ScalingList *scaling_list, // scaling list structure from ffmpeg
    uint8_t sizeID, uint8_t matrixID)
{
    uint8_t x, y, i, blkSize = 4<<sizeID;
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
            rpi->scaling_factors[index] = scaling_list->sl[sizeID][matrixID][i];
        }
    }
    if (sizeID>1)
        rpi->scaling_factors[index_offset] =
            scaling_list->sl_dc[sizeID-2][matrixID];
}

static void populate_scaling_factors(RPI_T *rpi, HEVCContext *s) {
    const ScalingList *sl =
        s->ps.pps->scaling_list_data_present_flag ? &s->ps.pps->scaling_list
                                                  : &s->ps.sps->scaling_list;
    int sid, mid;
    for (sid=0; sid<3; sid++)
        for (mid=0; mid<6; mid++)
            expand_scaling_list(rpi, sl, sid, mid);

    // second scaling matrix for 32x32 is at matrixID 3 not 1 in ffmpeg
    expand_scaling_list(rpi, sl, 3, 0);
    expand_scaling_list(rpi, sl, 3, 3);
}

//////////////////////////////////////////////////////////////////////////////
// Probabilities

static void populate_prob_tables(RPI_T *rpi, HEVCContext *s) {
    struct RPI_PROB *dst = &rpi->probabilities;
    struct FFM_PROB *src = (struct FFM_PROB *) s->HEVClc->cabac_state;
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
// Read YUV data from socket server

static int bytes_per_line(const HEVCSPS *sps, int jump, int x) {
    int width = FFMIN(jump, sps->width - x);
    return sps->bit_depth>8? (width>48? 128:64)
                           : (width>64? 128:64);
}

static void read_rect(RPI_T *rpi, char *buf, int addr64, int height, int bytes_per_line) {
    rpi->axi_read_alloc(rpi->id, bytes_per_line*height);
    if (bytes_per_line==128)
        rpi->axi_read_tx(rpi->id, ((uint64_t)addr64)<<6, 128*height);
    else {
        int y;
        for (y=0; y<height; y++, addr64+=2) rpi->axi_read_tx(rpi->id, ((uint64_t)addr64)<<6, 64);
    }
    rpi->axi_read_rx(rpi->id, bytes_per_line*height, buf);
}

#ifdef AXI_BUFFERS
//////////////////////////////////////////////////////////////////////////////
// Copy YUV output data to FFMPEG frame buffer

static void copy_luma(char *buf, int bpl, int height, int x, uint8_t *data, int linesize) {
    int y;
    for (y=0; y<height; y++)
        memcpy(data+y*linesize+x, buf+bpl*y, FFMIN(bpl, linesize-x));
}

static void copy_chroma(char *buf, int bpl, int height, int x, uint8_t *u, uint8_t *v, int linesize) {
    int i, j, y;
    for (y=0; y<height; y++, buf+=bpl) for (j=x,i=0; i<bpl && j<linesize; j++) {
        u[y*linesize+j] = buf[i++];
        v[y*linesize+j] = buf[i++];
    }
}

static void copy_luma10(char *buf, int bpl, int height, int x, uint8_t *data, int linesize) {
    int i, j, y;
    for (y=0; y<height; y++) {
        uint32_t *src = (uint32_t*) (buf+y*bpl);
        uint16_t *dst = (uint16_t*) (data+y*linesize);
        for (j=x,i=0; i<bpl/4; i++) {
            dst[j] = (src[i]>> 0)&0x3ff; if(++j==linesize/2) break;
            dst[j] = (src[i]>>10)&0x3ff; if(++j==linesize/2) break;
            dst[j] = (src[i]>>20)&0x3ff; if(++j==linesize/2) break;
        }
    }
}

static void copy_chroma10(char *buf, int bpl, int height, int x, uint8_t *u8, uint8_t *v8, int linesize) {
    int i, j, y;
    for (y=0; y<height; y++) {
        uint32_t *src = (uint32_t *) (buf+y*bpl);
        uint16_t *u16 = (uint16_t *) (u8+y*linesize);
        uint16_t *v16 = (uint16_t *) (v8+y*linesize);
        for (j=x,i=0; i<bpl/4; i++) {
            u16[j] = (src[i]>> 0)&0x3ff;
            v16[j] = (src[i]>>10)&0x3ff; if(++j==linesize/2) break;
            u16[j] = (src[i]>>20)&0x3ff; i++;
            v16[j] = (src[i]>> 0)&0x3ff; if(++j==linesize/2) break;
            u16[j] = (src[i]>>10)&0x3ff;
            v16[j] = (src[i]>>20)&0x3ff; if(++j==linesize/2) break;
        }
    }
}
#endif

//////////////////////////////////////////////////////////////////////////////
// Phase 1 command and bit FIFOs

static int p1_apb_write(RPI_T *rpi, uint16_t addr, uint32_t data) {
    if (rpi->cmd_len==rpi->cmd_max)
        av_assert0(rpi->cmd_fifo = realloc(rpi->cmd_fifo, (rpi->cmd_max*=2)*sizeof(struct RPI_CMD)));
    rpi->cmd_fifo[rpi->cmd_len].addr = addr;
    rpi->cmd_fifo[rpi->cmd_len].data = data;
    return rpi->cmd_len++;
}

static void p1_axi_write(RPI_T *rpi, uint32_t len, const void *ptr, int cmd_idx) {
    if (rpi->bit_len==rpi->bit_max)
        av_assert0(rpi->bit_fifo = realloc(rpi->bit_fifo, (rpi->bit_max*=2)*sizeof(struct RPI_BIT)));
    rpi->bit_fifo[rpi->bit_len].cmd = cmd_idx;
    rpi->bit_fifo[rpi->bit_len].ptr = ptr;
    rpi->bit_fifo[rpi->bit_len].len = len;
    rpi->bit_len++;
}

//////////////////////////////////////////////////////////////////////////////
// Write probability and scaling factor memories

static void WriteProb(RPI_T *rpi) {
    int i;
    uint8_t *p = (uint8_t *) &rpi->probabilities;
    for (i=0; i<sizeof(struct RPI_PROB); i+=4, p+=4)
        p1_apb_write(rpi, 0x1000+i, p[0] + (p[1]<<8) + (p[2]<<16) + (p[3]<<24));
}

static void WriteScalingFactors(RPI_T *rpi) {
    int i;
    uint8_t *p = (uint8_t *) rpi->scaling_factors;
    for (i=0; i<NUM_SCALING_FACTORS; i+=4, p+=4)
        p1_apb_write(rpi, 0x2000+i, p[0] + (p[1]<<8) + (p[2]<<16) + (p[3]<<24));
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

static void alloc_picture_space(RPI_T *rpi, HEVCContext *s, int thread_idx) {
    const HEVCSPS *sps = s->ps.sps;
    int CtbSizeY = 1<<sps->log2_ctb_size;
    int x64 = AXI_BASE64;

    rpi->PicWidthInCtbsY  = (sps->width + CtbSizeY - 1) / CtbSizeY;  //7-15
    rpi->PicHeightInCtbsY = (sps->height + CtbSizeY - 1) / CtbSizeY;  //7-17
#ifdef AXI_BUFFERS
    rpi->lumabytes64 = ((sps->height+64) * ((sps->width+95)/96) * 2);
    rpi->framebytes64 = ((rpi->lumabytes64 * 3)/2);
    rpi->lumastride64 = ((sps->height+64) * 128) / 64;
    rpi->chromastride64 = (((sps->height+64) * 128 ) / 2) / 64;

    x64 += 17 * rpi->framebytes64;
#endif

    // collocated reads/writes
    if (sps->sps_temporal_mvp_enabled_flag) {
        // 128 bits = 16 bytes per MV, one for every 16*16
        int collocatedStride64 = (rpi->PicWidthInCtbsY * (CtbSizeY/16) * 16 + 63)>>6;
        rpi->mvframebytes64 = rpi->PicHeightInCtbsY * (CtbSizeY/16) * collocatedStride64;
        rpi->mvstorage64 = x64;
        x64 += rpi->mvframebytes64 * 17; // Leave space for 17 reference pictures
        rpi->colstride64 = collocatedStride64;
        rpi->mvstride64 = collocatedStride64;
    }

    rpi->pubase64[0] = x64;
}

static int alloc_stream_space(RPI_T *rpi, HEVCContext *s, int thread_idx) {
    int stride64, x64 = rpi->pubase64[0];

    stride64 = 1 + (rpi->max_pu_msgs*2*rpi->PicWidthInCtbsY)/64;
    rpi->pubase64[thread_idx] = x64 + rpi->PicHeightInCtbsY*stride64 * thread_idx;
    rpi->pustep64 = stride64;
    x64 += rpi->PicHeightInCtbsY*stride64 * s->avctx->thread_count;

    stride64 = rpi->max_coeff64;
    rpi->coeffbase64[thread_idx] = x64 + rpi->PicHeightInCtbsY*stride64 * thread_idx;
    rpi->coeffstep64 = stride64;
    x64 += rpi->PicHeightInCtbsY*stride64 * s->avctx->thread_count;

    return x64;
}

//////////////////////////////////////////////////////////////////////////////
// Start or restart phase 1

static void phase1_begin(RPI_T *rpi, HEVCContext *s, int thread_idx) {
    rpi->apb_write_addr(rpi->id, RPI_PUWBASE,      rpi->pubase64[thread_idx]);
    rpi->apb_write(rpi->id, RPI_PUWSTRIDE,    rpi->pustep64);
    rpi->apb_write_addr(rpi->id, RPI_COEFFWBASE,   rpi->coeffbase64[thread_idx]);
    rpi->apb_write(rpi->id, RPI_COEFFWSTRIDE, rpi->coeffstep64);
}

///////////////////////////////////////////////////////////////////////////////
// Wait until phase 2 idle

static void wait_idle(RPI_T *rpi, int last) {
    for (;;) {
        int order;
        pthread_mutex_lock  (&rpi->mutex_phase2);
        order = rpi->phase2_order;
        pthread_mutex_unlock(&rpi->mutex_phase2);
        if (order==last) return;
    }
}

//////////////////////////////////////////////////////////////////////////////
// Handle PU and COEFF stream overflow

static int check_status(RPI_T *rpi) {
    int status, c, p;

    // this is the definition of successful completion of phase 1
    // it assures that status register is zero and all blocks in each tile have completed
    if (rpi->apb_read(rpi->id, RPI_CFSTATUS) == rpi->apb_read(rpi->id, RPI_CFNUM))
	return 0;

    status = rpi->apb_read(rpi->id, RPI_STATUS);

    p = (status>>4)&1;
    c = (status>>3)&1;
    if (p|c) { // overflow?
        wait_idle(rpi, rpi->phase1_order-1); // drain phase2 before changing memory layout
        if (p) rpi->max_pu_msgs += rpi->max_pu_msgs/2;
        if (c) rpi->max_coeff64 += rpi->max_coeff64/2;
        return 1;
    }
    return 2;
}

//////////////////////////////////////////////////////////////////////////////
// Write STATUS register with expected end CTU address of previous slice

static void end_previous_slice(RPI_T *rpi, HEVCContext *s, int ctb_addr_ts) {
    const HEVCPPS *pps = s->ps.pps;
    int last_x = pps->ctb_addr_ts_to_rs[ctb_addr_ts-1] % rpi->PicWidthInCtbsY;
    int last_y = pps->ctb_addr_ts_to_rs[ctb_addr_ts-1] / rpi->PicWidthInCtbsY;
    p1_apb_write(rpi, RPI_STATUS, 1 + (last_x<<5) + (last_y<<18));
}

static void wpp_pause(RPI_T *rpi, int ctb_row) {
    p1_apb_write(rpi, RPI_STATUS, (ctb_row<<18) + 0x25);
    p1_apb_write(rpi, RPI_TRANSFER, PROB_BACKUP);
    p1_apb_write(rpi, RPI_MODE, ctb_row==rpi->PicHeightInCtbsY-1?0x70000:0x30000);
    p1_apb_write(rpi, RPI_CONTROL, (ctb_row<<16) + 2);
}

static void wpp_end_previous_slice(RPI_T *rpi, HEVCContext *s, int ctb_addr_ts) {
    const HEVCPPS *pps = s->ps.pps;
    int new_x = s->sh.slice_ctb_addr_rs % rpi->PicWidthInCtbsY;
    int new_y = s->sh.slice_ctb_addr_rs / rpi->PicWidthInCtbsY;
    int last_x = pps->ctb_addr_ts_to_rs[ctb_addr_ts-1] % rpi->PicWidthInCtbsY;
    int last_y = pps->ctb_addr_ts_to_rs[ctb_addr_ts-1] / rpi->PicWidthInCtbsY;
    if (rpi->wpp_entry_x<2 && (rpi->wpp_entry_y<new_y || new_x>2) && rpi->PicWidthInCtbsY>2) wpp_pause(rpi, last_y);
    p1_apb_write(rpi, RPI_STATUS, 1 + (last_x<<5) + (last_y<<18));
    if (new_x==2 || rpi->PicWidthInCtbsY==2 && rpi->wpp_entry_y<new_y) p1_apb_write(rpi, RPI_TRANSFER, PROB_BACKUP);
}

//////////////////////////////////////////////////////////////////////////////

static void new_slice_segment(RPI_T *rpi, HEVCContext *s) {
    const HEVCSPS *sps = s->ps.sps;
    const HEVCPPS *pps = s->ps.pps;

    p1_apb_write(rpi, RPI_SPS0,
        (sps->log2_min_cb_size                    <<  0) +
        (sps->log2_ctb_size                       <<  4) +
        (sps->log2_min_tb_size                    <<  8) +
        (sps->log2_max_trafo_size                 << 12) +
        (sps->bit_depth                           << 16) +
        (sps->bit_depth                           << 20) +
        (sps->max_transform_hierarchy_depth_intra << 24) +
        (sps->max_transform_hierarchy_depth_inter << 28));

    p1_apb_write(rpi, RPI_SPS1,
        (sps->pcm.bit_depth                                        <<  0) +
        (sps->pcm.bit_depth_chroma                                 <<  4) +
        (sps->pcm.log2_min_pcm_cb_size                             <<  8) +
        (sps->pcm.log2_max_pcm_cb_size                             << 12) +
        (sps->separate_colour_plane_flag? 0:sps->chroma_format_idc << 16) +
        (sps->amp_enabled_flag                                     << 18) +
        (sps->pcm_enabled_flag                                     << 19) +
        (sps->scaling_list_enable_flag                             << 20) +
        (sps->sps_strong_intra_smoothing_enable_flag               << 21));

    p1_apb_write(rpi, RPI_PPS,
        (sps->log2_ctb_size - pps->diff_cu_qp_delta_depth   <<  0) +
        (pps->cu_qp_delta_enabled_flag                      <<  4) +
        (pps->transquant_bypass_enable_flag                 <<  5) +
        (pps->transform_skip_enabled_flag                   <<  6) +
        (pps->sign_data_hiding_flag                         <<  7) +
      (((pps->cb_qp_offset + s->sh.slice_cb_qp_offset)&255) <<  8) +
      (((pps->cr_qp_offset + s->sh.slice_cr_qp_offset)&255) << 16) +
        (pps->constrained_intra_pred_flag                   << 24));

    if (s->ps.sps->scaling_list_enable_flag) WriteScalingFactors(rpi);

    if (!s->sh.dependent_slice_segment_flag) {
        int ctb_col = s->sh.slice_ctb_addr_rs % rpi->PicWidthInCtbsY;
        int ctb_row = s->sh.slice_ctb_addr_rs / rpi->PicWidthInCtbsY;
        rpi->reg_slicestart = (ctb_col<<0) + (ctb_row<<16);
    }

    p1_apb_write(rpi, RPI_SLICESTART, rpi->reg_slicestart);
}

//////////////////////////////////////////////////////////////////////////////

static void write_slice(RPI_T *rpi, HEVCContext *s, uint8_t slice_w, uint8_t slice_h) {
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

    if (s->sh.slice_type==HEVC_SLICE_B) u32 |= s->sh.mvd_l1_zero_flag<<16;
    p1_apb_write(rpi, RPI_SLICE, u32);
}

//////////////////////////////////////////////////////////////////////////////
// Wavefront mode

static void wpp_entry_point(RPI_T *rpi, HEVCContext *s, int do_bte, int resetQPY, int ctb_addr_ts) {
    const HEVCSPS *sps = s->ps.sps;
    const HEVCPPS *pps = s->ps.pps;

    int ctb_size = 1<<sps->log2_ctb_size;
    int ctb_addr_rs = pps->ctb_addr_ts_to_rs[ctb_addr_ts];

    int ctb_col = rpi->wpp_entry_x = ctb_addr_rs % rpi->PicWidthInCtbsY;
    int ctb_row = rpi->wpp_entry_y = ctb_addr_rs / rpi->PicWidthInCtbsY;

    int endx = rpi->PicWidthInCtbsY-1;
    int endy = ctb_row;

    uint8_t slice_w = ctb_to_slice_w_h(ctb_col, ctb_size, sps->width,  pps->col_bd, pps->num_tile_columns);
    uint8_t slice_h = ctb_to_slice_w_h(ctb_row, ctb_size, sps->height, pps->row_bd, pps->num_tile_rows);

    p1_apb_write(rpi, RPI_TILESTART, 0);
    p1_apb_write(rpi, RPI_TILEEND, endx + (endy<<16));

    if (do_bte) p1_apb_write(rpi, RPI_BEGINTILEEND, endx + (endy<<16));

    write_slice(rpi, s, slice_w, ctb_row==rpi->PicHeightInCtbsY-1? slice_h : ctb_size);

    if (resetQPY) p1_apb_write(rpi, RPI_QP, sps->qp_bd_offset + s->sh.slice_qp);

    p1_apb_write(rpi, RPI_MODE, ctb_row==rpi->PicHeightInCtbsY-1? 0x60001 : 0x20001);
    p1_apb_write(rpi, RPI_CONTROL, (ctb_col<<0) + (ctb_row<<16));
}

//////////////////////////////////////////////////////////////////////////////
// Tiles mode

static void new_entry_point(RPI_T *rpi, HEVCContext *s, int do_bte, int resetQPY, int ctb_addr_ts) {
    const HEVCSPS *sps = s->ps.sps;
    const HEVCPPS *pps = s->ps.pps;

    int ctb_col = pps->ctb_addr_ts_to_rs[ctb_addr_ts] % rpi->PicWidthInCtbsY;
    int ctb_row = pps->ctb_addr_ts_to_rs[ctb_addr_ts] / rpi->PicWidthInCtbsY;

    int tile_x = ctb_to_tile (ctb_col, pps->col_bd, pps->num_tile_columns);
    int tile_y = ctb_to_tile (ctb_row, pps->row_bd, pps->num_tile_rows);

    int endx = pps->col_bd[tile_x+1] - 1;
    int endy = pps->row_bd[tile_y+1] - 1;

    uint8_t slice_w = ctb_to_slice_w_h(ctb_col, 1<<sps->log2_ctb_size, sps->width,  pps->col_bd, pps->num_tile_columns);
    uint8_t slice_h = ctb_to_slice_w_h(ctb_row, 1<<sps->log2_ctb_size, sps->height, pps->row_bd, pps->num_tile_rows);

    p1_apb_write(rpi, RPI_TILESTART, pps->col_bd[tile_x] + (pps->row_bd[tile_y]<<16));
    p1_apb_write(rpi, RPI_TILEEND, endx + (endy<<16));

    if (do_bte) p1_apb_write(rpi, RPI_BEGINTILEEND, endx + (endy<<16));

    write_slice(rpi, s, slice_w, slice_h);

    if (resetQPY) p1_apb_write(rpi, RPI_QP, sps->qp_bd_offset + s->sh.slice_qp);

    p1_apb_write(rpi, RPI_MODE, (0xFFFF                            <<  0)
                              + (0x0                               << 16)
                              + ((tile_x==pps->num_tile_columns-1) << 17)
                              + ((tile_y==pps->num_tile_rows-1)    << 18));

    p1_apb_write(rpi, RPI_CONTROL, (ctb_col<<0) + (ctb_row<<16));
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

static int get_thread_idx(RPI_T *rpi, AVCodecContext *avctx) {
    int idx;
    for (idx=0; idx<MAX_THREADS; idx++) if (rpi->thread_avctx[idx]==avctx) break;
    av_assert0(idx<MAX_THREADS);
    return idx;
}

//////////////////////////////////////////////////////////////////////////////
// Start frame

static int rpi_hevc_start_frame(
    AVCodecContext *avctx,
    const uint8_t *buffer,
    uint32_t size) {

    RPI_T *rpi = avctx->internal->hwaccel_priv_data;
    HEVCContext *s = avctx->priv_data;

    int thread_idx = get_thread_idx(rpi, 0); // Find first free slot

    rpi->thread_avctx[thread_idx] = avctx;
    rpi->thread_order[thread_idx] = rpi->decode_order++;

    ff_thread_finish_setup(avctx); // Allow next thread to enter rpi_hevc_start_frame
    hwaccel_mutex(avctx, pthread_mutex_unlock);

    // Enforcing phase 1 order precludes busy waiting for phase 2
    for (;;) {
        pthread_mutex_lock  (&rpi->mutex_phase1);
        if (rpi->thread_order[thread_idx]==rpi->phase1_order) break;
        pthread_mutex_unlock(&rpi->mutex_phase1);
    }
    rpi->phase1_order++;

    alloc_picture_space(rpi, s, thread_idx);
    rpi->bit_len = rpi->cmd_len = 0;
    return 0;
}

//////////////////////////////////////////////////////////////////////////////
// Slice messages

static void msg_slice(RPI_T *rpi, uint16_t msg) {
    rpi->slice_msgs[rpi->num_slice_msgs++] = msg;
}

static void program_slicecmds(RPI_T *rpi, int sliceid) {
    int i;
    p1_apb_write(rpi, RPI_SLICECMDS, rpi->num_slice_msgs+(sliceid<<8));
    for(i=0; i<rpi->num_slice_msgs; i++) {
        p1_apb_write(rpi, 0x4000+4*i, rpi->slice_msgs[i] & 0xffff);
    }
}

static void pre_slice_decode(RPI_T *rpi, HEVCContext *s) {
    const HEVCSPS *sps = s->ps.sps;
    const HEVCPPS *pps = s->ps.pps;
    SliceHeader *sh = &s->sh;

    int weightedPredFlag, i, rIdx;
    uint16_t cmd_slice;

    rpi->num_slice_msgs=0;
    cmd_slice = 0;
    if (sh->slice_type==HEVC_SLICE_I) cmd_slice = 1;
    if (sh->slice_type==HEVC_SLICE_P) cmd_slice = 2;
    if (sh->slice_type==HEVC_SLICE_B) cmd_slice = 3;

    if (sh->slice_type!=HEVC_SLICE_I) {
        cmd_slice += sh->nb_refs[L0]<<2;
        cmd_slice += sh->nb_refs[L1]<<6;
    }
    if (sh->slice_type==HEVC_SLICE_P
    ||  sh->slice_type==HEVC_SLICE_B) rpi->max_num_merge_cand = sh->max_num_merge_cand;

    cmd_slice += rpi->max_num_merge_cand<<11;

    if (sh->slice_temporal_mvp_enabled_flag) {
        if      (sh->slice_type==HEVC_SLICE_B) rpi->collocated_from_l0_flag = sh->collocated_list==L0;
        else if (sh->slice_type==HEVC_SLICE_P) rpi->collocated_from_l0_flag = 1;
    }
    cmd_slice += rpi->collocated_from_l0_flag<<14;

    if (sh->slice_type==HEVC_SLICE_P || sh->slice_type==HEVC_SLICE_B) {

        int NoBackwardPredFlag = 1; // Flag to say all reference pictures are from the past
        for(i=L0; i<=L1; i++) {
            for(rIdx=0; rIdx <sh->nb_refs[i]; rIdx++) {
                HEVCFrame *f = s->ref->refPicList[i].ref[rIdx];
                HEVCFrame *c = s->ref; // CurrentPicture
                if (c->poc < f->poc) NoBackwardPredFlag = 0;
            }
        }

        rpi->collocated_ref_idx = sh->collocated_ref_idx;
        if (s->ref->refPicList && s->ref->collocated_ref)
            for (i=0; i<HEVC_MAX_REFS; i++) {
                if (i<sh->nb_refs[L1]) rpi->RefPicList[1][i] = s->ref->refPicList[1].ref[i] - s->DPB;
                if (i<sh->nb_refs[L0]) rpi->RefPicList[0][i] = s->ref->refPicList[0].ref[i] - s->DPB;
            }

        cmd_slice += NoBackwardPredFlag<<10;
        msg_slice(rpi, cmd_slice);

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
                msg_slice(rpi, adjusted_pic+(lt<<4)+(weightedPredFlag<<5)+(weightedPredFlag<<6));
                msg_slice(rpi, f->poc);
                if (weightedPredFlag) {
                    msg_slice(rpi,   s->sh.luma_log2_weight_denom+(((i?s->  sh.luma_weight_l1:  s->sh.luma_weight_l0)[rIdx]   &0x1ff)<<3));
                    msg_slice(rpi,                                  (i?s->  sh.luma_offset_l1:  s->sh.luma_offset_l0)[rIdx]   & 0xff);
                    msg_slice(rpi, s->sh.chroma_log2_weight_denom+(((i?s->sh.chroma_weight_l1:s->sh.chroma_weight_l0)[rIdx][0]&0x1ff)<<3));
                    msg_slice(rpi,                                  (i?s->sh.chroma_offset_l1:s->sh.chroma_offset_l0)[rIdx][0]& 0xff);
                    msg_slice(rpi, s->sh.chroma_log2_weight_denom+(((i?s->sh.chroma_weight_l1:s->sh.chroma_weight_l0)[rIdx][1]&0x1ff)<<3));
                    msg_slice(rpi,                                  (i?s->sh.chroma_offset_l1:s->sh.chroma_offset_l0)[rIdx][1]& 0xff);
                }
            }
    }
    else
        msg_slice(rpi, cmd_slice);

    msg_slice(rpi, ((sh->beta_offset/2)&15)
        + (((sh->tc_offset/2)&15)                           <<  4)
        + (sh->disable_deblocking_filter_flag               <<  8)
        + (sh->slice_loop_filter_across_slices_enabled_flag <<  9)
        + (pps->loop_filter_across_tiles_enabled_flag       << 10)); // CMD_DEBLOCK

    msg_slice(rpi, ((sh->slice_cr_qp_offset&31)<<5) + (sh->slice_cb_qp_offset&31)); // CMD_QPOFF

    // collocated reads/writes
    if (sps->sps_temporal_mvp_enabled_flag) {
        int thread_idx = get_thread_idx(rpi, s->avctx);
        int CurrentPicture = s->ref - s->DPB;
        int colPic = rpi->RefPicList[sh->slice_type==HEVC_SLICE_B && rpi->collocated_from_l0_flag==0][rpi->collocated_ref_idx];
        rpi->mvbase64 [thread_idx] = rpi->mvstorage64 + CurrentPicture * rpi->mvframebytes64;
        if (sh->slice_type==HEVC_SLICE_I) {
            // Collocated picture not well defined here.  Use mvbase or previous value
            if (sh->first_slice_in_pic_flag)
                rpi->colbase64[thread_idx] = rpi->mvbase64[thread_idx]; // Ensure we don't read garbage
        }
        else
            rpi->colbase64[thread_idx] = rpi->mvstorage64 + colPic * rpi->mvframebytes64;
    }
}

//////////////////////////////////////////////////////////////////////////////
// End frame

static int rpi_hevc_end_frame(AVCodecContext *avctx) {
    RPI_T *rpi = avctx->internal->hwaccel_priv_data;
    HEVCContext *s = avctx->priv_data;
    const HEVCPPS *pps = s->ps.pps;
    const HEVCSPS *sps = s->ps.sps;
    const int thread_idx = get_thread_idx(rpi, avctx);
    int jump = sps->bit_depth>8?96:128;
    int CurrentPicture = s->ref - s->DPB;
    AVFrame *f = s->ref->frame;
    int last_x = pps->col_bd[pps->num_tile_columns]-1;
    int last_y = pps->row_bd[pps->num_tile_rows]-1;

    int i, a64, x;
    char *buf;
    int status = 1;

    // End of phase 1 command compilation
    if (pps->entropy_coding_sync_enabled_flag) {
        if (rpi->wpp_entry_x<2 && rpi->PicWidthInCtbsY>2) wpp_pause(rpi, last_y);
    }
    p1_apb_write(rpi, RPI_STATUS, 1 + (last_x<<5) + (last_y<<18));

    // Phase 1 ...
    for (;;) {
        // (Re-)allocate PU/COEFF stream space
        a64 = alloc_stream_space(rpi, s, thread_idx);
        // Send bitstream data
        for (i=0; i<rpi->bit_len; i++) {
            if ((unsigned int)a64 + (((unsigned int)rpi->bit_fifo[i].len + 31) >> 6) > (1U << 20)) {
                av_log(avctx, AV_LOG_ERROR, "Out of HEVC intermediate memory - maybe too many threads");
                status = -1;
                break;
            }
            rpi->axi_write(rpi->id, ((uint64_t)a64)<<6, rpi->bit_fifo[i].len, rpi->bit_fifo[i].ptr);
            rpi->cmd_fifo[rpi->bit_fifo[i].cmd].data = a64 + (rpi->axi_get_addr(rpi->id)>>6); // Set BFBASE
            a64 += (rpi->bit_fifo[i].len+63)/64;
        }
        // Send phase 1 commands (cache flush on real hardware)
        rpi->axi_write(rpi->id, ((uint64_t)a64)<<6, rpi->cmd_len * sizeof(struct RPI_CMD), rpi->cmd_fifo);
        rpi->axi_flush(rpi->id, RPI_CACHE_FLUSH_MODE_WB_INVALIDATE);
        phase1_begin(rpi, s, thread_idx);
        // Trigger command FIFO
        rpi->apb_write(rpi->id, RPI_CFNUM, rpi->cmd_len);
        rpi->apb_dump_regs(rpi->id, 0x0, 32);
        rpi->apb_dump_regs(rpi->id, 0x8000, 24);
        rpi->axi_dump(rpi->id, ((uint64_t)a64)<<6, rpi->cmd_len * sizeof(struct RPI_CMD));
        rpi->apb_write_addr(rpi->id, RPI_CFBASE, a64);
        rpi->wait_interrupt(rpi->id, 1);
        status = check_status(rpi);
        if (status != 1) break; // No PU/COEFF overflow?
    }
    pthread_mutex_unlock(&rpi->mutex_phase1);

    // Phase 2 ...
    for (;;) {
        pthread_mutex_lock  (&rpi->mutex_phase2);
        if (rpi->thread_order[thread_idx]==rpi->phase2_order) break;
        pthread_mutex_unlock(&rpi->mutex_phase2);
    }
    rpi->phase2_order++;

    rpi->apb_write_addr(rpi->id, RPI_PURBASE, rpi->pubase64[thread_idx]);
    rpi->apb_write(rpi->id, RPI_PURSTRIDE, rpi->pustep64);
    rpi->apb_write_addr(rpi->id, RPI_COEFFRBASE, rpi->coeffbase64[thread_idx]);
    rpi->apb_write(rpi->id, RPI_COEFFRSTRIDE, rpi->coeffstep64);

#if !defined(AXI_BUFFERS)
#define MANGLE(x) (((x) &~0xc0000000)>>6)
{
    const AVRpiZcRefPtr fr_buf = f ? av_rpi_zc_ref(avctx, f, f->format, 0) : NULL;
    uint32_t handle = fr_buf ? av_rpi_zc_vc_handle(fr_buf):0;
//    printf("%s cur:%d fr:%p handle:%d YUV:%x:%x ystride:%d ustride:%d ah:%d\n", __FUNCTION__, CurrentPicture, f, handle, get_vc_address_y(f), get_vc_address_u(f), f->linesize[0], f->linesize[1],  f->linesize[3]);
    rpi->apb_write(rpi->id, RPI_OUTYBASE, MANGLE(get_vc_address_y(f)));
    rpi->apb_write(rpi->id, RPI_OUTCBASE, MANGLE(get_vc_address_u(f)));
    rpi->apb_write(rpi->id, RPI_OUTYSTRIDE, f->linesize[3] * 128 / 64);
    rpi->apb_write(rpi->id, RPI_OUTCSTRIDE, f->linesize[3] * 128 / 64);
    av_rpi_zc_unref(fr_buf);
}
#else
    // Output frame and reference picture locations
    rpi->apb_write_addr(rpi->id, RPI_OUTYBASE, CurrentPicture * rpi->framebytes64);
    rpi->apb_write_addr(rpi->id, RPI_OUTCBASE, CurrentPicture * rpi->framebytes64 + rpi->lumabytes64);
    rpi->apb_write(rpi->id, RPI_OUTYSTRIDE, rpi->lumastride64);
    rpi->apb_write(rpi->id, RPI_OUTCSTRIDE, rpi->chromastride64);
#endif

#if !defined(AXI_BUFFERS)
{
    SliceHeader *sh = &s->sh;
    int rIdx;
    for(i=0; i<16; i++) {
        rpi->apb_write(rpi->id, 0x9000+16*i, 0);
        rpi->apb_write(rpi->id, 0x9004+16*i, 0);
        rpi->apb_write(rpi->id, 0x9008+16*i, 0);
        rpi->apb_write(rpi->id, 0x900C+16*i, 0);
    }

    for(i=L0; i<=L1; i++)
    for(rIdx=0; rIdx <sh->nb_refs[i]; rIdx++) {
        HEVCFrame *f1 = s->ref->refPicList[i].ref[rIdx];
        HEVCFrame *c = s->ref; // CurrentPicture
        int pic = f1 - s->DPB;
        // Make sure pictures are in range 0 to 15
        int adjusted_pic = f1<c? pic : pic-1;
        struct HEVCFrame *hevc = &s->DPB[pic];
        AVFrame *fr = hevc ? hevc->frame : NULL;
        const AVRpiZcRefPtr fr_buf = fr ? av_rpi_zc_ref(avctx, fr, fr->format, 0) : NULL;
        uint32_t handle = fr_buf ? av_rpi_zc_vc_handle(fr_buf):0;
//        printf("%s pic:%d (%d,%d,%d) fr:%p handle:%d YUV:%x:%x\n", __FUNCTION__, adjusted_pic, i, rIdx, pic, fr, handle, get_vc_address_y(fr), get_vc_address_u(fr));
        rpi->apb_write(rpi->id, 0x9000+16*adjusted_pic, MANGLE(get_vc_address_y(fr)));
        rpi->apb_write(rpi->id, 0x9008+16*adjusted_pic, MANGLE(get_vc_address_u(fr)));
        rpi->apb_write(rpi->id, RPI_OUTYSTRIDE, fr->linesize[3] * 128 / 64);
        rpi->apb_write(rpi->id, RPI_OUTCSTRIDE, fr->linesize[3] * 128 / 64);
        av_rpi_zc_unref(fr_buf);
    }
}
#else
    for(i=0; i<16; i++) {
        int pic = i < CurrentPicture ? i : i+1;
        rpi->apb_write_addr(rpi->id, 0x9000+16*i, pic * rpi->framebytes64);
        rpi->apb_write(rpi->id, 0x9004+16*i, rpi->lumastride64);
        rpi->apb_write_addr(rpi->id, 0x9008+16*i, pic * rpi->framebytes64 + rpi->lumabytes64);
        rpi->apb_write(rpi->id, 0x900C+16*i, rpi->chromastride64);
    }
#endif

    rpi->apb_write(rpi->id, RPI_CONFIG2,
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

    rpi->apb_write(rpi->id, RPI_FRAMESIZE, (sps->height<<16) + sps->width);
    rpi->apb_write(rpi->id, RPI_CURRPOC, s->poc);

    // collocated reads/writes
    if (sps->sps_temporal_mvp_enabled_flag) {
        rpi->apb_write(rpi->id, RPI_COLSTRIDE, rpi->colstride64);
        rpi->apb_write(rpi->id, RPI_MVSTRIDE,  rpi->mvstride64);
        rpi->apb_write_addr(rpi->id, RPI_MVBASE,    rpi->mvbase64 [thread_idx]);
        rpi->apb_write_addr(rpi->id, RPI_COLBASE,   rpi->colbase64[thread_idx]);
    }

    rpi->apb_dump_regs(rpi->id, 0x0, 32);
    rpi->apb_dump_regs(rpi->id, 0x8000, 24);

    // only do phase if phase 1 completed without errors
    if (status == 0) {
        rpi->apb_write(rpi->id, RPI_NUMROWS, rpi->PicHeightInCtbsY);
        rpi->apb_read_drop(rpi->id, RPI_NUMROWS); // Read back to confirm write has reached block
        rpi->wait_interrupt(rpi->id, 2);
    }

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
//printf("%s: %dx%d %d\n", __FUNCTION__, f->width, f->height, f->linesize[0]);
#if defined(AXI_BUFFERS)
    // Copy YUV output frame
    av_assert0(buf = malloc(128*sps->height));
    a64 = AXI_BASE64 + CurrentPicture * rpi->framebytes64;
    for(x=0; x<sps->width; x+=jump) {
        int bpl = bytes_per_line(sps, jump, x);
        read_rect(rpi, buf, a64, sps->height, bpl);
        (sps->bit_depth>8?copy_luma10:copy_luma)(buf, bpl, sps->height, x, f->data[0], f->linesize[0]);
        a64 += rpi->lumastride64;
    }
    a64 = AXI_BASE64 + CurrentPicture * rpi->framebytes64 + rpi->lumabytes64;
    for(x=0; x<sps->width; x+=jump) {
        int bpl = bytes_per_line(sps, jump, x);
        read_rect(rpi, buf, a64, sps->height/2, bpl);
        (sps->bit_depth>8?copy_chroma10:copy_chroma)(buf, bpl, sps->height/2, x/2, f->data[1], f->data[2], f->linesize[1]);
        a64 += rpi->chromastride64;
    }
    free(buf);
#endif
    rpi->thread_avctx[thread_idx] = 0;
    pthread_mutex_unlock(&rpi->mutex_phase2);

    hwaccel_mutex(avctx, pthread_mutex_lock);

    return 0;
}

//////////////////////////////////////////////////////////////////////////////

static void WriteBitstream(RPI_T *rpi, HEVCContext *s) {
    const int rpi_use_emu = 0; // FFmpeg removes emulation prevention bytes
    const int offset = 0; // Always 64-byte aligned in sim, need not be on real hardware
    GetBitContext *gb = &s->HEVClc->gb;
    int len = 1 + gb->size_in_bits/8 - gb->index/8;
    const void *ptr = &gb->buffer[gb->index/8];

    p1_axi_write(rpi, len, ptr, p1_apb_write(rpi, RPI_BFBASE, 0)); // BFBASE set later
    p1_apb_write(rpi, RPI_BFNUM, len);
    p1_apb_write(rpi, RPI_BFCONTROL, offset + (1<<7)); // Stop
    p1_apb_write(rpi, RPI_BFCONTROL, offset + (rpi_use_emu<<6));
}

//////////////////////////////////////////////////////////////////////////////
// Wavefront mode

static void wpp_decode_slice(RPI_T *rpi, HEVCContext *s, int ctb_addr_ts) {
    const HEVCPPS *pps = s->ps.pps;

    int i, resetQPY=1;
    int indep = !s->sh.dependent_slice_segment_flag;
    int ctb_col = s->sh.slice_ctb_addr_rs % rpi->PicWidthInCtbsY;

    if (ctb_addr_ts) wpp_end_previous_slice(rpi, s, ctb_addr_ts);
    pre_slice_decode(rpi, s);
    WriteBitstream(rpi, s);
    if (ctb_addr_ts==0 || indep || rpi->PicWidthInCtbsY==1) WriteProb(rpi);
    else if (ctb_col==0) p1_apb_write(rpi, RPI_TRANSFER, PROB_RELOAD);
    else resetQPY=0;
    program_slicecmds(rpi, s->slice_idx);
    new_slice_segment(rpi, s);
    wpp_entry_point(rpi, s, indep, resetQPY, ctb_addr_ts);
    for (i=0; i<s->sh.num_entry_point_offsets; i++) {
        int ctb_addr_rs = pps->ctb_addr_ts_to_rs[ctb_addr_ts];
        int ctb_row = ctb_addr_rs / rpi->PicWidthInCtbsY;
        int last_x = rpi->PicWidthInCtbsY-1;
        if (rpi->PicWidthInCtbsY>2) wpp_pause(rpi, ctb_row);
        p1_apb_write(rpi, RPI_STATUS, (ctb_row<<18) + (last_x<<5) + 2);
        if (rpi->PicWidthInCtbsY==2) p1_apb_write(rpi, RPI_TRANSFER, PROB_BACKUP);
        if (rpi->PicWidthInCtbsY==1) WriteProb(rpi);
        else p1_apb_write(rpi, RPI_TRANSFER, PROB_RELOAD);
        ctb_addr_ts += pps->column_width[0];
        wpp_entry_point(rpi, s, 0, 1, ctb_addr_ts);
    }
}

//////////////////////////////////////////////////////////////////////////////
// Tiles mode

static void decode_slice(RPI_T *rpi, HEVCContext *s, int ctb_addr_ts) {
    const HEVCPPS *pps = s->ps.pps;
    int i, resetQPY;

    if (ctb_addr_ts) end_previous_slice(rpi, s, ctb_addr_ts);
    pre_slice_decode(rpi, s);
    WriteBitstream(rpi, s);
    resetQPY = ctb_addr_ts==0
            || pps->tile_id[ctb_addr_ts]!=pps->tile_id[ctb_addr_ts-1]
            || !s->sh.dependent_slice_segment_flag;
    if (resetQPY) WriteProb(rpi);
    program_slicecmds(rpi, s->slice_idx);
    new_slice_segment(rpi, s);
    new_entry_point(rpi, s, !s->sh.dependent_slice_segment_flag, resetQPY, ctb_addr_ts);
    for (i=0; i<s->sh.num_entry_point_offsets; i++) {
        int ctb_addr_rs = pps->ctb_addr_ts_to_rs[ctb_addr_ts];
        int ctb_col = ctb_addr_rs % rpi->PicWidthInCtbsY;
        int ctb_row = ctb_addr_rs / rpi->PicWidthInCtbsY;
        int tile_x = ctb_to_tile (ctb_col, pps->col_bd, pps->num_tile_columns);
        int tile_y = ctb_to_tile (ctb_row, pps->row_bd, pps->num_tile_rows);
        int last_x = pps->col_bd[tile_x+1]-1;
        int last_y = pps->row_bd[tile_y+1]-1;
        p1_apb_write(rpi, RPI_STATUS, 2 + (last_x<<5) + (last_y<<18));
        WriteProb(rpi);
        ctb_addr_ts += pps->column_width[tile_x] * pps->row_height[tile_y];
        new_entry_point(rpi, s, 0, 1, ctb_addr_ts);
    }
}

//////////////////////////////////////////////////////////////////////////////

static int rpi_hevc_decode_slice(
    AVCodecContext *avctx,
    const uint8_t *buffer,
    uint32_t size) {

    RPI_T *rpi = avctx->internal->hwaccel_priv_data;
    HEVCContext *s = avctx->priv_data;
    const HEVCPPS *pps = s->ps.pps;
    int ctb_addr_ts = pps->ctb_addr_rs_to_ts[s->sh.slice_ctb_addr_rs];
    ff_hevc_cabac_init(s, ctb_addr_ts);
    if (s->ps.sps->scaling_list_enable_flag) populate_scaling_factors(rpi, s);
    populate_prob_tables(rpi, s);
    pps->entropy_coding_sync_enabled_flag? wpp_decode_slice(rpi, s, ctb_addr_ts)
                                             : decode_slice(rpi, s, ctb_addr_ts);
    return 0;
}

//////////////////////////////////////////////////////////////////////////////
// Bind to socket client

static int open_socket_client(RPI_T *rpi, const char *so) {
     *(void **) &rpi->ctrl_ffmpeg_init = rpi_ctrl_ffmpeg_init;
     *(void **) &rpi->apb_write        = rpi_apb_write;
     *(void **) &rpi->apb_write_addr   = rpi_apb_write_addr;
     *(void **) &rpi->apb_read         = rpi_apb_read;
     *(void **) &rpi->apb_read_drop    = rpi_apb_read_drop;
     *(void **) &rpi->axi_write        = rpi_axi_write;
     *(void **) &rpi->axi_read_alloc   = rpi_axi_read_alloc;
     *(void **) &rpi->axi_read_tx      = rpi_axi_read_tx;
     *(void **) &rpi->axi_read_rx      = rpi_axi_read_rx;
     *(void **) &rpi->axi_get_addr     = rpi_axi_get_addr;
     *(void **) &rpi->apb_dump_regs    = rpi_apb_dump_regs;
     *(void **) &rpi->axi_dump         = rpi_axi_dump;
     *(void **) &rpi->axi_flush        = rpi_axi_flush;
     *(void **) &rpi->wait_interrupt   = rpi_wait_interrupt;
     *(void **) &rpi->ctrl_ffmpeg_free = rpi_ctrl_ffmpeg_free;
    return 1;
}

//////////////////////////////////////////////////////////////////////////////

static int rpi_hevc_alloc_frame(AVCodecContext *avctx, AVFrame *f) {
    HEVCContext *s = avctx->priv_data;
    const HEVCSPS *sps = s->ps.sps;
    const int ALIGN = 16;

    f->width  = sps->width;
    f->height = sps->height;
    f->format = sps->pix_fmt;
    f->buf[0] = av_buffer_alloc(1);
    f->buf[1] = av_buffer_alloc(1);
    f->buf[2] = av_buffer_alloc(1);
    return av_image_alloc(f->data, f->linesize, f->width, f->height, f->format, ALIGN);
}

//////////////////////////////////////////////////////////////////////////////

static int rpi_hevc_init(AVCodecContext *avctx) {
    RPI_T *rpi = avctx->internal->hwaccel_priv_data;
    const char *err, *so;

    so = "./rpi_ffmpeg.so";

    if (avctx->width>4096 || avctx->height>4096) {
        av_log(NULL, AV_LOG_FATAL, "Picture size %dx%d exceeds 4096x4096 maximum for HWAccel\n", avctx->width, avctx->height);
        return AVERROR(ENOTSUP);
    }
    if (!open_socket_client(rpi, so)) {
        av_log(NULL, AV_LOG_FATAL, "%s\n", dlerror());
        return AVERROR_EXTERNAL;
    }
    err = rpi->ctrl_ffmpeg_init(NULL, &rpi->id);
    if (err) {
        av_log(NULL, AV_LOG_FATAL, "Could not connect to RPI server: %s\n", err);
        return AVERROR_EXTERNAL;
    }

    av_rpi_zc_init_local(avctx);

    pthread_mutex_init(&rpi->mutex_phase1, NULL);
    pthread_mutex_init(&rpi->mutex_phase2, NULL);

    // Initial PU/COEFF stream buffer sizes chosen so jellyfish40.265 requires 1 overflow/restart
    rpi->max_pu_msgs = 2+340; // 7.2 says at most 1611 messages per CTU
    rpi->max_coeff64 = 2+1404;

    av_assert0(rpi->cmd_fifo = malloc((rpi->cmd_max=1024)*sizeof(struct RPI_CMD)));
    av_assert0(rpi->bit_fifo = malloc((rpi->bit_max=1024)*sizeof(struct RPI_BIT)));
    return 0;
}

//////////////////////////////////////////////////////////////////////////////

static int rpi_hevc_free(AVCodecContext *avctx) {
    RPI_T *rpi = avctx->internal->hwaccel_priv_data;
    if (rpi->decode_order) wait_idle(rpi, rpi->decode_order);
    if (rpi->cmd_fifo) free(rpi->cmd_fifo);
    if (rpi->bit_fifo) free(rpi->bit_fifo);
    pthread_mutex_destroy(&rpi->mutex_phase1);
    pthread_mutex_destroy(&rpi->mutex_phase2);
    if (rpi->id && rpi->ctrl_ffmpeg_free) rpi->ctrl_ffmpeg_free(rpi->id);

    av_rpi_zc_uninit_local(avctx);
    return 0;
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

