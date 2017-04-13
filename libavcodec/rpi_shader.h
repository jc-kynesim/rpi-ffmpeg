#ifndef rpi_shader_H
#define rpi_shader_H

extern unsigned int rpi_shader[];

#define mc_setup_uv (rpi_shader + 0)
#define mc_filter_uv (rpi_shader + 112)
#define mc_filter_uv_b0 (rpi_shader + 260)
#define mc_filter_uv_b (rpi_shader + 424)
#define mc_exit_c (rpi_shader + 556)
#define mc_exit (rpi_shader + 574)
#define mc_setup (rpi_shader + 590)
#define mc_filter (rpi_shader + 834)
#define mc_filter_b (rpi_shader + 954)
#define mc_interrupt_exit12c (rpi_shader + 1074)
#define mc_interrupt_exit12 (rpi_shader + 1092)
#define mc_exit1 (rpi_shader + 1128)
#define mc_end (rpi_shader + 1144)

#endif
