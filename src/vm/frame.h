#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <debug.h>
#include <list.h>
#include "threads/thread.h"


struct frame_entry {
    struct thread * thread;
    struct list_elem elem;
    void* user_addr;
    bool used; // init true then false
};
void frame_init();
void frame_allocate(void * user_addr);
void frame_deallocate(void * user_adder);
struct frame_entry * get_frame_eviction();

#endif