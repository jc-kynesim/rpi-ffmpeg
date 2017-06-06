#ifndef rpi_shader_H
#define rpi_shader_H

extern unsigned int rpi_shader[];

#define mc_setup_c (rpi_shader + 0)
#define mc_filter_uv (rpi_shader + 142)
#define mc_filter_uv_b0 (rpi_shader + 278)
#define mc_sync_q0 (rpi_shader + 480)
#define mc_sync_q1 (rpi_shader + 494)
#define mc_sync_q2 (rpi_shader + 506)
#define mc_sync_q3 (rpi_shader + 518)
#define mc_sync_q4 (rpi_shader + 530)
#define mc_sync_q5 (rpi_shader + 544)
#define mc_sync_q6 (rpi_shader + 556)
#define mc_sync_q7 (rpi_shader + 568)
#define mc_sync_q8 (rpi_shader + 580)
#define mc_sync_q9 (rpi_shader + 594)
#define mc_sync_q10 (rpi_shader + 606)
#define mc_sync_q11 (rpi_shader + 618)
#define mc_exit (rpi_shader + 630)
#define mc_exit_c (rpi_shader + 630)
#define mc_interrupt_exit12 (rpi_shader + 646)
#define mc_interrupt_exit12c (rpi_shader + 646)
#define mc_setup (rpi_shader + 682)
#define mc_filter (rpi_shader + 924)
#define mc_filter_b (rpi_shader + 1064)
#define mc_filter_y_p00 (rpi_shader + 1204)
#define mc_filter_y_b00 (rpi_shader + 1300)
#define mc_end (rpi_shader + 1384)

#endif
