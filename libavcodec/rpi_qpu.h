#ifndef RPI_QPU_H
#define RPI_QPU_H

// Define RPI_FAST_CACHEFLUSH to use the VCSM cache flush code
// *** N.B. Code has rotted & crashes if this is unset (before this set of changes)
#define RPI_FAST_CACHEFLUSH

#define RPI_ONE_BUF 1

typedef struct gpu_mem_ptr_s {
  unsigned char *arm; // Pointer to memory mapped on ARM side
  int vc_handle;   // Videocore handle of relocatable memory
  int vcsm_handle; // Handle for use by VCSM
  int vc;       // Address for use in GPU code
  int numbytes; // Size of memory block
} GPU_MEM_PTR_T;

// General GPU functions
extern int gpu_malloc_cached(int numbytes, GPU_MEM_PTR_T *p);
extern int gpu_malloc_uncached(int numbytes, GPU_MEM_PTR_T *p);
extern void gpu_free(GPU_MEM_PTR_T *p);
extern void gpu_cache_flush(const GPU_MEM_PTR_T * const p);
extern void gpu_cache_flush3(GPU_MEM_PTR_T *p0,GPU_MEM_PTR_T *p1,GPU_MEM_PTR_T *p2);

#include "libavutil/frame.h"
#if !RPI_ONE_BUF
static inline uint32_t get_vc_address_y(const AVFrame * const frame) {
    GPU_MEM_PTR_T *p = av_buffer_pool_opaque(frame->buf[0]);
    return p->vc;
}

static inline uint32_t get_vc_address_u(const AVFrame * const frame) {
    GPU_MEM_PTR_T *p = av_buffer_pool_opaque(frame->buf[1]);
    return p->vc;
}

static inline uint32_t get_vc_address_v(const AVFrame * const frame) {
    GPU_MEM_PTR_T *p = av_buffer_pool_opaque(frame->buf[2]);
    return p->vc;
}

static inline GPU_MEM_PTR_T get_gpu_mem_ptr_y(const AVFrame * const frame) {
    return *(GPU_MEM_PTR_T *)av_buffer_pool_opaque(frame->buf[0]);
}

static inline GPU_MEM_PTR_T get_gpu_mem_ptr_u(const AVFrame * const frame) {
    return *(GPU_MEM_PTR_T *)av_buffer_pool_opaque(frame->buf[1]);
}

static inline GPU_MEM_PTR_T get_gpu_mem_ptr_v(const AVFrame * const frame) {
    return *(GPU_MEM_PTR_T *)av_buffer_pool_opaque(frame->buf[2]);
}

#else

static inline int gpu_is_buf1(const AVFrame * const frame)
{
    return frame->buf[1] == NULL;
}

static inline GPU_MEM_PTR_T * gpu_buf1_gmem(const AVFrame * const frame)
{
    return av_buffer_get_opaque(frame->buf[0]);
}

static inline GPU_MEM_PTR_T * gpu_buf3_gmem(const AVFrame * const frame, const int n)
{
    return av_buffer_pool_opaque(frame->buf[n]);
}


static inline uint32_t get_vc_address_y(const AVFrame * const frame) {
    return gpu_is_buf1(frame) ? gpu_buf1_gmem(frame)->vc : gpu_buf3_gmem(frame, 0)->vc;
}

static inline uint32_t get_vc_address_u(const AVFrame * const frame) {
    return gpu_is_buf1(frame) ?
        gpu_buf1_gmem(frame)->vc + frame->data[1] - frame->data[0] :
        gpu_buf3_gmem(frame, 1)->vc;
}

static inline uint32_t get_vc_address_v(const AVFrame * const frame) {
    return gpu_is_buf1(frame) ?
        gpu_buf1_gmem(frame)->vc + frame->data[2] - frame->data[0] :
        gpu_buf3_gmem(frame, 2)->vc;
}


static inline GPU_MEM_PTR_T get_gpu_mem_ptr_y(const AVFrame * const frame) {
    if (gpu_is_buf1(frame))
    {
        GPU_MEM_PTR_T g = *gpu_buf1_gmem(frame);
        g.numbytes = frame->data[1] - frame->data[0];
        return g;
    }
    else
        return *gpu_buf3_gmem(frame, 0);
}

static inline GPU_MEM_PTR_T get_gpu_mem_ptr_u(const AVFrame * const frame) {
    if (gpu_is_buf1(frame))
    {
        GPU_MEM_PTR_T g = *gpu_buf1_gmem(frame);
        g.arm += frame->data[1] - frame->data[0];
        g.vc += frame->data[1] - frame->data[0];
        g.numbytes = frame->data[2] - frame->data[1];  // chroma size
        return g;
    }
    else
        return *gpu_buf3_gmem(frame, 1);
}

static inline GPU_MEM_PTR_T get_gpu_mem_ptr_v(const AVFrame * const frame) {
    if (gpu_is_buf1(frame))
    {
        GPU_MEM_PTR_T g = *gpu_buf1_gmem(frame);
        g.arm += frame->data[2] - frame->data[0];
        g.vc += frame->data[2] - frame->data[0];
        g.numbytes = frame->data[2] - frame->data[1];  // chroma size
        return g;
    }
    else
        return *gpu_buf3_gmem(frame, 2);
}

#endif


// QPU specific functions
extern void rpi_test_qpu(void);

enum {
  QPU_MC_SETUP,
  QPU_MC_FILTER,
  QPU_MC_EXIT,
  QPU_MC_INTERRUPT_EXIT12,
  QPU_MC_FILTER_B,
  QPU_MC_FILTER_HONLY,
  QPU_MC_SETUP_UV,
  QPU_MC_FILTER_UV,
  QPU_MC_FILTER_UV_B0,
  QPU_MC_FILTER_UV_B,
  QPU_MC_INTERRUPT_EXIT8,
  QPU_MC_END
  };
extern unsigned int qpu_get_fn(int num);

#define QPU_N_UV   8
#define QPU_N_Y    12
#define QPU_N_MAX  16

#define QPU_MAIL_EL_VALS  2
#define QPU_MAIL_EL_SIZE  (QPU_MAIL_EL_VALS * sizeof(uint32_t))
#define QPU_MAIL_VALS_MAX (QPU_N_MAX * QPU_MAIL_EL_VALS)
#define QPU_MAIL_SIZE (QPU_MAIL_VALS_MAX * sizeof(uint32_t))

// VPU specific functions
extern unsigned int vpu_get_fn(void);
extern unsigned int vpu_get_constants(void);
//extern unsigned vpu_execute_code( unsigned code, unsigned r0, unsigned r1, unsigned r2, unsigned r3, unsigned r4, unsigned r5);
extern int vpu_post_code2( unsigned code, unsigned r0, unsigned r1, unsigned r2, unsigned r3, unsigned r4, unsigned r5, GPU_MEM_PTR_T *buf);
int vpu_qpu_post_code2(unsigned vpu_code, unsigned r0, unsigned r1, unsigned r2, unsigned r3, unsigned r4, unsigned r5,
    int qpu0_n, const uint32_t * qpu0_mail,
    int qpu1_n, const uint32_t * qpu1_mail);

extern void vpu_wait( int id);

// Simple test of shader code
extern int rpi_test_shader(void);

extern void rpi_do_block(const unsigned char *in_buffer_vc, int src_pitch, unsigned char *dst_vc, int dst_pitch, unsigned char *dst);
extern void rpi_do_block_arm(const unsigned char *in_buffer, int src_pitch, unsigned char *dst, int dst_pitch);

extern int gpu_get_mailbox(void);

#endif
