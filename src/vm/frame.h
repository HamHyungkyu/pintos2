#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <debug.h>
#include <list.h>
#include "threads/palloc.h"

struct frame_entry {
    struct thread * thread;
    struct list_elem elem;
    void* user_addr;
    void* kpage;
};
void frame_init();
void frame_allocate(void * user_addr, void *kpage);
void frame_deallocate(void * user_adder);
void frame_thread_remove(struct thread* t);
struct frame_entry * get_frame_eviction();
void * frame_kpage(enum palloc_flags flags);

#endif