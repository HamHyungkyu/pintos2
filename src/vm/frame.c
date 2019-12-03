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

void frame_allocate(void * user_addr){
    lock_acquire(&frame_lock);
    struct frame_entry *frame = malloc(sizeof(struct frame_entry));
    frame->thread = thread_current();
    frame->user_addr = user_addr;
    if(list_size(&frame_table)){
        list_push_back(&frame_table, &frame->elem);
    }
    else{
        list_insert_ordered(&frame_table, &frame->elem, &frame_less, NULL);
    }
    lock_release(&frame_lock);
}

void frame_deallocate(void * user_adder){
    // lock_acquire(&frame_lock);

    struct list_elem *a;
    struct frame_entry *entry_a;
    for(a = list_begin(&frame_table); a != list_end(&frame_table); a = a->next){
        entry_a = list_entry(a, struct frame_entry, elem);
        if(entry_a->user_addr == user_adder){
            list_remove(a);
            palloc_free_page(pagedir_get_page(entry_a->thread->pagedir, entry_a->user_addr));
            free(entry_a);
            break;
        }
    }
    // lock_release(&frame_lock);
}

void frame_destory(){
    lock_acquire(&frame_lock);
    struct list_elem *a;
    struct frame_entry *entry_a;

    for(a = list_begin(&frame_table); a != list_end(&frame_table); a = a->next){
        entry_a = list_entry(a, struct frame_entry, elem);
        list_remove(a);
        palloc_free_page(pagedir_get_page(entry_a->thread->pagedir, entry_a->user_addr));
        free(entry_a);
    }
    lock_release(&frame_lock);

}

void * frame_kpage(enum palloc_flags flags){
    lock_acquire(&frame_lock);    
    void * kpage = palloc_get_page(PAL_USER | flags);

    while(!kpage){
        // printf("frame kpage1\n");
        struct frame_entry * frame_eviction = get_frame_eviction();
        struct thread * t = frame_eviction->thread;
        void * page = pagedir_get_page(t->pagedir, frame_eviction->user_addr);
        struct stable_entry * entry = stable_find_entry(t, frame_eviction->user_addr);
        // printf("fame %x\n", frame_eviction->user_addr);
        // if(entry->is_swap){
            entry->swap_index = swap_out(page);
        // }

        entry->is_loaded = false;
        list_remove(&frame_eviction->elem);
        pagedir_clear_page(t->pagedir, entry->vaddr);
        palloc_free_page(page);
        frame_deallocate(frame_eviction);

        kpage = palloc_get_page(PAL_USER | flags);
    }
    lock_release(&frame_lock);

    return kpage;
}

struct frame_entry * get_frame_eviction(){
    // lock_acquire(&frame_lock);

    uint32_t * pagedir;
    struct list_elem *a;
    struct frame_entry *entry_a;
    int max_repeat = 3;
    while(max_repeat > 0){
        max_repeat --;
        for(a = list_begin(&frame_table); a != list_end(&frame_table); a = a->next){
            entry_a = list_entry(a, struct frame_entry, elem);
            pagedir = entry_a->thread->pagedir;
            struct stable_entry * sentry = stable_find_entry(entry_a->thread, entry_a->user_addr);
            if(sentry!= NULL){
                if(!sentry->used){
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
    }

    // lock_release(&frame_lock);
    return entry_a;
}

static bool frame_less(struct list_elem *a, struct list_elem *b, void *aux){
    struct frame_entry *entry_a = list_entry(a, struct frame_entry, elem);
    struct frame_entry *entry_b = list_entry(b, struct frame_entry, elem);
    return entry_a->user_addr < entry_b->user_addr;
}