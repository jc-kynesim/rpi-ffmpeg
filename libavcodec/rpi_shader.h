#ifndef rpi_shader_H
#define rpi_shader_H

extern unsigned int rpi_shader[];

#define mc_setup_uv (rpi_shader + 0)
#define mc_filter_uv (rpi_shader + 108)
#define mc_filter_uv_b0 (rpi_shader + 252)
#define mc_filter_uv_b (rpi_shader + 412)
#define mc_exit_c (rpi_shader + 542)
#define mc_exit (rpi_shader + 560)
#define mc_setup (rpi_shader + 578)
#define mc_filter (rpi_shader + 850)
#define mc_filter_b (rpi_shader + 970)
#define mc_interrupt_exit12c (rpi_shader + 1090)
#define mc_interrupt_exit12 (rpi_shader + 1108)
#define mc_exit1 (rpi_shader + 1146)
#define mc_end (rpi_shader + 1162)

#endif
