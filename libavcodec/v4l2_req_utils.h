#include "libavutil/log.h"

#define request_log(...) av_log(NULL, AV_LOG_INFO, __VA_ARGS__)

#define request_err(_ctx, ...) av_log(_ctx, AV_LOG_ERROR, __VA_ARGS__)
#define request_info(_ctx, ...) av_log(_ctx, AV_LOG_INFO, __VA_ARGS__)

