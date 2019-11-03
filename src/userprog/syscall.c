#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "threads/init.h"
#include "devices/shutdown.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "userprog/process.h"
#include "devices/input.h"
#include "threads/malloc.h"

static void syscall_handler(struct intr_frame *);
void address_checking(int * p);

void halt(void);
void exit(int status);
pid_t exec(const char* cmd_line);
int wait(pid_t pid);

bool create(const char * file, unsigned initial_size);
bool remove(const char * file);
int open(const char * file);
int filesize(int fd);
int read(int fd, uint8_t * buffer, unsigned size);
int write(int fd, const void *buffer, unsigned size);
void seek(int fd, unsigned position);
unsigned tell(int fd);
void close(int fd);

void file_checking(const char * file);
void thread_close(int status);

void syscall_init(void)
{
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler(struct intr_frame *f UNUSED)
{
  int *p = f->esp;

  address_checking(p);

  //printf ("System call: %d\n", *p);
  switch (*p)
  {
  //
  case SYS_HALT:
    halt();
    break;
  case SYS_EXIT:
    address_checking(p + 1);
    exit(*(p + 1));
    break;
  case SYS_EXEC:
    address_checking(p + 1);
    f->eax = exec(*(p + 1));
    break;
  case SYS_WAIT:
    address_checking(p + 1);
    f->eax = wait(*(p + 1));
    break;

  //
  case SYS_CREATE:
    address_checking(p + 2);
    f->eax = create(*(p + 1), *(p + 2));
    break;
  case SYS_REMOVE:
    f->eax = remove(*(p + 1));
    break;
  case SYS_OPEN:
    f->eax = open(*(p + 1));
    break;
  case SYS_FILESIZE:
    address_checking(p + 1);
    f->eax = filesize(*(p + 1));
    break;
  case SYS_READ:
    address_checking(p + 1);
    address_checking(p + 3);
    f->eax = read(*(p + 1), *(p + 2), *(p + 3));
    break;
  case SYS_WRITE:
    address_checking(p + 1);
    address_checking(p + 3);
    f->eax = write(*(p + 1), *(p + 2), *(p + 3));
    break;
  case SYS_SEEK:
    address_checking(p + 1);
    address_checking(p + 2);
    seek(*(p + 1), *(p + 2));
    break;
  case SYS_TELL:
    address_checking(p + 1);
    f->eax = tell(*(p + 1));
    break;
  case SYS_CLOSE:
    address_checking(p + 1);
    close(*(p + 1));
    break;
  }
}

void address_checking(int *p)
{
  if (!is_user_vaddr(p))
  {
    exit(-1);
  }
}

//
void halt(void)
{
  shutdown_power_off();
}

void exit(int status)
{
  thread_close(status);
  printf("%s: exit(%d)\n", thread_current()->name, status);
  thread_exit();
}

pid_t exec(const char *cmd_line)
{
  address_checking(cmd_line);

  char * ptr;
  char * cmd_line_cp = malloc(strlen(cmd_line) + 1);
  strlcpy(cmd_line_cp, cmd_line, strlen(cmd_line) + 1);
  char * file = strtok_r(cmd_line_cp, " ", &ptr);

  struct file * fp = filesys_open(file);
  free(cmd_line_cp);
  if(fp){
    return process_execute(cmd_line);
  }
  else{
    return -1;
  }
}

int wait(pid_t pid)
{
  return process_wait(pid);
}

//
bool create(const char * file, unsigned initial_size){
  address_checking(file);
  file_checking(file);
  return filesys_create(file, initial_size);
}

bool remove(const char * file){
  address_checking(file);
  file_checking(file);
  return filesys_remove(file);
}

int open(const char * file){
  address_checking(file);
  file_checking(file);

  struct file * fp = filesys_open(file);
  if(fp){
    for(int i = 3; i < 128; i++){
      if(thread_current()->fd[i] == NULL){
        thread_current()->fd[i] = fp;
        return i;
      }
    }
  }
  else{
    return -1;
  }
}

int filesize(int fd) {
  return file_length(thread_current()->fd[fd]);
}

int read(int fd, uint8_t * buffer, unsigned size){
  address_checking(buffer);
  if(fd == 0){
    for(int i = 0; i < size; i++){
      buffer[i] = input_getc();
    }
    return size;
  }
  else{
    if(thread_current()->fd[fd] != NULL){
      return file_read(thread_current()->fd[fd], buffer, size);
    }
    else{
      return -1;
    }
  }
}

int write(int fd, const void *buffer, unsigned size)
{
  address_checking(buffer);
  if (fd == 1)
  {
    putbuf(buffer, size);
    return size;
  }
  else{
    if(thread_current()->fd[fd] != NULL){
      return file_write(thread_current()->fd[fd], buffer, size);
    }
    else{
      return -1;
    }
  }
}

void seek(int fd, unsigned position) {
  file_seek(thread_current()->fd[fd], position);
}

unsigned tell(int fd){
  return file_tell(thread_current()->fd[fd]);
}

void close(int fd) {
  if(thread_current()->fd[fd] != NULL){
    thread_current()->fd[fd] = NULL;
    return file_close(thread_current()->fd[fd]);
  }
  else{
    exit(-1);
  }
}

//
void file_checking(const char * file){
  if(file == NULL){
    exit(-1);
  }
}

void thread_close(int status){
  thread_current()->exit_value = status;
  for(int i = 3; i < 128; i++){
    if(thread_current()->fd[i] != NULL){
      close(i);
    }
  }
}