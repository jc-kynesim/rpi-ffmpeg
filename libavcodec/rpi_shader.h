#ifndef rpi_shader_H
#define rpi_shader_H

extern unsigned int rpi_shader[];

#define mc_setup_c (rpi_shader + 0)
#define mc_filter_uv (rpi_shader + 194)
#define mc_filter_uv_b0 (rpi_shader + 332)
#define mc_filter_uv_b (rpi_shader + 512)
#define mc_exit (rpi_shader + 654)
#define mc_exit_c (rpi_shader + 654)
#define mc_setup (rpi_shader + 670)
#define mc_filter (rpi_shader + 954)
#define mc_filter_b (rpi_shader + 1074)
#define mc_interrupt_exit8c (rpi_shader + 1194)
#define mc_interrupt_exit12 (rpi_shader + 1222)
#define mc_exit1 (rpi_shader + 1258)
#define mc_end (rpi_shader + 1274)

#endif
