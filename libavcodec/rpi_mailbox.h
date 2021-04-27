#ifndef RPI_MAILBOX_H
#define RPI_MAILBOX_H

/* The image structure. */
typedef struct vc_image_extra_uv_s {
  void *u, *v;
  int vpitch;
} VC_IMAGE_EXTRA_UV_T;

typedef union {
    VC_IMAGE_EXTRA_UV_T uv;
//  VC_IMAGE_EXTRA_RGBA_T rgba;
//  VC_IMAGE_EXTRA_PAL_T pal;
//  VC_IMAGE_EXTRA_TF_T tf;
//  VC_IMAGE_EXTRA_BAYER_T bayer;
//  VC_IMAGE_EXTRA_MSBAYER_T msbayer;
//  VC_IMAGE_EXTRA_CODEC_T codec;
//  VC_IMAGE_EXTRA_OPENGL_T opengl;
} VC_IMAGE_EXTRA_T;


typedef struct VC_IMAGE_T {
  unsigned short                  type;           /* should restrict to 16 bits */
  unsigned short                  info;           /* format-specific info; zero for VC02 behaviour */
  unsigned short                  width;          /* width in pixels */
  unsigned short                  height;         /* height in pixels */
  int                             pitch;          /* pitch of image_data array in bytes */
  int                             size;           /* number of bytes available in image_data array */
  void                           *image_data;     /* pixel data */
  VC_IMAGE_EXTRA_T                extra;          /* extra data like palette pointer */
  void                           *metadata;       /* metadata header for the image */
  void                           *pool_object;    /* nonNULL if image was allocated from a vc_pool */
  int                             mem_handle;     /* the mem handle for relocatable memory storage */
  int                             metadata_size;  /* size of metadata of each channel in bytes */
  int                             channel_offset; /* offset of consecutive channels in bytes */
  uint32_t                        video_timestamp;/* 90000 Hz RTP times domain - derived from audio timestamp */
  uint8_t                         num_channels;   /* number of channels (2 for stereo) */
  uint8_t                         current_channel;/* the channel this header is currently pointing to */
  uint8_t                         linked_multichann_flag;/* Indicate the header has the linked-multichannel structure*/
  uint8_t                         is_channel_linked;     /* Track if the above structure is been used to link the header
                                                            into a linked-mulitchannel image */
  uint8_t                         channel_index;         /* index of the channel this header represents while
                                                            it is being linked. */
  uint8_t                         _dummy[3];      /* pad struct to 64 bytes */
} VC_IMAGE_T;

typedef int vc_image_t_size_check[(sizeof(VC_IMAGE_T) == 64) * 2 - 1];


extern int mbox_open(void);
extern void mbox_close(int file_desc);

int mbox_get_image_params(int fd, VC_IMAGE_T * img);

int mbox_request_clock(int fd);
int mbox_release_clock(int fd);

#endif
