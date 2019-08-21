#include "config.h"
#include "libavcodec/avcodec.h"
#include "libavutil/avassert.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>

//  How to access GPIO registers from C-code on the Raspberry-Pi
//  Example program
//  15-January-2012
//  Dom and Gert

// Access from ARM Running Linux

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sched.h>
#include <time.h>

#include <unistd.h>
#include <pthread.h>
#include <interface/vcsm/user-vcsm.h>
#include <bcm_host.h>
#include "rpi_mailbox.h"
#include "rpi_ctrl_ffmpeg.h"

// argon block doesn't see VC sdram alias bits
#define MANGLE(x) ((x) &~0xc0000000)
#ifdef AXI_BUFFERS
#define AXI_MEM_SIZE (64*1024*1024)
#else
#define AXI_MEM_SIZE (64*1024*1024)
#endif

#define PAGE_SIZE (4*1024)
#define BLOCK_SIZE (0x10000)
#define CACHED 0
#define VERBOSE 0

static inline void __DMB2(void) {}//{ asm volatile ("dmb" ::: "memory"); }


// GPU memory alloc fns (internal)
typedef struct gpu_mem_ptr_s {
  unsigned char *arm; // Pointer to memory mapped on ARM side
  int vc_handle;   // Videocore handle of relocatable memory
  int vcsm_handle; // Handle for use by VCSM
  unsigned int vc;       // Address for use in GPU code
  unsigned int numbytes; // Size of memory block
} GPU_MEM_PTR_T;

typedef enum
{
    RPI_CACHE_FLUSH_MODE_INVALIDATE     = 1,
    RPI_CACHE_FLUSH_MODE_WRITEBACK      = 2,
    RPI_CACHE_FLUSH_MODE_WB_INVALIDATE  = 3
} rpi_cache_flush_mode_t;

// GPU_MEM_PTR_T alloc fns
static int gpu_malloc_cached_internal(const int mb, const int numbytes, GPU_MEM_PTR_T * const p) {
  p->numbytes = (numbytes + 255) & ~255;  // Round up
  p->vcsm_handle = vcsm_malloc_cache(p->numbytes, VCSM_CACHE_TYPE_HOST | 0x80, (char *)"Video Frame" );
  av_assert0(p->vcsm_handle);
  p->vc_handle = vcsm_vc_hdl_from_hdl(p->vcsm_handle);
  av_assert0(p->vc_handle);
  p->arm = vcsm_lock(p->vcsm_handle);
  av_assert0(p->arm);
  p->vc = mbox_mem_lock(mb, p->vc_handle);
  av_assert0(p->vc);
  printf("***** %s, %d\n", __func__, numbytes);

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
  p->vc = mbox_mem_lock(mb, p->vc_handle);
  av_assert0(p->vc);
  printf("***** %s, %d\n", __func__, numbytes);
  return 0;
}

static void gpu_free_internal(const int mb, GPU_MEM_PTR_T * const p) {
  mbox_mem_unlock(mb, p->vc_handle);
  vcsm_unlock_ptr(p->arm);
  vcsm_free(p->vcsm_handle);
  memset(p, 0, sizeof(*p));  // Ensure we crash hard if we try and use this again
  printf("***** %s\n", __func__);
}

static void gpu_clean_invalidate(GPU_MEM_PTR_T * const p, int mode) {
  struct vcsm_user_clean_invalid_s iocache = {};
  iocache.s[0].handle = p->vcsm_handle;
  iocache.s[0].cmd = mode;
  iocache.s[0].addr = (int) p->arm;
  iocache.s[0].size  = p->numbytes;
  vcsm_clean_invalid( &iocache );
  printf("***** %s mode:%d\n", __func__, mode);
}

//
// Set up a memory regions to access periperhals
//
static void *setup_io(const char *dev, unsigned long base)
{
   void *gpio_map;
   int  mem_fd;

   /* open /dev/mem */
   if ((mem_fd = open(dev, O_RDWR|O_SYNC) ) < 0) {
       av_log(NULL, AV_LOG_WARNING, "can't open %s\n", dev);
       return NULL;
   }

   // Now map it
   gpio_map = (unsigned char *)mmap(
      NULL,
      BLOCK_SIZE,
      PROT_READ|PROT_WRITE,
      MAP_SHARED,
      mem_fd,
      base
   );

   if (gpio_map == MAP_FAILED) {
       av_log(NULL, AV_LOG_WARNING, "GPIO mapping failed");
       close(mem_fd);
       return NULL;
   }

   return gpio_map;
} // setup_io

static void release_io(void *gpio_map)
{
   int s = munmap(gpio_map, BLOCK_SIZE);
   av_assert0(s == 0);
}

struct RPI_DEBUG {
    FILE *fp_reg;
    int mbox;
    GPU_MEM_PTR_T axi;
    void *read_buf;
    int32_t read_buf_size, read_buf_used;
    volatile unsigned int *apb;
    volatile unsigned int *interrupt;
    //volatile unsigned int *sdram;
};

//////////////////////////////////////////////////////////////////////////////

void rpi_apb_write_addr(void *id, uint16_t addr, uint32_t data) {
    struct RPI_DEBUG *rpi = (struct RPI_DEBUG *) id;
    if (VERBOSE)
    fprintf(rpi->fp_reg, "P %x %08x\n", addr, data);
    __DMB2();
    rpi->apb[addr>>2] = data + (MANGLE(rpi->axi.vc)>>6);
}

uint64_t rpi_axi_get_addr(void *id) {
    struct RPI_DEBUG *rpi = (struct RPI_DEBUG *) id;
    return (uint64_t)MANGLE(rpi->axi.vc);
}

void rpi_apb_write(void *id, uint16_t addr, uint32_t data) {
    struct RPI_DEBUG *rpi = (struct RPI_DEBUG *) id;
    if (VERBOSE)
    fprintf(rpi->fp_reg, "W %x %08x\n", addr, data);
    __DMB2();
    rpi->apb[addr>>2] = data;
}

uint32_t rpi_apb_read(void *id, uint16_t addr) {
    struct RPI_DEBUG *rpi = (struct RPI_DEBUG *) id;
    uint32_t v = rpi->apb[addr>>2];
    __DMB2();
    if (VERBOSE)
    fprintf(rpi->fp_reg, "R %x (=%x)\n", addr, v);
    return v;
}

void rpi_apb_read_drop(void *id, uint16_t addr) {
    struct RPI_DEBUG *rpi = (struct RPI_DEBUG *) id;
    uint32_t v = rpi->apb[addr>>2];
    __DMB2();
    if (VERBOSE)
    fprintf(rpi->fp_reg, "R %x (=%x)\n", addr, v);
}

void rpi_axi_write(void *id, uint64_t addr, uint32_t size, void *buf) {
    struct RPI_DEBUG *rpi = (struct RPI_DEBUG *) id;
    if (VERBOSE)
    fprintf(rpi->fp_reg, "L %08" PRIx64 " %08x\n", addr, size);
    av_assert0(addr + size <= AXI_MEM_SIZE);
    __DMB2();
    memcpy(rpi->axi.arm + addr, buf, size);
}

void rpi_axi_read_alloc(void *id, uint32_t size) {
    struct RPI_DEBUG *rpi = (struct RPI_DEBUG *) id;
    av_assert0(rpi->read_buf == NULL);
    rpi->read_buf = malloc(size);
    rpi->read_buf_size = size;
    rpi->read_buf_used = 0;
}

void rpi_axi_read_tx(void *id, uint64_t addr, uint32_t size) {
    struct RPI_DEBUG *rpi = (struct RPI_DEBUG *) id;
    av_assert0(rpi->read_buf_used + size <= rpi->read_buf_size);
    if (VERBOSE)
    fprintf(rpi->fp_reg, "S %08" PRIx64 " %08x\n", addr, size);
    av_assert0(addr + size <= AXI_MEM_SIZE);
    __DMB2();
    memcpy((char *)rpi->read_buf + rpi->read_buf_used, rpi->axi.arm + addr, size);
    rpi->read_buf_used += size;
}

void rpi_axi_read_rx(void *id, uint32_t size, void *buf) {
    struct RPI_DEBUG *rpi = (struct RPI_DEBUG *) id;
    av_assert0(size == rpi->read_buf_used);
    fprintf(rpi->fp_reg, "Z " PRIx64 " %08x\n", size);
    memcpy(buf, rpi->read_buf, size);
    free(rpi->read_buf);
    rpi->read_buf = NULL;
    rpi->read_buf_size = 0;
    rpi->read_buf_used = 0;
}

static int getthreadnum(unsigned pid)
{
   static unsigned pids[8];
   int i;
   for (i = 0; i < 8; i++)
   {
      if (pids[i] == 0)
         pids[i] = pid;
      if (pids[i] == pid)
         return i;
   }
   return -1;
}

#define _NOP() //do { __asm__ __volatile__ ("nop"); } while (0)

static void yield(void)
{
  int i;
  for (i=0; i<0; i++)
    _NOP();
  usleep(1000);
}


void rpi_wait_interrupt(void *id, int phase) {
    struct RPI_DEBUG *rpi = (struct RPI_DEBUG *) id;
    static struct timespec tfirst={0,0};
    static __thread struct timespec tstart={0,0};
    struct timespec tend={0,0};
    unsigned pid = (unsigned)pthread_self();
    clock_gettime(CLOCK_MONOTONIC, &tend);
    if (tstart.tv_sec == 0 && tstart.tv_nsec == 0)
       tstart = tend;
    if (tfirst.tv_sec == 0 && tfirst.tv_nsec == 0)
    {
       /*printf("%s:  Resetting sdram stats\n", __FUNCTION__);
       rpi->sdram[0x30/4] = 0;*/
       tfirst = tend;
    }
    if (VERBOSE)
    printf("%08llu: %s:  IN thread:%u phase:%d time:%llu\n", ((tend.tv_sec * 1000000000ULL + tend.tv_nsec) - (tfirst.tv_sec * 1000000000ULL + tfirst.tv_nsec))/1000, 
       __FUNCTION__, getthreadnum(pid), phase, ((tend.tv_sec * 1000000000ULL + tend.tv_nsec) - (tstart.tv_sec * 1000000000ULL + tstart.tv_nsec))/1000);
    /*enum {IDL=0x30/4, RTC=0x34/4, WTC=0x38/4, RDC=0x3c/4, WDC=0x40/4, RAC=0x44/4, CYC=0x48/4, CMD=0x4c/4, DAT=0x50/4, RDCMD=0x78/4, RDSUB=0x7c/4, WRCMD=0x80/4, WRSUB=0x84/4, MWRCMD=0x88/4, MWRSUB=0x8c/4,};
    printf("IDL:%u RTC:%u WTC:%u RDC:%u WDC:%u RAC:%u CYC:%u CMD:%u DAT:%u RDCMD:%u RDSUB:%u WRCMD:%u WRSUB:%u MWRCMD:%u MWRSUB:%u\n", 
      rpi->sdram[IDL], rpi->sdram[RTC], rpi->sdram[WTC], rpi->sdram[RDC], rpi->sdram[WDC], rpi->sdram[RAC], rpi->sdram[CYC], rpi->sdram[CMD], rpi->sdram[DAT],
      rpi->sdram[RDCMD], rpi->sdram[RDSUB], rpi->sdram[WRCMD], rpi->sdram[WRSUB], rpi->sdram[MWRCMD], rpi->sdram[MWRSUB]);
    rpi->sdram[0x30/4] = 0;*/

    if (VERBOSE)
    fprintf(rpi->fp_reg, "I %d\n", phase);
    __DMB2();
#if 0
    av_assert0(phase == 1 || phase == 2);
    for (;;) {
        if      (phase==1 && rpi->apb[0x74>>2]==rpi->apb[0x70>>2]) break;
        else if (phase==2 && (rpi->apb[0x8028/*STATUS2*/>>2]&1)==0) break;
    }
    fprintf(rpi->fp_reg, "I %d done\n", phase);
#else
    #define ARG_IC_ICTRL_ACTIVE1_INT_SET                   0x00000001
    #define ARG_IC_ICTRL_ACTIVE1_EDGE_SET                  0x00000002
    #define ARG_IC_ICTRL_ACTIVE1_EN_SET                    0x00000004
    #define ARG_IC_ICTRL_ACTIVE1_STATUS_SET                0x00000008
    #define ARG_IC_ICTRL_ACTIVE2_INT_SET                   0x00000010
    #define ARG_IC_ICTRL_ACTIVE2_EDGE_SET                  0x00000020
    #define ARG_IC_ICTRL_ACTIVE2_EN_SET                    0x00000040
    #define ARG_IC_ICTRL_ACTIVE2_STATUS_SET                0x00000080
    //if (rpi->interrupt[0] &~ (ARG_IC_ICTRL_ACTIVE1_INT_SET|ARG_IC_ICTRL_ACTIVE2_INT_SET|ARG_IC_ICTRL_ACTIVE1_EDGE_SET|ARG_IC_ICTRL_ACTIVE2_EDGE_SET|ARG_IC_ICTRL_ACTIVE1_STATUS_SET|ARG_IC_ICTRL_ACTIVE2_STATUS_SET))
    //fprintf(rpi->fp_reg, "I %d %x in\n", phase, rpi->interrupt[0]);

    if (phase == 1) {
      while (!(rpi->interrupt[0] & ARG_IC_ICTRL_ACTIVE1_INT_SET))
        yield();
      rpi->interrupt[0] = rpi->interrupt[0] &~ ARG_IC_ICTRL_ACTIVE2_INT_SET; //ARG_IC_ICTRL_ACTIVE1_INT_SET|ARG_IC_ICTRL_ACTIVE2_EDGE_SET|ARG_IC_ICTRL_ACTIVE2_EDGE_SET;
    } else if (phase == 2) {
      while (!(rpi->interrupt[0] & ARG_IC_ICTRL_ACTIVE2_INT_SET))
        yield();
      rpi->interrupt[0] = rpi->interrupt[0] &~ ARG_IC_ICTRL_ACTIVE1_INT_SET; //ARG_IC_ICTRL_ACTIVE2_INT_SET|ARG_IC_ICTRL_ACTIVE1_EDGE_SET|ARG_IC_ICTRL_ACTIVE2_EDGE_SET;
    } else av_assert0(0);
#endif
    //fprintf(rpi->fp_reg, "I %d %x out\n", phase, rpi->interrupt[0]);
    if (phase == 2)
    {
      __DMB2();
      if (VERBOSE)
      fprintf(rpi->fp_reg, "YBASE:%08x CBASE:%08x\n", rpi->apb[0x8018>>2]*64, rpi->apb[0x8020>>2]*64);
    }
    clock_gettime(CLOCK_MONOTONIC, &tend);

    if (VERBOSE)
    printf("%08llu: %s: OUT thread:%u phase:%d time:%llu\n", ((tend.tv_sec * 1000000000ULL + tend.tv_nsec) - (tfirst.tv_sec * 1000000000ULL + tfirst.tv_nsec))/1000, 
       __FUNCTION__, getthreadnum(pid), phase, ((tend.tv_sec * 1000000000ULL + tend.tv_nsec) - (tstart.tv_sec * 1000000000ULL + tstart.tv_nsec))/1000);
    /*printf("IDL:%u RTC:%u WTC:%u RDC:%u WDC:%u RAC:%u CYC:%u CMD:%u DAT:%u RDCMD:%u RDSUB:%u WRCMD:%u WRSUB:%u MWRCMD:%u MWRSUB:%u\n", 
      rpi->sdram[IDL], rpi->sdram[RTC], rpi->sdram[WTC], rpi->sdram[RDC], rpi->sdram[WDC], rpi->sdram[RAC], rpi->sdram[CYC], rpi->sdram[CMD], rpi->sdram[DAT],
      rpi->sdram[RDCMD], rpi->sdram[RDSUB], rpi->sdram[WRCMD], rpi->sdram[WRSUB], rpi->sdram[MWRCMD], rpi->sdram[MWRSUB]);*/

    tstart = tend;
}


void rpi_apb_dump_regs(void *id, uint16_t addr, int num) {
    struct RPI_DEBUG *rpi = (struct RPI_DEBUG *) id;
    int i;
    __DMB2();
    if (VERBOSE)
    for (i=0; i<num; i++)
    {
      if ((i%4)==0)
        fprintf(rpi->fp_reg, "%08x: ", 0x7eb00000 + addr + 4*i);
      fprintf(rpi->fp_reg, "%08x", rpi->apb[(addr>>2)+i]);
      if ((i%4)==3 || i+1 == num)
        fprintf(rpi->fp_reg, "\n");
      else
        fprintf(rpi->fp_reg, " ");
    }
}

void rpi_axi_dump(void *id, uint64_t addr, uint32_t size) {
    struct RPI_DEBUG *rpi = (struct RPI_DEBUG *) id;
    int i;
    __DMB2();
    if (VERBOSE)
    for (i=0; i<size>>2; i++)
    {
      if ((i%4)==0)
        fprintf(rpi->fp_reg, "%08x: ", MANGLE(rpi->axi.vc) + (uint32_t)addr + 4*i);
      fprintf(rpi->fp_reg, "%08x", ((uint32_t*)rpi->axi.arm)[(addr>>2)+i]);
      if ((i%4)==3 || i+1 == size>>2)
        fprintf(rpi->fp_reg, "\n");
      else
        fprintf(rpi->fp_reg, " ");
    }
}

void rpi_axi_flush(void *id, int mode) {
    struct RPI_DEBUG *rpi = (struct RPI_DEBUG *) id;
    if (CACHED)
    {
        gpu_clean_invalidate(&rpi->axi, mode);
    }
}

//////////////////////////////////////////////////////////////////////////////

const char * rpi_ctrl_ffmpeg_init(const char *hwaccel_device, void **id) {
    struct RPI_DEBUG *rpi = calloc(1, sizeof(struct RPI_DEBUG));
    (void) hwaccel_device;
    const char * rv = NULL;

    printf("%s\n id=%p\n", __FUNCTION__, rpi);

    if (!rpi)
        return "out of memory";

    rpi->mbox = -1;

    bcm_host_init();
    vcsm_init();

    rpi->fp_reg = stderr;

    if ((rpi->apb = setup_io("/dev/argon-hevcmem", 0)) == NULL)
    {
        rv = "Failed to open apb";
        goto fail;
    }

    if ((rpi->interrupt = setup_io("/dev/argon-intcmem", 0)) == NULL)
    {
        rv = "Failed to open interrupt";
        goto fail;
    }

    if ((rpi->mbox = mbox_open()) < 0)
    {
      rv = "Failed to open mbox";
      goto fail;
    }

    if ((CACHED ? gpu_malloc_cached_internal:gpu_malloc_uncached_internal)(rpi->mbox, AXI_MEM_SIZE, &rpi->axi) != 0)
    {
        rv = "out of memory";
        goto fail;
    }

    fprintf(rpi->fp_reg, "A 100000000 apb:%p axi.arm:%p axi.vc:%08x\n", rpi->apb, rpi->axi.arm, MANGLE(rpi->axi.vc));
    *id = rpi;
    return 0;

fail:
    rpi_ctrl_ffmpeg_free(rpi);
    return rv;
}

void rpi_ctrl_ffmpeg_free(void *id) {
    struct RPI_DEBUG *rpi = (struct RPI_DEBUG *) id;
    if (rpi->mbox >= 0)
    {
        gpu_free_internal(rpi->mbox, &rpi->axi);
        mbox_close(rpi->mbox);
    }
    if (rpi->interrupt != NULL)
        release_io((void *)rpi->interrupt);
    if (rpi->apb != NULL)
        release_io((void *)rpi->apb);
    vcsm_exit();
    bcm_host_deinit();
}
