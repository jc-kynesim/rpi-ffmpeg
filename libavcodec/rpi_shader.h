#ifndef rpi_shader_H
#define rpi_shader_H

extern unsigned int rpi_shader[];

#define mc_setup_uv (rpi_shader + 0)
#define mc_filter_uv (rpi_shader + 144)
#define mc_filter_uv_b0 (rpi_shader + 334)
#define mc_filter_uv_b (rpi_shader + 486)
#define mc_exit (rpi_shader + 662)
#define mc_interrupt_exit8 (rpi_shader + 680)
#define mc_setup (rpi_shader + 710)
#define mc_filter (rpi_shader + 864)
#define mc_filter_b (rpi_shader + 1104)
#define mc_interrupt_exit12 (rpi_shader + 1360)
#define mc_end (rpi_shader + 1398)

#endif
