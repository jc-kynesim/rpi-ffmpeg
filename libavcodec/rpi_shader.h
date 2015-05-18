#ifndef rpi_shader_H
#define rpi_shader_H

extern unsigned int rpi_shader[];

#define mc_setup_uv (rpi_shader + 0)
#define mc_filter_uv (rpi_shader + 152)
#define mc_filter_uv_b0 (rpi_shader + 342)
#define mc_filter_uv_b (rpi_shader + 494)
#define mc_exit (rpi_shader + 670)
#define mc_interrupt_exit8 (rpi_shader + 688)
#define mc_end (rpi_shader + 718)

#endif
