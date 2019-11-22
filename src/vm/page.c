#include "vm/page.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "userprog/process.h"
bool stable_less(struct hash_elem *a, struct hash_elem *b, void *aux);
size_t stable_hash_hash(struct hash_elem *a, void *aux);
struct stable_entry* stable_find_entry(void* addr);

void stable_init(struct hash *table){
    hash_init(table, &stable_hash_hash, &stable_less, NULL);
}

void* stable_alloc(void* addr, struct file* file, size_t offset, size_t read_bytes, mapid_t mapid){
    struct stable_entry *entry = malloc(sizeof(struct stable_entry));
    entry->vaddr = addr;
    entry->file = file_reopen(file);
    entry->offset = offset;
    entry->read_bytes = read_bytes;
    entry->is_loaded = false;
    entry->zero_bytes = PGSIZE - read_bytes;
    entry->mapid = mapid;
    return addr + PGSIZE;
}

bool stable_frame_alloc(void* addr, bool writable){
    struct stable_entry *entry = stable_find_entry(addr);
    struct thread *t = thread_current();
    if(entry == NULL)
        return false;
    uint8_t *kpage = palloc_get_page(PAL_USER);
    file_read_at(entry->file, kpage, entry->read_bytes, entry->offset);
    memset(kpage + entry->read_bytes, 0, entry->zero_bytes);
    if(!pagedir_set_page(t->pagedir, addr, kpage, writable)){
        palloc_free_page(kpage);
        return false;
    }
    return true;    
}

struct stable_entry* stable_find_entry(void* addr){
    struct hash *table = &thread_current()->stable;
    struct hash_iterator *i;
    struct stable_entry *entry;
    hash_first(&i, table);
    entry = hash_entry(i->elem, struct stable_entry, elem); 
    while (hash_next(i) != i->elem)
    {
        if(entry->vaddr <= addr && entry->vaddr + PGSIZE > addr){
            return entry;
        }
    }
    return NULL;
}



bool stable_less(struct hash_elem *a, struct hash_elem *b, void *aux){
    struct stable_entry * entry_a = hash_entry(a, struct stable_entry, elem); 
    struct stable_entry * entry_b = hash_entry(b, struct stable_entry, elem);   
    return entry_a->vaddr < entry_b->vaddr; 
}

size_t stable_hash_hash(struct hash_elem *a, void *aux){
    struct stable_entry *entry = hash_entry(a, struct stable_entry, elem); 
    return entry->vaddr;
}