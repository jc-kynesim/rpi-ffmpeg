#ifndef rpi_shader_H
#define rpi_shader_H

extern unsigned int rpi_shader[];

#define mc_setup_uv (rpi_shader + 0)
#define mc_filter_uv (rpi_shader + 148)
#define mc_filter_uv_b0 (rpi_shader + 338)
#define mc_filter_uv_b (rpi_shader + 490)
#define mc_exit (rpi_shader + 666)
#define mc_interrupt_exit8 (rpi_shader + 684)
#define mc_setup (rpi_shader + 714)
#define mc_filter (rpi_shader + 868)
#define mc_filter_b (rpi_shader + 1108)
#define mc_interrupt_exit12 (rpi_shader + 1364)
#define mc_end (rpi_shader + 1402)

#endif
