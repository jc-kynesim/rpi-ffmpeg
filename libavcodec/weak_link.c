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

#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>
#include "weak_link.h"

struct ff_weak_link_master {
    atomic_int ref_count;    /* 0 is single ref for easier atomics */
    pthread_rwlock_t lock;
    void * ptr;
};

static inline struct ff_weak_link_master * weak_link_x(struct ff_weak_link_client * c)
{
    return (struct ff_weak_link_master *)c;
}

struct ff_weak_link_master * ff_weak_link_new(void * p)
{
    struct ff_weak_link_master * w = malloc(sizeof(*w));
    if (!w)
        return NULL;
    atomic_init(&w->ref_count, 0);
    w->ptr = p;
    if (pthread_rwlock_init(&w->lock, NULL)) {
        free(w);
        return NULL;
    }
    return w;
}

static void weak_link_do_unref(struct ff_weak_link_master * const w)
{
    int n = atomic_fetch_sub(&w->ref_count, 1);
    if (n)
        return;

    pthread_rwlock_destroy(&w->lock);
    free(w);
}

// Unref & break link
void ff_weak_link_break(struct ff_weak_link_master ** ppLink)
{
    struct ff_weak_link_master * const w = *ppLink;
    if (!w)
        return;

    *ppLink = NULL;
    pthread_rwlock_wrlock(&w->lock);
    w->ptr = NULL;
    pthread_rwlock_unlock(&w->lock);

    weak_link_do_unref(w);
}

struct ff_weak_link_client* ff_weak_link_ref(struct ff_weak_link_master * w)
{
    if (!w)
        return NULL;
    atomic_fetch_add(&w->ref_count, 1);
    return (struct ff_weak_link_client*)w;
}

void ff_weak_link_unref(struct ff_weak_link_client ** ppLink)
{
    struct ff_weak_link_master * const w = weak_link_x(*ppLink);
    if (!w)
        return;

    *ppLink = NULL;
    weak_link_do_unref(w);
}

void * ff_weak_link_lock(struct ff_weak_link_client ** ppLink)
{
    struct ff_weak_link_master * const w = weak_link_x(*ppLink);

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
void ff_weak_link_unlock(struct ff_weak_link_client * c)
{
    struct ff_weak_link_master * const w = weak_link_x(c);
    if (w)
        pthread_rwlock_unlock(&w->lock);
}


