#ifndef RPI_MAILBOX_H
#define RPI_MAILBOX_H

extern int mbox_open(void);
extern void mbox_close(int file_desc);

extern unsigned mem_lock(int file_desc, unsigned handle);
extern unsigned mem_unlock(int file_desc, unsigned handle);

#endif
