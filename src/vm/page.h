#ifndef VM_PAGE_H
#define VM_PAGE_H
#include <hash.h>
#include "lib/user/syscall.h"
#include <syscall-nr.h>
#include "threads/vaddr.h"
#include "filesys/file.h"

struct stable_entry {
    void* vaddr; // virtual page address
    size_t offset; // page offset
    bool is_loaded; // Is loaded on physical memory
    struct file * file; // File pointer for read file
    size_t read_bytes; // To read bytes
    size_t zero_bytes; // size of extra 0 
    struct hash_elem elem; // hash table element
    bool writable;
    mapid_t mapid; // mmap id
    size_t swap_index; // swap index
};

bool stable_stack_alloc(void *addr);
struct stable_entry* stable_alloc(void* addr, struct file* file, size_t offset, size_t read_bytes, bool writable, mapid_t mapid);
void stable_init(struct hash *table);
bool stable_frame_alloc(void* addr);
void stable_free(struct stable_entry *entry);
void stable_munmap(mapid_t mapping);

#endif