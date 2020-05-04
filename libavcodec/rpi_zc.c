#include "config.h"

#include "libavcodec/avcodec.h"
#include "rpi_mem.h"
#include "rpi_mailbox.h"
#include "rpi_zc.h"
#include "libavutil/avassert.h"
#include <pthread.h>

#include "libavutil/buffer_internal.h"

#pragma GCC diagnostic push
// Many many redundant decls in the header files
#pragma GCC diagnostic ignored "-Wredundant-decls"
#include <interface/vctypes/vc_image_types.h>
#include <interface/vcsm/user-vcsm.h>
#pragma GCC diagnostic pop

#define TRACE_ALLOC 0
#define DEBUG_ALWAYS_KEEP_LOCKED 0

struct ZcPoolEnt;

typedef struct ZcPool
{
    size_t numbytes;
    struct ZcPoolEnt * head;
    pthread_mutex_t lock;
} ZcPool;

typedef struct ZcPoolEnt
{
    size_t numbytes;

    unsigned int vcsm_handle;
    unsigned int vc_handle;
    void * map_arm;
    unsigned int map_vc;

    struct ZcPoolEnt * next;
    struct ZcPool * pool;
} ZcPoolEnt;

typedef struct ZcOldCtxVals
{
    int thread_safe_callbacks;
    int (*get_buffer2)(struct AVCodecContext *s, AVFrame *frame, int flags);
    void * opaque;
} ZcOldCtxVals;

typedef struct AVZcEnv
{
    unsigned int refcount;
    ZcOldCtxVals old;

    void * pool_env;
    av_rpi_zc_alloc_buf_fn_t * alloc_buf;
    av_rpi_zc_free_pool_fn_t * free_pool;

    unsigned int pool_size;
} ZcEnv;

typedef struct ZcUserBufEnv {
    void * v;
    const av_rpi_zc_buf_fn_tab_t * fn;
    size_t numbytes;
    int offset;
} ZcUserBufEnv;

#define ZC_BUF_INVALID  0
#define ZC_BUF_VALID    1
#define ZC_BUF_NEVER    2

typedef struct ZcBufEnv {
    GPU_MEM_PTR_T gmem;
    AVZcEnvPtr zc;
    int is_valid;
    AVBufferRef * user;
    AVRpiZcFrameGeometry geo;
    size_t size_y;
    size_t size_c;
    size_t size_pic;
    ssize_t offset;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} ZcBufEnv;






#define ALLOC_PAD       0
#define ALLOC_ROUND     0x1000
#define STRIDE_ROUND    64
#define STRIDE_OR       0

#define DEBUG_ZAP0_BUFFERS 0

static inline int av_rpi_is_sand_format(const int format)
{
    return (format >= AV_PIX_FMT_SAND128 && format <= AV_PIX_FMT_SAND64_16) ||
        (format == AV_PIX_FMT_RPI4_8 || format == AV_PIX_FMT_RPI4_10);
}

static inline int av_rpi_is_sand_frame(const AVFrame * const frame)
{
    return av_rpi_is_sand_format(frame->format);
}

//----------------------------------------------------------------------------
//
// Internal pool stuff

// Pool entry functions

static ZcPoolEnt * zc_pool_ent_alloc(ZcPool * const pool, const size_t req_size)
{
    ZcPoolEnt * const zp = av_mallocz(sizeof(ZcPoolEnt));

    // Round up to 4k & add 4k
    const unsigned int alloc_size = (req_size + ALLOC_PAD + ALLOC_ROUND - 1) & ~(ALLOC_ROUND - 1);

    if (zp == NULL) {
        av_log(NULL, AV_LOG_ERROR, "av_malloc(ZcPoolEnt) failed\n");
        goto fail0;
    }

    // The 0x80 here maps all pages here rather than waiting for lazy mapping
    // BEWARE that in GPU land a later unlock/lock pair will put us back into
    // lazy mode - which will also break cache invalidate calls.
    if ((zp->vcsm_handle = vcsm_malloc_cache(alloc_size, VCSM_CACHE_TYPE_HOST | 0x80, "ffmpeg_rpi_zc")) == 0)
    {
        av_log(NULL, AV_LOG_ERROR, "av_gpu_malloc_cached(%d) failed\n", alloc_size);
        goto fail1;
    }

#if TRACE_ALLOC
    printf("%s: Alloc %#x bytes @ h=%d\n", __func__, alloc_size, zp->vcsm_handle);
#endif

    zp->numbytes = alloc_size;
    zp->pool = pool;
    return zp;

fail1:
    av_free(zp);
fail0:
    return NULL;
}

static void zc_pool_ent_free(ZcPoolEnt * const zp)
{
#if TRACE_ALLOC
    printf("%s: Free %#x bytes @ h=%d\n", __func__, zp->numbytes, zp->vcsm_handle);
#endif

    if (zp->vcsm_handle != 0)
    {
        // VC addr & handle need no dealloc
        if (zp->map_arm != NULL)
            vcsm_unlock_hdl(zp->vcsm_handle);
        vcsm_free(zp->vcsm_handle);
    }
    av_free(zp);
}

//----------------------------------------------------------------------------
//
// Pool functions

static void zc_pool_free_ent_list(ZcPoolEnt * p)
{
    while (p != NULL)
    {
        ZcPoolEnt * const zp = p;
        p = p->next;
        zc_pool_ent_free(zp);
    }
}

static void zc_pool_flush(ZcPool * const pool)
{
    ZcPoolEnt * p = pool->head;
    pool->head = NULL;
    pool->numbytes = ~0U;
    zc_pool_free_ent_list(p);
}

static ZcPoolEnt * zc_pool_get_ent(ZcPool * const pool, const size_t req_bytes)
{
    ZcPoolEnt * zp = NULL;
    ZcPoolEnt * flush_list = NULL;
    size_t numbytes;

    pthread_mutex_lock(&pool->lock);

    numbytes = pool->numbytes;

    // If size isn't close then dump the pool
    // Close in this context means within 128k
    if (req_bytes > numbytes || req_bytes + 0x20000 < numbytes)
    {
        flush_list = pool->head;
        pool->head = NULL;
        pool->numbytes = numbytes = req_bytes;
    }
    else if (pool->head != NULL)
    {
        zp = pool->head;
        pool->head = zp->next;
    }

    pthread_mutex_unlock(&pool->lock);

    zc_pool_free_ent_list(flush_list);

    if (zp == NULL)
        zp = zc_pool_ent_alloc(pool, numbytes);

    return zp;
}

static void zc_pool_put_ent(ZcPoolEnt * const zp)
{
    ZcPool * const pool = zp == NULL ? NULL : zp->pool;
    if (zp != NULL)
    {
        pthread_mutex_lock(&pool->lock);
#if TRACE_ALLOC
        printf("%s: Recycle %#x, %#x\n", __func__, pool->numbytes, zp->numbytes);
#endif

        if (pool->numbytes == zp->numbytes)
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

static ZcPool *
zc_pool_new(void)
{
    ZcPool * const pool = av_mallocz(sizeof(*pool));
    if (pool == NULL)
        return NULL;

    pool->numbytes = -1;
    pool->head = NULL;
    pthread_mutex_init(&pool->lock, NULL);
    return pool;
}

static void
zc_pool_delete(ZcPool * const pool)
{
    if (pool != NULL)
    {
        pool->numbytes = -1;
        zc_pool_flush(pool);
        pthread_mutex_destroy(&pool->lock);
        av_free(pool);
    }
}

//============================================================================
//
// ZC implementation using above pool implementation
//
// Fn table fns...

static void zc_pool_free_v(void * v)
{
    zc_pool_put_ent(v);
}

static unsigned int zc_pool_ent_vcsm_handle_v(void * v)
{
    ZcPoolEnt * zp = v;
    return zp->vcsm_handle;
}

static unsigned int zc_pool_ent_vc_handle_v(void * v)
{
    ZcPoolEnt * zp = v;
    if (zp->vc_handle == 0)
    {
        if ((zp->vc_handle = vcsm_vc_hdl_from_hdl(zp->vcsm_handle)) == 0)
            av_log(NULL, AV_LOG_ERROR, "%s: Failed to map VCSM handle %d to VC handle\n",
                   __func__, zp->vcsm_handle);
    }
    return zp->vc_handle;
}

static void * zc_pool_ent_map_arm_v(void * v)
{
    ZcPoolEnt * zp = v;
    if (zp->map_arm == NULL)
    {
        if ((zp->map_arm = vcsm_lock(zp->vcsm_handle)) == NULL)
            av_log(NULL, AV_LOG_ERROR, "%s: Failed to map VCSM handle %d to ARM address\n",
                   __func__, zp->vcsm_handle);
    }
    return zp->map_arm;
}

static unsigned int zc_pool_ent_map_vc_v(void * v)
{
    ZcPoolEnt * zp = v;
    if (zp->map_vc == 0)
    {
        if ((zp->map_vc = vcsm_vc_addr_from_hdl(zp->vcsm_handle)) == 0)
            av_log(NULL, AV_LOG_ERROR, "%s: Failed to map VCSM handle %d to VC address\n",
                   __func__, zp->vcsm_handle);
    }
    return zp->map_vc;
}

static const av_rpi_zc_buf_fn_tab_t zc_pool_buf_fns = {
    .free        = zc_pool_free_v,
    .vcsm_handle = zc_pool_ent_vcsm_handle_v,
    .vc_handle   = zc_pool_ent_vc_handle_v,
    .map_arm     = zc_pool_ent_map_arm_v,
    .map_vc      = zc_pool_ent_map_vc_v,
};

// ZC Env fns

// Delete pool
// All buffers guaranteed freed by now
static void
zc_pool_delete_v(void * v)
{
    zc_pool_delete((ZcPool *)v);
    rpi_mem_gpu_uninit();
}

// Allocate a new ZC buffer
static AVBufferRef *
zc_pool_buf_alloc(void * v, size_t size, const AVRpiZcFrameGeometry * geo)
{
    ZcPool * const pool = v;
    ZcPoolEnt *const zp = zc_pool_get_ent(pool, size);
    AVBufferRef * buf;

    (void)geo;  // geo ignored here

    if (zp == NULL) {
        av_log(NULL, AV_LOG_ERROR, "zc_pool_alloc(%d) failed\n", size);
        goto fail0;
    }

    if ((buf = av_rpi_zc_buf(size, 0, zp, &zc_pool_buf_fns)) == NULL)
    {
        av_log(NULL, AV_LOG_ERROR, "av_rpi_zc_buf() failed\n");
        goto fail2;
    }

    return buf;

fail2:
    zc_pool_put_ent(zp);
fail0:
    return NULL;
}

// Init wrappers - the public fns

AVZcEnvPtr
av_rpi_zc_int_env_alloc(void * logctx)
{
    ZcEnv * zc;
    ZcPool * pool_env;

    if (rpi_mem_gpu_init(0) < 0)
        return NULL;

    if ((pool_env = zc_pool_new()) == NULL)
        goto fail1;

    if ((zc = av_rpi_zc_env_alloc(logctx, pool_env, zc_pool_buf_alloc, zc_pool_delete_v)) == NULL)
        goto fail2;

    return zc;

fail2:
    zc_pool_delete(pool_env);
fail1:
    rpi_mem_gpu_uninit();
    return NULL;
}

void
av_rpi_zc_int_env_freep(AVZcEnvPtr * zcp)
{
    const AVZcEnvPtr zc = *zcp;
    *zcp = NULL;
    if (zc != NULL)
        av_rpi_zc_env_release(zc);
}

//============================================================================
//
// Geometry
//
// This is a separate chunck to the rest

// Get mailbox fd - should be in a lock when called
// Rely on process close to close it
static int mbox_fd(void)
{
    static int fd = -1;
    if (fd != -1)
        return fd;
    return (fd = mbox_open());
}

AVRpiZcFrameGeometry av_rpi_zc_frame_geometry(
    const int format, const unsigned int video_width, const unsigned int video_height)
{
    static pthread_mutex_t sand_lock = PTHREAD_MUTEX_INITIALIZER;

    AVRpiZcFrameGeometry geo = {
        .format       = format,
        .video_width  = video_width,
        .video_height = video_height
    };

    switch (format)
    {
        case AV_PIX_FMT_YUV420P:
            geo.stride_y = ((video_width + 32 + STRIDE_ROUND - 1) & ~(STRIDE_ROUND - 1)) | STRIDE_OR;
            geo.stride_c = geo.stride_y / 2;
            geo.height_y = (video_height + 32 + 31) & ~31;
            geo.height_c = geo.height_y / 2;
            geo.planes_c = 2;
            geo.stripes = 1;
            geo.bytes_per_pel = 1;
            geo.stripe_is_yc = 1;
            break;

        case AV_PIX_FMT_YUV420P10:
            geo.stride_y = ((video_width * 2 + 64 + STRIDE_ROUND - 1) & ~(STRIDE_ROUND - 1)) | STRIDE_OR;
            geo.stride_c = geo.stride_y / 2;
            geo.height_y = (video_height + 32 + 31) & ~31;
            geo.height_c = geo.height_y / 2;
            geo.planes_c = 2;
            geo.stripes = 1;
            geo.bytes_per_pel = 2;
            geo.stripe_is_yc = 1;
            break;

        case AV_PIX_FMT_SAND128:
        case AV_PIX_FMT_RPI4_8:
        {
            const unsigned int stripe_w = 128;

            static VC_IMAGE_T img = {0};

            // Given the overhead of calling the mailbox keep a stashed
            // copy as we will almost certainly just want the same numbers again
            // but that means we need a lock
            pthread_mutex_lock(&sand_lock);

            if (img.width != video_width || img.height != video_height)
            {
                VC_IMAGE_T new_img = {
                    .type = VC_IMAGE_YUV_UV,
                    .width = video_width,
                    .height = video_height
                };

                mbox_get_image_params(mbox_fd(), &new_img);
                img = new_img;
            }

            geo.stride_y = stripe_w;
            geo.stride_c = stripe_w;
            geo.height_y = ((intptr_t)img.extra.uv.u - (intptr_t)img.image_data) / stripe_w;
            geo.height_c = img.pitch / stripe_w - geo.height_y;
            geo.stripe_is_yc = 1;
            if (geo.height_y * stripe_w > img.pitch)
            {
                // "tall" sand - all C blocks now follow Y
                geo.height_y = img.pitch / stripe_w;
                geo.height_c = geo.height_y;
                geo.stripe_is_yc = 0;
            }
            geo.planes_c = 1;
            geo.stripes = (video_width + stripe_w - 1) / stripe_w;
            geo.bytes_per_pel = 1;

            pthread_mutex_unlock(&sand_lock);
#if 0
            printf("Req: %dx%d: stride=%d/%d, height=%d/%d, stripes=%d, img.pitch=%d\n",
                   video_width, video_height,
                   geo.stride_y, geo.stride_c,
                   geo.height_y, geo.height_c,
                   geo.stripes, img.pitch);
#endif
            av_assert0((int)geo.height_y > 0 && (int)geo.height_c > 0);
            av_assert0(geo.height_y >= video_height && geo.height_c >= video_height / 2);
            break;
        }

        case AV_PIX_FMT_RPI4_10:
        {
            const unsigned int stripe_w = 128;  // bytes

            static pthread_mutex_t sand_lock = PTHREAD_MUTEX_INITIALIZER;
            static VC_IMAGE_T img = {0};

            // Given the overhead of calling the mailbox keep a stashed
            // copy as we will almost certainly just want the same numbers again
            // but that means we need a lock
            pthread_mutex_lock(&sand_lock);

            if (img.width != video_width || img.height != video_height)
            {
                VC_IMAGE_T new_img = {
                    .type = VC_IMAGE_YUV10COL,
                    .width = video_width,
                    .height = video_height
                };

                mbox_get_image_params(mbox_fd(), &new_img);
                img = new_img;
            }

            geo.stride_y = stripe_w;
            geo.stride_c = stripe_w;
            geo.height_y = ((intptr_t)img.extra.uv.u - (intptr_t)img.image_data) / stripe_w;
            geo.height_c = img.pitch / stripe_w - geo.height_y;
            geo.planes_c = 1;
            geo.stripes = ((video_width * 4 + 2) / 3 + stripe_w - 1) / stripe_w;
            geo.bytes_per_pel = 1;
            geo.stripe_is_yc = 1;

            pthread_mutex_unlock(&sand_lock);

#if 0
            printf("Req: %dx%d: stride=%d/%d, height=%d/%d, stripes=%d, img.pitch=%d\n",
                   video_width, video_height,
                   geo.stride_y, geo.stride_c,
                   geo.height_y, geo.height_c,
                   geo.stripes, img.pitch);
#endif
            av_assert0((int)geo.height_y > 0 && (int)geo.height_c > 0);
            av_assert0(geo.height_y >= video_height && geo.height_c >= video_height / 2);
            break;
        }

        case AV_PIX_FMT_SAND64_16:
        case AV_PIX_FMT_SAND64_10:
        {
            const unsigned int stripe_w = 128;  // bytes

            static pthread_mutex_t sand_lock = PTHREAD_MUTEX_INITIALIZER;
            static VC_IMAGE_T img = {0};

            // Given the overhead of calling the mailbox keep a stashed
            // copy as we will almost certainly just want the same numbers again
            // but that means we need a lock
            pthread_mutex_lock(&sand_lock);

             if (img.width != video_width || img.height != video_height)
            {
                VC_IMAGE_T new_img = {
                    .type = VC_IMAGE_YUV_UV_16,
                    .width = video_width,
                    .height = video_height
                };

                mbox_get_image_params(mbox_fd(), &new_img);
                img = new_img;
            }

            geo.stride_y = stripe_w;
            geo.stride_c = stripe_w;
            geo.height_y = ((intptr_t)img.extra.uv.u - (intptr_t)img.image_data) / stripe_w;
            geo.height_c = img.pitch / stripe_w - geo.height_y;
            geo.planes_c = 1;
            geo.stripes = (video_width * 2 + stripe_w - 1) / stripe_w;
            geo.bytes_per_pel = 2;
            geo.stripe_is_yc = 1;

            pthread_mutex_unlock(&sand_lock);
            break;
        }

        default:
            break;
    }
    return geo;
}

//============================================================================
//
// ZC Env fns
//
// Frame copy fns

static AVBufferRef * zc_copy(const AVZcEnvPtr zc,
    const AVFrame * const src)
{
    AVFrame dest_frame;
    AVFrame * const dest = &dest_frame;
    unsigned int i;
    uint8_t * psrc, * pdest;

    dest->format = src->format;
    dest->width = src->width;
    dest->height = src->height;

    if (av_rpi_zc_get_buffer(zc, dest) != 0 ||
        av_rpi_zc_resolve_frame(dest, ZC_RESOLVE_ALLOC_VALID) != 0)
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


static AVBufferRef * zc_420p10_to_sand128(const AVZcEnvPtr zc,
    const AVFrame * const src)
{
    assert(0);
    return NULL;
}


static AVBufferRef * zc_sand64_16_to_sand128(const AVZcEnvPtr zc,
    const AVFrame * const src, const unsigned int src_bits)
{
    assert(0);
    return NULL;
}

//----------------------------------------------------------------------------
//
// Public info extraction calls

static void zc_buf_env_free_cb(void * opaque, uint8_t * data);

static inline ZcBufEnv * pic_zbe_ptr(AVBufferRef *const buf)
{
    // Kludge where we check the free fn to check this is really
    // one of our buffers - can't think of a better way
    return buf == NULL || buf->buffer->free != zc_buf_env_free_cb ? NULL :
        av_buffer_get_opaque(buf);
}

static inline GPU_MEM_PTR_T * pic_gm_ptr(AVBufferRef * const buf)
{
    // As gmem is the first el NULL should be preserved
    return &pic_zbe_ptr(buf)->gmem;
}

unsigned int av_rpi_zc_vcsm_handle(const AVRpiZcRefPtr fr_ref)
{
    const GPU_MEM_PTR_T * const p = pic_gm_ptr(fr_ref);
    return p == NULL ? 0 : p->vcsm_handle;
}

int av_rpi_zc_vc_handle(const AVRpiZcRefPtr fr_ref)
{
    const GPU_MEM_PTR_T * const p = pic_gm_ptr(fr_ref);
    return p == NULL ? -1 : p->vc_handle;
}

int av_rpi_zc_offset(const AVRpiZcRefPtr fr_ref)
{
    const ZcBufEnv * const zbe = pic_zbe_ptr(fr_ref);
    return zbe == NULL ? 0 : zbe->offset;
}

int av_rpi_zc_length(const AVRpiZcRefPtr fr_ref)
{
    const ZcBufEnv * const zbe = pic_zbe_ptr(fr_ref);
    return zbe == NULL ? 0 : zbe->size_pic;
}

int av_rpi_zc_numbytes(const AVRpiZcRefPtr fr_ref)
{
    const GPU_MEM_PTR_T * const p = pic_gm_ptr(fr_ref);
    return p == NULL ? 0 : p->numbytes;
}

const AVRpiZcFrameGeometry * av_rpi_zc_geometry(const AVRpiZcRefPtr fr_ref)
{
    const ZcBufEnv * const zbe = pic_zbe_ptr(fr_ref);
    return zbe == NULL ? NULL : &zbe->geo;
}

AVRpiZcRefPtr av_rpi_zc_ref(void * const logctx, const AVZcEnvPtr zc,
    const AVFrame * const frame, const enum AVPixelFormat expected_format, const int maycopy)
{
    av_assert0(!maycopy || zc != NULL);

    if (frame->format != AV_PIX_FMT_YUV420P &&
        frame->format != AV_PIX_FMT_YUV420P10 &&
        !av_rpi_is_sand_frame(frame))
    {
        av_log(logctx, AV_LOG_WARNING, "%s: *** Format not SAND/YUV420P: %d\n", __func__, frame->format);
        return NULL;
    }

    if (frame->buf[1] != NULL || frame->format != expected_format)
    {
#if RPI_ZC_SAND_8_IN_10_BUF
        if (frame->format == AV_PIX_FMT_SAND64_10 && expected_format == AV_PIX_FMT_SAND128 && frame->buf[RPI_ZC_SAND_8_IN_10_BUF] != NULL)
        {
//            av_log(s, AV_LOG_INFO, "%s: --- found buf[4]\n", __func__);
            return av_buffer_ref(frame->buf[RPI_ZC_SAND_8_IN_10_BUF]);
        }
#endif

        if (maycopy)
        {
            if (frame->buf[1] != NULL)
                av_log(logctx, AV_LOG_INFO, "%s: *** Not a single buf frame: copying\n", __func__);
            else
                av_log(logctx, AV_LOG_INFO, "%s: *** Unexpected frame format %d: copying to %d\n", __func__, frame->format, expected_format);

            switch (frame->format)
            {
                case AV_PIX_FMT_YUV420P10:
                    return zc_420p10_to_sand128(zc, frame);

                case AV_PIX_FMT_SAND64_10:
                    return zc_sand64_16_to_sand128(zc, frame, 10);

                default:
                    return zc_copy(zc, frame);
            }
        }
        else
        {
            if (frame->buf[1] != NULL)
                av_log(logctx, AV_LOG_WARNING, "%s: *** Not a single buf frame: buf[1] != NULL\n", __func__);
            else
                av_log(logctx, AV_LOG_INFO, "%s: *** Unexpected frame format: %d != %d\n", __func__, frame->format, expected_format);
            return NULL;
        }
    }

    if (pic_gm_ptr(frame->buf[0]) == NULL)
    {
        if (maycopy)
        {
            av_log(logctx, AV_LOG_INFO, "%s: *** Not one of our buffers: copying\n", __func__);
            return zc_copy(zc, frame);
        }
        else
        {
            av_log(logctx, AV_LOG_WARNING, "%s: *** Not one of our buffers: NULL\n", __func__);
            return NULL;
        }
    }

    return av_buffer_ref(frame->buf[0]);
}

void av_rpi_zc_unref(AVRpiZcRefPtr fr_ref)
{
    if (fr_ref != NULL)
    {
        av_buffer_unref(&fr_ref);
    }
}

//----------------------------------------------------------------------------

// Extract user environment from an AVBufferRef
void * av_rpi_zc_buf_v(AVBufferRef * const buf)
{
    ZcBufEnv * const zbe = pic_zbe_ptr(buf);
    if (zbe != NULL && zbe->user != NULL)
    {
        const ZcUserBufEnv * const zub = (const ZcUserBufEnv *)zbe->user->data;
        return zub == NULL ? NULL : zub->v;
    }
    return NULL;
}

// AV buffer pre-free callback
static void zc_user_buf_free_cb(void * opaque, uint8_t * data)
{
    if (opaque != NULL)
    {
        ZcUserBufEnv * const zub = opaque;

        if (zub->fn->free)
            zub->fn->free(zub->v);

        av_free(zub);
    }
}

static void zc_buf_env_free_cb(void * opaque, uint8_t * data)
{
    if (opaque != NULL)
    {
        ZcBufEnv * const zbe = opaque;

        av_buffer_unref(&zbe->user);

        if (zbe->zc != NULL)
            av_rpi_zc_env_release(zbe->zc);

        pthread_cond_destroy(&zbe->cond);
        pthread_mutex_destroy(&zbe->lock);
        av_free(zbe);
    }
}


// Wrap the various ZC bits in an AV Buffer and resolve those things we want
// resolved now.
// Currently we resolve everything, but in future we might not
AVBufferRef * av_rpi_zc_buf(size_t numbytes, int addr_offset, void * v, const av_rpi_zc_buf_fn_tab_t * fn_tab)
{
    AVBufferRef *buf;
    ZcUserBufEnv * zub;

    if ((zub = av_malloc(sizeof(ZcUserBufEnv))) == NULL)
        return NULL;

    zub->fn = fn_tab;
    zub->v = v;
    zub->numbytes = numbytes;
    zub->offset = addr_offset;

    if ((buf = av_buffer_create((uint8_t*)zub, sizeof(*zub), zc_user_buf_free_cb, zub, 0)) == NULL)
    {
        av_log(NULL, AV_LOG_ERROR, "ZC: Failed av_buffer_create\n");
        av_free(zub);
        return NULL;
    }

    return buf;
}

int av_rpi_zc_resolve_buffer(AVBufferRef * const buf, const int alloc_mode)
{
    ZcBufEnv * const zbe = pic_zbe_ptr(buf);

    if (zbe == NULL)
        return AVERROR(EINVAL);

    if (alloc_mode == ZC_RESOLVE_FAIL && !zbe->is_valid)
        return AVERROR(EAGAIN);

    if (alloc_mode == ZC_RESOLVE_WAIT_VALID && !zbe->is_valid)
    {
        pthread_mutex_lock(&zbe->lock);
        while (!zbe->is_valid)
            pthread_cond_wait(&zbe->cond, &zbe->lock);
        pthread_mutex_unlock(&zbe->lock);
    }

    if (zbe->is_valid == ZC_BUF_NEVER)
        return AVERROR(EINVAL);

    // Do alloc if we need it
    if (zbe->user == NULL)
    {
        ZcEnv * const zc = zbe->zc;
        const ZcUserBufEnv * zub;

        av_assert0(alloc_mode == ZC_RESOLVE_ALLOC || alloc_mode == ZC_RESOLVE_ALLOC_VALID);

        if ((zbe->user = zc->alloc_buf(zc->pool_env, zbe->size_pic, &zbe->geo)) == NULL)
        {
            av_log(NULL, AV_LOG_ERROR, "rpi_get_display_buffer: Failed to get buffer from pool\n");
            goto fail;
        }
        zub = (const ZcUserBufEnv *)zbe->user->data;

        // Track

        zbe->offset = zub->offset;
        zbe->gmem.numbytes = zub->numbytes;
        if ((zbe->gmem.arm =  zub->fn->map_arm(zub->v)) == NULL)
        {
            av_log(NULL, AV_LOG_ERROR, "ZC: Failed to lock vcsm_handle %u\n", zbe->gmem.vcsm_handle);
            goto fail;
        }

        if ((zbe->gmem.vcsm_handle = zub->fn->vcsm_handle(zub->v)) == 0)
        {
            av_log(NULL, AV_LOG_ERROR, "ZC: Failed to get vcsm_handle\n");
            goto fail;
        }

        if ((zbe->gmem.vc_handle = zub->fn->vc_handle(zub->v)) == 0)
        {
            av_log(NULL, AV_LOG_ERROR, "ZC: Failed to get vc handle from vcsm_handle %u\n", zbe->gmem.vcsm_handle);
            goto fail;
        }
        if ((zbe->gmem.vc = zub->fn->map_vc(zub->v)) == 0)
        {
            av_log(NULL, AV_LOG_ERROR, "ZC: Failed to get vc addr from vcsm_handle %u\n", zbe->gmem.vcsm_handle);
            goto fail;
        }

        buf->buffer->data = zbe->gmem.arm + zbe->offset;
        buf->buffer->size = zbe->size_pic;

        // In this mode we shouldn't have anyone waiting for us
        // so no need to signal
        if (alloc_mode == ZC_RESOLVE_ALLOC_VALID)
            zbe->is_valid = 1;
    }

    // Just overwrite - no point in testing
    buf->data = zbe->gmem.arm + zbe->offset;
    buf->size = zbe->size_pic;
    return 0;

fail:
    av_buffer_unref(&zbe->user);
    return AVERROR(ENOMEM);
}

int av_rpi_zc_resolve_frame(AVFrame * const frame, const int may_alloc)
{
    int rv;

    // Do alloc if we need it
    if ((rv = av_rpi_zc_resolve_buffer(frame->buf[0], may_alloc)) != 0)
        return rv;

    // If we are a framebuf copy then the alloc can be done but we haven't
    // imported its results yet
    if (frame->data[0] == NULL)
    {
        const ZcBufEnv * const zbe = pic_zbe_ptr(frame->buf[0]);

        frame->linesize[0] = zbe->geo.stride_y;
        frame->linesize[1] = zbe->geo.stride_c;
        frame->linesize[2] = zbe->geo.stride_c;
        // abuse: linesize[3] = "stripe stride"
        // stripe_stride is NOT the stride between slices it is (that / geo.stride_y).
        // In a general case this makes the calculation an xor and multiply rather
        // than a divide and multiply
        if (zbe->geo.stripes > 1)
            frame->linesize[3] = zbe->geo.stripe_is_yc ? zbe->geo.height_y + zbe->geo.height_c : zbe->geo.height_y;

        frame->data[0] = frame->buf[0]->data;
        frame->data[1] = frame->data[0] + (zbe->geo.stripe_is_yc ? zbe->size_y : zbe->size_y * zbe->geo.stripes);
        if (zbe->geo.planes_c > 1)
            frame->data[2] = frame->data[1] + zbe->size_c;

        frame->extended_data = frame->data;
        // Leave extended buf alone
    }

    return 0;
}

int av_rpi_zc_set_valid_frame(AVFrame * const frame)
{
    ZcBufEnv * const zbe = pic_zbe_ptr(frame->buf[0]);

    if (zbe == NULL)
        return AVERROR(EINVAL);

    zbe->is_valid = ZC_BUF_VALID;
    pthread_cond_broadcast(&zbe->cond);

    return 0;
}

int av_rpi_zc_set_broken_frame(AVFrame * const frame)
{
    ZcBufEnv * const zbe = pic_zbe_ptr(frame->buf[0]);

    if (zbe == NULL)
        return AVERROR(EINVAL);

    zbe->is_valid = ZC_BUF_NEVER;
    pthread_cond_broadcast(&zbe->cond);

    return 0;
}

void av_rpi_zc_set_decoder_pool_size(ZcEnv *const zc, const unsigned int pool_size)
{
    zc->pool_size = pool_size;
}

unsigned int av_rpi_zc_get_decoder_pool_size(ZcEnv *const zc)
{
    return zc->pool_size;
}

int av_rpi_zc_get_buffer(ZcEnv *const zc, AVFrame * const frame)
{
#if 1
    ZcBufEnv * zbe = av_mallocz(sizeof(*zbe));

    for (unsigned int i = 0; i < AV_NUM_DATA_POINTERS; i++) {
        frame->buf[i] = NULL;
        frame->data[i] = NULL;
        frame->linesize[i] = 0;
    }

    if (zbe == NULL)
        return AVERROR(ENOMEM);

    if ((frame->buf[0] = av_buffer_create((uint8_t *)zbe, sizeof(*zbe), zc_buf_env_free_cb, zbe, 0)) == NULL)
    {
        av_free(zbe);
        return AVERROR(ENOMEM);
    }

    pthread_mutex_init(&zbe->lock, NULL);
    pthread_cond_init(&zbe->cond, NULL);
    zbe->zc = zc;
    atomic_fetch_add(&zc->refcount, 1);

    zbe->geo = av_rpi_zc_frame_geometry(frame->format, frame->width, frame->height);  // Note geometry for later use
    zbe->size_y = zbe->geo.stride_y * zbe->geo.height_y;
    zbe->size_c = zbe->geo.stride_c * zbe->geo.height_c;
    zbe->size_pic = (zbe->size_y + zbe->size_c * zbe->geo.planes_c) * zbe->geo.stripes;

#else
    const AVRpiZcFrameGeometry geo = av_rpi_zc_frame_geometry(frame->format, frame->width, frame->height);
    const unsigned int size_y = geo.stride_y * geo.height_y;
    const unsigned int size_c = geo.stride_c * geo.height_c;
    const unsigned int size_pic = (size_y + size_c * geo.planes_c) * geo.stripes;
    AVBufferRef * buf;
    unsigned int i;

//    printf("Do local alloc: format=%#x, %dx%d: %u\n", frame->format, frame->width, frame->height, size_pic);

    if ((buf = zc->alloc_buf(zc->pool_env, size_pic, &geo)) == NULL)
    {
        av_log(NULL, AV_LOG_ERROR, "rpi_get_display_buffer: Failed to get buffer from pool\n");
        return AVERROR(ENOMEM);
    }

    // Track
    atomic_fetch_add(&zc->refcount, 1);
    pic_zbe_ptr(buf)->zc = zc;

    for (i = 0; i < AV_NUM_DATA_POINTERS; i++) {
        frame->buf[i] = NULL;
        frame->data[i] = NULL;
        frame->linesize[i] = 0;
    }

    frame->buf[0] = buf;

    frame->linesize[0] = geo.stride_y;
    frame->linesize[1] = geo.stride_c;
    frame->linesize[2] = geo.stride_c;
    // abuse: linesize[3] = "stripe stride"
    // stripe_stride is NOT the stride between slices it is (that / geo.stride_y).
    // In a general case this makes the calculation an xor and multiply rather
    // than a divide and multiply
    if (geo.stripes > 1)
        frame->linesize[3] = geo.stripe_is_yc ? geo.height_y + geo.height_c : geo.height_y;

    frame->data[0] = buf->data;
    frame->data[1] = frame->data[0] + (geo.stripe_is_yc ? size_y : size_y * geo.stripes);
    if (geo.planes_c > 1)
        frame->data[2] = frame->data[1] + size_c;

    frame->extended_data = frame->data;
    // Leave extended buf alone

#if RPI_ZC_SAND_8_IN_10_BUF != 0
    // *** If we intend to use this for real we will want a 2nd buffer pool
    frame->buf[RPI_ZC_SAND_8_IN_10_BUF] = zc_pool_buf_alloc(&zc->pool, size_pic);  // *** 2 * wanted size - kludge
#endif
#endif

    return 0;
}

void av_rpi_zc_env_release(const AVZcEnvPtr zc)
{
    const int n = atomic_fetch_add(&zc->refcount, -1);
    if (n == 1)  // was 1, now 0
    {
        zc->free_pool(zc->pool_env);
        av_free(zc);
    }
}

AVZcEnvPtr av_rpi_zc_env_alloc(void * logctx,
                    void * pool_env,
                    av_rpi_zc_alloc_buf_fn_t * alloc_buf_fn,
                    av_rpi_zc_free_pool_fn_t * free_pool_fn)
{
    ZcEnv * zc;

    if ((zc = av_mallocz(sizeof(ZcEnv))) == NULL)
    {
        av_log(logctx, AV_LOG_ERROR, "av_rpi_zc_env_alloc: Context allocation failed\n");
        return NULL;
    }

    *zc = (ZcEnv){
        .refcount = ATOMIC_VAR_INIT(1),
        .pool_env = pool_env,
        .alloc_buf = alloc_buf_fn,
        .free_pool = free_pool_fn,
        .pool_size = 0
    };

    return zc;
}

//============================================================================
//
// External ZC initialisation

#define RPI_GET_BUFFER2 1


static int zc_get_buffer2(struct AVCodecContext *s, AVFrame *frame, int flags)
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
             av_rpi_is_sand_frame(frame))
    {
        if ((rv = av_rpi_zc_get_buffer(s->opaque, frame)) == 0)
            rv = av_rpi_zc_resolve_frame(frame, ZC_RESOLVE_ALLOC_VALID);
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

int av_rpi_zc_in_use(const struct AVCodecContext * const s)
{
    return s->get_buffer2 == zc_get_buffer2;
}

int av_rpi_zc_init2(struct AVCodecContext * const s,
                    void * pool_env,
                    av_rpi_zc_alloc_buf_fn_t * alloc_buf_fn,
                    av_rpi_zc_free_pool_fn_t * free_pool_fn)
{
    ZcEnv * zc;

    av_assert0(!av_rpi_zc_in_use(s));

    if ((zc = av_rpi_zc_env_alloc(s, pool_env, alloc_buf_fn, free_pool_fn)) == NULL)
        return AVERROR(ENOMEM);

    zc->old = (ZcOldCtxVals){
        .opaque = s->opaque,
        .get_buffer2 = s->get_buffer2,
        .thread_safe_callbacks = s->thread_safe_callbacks
    };

    s->opaque = zc;
    s->get_buffer2 = zc_get_buffer2;
    s->thread_safe_callbacks = 1;
    return 0;
}

void av_rpi_zc_uninit2(struct AVCodecContext * const s)
{
    ZcEnv * const zc = s->opaque;

    av_assert0(av_rpi_zc_in_use(s));

    s->get_buffer2 = zc->old.get_buffer2;
    s->opaque = zc->old.opaque;
    s->thread_safe_callbacks = zc->old.thread_safe_callbacks;

    av_rpi_zc_env_release(zc);
}

