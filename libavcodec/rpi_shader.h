#ifndef rpi_shader_H
#define rpi_shader_H

extern unsigned int rpi_shader[];

#define mc_setup_c (rpi_shader + 0)
#define mc_filter_uv (rpi_shader + 142)
#define mc_filter_uv_b0 (rpi_shader + 278)
#define mc_sync_init_0 (rpi_shader + 480)
#define mc_sync_q0 (rpi_shader + 494)
#define mc_sync_q1 (rpi_shader + 518)
#define mc_sync_q2 (rpi_shader + 530)
#define mc_sync_q3 (rpi_shader + 542)
#define mc_sync_q4 (rpi_shader + 554)
#define mc_sync_q5 (rpi_shader + 578)
#define mc_sync_q6 (rpi_shader + 590)
#define mc_sync_q7 (rpi_shader + 602)
#define mc_sync_q8 (rpi_shader + 614)
#define mc_sync_q9 (rpi_shader + 638)
#define mc_sync_q10 (rpi_shader + 650)
#define mc_sync_q11 (rpi_shader + 662)
#define mc_exit (rpi_shader + 674)
#define mc_exit_c (rpi_shader + 674)
#define mc_interrupt_exit12 (rpi_shader + 690)
#define mc_interrupt_exit12c (rpi_shader + 690)
#define mc_setup (rpi_shader + 728)
#define mc_filter (rpi_shader + 970)
#define mc_filter_b (rpi_shader + 1110)
#define mc_filter_y_p00 (rpi_shader + 1250)
#define mc_filter_y_b00 (rpi_shader + 1346)
#define mc_end (rpi_shader + 1430)

#endif
