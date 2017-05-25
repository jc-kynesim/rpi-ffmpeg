#ifndef rpi_shader_H
#define rpi_shader_H

extern unsigned int rpi_shader[];

#define mc_setup_c (rpi_shader + 0)
#define mc_filter_uv (rpi_shader + 154)
#define mc_filter_uv_b0 (rpi_shader + 288)
#define mc_exit (rpi_shader + 574)
#define mc_exit_c (rpi_shader + 574)
#define mc_interrupt_exit12 (rpi_shader + 590)
#define mc_interrupt_exit12c (rpi_shader + 590)
#define mc_setup (rpi_shader + 626)
#define mc_filter (rpi_shader + 870)
#define mc_filter_b (rpi_shader + 1010)
#define mc_end (rpi_shader + 1150)

#endif
