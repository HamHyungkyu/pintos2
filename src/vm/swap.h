//swap.h
#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <bitmap.h>
#include "devices/block.h"
#include "threads/vaddr.h"

void swap_init(void);
void swap_in(size_t swap_index, void * frame_page);
size_t swap_out(void * frame_page);

#endif