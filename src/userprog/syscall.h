#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

typedef int pid_t;

void syscall_init (void);

void address_checking(int * p);

void halt(void);
void exit(int status);
pid_t exec(const char* cmd_line);
int wait(pid_t pid);
#endif /* userprog/syscall.h */
