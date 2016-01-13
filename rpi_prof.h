#ifndef RPI_PROFILE_H
#define RPI_PROFILE_H

#if RPI_PROFILE

#include "libavutil/arm/v7_pmu.h"

#ifdef RPI_PROC_ALLOC
#define X volatile
#define Z =0
#else
#define X extern volatile
#define Z
#endif

X unsigned int av_rpi_residual_sig_coeffs Z;
X unsigned int av_rpi_residual_sig_bits Z;

X uint64_t av_rpi_residual_group_cycles Z;
X unsigned int av_rpi_residual_group_cnt Z;
#define RPI_residual_group_MAX_DURATION 5000

X uint64_t av_rpi_residual_xy_final_cycles Z;
X unsigned int av_rpi_residual_xy_final_cnt Z;
#define RPI_residual_xy_final_MAX_DURATION 5000

X uint64_t av_rpi_residual_abs_cycles Z;
X unsigned int av_rpi_residual_abs_cnt Z;
#define RPI_residual_abs_MAX_DURATION 5000

X uint64_t av_rpi_residual_greater1_cycles Z;
X unsigned int av_rpi_residual_greater1_cnt Z;
#define RPI_residual_greater1_MAX_DURATION 10000

X uint64_t av_rpi_residual_scale_cycles Z;
X unsigned int av_rpi_residual_scale_cnt Z;
#define RPI_residual_scale_MAX_DURATION 10000

X uint64_t av_rpi_residual_core_cycles Z;
X unsigned int av_rpi_residual_core_cnt Z;
#define RPI_residual_core_MAX_DURATION 100000

X uint64_t av_rpi_residual_base_cycles Z;
X unsigned int av_rpi_residual_base_cnt Z;
#define RPI_residual_base_MAX_DURATION 100000

X uint64_t av_rpi_residual_sig_cycles Z;
X unsigned int av_rpi_residual_sig_cnt Z;
#define RPI_residual_sig_MAX_DURATION 10000


#undef X
#undef Z

#define PROFILE_INIT()\
do {\
    av_arm_enable_pmu();\
    av_arm_enable_ccnt();\
} while (0)

#define PROFILE_START()\
do {\
    volatile uint32_t perf_1 = av_arm_read_ccnt();\
    volatile uint32_t perf_2


#define PROFILE_ACC(x)\
    perf_2 = av_arm_read_ccnt();\
    {\
        const uint32_t duration = perf_2 - perf_1;\
        if (duration < RPI_##x##_MAX_DURATION)\
        {\
            av_rpi_##x##_cycles += duration;\
            av_rpi_##x##_cnt += 1;\
        }\
    }\
} while(0)

#define PROFILE_PRINTF(x)\
    printf("%-20s cycles=%14" PRIu64 " cnt=%u\n", #x, av_rpi_##x##_cycles, av_rpi_##x##_cnt)

#else

// No profile
#define PROFILE_INIT()
#define PROFILE_START()
#define PROFILE_ACC(x)
#define PROFILE_PRINTF(x)

#endif

#endif
