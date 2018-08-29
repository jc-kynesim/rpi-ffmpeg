#ifndef AVCODEC_RPI_HEVC_MV_H
#define AVCODEC_RPI_HEVC_MV_H

#include "config.h"

typedef int32_t MvXY;

typedef struct HEVCRpiMvField {
    MvXY xy[2];
    int8_t ref_idx[2];
    int8_t pred_flag;
    int8_t dummy; // To 12 bytes
} HEVCRpiMvField;


#define MV_X(xy) (((xy) << 16) >> 16)
#define MV_Y(xy) ((xy) >> 16)
#define MV_XY(x, y) ((x & 0xffff) | ((y) << 16))

#if ARCH_ARM
#include "arm/rpi_hevc_mv_arm.h"
#endif

#ifndef mvxy_add
static inline MvXY mvxy_add(const MvXY a, const MvXY b)
{
    return MV_XY(MV_X(a) + MV_X(b), MV_Y(a) + MV_Y(b));
}
#endif


#ifndef mv_scale_xy
static inline MvXY mv_scale_xy(const MvXY const src, int td, int tb)
{
    int tx, scale_factor;

    td = td == 0 ? 1 : av_clip_int8(td);
    tb = av_clip_int8(tb);
    tx = (0x4000 + (abs(td) >> 1)) / td;
    scale_factor = av_clip_intp2((tb * tx + 32) >> 6, 12);
    return MV_XY(
        av_clip_int16((scale_factor * MV_X(src) + 127 +
                           (scale_factor * MV_X(src) < 0)) >> 8),
        av_clip_int16((scale_factor * MV_Y(src) + 127 +
                           (scale_factor * MV_Y(src) < 0)) >> 8));
}
#endif

// 8.3.1 states that the bitstream may not contain poc diffs that do not
// fit in 16 bits, so given that we don't care about the high bits we only
// store the low 16 + LT & Inter flags

#define COL_POC_INTRA   0
#define COL_POC_INTER   (1 << 16)
#define COL_POC_LT      (1 << 17)
#define COL_POC_DIFF(x,y) ((int16_t)((x) - (y)))
#define COL_POC_MAKE_INTER(lt,poc) (COL_POC_INTER | ((lt) ? COL_POC_LT : 0) | ((poc) & 0xffff))
#define COL_POC_IS_LT(x) (((x) & COL_POC_LT) != 0)

typedef struct ColMv_s {
    int32_t poc;
    int32_t xy;
} ColMv;

typedef struct ColMvField_s {
    ColMv L[2];
} ColMvField;



#endif // AVCODEC_RPI_HEVC_MV_H
