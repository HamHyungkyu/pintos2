#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include "vm/swap.h"
#include "vm/page.h"
#include "vm/frame.h"

static struct lock frame_lock;
static struct list frame_table;
static bool frame_less(struct list_elem *a, struct list_elem *b, void *aux);

void frame_init(){
    list_init(&frame_table);
    lock_init(&frame_lock);
}

void frame_allocate(struct stable_entry* entry, void *kpage){
    lock_acquire(&frame_lock);
    struct frame_entry *frame = malloc(sizeof(struct frame_entry));
    frame->thread = thread_current();
    frame->user_addr = entry->vaddr;
    frame->entry = entry;
    frame->kpage = kpage;
    if(list_size(&frame_table)){
        list_push_back(&frame_table, &frame->elem);
    }
    else{
        list_insert_ordered(&frame_table, &frame->elem, &frame_less, NULL);
    }

    lock_release(&frame_lock);
}

void frame_deallocate(void * user_adder){
    lock_acquire(&frame_lock);
    struct list_elem *a;
    struct frame_entry *entry_a;
    for(a = list_begin(&frame_table); a != list_end(&frame_table); a = a->next){
        entry_a = list_entry(a, struct frame_entry, elem);
        if(entry_a->user_addr == user_adder){
            list_remove(a);
            pagedir_clear_page(entry_a->thread->pagedir, entry_a->user_addr);
            palloc_free_page(entry_a->kpage);
            free(entry_a);
            break;
        }
    }
    lock_release(&frame_lock);
}

void frame_thread_remove(struct thread* t){
    lock_acquire(&frame_lock);
    struct list_elem *a;
    struct frame_entry *entry_a;
    for(a = list_begin(&frame_table); a != list_end(&frame_table); ){
        struct list_elem *b = a;
        a = a->next;        
        entry_a = list_entry(b, struct frame_entry, elem);
        if(entry_a->thread == t){
            list_remove(b);
            free(entry_a);
        }
    }
    lock_release(&frame_lock);
}

void frame_destory(){
    lock_acquire(&frame_lock);
    struct list_elem *a;
    struct frame_entry *entry_a;

    for(a = list_begin(&frame_table); a != list_end(&frame_table); a = a->next){
        entry_a = list_entry(a, struct frame_entry, elem);
        list_remove(a);
        palloc_free_page(entry_a->kpage);
        free(entry_a);
    }
    lock_release(&frame_lock);
}



void * frame_kpage(enum palloc_flags flags){
    lock_acquire(&frame_lock); 
    void * kpage = palloc_get_page(PAL_USER | flags);
    while(!kpage){
        struct frame_entry * frame_eviction = get_frame_eviction();
        struct thread * t = frame_eviction->thread;
         struct stable_entry * entry;
        entry = frame_eviction->entry;
        if(entry->file != NULL && entry->mapid != -1){
            stable_write_back(entry);
        }
        else{
            entry->swap_index = swap_out(frame_eviction->kpage);
        }
        entry->is_loaded = false;
        pagedir_clear_page(t->pagedir, frame_eviction->user_addr);
        palloc_free_page(frame_eviction->kpage);
        list_remove(&frame_eviction->elem);
        free(frame_eviction);
        kpage = palloc_get_page(PAL_USER | flags);
    }

    lock_release(&frame_lock);

    return kpage;
}

struct frame_entry * get_frame_eviction(){
    uint32_t * pagedir;
    struct list_elem *a;
    struct frame_entry *entry_a;
    int max_repeat = 3;
    while(max_repeat > 0){
        max_repeat --;
        for(a = list_begin(&frame_table); a != list_end(&frame_table); a = a->next){
            entry_a = list_entry(a, struct frame_entry, elem);
            pagedir = entry_a->thread->pagedir;
            struct stable_entry * sentry = entry_a->entry;
            if(sentry!= NULL){
                    if(pagedir_is_accessed(pagedir, entry_a->user_addr)){
                        pagedir_set_accessed(pagedir, entry_a->user_addr, false);
                        continue;
                    }
                    else{
                        break;
                    }
            }
        }
    }

    if(entry_a == NULL){
        PANIC("Evict Fail");
    }

    return entry_a;
}

static bool frame_less(struct list_elem *a, struct list_elem *b, void *aux){
    struct frame_entry *entry_a = list_entry(a, struct frame_entry, elem);
    struct frame_entry *entry_b = list_entry(b, struct frame_entry, elem);
    return entry_a->user_addr < entry_b->user_addr;
}