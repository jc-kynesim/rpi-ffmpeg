/*
    Copyright (C) 2024  John Cox john.cox@raspberrypi.com

    Permission is hereby granted, free of charge, to any person
    obtaining a copy of this software and associated documentation
    files (the "Software"), to deal in the Software without
    restriction, including without limitation the rights to use, copy,
    modify, merge, publish, distribute, sublicense, and/or sell copies
    of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be
    included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
    HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
    WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
 */

#ifndef AVCODEC_V4L2_REQ_DEVSCAN_H
#define AVCODEC_V4L2_REQ_DEVSCAN_H

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
