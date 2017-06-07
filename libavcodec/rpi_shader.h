#ifndef rpi_shader_H
#define rpi_shader_H

extern unsigned int rpi_shader[];

#define mc_setup_c (rpi_shader + 0)
#define mc_filter_uv (rpi_shader + 142)
#define mc_filter_uv_b0 (rpi_shader + 278)
#define mc_sync_init_0 (rpi_shader + 480)
#define mc_sync_q0 (rpi_shader + 492)
#define mc_sync_q1 (rpi_shader + 510)
#define mc_sync_q2 (rpi_shader + 522)
#define mc_sync_q3 (rpi_shader + 534)
#define mc_sync_q4 (rpi_shader + 546)
#define mc_sync_q5 (rpi_shader + 564)
#define mc_sync_q6 (rpi_shader + 576)
#define mc_sync_q7 (rpi_shader + 588)
#define mc_sync_q8 (rpi_shader + 600)
#define mc_sync_q9 (rpi_shader + 618)
#define mc_sync_q10 (rpi_shader + 630)
#define mc_sync_q11 (rpi_shader + 642)
#define mc_exit (rpi_shader + 654)
#define mc_exit_c (rpi_shader + 654)
#define mc_interrupt_exit12 (rpi_shader + 670)
#define mc_interrupt_exit12c (rpi_shader + 670)
#define mc_setup (rpi_shader + 708)
#define mc_filter (rpi_shader + 950)
#define mc_filter_b (rpi_shader + 1090)
#define mc_filter_y_p00 (rpi_shader + 1230)
#define mc_filter_y_b00 (rpi_shader + 1326)
#define mc_end (rpi_shader + 1410)

#endif
