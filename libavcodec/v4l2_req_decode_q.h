#ifndef AVCODEC_V4L2_REQ_DECODE_Q_H
#define AVCODEC_V4L2_REQ_DECODE_Q_H

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

