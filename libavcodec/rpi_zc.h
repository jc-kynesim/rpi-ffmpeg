/*
Copyright (c) 2018 Raspberry Pi (Trading) Ltd.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Authors: John Cox
*/

#ifndef LIBAVCODEC_RPI_ZC_H
#define LIBAVCODEC_RPI_ZC_H

// Zero-Copy frame code for RPi
// RPi needs Y/U/V planes to be contiguous for display.  By default
// ffmpeg will allocate separated planes so a memcpy is needed before
// display.  This code provides a method a making ffmpeg allocate a single
// bit of memory for the frame when can then be reference counted until
// display has finished with it.

// Frame buffer number in which to stuff an 8-bit copy of a 16-bit frame
// 0 disables
// *** This option still in development
//     Only works if SAO active
//     Allocates buffers that are twice the required size
#define RPI_ZC_SAND_8_IN_10_BUF  0

struct AVBufferRef;
struct AVFrame;
struct AVCodecContext;
enum AVPixelFormat;

// "Opaque" pointer to whatever we are using as a buffer reference
typedef struct AVBufferRef * AVRpiZcRefPtr;

struct AVZcEnv;
typedef struct AVZcEnv * AVZcEnvPtr;

typedef struct AVRpiZcFrameGeometry
{
    unsigned int stride_y;  // Luma stride (bytes)
    unsigned int height_y;  // Luma height (lines)
    unsigned int stride_c;  // Chroma stride (bytes)
    unsigned int height_c;  // Chroma stride (lines)
    unsigned int planes_c;  // Chroma plane count (U, V = 2, interleaved = 1)
    unsigned int stripes;   // Number of stripes (sand)
    unsigned int bytes_per_pel;
    int stripe_is_yc;       // A single stripe is Y then C (false for tall sand)

    int format;                 // Requested format
    unsigned int video_width;   // Requested width
    unsigned int video_height;  // Requested height
} AVRpiZcFrameGeometry;

// Get expected MMAL geometry for a given format, width & height
AVRpiZcFrameGeometry av_rpi_zc_frame_geometry(
    const int format,
    const unsigned int video_width, const unsigned int video_height);

//----------------------------------------------------------------------------
//
// Calls that extract info from a ZC frame whether internally or externally
// allocated

// Generate a ZC reference to the buffer(s) in this frame
// If the buffer doesn't appear to be one allocated by ZC
// then the behaviour depends on maycopy:
//   If maycopy=0 then return NULL
//   If maycopy=1 && the src frame is in a form where we can easily copy
//     the data, then allocate a new buffer and copy the data into it
//   Otherwise return NULL
// If maycopy == 0 then ZC may be NULL
AVRpiZcRefPtr av_rpi_zc_ref(void * const logging_context, const AVZcEnvPtr zc,
    const struct AVFrame * const frame, const enum AVPixelFormat expected_format, const int maycopy);

// Unreference the buffer refed/allocated by _zc_ref
// If fr_ref is NULL then this will NOP
void av_rpi_zc_unref(AVRpiZcRefPtr fr_ref);

// Get the vc_handle from the frame ref
// Returns -1 if ref doesn't look valid
int av_rpi_zc_vc_handle(const AVRpiZcRefPtr fr_ref);
// Get the vcsm_handle from the frame ref
// Returns 0 if ref doesn't look valid
unsigned int av_rpi_zc_vcsm_handle(const AVRpiZcRefPtr fr_ref);
// Get offset from the start of the memory referenced
// by the vc_handle to valid data
int av_rpi_zc_offset(const AVRpiZcRefPtr fr_ref);
// Length of buffer data
int av_rpi_zc_length(const AVRpiZcRefPtr fr_ref);
// Get the number of bytes allocated from the frame ref
// Returns 0 if ref doesn't look valid
int av_rpi_zc_numbytes(const AVRpiZcRefPtr fr_ref);
// Geometry this frame was allocated with
const AVRpiZcFrameGeometry * av_rpi_zc_geometry(const AVRpiZcRefPtr fr_ref);

//----------------------------------------------------------------------------
//
// Calls for external frame allocation

// Callbacks registered in av_rpi_zc_init2

// Callback to allocate a buf for a frame
// The frame itself is generated in the calling code
//
// Parameters:
//   pool_env  value passed to av-rpi_zc_init2
//   size      size wanted
//   geo       geometry of the frame to be allocated
// Returns:
//   NULL      Alloc failed
//   ptr       AVBufferBuf* of allocated buffer
//             In most cases av_rpi_zc_buf will be called by this function
//             and this will be the buf returned by that.
typedef AVBufferRef * av_rpi_zc_alloc_buf_fn_t(void * pool_env, size_t size,
                                               const AVRpiZcFrameGeometry * geo);

// Callback once ffmpeg is completely done with this pool
// Called once all allocated buffers have been derefed and ffmpegs ref to this
// pool has been dropped
typedef void av_rpi_zc_free_pool_fn_t(void * pool_env);

// Init ZC into a context
// Sets opaque, get_buffer2, thread_safe_callbacks
// Use if you want to allocate your own pools and/or create ZC buffers for
// all decoders
// RPI HEVC decoders will allocate appropriate VCSM buffers which can be taken
// apart by av_rpi_zc_xxx calls without this
int av_rpi_zc_init2(struct AVCodecContext * const s,
                    void * pool_env, av_rpi_zc_alloc_buf_fn_t * alloc_buf_fn,
                    av_rpi_zc_free_pool_fn_t * free_pool_fn);

// Free ZC from a context
void av_rpi_zc_uninit2(struct AVCodecContext * const s);

// Get minimum pool size in frames - valid by the time the first alloc request
// occurs.  Takes into account thread requests and DPB sizes derived from SPS
// rather than just adding a worst case DPB size.
unsigned int av_rpi_zc_get_decoder_pool_size(const AVZcEnvPtr zc);

typedef struct av_rpi_zc_buf_fn_tab_s {
    // This AVBuffer is being freed by ffmpeg - return memory
    // to external pool. Memory may be, but need not be, unmapped.
    // v is the ptr passed in av_rpi_zc_buf
    void (* free)(void * v);

    // Return appropriate handles / mappings
    // v is the ptr passed in av_rpi_zc_buf
    unsigned int (* vcsm_handle)(void * v);
    unsigned int (* vc_handle)(void * v);
    void * (* map_arm)(void * v);
    unsigned int (* map_vc)(void * v);
} av_rpi_zc_buf_fn_tab_t;

// Allocate a ZC AVBufferRef and set its callback table
// Doesn't take a buffer address directly - relies on callbacks to return
// addresses as they are required.  Mappings need not be generated until
// the map callbacks are called but they should persist from then until
// the buffer is freed.
//
// Parameters:
//   numbytes    Size of the buffer
//   addr_offset Offset to first usable byte of buffer (for alignment)
//               normally 0
//   v           Pointer passed to callbacks
//   fn_tab      Function table
AVBufferRef * av_rpi_zc_buf(size_t numbytes, int addr_offset, void * v, const av_rpi_zc_buf_fn_tab_t * fn_tab);

// Get v ptr set in in av_rpi_zc_buf
void * av_rpi_zc_buf_v(AVBufferRef * const buf);

//----------------------------------------------------------------------------
//
// Mostly internal calls but might possibly be wanted by outside code

void av_rpi_zc_int_env_freep(AVZcEnvPtr * zc);
AVZcEnvPtr av_rpi_zc_int_env_alloc(void * const logctx);
void av_rpi_zc_set_decoder_pool_size(const AVZcEnvPtr zc, const unsigned int pool_size);

// Test to see if the context is using zc (checks get_buffer2)
int av_rpi_zc_in_use(const struct AVCodecContext * const s);

// Get buffer generates placeholders for later alloc
int av_rpi_zc_get_buffer(const AVZcEnvPtr zc, AVFrame * const frame);
// Resolve actually does the alloc (noop if already alloced)
// Set data pointers on a buffer/frame that was copied before the alloc
// accured
#define ZC_RESOLVE_FAIL         0  // return error on invalid
#define ZC_RESOLVE_ALLOC        1  // alloc as invalid
#define ZC_RESOLVE_WAIT_VALID   2  // wait for valid
#define ZC_RESOLVE_ALLOC_VALID  3  // alloc as valid
int av_rpi_zc_resolve_buffer(AVBufferRef * const buf, const int may_alloc);
int av_rpi_zc_resolve_frame(AVFrame * const frame, const int may_alloc);

int av_rpi_zc_set_valid_frame(AVFrame * const frame);
int av_rpi_zc_set_broken_frame(AVFrame * const frame);




AVZcEnvPtr av_rpi_zc_env_alloc(void * logctx,
                    void * pool_env,
                    av_rpi_zc_alloc_buf_fn_t * alloc_buf_fn,
                    av_rpi_zc_free_pool_fn_t * free_pool_fn);
void av_rpi_zc_env_release(const AVZcEnvPtr zc);


#endif

