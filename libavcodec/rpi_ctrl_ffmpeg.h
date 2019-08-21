// rpi_ctrl_ffmpeg.h
//
// This file contains prototypes for the functions used to control the socket
// interface when using ffmpeg.
//

#ifndef __CTRL_FFMPEG_H__
#define __CTRL_FFMPEG_H__

#include <stdint.h>

const char *rpi_ctrl_ffmpeg_init  (const char *hwaccel_device, void **id);
void      rpi_apb_write_addr    (void *id, uint16_t addr, uint32_t data);
void      rpi_apb_write         (void *id, uint16_t addr, uint32_t data);
uint32_t  rpi_apb_read          (void *id, uint16_t addr);
void      rpi_apb_read_drop     (void *id, uint16_t addr);
void      rpi_axi_write         (void *id, uint64_t addr, uint32_t size, void *buf);
void      rpi_axi_read          (void *id, uint64_t addr, uint32_t size, void *buf);
void      rpi_axi_read_alloc    (void *id, uint32_t size);
void      rpi_axi_read_tx       (void *id, uint64_t addr, uint32_t size);
void      rpi_axi_read_rx       (void *id, uint32_t size, void *buf);
void      rpi_wait_interrupt    (void *id, int phase);
void      rpi_ctrl_ffmpeg_free  (void *id);
uint64_t  rpi_axi_get_addr      (void *id);
void rpi_apb_dump_regs(void *id, uint16_t addr, int num);
void rpi_axi_dump(void *id, uint64_t addr, uint32_t size);
void rpi_axi_flush(void *id, int mode);

#endif // __CTRL_FILES_H__
