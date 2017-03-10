#ifndef rpi_shader_H
#define rpi_shader_H

extern unsigned int rpi_shader[];

#define mc_setup_uv (rpi_shader + 0)
#define mc_filter_uv (rpi_shader + 128)
#define mc_filter_uv_b0 (rpi_shader + 270)
#define mc_filter_uv_b (rpi_shader + 440)
#define mc_exit (rpi_shader + 596)
#define mc_interrupt_exit8 (rpi_shader + 614)
#define mc_setup (rpi_shader + 644)
#define mc_filter (rpi_shader + 930)
#define mc_filter_b (rpi_shader + 1050)
#define mc_interrupt_exit12 (rpi_shader + 1170)
#define mc_exit1 (rpi_shader + 1208)
#define mc_end (rpi_shader + 1224)

#endif
