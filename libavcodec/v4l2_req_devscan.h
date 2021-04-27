#ifndef _DEVSCAN_H_
#define _DEVSCAN_H_

#include <stdint.h>

struct devscan;
struct decdev;
enum v4l2_buf_type;

/* These return pointers to data in the devscan structure and so are vaild
 * for the lifetime of that
 */
const char *decdev_media_path(const struct decdev *const dev);
const char *decdev_video_path(const struct decdev *const dev);
enum v4l2_buf_type decdev_src_type(const struct decdev *const dev);
uint32_t decdev_src_pixelformat(const struct decdev *const dev);

const struct decdev *devscan_find(struct devscan *const scan, const uint32_t src_fmt_v4l2);

int devscan_build(void * const dc, struct devscan **pscan);
void devscan_delete(struct devscan **const pScan);

#endif
