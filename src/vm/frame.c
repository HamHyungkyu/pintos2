#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include "vm/swap.h"
#include "vm/page.h"
#include "vm/frame.h"

static struct list frame_table;
static bool frame_less(struct list_elem *a, struct list_elem *b, void *aux);

void frame_init(){
    list_init(&frame_table);
}

void frame_allocate(void * user_addr){
    struct frame_entry *frame = malloc(sizeof(struct frame_entry));
    frame->thread = thread_current();
    frame->user_addr = user_addr;
    if(list_size(&frame_table)){
        list_push_back(&frame_table, &frame->elem);
    }
    else{
        list_insert_ordered(&frame_table, &frame->elem, &frame_less, NULL);
    }
}

void frame_deallocate(void * user_adder){
    struct list_elem *a;
    struct frame_entry *entry_a;
    for(a = list_begin(&frame_table); a != list_end(&frame_table); a = a->next){
        entry_a = list_entry(a, struct frame_entry, elem);
        if(entry_a->user_addr == user_adder){
            list_remove(a);
            palloc_free_page(pagedir_get_page(thread_current()->pagedir, entry_a->user_addr));
            free(entry_a);
            break;
        }
    }
}

void frame_destory(){
    struct list_elem *a;
    struct frame_entry *entry_a;

    for(a = list_begin(&frame_table); a != list_end(&frame_table); a = a->next){
        entry_a = list_entry(a, struct frame_entry, elem);
        list_remove(a);
        palloc_free_page(pagedir_get_page(thread_current()->pagedir, entry_a->user_addr));
        free(entry_a);
    }
}

void * frame_kpage(enum palloc_flags flags){
    void * kpage = palloc_get_page(PAL_USER | flags);

    while(!kpage){
        //printf("frame kpage1\n");
        struct frame_entry * frame_eviction = get_frame_eviction();
        struct thread * t = frame_eviction->thread;
        void * page = pagedir_get_page(thread_current()->pagedir, frame_eviction->user_addr);
        struct stable_entry * entry = stable_find_entry(t, frame_eviction->user_addr);

        if(entry->is_swap){
            entry->swap_index = swap_out(page);
        }

        entry->is_loaded = false;
        list_remove(&frame_eviction->elem);
        pagedir_clear_page(t->pagedir, entry->vaddr);
        palloc_free_page(page);
        frame_deallocate(frame_eviction);

        kpage = palloc_get_page(PAL_USER | flags);
    }

    return kpage;
}

struct frame_entry * get_frame_eviction(){
    uint32_t * pagedir;
    struct list_elem *a;
    struct frame_entry *entry_a;

    for(a = list_begin(&frame_table); a != list_end(&frame_table); a = a->next){
        entry_a = list_entry(a, struct frame_entry, elem);
        pagedir = entry_a->thread->pagedir;
        struct stable_entry * sentry = stable_find_entry(thread_current(), entry_a->user_addr);
        if(sentry!= NULL){
            if(!sentry->used){
                if(pagedir_is_accessed(pagedir, entry_a->user_addr)){
                    pagedir_set_accessed(pagedir, entry_a->user_addr, false);
                    continue;
                }
                else{
                    //printf("eviction1\n");
                    return entry_a;
                }
            }
        }
    }

    for(a = list_begin(&frame_table); a != list_end(&frame_table); a = a->next){
        entry_a = list_entry(a, struct frame_entry, elem);
        pagedir = entry_a->thread->pagedir;
        struct stable_entry * sentry = stable_find_entry(thread_current(), entry_a->user_addr);
        if(sentry!= NULL){
            if(!sentry->used){
                if(pagedir_is_accessed(pagedir, entry_a->user_addr)){
                    pagedir_set_accessed(pagedir, entry_a->user_addr, false);
                    continue;
                }
                else{
                    //printf("eviction2\n");
                    return entry_a;
                }
            }
        }
    }
}

static bool frame_less(struct list_elem *a, struct list_elem *b, void *aux){
    struct frame_entry *entry_a = list_entry(a, struct frame_entry, elem);
    struct frame_entry *entry_b = list_entry(b, struct frame_entry, elem);
    return entry_a->user_addr < entry_b->user_addr;
}