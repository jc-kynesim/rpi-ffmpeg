#ifndef rpi_shader_H
#define rpi_shader_H

extern unsigned int rpi_shader[];

#define mc_setup (rpi_shader + 0)
#define mc_filter_uv (rpi_shader + 146)
#define mc_filter (rpi_shader + 364)
#define mc_filter_b (rpi_shader + 674)
#define mc_filter_honly (rpi_shader + 898)
#define mc_exit (rpi_shader + 1052)
#define mc_exit1 (rpi_shader + 1070)
#define mc_interrupt_exit (rpi_shader + 1086)
#define mc_interrupt_exit4 (rpi_shader + 1124)
#define mc_interrupt_exit8 (rpi_shader + 1146)
#define mc_setup_uv (rpi_shader + 1176)
#define mc_filter_uv_b (rpi_shader + 1318)
#define mc_end (rpi_shader + 1546)

#endif
