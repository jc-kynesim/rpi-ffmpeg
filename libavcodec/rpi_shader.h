#ifndef rpi_shader_H
#define rpi_shader_H

extern unsigned int rpi_shader[];

#define mc_setup (rpi_shader + 0)
#define mc_filter_uv (rpi_shader + 146)
#define mc_filter (rpi_shader + 360)
#define mc_filter_b (rpi_shader + 670)
#define mc_filter_honly (rpi_shader + 894)
#define mc_exit (rpi_shader + 1048)
#define mc_exit1 (rpi_shader + 1066)
#define mc_interrupt_exit (rpi_shader + 1082)
#define mc_interrupt_exit4 (rpi_shader + 1120)
#define mc_interrupt_exit8 (rpi_shader + 1142)
#define mc_setup_uv (rpi_shader + 1172)
#define mc_filter_uv_b (rpi_shader + 1314)
#define mc_end (rpi_shader + 1542)

#endif
