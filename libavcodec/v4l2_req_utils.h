/*
    Copyright (C) 2024  John Cox john.cox@raspberrypi.com

    Permission is hereby granted, free of charge, to any person
    obtaining a copy of this software and associated documentation
    files (the "Software"), to deal in the Software without
    restriction, including without limitation the rights to use, copy,
    modify, merge, publish, distribute, sublicense, and/or sell copies
    of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be
    included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
    HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
    WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
 */

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
