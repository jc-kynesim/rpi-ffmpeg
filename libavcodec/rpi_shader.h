#ifndef rpi_shader_H
#define rpi_shader_H

extern unsigned int rpi_shader[];

#define mc_setup_uv (rpi_shader + 0)
#define mc_filter_uv (rpi_shader + 120)
#define mc_filter_uv_b0 (rpi_shader + 262)
#define mc_filter_uv_b (rpi_shader + 430)
#define mc_exit (rpi_shader + 558)
#define mc_interrupt_exit8 (rpi_shader + 576)
#define mc_setup (rpi_shader + 606)
#define mc_filter (rpi_shader + 890)
#define mc_filter_b (rpi_shader + 1010)
#define mc_interrupt_exit12 (rpi_shader + 1130)
#define mc_exit1 (rpi_shader + 1168)
#define mc_end (rpi_shader + 1184)

#endif
