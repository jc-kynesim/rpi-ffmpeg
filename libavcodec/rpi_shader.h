#ifndef rpi_shader_H
#define rpi_shader_H

extern unsigned int rpi_shader[];

#define mc_setup_c (rpi_shader + 0)
#define mc_filter_uv (rpi_shader + 128)
#define mc_filter_uv_b0 (rpi_shader + 290)
#define mc_filter_uv_b (rpi_shader + 470)
#define mc_exit_c (rpi_shader + 612)
#define mc_exit (rpi_shader + 624)
#define mc_setup (rpi_shader + 640)
#define mc_filter (rpi_shader + 924)
#define mc_filter_b (rpi_shader + 1044)
#define mc_interrupt_exit6c (rpi_shader + 1164)
#define mc_interrupt_exit8c (rpi_shader + 1184)
#define mc_interrupt_exit12 (rpi_shader + 1208)
#define mc_exit1 (rpi_shader + 1244)
#define mc_end (rpi_shader + 1260)

#endif
