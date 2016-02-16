#ifndef rpi_shader_H
#define rpi_shader_H

extern unsigned int rpi_shader[];

#define mc_setup_uv (rpi_shader + 0)
#define mc_filter_uv (rpi_shader + 138)
#define mc_filter_uv_b0 (rpi_shader + 330)
#define mc_filter_uv_b (rpi_shader + 492)
#define mc_exit (rpi_shader + 678)
#define mc_interrupt_exit8 (rpi_shader + 696)
#define mc_setup (rpi_shader + 726)
#define mc_filter (rpi_shader + 1110)
#define mc_filter_b (rpi_shader + 1240)
#define mc_interrupt_exit12 (rpi_shader + 1372)
#define mc_exit1 (rpi_shader + 1410)
#define mc_end (rpi_shader + 1426)

#endif
