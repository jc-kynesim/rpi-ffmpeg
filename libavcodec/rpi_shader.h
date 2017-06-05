#ifndef rpi_shader_H
#define rpi_shader_H

extern unsigned int rpi_shader[];

#define mc_setup_c (rpi_shader + 0)
#define mc_filter_uv (rpi_shader + 142)
#define mc_filter_uv_b0 (rpi_shader + 278)
#define mc_sync_a (rpi_shader + 480)
#define mc_sync_a0 (rpi_shader + 502)
#define mc_exit (rpi_shader + 558)
#define mc_exit_c (rpi_shader + 558)
#define mc_interrupt_exit12 (rpi_shader + 574)
#define mc_interrupt_exit12c (rpi_shader + 574)
#define mc_setup (rpi_shader + 610)
#define mc_filter (rpi_shader + 852)
#define mc_filter_b (rpi_shader + 992)
#define mc_filter_y_p00 (rpi_shader + 1132)
#define mc_filter_y_b00 (rpi_shader + 1228)
#define mc_end (rpi_shader + 1312)

#endif
