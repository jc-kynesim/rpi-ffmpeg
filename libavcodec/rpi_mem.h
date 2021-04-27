/*
Copyright (c) 2018 Raspberry Pi (Trading) Ltd.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Authors: John Cox, Ben Avison
*/

#ifndef RPI_MEM_H
#define RPI_MEM_H

typedef struct gpu_mem_ptr_s {
  unsigned char *arm; // Pointer to memory mapped on ARM side
  int vc_handle;   // Videocore handle of relocatable memory
  int vcsm_handle; // Handle for use by VCSM
  int vc;       // Address for use in GPU code
  int numbytes; // Size of memory block
} GPU_MEM_PTR_T;

// General GPU functions

#define GPU_INIT_GPU 1
#define GPU_INIT_CMA 2

extern int gpu_malloc_cached(int numbytes, GPU_MEM_PTR_T *p);
extern int gpu_malloc_uncached(int numbytes, GPU_MEM_PTR_T *p);
extern void gpu_free(GPU_MEM_PTR_T * const p);
int rpi_mem_gpu_init(const unsigned int flags);
void rpi_mem_gpu_uninit(void);

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

struct AVFrame;
void rpi_cache_flush_add_gm_ptr(rpi_cache_flush_env_t * const rfe, const GPU_MEM_PTR_T * const gm, const rpi_cache_flush_mode_t mode);
void rpi_cache_flush_add_gm_range(rpi_cache_flush_env_t * const rfe, const GPU_MEM_PTR_T * const gm, const rpi_cache_flush_mode_t mode,
  const unsigned int offset, const unsigned int size);
void rpi_cache_flush_add_gm_blocks(rpi_cache_flush_env_t * const rfe, const GPU_MEM_PTR_T * const gm, const unsigned int mode,
  const unsigned int offset0, const unsigned int block_size, const unsigned int blocks, const unsigned int block_stride);
void rpi_cache_flush_add_frame(rpi_cache_flush_env_t * const rfe, const struct AVFrame * const frame, const rpi_cache_flush_mode_t mode);
void rpi_cache_flush_add_frame_block(rpi_cache_flush_env_t * const rfe, const struct AVFrame * const frame, const rpi_cache_flush_mode_t mode,
  const unsigned int x0, const unsigned int y0, const unsigned int width, const unsigned int height,
  const unsigned int uv_shift, const int do_luma, const int do_chroma);

// init, add, finish for one gm ptr
void rpi_cache_flush_one_gm_ptr(const GPU_MEM_PTR_T * const p, const rpi_cache_flush_mode_t mode);

#endif
