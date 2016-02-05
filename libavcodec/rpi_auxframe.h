
struct AVFrame;
struct AVBufferRef;

uint8_t * rpi_gpu_buf_data_arm(const struct AVBufferRef * const buf);
struct AVBufferRef * rpi_gpu_buf_alloc(const unsigned int numbytes, const int flags);


#define RPI_AUX_FRAME_XBLK_SHIFT 4  // blk width 16
#define RPI_AUX_FRAME_XBLK_WIDTH (1 << RPI_AUX_FRAME_XBLK_SHIFT)

typedef struct RpiAuxframeDesc
{
    struct AVBufferRef * buf;
    uint8_t * data_y;
    uint8_t * data_c;
    unsigned int stride;
} RpiAuxframeDesc;

int rpi_auxframe_attach(struct AVFrame * const frame);

