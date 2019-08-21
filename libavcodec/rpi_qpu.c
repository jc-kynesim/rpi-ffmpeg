#if 1//defined(RPI) || defined (RPI_DISPLAY)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include "libavutil/avassert.h"

#include "config.h"

#include <pthread.h>
#include <time.h>

#include <interface/vcsm/user-vcsm.h>

#include "rpi_mailbox.h"
#include "rpi_qpu.h"

#pragma GCC diagnostic push
// Many many redundant decls in the header files
#pragma GCC diagnostic ignored "-Wredundant-decls"
#include "interface/vmcs_host/vc_vchi_gpuserv.h"
#pragma GCC diagnostic pop

// QPU "noflush" flags
// a mixture of flushing & profiling

#define QPU_FLAGS_NO_FLUSH_VPU          1       // If unset VPU cache will be flushed
#define QPU_FLAGS_PROF_CLEAR_AND_ENABLE 2       // Clear & Enable detailed QPU profiling registers
#define QPU_FLAGS_PROF_OUTPUT_COUNTS    4       // Print the results
#define QPU_FLAGS_OUTPUT_QPU_TIMES      8       // Print QPU times - independant of the profiling
#define QPU_FLAGS_NO_FLUSH_QPU          16      // If unset flush QPU caches & TMUs (uniforms always flushed)

#define vcos_verify_ge0(x) ((x)>=0)

struct rpi_cache_flush_env_s {
//    unsigned int n;
//    struct vcsm_user_clean_invalid_s a[CFE_A_COUNT];
  struct vcsm_user_clean_invalid2_s v;
};

typedef struct gpu_env_s
{
  int open_count;
  int init_count;
  int mb;
  int vpu_i_cache_flushed;
} gpu_env_t;

// Stop more than one thread trying to allocate memory or use the processing resources at once
static pthread_mutex_t gpu_mutex = PTHREAD_MUTEX_INITIALIZER;
static gpu_env_t * gpu = NULL;


// GPU memory alloc fns (internal)

// GPU_MEM_PTR_T alloc fns
static int gpu_malloc_cached_internal(const int mb, const int numbytes, GPU_MEM_PTR_T * const p) {
  p->numbytes = (numbytes + 255) & ~255;  // Round up
  p->vcsm_handle = vcsm_malloc_cache(p->numbytes, VCSM_CACHE_TYPE_HOST | 0x80, (char *)"Video Frame" );
  //p->vcsm_handle = vcsm_malloc_cache(numbytes, VCSM_CACHE_TYPE_VC, (char *)"Video Frame" );
  //p->vcsm_handle = vcsm_malloc_cache(numbytes, VCSM_CACHE_TYPE_NONE, (char *)"Video Frame" );
  //p->vcsm_handle = vcsm_malloc_cache(numbytes, VCSM_CACHE_TYPE_HOST_AND_VC, (char *)"Video Frame" );
  av_assert0(p->vcsm_handle);
  p->vc_handle = vcsm_vc_hdl_from_hdl(p->vcsm_handle);
  av_assert0(p->vc_handle);
  p->arm = vcsm_lock(p->vcsm_handle);
  av_assert0(p->arm);
  p->vc = vcsm_vc_addr_from_hdl(p->vcsm_handle);
  av_assert0(p->vc);
//  printf("***** %s, %d\n", __func__, numbytes);

  return 0;
}

static int gpu_malloc_uncached_internal(const int mb, const int numbytes, GPU_MEM_PTR_T * const p) {
  p->numbytes = numbytes;
  p->vcsm_handle = vcsm_malloc_cache(numbytes, VCSM_CACHE_TYPE_NONE | 0x80, (char *)"Video Frame" );
  av_assert0(p->vcsm_handle);
  p->vc_handle = vcsm_vc_hdl_from_hdl(p->vcsm_handle);
  av_assert0(p->vc_handle);
  p->arm = vcsm_lock(p->vcsm_handle);
  av_assert0(p->arm);
  p->vc = vcsm_vc_addr_from_hdl(p->vcsm_handle);
  av_assert0(p->vc);
//  printf("***** %s, %d\n", __func__, numbytes);
  return 0;
}

static void gpu_free_internal(const int mb, GPU_MEM_PTR_T * const p) {
//  mbox_mem_unlock(mb, p->vc_handle);
  vcsm_unlock_ptr(p->arm);
  vcsm_free(p->vcsm_handle);
  memset(p, 0, sizeof(*p));  // Ensure we crash hard if we try and use this again
//  printf("***** %s\n", __func__);
}


// GPU init, free, lock, unlock

static void gpu_term(void)
{
  gpu_env_t * const ge = gpu;

  // We have to hope that eveything has terminated...
  gpu = NULL;

  vc_gpuserv_deinit();

  vcsm_exit();

  mbox_close(ge->mb);

  free(ge);
}


// Connect to QPU, returns 0 on success.
static int gpu_init(gpu_env_t ** const gpu) {
  gpu_env_t * const ge = calloc(1, sizeof(gpu_env_t));
  *gpu = NULL;

  if (ge == NULL)
    return -1;

  if ((ge->mb = mbox_open()) < 0)
    return -1;

  vcsm_init();

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

int gpu_get_mailbox(void)
{
  av_assert0(gpu);
  return gpu->mb;
}

void gpu_ref(void)
{
  gpu_lock_ref();
  gpu_unlock();
}

void gpu_unref(void)
{
  gpu_env_t * const ge = gpu_lock();
  gpu_unlock_unref(ge);
}

// ----------------------------------------------------------------------------
//
// Cache flush functions

#define CACHE_EL_MAX 16

rpi_cache_flush_env_t * rpi_cache_flush_init()
{
  rpi_cache_flush_env_t * const rfe = malloc(sizeof(rpi_cache_flush_env_t) +
            sizeof(struct vcsm_user_clean_invalid2_block_s) * CACHE_EL_MAX);
  if (rfe == NULL)
    return NULL;

  rfe->v.op_count = 0;
  return rfe;
}

void rpi_cache_flush_abort(rpi_cache_flush_env_t * const rfe)
{
  if (rfe != NULL)
    free(rfe);
}

int rpi_cache_flush_finish(rpi_cache_flush_env_t * const rfe)
{
  int rc = 0;

  if (vcsm_clean_invalid2(&rfe->v) != 0)
    rc = -1;

  free(rfe);

  if (rc == 0)
    return 0;

  av_log(NULL, AV_LOG_ERROR, "vcsm_clean_invalid failed: errno=%d\n", errno);
  return rc;
}

inline void rpi_cache_flush_add_gm_blocks(rpi_cache_flush_env_t * const rfe, const GPU_MEM_PTR_T * const gm, const unsigned int mode,
  const unsigned int offset0, const unsigned int block_size, const unsigned int blocks, const unsigned int block_stride)
{
  struct vcsm_user_clean_invalid2_block_s * const b = rfe->v.s + rfe->v.op_count++;

  av_assert0(rfe->v.op_count <= CACHE_EL_MAX);

  b->invalidate_mode = mode;
  b->block_count = blocks;
  b->start_address = gm->arm + offset0;
  b->block_size = block_size;
  b->inter_block_stride = block_stride;
}

void rpi_cache_flush_add_gm_range(rpi_cache_flush_env_t * const rfe, const GPU_MEM_PTR_T * const gm, const unsigned int mode,
  const unsigned int offset, const unsigned int size)
{
  // Deal with empty pointer trivially
  if (gm == NULL || size == 0)
    return;

  av_assert0(offset <= gm->numbytes);
  av_assert0(size <= gm->numbytes);
  av_assert0(offset + size <= gm->numbytes);

  rpi_cache_flush_add_gm_blocks(rfe, gm, mode, offset, size, 1, 0);
}

void rpi_cache_flush_add_gm_ptr(rpi_cache_flush_env_t * const rfe, const GPU_MEM_PTR_T * const gm, const unsigned int mode)
{
  rpi_cache_flush_add_gm_blocks(rfe, gm, mode, 0, gm->numbytes, 1, 0);
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

// Call this to clean and invalidate a region of memory
void rpi_cache_flush_one_gm_ptr(const GPU_MEM_PTR_T *const p, const rpi_cache_flush_mode_t mode)
{
  rpi_cache_flush_env_t * rfe = rpi_cache_flush_init();
  rpi_cache_flush_add_gm_ptr(rfe, p, mode);
  rpi_cache_flush_finish(rfe);
}


// ----------------------------------------------------------------------------

#endif // RPI
