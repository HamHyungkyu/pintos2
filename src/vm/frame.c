#include "threads/malloc.h"
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

struct frame_entry * get_frame_eviction(){
    struct list_elem *a;
    struct frame_entry *entry_a;

    for(a = list_begin(&frame_table); a != list_end(&frame_table); a = a->next){
        entry_a = list_entry(a, struct frame_entry, elem);
        if(entry_a->used){
            entry_a = false;
        }
        else{
            return entry_a;
        }
    }
}

static bool frame_less(struct list_elem *a, struct list_elem *b, void *aux){
    struct frame_entry *entry_a = list_entry(a, struct frame_entry, elem);
    struct frame_entry *entry_b = list_entry(b, struct frame_entry, elem);
    return entry_a->user_addr < entry_b->user_addr;
}