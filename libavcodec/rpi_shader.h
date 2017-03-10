#ifndef rpi_shader_H
#define rpi_shader_H

extern unsigned int rpi_shader[];

#define mc_setup_uv (rpi_shader + 0)
#define mc_filter_uv (rpi_shader + 120)
#define mc_filter_uv_b0 (rpi_shader + 262)
#define mc_filter_uv_b (rpi_shader + 430)
#define mc_exit (rpi_shader + 586)
#define mc_interrupt_exit8 (rpi_shader + 604)
#define mc_setup (rpi_shader + 634)
#define mc_filter (rpi_shader + 918)
#define mc_filter_b (rpi_shader + 1038)
#define mc_interrupt_exit12 (rpi_shader + 1158)
#define mc_exit1 (rpi_shader + 1196)
#define mc_end (rpi_shader + 1212)

#endif
