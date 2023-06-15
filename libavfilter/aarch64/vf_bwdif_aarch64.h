#ifndef AVFILTER_AARCH64_VF_BWDIF_H_
#define AVFILTER_AARCH64_VF_BWDIF_H_

void ff_bwdif_filter_line4_aarch64(void * dst1, int d_stride,
                          const void * prev1, const void * cur1, const void * next1, int prefs,
                          int w, int parity, int clip_max);

#endif
