#ifndef rpi_shader_H
#define rpi_shader_H

extern unsigned int rpi_shader[];

#define mc_setup_uv (rpi_shader + 0)
#define mc_filter_uv (rpi_shader + 120)
#define mc_filter_uv_b0 (rpi_shader + 264)
#define mc_filter_uv_b (rpi_shader + 432)
#define mc_exit_c (rpi_shader + 562)
#define mc_exit (rpi_shader + 580)
#define mc_interrupt_exit8 (rpi_shader + 598)
#define mc_setup (rpi_shader + 628)
#define mc_filter (rpi_shader + 912)
#define mc_filter_b (rpi_shader + 1032)
#define mc_interrupt_exit12c (rpi_shader + 1152)
#define mc_interrupt_exit12 (rpi_shader + 1170)
#define mc_exit1 (rpi_shader + 1208)
#define mc_end (rpi_shader + 1224)

#endif
