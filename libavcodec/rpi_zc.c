#include "config.h"

#ifdef RPI
#include "rpi_qpu.h"
#include "rpi_zc.h"

static inline GPU_MEM_PTR_T * pic_gm_ptr(AVBufferRef * const buf)
{
    return buf == NULL ? NULL : av_buffer_get_opaque(buf);
}

AVRpiZcRefPtr av_rpi_zc_ref(const AVFrame * const frame)
{
    if (frame->buf[1] != NULL)
    {
        printf("%s: *** Not a single buf frame\n", __func__);
    }
    return av_buffer_ref(frame->buf[0]);
}

int av_rpi_zc_vc_handle(const AVRpiZcRefPtr fr_ref)
{
    const GPU_MEM_PTR_T * const p = pic_gm_ptr(fr_ref);
    return p == NULL ? -1 : p->vc_handle;
}

int av_rpi_zc_numbytes(const AVRpiZcRefPtr fr_ref)
{
    const GPU_MEM_PTR_T * const p = pic_gm_ptr(fr_ref);
    return p == NULL ? 0 : p->numbytes;
}

void av_rpi_zc_unref(AVRpiZcRefPtr fr_ref)
{
    if (fr_ref != NULL)
    {
        av_buffer_unref(&fr_ref);
    }
}


static void rpi_free_display_buffer(void *opaque, uint8_t *data)
{
    GPU_MEM_PTR_T *const gmem = opaque;
//    printf("%s: data=%p\n", __func__, data);
    av_gpu_free(gmem);
}

static int rpi_get_display_buffer(struct AVCodecContext * const s, AVFrame * const frame, const int flags)
{
    GPU_MEM_PTR_T * const gmem = av_malloc(sizeof(GPU_MEM_PTR_T));
    const unsigned int stride_y = (frame->width + 31) & ~31;
    const unsigned int height_y = (frame->height + 15) & ~15;
    const unsigned int size_y = stride_y * height_y;
    const unsigned int stride_c = stride_y / 2;
    const unsigned int height_c = height_y / 2;
    const unsigned int size_c = stride_c * height_c;
    const unsigned int size_pic = size_y + size_c * 2;
    AVBufferRef * buf;
    int rv;
    unsigned int i;

//    printf("Do local alloc: format=%#x, %dx%d: %u\n", frame->format, frame->width, frame->height, size_pic);

    if (gmem == NULL) {
        printf("av_malloc(GPU_MEM_PTR_T) failed\n");
        rv = AVERROR(ENOMEM);
        goto fail0;
    }

    if ((rv = av_gpu_malloc_cached(size_pic, gmem)) != 0)
    {
        printf("av_gpu_malloc_cached(%d) failed\n", size_pic);
        goto fail1;
    }

    if ((buf = av_buffer_create(gmem->arm, size_pic, rpi_free_display_buffer, gmem, 0)) == NULL) {
        printf("av_buffer_create() failed\n");
        rv = -1;
        goto fail2;
    }

    for (i = 0; i < AV_NUM_DATA_POINTERS; i++) {
        frame->buf[i] = NULL;
        frame->data[i] = NULL;
        frame->linesize[i] = 0;
    }

    frame->buf[0] = buf;
    frame->linesize[0] = stride_y;
    frame->linesize[1] = stride_c;
    frame->linesize[2] = stride_c;
    frame->data[0] = gmem->arm;
    frame->data[1] = frame->data[0] + size_y;
    frame->data[2] = frame->data[1] + size_c;
    frame->extended_data = frame->data;
    // Leave extended buf alone

    return 0;

fail2:
    av_gpu_free(gmem);
fail1:
    av_free(gmem);
fail0:
    return rv;
}


int av_rpi_zc_get_buffer2(struct AVCodecContext *s, AVFrame *frame, int flags)
{
    int rv;

    if ((s->codec->capabilities & AV_CODEC_CAP_DR1) == 0 ||
        frame->format != AV_PIX_FMT_YUV420P)
    {
//        printf("Do default alloc: format=%#x\n", frame->format);
        rv = avcodec_default_get_buffer2(s, frame, flags);
    }
    else
    {
        rv = rpi_get_display_buffer(s, frame, flags);
    }

#if 0
    printf("%s: %dx%d lsize=%d/%d/%d data=%p/%p/%p bref=%p/%p/%p opaque[0]=%p\n", __func__,
        frame->width, frame->height,
        frame->linesize[0], frame->linesize[1], frame->linesize[2],
        frame->data[0], frame->data[1], frame->data[2],
        frame->buf[0], frame->buf[1], frame->buf[2],
        av_buffer_get_opaque(frame->buf[0]));
#endif
    return rv;
}

#endif  // RPI

