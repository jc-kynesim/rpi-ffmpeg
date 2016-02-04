#include "config.h"

#ifdef RPI
#include "rpi_qpu.h"
#include "rpi_zc.h"
#include "libavutil/buffer_internal.h"

// Callback when buffer unrefed to zero
static void rpi_free_display_buffer(void *opaque, uint8_t *data)
{
    GPU_MEM_PTR_T *const gmem = opaque;
//    printf("%s: data=%p\n", __func__, data);
    gpu_free(gmem);
}

static inline GPU_MEM_PTR_T * pic_gm_ptr(AVBufferRef * const buf)
{
    // Kludge where we check the free fn to check this is really
    // one of our buffers - can't think of a better way
    return buf == NULL || buf->buffer->free != rpi_free_display_buffer ? NULL :
        av_buffer_get_opaque(buf);
}

static int rpi_get_display_buffer(AVFrame * const frame)
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

    if ((rv = gpu_malloc_cached(size_pic, gmem)) != 0)
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
    gpu_free(gmem);
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
        rv = rpi_get_display_buffer(frame);
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


static AVBufferRef * zc_copy(const AVFrame * const src)
{
    AVFrame dest_frame;
    AVFrame * const dest = &dest_frame;
    unsigned int i;
    uint8_t * psrc, * pdest;

    dest->width = src->width;
    dest->height = src->height;

    if (rpi_get_display_buffer(dest) != 0)
    {
        return NULL;
    }

    for (i = 0, psrc = src->data[0], pdest = dest->data[0];
         i != dest->height;
         ++i, psrc += src->linesize[0], pdest += dest->linesize[0])
    {
        memcpy(pdest, psrc, dest->width);
    }
    for (i = 0, psrc = src->data[1], pdest = dest->data[1];
         i != dest->height / 2;
         ++i, psrc += src->linesize[1], pdest += dest->linesize[1])
    {
        memcpy(pdest, psrc, dest->width / 2);
    }
    for (i = 0, psrc = src->data[2], pdest = dest->data[2];
         i != dest->height / 2;
         ++i, psrc += src->linesize[2], pdest += dest->linesize[2])
    {
        memcpy(pdest, psrc, dest->width / 2);
    }

    return dest->buf[0];
}


AVRpiZcRefPtr av_rpi_zc_ref(const AVFrame * const frame, const int maycopy)
{
    if (frame->format != AV_PIX_FMT_YUV420P)
    {
        printf("%s: *** Format not YUV420P: %d\n", __func__, frame->format);
        return NULL;
    }

    if (frame->buf[1] != NULL)
    {
        if (maycopy)
        {
            printf("%s: *** Not a single buf frame: copying\n", __func__);
            return zc_copy(frame);
        }
        else
        {
            printf("%s: *** Not a single buf frame: NULL\n", __func__);
            return NULL;
        }
    }

    if (pic_gm_ptr(frame->buf[0]) == NULL)
    {
        if (maycopy)
        {
            printf("%s: *** Not one of our buffers: copying\n", __func__);
            return zc_copy(frame);
        }
        else
        {
            printf("%s: *** Not one of our buffers: NULL\n", __func__);
            return NULL;
        }
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




#endif  // RPI

