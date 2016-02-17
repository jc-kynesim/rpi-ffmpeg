#ifndef RPI_MAILBOX_H
#define RPI_MAILBOX_H

extern int mbox_open(void);
extern void mbox_close(int file_desc);

extern unsigned get_version(int file_desc);
extern unsigned mem_alloc(int file_desc, unsigned size, unsigned align, unsigned flags);
extern unsigned mem_free(int file_desc, unsigned handle);
extern unsigned mem_lock(int file_desc, unsigned handle);
extern unsigned mem_unlock(int file_desc, unsigned handle);
extern void *mapmem_shared(unsigned base, unsigned size);
extern void *mapmem_private(unsigned base, unsigned size);
extern void unmapmem(void *addr, unsigned size);

extern unsigned execute_code(int file_desc, unsigned code, unsigned r0, unsigned r1, unsigned r2, unsigned r3, unsigned r4, unsigned r5);
extern unsigned execute_qpu(int file_desc, unsigned num_qpus, unsigned control, unsigned noflush, unsigned timeout);
extern void execute_multi(int file_desc,
   unsigned num_qpus, unsigned control, unsigned noflush, unsigned timeout,
   unsigned num_qpus_2, unsigned control_2, unsigned noflush_2, unsigned timeout_2,
   unsigned code, unsigned r0, unsigned r1, unsigned r2, unsigned r3, unsigned r4, unsigned r5,
   unsigned code_2, unsigned r0_2, unsigned r1_2, unsigned r2_2, unsigned r3_2, unsigned r4_2, unsigned r5_2);
extern unsigned qpu_enable(int file_desc, unsigned enable);

void qpu_stat_poke(void);

#endif
