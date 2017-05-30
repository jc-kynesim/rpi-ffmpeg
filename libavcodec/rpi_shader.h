#ifndef rpi_shader_H
#define rpi_shader_H

extern unsigned int rpi_shader[];

#define mc_setup_c (rpi_shader + 0)
#define mc_filter_uv (rpi_shader + 142)
#define mc_filter_uv_b0 (rpi_shader + 276)
#define mc_exit (rpi_shader + 562)
#define mc_exit_c (rpi_shader + 562)
#define mc_interrupt_exit12 (rpi_shader + 578)
#define mc_interrupt_exit12c (rpi_shader + 578)
#define mc_setup (rpi_shader + 614)
#define mc_filter (rpi_shader + 856)
#define mc_filter_b (rpi_shader + 996)
#define mc_end (rpi_shader + 1136)

#endif
