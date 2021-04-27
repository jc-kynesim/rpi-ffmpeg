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

#include <memory.h>
#include <semaphore.h>
#include <pthread.h>

#include "v4l2_req_decode_q.h"

int decode_q_in_q(const req_decode_ent * const d)
{
    return d->in_q;
}

void decode_q_add(req_decode_q * const q, req_decode_ent * const d)
{
    pthread_mutex_lock(&q->q_lock);
    if (!q->head) {
        q->head = d;
        q->tail = d;
        d->prev = NULL;
    }
    else {
        q->tail->next = d;
        d->prev = q->tail;
        q->tail = d;
    }
    d->next = NULL;
    d->in_q = 1;
    pthread_mutex_unlock(&q->q_lock);
}

// Remove entry from Q - if head wake-up anything that was waiting
void decode_q_remove(req_decode_q * const q, req_decode_ent * const d)
{
    int try_signal = 0;

    if (!d->in_q)
        return;

    pthread_mutex_lock(&q->q_lock);
    if (d->prev)
        d->prev->next = d->next;
    else {
        try_signal = 1;  // Only need to signal if we were head
        q->head = d->next;
    }

    if (d->next)
        d->next->prev = d->prev;
    else
        q->tail = d->prev;

    // Not strictly needed but makes debug easier
    d->next = NULL;
    d->prev = NULL;
    d->in_q = 0;
    pthread_mutex_unlock(&q->q_lock);

    if (try_signal)
        pthread_cond_broadcast(&q->q_cond);
}

void decode_q_wait(req_decode_q * const q, req_decode_ent * const d)
{
    pthread_mutex_lock(&q->q_lock);

    while (q->head != d)
        pthread_cond_wait(&q->q_cond, &q->q_lock);

    pthread_mutex_unlock(&q->q_lock);
}

void decode_q_uninit(req_decode_q * const q)
{
    pthread_mutex_destroy(&q->q_lock);
    pthread_cond_destroy(&q->q_cond);
}

void decode_q_init(req_decode_q * const q)
{
    memset(q, 0, sizeof(*q));
    pthread_mutex_init(&q->q_lock, NULL);
    pthread_cond_init(&q->q_cond, NULL);
}


