#ifndef rpi_shader_H
#define rpi_shader_H

extern unsigned int rpi_shader[];

#define mc_setup_c (rpi_shader + 0)
#define mc_filter_uv (rpi_shader + 156)
#define mc_filter_uv_b0 (rpi_shader + 294)
#define mc_interrupt_exit8c (rpi_shader + 588)
#define mc_exit (rpi_shader + 616)
#define mc_exit_c (rpi_shader + 616)
#define mc_interrupt_exit12 (rpi_shader + 632)
#define mc_setup (rpi_shader + 668)
#define mc_filter (rpi_shader + 946)
#define mc_filter_b (rpi_shader + 1086)
#define mc_end (rpi_shader + 1226)

#endif
