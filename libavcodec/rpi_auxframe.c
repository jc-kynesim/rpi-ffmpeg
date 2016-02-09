#include "config.h"

#include "libavutil/buffer.h"
#include "libavutil/frame.h"
#include "libavcodec/rpi_qpu.h"
#include "libavcodec/rpi_auxframe.h"

// Callback when buffer unrefed to zero
static void rpi_gpu_buf_delete(void *opaque, uint8_t *data)
{
    GPU_MEM_PTR_T *const gmem = opaque;
    printf("%s: data=%p\n", __func__, data);
    gpu_free(gmem);
}

uint8_t * rpi_gpu_buf_data_arm(const AVBufferRef * const buf)
{
    return buf->data;
}

AVBufferRef * rpi_gpu_buf_alloc(const unsigned int numbytes, const int flags)
{
    AVBufferRef * buf;
    GPU_MEM_PTR_T * const gmem = av_malloc(sizeof(GPU_MEM_PTR_T));
    int rv;


    if (gmem == NULL) {
        printf("av_malloc(GPU_MEM_PTR_T) failed\n");
        goto fail0;
    }

    if ((rv = gpu_malloc_cached(numbytes, gmem)) != 0)
    {
        printf("av_gpu_malloc_cached(%d) failed: rv=%d\n", numbytes, rv);
        goto fail1;
    }

    if ((buf = av_buffer_create(gmem->arm, numbytes, rpi_gpu_buf_delete, gmem, flags)) == NULL) {
        printf("av_buffer_create() failed\n");
        goto fail2;
    }

    memset(buf->data, 0x80, numbytes);

    printf("%s(%d): %p\n", __func__, numbytes, buf->data);

    return buf;

fail2:
    gpu_free(gmem);
fail1:
    av_free(gmem);
fail0:
    return NULL;
}


// Callback from av_buffer_unref
static void auxframe_desc_buffer_delete(void *opaque, uint8_t *data)
{
    RpiAuxframeDesc * const afd = opaque;

    av_buffer_unref(&afd->buf);
    av_free(afd);
}

int rpi_auxframe_attach(AVFrame * const frame)
{
    const unsigned int stride_af = frame->height << RPI_AUX_FRAME_XBLK_SHIFT;
    const unsigned int height_af_y = (frame->width + RPI_AUX_FRAME_XBLK_WIDTH - 1) >> RPI_AUX_FRAME_XBLK_SHIFT;
    // ?? 4:4:4 ??  Do we care ??
    const unsigned int height_af_c = (frame->width + RPI_AUX_FRAME_XBLK_WIDTH * 2 - 1) >> (RPI_AUX_FRAME_XBLK_SHIFT + 1);

    RpiAuxframeDesc *const afd = av_malloc(sizeof(RpiAuxframeDesc));

    if (afd == NULL)
        return -1;

    if ((afd->buf = rpi_gpu_buf_alloc(stride_af * (height_af_y + height_af_c), AV_BUFFER_FLAG_READONLY)) == NULL)
    {
        goto fail1;
    }

    afd->stride = stride_af;
    afd->data_y = rpi_gpu_buf_data_arm(afd->buf);
    afd->data_c = afd->data_y + stride_af * height_af_y;

    // Kludge into the bufer array at the end
    // This will be auto-freed / copied as required but shouldn't confuse
    // any other software (zero-copy) that checks buf[1] to see what sort
    // of frame allocation we have
    if ((frame->buf[AV_NUM_DATA_POINTERS - 1] =
         av_buffer_create((uint8_t *)afd, sizeof(*afd), auxframe_desc_buffer_delete, afd, AV_BUFFER_FLAG_READONLY)) == NULL)
    {
        goto fail2;
    }

    return 0;

fail2:
    av_buffer_unref(&afd->buf);
fail1:
    av_free(afd);
    return AVERROR(ENOMEM);
}


