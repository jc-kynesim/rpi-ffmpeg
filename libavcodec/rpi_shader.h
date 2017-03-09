#ifndef rpi_shader_H
#define rpi_shader_H

extern unsigned int rpi_shader[];

#define mc_setup_uv (rpi_shader + 0)
#define mc_filter_uv (rpi_shader + 140)
#define mc_filter_uv_b0 (rpi_shader + 282)
#define mc_filter_uv_b (rpi_shader + 454)
#define mc_exit (rpi_shader + 610)
#define mc_interrupt_exit8 (rpi_shader + 628)
#define mc_setup (rpi_shader + 658)
#define mc_filter (rpi_shader + 948)
#define mc_filter_b (rpi_shader + 1068)
#define mc_interrupt_exit12 (rpi_shader + 1188)
#define mc_exit1 (rpi_shader + 1226)
#define mc_end (rpi_shader + 1242)

#endif
