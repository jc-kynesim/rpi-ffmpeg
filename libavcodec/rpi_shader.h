#ifndef rpi_shader_H
#define rpi_shader_H

extern unsigned int rpi_shader[];

#define mc_setup_uv (rpi_shader + 0)
#define mc_filter_uv (rpi_shader + 132)
#define mc_filter_uv_b0 (rpi_shader + 312)
#define mc_filter_uv_b (rpi_shader + 466)
#define mc_exit (rpi_shader + 656)
#define mc_interrupt_exit8 (rpi_shader + 674)
#define mc_setup (rpi_shader + 704)
#define mc_filter (rpi_shader + 1056)
#define mc_filter_b (rpi_shader + 1182)
#define mc_interrupt_exit12 (rpi_shader + 1314)
#define mc_exit1 (rpi_shader + 1352)
#define mc_end (rpi_shader + 1368)

#endif
