#include "threads/thread.h"
#include "threads/palloc.h"
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
    frame->page = NULL;
    frame->used = true;
    struct frame_entry *entry_a = list_entry(&frame->elem, struct frame_entry, elem);
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
        free(entry_a);
    }
}

void * frame_kpage(){
    struct frame_entry * frame_eviction = get_frame_eviction();
    struct thread * t = frame_eviction->thread;

    /*
    if(pagedir_is_dirty(t->pagedir, frame_eviction->user_addr)){
    }
    */
    
    struct stable_entry* entry = stable_find_entry(t, frame_eviction->user_addr);
    entry->swap_index = swap_out(frame_eviction->page);
    entry->is_loaded = false;
    pagedir_clear_page(t->pagedir, frame_eviction->user_addr);
    palloc_free_page(frame_eviction->page);
    free(frame_eviction);

    return palloc_get_page(PAL_USER);
}

struct frame_entry * get_frame_eviction(){
    uint32_t * pagedir = thread_current()->pagedir;
    struct list_elem *a;
    struct frame_entry *entry_a;

    for(a = list_begin(&frame_table); a != list_end(&frame_table); a = a->next){
        entry_a = list_entry(a, struct frame_entry, elem);
        if(!entry_a->used){
            //Second Chance
            if(pagedir_is_accessed(pagedir, entry_a->user_addr)){
                pagedir_set_accessed(pagedir, entry_a->user_addr, false);
                continue;
            }
        }
        else{
            continue;
        }

        return entry_a;
    }

    for(a = list_begin(&frame_table); a != list_end(&frame_table); a = a->next){
        entry_a = list_entry(a, struct frame_entry, elem);
        if(!entry_a->used){
            if(pagedir_is_accessed(pagedir, entry_a->user_addr)){
                pagedir_set_accessed(pagedir, entry_a->user_addr, false);
                continue;
            }
        }
        else{
            continue;
        }

        return entry_a;
    }
}

static bool frame_less(struct list_elem *a, struct list_elem *b, void *aux){
    struct frame_entry *entry_a = list_entry(a, struct frame_entry, elem);
    struct frame_entry *entry_b = list_entry(b, struct frame_entry, elem);
    return entry_a->user_addr < entry_b->user_addr;
}