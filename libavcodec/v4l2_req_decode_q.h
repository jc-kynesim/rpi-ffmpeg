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

#ifndef AVCODEC_V4L2_REQ_DECODE_Q_H
#define AVCODEC_V4L2_REQ_DECODE_Q_H

#include <pthread.h>

typedef struct req_decode_ent {
    struct req_decode_ent * next;
    struct req_decode_ent * prev;
    int in_q;
} req_decode_ent;

typedef struct req_decode_q {
    pthread_mutex_t q_lock;
    pthread_cond_t q_cond;
    req_decode_ent * head;
    req_decode_ent * tail;
} req_decode_q;

int decode_q_in_q(const req_decode_ent * const d);
void decode_q_add(req_decode_q * const q, req_decode_ent * const d);
void decode_q_remove(req_decode_q * const q, req_decode_ent * const d);
void decode_q_wait(req_decode_q * const q, req_decode_ent * const d);
void decode_q_uninit(req_decode_q * const q);
void decode_q_init(req_decode_q * const q);

#endif

