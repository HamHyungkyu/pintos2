#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <debug.h>
#include <list.h>
#include <sdtint.h>
#include "threads/thread.h"


struct frame_entry {
    struct thread * thread;
    struct list_elem elem;
    void* user_addr;
};