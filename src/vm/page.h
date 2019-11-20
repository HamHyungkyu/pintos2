#ifndef VM_PAGE_H
#define VM_PAGE_H
#include <hash.h>
#include "filesys/file.h"

struct stable_entry {
    void* vaddr; // virtual page address
    size_t offset; // page offset
    bool isLoaded; // Is loaded on physical memory
    struct file * file; // File pointer for read file
    size_t readBytes; // To read bytes
    size_t zeroBytes; // size of extra 0 
    struct hash_elem elem; // hash table element
    mapid_t mapid; // mmap id
    size_t swap_index; // swap index
}



#endif