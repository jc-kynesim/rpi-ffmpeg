#ifndef rpi_shader_H
#define rpi_shader_H

extern unsigned int rpi_shader[];

#define mc_setup_uv (rpi_shader + 0)
#define mc_filter_uv (rpi_shader + 132)
#define mc_filter_uv_b0 (rpi_shader + 300)
#define mc_filter_uv_b (rpi_shader + 444)
#define mc_exit (rpi_shader + 630)
#define mc_interrupt_exit8 (rpi_shader + 648)
#define mc_setup (rpi_shader + 678)
#define mc_filter (rpi_shader + 1030)
#define mc_filter_b (rpi_shader + 1158)
#define mc_interrupt_exit12 (rpi_shader + 1288)
#define mc_exit1 (rpi_shader + 1326)
#define mc_end (rpi_shader + 1342)

#endif
