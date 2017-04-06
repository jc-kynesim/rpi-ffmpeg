#ifndef rpi_shader_H
#define rpi_shader_H

extern unsigned int rpi_shader[];

#define mc_setup_uv (rpi_shader + 0)
#define mc_filter_uv (rpi_shader + 130)
#define mc_filter_uv_b0 (rpi_shader + 292)
#define mc_filter_uv_b (rpi_shader + 468)
#define mc_exit_c (rpi_shader + 614)
#define mc_exit (rpi_shader + 644)
#define mc_setup (rpi_shader + 660)
#define mc_filter (rpi_shader + 930)
#define mc_filter_b (rpi_shader + 1050)
#define mc_interrupt_exit12c (rpi_shader + 1170)
#define mc_interrupt_exit12 (rpi_shader + 1220)
#define mc_exit1 (rpi_shader + 1256)
#define mc_end (rpi_shader + 1272)

#endif
