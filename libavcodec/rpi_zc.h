#include "libavutil/frame.h"
#include "libavcodec/avcodec.h"

// "Opaque" pointer to whatever we are using as a buffer reference
typedef AVBufferRef * AVRpiZcRefPtr;

// Replacement fn for avctx->get_buffer2
int av_rpi_zc_get_buffer2(struct AVCodecContext *s, AVFrame *frame, int flags);

// Generate a ZC reference to the buffer(s) in this frame
// If the buffer doesn't appear to be one allocated by _get_buffer_2
// then the behaviour depends on maycopy:
//   If maycopy=0 then return NULL
//   If maycopy=1 && the src frame is in a form where we can easily copy
//     the data, then allocate a new buffer and copy the data into it
//   Otherwise return NULL
AVRpiZcRefPtr av_rpi_zc_ref(const AVFrame * const frame, const int maycopy);

// Get the vc_handle from the frame ref
// Returns -1 if ref doesn't look valid
int av_rpi_zc_vc_handle(const AVRpiZcRefPtr fr_ref);
// Get the number of bytes allocated from the frame ref
// Returns 0 if ref doesn't look valid
int av_rpi_zc_numbytes(const AVRpiZcRefPtr fr_ref);

// Unreference the buffer refed/allocated by _zc_ref
// If fr_ref is NULL then this will NOP
void av_rpi_zc_unref(AVRpiZcRefPtr fr_ref);



