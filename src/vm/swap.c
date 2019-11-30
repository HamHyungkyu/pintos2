//swap.c
#include "vm/swap.h"

struct block *swap_block;
struct bitmap *swap_map;

#define SECTORS_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

void swap_init(void){
	swap_block = block_get_role(BLOCK_SWAP);
	if(swap_block){
		swap_map = bitmap_create(block_size(swap_block) / SECTORS_PER_PAGE);
		if(swap_map){
			bitmap_set_all(swap_map, 0);
		}
		else{
			return;
		}
	}
	else{
		return;
	}
}

void swap_in(size_t swap_index, void * frame_page){
	if(swap_map && swap_block){
		bitmap_flip(swap_map, swap_index);
		for(size_t i = 0; i < SECTORS_PER_PAGE; i++){
			block_read(swap_block, swap_index * SECTORS_PER_PAGE + i, (uint8_t *) frame_page + i * BLOCK_SECTOR_SIZE);
			return;
		}
	}
	else{
		return;
	}
}

size_t swap_out(void * frame_page){
	if(swap_map && swap_block){
		size_t free_index = bitmap_scan_and_flip(swap_map, 0, 1, 0);
		for(size_t i = 0; i < SECTORS_PER_PAGE; i++){
			block_write(swap_block, free_index * SECTORS_PER_PAGE + i, (uint8_t *) frame_page + i * BLOCK_SECTOR_SIZE);
		}
		return free_index;
	}
	else{
		return -1;
	}
}