#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/eventfd.h>

#include "v4l2_req_pollqueue.h"
#include "v4l2_req_utils.h"


struct pollqueue;

enum polltask_state {
    POLLTASK_UNQUEUED = 0,
    POLLTASK_QUEUED,
    POLLTASK_RUNNING,
    POLLTASK_Q_KILL,
    POLLTASK_RUN_KILL,
};

struct polltask {
    struct polltask *next;
    struct polltask *prev;
    struct pollqueue *q;
    enum polltask_state state;

    int fd;
    short events;

    void (*fn)(void *v, short revents);
    void * v;

    uint64_t timeout; /* CLOCK_MONOTONIC time, 0 => never */
    sem_t kill_sem;
};

struct pollqueue {
    atomic_int ref_count;
    pthread_mutex_t lock;

    struct polltask *head;
    struct polltask *tail;

    bool kill;
    bool no_prod;
    int prod_fd;
    struct polltask *prod_pt;
    pthread_t worker;
};

struct polltask *polltask_new(struct pollqueue *const pq,
                              const int fd, const short events,
                  void (*const fn)(void *v, short revents),
                  void *const v)
{
    struct polltask *pt;

    if (!events)
        return NULL;

    pt = malloc(sizeof(*pt));
    if (!pt)
        return NULL;

    *pt = (struct polltask){
        .next = NULL,
        .prev = NULL,
        .q = pollqueue_ref(pq),
        .fd = fd,
        .events = events,
        .fn = fn,
        .v = v
    };

    sem_init(&pt->kill_sem, 0, 0);

    return pt;
}

static void pollqueue_rem_task(struct pollqueue *const pq, struct polltask *const pt)
{
    if (pt->prev)
        pt->prev->next = pt->next;
    else
        pq->head = pt->next;
    if (pt->next)
        pt->next->prev = pt->prev;
    else
        pq->tail = pt->prev;
    pt->next = NULL;
    pt->prev = NULL;
}

static void polltask_free(struct polltask * const pt)
{
    sem_destroy(&pt->kill_sem);
    free(pt);
}

static int pollqueue_prod(const struct pollqueue *const pq)
{
    static const uint64_t one = 1;
    return write(pq->prod_fd, &one, sizeof(one));
}

void polltask_delete(struct polltask **const ppt)
{
    struct polltask *const pt = *ppt;
    struct pollqueue * pq;
    enum polltask_state state;
    bool prodme;

    if (!pt)
        return;

    pq = pt->q;
    pthread_mutex_lock(&pq->lock);
    state = pt->state;
    pt->state = (state == POLLTASK_RUNNING) ? POLLTASK_RUN_KILL : POLLTASK_Q_KILL;
    prodme = !pq->no_prod;
    pthread_mutex_unlock(&pq->lock);

    if (state != POLLTASK_UNQUEUED) {
        if (prodme)
            pollqueue_prod(pq);
        while (sem_wait(&pt->kill_sem) && errno == EINTR)
            /* loop */;
    }

    // Leave zapping the ref until we have DQed the PT as might well be
    // legitimately used in it
    *ppt = NULL;
    polltask_free(pt);
    pollqueue_unref(&pq);
}

static uint64_t pollqueue_now(int timeout)
{
    struct timespec now;
    uint64_t now_ms;

    if (clock_gettime(CLOCK_MONOTONIC, &now))
        return 0;
    now_ms = (now.tv_nsec / 1000000) + (uint64_t)now.tv_sec * 1000 + timeout;
    return now_ms ? now_ms : (uint64_t)1;
}

void pollqueue_add_task(struct polltask *const pt, const int timeout)
{
    bool prodme = false;
    struct pollqueue * const pq = pt->q;

    pthread_mutex_lock(&pq->lock);
    if (pt->state != POLLTASK_Q_KILL && pt->state != POLLTASK_RUN_KILL) {
        if (pq->tail)
            pq->tail->next = pt;
        else
            pq->head = pt;
        pt->prev = pq->tail;
        pt->next = NULL;
        pt->state = POLLTASK_QUEUED;
        pt->timeout = timeout < 0 ? 0 : pollqueue_now(timeout);
        pq->tail = pt;
        prodme = !pq->no_prod;
    }
    pthread_mutex_unlock(&pq->lock);
    if (prodme)
        pollqueue_prod(pq);
}

static void *poll_thread(void *v)
{
    struct pollqueue *const pq = v;
    struct pollfd *a = NULL;
    size_t asize = 0;

    pthread_mutex_lock(&pq->lock);
    do {
        unsigned int i;
        unsigned int n = 0;
        struct polltask *pt;
        struct polltask *pt_next;
        uint64_t now = pollqueue_now(0);
        int timeout = -1;
        int rv;

        for (pt = pq->head; pt; pt = pt_next) {
            int64_t t;

            pt_next = pt->next;

            if (pt->state == POLLTASK_Q_KILL) {
                pollqueue_rem_task(pq, pt);
                sem_post(&pt->kill_sem);
                continue;
            }

            if (n >= asize) {
                asize = asize ? asize * 2 : 4;
                a = realloc(a, asize * sizeof(*a));
                if (!a) {
                    request_log("Failed to realloc poll array to %zd\n", asize);
                    goto fail_locked;
                }
            }

            a[n++] = (struct pollfd){
                .fd = pt->fd,
                .events = pt->events
            };

            t = (int64_t)(pt->timeout - now);
            if (pt->timeout && t < INT_MAX &&
                (timeout < 0 || (int)t < timeout))
                timeout = (t < 0) ? 0 : (int)t;
        }
        pthread_mutex_unlock(&pq->lock);

        if ((rv = poll(a, n, timeout)) == -1) {
            if (errno != EINTR) {
                request_log("Poll error: %s\n", strerror(errno));
                goto fail_unlocked;
            }
        }

        pthread_mutex_lock(&pq->lock);
        now = pollqueue_now(0);

        /* Prodding in this loop is pointless and might lead to
         * infinite looping
        */
        pq->no_prod = true;
        for (i = 0, pt = pq->head; i < n; ++i, pt = pt_next) {
            pt_next = pt->next;

            /* Pending? */
            if (a[i].revents ||
                (pt->timeout && (int64_t)(now - pt->timeout) >= 0)) {
                pollqueue_rem_task(pq, pt);
                if (pt->state == POLLTASK_QUEUED)
                    pt->state = POLLTASK_RUNNING;
                if (pt->state == POLLTASK_Q_KILL)
                    pt->state = POLLTASK_RUN_KILL;
                pthread_mutex_unlock(&pq->lock);

                /* This can add new entries to the Q but as
                 * those are added to the tail our existing
                 * chain remains intact
                */
                pt->fn(pt->v, a[i].revents);

                pthread_mutex_lock(&pq->lock);
                if (pt->state == POLLTASK_RUNNING)
                    pt->state = POLLTASK_UNQUEUED;
                if (pt->state == POLLTASK_RUN_KILL)
                    sem_post(&pt->kill_sem);
            }
        }
        pq->no_prod = false;

    } while (!pq->kill);

fail_locked:
    pthread_mutex_unlock(&pq->lock);
fail_unlocked:
    free(a);
    return NULL;
}

static void prod_fn(void *v, short revents)
{
    struct pollqueue *const pq = v;
    char buf[8];
    if (revents)
        read(pq->prod_fd, buf, 8);
    if (!pq->kill)
        pollqueue_add_task(pq->prod_pt, -1);
}

struct pollqueue * pollqueue_new(void)
{
    struct pollqueue *pq = malloc(sizeof(*pq));
    if (!pq)
        return NULL;
    *pq = (struct pollqueue){
        .ref_count = ATOMIC_VAR_INIT(0),
        .lock = PTHREAD_MUTEX_INITIALIZER,
        .head = NULL,
        .tail = NULL,
        .kill = false,
        .prod_fd = -1
    };

    pq->prod_fd = eventfd(0, EFD_NONBLOCK);
    if (pq->prod_fd == 1)
        goto fail1;
    pq->prod_pt = polltask_new(pq, pq->prod_fd, POLLIN, prod_fn, pq);
    if (!pq->prod_pt)
        goto fail2;
    pollqueue_add_task(pq->prod_pt, -1);
    if (pthread_create(&pq->worker, NULL, poll_thread, pq))
        goto fail3;
    // Reset ref count which will have been inced by the add_task
    atomic_store(&pq->ref_count, 0);
    return pq;

fail3:
    polltask_free(pq->prod_pt);
fail2:
    close(pq->prod_fd);
fail1:
    free(pq);
    return NULL;
}

static void pollqueue_free(struct pollqueue *const pq)
{
    void *rv;

    pthread_mutex_lock(&pq->lock);
    pq->kill = true;
    pollqueue_prod(pq);
    pthread_mutex_unlock(&pq->lock);

    pthread_join(pq->worker, &rv);
    polltask_free(pq->prod_pt);
    pthread_mutex_destroy(&pq->lock);
    close(pq->prod_fd);
    free(pq);
}

struct pollqueue * pollqueue_ref(struct pollqueue *const pq)
{
    atomic_fetch_add(&pq->ref_count, 1);
    return pq;
}

void pollqueue_unref(struct pollqueue **const ppq)
{
    struct pollqueue * const pq = *ppq;

    if (!pq)
        return;
    *ppq = NULL;

    if (atomic_fetch_sub(&pq->ref_count, 1) != 0)
        return;

    pollqueue_free(pq);
}



