#ifndef rpi_shader_H
#define rpi_shader_H

extern unsigned int rpi_shader[];

#define mc_setup_c (rpi_shader + 0)
#define mc_filter_uv (rpi_shader + 132)
#define mc_filter_uv_b0 (rpi_shader + 296)
#define mc_filter_uv_b (rpi_shader + 478)
#define mc_exit_c (rpi_shader + 620)
#define mc_exit (rpi_shader + 632)
#define mc_setup (rpi_shader + 648)
#define mc_filter (rpi_shader + 932)
#define mc_filter_b (rpi_shader + 1052)
#define mc_interrupt_exit6c (rpi_shader + 1172)
#define mc_interrupt_exit8c (rpi_shader + 1192)
#define mc_interrupt_exit12 (rpi_shader + 1216)
#define mc_exit1 (rpi_shader + 1252)
#define mc_end (rpi_shader + 1268)

#endif
