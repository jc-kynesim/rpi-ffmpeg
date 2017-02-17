#ifndef rpi_shader_H
#define rpi_shader_H

extern unsigned int rpi_shader[];

#define mc_setup_uv (rpi_shader + 0)
#define mc_filter_uv (rpi_shader + 132)
#define mc_filter_uv_b0 (rpi_shader + 274)
#define mc_filter_uv_b (rpi_shader + 392)
#define mc_exit (rpi_shader + 542)
#define mc_interrupt_exit8 (rpi_shader + 560)
#define mc_setup (rpi_shader + 590)
#define mc_filter (rpi_shader + 874)
#define mc_filter_b (rpi_shader + 994)
#define mc_interrupt_exit12 (rpi_shader + 1116)
#define mc_exit1 (rpi_shader + 1154)
#define mc_end (rpi_shader + 1170)

#endif
