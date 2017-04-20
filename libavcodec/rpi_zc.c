#include "config.h"
#ifdef RPI
#include "rpi_qpu.h"
#include "rpi_zc.h"
#include "libavutil/avassert.h"
#include "libavutil/buffer_internal.h"

struct ZcPoolEnt;

typedef struct ZcPool
{
    int numbytes;
    unsigned int n;
    struct ZcPoolEnt * head;
    pthread_mutex_t lock;
} ZcPool;

typedef struct ZcPoolEnt
{
    // It is important that we start with gmem as other bits of code will expect to see that
    GPU_MEM_PTR_T gmem;
    unsigned int n;
    struct ZcPoolEnt * next;
    struct ZcPool * pool;
} ZcPoolEnt;

#if 1
//#define ALLOC_PAD       0x1000
#define ALLOC_PAD       0
#define ALLOC_ROUND     0x1000
//#define ALLOC_N_OFFSET  0x100
#define ALLOC_N_OFFSET  0
#define STRIDE_ROUND    0x80
#define STRIDE_OR       0x80
#else
#define ALLOC_PAD       0
#define ALLOC_ROUND     0x1000
#define ALLOC_N_OFFSET  0
#define STRIDE_ROUND    32
#define STRIDE_OR       0
#endif

#define DEBUG_ZAP0_BUFFERS 1


static ZcPoolEnt * zc_pool_ent_alloc(ZcPool * const pool, const unsigned int req_size)
{
    ZcPoolEnt * const zp = av_malloc(sizeof(ZcPoolEnt));

    // Round up to 4k & add 4k
    const unsigned int alloc_size = (req_size + ALLOC_PAD + ALLOC_ROUND - 1) & ~(ALLOC_ROUND - 1);

    if (zp == NULL) {
        av_log(NULL, AV_LOG_ERROR, "av_malloc(ZcPoolEnt) failed\n");
        goto fail0;
    }

    if (gpu_malloc_cached(alloc_size, &zp->gmem) != 0)
    {
        av_log(NULL, AV_LOG_ERROR, "av_gpu_malloc_cached(%d) failed\n", alloc_size);
        goto fail1;
    }

    zp->next = NULL;
    zp->pool = pool;
    zp->n = pool->n++;
    return zp;

fail1:
    av_free(zp);
fail0:
    return NULL;
}

static void zc_pool_ent_free(ZcPoolEnt * const zp)
{
    gpu_free(&zp->gmem);
    av_free(zp);
}

static void zc_pool_flush(ZcPool * const pool)
{
    ZcPoolEnt * p = pool->head;
    pool->head = NULL;
    while (p != NULL)
    {
        ZcPoolEnt * const zp = p;
        p = p->next;
        zc_pool_ent_free(zp);
    }
}

static ZcPoolEnt * zc_pool_alloc(ZcPool * const pool, const int numbytes)
{
    ZcPoolEnt * zp;
    pthread_mutex_lock(&pool->lock);

    if (numbytes != pool->numbytes)
    {
        zc_pool_flush(pool);
        pool->numbytes = numbytes;
    }

    if (pool->head != NULL)
    {
        zp = pool->head;
        pool->head = zp->next;
    }
    else
    {
        zp = zc_pool_ent_alloc(pool, numbytes);
    }

    pthread_mutex_unlock(&pool->lock);

    // Start with our buffer empty of preconceptions
//    rpi_cache_flush_one_gm_ptr(&zp->gmem, RPI_CACHE_FLUSH_MODE_INVALIDATE);

    return zp;
}

static void zc_pool_free(ZcPoolEnt * const zp)
{
    ZcPool * const pool = zp == NULL ? NULL : zp->pool;
    if (zp != NULL)
    {
        pthread_mutex_lock(&pool->lock);
        if (pool->numbytes == zp->gmem.numbytes)
        {
            zp->next = pool->head;
            pool->head = zp;
            pthread_mutex_unlock(&pool->lock);
        }
        else
        {
            pthread_mutex_unlock(&pool->lock);
            zc_pool_ent_free(zp);
        }
    }
}

static void
zc_pool_init(ZcPool * const pool)
{
    pool->numbytes = -1;
    pool->head = NULL;
    pthread_mutex_init(&pool->lock, NULL);
}

static void
zc_pool_destroy(ZcPool * const pool)
{
    pool->numbytes = -1;
    zc_pool_flush(pool);
    pthread_mutex_destroy(&pool->lock);
}


typedef struct AVZcEnv
{
    ZcPool pool;
} ZcEnv;

// Callback when buffer unrefed to zero
static void rpi_free_display_buffer(void *opaque, uint8_t *data)
{
    ZcPoolEnt *const zp = opaque;
//    printf("%s: data=%p\n", __func__, data);
    zc_pool_free(zp);
}

static inline GPU_MEM_PTR_T * pic_gm_ptr(AVBufferRef * const buf)
{
    // Kludge where we check the free fn to check this is really
    // one of our buffers - can't think of a better way
    return buf == NULL || buf->buffer->free != rpi_free_display_buffer ? NULL :
        av_buffer_get_opaque(buf);
}

AVRpiZcFrameGeometry av_rpi_zc_frame_geometry(
    const int format, const unsigned int video_width, const unsigned int video_height)
{
    AVRpiZcFrameGeometry geo;

    switch (format)
    {
        case AV_PIX_FMT_YUV420P:
            geo.stride_y = ((video_width + 32 + STRIDE_ROUND - 1) & ~(STRIDE_ROUND - 1)) | STRIDE_OR;
        //    geo.stride_y = ((video_width + 32 + 31) & ~31);
            geo.stride_c = geo.stride_y / 2;
        //    geo.height_y = (video_height + 15) & ~15;
            geo.height_y = (video_height + 32 + 31) & ~31;
            geo.height_c = geo.height_y / 2;
            geo.planes_c = 2;
            geo.stripes = 1;
            break;

        case AV_PIX_FMT_SAND128:
        {
            const unsigned int stripe_w = 128;

            av_assert0(video_width == 1920 && video_height == 1080);

            geo.stride_y = stripe_w;
            geo.stride_c = stripe_w;
            geo.height_y = 0x23000 / stripe_w;
            geo.height_c = (0x35800 - 0x23000) / stripe_w;
            geo.planes_c = 1;
            geo.stripes = (video_width + stripe_w - 1) / stripe_w;
            break;
        }

        default:
            memset(&geo, 0, sizeof(geo));
            break;
    }
    return geo;
}


static AVBufferRef * rpi_buf_pool_alloc(ZcPool * const pool, int size)
{
    ZcPoolEnt *const zp = zc_pool_alloc(pool, size);
    AVBufferRef * buf;
    intptr_t idata = (intptr_t)zp->gmem.arm;
#if ALLOC_N_OFFSET != 0
    intptr_t noff = (zp->n * ALLOC_N_OFFSET) & (ALLOC_PAD - 1);
#endif

    if (zp == NULL) {
        av_log(NULL, AV_LOG_ERROR, "zc_pool_alloc(%d) failed\n", size);
        goto fail0;
    }

#if ALLOC_N_OFFSET != 0
    idata = ((idata & ~(ALLOC_PAD - 1)) | noff) + (((idata & (ALLOC_PAD - 1)) > noff) ? ALLOC_PAD : 0);
#endif

#if DEBUG_ZAP0_BUFFERS
    memset((void*)idata, 0, size);
#endif

    if ((buf = av_buffer_create((void *)idata, size, rpi_free_display_buffer, zp, AV_BUFFER_FLAG_READONLY)) == NULL)
    {
        av_log(NULL, AV_LOG_ERROR, "av_buffer_create() failed\n");
        goto fail2;
    }

    return buf;

fail2:
    zc_pool_free(zp);
fail0:
    return NULL;
}

static int rpi_get_display_buffer(struct AVCodecContext * const s, AVFrame * const frame)
{
    ZcEnv *const zc = s->get_buffer_context;
    const AVRpiZcFrameGeometry geo = av_rpi_zc_frame_geometry(frame->format, frame->width, frame->height);
    const unsigned int size_y = geo.stride_y * geo.height_y;
    const unsigned int size_c = geo.stride_c * geo.height_c;
    const unsigned int size_pic = (size_y + size_c * geo.planes_c) * geo.stripes;
    AVBufferRef * buf;
    unsigned int i;

//    printf("Do local alloc: format=%#x, %dx%d: %u\n", frame->format, frame->width, frame->height, size_pic);

    if ((buf = rpi_buf_pool_alloc(&zc->pool, size_pic)) == NULL)
    {
        av_log(s, AV_LOG_ERROR, "rpi_get_display_buffer: Failed to get buffer from pool\n");
        return AVERROR(ENOMEM);
    }

    for (i = 0; i < AV_NUM_DATA_POINTERS; i++) {
        frame->buf[i] = NULL;
        frame->data[i] = NULL;
        frame->linesize[i] = 0;
    }

    frame->buf[0] = buf;

    frame->linesize[0] = geo.stride_y;
    frame->linesize[1] = geo.stride_c;
    frame->linesize[2] = geo.stride_c;
    if (geo.stripes > 1)
        frame->linesize[3] = geo.height_y + geo.height_c;      // abuse: linesize[3] = stripe stride

    frame->data[0] = buf->data;
    frame->data[1] = frame->data[0] + size_y;
    if (geo.planes_c > 1)
        frame->data[2] = frame->data[1] + size_c;

    frame->extended_data = frame->data;
    // Leave extended buf alone

    return 0;
}

#define RPI_GET_BUFFER2 1

int av_rpi_zc_get_buffer2(struct AVCodecContext *s, AVFrame *frame, int flags)
{
#if !RPI_GET_BUFFER2
    return avcodec_default_get_buffer2(s, frame, flags);
#else
    int rv;

    if ((s->codec->capabilities & AV_CODEC_CAP_DR1) == 0)
    {
//        printf("Do default alloc: format=%#x\n", frame->format);
        rv = avcodec_default_get_buffer2(s, frame, flags);
    }
    else if (frame->format == AV_PIX_FMT_YUV420P ||
             frame->format == AV_PIX_FMT_SAND128)
    {
        rv = rpi_get_display_buffer(s, frame);
    }
    else
    {
        rv = avcodec_default_get_buffer2(s, frame, flags);
    }

#if 0
    printf("%s: fmt:%d, %dx%d lsize=%d/%d/%d/%d data=%p/%p/%p bref=%p/%p/%p opaque[0]=%p\n", __func__,
        frame->format, frame->width, frame->height,
        frame->linesize[0], frame->linesize[1], frame->linesize[2], frame->linesize[3],
        frame->data[0], frame->data[1], frame->data[2],
        frame->buf[0], frame->buf[1], frame->buf[2],
        av_buffer_get_opaque(frame->buf[0]));
#endif
    return rv;
#endif
}


static AVBufferRef * zc_copy(struct AVCodecContext * const s,
    const AVFrame * const src)
{
    AVFrame dest_frame;
    AVFrame * const dest = &dest_frame;
    unsigned int i;
    uint8_t * psrc, * pdest;

    dest->width = src->width;
    dest->height = src->height;

    if (rpi_get_display_buffer(s, dest) != 0)
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


AVRpiZcRefPtr av_rpi_zc_ref(struct AVCodecContext * const s,
    const AVFrame * const frame, const int maycopy)
{
    assert(s != NULL);

    if (frame->format != AV_PIX_FMT_YUV420P &&
        frame->format != AV_PIX_FMT_SAND128)
    {
        av_log(s, AV_LOG_WARNING, "%s: *** Format not SAND/YUV420P: %d\n", __func__, frame->format);
        return NULL;
    }

    if (frame->buf[1] != NULL)
    {
        av_assert0(frame->format == AV_PIX_FMT_YUV420P);
        if (maycopy)
        {
            av_log(s, AV_LOG_INFO, "%s: *** Not a single buf frame: copying\n", __func__);
            return zc_copy(s, frame);
        }
        else
        {
            av_log(s, AV_LOG_WARNING, "%s: *** Not a single buf frame: NULL\n", __func__);
            return NULL;
        }
    }

    if (pic_gm_ptr(frame->buf[0]) == NULL)
    {
        if (maycopy)
        {
            av_log(s, AV_LOG_INFO, "%s: *** Not one of our buffers: copying\n", __func__);
            return zc_copy(s, frame);
        }
        else
        {
            av_log(s, AV_LOG_WARNING, "%s: *** Not one of our buffers: NULL\n", __func__);
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

int av_rpi_zc_offset(const AVRpiZcRefPtr fr_ref)
{
    const GPU_MEM_PTR_T * const p = pic_gm_ptr(fr_ref);
    return p == NULL ? 0 : fr_ref->data - p->arm;
}

int av_rpi_zc_length(const AVRpiZcRefPtr fr_ref)
{
    return fr_ref == NULL ? 0 : fr_ref->size;
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

AVZcEnvPtr av_rpi_zc_env_alloc(void)
{
    ZcEnv * const zc = av_mallocz(sizeof(ZcEnv));
    if (zc == NULL)
    {
        av_log(NULL, AV_LOG_ERROR, "av_rpi_zc_env_alloc: Context allocation failed\n");
        return NULL;
    }

    zc_pool_init(&zc->pool);
    return zc;
}

void av_rpi_zc_env_free(AVZcEnvPtr zc)
{
    if (zc != NULL)
    {
        zc_pool_destroy(&zc->pool); ;
        av_free(zc);
    }
}

int av_rpi_zc_init(struct AVCodecContext * const s)
{
    ZcEnv * const zc = av_rpi_zc_env_alloc();
    if (zc == NULL)
    {
        return AVERROR(ENOMEM);
    }

    s->get_buffer_context = zc;
    s->get_buffer2 = av_rpi_zc_get_buffer2;
    return 0;
}

void av_rpi_zc_uninit(struct AVCodecContext * const s)
{
    if (s->get_buffer2 == av_rpi_zc_get_buffer2)
    {
        ZcEnv * const zc = s->get_buffer_context;
        s->get_buffer2 = avcodec_default_get_buffer2;
        s->get_buffer_context = NULL;
        av_rpi_zc_env_free(zc);
    }
}

#endif  // RPI

