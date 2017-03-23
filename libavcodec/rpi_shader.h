#ifndef rpi_shader_H
#define rpi_shader_H

extern unsigned int rpi_shader[];

#define mc_setup_uv (rpi_shader + 0)
#define mc_exit_nowait (rpi_shader + 34)
#define mc_filter_uv (rpi_shader + 34)
#define mc_filter_uv_b (rpi_shader + 34)
#define mc_filter_uv_b0 (rpi_shader + 34)
#define mc_exit (rpi_shader + 40)
#define mc_interrupt_exit8 (rpi_shader + 126)
#define mc_setup (rpi_shader + 146)
#define mc_filter (rpi_shader + 506)
#define mc_filter_b (rpi_shader + 626)
#define mc_interrupt_exit12 (rpi_shader + 746)
#define mc_exit1 (rpi_shader + 854)
#define mc_end (rpi_shader + 870)

#endif
