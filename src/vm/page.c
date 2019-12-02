#include "vm/page.h"
#include "vm/frame.h"
#include "threads/palloc.h"
#include "userprog/process.h"
#include "threads/interrupt.h"

bool stable_less(struct hash_elem *a, struct hash_elem *b, void *aux);
size_t stable_hash_hash(struct hash_elem *a, void *aux);
struct stable_entry* stable_stack_element(void* addr);
void stable_free_elem(struct hash_elem *elem, void *aux);
void stable_mapping_free(struct hash_elem *elem, void* aux);

void stable_init(struct hash *table){
    hash_init(table, &stable_hash_hash, &stable_less, NULL);
}

static bool
install_page(void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page(t->pagedir, upage) == NULL && pagedir_set_page(t->pagedir, upage, kpage, writable));
}

bool stable_stack_alloc(void *addr){
    void *sp;
    struct stable_entry *entry;
    uint8_t *kpage;
    bool success = false;
    if(PHYS_BASE - 2048 * PGSIZE <= addr && addr < PHYS_BASE){
        //printf("stack alloc\n");
        for(sp = PHYS_BASE - PGSIZE; sp >= pg_round_down(addr); sp -= PGSIZE){
            struct thread *t = thread_current();
            if(stable_is_exist(t, sp)){
                continue;
            }
            entry = stable_stack_element(sp);
            if(!entry){
                return false;
            }
            kpage = frame_kpage(PAL_USER);
            if (kpage != NULL)
            {
                success = install_page(sp, kpage, entry->writable);
                if(!success)
                {
                    palloc_free_page(kpage);
                    stable_free(entry);
                    return false;
                }

                if(intr_context()){
                    entry->used = false;
                }
            }
            else{
                stable_free(entry);
                return false;
            }
        }
        return success;
    }
    else{
        return false;
    }
}

struct stable_entry* stable_stack_element(void* addr){
    struct stable_entry *entry = malloc(sizeof(struct stable_entry));
    entry->vaddr = pg_round_down(addr);
    entry->offset = addr - entry->vaddr;
    entry->file = NULL;
    entry->read_bytes = PGSIZE;
    entry->is_loaded = true;
    entry->zero_bytes = 0;
    entry->writable = true;
    entry->mapid = -1;
    entry->is_swap = true;
    entry->used = true;
    hash_insert(&thread_current()->stable, &entry->elem);
    return entry;
}

struct stable_entry* stable_alloc(void* addr, struct file* file, size_t offset, size_t read_bytes, bool writable, mapid_t mapid){
    struct stable_entry *entry = malloc(sizeof(struct stable_entry));
    entry->vaddr = pg_round_down(addr);
    entry->offset = addr - entry->vaddr;
    entry->file = file_reopen(file);
    file_seek(entry->file, offset);
    entry->read_bytes = read_bytes;
    entry->is_loaded = false;
    entry->zero_bytes = PGSIZE - read_bytes;
    entry->writable = writable; 
    entry->mapid = mapid;
    entry->is_swap = false;
    entry->used = false;
    hash_insert(&thread_current()->stable, &entry->elem);
    return entry;
}

/* When Page fault exceptin, check valid address and allocate frame.
 */
bool stable_frame_alloc(void* addr){
    //printf("stable1\n");
    struct thread *t = thread_current();
    struct stable_entry *entry = stable_find_entry(t, addr);
    
    if(entry == NULL){
        return false;
    }
    entry->used = true;
    if(entry->is_loaded){
        return false;
    }
    //printf("stable2\n");
    uint8_t *kpage;

    if(entry->is_swap){
        kpage = frame_kpage(PAL_USER);

        if(!install_page(pg_round_down(addr), kpage, entry->writable)){
            palloc_free_page(kpage);
            return false;
        }
        //printf("is_swap\n");
        swap_in(entry->swap_index, entry->vaddr);
    }
    else{
        enum palloc_flags flags = PAL_USER;
        if(entry->read_bytes == 0){
            flags |= PAL_ZERO;
        }

        kpage = frame_kpage(flags);

        if(entry->read_bytes > 0){
            if(file_read(entry->file, kpage, entry->read_bytes) != (int) entry->read_bytes){
                palloc_free_page(kpage);
                return false;
            }
            memset(kpage + entry->read_bytes, 0, entry->zero_bytes);
        }

        if(!install_page(pg_round_down(addr), kpage, entry->writable)){
            palloc_free_page(kpage);
            return false;
        }
    }
    frame_allocate(pg_round_down(addr));
    entry->is_loaded = true;
    return true;    
}

/*  Find stable entry by user address. If invalid, return NULL
*/
struct stable_entry* stable_find_entry(struct thread *t, void* addr){
    struct stable_entry entry;
    entry.vaddr = pg_round_down(addr);
    struct hash_elem *elem = hash_find(&t->stable, &entry.elem);
    if(elem != NULL){
        return hash_entry(elem, struct stable_entry, elem);
    }
    return NULL;
}

bool stable_is_exist(struct thread* t, void *addr){
    struct stable_entry entry;
    entry.vaddr = pg_round_down(addr);
    if(hash_find(&t->stable, &entry.elem) != NULL){
        return true;
    }
    return false;
}

void stable_munmap(mapid_t mapping){
    struct hash *table = &thread_current()->stable;
    struct hash_iterator *i;
    hash_first(&i, table);
    struct hash_elem *elem;
    struct stable_entry *entry;
    table->aux = mapping;    
    hash_apply(table, &stable_mapping_free);
    table->aux = NULL;    
}

void stable_mapping_free(struct hash_elem *elem, void* aux){
    struct stable_entry *entry = hash_entry(elem, struct stable_entry, elem);
    if(entry->mapid == aux){
        stable_free(entry);
    }
}

void stable_free(struct stable_entry *entry){
    void * addr = pg_round_down(entry->vaddr);
    stable_write_back(entry);
    if(entry->is_loaded){
        pagedir_clear_page(thread_current()->pagedir, addr);
        frame_deallocate(addr);
    }
    hash_delete(&thread_current()->stable, &entry->elem);
    free(entry);
}

void stable_write_back(struct stable_entry *entry){
    void * addr = pg_round_down(entry->vaddr);
    if(entry-> is_loaded && entry->file != NULL && entry->mapid != -1){
        if(pagedir_is_dirty(thread_current()->pagedir, addr)){
            file_write_at(entry->file, addr, entry->read_bytes, entry->offset);
            pagedir_set_dirty (thread_current()->pagedir,  addr, false);
        }
        file_close(entry->file);
    }
}

void stable_exit(struct hash *hash){
    hash_destroy(hash, stable_func);
}

void stable_func (struct hash_elem *e, void *aux UNUSED)
{
  struct stable_entry *entry = hash_entry(e, struct stable_entry, elem);
  stable_free(entry);
}

void stable_free_elem(struct hash_elem *elem, void *aux){
    stable_write_back(hash_entry(elem, struct stable_entry, elem));
}


bool stable_less(struct hash_elem *a, struct hash_elem *b, void *aux){ 
    return stable_hash_hash(a, aux) <stable_hash_hash(b, aux); 
}

size_t stable_hash_hash(struct hash_elem *a, void *aux){
    struct stable_entry *entry = hash_entry(a, struct stable_entry, elem); 
    return entry->vaddr;
}