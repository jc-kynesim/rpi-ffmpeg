#ifndef rpi_shader_H
#define rpi_shader_H

extern unsigned int rpi_shader[];

#define mc_setup_c (rpi_shader + 0)
#define mc_filter_uv (rpi_shader + 196)
#define mc_filter_uv_b0 (rpi_shader + 324)
#define mc_interrupt_exit8c (rpi_shader + 598)
#define mc_exit (rpi_shader + 626)
#define mc_exit_c (rpi_shader + 626)
#define mc_interrupt_exit12 (rpi_shader + 642)
#define mc_exit1 (rpi_shader + 678)
#define mc_setup (rpi_shader + 694)
#define mc_filter (rpi_shader + 980)
#define mc_filter_b (rpi_shader + 1100)
#define mc_end (rpi_shader + 1220)

#endif
