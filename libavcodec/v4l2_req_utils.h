#ifndef AVCODEC_V4L2_REQ_UTILS_H
#define AVCODEC_V4L2_REQ_UTILS_H

#include <stdint.h>
#include "libavutil/log.h"

#define request_log(...) av_log(NULL, AV_LOG_INFO, __VA_ARGS__)

#define request_err(_ctx, ...) av_log(_ctx, AV_LOG_ERROR, __VA_ARGS__)
#define request_warn(_ctx, ...) av_log(_ctx, AV_LOG_WARNING, __VA_ARGS__)
#define request_info(_ctx, ...) av_log(_ctx, AV_LOG_INFO, __VA_ARGS__)
#define request_debug(_ctx, ...) av_log(_ctx, AV_LOG_DEBUG, __VA_ARGS__)

static inline char safechar(char c) {
    return c > 0x20 && c < 0x7f ? c : '.';
}

static inline const char * strfourcc(char tbuf[5], uint32_t fcc) {
    tbuf[0] = safechar((fcc >>  0) & 0xff);
    tbuf[1] = safechar((fcc >>  8) & 0xff);
    tbuf[2] = safechar((fcc >> 16) & 0xff);
    tbuf[3] = safechar((fcc >> 24) & 0xff);
    tbuf[4] = '\0';
    return tbuf;
}

#endif
