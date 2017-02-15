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
#define mc_filter (rpi_shader + 1028)
#define mc_filter_b (rpi_shader + 1150)
#define mc_interrupt_exit12 (rpi_shader + 1274)
#define mc_exit1 (rpi_shader + 1312)
#define mc_end (rpi_shader + 1328)

#endif
