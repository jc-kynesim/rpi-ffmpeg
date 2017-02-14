#ifndef rpi_shader_H
#define rpi_shader_H

extern unsigned int rpi_shader[];

#define mc_setup_uv (rpi_shader + 0)
#define mc_filter_uv (rpi_shader + 130)
#define mc_filter_uv_b0 (rpi_shader + 314)
#define mc_filter_uv_b (rpi_shader + 466)
#define mc_exit (rpi_shader + 642)
#define mc_interrupt_exit8 (rpi_shader + 660)
#define mc_setup (rpi_shader + 690)
#define mc_filter (rpi_shader + 1042)
#define mc_filter_b (rpi_shader + 1168)
#define mc_interrupt_exit12 (rpi_shader + 1300)
#define mc_exit1 (rpi_shader + 1338)
#define mc_end (rpi_shader + 1354)

#endif
