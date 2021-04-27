#ifndef POLLQUEUE_H_
#define POLLQUEUE_H_

struct polltask;
struct pollqueue;

struct polltask *polltask_new(const int fd, const short events,
			      void (*const fn)(void *v, short revents),
			      void *const v);
void polltask_delete(struct polltask **const ppt);

void pollqueue_add_task(struct pollqueue *const pq, struct polltask *const pt,
			const int timeout);
struct pollqueue * pollqueue_new(void);
void pollqueue_delete(struct pollqueue **const ppq);

#endif /* POLLQUEUE_H_ */
