#ifndef rpi_shader_H
#define rpi_shader_H

extern unsigned int rpi_shader[];

#define mc_setup_uv (rpi_shader + 0)
#define mc_filter_uv (rpi_shader + 148)
#define mc_filter_uv_b0 (rpi_shader + 310)
#define mc_filter_uv_b (rpi_shader + 458)
#define mc_exit (rpi_shader + 630)
#define mc_interrupt_exit8 (rpi_shader + 648)
#define mc_end (rpi_shader + 678)

#endif
