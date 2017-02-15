#ifndef rpi_shader_H
#define rpi_shader_H

extern unsigned int rpi_shader[];

#define mc_setup_uv (rpi_shader + 0)
#define mc_filter_uv (rpi_shader + 130)
#define mc_filter_uv_b0 (rpi_shader + 310)
#define mc_filter_uv_b (rpi_shader + 464)
#define mc_exit (rpi_shader + 662)
#define mc_interrupt_exit8 (rpi_shader + 680)
#define mc_setup (rpi_shader + 710)
#define mc_filter (rpi_shader + 1062)
#define mc_filter_b (rpi_shader + 1188)
#define mc_interrupt_exit12 (rpi_shader + 1320)
#define mc_exit1 (rpi_shader + 1358)
#define mc_end (rpi_shader + 1374)

#endif
