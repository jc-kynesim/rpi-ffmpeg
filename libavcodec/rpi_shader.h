#ifndef rpi_shader_H
#define rpi_shader_H

extern unsigned int rpi_shader[];

#define mc_setup_uv (rpi_shader + 0)
#define mc_filter_uv (rpi_shader + 130)
#define mc_filter_uv_b0 (rpi_shader + 294)
#define mc_filter_uv_b (rpi_shader + 472)
#define mc_exit_c (rpi_shader + 620)
#define mc_exit (rpi_shader + 650)
#define mc_setup (rpi_shader + 666)
#define mc_filter (rpi_shader + 950)
#define mc_filter_b (rpi_shader + 1070)
#define mc_interrupt_exit12c (rpi_shader + 1190)
#define mc_interrupt_exit12 (rpi_shader + 1240)
#define mc_exit1 (rpi_shader + 1276)
#define mc_end (rpi_shader + 1292)

#endif
