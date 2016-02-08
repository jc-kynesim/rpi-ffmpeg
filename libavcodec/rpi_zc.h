#ifndef LIBAVCODEC_RPI_ZC_H
#define LIBAVCODEC_RPI_ZC_H

// Zero-Copy frame code for RPi
// RPi needs Y/U/V planes to be contiguous for display.  By default
// ffmpeg will allocate separated planes so a memcpy is needed before
// display.  This code prodes a method a making ffmpeg allocate a single
// bit of memory for the frame when can then be refrence counted until
// display ahs finsihed with it.

#include "libavutil/frame.h"
#include "libavcodec/avcodec.h"

// "Opaque" pointer to whatever we are using as a buffer reference
typedef AVBufferRef * AVRpiZcRefPtr;

typedef struct AVRpiZcFrameGeometry
{
    unsigned int stride_y;
    unsigned int height_y;
    unsigned int stride_c;
    unsigned int height_c;
} AVRpiZcFrameGeometry;


AVRpiZcFrameGeometry av_rpi_zc_frame_geometry(
    const unsigned int video_width, const unsigned int video_height);

// Replacement fn for avctx->get_buffer2
// Should be set before calling avcodec_decode_open2
//
// N.B. in addition to to setting avctx->get_buffer2, avctx->refcounted_frames
// must be set to 1 as otherwise the buffer info is killed before being returned
// by avcodec_decode_video2.  Note also that this means that the AVFrame that is
// return must be manually derefed with av_frame_unref.  This should be done
// after av_rpi_zc_ref has been called.
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

#endif

