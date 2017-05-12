#ifndef rpi_shader_H
#define rpi_shader_H

extern unsigned int rpi_shader[];

#define mc_setup_c (rpi_shader + 0)
#define mc_filter_uv (rpi_shader + 198)
#define mc_filter_uv_b0 (rpi_shader + 360)
#define mc_filter_uv_b (rpi_shader + 540)
#define mc_exit (rpi_shader + 682)
#define mc_exit_c (rpi_shader + 682)
#define mc_setup (rpi_shader + 698)
#define mc_filter (rpi_shader + 982)
#define mc_filter_b (rpi_shader + 1102)
#define mc_interrupt_exit8c (rpi_shader + 1222)
#define mc_interrupt_exit12 (rpi_shader + 1250)
#define mc_exit1 (rpi_shader + 1286)
#define mc_end (rpi_shader + 1302)

#endif
