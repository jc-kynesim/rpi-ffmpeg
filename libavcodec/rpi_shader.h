#ifndef rpi_shader_H
#define rpi_shader_H

extern unsigned int rpi_shader[];

#define mc_setup_c_q0 (rpi_shader + 0)
#define mc_start (rpi_shader + 0)
#define mc_setup_c_qn (rpi_shader + 2)
#define mc_filter_uv (rpi_shader + 144)
#define mc_filter_uv_b0 (rpi_shader + 280)
#define mc_sync_q0 (rpi_shader + 482)
#define mc_sync_q1 (rpi_shader + 500)
#define mc_sync_q2 (rpi_shader + 512)
#define mc_sync_q3 (rpi_shader + 524)
#define mc_sync_q4 (rpi_shader + 536)
#define mc_sync_q5 (rpi_shader + 554)
#define mc_sync_q6 (rpi_shader + 566)
#define mc_sync_q7 (rpi_shader + 578)
#define mc_sync_q8 (rpi_shader + 590)
#define mc_sync_q9 (rpi_shader + 608)
#define mc_sync_q10 (rpi_shader + 620)
#define mc_sync_q11 (rpi_shader + 632)
#define mc_exit (rpi_shader + 644)
#define mc_exit_c (rpi_shader + 644)
#define mc_interrupt_exit12 (rpi_shader + 658)
#define mc_interrupt_exit12c (rpi_shader + 658)
#define mc_setup_y_q0 (rpi_shader + 674)
#define mc_setup_y_qn (rpi_shader + 676)
#define mc_filter (rpi_shader + 918)
#define mc_filter_b (rpi_shader + 1058)
#define mc_filter_y_p00 (rpi_shader + 1198)
#define mc_filter_y_b00 (rpi_shader + 1294)
#define mc_end (rpi_shader + 1378)

#endif
