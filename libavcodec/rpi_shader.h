#ifndef rpi_shader_H
#define rpi_shader_H

extern unsigned int rpi_shader[];

#define mc_setup_uv (rpi_shader + 0)
#define mc_filter_uv (rpi_shader + 132)
#define mc_filter_uv_b0 (rpi_shader + 274)
#define mc_filter_uv_b (rpi_shader + 416)
#define mc_exit (rpi_shader + 564)
#define mc_interrupt_exit8 (rpi_shader + 582)
#define mc_setup (rpi_shader + 612)
#define mc_filter (rpi_shader + 902)
#define mc_filter_b (rpi_shader + 1022)
#define mc_interrupt_exit12 (rpi_shader + 1142)
#define mc_exit1 (rpi_shader + 1180)
#define mc_end (rpi_shader + 1196)

#endif
