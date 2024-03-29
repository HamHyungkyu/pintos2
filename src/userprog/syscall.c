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
#include "vm/frame.h"
#include "vm/page.h"
#include <debug.h>

static int mapid;
static struct lock mapid_lock;

static void syscall_handler(struct intr_frame *);
void address_checking(int * p);
void buffer_checking(void *buffer, unsigned size);
void halt(void);
void exit(int status);
pid_t exec(const char* cmd_line);
int wait(pid_t pid);

bool create(const char * file, unsigned initial_size);
bool remove(const char * file);
int open(const char * file);
int filesize(int fd);
int read(int fd, void * buffer, unsigned size);
int write(int fd, const void *buffer, unsigned size);
void seek(int fd, unsigned position);
unsigned tell(int fd);
void close(int fd);


mapid_t mmap (int fd, void *addr);
void munmap (mapid_t mapping);

void file_checking(const char * file);
void thread_close(int status);

static struct lock file_lock;
void file_lock_acquire(void);
void file_lock_release(void);

void syscall_init(void)
{
  lock_init(&file_lock);
  lock_init(&mapid_lock);
  mapid = 0;
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler(struct intr_frame *f UNUSED)
{
  int *p = f->esp;
  //printf("%d", *p);

  address_checking(p);

  bool writeb = (f->error_code & PF_W) != 0;
  bool userb = (f->error_code & PF_U) != 0;

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
    file_lock_acquire();
    f->eax = open(*(p + 1));
    file_lock_release();
    break;
  case SYS_FILESIZE:
    address_checking(p + 1);
    f->eax = filesize(*(p + 1));
    break;
  case SYS_READ:
    address_checking(p + 1);
    address_checking(p + 3);
    file_lock_acquire();
    f->eax = read(*(p + 1), *(p + 2), *(p + 3));
    file_lock_release();
    break;
  case SYS_WRITE:
    address_checking(p + 1);
    address_checking(p + 3);
    //printf("sys_write\n");
    file_lock_acquire();
    f->eax = write(*(p + 1), *(p + 2), *(p + 3));
    file_lock_release();
    //printf("sys_write\n");
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
  case SYS_MMAP: 
    address_checking(p + 1);
    address_checking(p + 2);
    f->eax = mmap(*(p + 1), *(p + 2));
    break;
  case SYS_MUNMAP: 
    address_checking(p + 1);
    munmap(*(p + 1));
    break;
  }
}

void address_checking(int * p)
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
  if(file_lock.holder == thread_current()){
    file_lock_release();
  }
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
    file_close(fp);
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
      struct file* file_in_fd = thread_current()->fd[i];
      if(file_in_fd == NULL){
        thread_current()->fd[i] = fp;
        return i;
      } 
      else if(inode_get_inumber(file_get_inode(file_in_fd)) == inode_get_inumber(file_get_inode(fp))){
        file_close(fp);
        fp = file_reopen(file_in_fd);
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

int read(int fd, void * b, unsigned size){
  uint8_t *buffer = b;
  address_checking(buffer);
  buffer_checking(buffer, size);
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
  buffer_checking(buffer, size);

  if (fd == 1)
  {
    putbuf(buffer, size);
    return size;
  }
  else{
    if(thread_current()->fd[fd] != NULL){
      int buf = file_write(thread_current()->fd[fd], buffer, size);
      return buf;
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
    struct file * close_file = thread_current()->fd[fd];
    file_allow_write(close_file);
    thread_current()->fd[fd] = NULL;
    return file_close(close_file);
  }
  else{
    // printf("exit close");
    exit(-1);
  }
}

mapid_t mmap(int fd, void* addr){
  mapid_t mapping = mapid;
  lock_acquire(&mapid_lock);
  mapid++;
  lock_release(&mapid_lock);
  struct thread *t = thread_current();
  struct file * file = file_reopen(t->fd[fd]);
  void * upage = addr;
  off_t offset = 0;
  uint32_t read_bytes = file_length(file);
  uint32_t zero_bytes = PGSIZE - (read_bytes % PGSIZE);
 
  while(read_bytes > 0 || zero_bytes > 0){
    size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
    size_t page_zero_bytes = PGSIZE - page_read_bytes;
    if(read_bytes == 0 && zero_bytes == PGSIZE)
      break;
    if(upage == NULL || upage != pg_round_down(upage) ||stable_is_exist(t, upage)){
      munmap(mapping);
      return -1;
    }
    stable_alloc(upage, file, offset, page_read_bytes, true, mapping);

    offset += page_read_bytes;
    read_bytes -= page_read_bytes;
    zero_bytes -= page_zero_bytes;
    upage += PGSIZE;
  }
  file_close(file);
  return mapping;
}

void munmap(mapid_t mapping){
  stable_munmap(mapping);
}

//
void file_checking(const char * file){
  if(file == NULL){
    // printf("exit file checking");
    exit(-1);
  }
}

void thread_close(int status){
  thread_current()->exit_value = status;
  for(int i = 3; i < 131; i++){
    if(thread_current()->fd[i] != NULL){
      close(i);
    }
  }
}

void buffer_checking(void *b, unsigned size){
  void *buffer = b;
  int cunt = size / PGSIZE + 1;
  int zero_bytes = size % PGSIZE;
  for(int i = 0; i < cunt; i ++){
    struct stable_entry *entry = stable_find_entry(thread_current(), buffer);
    if(entry != NULL && !entry->is_loaded){
      stable_frame_alloc(entry->vaddr);
    }
    else if(entry == NULL){
      entry = stable_alloc(buffer, NULL, 0, PGSIZE, true, -1);
      stable_frame_alloc(entry->vaddr);
    }
    buffer += PGSIZE;
  }

}

void file_lock_acquire(void){
  lock_acquire(&file_lock);
}

void file_lock_release(void){
  lock_release(&file_lock);
}