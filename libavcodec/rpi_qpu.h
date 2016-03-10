#ifndef RPI_QPU_H
#define RPI_QPU_H

// Define RPI_FAST_CACHEFLUSH to use the VCSM cache flush code
#define RPI_FAST_CACHEFLUSH

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
extern void gpu_cache_flush3(GPU_MEM_PTR_T *p0,GPU_MEM_PTR_T *p1,GPU_MEM_PTR_T *p2);

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
