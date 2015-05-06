#ifndef RPI_QPU_H
#define RPI_QPU_H

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
extern void gpu_cache_flush(GPU_MEM_PTR_T *p);

// QPU specific functions
extern void qpu_run_shader12(int code, int num, int code2, int num2, int unifs1, int unifs2, int unifs3, int unifs4, int unifs5, int unifs6, int unifs7, int unifs8, int unifs9, int unifs10, int unifs11, int unifs12);

enum {
  QPU_MC_SETUP,
  QPU_MC_FILTER,
  QPU_MC_EXIT,
  QPU_MC_INTERRUPT_EXIT,
  QPU_MC_FILTER_B,
  QPU_MC_FILTER_HONLY,
  QPU_MC_SETUP_UV,
  QPU_MC_FILTER_UV,
  QPU_MC_FILTER_UV_B,
  QPU_MC_END
  };
extern unsigned int qpu_get_fn(int num);

// VPU specific functions
extern unsigned int vpu_get_fn(void);
extern unsigned int vpu_get_constants(void);
extern unsigned vpu_execute_code( unsigned code, unsigned r0, unsigned r1, unsigned r2, unsigned r3, unsigned r4, unsigned r5);
extern int vpu_post_code( unsigned code, unsigned r0, unsigned r1, unsigned r2, unsigned r3, unsigned r4, unsigned r5, GPU_MEM_PTR_T *buf);
extern void vpu_wait( int id);

// Simple test of shader code
extern int rpi_test_shader(void);

extern void rpi_do_block(const unsigned char *in_buffer_vc, int src_pitch, unsigned char *dst_vc, int dst_pitch, unsigned char *dst);
extern void rpi_do_block_arm(const unsigned char *in_buffer, int src_pitch, unsigned char *dst, int dst_pitch);

#endif
