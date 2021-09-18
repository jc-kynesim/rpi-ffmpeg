#ifndef POLLQUEUE_H_
#define POLLQUEUE_H_

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

#endif /* POLLQUEUE_H_ */
