#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>
#include "v4l2_req_weak_link.h"

struct weak_link_master {
    atomic_int ref_count;    /* 0 is single ref for easier atomics */
    pthread_rwlock_t lock;
    void * ptr;
};

static inline struct weak_link_master * weak_link_x(struct weak_link_client * c)
{
    return (struct weak_link_master *)c;
}

struct weak_link_master * weak_link_new(void * p)
{
    struct weak_link_master * w = malloc(sizeof(*w));
    if (!w)
        return NULL;
    w->ptr = p;
    if (pthread_rwlock_init(&w->lock, NULL)) {
        free(w);
        return NULL;
    }
    return w;
}

static void weak_link_do_unref(struct weak_link_master * const w)
{
    int n = atomic_fetch_sub(&w->ref_count, 1);
    if (n)
        return;

    pthread_rwlock_destroy(&w->lock);
    free(w);
}

// Unref & break link
void weak_link_break(struct weak_link_master ** ppLink)
{
    struct weak_link_master * const w = *ppLink;
    if (!w)
        return;

    *ppLink = NULL;
    pthread_rwlock_wrlock(&w->lock);
    w->ptr = NULL;
    pthread_rwlock_unlock(&w->lock);

    weak_link_do_unref(w);
}

struct weak_link_client* weak_link_ref(struct weak_link_master * w)
{
    atomic_fetch_add(&w->ref_count, 1);
    return (struct weak_link_client*)w;
}

void weak_link_unref(struct weak_link_client ** ppLink)
{
    struct weak_link_master * const w = weak_link_x(*ppLink);
    if (!w)
        return;

    *ppLink = NULL;
    weak_link_do_unref(w);
}

void * weak_link_lock(struct weak_link_client ** ppLink)
{
    struct weak_link_master * const w = weak_link_x(*ppLink);

    if (!w)
        return NULL;

    if (pthread_rwlock_rdlock(&w->lock))
        goto broken;

    if (w->ptr)
        return w->ptr;

    pthread_rwlock_unlock(&w->lock);

broken:
    *ppLink = NULL;
    weak_link_do_unref(w);
    return NULL;
}

// Ignores a NULL c (so can be on the return path of both broken & live links)
void weak_link_unlock(struct weak_link_client * c)
{
    struct weak_link_master * const w = weak_link_x(c);
    if (w)
        pthread_rwlock_unlock(&w->lock);
}


