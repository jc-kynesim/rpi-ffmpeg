#ifndef rpi_shader_H
#define rpi_shader_H

extern unsigned int rpi_shader[];

#define mc_setup_uv (rpi_shader + 0)
#define mc_filter_uv (rpi_shader + 132)
#define mc_filter_uv_b0 (rpi_shader + 274)
#define mc_filter_uv_b (rpi_shader + 392)
#define mc_exit (rpi_shader + 540)
#define mc_interrupt_exit8 (rpi_shader + 558)
#define mc_setup (rpi_shader + 588)
#define mc_filter (rpi_shader + 872)
#define mc_filter_b (rpi_shader + 992)
#define mc_interrupt_exit12 (rpi_shader + 1114)
#define mc_exit1 (rpi_shader + 1152)
#define mc_end (rpi_shader + 1168)

#endif
