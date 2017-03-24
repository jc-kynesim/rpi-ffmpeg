#ifdef RPI
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include "libavutil/avassert.h"

#include "config.h"

#include <pthread.h>
#include <time.h>

#include "rpi_mailbox.h"
#include "rpi_qpu.h"
#include "rpi_shader.h"
#include "rpi_hevc_transform.h"

#pragma GCC diagnostic push
// Many many redundant decls in the header files
#pragma GCC diagnostic ignored "-Wredundant-decls"
#include "interface/vmcs_host/vc_vchi_gpuserv.h"
#pragma GCC diagnostic pop

// Trace time spent waiting for GPU (VPU/QPU) (1=Yes, 0=No)
#define RPI_TRACE_TIME_VPU_QPU_WAIT     1

// QPU "noflush" flags
// a mixture of flushing & profiling

#define QPU_FLAGS_NO_FLUSH_VPU          1       // If unset VPU cache will be flushed
#define QPU_FLAGS_PROF_CLEAR_AND_ENABLE 2       // Clear & Enable detailed QPU profiling registers
#define QPU_FLAGS_PROF_OUTPUT_COUNTS    4       // Print the results
#define QPU_FLAGS_OUTPUT_QPU_TIMES      8       // Print QPU times - independant of the profiling
#define QPU_FLAGS_NO_FLUSH_QPU          16      // If unset flush QPU caches & TMUs (uniforms always flushed)

// On Pi2 there is no way to access the VPU L2 cache
// GPU_MEM_FLG should be 4 for uncached memory.  (Or C for alias to allocate in the VPU L2 cache)
// However, if using VCSM allocated buffers, need to use C at the moment because VCSM does not allocate uncached memory correctly
// The QPU crashes if we mix L2 cached and L2 uncached accesses due to a HW bug.
#define GPU_MEM_FLG 0x4
// GPU_MEM_MAP is meaningless on the Pi2 and should be left at 0  (On Pi1 it allows ARM to access VPU L2 cache)
#define GPU_MEM_MAP 0x0

#define vcos_verify_ge0(x) ((x)>=0)

/*static const unsigned code[] =
{
  #include "rpi_shader.hex"
};*/

// Size in 32bit words
#define QPU_CODE_SIZE 2048
#define VPU_CODE_SIZE 2048

const short rpi_transMatrix2even[32][16] = { // Even rows first
{64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64,  64},
{90,  87,  80,  70,  57,  43,  25,   9,  -9, -25, -43, -57, -70, -80, -87, -90},
{89,  75,  50,  18, -18, -50, -75, -89, -89, -75, -50, -18,  18,  50,  75,  89},
{87,  57,   9, -43, -80, -90, -70, -25,  25,  70,  90,  80,  43,  -9, -57, -87},
{83,  36, -36, -83, -83, -36,  36,  83,  83,  36, -36, -83, -83, -36,  36,  83},
{80,   9, -70, -87, -25,  57,  90,  43, -43, -90, -57,  25,  87,  70,  -9, -80},
{75, -18, -89, -50,  50,  89,  18, -75, -75,  18,  89,  50, -50, -89, -18,  75},
{70, -43, -87,   9,  90,  25, -80, -57,  57,  80, -25, -90,  -9,  87,  43, -70},
{64, -64, -64,  64,  64, -64, -64,  64,  64, -64, -64,  64,  64, -64, -64,  64},
{57, -80, -25,  90,  -9, -87,  43,  70, -70, -43,  87,   9, -90,  25,  80, -57},
{50, -89,  18,  75, -75, -18,  89, -50, -50,  89, -18, -75,  75,  18, -89,  50},
{43, -90,  57,  25, -87,  70,   9, -80,  80,  -9, -70,  87, -25, -57,  90, -43},
{36, -83,  83, -36, -36,  83, -83,  36,  36, -83,  83, -36, -36,  83, -83,  36},
{25, -70,  90, -80,  43,   9, -57,  87, -87,  57,  -9, -43,  80, -90,  70, -25},
{18, -50,  75, -89,  89, -75,  50, -18, -18,  50, -75,  89, -89,  75, -50,  18},
{ 9, -25,  43, -57,  70, -80,  87, -90,  90, -87,  80, -70,  57, -43,  25,  -9},
// Odd rows
{90,  90,  88,  85,  82,  78,  73,  67,  61,  54,  46,  38,  31,  22,  13,   4},
{90,  82,  67,  46,  22,  -4, -31, -54, -73, -85, -90, -88, -78, -61, -38, -13},
{88,  67,  31, -13, -54, -82, -90, -78, -46,  -4,  38,  73,  90,  85,  61,  22},
{85,  46, -13, -67, -90, -73, -22,  38,  82,  88,  54,  -4, -61, -90, -78, -31},
{82,  22, -54, -90, -61,  13,  78,  85,  31, -46, -90, -67,   4,  73,  88,  38},
{78,  -4, -82, -73,  13,  85,  67, -22, -88, -61,  31,  90,  54, -38, -90, -46},
{73, -31, -90, -22,  78,  67, -38, -90, -13,  82,  61, -46, -88,  -4,  85,  54},
{67, -54, -78,  38,  85, -22, -90,   4,  90,  13, -88, -31,  82,  46, -73, -61},
{61, -73, -46,  82,  31, -88, -13,  90,  -4, -90,  22,  85, -38, -78,  54,  67},
{54, -85,  -4,  88, -46, -61,  82,  13, -90,  38,  67, -78, -22,  90, -31, -73},
{46, -90,  38,  54, -90,  31,  61, -88,  22,  67, -85,  13,  73, -82,   4,  78},
{38, -88,  73,  -4, -67,  90, -46, -31,  85, -78,  13,  61, -90,  54,  22, -82},
{31, -78,  90, -61,   4,  54, -88,  82, -38, -22,  73, -90,  67, -13, -46,  85},
{22, -61,  85, -90,  73, -38,  -4,  46, -78,  90, -82,  54, -13, -31,  67, -88},
{13, -38,  61, -78,  88, -90,  85, -73,  54, -31,   4,  22, -46,  67, -82,  90},
{ 4, -13,  22, -31,  38, -46,  54, -61,  67, -73,  78, -82,  85, -88,  90, -90}
};

// Code/constants on GPU
struct GPU
{
  unsigned int qpu_code[QPU_CODE_SIZE];
  unsigned int vpu_code[VPU_CODE_SIZE];
  short transMatrix2even[16*16*2];
};


#define WAIT_COUNT_MAX 16

typedef struct trace_time_wait_s
{
  int count;
  unsigned int jcount;
  int64_t start0;
  int64_t last_update;
  int64_t start[WAIT_COUNT_MAX];
  int64_t total[WAIT_COUNT_MAX];
} trace_time_wait_t;

typedef struct vq_wait_s
{
  sem_t sem;
  struct vq_wait_s * next;
} vq_wait_t;

#define VQ_WAIT_POOL_SIZE 16
typedef struct vq_wait_pool_s
{
  vq_wait_t * head;
  vq_wait_t pool[VQ_WAIT_POOL_SIZE];
} vq_wait_pool_t;

static void vq_wait_pool_init(vq_wait_pool_t * const pool);
static void vq_wait_pool_deinit(vq_wait_pool_t * const pool);

typedef struct gpu_env_s
{
  int open_count;
  int init_count;
  int mb;
  GPU_MEM_PTR_T code_gm_ptr;
  vq_wait_pool_t wait_pool;
#if RPI_TRACE_TIME_VPU_QPU_WAIT
  trace_time_wait_t ttw;
#endif
} gpu_env_t;

// Stop more than one thread trying to allocate memory or use the processing resources at once
static pthread_mutex_t gpu_mutex = PTHREAD_MUTEX_INITIALIZER;
static gpu_env_t * gpu = NULL;

#if RPI_TRACE_TIME_VPU_QPU_WAIT

static int64_t ns_time(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * (int64_t)1000000000 + ts.tv_nsec;
}


#define WAIT_TIME_PRINT_PERIOD (int64_t)2000000000

#define T_MS(t) ((unsigned int)((t)/(int64_t)1000000) % 1000U)
#define T_SEC(t) (unsigned int)((t)/(int64_t)1000000000)
#define T_ARG(t) T_SEC(t), T_MS(t)
#define T_FMT "%u.%03u"

static void ttw_print(trace_time_wait_t * ttw, const int64_t now)
{
  // Update totals for levels that are still pending
  for (int i = 0; i < ttw->count; ++i) {
    ttw->total[i] += now - ttw->start[i];
    ttw->start[i] = now;
  }

  printf("Jobs:%u, Total:" T_FMT ", Idle:" T_FMT ", 1:" T_FMT ", 2:" T_FMT ", 3:" T_FMT ", 4:" T_FMT "\n",
         ttw->jcount,
         T_ARG(now - ttw->start0), T_ARG(now - ttw->start0 - ttw->total[0]),
         T_ARG(ttw->total[0]),
         T_ARG(ttw->total[1]),
         T_ARG(ttw->total[2]),
         T_ARG(ttw->total[3]));
}

#endif

// GPU memory alloc fns (internal)

// GPU_MEM_PTR_T alloc fns
static int gpu_malloc_cached_internal(const int mb, const int numbytes, GPU_MEM_PTR_T * const p) {
  p->numbytes = numbytes;
  p->vcsm_handle = vcsm_malloc_cache(numbytes, VCSM_CACHE_TYPE_HOST, (char *)"Video Frame" );
  //p->vcsm_handle = vcsm_malloc_cache(numbytes, VCSM_CACHE_TYPE_VC, (char *)"Video Frame" );
  //p->vcsm_handle = vcsm_malloc_cache(numbytes, VCSM_CACHE_TYPE_NONE, (char *)"Video Frame" );
  //p->vcsm_handle = vcsm_malloc_cache(numbytes, VCSM_CACHE_TYPE_HOST_AND_VC, (char *)"Video Frame" );
  av_assert0(p->vcsm_handle);
  p->vc_handle = vcsm_vc_hdl_from_hdl(p->vcsm_handle);
  av_assert0(p->vc_handle);
  p->arm = vcsm_lock(p->vcsm_handle);
  av_assert0(p->arm);
  p->vc = mbox_mem_lock(mb, p->vc_handle);
  av_assert0(p->vc);
  return 0;
}

static int gpu_malloc_uncached_internal(const int mb, const int numbytes, GPU_MEM_PTR_T * const p) {
  p->numbytes = numbytes;
  p->vcsm_handle = vcsm_malloc_cache(numbytes, VCSM_CACHE_TYPE_NONE, (char *)"Video Frame" );
  av_assert0(p->vcsm_handle);
  p->vc_handle = vcsm_vc_hdl_from_hdl(p->vcsm_handle);
  av_assert0(p->vc_handle);
  p->arm = vcsm_lock(p->vcsm_handle);
  av_assert0(p->arm);
  p->vc = mbox_mem_lock(mb, p->vc_handle);
  av_assert0(p->vc);
  return 0;
}

static void gpu_free_internal(const int mb, GPU_MEM_PTR_T * const p) {
  mbox_mem_unlock(mb, p->vc_handle);
  vcsm_unlock_ptr(p->arm);
  vcsm_free(p->vcsm_handle);
  memset(p, 0, sizeof(*p));  // Ensure we crash hard if we try and use this again
}


// GPU init, free, lock, unlock

static void gpu_term(void)
{
  gpu_env_t * const ge = gpu;

  // We have to hope that eveything has terminated...
  gpu = NULL;

  vc_gpuserv_deinit();

  gpu_free_internal(ge->mb, &ge->code_gm_ptr);

  vcsm_exit();

  mbox_close(ge->mb);

  vq_wait_pool_deinit(&ge->wait_pool);

  free(ge);
}


// Connect to QPU, returns 0 on success.
static int gpu_init(gpu_env_t ** const gpu) {
  volatile struct GPU* ptr;
  gpu_env_t * const ge = calloc(1, sizeof(gpu_env_t));
  *gpu = NULL;

  if (ge == NULL)
    return -1;

  if ((ge->mb = mbox_open()) < 0)
    return -1;

  vq_wait_pool_init(&ge->wait_pool);

  vcsm_init();

  gpu_malloc_uncached_internal(ge->mb, sizeof(struct GPU), &ge->code_gm_ptr);
  ptr = (volatile struct GPU*)ge->code_gm_ptr.arm;

  // Zero everything so we have zeros between the code bits
  memset((void *)ptr, 0, sizeof(*ptr));

  // Now copy over the QPU code into GPU memory
  {
    int num_bytes = (char *)mc_end - (char *)rpi_shader;
    av_assert0(num_bytes<=QPU_CODE_SIZE*sizeof(unsigned int));
    memcpy((void*)ptr->qpu_code, rpi_shader, num_bytes);
  }
  // And the VPU code
  {
    int num_bytes = sizeof(rpi_hevc_transform);
    av_assert0(num_bytes<=VPU_CODE_SIZE*sizeof(unsigned int));
    memcpy((void*)ptr->vpu_code, rpi_hevc_transform, num_bytes);
  }
  // And the transform coefficients
  memcpy((void*)ptr->transMatrix2even, rpi_transMatrix2even, sizeof(rpi_transMatrix2even));

  *gpu = ge;
  return 0;
}



static void gpu_unlock(void) {
  pthread_mutex_unlock(&gpu_mutex);
}

// Make sure we have exclusive access to the mailbox, and enable qpu if necessary.
static gpu_env_t * gpu_lock(void) {
  pthread_mutex_lock(&gpu_mutex);

  av_assert0(gpu != NULL);
  return gpu;
}

static gpu_env_t * gpu_lock_ref(void)
{
  pthread_mutex_lock(&gpu_mutex);

  if (gpu == NULL) {
    int rv = gpu_init(&gpu);
    if (rv != 0) {
      gpu_unlock();
      return NULL;
    }
  }

  ++gpu->open_count;
  return gpu;
}

static void gpu_unlock_unref(gpu_env_t * const ge)
{
  if (--ge->open_count == 0)
    gpu_term();

  gpu_unlock();
}


// Public gpu fns

// Allocate memory on GPU
// Fills in structure <p> containing ARM pointer, videocore handle, videocore memory address, numbytes
// Returns 0 on success.
// This allocates memory that will not be cached in ARM's data cache.
// Therefore safe to use without data cache flushing.
int gpu_malloc_uncached(int numbytes, GPU_MEM_PTR_T *p)
{
  int r;
  gpu_env_t * const ge = gpu_lock_ref();
  if (ge == NULL)
    return -1;
  r = gpu_malloc_uncached_internal(ge->mb, numbytes, p);
  gpu_unlock();
  return r;
}

// This allocates data that will be
//    Cached in ARM L2
//    Uncached in VPU L2
int gpu_malloc_cached(int numbytes, GPU_MEM_PTR_T *p)
{
  int r;
  gpu_env_t * const ge = gpu_lock_ref();
  if (ge == NULL)
    return -1;
  r = gpu_malloc_cached_internal(ge->mb, numbytes, p);
  gpu_unlock();
  return r;
}

void gpu_free(GPU_MEM_PTR_T * const p) {
  gpu_env_t * const ge = gpu_lock();
  gpu_free_internal(ge->mb, p);
  gpu_unlock_unref(ge);
}

unsigned int vpu_get_fn(void) {
  // Make sure that the gpu is initialized
  av_assert0(gpu != NULL);
  return gpu->code_gm_ptr.vc + offsetof(struct GPU, vpu_code);
}

unsigned int vpu_get_constants(void) {
  av_assert0(gpu != NULL);
  return gpu->code_gm_ptr.vc + offsetof(struct GPU,transMatrix2even);
}

int gpu_get_mailbox(void)
{
  av_assert0(gpu);
  return gpu->mb;
}

// ----------------------------------------------------------------------------
//
// Cache flush functions


rpi_cache_flush_env_t * rpi_cache_flush_init()
{
    rpi_cache_flush_env_t * const rfe = calloc(1, sizeof(rpi_cache_flush_env_t));
    if (rfe == NULL)
        return NULL;

    return rfe;
}

void rpi_cache_flush_abort(rpi_cache_flush_env_t * const rfe)
{
    if (rfe != NULL)
        free(rfe);
}

int rpi_cache_flush_finish(rpi_cache_flush_env_t * const rfe)
{
    int rc = vcsm_clean_invalid(&rfe->a);

    free(rfe);

    if (rc == 0)
        return 0;

    av_log(NULL, AV_LOG_ERROR, "vcsm_clean_invalid failed: errno=%d\n", errno);
    return rc;
}

void rpi_cache_flush_add_gm_ptr(rpi_cache_flush_env_t * const rfe, const GPU_MEM_PTR_T * const gm, const unsigned int mode)
{
    av_assert0(rfe->n < sizeof(rfe->a.s) / sizeof(rfe->a.s[0]));

    // Deal with empty pointer trivially
    if (gm == NULL || gm->numbytes == 0)
        return;

    rfe->a.s[rfe->n].cmd = mode;
    rfe->a.s[rfe->n].handle = gm->vcsm_handle;
    rfe->a.s[rfe->n].addr = (unsigned int)gm->arm;
    rfe->a.s[rfe->n].size = gm->numbytes;
    ++rfe->n;
}

void rpi_cache_flush_add_gm_range(rpi_cache_flush_env_t * const rfe, const GPU_MEM_PTR_T * const gm, const unsigned int mode,
  const unsigned int offset, const unsigned int size)
{
    // Deal with empty pointer trivially
    if (gm == NULL || size == 0)
        return;

    av_assert0(rfe->n < sizeof(rfe->a.s) / sizeof(rfe->a.s[0]));
    av_assert0(offset + size <= gm->numbytes);

    rfe->a.s[rfe->n].cmd = mode;
    rfe->a.s[rfe->n].handle = gm->vcsm_handle;
    rfe->a.s[rfe->n].addr = (unsigned int)gm->arm + offset;
    rfe->a.s[rfe->n].size = size;
    ++rfe->n;
}

void rpi_cache_flush_add_frame(rpi_cache_flush_env_t * const rfe, const AVFrame * const frame, const unsigned int mode)
{
#if !RPI_ONE_BUF
#error Fixme! (NIF)
#endif
  if (gpu_is_buf1(frame)) {
    rpi_cache_flush_add_gm_ptr(rfe, gpu_buf1_gmem(frame), mode);
  }
  else
  {
    rpi_cache_flush_add_gm_ptr(rfe, gpu_buf3_gmem(frame, 0), mode);
    rpi_cache_flush_add_gm_ptr(rfe, gpu_buf3_gmem(frame, 1), mode);
    rpi_cache_flush_add_gm_ptr(rfe, gpu_buf3_gmem(frame, 2), mode);
  }
}

void rpi_cache_flush_add_frame_lines(rpi_cache_flush_env_t * const rfe, const AVFrame * const frame, const unsigned int mode,
  const unsigned int start_line, const unsigned int n, const unsigned int uv_shift, const int do_luma, const int do_chroma)
{
  const unsigned int y_offset = frame->linesize[0] * start_line;
  const unsigned int y_size = frame->linesize[0] * n;
  // Round UV up/down to get everything
  const unsigned int uv_rnd = (1U << uv_shift) >> 1;
  const unsigned int uv_offset = frame->linesize[1] * (start_line >> uv_shift);
  const unsigned int uv_size = frame->linesize[1] * ((start_line + n + uv_rnd) >> uv_shift) - uv_offset;

  if (gpu_is_buf1(frame)) {
    const GPU_MEM_PTR_T * const gm = gpu_buf1_gmem(frame);
    if (do_luma) {
      rpi_cache_flush_add_gm_range(rfe, gm, mode, y_offset, y_size);
    }
    if (do_chroma) {
      rpi_cache_flush_add_gm_range(rfe, gm, mode, frame->data[1] - frame->data[0] + uv_offset, uv_size);
      rpi_cache_flush_add_gm_range(rfe, gm, mode, frame->data[2] - frame->data[0] + uv_offset, uv_size);
    }
  }
  else
  {
    if (do_luma) {
      rpi_cache_flush_add_gm_range(rfe, gpu_buf3_gmem(frame, 0), mode, y_offset, y_size);
    }
    if (do_chroma) {
      rpi_cache_flush_add_gm_range(rfe, gpu_buf3_gmem(frame, 1), mode, uv_offset, uv_size);
      rpi_cache_flush_add_gm_range(rfe, gpu_buf3_gmem(frame, 2), mode, uv_offset, uv_size);
    }
  }
}

// Call this to clean and invalidate a region of memory
void rpi_cache_flush_one_gm_ptr(const GPU_MEM_PTR_T *const p, const rpi_cache_flush_mode_t mode)
{
  rpi_cache_flush_env_t * rfe = rpi_cache_flush_init();
  rpi_cache_flush_add_gm_ptr(rfe, p, mode);
  rpi_cache_flush_finish(rfe);
}


// ----------------------------------------------------------------------------


// Wait abstractions - mostly so we can easily add profile code
static void vq_wait_pool_init(vq_wait_pool_t * const wp)
{
  unsigned int i;
  for (i = 0; i != VQ_WAIT_POOL_SIZE; ++i) {
    sem_init(&wp->pool[i].sem, 0, 0);
    wp->pool[i].next = wp->pool + i + 1;
  }
  wp->head = wp->pool + 0;
  wp->pool[VQ_WAIT_POOL_SIZE - 1].next = NULL;
}

static void vq_wait_pool_deinit(vq_wait_pool_t * const wp)
{
  unsigned int i;
  wp->head = NULL;
  for (i = 0; i != VQ_WAIT_POOL_SIZE; ++i) {
    sem_destroy(&wp->pool[i].sem);
    wp->pool[i].next = NULL;
  }
}


// If sem_init actually takes time then maybe we want a pool...
static vq_wait_t * vq_wait_new(void)
{
  gpu_env_t * const ge = gpu_lock_ref();
  vq_wait_t * const wait = ge->wait_pool.head;
  ge->wait_pool.head = wait->next;
  wait->next = NULL;

#if RPI_TRACE_TIME_VPU_QPU_WAIT
  {
    trace_time_wait_t * const ttw = &ge->ttw;
    const int n = ttw->count++;
    ttw->jcount++;
    ttw->start[n] = ns_time();
  }
#endif

  gpu_unlock();
  return wait;
}

static void vq_wait_delete(vq_wait_t * const wait)
{
  gpu_env_t * const ge = gpu_lock();
  wait->next = ge->wait_pool.head;
  ge->wait_pool.head = wait;

#if RPI_TRACE_TIME_VPU_QPU_WAIT
  {
    trace_time_wait_t * const ttw = &ge->ttw;
    const int n = --ttw->count;
    const int64_t now = ns_time();
    av_assert0(n >= 0);

    ttw->total[n] += now - ttw->start[n];
    if (ttw->start0 == 0)
    {
      ttw->last_update = ttw->start[n];
      ttw->start0 = ttw->start[n];
    }
    if (now - ttw->last_update > WAIT_TIME_PRINT_PERIOD)
    {
      ttw->last_update += WAIT_TIME_PRINT_PERIOD;
      ttw_print(ttw, now);
    }
  }
#endif
  gpu_unlock_unref(ge);
}

static void vq_wait_wait(vq_wait_t * const wait)
{
  int err;

  while ((err = sem_wait(&wait->sem)) == -1 && errno == EINTR)
    /* loop */;
}

static void vq_wait_post(vq_wait_t * const wait)
{
  sem_post(&wait->sem);
}



// Header comments were wrong for these two
#define VPU_QPU_MASK_QPU  1
#define VPU_QPU_MASK_VPU  2

#define VPU_QPU_JOB_MAX 4
typedef struct vpu_qpu_job_env_s
{
  unsigned int n;
  unsigned int mask;
  struct gpu_job_s j[VPU_QPU_JOB_MAX];
} vpu_qpu_job_env_t;

static vpu_qpu_job_env_t * vpu_qpu_job_new(void)
{
  vpu_qpu_job_env_t * vqj = calloc(1, sizeof(vpu_qpu_job_env_t));
  return vqj;
}

static void vpu_qpu_job_delete(vpu_qpu_job_env_t * const vqj)
{
  memset(vqj, 0, sizeof(*vqj));
  free(vqj);
}

static inline struct gpu_job_s * new_job(vpu_qpu_job_env_t * const vqj)
{
  struct gpu_job_s * const j = vqj->j + vqj->n++;
  av_assert0(vqj->n <= VPU_QPU_JOB_MAX);
  return j;
}

static void vpu_qpu_add_vpu(vpu_qpu_job_env_t * const vqj, const uint32_t vpu_code,
  const unsigned r0, const unsigned r1, const unsigned r2, const unsigned r3, const unsigned r4, const unsigned r5)
{
  if (vpu_code != 0) {
    struct gpu_job_s *const j = new_job(vqj);
    vqj->mask |= VPU_QPU_MASK_VPU;

    j->command = EXECUTE_VPU;
    j->u.v.q[0] = vpu_code;
    j->u.v.q[1] = r0;
    j->u.v.q[2] = r1;
    j->u.v.q[3] = r2;
    j->u.v.q[4] = r3;
    j->u.v.q[5] = r4;
    j->u.v.q[6] = r5;
  }
}

// flags are QPU_FLAGS_xxx
static void vpu_qpu_add_qpu(vpu_qpu_job_env_t * const vqj, const unsigned int n, const uint32_t flags, const uint32_t * const mail)
{
  if (n != 0) {
    struct gpu_job_s *const j = new_job(vqj);
    vqj->mask |= VPU_QPU_MASK_QPU;

    j->command = EXECUTE_QPU;
    j->u.q.jobs = n;
    j->u.q.noflush = flags;
    j->u.q.timeout = 5000;
    memcpy(j->u.q.control, mail, n * QPU_MAIL_EL_VALS * sizeof(uint32_t));
  }
}

static void vpu_qpu_callback_wait(void * v)
{
  vq_wait_post(v);
}

static void vpu_qpu_add_sync_this(vpu_qpu_job_env_t * const vqj, vpu_qpu_wait_h * const wait_h)
{
  vq_wait_t * wait;

  if (vqj->mask == 0) {
    *wait_h = NULL;
    return;
  }

  // We are going to want a sync object
  wait = vq_wait_new();

  // There are 2 VPU Qs & 1 QPU Q so we can collapse sync
  // If we only posted one thing or only QPU jobs
  if (vqj->n == 1 || vqj->mask == VPU_QPU_MASK_QPU)
  {
    struct gpu_job_s * const j = vqj->j + (vqj->n - 1);
    av_assert0(j->callback.func == 0);

    j->callback.func = vpu_qpu_callback_wait;
    j->callback.cookie = wait;
  }
  else
  {
    struct gpu_job_s *const j = new_job(vqj);

    j->command = EXECUTE_SYNC;
    j->u.s.mask = vqj->mask;
    j->callback.func = vpu_qpu_callback_wait;
    j->callback.cookie = wait;
  }

  vqj->mask = 0;
  *wait_h = wait;
}

static int vpu_qpu_start(vpu_qpu_job_env_t * const vqj)
{
  return vqj->n == 0 ? 0 : vc_gpuserv_execute_code(vqj->n, vqj->j);
}


int vpu_qpu_post_code2(unsigned vpu_code, unsigned r0, unsigned r1, unsigned r2, unsigned r3, unsigned r4, unsigned r5,
    int qpu0_n, const uint32_t * qpu0_mail,
    int qpu1_n, const uint32_t * qpu1_mail,
    vpu_qpu_wait_h * const wait_h)
{
  vpu_qpu_job_env_t * const vqj = vpu_qpu_job_new();

  uint32_t qpu_flags = QPU_FLAGS_NO_FLUSH_VPU;
#if 0
  static int z = 0;
  if (z == 0) {
    z = 1;
    qpu_flags |= QPU_FLAGS_PROF_CLEAR_AND_ENABLE;
  }
  else if ((z++ & 7) == 0) {
    qpu_flags |= QPU_FLAGS_PROF_OUTPUT_COUNTS;
  }
#endif

  vpu_qpu_add_vpu(vqj, vpu_code, r0, r1, r2, r3, r4, r5);
  vpu_qpu_add_qpu(vqj, qpu0_n, QPU_FLAGS_NO_FLUSH_VPU, qpu0_mail);
  vpu_qpu_add_qpu(vqj, qpu1_n, qpu_flags, qpu1_mail);
  if (wait_h != NULL)
    vpu_qpu_add_sync_this(vqj, wait_h);
  av_assert0(vpu_qpu_start(vqj) == 0);
  vpu_qpu_job_delete(vqj);

  return 0;
}

void vpu_qpu_wait(vpu_qpu_wait_h * const wait_h)
{
  if (wait_h != NULL)
  {
    vq_wait_t * const wait = *wait_h;
    if (wait != NULL) {
      *wait_h = NULL;
      vq_wait_wait(wait);
      vq_wait_delete(wait);
    }
  }
}

int vpu_qpu_init()
{
  gpu_env_t * const ge = gpu_lock_ref();
  if (ge == NULL)
    return -1;

  if (ge->init_count++ == 0)
  {
    vc_gpuserv_init();
  }

  gpu_unlock();
  return 0;
}

void vpu_qpu_term()
{
  gpu_env_t * const ge = gpu_lock();

  if (--ge->init_count == 0) {
    vc_gpuserv_deinit();

#if RPI_TRACE_TIME_VPU_QPU_WAIT
    ttw_print(&ge->ttw, ns_time());
#endif
  }

  gpu_unlock_unref(ge);
}

uint32_t qpu_fn(const int * const mc_fn)
{
  return gpu->code_gm_ptr.vc + ((const char *)mc_fn - (const char *)rpi_shader) + offsetof(struct GPU, qpu_code);
}

#if 0
typedef unsigned int uint32_t;

typedef struct mvs_s {
    GPU_MEM_PTR_T unif_mvs_ptr;
    uint32_t *unif_mvs; // Base of memory for motion vector commands

    // _base pointers are to the start of the row
    uint32_t *mvs_base[8];
    // these pointers are to the next free space
    uint32_t *u_mvs[8];

} HEVCContext;

#define RPI_CHROMA_COMMAND_WORDS 12

static void rpi_inter_clear(HEVCContext *s)
{
    int i;
    for(i=0;i<8;i++) {
        s->u_mvs[i] = s->mvs_base[i];
        *s->u_mvs[i]++ = 0;
        *s->u_mvs[i]++ = 0;
        *s->u_mvs[i]++ = 0;
        *s->u_mvs[i]++ = 0;
        *s->u_mvs[i]++ = 0;
        *s->u_mvs[i]++ = 128;  // w
        *s->u_mvs[i]++ = 128;  // h
        *s->u_mvs[i]++ = 128;  // stride u
        *s->u_mvs[i]++ = 128;  // stride v
        s->u_mvs[i] += 3;  // Padding words
    }
}

static void rpi_execute_inter_qpu(HEVCContext *s)
{
    int k;
    uint32_t *unif_vc = (uint32_t *)s->unif_mvs_ptr.vc;

    for(k=0;k<8;k++) {
        s->u_mvs[k][-RPI_CHROMA_COMMAND_WORDS] = qpu_get_fn(QPU_MC_EXIT); // Add exit command
        s->u_mvs[k][-RPI_CHROMA_COMMAND_WORDS+3] = qpu_get_fn(QPU_MC_SETUP); // A dummy texture location (maps to our code) - this is needed as the texture requests are pipelined
        s->u_mvs[k][-RPI_CHROMA_COMMAND_WORDS+4] = qpu_get_fn(QPU_MC_SETUP); //  dummy location for V
    }

    s->u_mvs[8-1][-RPI_CHROMA_COMMAND_WORDS] = qpu_get_fn(QPU_MC_INTERRUPT_EXIT8); // This QPU will signal interrupt when all others are done and have acquired a semaphore

    qpu_run_shader8(qpu_get_fn(QPU_MC_SETUP_UV),
      (uint32_t)(unif_vc+(s->mvs_base[0 ] - (uint32_t*)s->unif_mvs_ptr.arm)),
      (uint32_t)(unif_vc+(s->mvs_base[1 ] - (uint32_t*)s->unif_mvs_ptr.arm)),
      (uint32_t)(unif_vc+(s->mvs_base[2 ] - (uint32_t*)s->unif_mvs_ptr.arm)),
      (uint32_t)(unif_vc+(s->mvs_base[3 ] - (uint32_t*)s->unif_mvs_ptr.arm)),
      (uint32_t)(unif_vc+(s->mvs_base[4 ] - (uint32_t*)s->unif_mvs_ptr.arm)),
      (uint32_t)(unif_vc+(s->mvs_base[5 ] - (uint32_t*)s->unif_mvs_ptr.arm)),
      (uint32_t)(unif_vc+(s->mvs_base[6 ] - (uint32_t*)s->unif_mvs_ptr.arm)),
      (uint32_t)(unif_vc+(s->mvs_base[7 ] - (uint32_t*)s->unif_mvs_ptr.arm))
      );
}

void rpi_test_qpu(void)
{
    HEVCContext mvs;
    HEVCContext *s = &mvs;
    int i;
    int uv_commands_per_qpu = (1 + (256*64*2)/(4*4)) * RPI_CHROMA_COMMAND_WORDS;
    uint32_t *p;
    printf("Allocate memory\n");
    gpu_malloc_uncached( 8 * uv_commands_per_qpu * sizeof(uint32_t), &s->unif_mvs_ptr );
    s->unif_mvs = (uint32_t *) s->unif_mvs_ptr.arm;

    // Set up initial locations for uniform streams
    p = s->unif_mvs;
    for(i = 0; i < 8; i++) {
        s->mvs_base[i] = p;
        p += uv_commands_per_qpu;
    }
    // Now run a simple program that should just quit immediately after a single texture fetch
    rpi_inter_clear(s);
    for(i=0;i<4;i++) {
      printf("Launch QPUs\n");
      rpi_execute_inter_qpu(s);
      printf("Done\n");
    }
    printf("Free memory\n");
    gpu_free(&s->unif_mvs_ptr);
    return;
}
#endif

#if 0

int32_t hcoeffs[] = {-4, 10, -21, 70, 90, -24, 11, -4};
//int32_t hcoeffs[] = {1, 1, 1, 1, 1, 1, 1, 1};
int32_t vcoeffs[] = {-2, 6, -13, 37, 115, -20, 9, -4};
//int32_t vcoeffs[] = {1, 1, 1, 1, 1, 1, 1, 1};

#define ENCODE_COEFFS(c0, c1, c2, c3) (((c0-1) & 0xff) | ((c1-1) & 0xff) << 8 | ((c2-1) & 0xff) << 16 | ((c3-1) & 0xff) << 24);

static uint8_t av_clip_uint8(int32_t a)
{
    if (a&(~255)) return (-a)>>31;
    else          return a;
}

static int32_t filter8(const uint8_t *data, int pitch)
{
   int32_t vsum = 0;
   int x, y;

   for (y = 0; y < 8; y++) {
      int32_t hsum = 0;

      for (x = 0; x < 8; x++)
         hsum += hcoeffs[x]*data[x + y * pitch];

      vsum += vcoeffs[y]*av_clip_uint8( (hsum + 64) >> 7); // Added brackets to stop compiler warning
   }

   return av_clip_uint8( (vsum + 64) >> 7);
}

// Note regression changes coefficients so is not thread safe
//#define REGRESSION
#ifdef REGRESSION
#define CMAX 100
#else
#define CMAX 2
#endif
#define YMAX 16

int rpi_test_shader(void)
{
   int i, c;

   uint32_t *unifs;

   uint8_t *in_buffer;
   uint8_t *out_buffer[2];

   GPU_MEM_PTR_T unifs_ptr;
   GPU_MEM_PTR_T in_buffer_ptr;
   GPU_MEM_PTR_T out_buffer_ptr[2];

   // Addresses in GPU memory of filter programs
   uint32_t mc_setup = 0;
   uint32_t mc_filter = 0;
   uint32_t mc_exit = 0;

   int pitch = 0x500;

   if (gpu==NULL) {
      gpu_lock();
      gpu_unlock();
   }

   printf("This needs to change to reflect new assembler\n");
   // Use table to compute locations of program start points
   mc_setup = code[0] + gpu->vc;
   mc_filter = code[1] + gpu->vc;
   mc_exit = code[2] + gpu->vc;

   if (!vcos_verify_ge0(gpu_malloc_uncached(4*64,&unifs_ptr))) {
      return -2;
   }
   unifs = (uint32_t*)unifs_ptr.arm;

   if (!vcos_verify_ge0(gpu_malloc_uncached(64*23,&in_buffer_ptr))) {
      return -3;
   }
   in_buffer = (uint8_t*)in_buffer_ptr.arm;

   if (!vcos_verify_ge0(gpu_malloc_uncached(16*pitch,&out_buffer_ptr[0])) || !vcos_verify_ge0(gpu_malloc_uncached(16*pitch,&out_buffer_ptr[1]))) {
      return -4;
   }
   out_buffer[0] = (uint8_t*)out_buffer_ptr[0].arm;
   out_buffer[1] = (uint8_t*)out_buffer_ptr[1].arm;

   for (c = 0; c < CMAX; c++) {
      int xo[] = {rand()&31, rand()&31};

#ifdef REGRESSION
      for (i = 0; i < 8; i++) {
         hcoeffs[i] = (int8_t)rand();
         vcoeffs[i] = (int8_t)rand();
         if (hcoeffs[i]==-128)
           hcoeffs[i]++;
         if (vcoeffs[i]==-128)
           vcoeffs[i]++;
      }
#endif

      for (i = 0; i < 64*23; i++) {
         //printf("%d %d %p\n",i,gpu->mb,&in_buffer[i]);
         in_buffer[i] = rand();
      }

      // Clear output array
      {
        int b;
        for(b=0;b<2;b++) {
          for(i=0;i<16*16;i++) {
            out_buffer[b][i] = 3;
          }
        }
      }

      unifs[0] = mc_filter;
      unifs[1] = in_buffer_ptr.vc+xo[0]+16;
      unifs[2] = 64; // src pitch
      unifs[3] = pitch; // dst pitch
      unifs[4] = 0; // Padding
      unifs[5] = 0;
      unifs[6] = 0;
      unifs[7 ] = mc_filter;
      unifs[8 ] = in_buffer_ptr.vc+xo[1]+16;
      unifs[9 ] = ENCODE_COEFFS(hcoeffs[0], hcoeffs[1], hcoeffs[2], hcoeffs[3]);
      unifs[10] = ENCODE_COEFFS(hcoeffs[4], hcoeffs[5], hcoeffs[6], hcoeffs[7]);
      unifs[11] = ENCODE_COEFFS(vcoeffs[0], vcoeffs[1], vcoeffs[2], vcoeffs[3]);
      unifs[12] = ENCODE_COEFFS(vcoeffs[4], vcoeffs[5], vcoeffs[6], vcoeffs[7]);
      unifs[13] = out_buffer_ptr[0].vc;
      unifs[14] = mc_exit;
      unifs[15] = in_buffer_ptr.vc+xo[1]+16;        // dummy
      unifs[16] = ENCODE_COEFFS(hcoeffs[0], hcoeffs[1], hcoeffs[2], hcoeffs[3]);
      unifs[17] = ENCODE_COEFFS(hcoeffs[4], hcoeffs[5], hcoeffs[6], hcoeffs[7]);
      unifs[18] = ENCODE_COEFFS(vcoeffs[0], vcoeffs[1], vcoeffs[2], vcoeffs[3]);
      unifs[19] = ENCODE_COEFFS(vcoeffs[4], vcoeffs[5], vcoeffs[6], vcoeffs[7]);
      unifs[20] = out_buffer_ptr[1].vc;

      printf("Gpu->vc=%x Code=%x dst=%x\n",gpu->vc, mc_filter,out_buffer_ptr[1].vc);

      // flush_dcache(); TODO is this needed on ARM side? - tried to use the direct alias to avoid this problem

      //qpu_run_shader(mc_setup, unifs_ptr.vc);
      //qpu_run_shader(gpu, gpu->vc, unifs_ptr.vc);
      rpi_do_block(in_buffer_ptr.vc+xo[0]+16, 64, out_buffer_ptr[0].vc, pitch,out_buffer[0]);
      rpi_do_block(in_buffer_ptr.vc+xo[1]+16, 64, out_buffer_ptr[1].vc, pitch,out_buffer[1]);

      if (1)
      {
         int x, y, b;
         int bad = 0;

         for (b=0; b<2; ++b)
            for (y=0; y<YMAX; ++y)
               for (x=0; x<16; ++x) {
                  int32_t ref = filter8(in_buffer+x+y*64+xo[b], 64);

                  if (out_buffer[b][x+y*pitch] != ref) {
                      bad = 1;
//                     printf("%d, %d, %d, %d\n", c, b, x, y);
                  }
#ifndef REGRESSION
                  //printf("%08x %08x\n", out_buffer[b][x+y*pitch], ref);
#endif
               }
          if (bad)
            printf("Failed dst=%x test=%d\n",out_buffer_ptr[1].vc,c);
          else
            printf("Passed dst=%x test=%d\n",out_buffer_ptr[1].vc,c);
      }
      //printf("%d\n", simpenrose_get_qpu_tick_count());
   }

   gpu_free(&out_buffer_ptr[0]);
   gpu_free(&out_buffer_ptr[1]);
   gpu_free(&in_buffer_ptr);
   gpu_free(&unifs_ptr);

   return 0;
}

void rpi_do_block_arm(const uint8_t *in_buffer, int src_pitch, uint8_t *dst, int dst_pitch)
{
  int x,y;
  for (y=0; y<16; ++y) {
    for (x=0; x<16; ++x) {
       dst[x+y*dst_pitch] = filter8(in_buffer+x+y*src_pitch, src_pitch);
    }
  }
}

void rpi_do_block(const uint8_t *in_buffer_vc, int src_pitch, uint8_t *dst_vc, int dst_pitch, uint8_t *dst)
{
   uint32_t *unifs;

   GPU_MEM_PTR_T unifs_ptr;
   //uint8_t *out_buffer;
   //GPU_MEM_PTR_T out_buffer_ptr;

   // Addresses in GPU memory of filter programs
   uint32_t mc_setup = 0;
   uint32_t mc_filter = 0;
   uint32_t mc_exit = 0;
   //int x,y;

   if (gpu==NULL) {
      gpu_lock();
      gpu_unlock();
   }

   // Use table to compute locations of program start points
   mc_setup = code[0] + gpu->vc;
   mc_filter = code[1] + gpu->vc;
   mc_exit = code[2] + gpu->vc;

   if (!vcos_verify_ge0(gpu_malloc_uncached(4*64,&unifs_ptr))) {
      return;
   }
   //gpu_malloc_uncached(16*dst_pitch,&out_buffer_ptr);
   //out_buffer = (uint8_t*)out_buffer_ptr.arm;

   /*for (y=0; y<16; ++y) {
      for (x=0; x<16; ++x) {
         out_buffer[x+y*dst_pitch] = 7;
      }
    }*/

   unifs = (uint32_t*)unifs_ptr.arm;

    unifs[0] = mc_filter;
    unifs[1] = (int)in_buffer_vc;
    unifs[2] = src_pitch; // src pitch
    unifs[3] = dst_pitch; // dst pitch
    unifs[4] = 0; // Padding
    unifs[5] = 0;
    unifs[6] = 0;
    unifs[7 ] = mc_exit;
    unifs[8 ] = (int)in_buffer_vc;
    unifs[9 ] = ENCODE_COEFFS(hcoeffs[0], hcoeffs[1], hcoeffs[2], hcoeffs[3]);
    unifs[10] = ENCODE_COEFFS(hcoeffs[4], hcoeffs[5], hcoeffs[6], hcoeffs[7]);
    unifs[11] = ENCODE_COEFFS(vcoeffs[0], vcoeffs[1], vcoeffs[2], vcoeffs[3]);
    unifs[12] = ENCODE_COEFFS(vcoeffs[4], vcoeffs[5], vcoeffs[6], vcoeffs[7]);
    unifs[13] = (int)dst_vc;
    //unifs[13] = (int)out_buffer_ptr.vc;

    //printf("Gpu->vc=%x Code=%x dst=%x\n",gpu->vc, mc_filter,out_buffer_ptr[1].vc);

    qpu_run_shader(mc_setup, unifs_ptr.vc);

    /*for (y=0; y<16; ++y) {
      for (x=0; x<16; ++x) {
         dst[x+y*dst_pitch] = out_buffer[x+y*dst_pitch];
      }
    }*/

    gpu_free(&unifs_ptr);
    //gpu_free(&out_buffer_ptr);
}



#endif

#endif // RPI
