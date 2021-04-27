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

#ifndef AVCODEC_V4L2_REQ_POLLQUEUE_H
#define AVCODEC_V4L2_REQ_POLLQUEUE_H

struct polltask;
struct pollqueue;

struct polltask *polltask_new(struct pollqueue *const pq,
			      const int fd, const short events,
			      void (*const fn)(void *v, short revents),
			      void *const v);
void polltask_delete(struct polltask **const ppt);

void pollqueue_add_task(struct polltask *const pt, const int timeout);
struct pollqueue * pollqueue_new(void);
void pollqueue_unref(struct pollqueue **const ppq);
struct pollqueue * pollqueue_ref(struct pollqueue *const pq);

#endif /* AVCODEC_V4L2_REQ_POLLQUEUE_H_ */
