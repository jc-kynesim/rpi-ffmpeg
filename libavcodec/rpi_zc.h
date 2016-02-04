#include "libavutil/frame.h"
#include "libavcodec/avcodec.h"

typedef AVBufferRef * AVRpiZcRefPtr;

int av_rpi_zc_get_buffer2(struct AVCodecContext *s, AVFrame *frame, int flags);

AVRpiZcRefPtr av_rpi_zc_ref(const AVFrame * const frame);
int av_rpi_zc_vc_handle(const AVRpiZcRefPtr fr_ref);
int av_rpi_zc_numbytes(const AVRpiZcRefPtr fr_ref);

void av_rpi_zc_unref(AVRpiZcRefPtr fr_ref);



