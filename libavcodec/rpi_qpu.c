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
#define RPI_TRACE_TIME_VPU_QPU_WAIT     0

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

typedef struct trace_time_one_s
{
  int count;
  int64_t start[WAIT_COUNT_MAX];
  int64_t total[WAIT_COUNT_MAX];
} trace_time_one_t;

typedef struct trace_time_wait_s
{
  unsigned int jcount;
  int64_t start0;
  int64_t last_update;
  trace_time_one_t active;
  trace_time_one_t wait;
} trace_time_wait_t;

typedef struct vq_wait_s
{
  sem_t sem;
  unsigned int cost;
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
  unsigned int current_load;
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

static void tto_print(trace_time_one_t * tto, const int64_t now, const int64_t start0, const char * const prefix)
{
  // Update totals for levels that are still pending
  for (int i = 0; i < tto->count; ++i) {
    tto->total[i] += now - tto->start[i];
    tto->start[i] = now;
  }

  printf("%s: Idle:" T_FMT ", 1:" T_FMT ", 2:" T_FMT ", 3:" T_FMT ", 4:" T_FMT "\n",
         prefix,
         T_ARG(now - start0 - tto->total[0]),
         T_ARG(tto->total[0]),
         T_ARG(tto->total[1]),
         T_ARG(tto->total[2]),
         T_ARG(tto->total[3]));
}


static void tto_start(trace_time_one_t * const tto, const int64_t now)
{
  av_assert0(tto->count < WAIT_COUNT_MAX);
  tto->start[tto->count++] = now;
}

static void tto_end(trace_time_one_t * const tto, const int64_t now)
{
  const int n = --tto->count;
  av_assert0(n >= 0);
  tto->total[n] += now - tto->start[n];
}

static void ttw_print(trace_time_wait_t * const ttw, const int64_t now)
{
  printf("Jobs:%d, Total time=" T_FMT "\n", ttw->jcount, T_ARG(now - ttw->start0));
  tto_print(&ttw->active, now, ttw->start0, "Active");
  tto_print(&ttw->wait,   now, ttw->start0, "  Wait");
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

static inline gpu_env_t * gpu_ptr(void)
{
  av_assert0(gpu != NULL);
  return gpu;
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
    int rc = (rfe->n == 0) ? 0 : vcsm_clean_invalid(&rfe->a);

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
    av_assert0(offset <= gm->numbytes);
    av_assert0(size <= gm->numbytes);
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

  // As all unsigned they will also reject -ve
  // Test individually as well as added to reject overflow
  av_assert0(start_line <= (unsigned int)frame->height);
  av_assert0(n <= (unsigned int)frame->height);
  av_assert0(start_line + n <= (unsigned int)frame->height);

  if (gpu_is_buf1(frame)) {
    const GPU_MEM_PTR_T * const gm = gpu_buf1_gmem(frame);
    if (do_luma) {
      rpi_cache_flush_add_gm_range(rfe, gm, mode, (frame->data[0] - gm->arm) + y_offset, y_size);
    }
    if (do_chroma) {
      rpi_cache_flush_add_gm_range(rfe, gm, mode, (frame->data[1] - gm->arm) + uv_offset, uv_size);
      rpi_cache_flush_add_gm_range(rfe, gm, mode, (frame->data[2] - gm->arm) + uv_offset, uv_size);
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
static vq_wait_t * vq_wait_new(const unsigned int cost)
{
  gpu_env_t * const ge = gpu_lock_ref();
  vq_wait_t * const wait = ge->wait_pool.head;
  ge->wait_pool.head = wait->next;
  ge->current_load += cost;
  wait->cost = cost;
  wait->next = NULL;

#if RPI_TRACE_TIME_VPU_QPU_WAIT
  tto_start(&ge->ttw.active, ns_time());
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
    const int64_t now = ns_time();
    ++ttw->jcount;
    tto_end(&ttw->wait, now);

    if (ttw->start0 == 0)
    {
      ttw->start0 = ttw->active.start[0];
      ttw->last_update = ttw->start0;
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
#if RPI_TRACE_TIME_VPU_QPU_WAIT
  {
      const int64_t now = ns_time();
      gpu_env_t * const ge = gpu_lock();
      tto_start(&ge->ttw.wait, now);
      gpu_unlock();
  }
#endif

  while (sem_wait(&wait->sem) == -1 && errno == EINTR)
    /* loop */;
}

static void vq_wait_post(vq_wait_t * const wait)
{
#if !RPI_TRACE_TIME_VPU_QPU_WAIT
  if (wait->cost != 0)
#endif
  {
    gpu_env_t *const ge = gpu_lock();
    ge->current_load -= wait->cost;
#if RPI_TRACE_TIME_VPU_QPU_WAIT
    tto_end(&ge->ttw.active, ns_time());
#endif
    gpu_unlock();
  }

  sem_post(&wait->sem);
}



// Header comments were wrong for these two
#define VPU_QPU_MASK_QPU  1
#define VPU_QPU_MASK_VPU  2

#define VPU_QPU_JOB_MAX 4
struct vpu_qpu_job_env_s
{
  unsigned int n;
  unsigned int mask;
  unsigned int cost;
  struct gpu_job_s j[VPU_QPU_JOB_MAX];
};

typedef struct vpu_qpu_job_env_s vpu_qpu_job_env_t;

vpu_qpu_job_env_t * vpu_qpu_job_new(void)
{
  vpu_qpu_job_env_t * vqj = calloc(1, sizeof(vpu_qpu_job_env_t));
  return vqj;
}

void vpu_qpu_job_delete(vpu_qpu_job_env_t * const vqj)
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

void vpu_qpu_job_add_vpu(vpu_qpu_job_env_t * const vqj, const uint32_t vpu_code,
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
void vpu_qpu_job_add_qpu(vpu_qpu_job_env_t * const vqj, const unsigned int n, const unsigned int cost, const uint32_t * const mail)
{
  if (n != 0) {
    struct gpu_job_s *const j = new_job(vqj);
    vqj->mask |= VPU_QPU_MASK_QPU;
    vqj->cost += cost;

    j->command = EXECUTE_QPU;
    j->u.q.jobs = n;
    j->u.q.noflush = QPU_FLAGS_NO_FLUSH_VPU;
    j->u.q.timeout = 5000;
    memcpy(j->u.q.control, mail, n * QPU_MAIL_EL_VALS * sizeof(uint32_t));
  }
}

// Convert callback to sem post
static void vpu_qpu_job_callback_wait(void * v)
{
  vq_wait_post(v);
}

void vpu_qpu_job_add_sync_this(vpu_qpu_job_env_t * const vqj, vpu_qpu_wait_h * const wait_h)
{
  vq_wait_t * wait;

  if (vqj->mask == 0) {
    *wait_h = NULL;
    return;
  }

  // We are going to want a sync object
  wait = vq_wait_new(vqj->cost);

  // There are 2 VPU Qs & 1 QPU Q so we can collapse sync
  // If we only posted one thing or only QPU jobs
  if (vqj->n == 1 || vqj->mask == VPU_QPU_MASK_QPU)
  {
    struct gpu_job_s * const j = vqj->j + (vqj->n - 1);
    av_assert0(j->callback.func == 0);

    j->callback.func = vpu_qpu_job_callback_wait;
    j->callback.cookie = wait;
  }
  else
  {
    struct gpu_job_s *const j = new_job(vqj);

    j->command = EXECUTE_SYNC;
    j->u.s.mask = vqj->mask;
    j->callback.func = vpu_qpu_job_callback_wait;
    j->callback.cookie = wait;
  }

  vqj->cost = 0;
  vqj->mask = 0;
  *wait_h = wait;
}

int vpu_qpu_job_start(vpu_qpu_job_env_t * const vqj)
{
  return vqj->n == 0 ? 0 : vc_gpuserv_execute_code(vqj->n, vqj->j);
}

// Simple wrapper of start + delete
int vpu_qpu_job_finish(vpu_qpu_job_env_t * const vqj)
{
  int rv;
  rv = vpu_qpu_job_start(vqj);
  vpu_qpu_job_delete(vqj);
  return rv;
}

unsigned int vpu_qpu_current_load(void)
{
  return gpu_ptr()->current_load;
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

#endif // RPI
