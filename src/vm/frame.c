#include "threads/malloc.h"
#include "vm/frame.h"

static struct list frame_table;

void frame_init(){
    list_init(&frame_table);
}

void frame_allocate(void * user_addr){
    struct frame_entry *frame = malloc(sizeof(struct frame_entry));
    frame->thread = thread_current();
    frame->user_addr = user_addr;
    list_insert_ordered(&frame_table, &frame->elem, &frame_less, NULL);
}

void frame_deallocate(void * user_adder){
}

void frame_destory(){

}

static bool frame_less(struct list_elem *a, struct list_elem *b, void *aux){
    struct frame_entry *entry_a = list_entry(a, struct frame_entry, elem);
    struct frame_entry *entry_b = list_entry(b, struct frame_entry, elem);
    return entry_a->user_addr < entry_b->user_addr;
}