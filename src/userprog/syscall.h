#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#define PF_P 0x1    /* 0: not-present page. 1: access rights violation. */
#define PF_W 0x2    /* 0: read, 1: write. */
#define PF_U 0x4    /* 0: kernel, 1: user process. */

typedef int pid_t;

void syscall_init (void);
#endif /* userprog/syscall.h */
