// ffmpeg_sample.c
// Date: Sep 05, 2013
// Code based on a https://raw.github.com/phamquy/FFmpeg-tutorial-samples/master/tutorial01.c
// Tested on CentOS 5.7, GCC 4.1.2,FFMPEG 0.10.1
// libavcodec.so.53.60.100  libavdevice.so.53.4.100  libavfilter.so.2.60.100
// libavformat.so.53.31.100  libavutil.so.51.34.101  libswresample.so.0.6.100
// libswscale.so.2.1.100
//
// A small sample program that shows how to use libavformat to decode a video file and save it as Y frames.
//
// Build (assuming libavformat, libavcodec, libavutils are correctly installed on your system).
//
// gcc -o sample ffmpeg_sample.c -lavformat
//
// Run using
//
// ./sample myvideofile.mpg
//
// To view the generated output
//
// mplayer -demuxer rawvideo -rawvideo w=[LINESIZE]:h=[HEIGHT]:format=y8 out.raw -loop 0

#ifdef RPI
#define RPI_DISPLAY 1
#define RPI_ZERO_COPY 1
#endif

#include "config.h"
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>

#if HAVE_ISATTY
#if HAVE_IO_H
#include <io.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#endif

#include "libavformat/avformat.h"
#include "libavdevice/avdevice.h"
#include "libswresample/swresample.h"
#include "libavutil/opt.h"
#include "libavutil/channel_layout.h"
#include "libavutil/parseutils.h"
#include "libavutil/samplefmt.h"
#include "libavutil/fifo.h"
#include "libavutil/internal.h"
#include "libavutil/intreadwrite.h"
#include "libavutil/dict.h"
#include "libavutil/mathematics.h"
#include "libavutil/pixdesc.h"
#include "libavutil/avstring.h"
#include "libavutil/libm.h"
#include "libavutil/imgutils.h"
#include "libavutil/timestamp.h"
#include "libavutil/bprint.h"
#include "libavutil/time.h"
#include "libavutil/threadmessage.h"
#include "libavcodec/mathops.h"
#include "libavformat/os_support.h"

#ifdef RPI_DISPLAY
#include <bcm_host.h>
#include <interface/mmal/mmal.h>
#include <interface/mmal/mmal_parameters_camera.h>
#include <interface/mmal/mmal_buffer.h>
#include <interface/mmal/util/mmal_util.h>
#include <interface/mmal/util/mmal_default_components.h>
#include <interface/mmal/util/mmal_connection.h>
#include <interface/mmal/util/mmal_util_params.h>
#include "libavcodec/rpi_zc.h"
#endif


#ifdef RPI_DISPLAY

#define NUM_BUFFERS 4

static MMAL_COMPONENT_T* rpi_display = NULL;
static MMAL_POOL_T *rpi_pool = NULL;

static MMAL_POOL_T* display_alloc_pool(MMAL_PORT_T* port, size_t w, size_t h)
{
    MMAL_POOL_T* pool;
    size_t i;
    size_t size = (w*h*3)/2;
#ifdef RPI_ZERO_COPY
    mmal_port_parameter_set_boolean(port, MMAL_PARAMETER_ZERO_COPY, MMAL_TRUE); // Does this mark that the buffer contains a vc_handle?  Would have expected a vc_image?
    pool = mmal_port_pool_create(port, NUM_BUFFERS, 0);
    assert(pool);
#else
    pool = mmal_port_pool_create(port, NUM_BUFFERS, size);

    for (i = 0; i < NUM_BUFFERS; ++i)
    {
       MMAL_BUFFER_HEADER_T* buffer = pool->header[i];
       char * bufPtr = buffer->data;
       memset(bufPtr, i*30, w*h);
       memset(bufPtr+w*h, 128, (w*h)/2);
    }
#endif

    return pool;
}

static void display_cb_input(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) {
#ifdef RPI_ZERO_COPY
    av_rpi_zc_unref(buffer->user_data);
#endif
    mmal_buffer_header_release(buffer);
}

static void display_cb_control(MMAL_PORT_T *port,MMAL_BUFFER_HEADER_T *buffer) {
  mmal_buffer_header_release(buffer);
}

static MMAL_COMPONENT_T* display_init(size_t x, size_t y, size_t w, size_t h)
{
    MMAL_COMPONENT_T* display;
    int w2 = (w+31)&~31;
    int h2 = (h+15)&~15;
    MMAL_DISPLAYREGION_T region =
    {
        {MMAL_PARAMETER_DISPLAYREGION, sizeof(region)},
        .set = MMAL_DISPLAY_SET_LAYER | MMAL_DISPLAY_SET_FULLSCREEN | MMAL_DISPLAY_SET_DEST_RECT,
        .layer = 2,
        .fullscreen = 0,
        .dest_rect = {x, y, w, h}
    };
    bcm_host_init();  // TODO is this needed?
    mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_RENDERER, &display);
    assert(display);

    mmal_port_parameter_set(display->input[0], &region.hdr);

    MMAL_ES_FORMAT_T* format = display->input[0]->format;
    format->encoding = MMAL_ENCODING_I420;
    format->es->video.width = w2;
    format->es->video.height = h2;
    format->es->video.crop.x = 0;
    format->es->video.crop.y = 0;
    format->es->video.crop.width = w;
    format->es->video.crop.height = h;
    mmal_port_format_commit(display->input[0]);

    mmal_component_enable(display);

    rpi_pool = display_alloc_pool(display->input[0], w2, h2);

    mmal_port_enable(display->input[0],display_cb_input);
    mmal_port_enable(display->control,display_cb_control);

    printf("Allocated display %d %d\n",w,h);

    return display;
}

static void display_frame(MMAL_COMPONENT_T* const display, const AVFrame* const fr)
{
    if (!display || !rpi_pool)
        return;
    MMAL_BUFFER_HEADER_T* buf = mmal_queue_get(rpi_pool->queue);
    if (!buf) {
      // Running too fast so drop the frame
        printf("Drop frame\n");
      return;
    }
    assert(buf);
    buf->cmd = 0;
    buf->offset = 0; // Offset to valid data
    buf->flags = 0;
#ifdef RPI_ZERO_COPY
{
    const AVRpiZcRefPtr fr_buf = av_rpi_zc_ref(fr, 1);

    buf->user_data = fr_buf;
    buf->data = av_rpi_zc_vc_handle(fr_buf);
    buf->alloc_size =
        buf->length = av_rpi_zc_numbytes(fr_buf);
}
#else
{
    int w = fr->width;
    int h = fr->height;
    int w2 = (w+31)&~31;
    int h2 = (h+15)&~15;

    buf->length = (w2 * h2 * 3)/2;
    buf->user_data = NULL;

    //mmal_buffer_header_mem_lock(buf);
    memcpy(buf->data, fr->data[0], w2 * h);
    memcpy(buf->data+w2*h2, fr->data[1], w2 * h / 4);
    memcpy(buf->data+w2*h2*5/4, fr->data[2], w2 * h / 4);
    //mmal_buffer_header_mem_unlock(buf);
}
#endif

    if (mmal_port_send_buffer(display->input[0], buf) != MMAL_SUCCESS)
    {
        av_rpi_zc_unref(buf->user_data);
        mmal_buffer_header_release(buf);
    }
}

static void display_exit(MMAL_COMPONENT_T* display)
{
    if (display) {
        mmal_component_destroy(display);
    }
    if (rpi_pool) {
        mmal_port_pool_destroy(display->input[0], rpi_pool);
    }
}

#endif


static int readsave_frames(int videoStreamIdx
                , AVFormatContext *pFormatCtx
                , AVCodecContext  *pCodecCtx
                , AVCodec         *pCodec
)
{
    int             y, i;
    FILE           *pFile;
    AVPacket        packet;
    int             frameFinished;
    AVFrame        *pFrame;


    // Open file
    pFile=fopen("out.raw", "wb");
    if(pFile==NULL)
    {
        printf("Unable to open output file\n");
        return -1;
    }

    /// Allocate video frame
    pFrame = avcodec_alloc_frame();

    printf("\n");
    for(i=0; av_read_frame(pFormatCtx, &packet) >= 0;) {

        // Is this a packet from the video stream?
        if(packet.stream_index==videoStreamIdx) {
            i++;
            frameFinished = 0;

            /// Decode video frame
            avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);

            // Did we get a video frame?
            if(frameFinished) {

                printf("Frame [%d]: pts=%lld, pkt_pts=%lld, pkt_dts=%lld\n", i, pFrame->pts, pFrame->pkt_pts, pFrame->pkt_dts);
                printf("Buf[0]=%p, data[0]=%p\n", pFrame->buf[0], pFrame->data[0]);

#ifdef RPI_DISPLAY
                if (!rpi_display)
                    rpi_display = display_init(0,0,pFrame->width,pFrame->height);
                display_frame(rpi_display,pFrame);
                av_frame_unref(pFrame);
#else
                // Write pixel data
                for(y=0; y != pFrame->height; y++)
                    fwrite(pFrame->data[0]+y*pFrame->linesize[0], 1, pFrame->linesize[0], pFile);
#endif
            }
        }

        // Free the packet that was allocated by av_read_frame
        av_free_packet(&packet);
    }
    printf("\n");

    /// Free the Y frame
    av_free(pFrame);

    // Close file
    fclose(pFile);

    return 0;
}


int main(int argc, char *argv[]) {
    AVFormatContext *pFormatCtx;
    int             videoStreamIdx;
    AVCodecContext  *pCodecCtx;
    AVCodec         *pCodec;

    if(argc < 2) {
        printf("Please provide a movie file\n");
        return -1;
    }
    // Register all formats and codecs
    av_register_all();

    pFormatCtx = avformat_alloc_context();

    /// Open video file
    if(avformat_open_input(&pFormatCtx, argv[1], 0, NULL) != 0)
    {
        printf("avformat_open_input failed: Couldn't open file\n");
        return -1; // Couldn't open file
    }

    /// Retrieve stream information
    if(avformat_find_stream_info(pFormatCtx, NULL) < 0)
    {
        printf("avformat_find_stream_info failed: Couldn't find stream information\n");
        return -1; // Couldn't find stream information
    }

    /// Dump information about file onto standard error
    av_dump_format(pFormatCtx, 0, argv[1], 0);


    /// Find the first video stream
    {
        int i = 0;
        videoStreamIdx=-1;
        for(i=0; i != pFormatCtx->nb_streams; i++)
        {
            if(pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) { //CODEC_TYPE_VIDEO
                videoStreamIdx=i;
                break;
            }
        }
    }
    /// Check if video stream is found
    if(videoStreamIdx==-1)
        return -1; // Didn't find a video stream


    /// Get a pointer to the codec context for the video stream
    pCodecCtx = pFormatCtx->streams[videoStreamIdx]->codec;


    /// Find the decoder for the video stream
    pCodec = avcodec_find_decoder( pCodecCtx->codec_id);
    if(pCodec==NULL) {
        fprintf(stderr, "Unsupported codec!\n");
        return -1; // Codec not found
    }

#ifdef RPI_ZERO_COPY
    pCodecCtx->get_buffer2 = av_rpi_zc_get_buffer2;
    pCodecCtx->refcounted_frames = 1;
#endif

    /// Open codec
    if( avcodec_open2(pCodecCtx, pCodec, NULL) < 0 )
        return -1; // Could not open codec

    // Read frames and save them to disk
    if ( readsave_frames(videoStreamIdx, pFormatCtx, pCodecCtx, pCodec) < 0)
    {
        return -1;
    }

    /// Close the codec
    avcodec_close(pCodecCtx);

    /// Close the video file
    avformat_close_input(&pFormatCtx);

#ifdef RPI_DISPLAY
    display_exit(rpi_display);
#endif

    return 0;
}

