#ifndef rpi_shader_H
#define rpi_shader_H

extern unsigned int rpi_shader[];

#define mc_setup_uv (rpi_shader + 0)
#define mc_filter_uv (rpi_shader + 152)
#define mc_filter_uv_b0 (rpi_shader + 316)
#define mc_filter_uv_b (rpi_shader + 476)
#define mc_exit (rpi_shader + 650)
#define mc_interrupt_exit8 (rpi_shader + 668)
#define mc_end (rpi_shader + 698)

#endif
