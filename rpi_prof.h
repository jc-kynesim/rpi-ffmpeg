#ifndef RPI_PROFILE_H
#define RPI_PROFILE_H

#if ARCH_ARM
#define RPI_PROFILE 1
#else
#define RPI_PROFILE 0
#endif

#if RPI_PROFILE

#include "libavutil/arm/v7_pmu.h"

#ifdef RPI_PROC_ALLOC
#define X volatile
#define Z =0
#else
#define X extern volatile
#define Z
#endif

X unsigned int rpi_residual_sig_coeffs Z;
X unsigned int rpi_residual_sig_bits Z;

X uint64_t rpi_residual_abs_cycles Z;
X unsigned int rpi_residual_abs_cnt Z;
#define RPI_residual_abs_MAX_DURATION 5000

X uint64_t rpi_residual_greater1_cycles Z;
X unsigned int rpi_residual_greater1_cnt Z;
#define RPI_residual_greater1_MAX_DURATION 10000

X uint64_t rpi_residual_core_cycles Z;
X unsigned int rpi_residual_core_cnt Z;
#define RPI_residual_core_MAX_DURATION 100000

X uint64_t rpi_residual_base_cycles Z;
X unsigned int rpi_residual_base_cnt Z;
#define RPI_residual_base_MAX_DURATION 100000

#undef X
#undef Z

#define PROFILE_INIT()\
do {\
    enable_pmu();\
    enable_ccnt();\
} while (0)

#define PROFILE_START()\
do {\
    volatile uint32_t perf_1 = read_ccnt();\
    volatile uint32_t perf_2


#define PROFILE_ACC(x)\
    perf_2 = read_ccnt();\
    {\
        const uint32_t duration = perf_2 - perf_1;\
        if (duration < RPI_##x##_MAX_DURATION)\
        {\
            rpi_##x##_cycles += duration;\
            rpi_##x##_cnt += 1;\
        }\
    }\
} while(0)

#define PROFILE_PRINTF(x)\
    printf("%-20s cycles=%14" PRIu64 " cnt=%u\n", #x, rpi_##x##_cycles, rpi_##x##_cnt)

#else

// No profile
#define PROFILE_INIT()
#define PROFILE_START()
#define PROFILE_ACC(x)
#define PROFILE_PRINTF(x)

#endif

#endif

