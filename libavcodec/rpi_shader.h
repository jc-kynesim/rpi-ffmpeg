#ifndef rpi_shader_H
#define rpi_shader_H

extern unsigned int rpi_shader[];

#define mc_setup_uv (rpi_shader + 0)
#define mc_filter_uv (rpi_shader + 136)
#define mc_filter_uv_b0 (rpi_shader + 328)
#define mc_filter_uv_b (rpi_shader + 488)
#define mc_exit (rpi_shader + 672)
#define mc_interrupt_exit8 (rpi_shader + 690)
#define mc_setup (rpi_shader + 720)
#define mc_filter (rpi_shader + 1104)
#define mc_filter_b (rpi_shader + 1234)
#define mc_interrupt_exit12 (rpi_shader + 1366)
#define mc_exit1 (rpi_shader + 1404)
#define mc_end (rpi_shader + 1420)

#endif
