#ifndef rpi_shader_H
#define rpi_shader_H

extern unsigned int rpi_shader[];

#define mc_setup_c (rpi_shader + 0)
#define mc_filter_uv (rpi_shader + 156)
#define mc_filter_uv_b0 (rpi_shader + 294)
#define mc_exit (rpi_shader + 588)
#define mc_exit_c (rpi_shader + 588)
#define mc_interrupt_exit12 (rpi_shader + 604)
#define mc_interrupt_exit12c (rpi_shader + 604)
#define mc_setup (rpi_shader + 640)
#define mc_filter (rpi_shader + 918)
#define mc_filter_b (rpi_shader + 1058)
#define mc_end (rpi_shader + 1198)

#endif
