#ifndef rpi_shader_H
#define rpi_shader_H

extern unsigned int rpi_shader[];

#define mc_setup_uv (rpi_shader + 0)
#define mc_filter_uv (rpi_shader + 132)
#define mc_filter_uv_b0 (rpi_shader + 300)
#define mc_filter_uv_b (rpi_shader + 436)
#define mc_exit (rpi_shader + 608)
#define mc_interrupt_exit8 (rpi_shader + 626)
#define mc_setup (rpi_shader + 656)
#define mc_filter (rpi_shader + 1000)
#define mc_filter_b (rpi_shader + 1122)
#define mc_interrupt_exit12 (rpi_shader + 1246)
#define mc_exit1 (rpi_shader + 1284)
#define mc_end (rpi_shader + 1300)

#endif
