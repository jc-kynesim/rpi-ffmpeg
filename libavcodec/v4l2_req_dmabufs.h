#ifndef DMABUFS_H
#define DMABUFS_H

#include <stddef.h>

struct dmabufs_ctl;
struct dmabuf_h;

struct dmabufs_ctl * dmabufs_ctl_new(void);
void dmabufs_ctl_delete(struct dmabufs_ctl ** const pdbsc);

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
