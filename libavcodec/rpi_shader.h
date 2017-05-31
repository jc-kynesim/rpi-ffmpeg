#ifndef rpi_shader_H
#define rpi_shader_H

extern unsigned int rpi_shader[];

#define mc_setup_c (rpi_shader + 0)
#define mc_filter_uv (rpi_shader + 142)
#define mc_filter_uv_b0 (rpi_shader + 278)
#define mc_exit (rpi_shader + 484)
#define mc_exit_c (rpi_shader + 484)
#define mc_interrupt_exit12 (rpi_shader + 500)
#define mc_interrupt_exit12c (rpi_shader + 500)
#define mc_setup (rpi_shader + 536)
#define mc_filter (rpi_shader + 778)
#define mc_filter_b (rpi_shader + 918)
#define mc_end (rpi_shader + 1058)

#endif
