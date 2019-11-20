#include "vm/page.h"


void* stable_alloc(void* addr, struct file* file, size_t offset, size_t read_bytes, mapid_t mapid){
    struct stable_entry *entry = malloc(sizeof(struct stable_entry));
    entry->vaddr = addr;
    entry->file = file;
    entry->offset = offset;
    entry->read_bytes = read_bytes;
    entry->is_loaded = false;
    entry->zero_bytes = PGSIZE - read_bytes;
    entry->mapid = mapid;
    return addr + PGSIZE;
}

bool stable_less(struct hash_elem *a, struct hash_elem *b){
    struct stable_entry * entry_a = hash_entry(a, struct stable_entry, elem); 
    struct stable_entry * entry_b = hash_entry(b, struct stable_entry, elem);   
    return entry_a->vaddr < entry_b->vaddr; 
}