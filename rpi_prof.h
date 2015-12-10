#ifdef RPI_PROC_ALLOC
#define X volatile
#define Z =0
#else
#define X extern volatile
#define Z
#endif

X unsigned int rpi_residual_count Z;
X unsigned int rpi_residual_signs Z;
X unsigned int rpi_residual_sig_coeffs Z;
X unsigned int rpi_residual_sig_bits Z;
X uint64_t rpi_residual_abs_cycles Z;
X unsigned int rpi_residual_abs_cnt Z;

#undef X
#undef Z

