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

#ifndef AVCODEC_V4L2_REQ_DMABUFS_H
#define AVCODEC_V4L2_REQ_DMABUFS_H

#include <stddef.h>

struct dmabufs_ctl;
struct dmabuf_h;

struct dmabufs_ctl * dmabufs_ctl_new(void);
struct dmabufs_ctl * dmabufs_ctl_new_vidbuf_cached(void);
void dmabufs_ctl_unref(struct dmabufs_ctl ** const pdbsc);
struct dmabufs_ctl * dmabufs_ctl_ref(struct dmabufs_ctl * const dbsc);

// Need not preserve old contents
// On NULL return old buffer is freed
struct dmabuf_h * dmabuf_realloc(struct dmabufs_ctl * dbsc, struct dmabuf_h *, size_t size);

static inline struct dmabuf_h * dmabuf_alloc(struct dmabufs_ctl * dbsc, size_t size) {
    return dmabuf_realloc(dbsc, NULL, size);
}
/* Create from existing fd - dups(fd) */
struct dmabuf_h * dmabuf_import(int fd, size_t size);
/* Import an MMAP - return NULL if mapptr = MAP_FAIL */
struct dmabuf_h * dmabuf_import_mmap(void * mapptr, size_t size);

void * dmabuf_map(struct dmabuf_h * const dh);

/* flags from linux/dmabuf.h DMA_BUF_SYNC_xxx */
int dmabuf_sync(struct dmabuf_h * const dh, unsigned int flags);

int dmabuf_write_start(struct dmabuf_h * const dh);
int dmabuf_write_end(struct dmabuf_h * const dh);
int dmabuf_read_start(struct dmabuf_h * const dh);
int dmabuf_read_end(struct dmabuf_h * const dh);

int dmabuf_fd(const struct dmabuf_h * const dh);
/* Allocated size */
size_t dmabuf_size(const struct dmabuf_h * const dh);
/* Bytes in use */
size_t dmabuf_len(const struct dmabuf_h * const dh);
/* Set bytes in use */
void dmabuf_len_set(struct dmabuf_h * const dh, const size_t len);
void dmabuf_free(struct dmabuf_h * dh);

#endif
