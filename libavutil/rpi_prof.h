#ifndef RPI_PROFILE_H
#define RPI_PROFILE_H

#include "config.h"
#include <stdint.h>

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

X uint64_t av_rpi_planar_cycles Z;
X unsigned int av_rpi_planar_cnt Z;
#define RPI_planar_MAX_DURATION 10000

X uint64_t av_rpi_dc_cycles Z;
X unsigned int av_rpi_dc_cnt Z;
#define RPI_dc_MAX_DURATION 10000

X uint64_t av_rpi_angular_h_cycles Z;
X unsigned int av_rpi_angular_h_cnt Z;
#define RPI_angular_h_MAX_DURATION 10000

X uint64_t av_rpi_angular_v_cycles Z;
X unsigned int av_rpi_angular_v_cnt Z;
#define RPI_angular_v_MAX_DURATION 10000

X uint64_t av_rpi_angular_cycles Z;
X unsigned int av_rpi_angular_cnt Z;
#define RPI_angular_MAX_DURATION 10000


X uint64_t av_rpi_planar_c_cycles Z;
X unsigned int av_rpi_planar_c_cnt Z;
#define RPI_planar_c_MAX_DURATION 10000

X uint64_t av_rpi_dc_c_cycles Z;
X unsigned int av_rpi_dc_c_cnt Z;
#define RPI_dc_c_MAX_DURATION 10000

X uint64_t av_rpi_angular_h_c_cycles Z;
X unsigned int av_rpi_angular_h_c_cnt Z;
#define RPI_angular_h_c_MAX_DURATION 10000

X uint64_t av_rpi_angular_v_c_cycles Z;
X unsigned int av_rpi_angular_v_c_cnt Z;
#define RPI_angular_v_c_MAX_DURATION 10000

X uint64_t av_rpi_angular_c_cycles Z;
X unsigned int av_rpi_angular_c_cnt Z;
#define RPI_angular_c_MAX_DURATION 10000


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
            av_rpi_##x##_cycles += duration;\
            av_rpi_##x##_cnt += 1;\
        }\
    }\
} while(0)

#define PROFILE_PRINTF(x)\
    printf("%-20s cycles=%14" PRIu64 ";  cnt=%8u;  avg=%5" PRIu64 "\n", #x, av_rpi_##x##_cycles, av_rpi_##x##_cnt, av_rpi_##x##_cycles / (uint64_t)av_rpi_##x##_cnt)

#else

// No profile
#define PROFILE_INIT()
#define PROFILE_START()
#define PROFILE_ACC(x)
#define PROFILE_PRINTF(x)

#endif

#endif

