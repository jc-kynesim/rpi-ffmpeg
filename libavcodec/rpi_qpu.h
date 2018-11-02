#ifndef RPI_QPU_H
#define RPI_QPU_H

#pragma GCC diagnostic push
// Many many redundant decls in the header files
#pragma GCC diagnostic ignored "-Wredundant-decls"
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#include "interface/vmcs_host/vc_vchi_gpuserv.h"
#pragma GCC diagnostic pop


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
extern void gpu_free(GPU_MEM_PTR_T * const p);

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

static inline GPU_MEM_PTR_T * gpu_buf3_gmem(const AVFrame * const frame, const unsigned int n)
{
    return av_buffer_pool_opaque(frame->buf[n]);
}

static inline uint32_t get_vc_address3(const AVFrame * const frame, const unsigned int n)
{
    const GPU_MEM_PTR_T * const gm = gpu_is_buf1(frame) ? gpu_buf1_gmem(frame) : gpu_buf3_gmem(frame, n);
    return gm->vc + (frame->data[n] - gm->arm);
}


static inline uint32_t get_vc_address_y(const AVFrame * const frame) {
    return get_vc_address3(frame, 0);
}

static inline uint32_t get_vc_address_u(const AVFrame * const frame) {
    return get_vc_address3(frame, 1);
}

static inline uint32_t get_vc_address_v(const AVFrame * const frame) {
    return get_vc_address3(frame, 2);
}

#if 0
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
#endif

// Cache flush stuff

struct rpi_cache_flush_env_s;
typedef struct rpi_cache_flush_env_s rpi_cache_flush_env_t;

typedef struct {uint32_t t[33];} rpi_cache_buf_t;

rpi_cache_flush_env_t * rpi_cache_flush_init(rpi_cache_buf_t * const buf);
// Free env without flushing
void rpi_cache_flush_abort(rpi_cache_flush_env_t * const rfe);
// Do the accumulated flush & clear but do not free the env
int rpi_cache_flush_execute(rpi_cache_flush_env_t * const rfe);
// Do the accumulated flush & free the env
int rpi_cache_flush_finish(rpi_cache_flush_env_t * const rfe);

typedef enum
{
    RPI_CACHE_FLUSH_MODE_INVALIDATE     = 1,
    RPI_CACHE_FLUSH_MODE_WRITEBACK      = 2,
    RPI_CACHE_FLUSH_MODE_WB_INVALIDATE  = 3
} rpi_cache_flush_mode_t;

void rpi_cache_flush_add_gm_ptr(rpi_cache_flush_env_t * const rfe, const GPU_MEM_PTR_T * const gm, const rpi_cache_flush_mode_t mode);
void rpi_cache_flush_add_gm_range(rpi_cache_flush_env_t * const rfe, const GPU_MEM_PTR_T * const gm, const rpi_cache_flush_mode_t mode,
  const unsigned int offset, const unsigned int size);
void rpi_cache_flush_add_gm_blocks(rpi_cache_flush_env_t * const rfe, const GPU_MEM_PTR_T * const gm, const unsigned int mode,
  const unsigned int offset0, const unsigned int block_size, const unsigned int blocks, const unsigned int block_stride);
void rpi_cache_flush_add_frame(rpi_cache_flush_env_t * const rfe, const AVFrame * const frame, const rpi_cache_flush_mode_t mode);
void rpi_cache_flush_add_frame_block(rpi_cache_flush_env_t * const rfe, const AVFrame * const frame, const rpi_cache_flush_mode_t mode,
  const unsigned int x0, const unsigned int y0, const unsigned int width, const unsigned int height,
  const unsigned int uv_shift, const int do_luma, const int do_chroma);

// init, add, finish for one gm ptr
void rpi_cache_flush_one_gm_ptr(const GPU_MEM_PTR_T * const p, const rpi_cache_flush_mode_t mode);


// QPU specific functions

typedef struct HEVCRpiQpu {
    uint32_t c_pxx;
    uint32_t c_pxx_l1;
    uint32_t c_bxx;
    uint32_t y_pxx;
    uint32_t y_bxx;
    uint32_t y_p00;
    uint32_t y_b00;
} HEVCRpiQpu;

int rpi_hevc_qpu_init_fn(HEVCRpiQpu * const qf, const unsigned int bit_depth);

uint32_t qpu_fn(const int * const mc_fn);
uint32_t qpu_dummy(void);

#define QPU_N_GRP    4
#define QPU_N_MAX    12

#define QPU_MAIL_EL_VALS  2

struct vpu_qpu_wait_s;
typedef struct vq_wait_s * vpu_qpu_wait_h;

// VPU specific functions

struct vpu_qpu_job_env_s;
typedef struct vpu_qpu_job_env_s * vpu_qpu_job_h;

#define VPU_QPU_JOB_MAX 4
struct vpu_qpu_job_env_s
{
  unsigned int n;
  unsigned int mask;
  struct gpu_job_s j[VPU_QPU_JOB_MAX];
};
typedef struct vpu_qpu_job_env_s vpu_qpu_job_env_t;

vpu_qpu_job_h vpu_qpu_job_init(vpu_qpu_job_env_t * const buf);
void vpu_qpu_job_delete(const vpu_qpu_job_h vqj);
void vpu_qpu_job_add_vpu(const vpu_qpu_job_h vqj, const uint32_t vpu_code,
  const unsigned r0, const unsigned r1, const unsigned r2, const unsigned r3, const unsigned r4, const unsigned r5);
void vpu_qpu_job_add_qpu(const vpu_qpu_job_h vqj, const unsigned int n, const uint32_t * const mail);
void vpu_qpu_job_add_sync_this(const vpu_qpu_job_h vqj, vpu_qpu_wait_h * const wait_h);
int vpu_qpu_job_add_sync_sem(vpu_qpu_job_env_t * const vqj, sem_t * const sem);
int vpu_qpu_job_start(const vpu_qpu_job_h vqj);
int vpu_qpu_job_finish(const vpu_qpu_job_h vqj);

extern unsigned int vpu_get_fn(const unsigned int bit_depth);
extern unsigned int vpu_get_constants(void);

// Waits for previous post_codee to complete and Will null out *wait_h after use
void vpu_qpu_wait(vpu_qpu_wait_h * const wait_h);
int vpu_qpu_init(void);
void vpu_qpu_term(void);

extern int gpu_get_mailbox(void);
void gpu_ref(void);
void gpu_unref(void);

#endif
