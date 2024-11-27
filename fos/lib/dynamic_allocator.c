/*
 * dynamic_allocator.c
 *
 *  Created on: Sep 21, 2023
 *      Author: HP
 */
#include <inc/assert.h>
#include <inc/string.h>
#include "../inc/dynamic_allocator.h"


//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//

//=====================================================
// 1) GET BLOCK SIZE (including size of its meta data):
//=====================================================
__inline__ uint32 get_block_size(void* va)
{
	uint32 *curBlkMetaData = ((uint32 *)va - 1) ;
	return (*curBlkMetaData) & ~(0x1);
}

//===========================
// 2) GET BLOCK STATUS:
//===========================
__inline__ int8 is_free_block(void* va)
{
	uint32 *curBlkMetaData = ((uint32 *)va - 1) ;
	return (~(*curBlkMetaData) & 0x1) ;
}

//===========================
// 3) ALLOCATE BLOCK:
//===========================

void *alloc_block(uint32 size, int ALLOC_STRATEGY)
{
	void *va = NULL;
	switch (ALLOC_STRATEGY)
	{
	case DA_FF:
		va = alloc_block_FF(size);
		break;
	case DA_NF:
		va = alloc_block_NF(size);
		break;
	case DA_BF:
		va = alloc_block_BF(size);
		break;
	case DA_WF:
		va = alloc_block_WF(size);
		break;
	default:
		cprintf("Invalid allocation strategy\n");
		break;
	}
	return va;
}

//===========================
// 4) PRINT BLOCKS LIST:
//===========================

void print_blocks_list(struct MemBlock_LIST list)
{
	cprintf("=========================================\n");
	struct BlockElement* blk ;
	cprintf("\nDynAlloc Blocks List:\n");
	LIST_FOREACH(blk, &list)
	{
		cprintf("(size: %d, isFree: %d)\n", get_block_size(blk), is_free_block(blk)) ;
	}
	cprintf("=========================================\n");

}
//
////********************************************************************************//
////********************************************************************************//

//==================================================================================//
//++============================== ADDED HELPERS ===================================//
//==================================================================================//

//TODO [PROJECT'24.MS1 - #099] [3] DYNAMIC ALLOCATOR - HELPERS

// Defining a MACRO for the split threshold.
#define MIN_SPLIT_THRESHOLD(requested_size) \
		((requested_size) + DYN_ALLOC_MIN_FREE_BLOCK_SIZE)

// Checks footer or header LSB, if 1; it denotes an allocated block
#define IS_ALLOCATED(x) (x & 1)


__inline__ uint32* get_block_header(void* va) {
	return (uint32 *)va - 1;
}

__inline__ uint32* get_block_footer(void* va, uint32 totalSize) {
	return (uint32*)(va + totalSize - DYN_ALLOC_HEADER_FOOTER_SIZE);
}

// Checks if the total_size can be split to two blocks, one of them being the requested_size
__inline__ uint8 is_splittable(uint32 total_size, uint32 requested_size) {
	return total_size >= MIN_SPLIT_THRESHOLD(requested_size);
}

// Checks if both header and footer are set correctly, no way to check for boundaries as handled by hardware fault.
// We can define new variable that holds the_a_block and the_z_block as boundaries, however, as per the design so far, no need.
uint8 valid_block_address(void* va) {
	return *get_block_header(va) == *get_block_footer(va, get_block_size(va));
}


// Allocates a free block and removes it from the list.
void allocate_free_block(struct BlockElement* block, uint32 size) {
	set_block_data(block, size, DYN_ALLOC_ALLOCATED);
	LIST_REMOVE(&freeBlocksList, block);
}


// Splitting a block and create a new free block
void split_free_block_and_allocate(struct BlockElement* block, uint32 requested_size, uint32 total_block_size) {
	// Creating the new free block in the remaining free region.
	struct BlockElement* new_free_block =
			(struct BlockElement*)((uint32)block + requested_size);

	// Setup the new free block
	set_block_data(new_free_block, total_block_size - requested_size, DYN_ALLOC_FREE);
	LIST_INSERT_AFTER(&freeBlocksList, block, new_free_block);

	allocate_free_block(block, requested_size);
}


////********************************************************************************//
////********************************************************************************//

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//

bool is_initialized = 0;
//==================================
// [1] INITIALIZE DYNAMIC ALLOCATOR:
//==================================
void initialize_dynamic_allocator(uint32 daStart, uint32 initSizeOfAllocatedSpace)
{
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		if (initSizeOfAllocatedSpace % 2 != 0) initSizeOfAllocatedSpace++; //ensure it's multiple of 2
		if (initSizeOfAllocatedSpace == 0)
			return ;
		is_initialized = 1;
	}
	//==================================================================================
	//==================================================================================

	//TODO: [PROJECT'24.MS1 - #04] [3] DYNAMIC ALLOCATOR - initialize_dynamic_allocator [DONE]

	// Beginning and Ending special blocks (simple integers both holding 1)
	uint32* the_a_block = (uint32*)daStart;
	uint32* the_z_block = (uint32*)(daStart + initSizeOfAllocatedSpace) - 1;

	*the_a_block = 1;
	*the_z_block = 1;

	// Calculating the size of the first block - beg & end integers.
	uint32 alpha_block_size = initSizeOfAllocatedSpace - DYN_ALLOC_HEADER_FOOTER_SIZE;

	// Header and Footer for the first block, placed after the A block and before the Z block respectively.
	uint32* alpha_block_header = the_a_block + 1;
	uint32* alpha_block_footer = the_z_block - 1;

	// Contains the size, LSB is 0 (free) as guaranteed even and it's empty.
	*alpha_block_header = alpha_block_size;
	*alpha_block_footer = alpha_block_size;

	// First and only free block, As this is the initializing point, so the whole heap is just one free block)
	struct BlockElement* alpha_block = (struct BlockElement*)(alpha_block_header + 1);

	// Initializing the freeBlocksList and adding the first free block to the list.
	LIST_INIT(&freeBlocksList);
	LIST_INSERT_HEAD(&freeBlocksList, alpha_block);
}
//==================================
// [2] SET BLOCK HEADER & FOOTER:
//==================================
void set_block_data(void* va, uint32 totalSize, bool isAllocated)
{
	//TODO: [PROJECT'24.MS1 - #05] [3] DYNAMIC ALLOCATOR - set_block_data [DONE]

	uint32* x_block_header = get_block_header(va);
	uint32* x_block_footer = get_block_footer(va, totalSize);

	if(isAllocated) totalSize++; // Assigning the LSB

	*x_block_header = totalSize;
	*x_block_footer = totalSize;
}


//=========================================
// [3] ALLOCATE BLOCK BY FIRST FIT:
//=========================================
void *alloc_block_FF(uint32 size)
{
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		if (size % 2 != 0) size++;	//ensure that the size is even (to use LSB as allocation flag)
		if (size < DYN_ALLOC_MIN_BLOCK_SIZE)
			size = DYN_ALLOC_MIN_BLOCK_SIZE ;
		if (!is_initialized)
		{
			uint32 required_size = size + 2*sizeof(int) /*header & footer*/ + 2*sizeof(int) /*da begin & end*/ ;
			uint32 da_start = (uint32)sbrk(ROUNDUP(required_size, PAGE_SIZE)/PAGE_SIZE);
			uint32 da_break = (uint32)sbrk(0);
			initialize_dynamic_allocator(da_start, da_break - da_start);
		}
	}
	//==================================================================================
	//==================================================================================

	//TODO: [PROJECT'24.MS1 - #06] [3] DYNAMIC ALLOCATOR - alloc_block_FF [DONE]
	if (size == 0) return NULL;

	// Adding the header and footer combined size to the user requested size.
	size += DYN_ALLOC_HEADER_FOOTER_SIZE;

	struct BlockElement* iterator_block;

	LIST_FOREACH(iterator_block, &freeBlocksList) {
		uint32 blk_size = get_block_size(iterator_block);
		if (blk_size >= size) {
			// Check if it can be split to eliminate internal fragmentation.
			if (is_splittable(blk_size, size)) {
				split_free_block_and_allocate(iterator_block, size, blk_size);
			}
			else {
				allocate_free_block(iterator_block, blk_size);
			}

			return iterator_block;
		}
	}

	uint32 sbrk_ret = (uint32)sbrk(ROUNDUP(size, PAGE_SIZE)/PAGE_SIZE);

	if (sbrk_ret != -1) {
		uint32 available_size = ROUNDUP(size, PAGE_SIZE);
		struct BlockElement* new_block;

		/**
		 * Check if the current last block is free to merge before allocating.
		 */

		uint32* prev_block_footer = get_block_header((void*) sbrk_ret) - 2; // - 2 as there is a special end block in the way.

		// Merge with previous block
		if (!IS_ALLOCATED(*prev_block_footer)) {
			// re-set the prev_block data to new size
			uint32 block_new_size = available_size + (*prev_block_footer);
			new_block = (void*)((uint32)prev_block_footer - (*prev_block_footer) + DYN_ALLOC_HEADER_FOOTER_SIZE);

			// Re-setting the merged block data.
			set_block_data(new_block, block_new_size, DYN_ALLOC_FREE);
		} else {
			// The new block to be placed @ the start of the newly added space to the heap.
			// The old special ending block will become the header for this block.
			new_block = (struct BlockElement*)sbrk_ret;

			// Check if it can be split to eliminate internal fragmentation.
			if (is_splittable(available_size, size)) {
				split_free_block_and_allocate(new_block, size, available_size);
			}
			else {
				allocate_free_block(new_block, available_size);
			}

		}

		uint32* new_end_block = (uint32*)(sbrk_ret + available_size - sizeof(int)) ;
		*new_end_block = 1;

		return new_block;
	}

	return NULL;
}
//=========================================
// [4] ALLOCATE BLOCK BY BEST FIT:
//=========================================
void *alloc_block_BF(uint32 size)
{
	//TODO: [PROJECT'24.MS1 - BONUS] [3] DYNAMIC ALLOCATOR - alloc_block_BF [DONE]
	//==================================================================================
	//DON'T CHANGE THESE LINES==========================================================
	//==================================================================================
	{
		if (size % 2 != 0) size++;	//ensure that the size is even (to use LSB as allocation flag)
		if (size < DYN_ALLOC_MIN_BLOCK_SIZE)
			size = DYN_ALLOC_MIN_BLOCK_SIZE ;
		if (!is_initialized)
		{
			uint32 required_size = size + 2*sizeof(int) /*header & footer*/ + 2*sizeof(int) /*da begin & end*/ ;
			uint32 da_start = (uint32)sbrk(ROUNDUP(required_size, PAGE_SIZE)/PAGE_SIZE);
			uint32 da_break = (uint32)sbrk(0);
			initialize_dynamic_allocator(da_start, da_break - da_start);
		}
	}
	//==================================================================================
	//==================================================================================

	if (size == 0) return NULL;

	// Adding the header and footer combined size to the user requested size.
	size += DYN_ALLOC_HEADER_FOOTER_SIZE;

	struct BlockElement* iterator_block;
	struct BlockElement* best_fit_block = NULL;
	uint32 min_size_difference = 0xFFFFFFFF;  // Initialize to maximum possible value

	// First pass: Find the block with the smallest suitable size
	LIST_FOREACH(iterator_block, &freeBlocksList) {
		uint32 blk_size = get_block_size(iterator_block);
		if (blk_size >= size) {
			uint32 current_difference = blk_size - size;
			if (current_difference < min_size_difference) {
				min_size_difference = current_difference;
				best_fit_block = iterator_block;
			}
		}
	}

	// If we found a suitable block
	if (best_fit_block != NULL) {
		uint32 best_blk_size = get_block_size(best_fit_block);
		// Check if it can be split to eliminate internal fragmentation
		if (is_splittable(best_blk_size, size)) {
			split_free_block_and_allocate(best_fit_block, size, best_blk_size);
		}
		else {
			allocate_free_block(best_fit_block, best_blk_size);
		}
		return best_fit_block;
	}

	// If no suitable block found, request more memory
	uint32 sbrk_ret = (uint32)sbrk(ROUNDUP(size, PAGE_SIZE)/PAGE_SIZE);
	if (sbrk_ret != -1) {
		uint32 available_size = ROUNDUP(size, PAGE_SIZE);
		struct BlockElement* new_block;

		/**
		 * Check if the current last block is free to merge before allocating.
		 */

		uint32* prev_block_footer = get_block_header((void*) sbrk_ret) - 2; // - 2 as there is a special end block in the way.

		// Merge with previous block
		if (!IS_ALLOCATED(*prev_block_footer)) {
			// re-set the prev_block data to new size
			uint32 block_new_size = available_size + (*prev_block_footer);
			new_block = (void*)((uint32)prev_block_footer - (*prev_block_footer) + DYN_ALLOC_HEADER_FOOTER_SIZE);

			// Re-setting the merged block data.
			set_block_data(new_block, block_new_size, DYN_ALLOC_FREE);
		} else {
			// The new block to be placed @ the start of the newly added space to the heap.
			// The old special ending block will become the header for this block.
			new_block = (struct BlockElement*)sbrk_ret;

			// Check if it can be split to eliminate internal fragmentation.
			if (is_splittable(available_size, size)) {
				split_free_block_and_allocate(new_block, size, available_size);
			}
			else {
				allocate_free_block(new_block, available_size);
			}

		}

		uint32* new_end_block = (uint32*)(sbrk_ret + available_size - sizeof(int)) ;
		*new_end_block = 1;

		return new_block;
	}

	return NULL;
}

//===================================================
// [5] FREE BLOCK WITH COALESCING:
//===================================================
void free_block(void *va)
{
	//TODO: [PROJECT'24.MS1 - #07] [3] DYNAMIC ALLOCATOR - free_block [DONE]

	// Checking for NULL
	if (va == NULL) return;

	// Setting the block as free.
	struct BlockElement* freed_block = (struct BlockElement*)va;
	set_block_data(freed_block, get_block_size(freed_block), DYN_ALLOC_FREE);

	// Placement variable, counts the supposed index of the block.
	uint32 idx = 0;
	struct BlockElement* iterator_block;

	/**
	 * Finding the closest free block to insert the newly freed block.
	 *
	 * Inside the loop we can use LIST_INSERT_BEFORE if the found address
	 * is bigger than the one we're inserting.
	 *
	 * The loop isn't enough as the list might be empty or the address can
	 * be bigger than all elements, handled by the if condition right after.
	 */
	LIST_FOREACH(iterator_block, &freeBlocksList) {
		if (freed_block < iterator_block) {
			LIST_INSERT_BEFORE(&freeBlocksList, iterator_block, freed_block);
			break;
		}
		idx++;
	}

	if (idx == freeBlocksList.size) { LIST_INSERT_TAIL(&freeBlocksList, freed_block); }


	// Merging if possible.
	// Check header for next block
	uint32* next_block_header = get_block_footer(freed_block, get_block_size(freed_block)) + 1;
	uint32* prev_block_footer = get_block_header(freed_block) - 1;

	// Merge with next block
	if (!IS_ALLOCATED(*next_block_header)) {
		// remove next block from list
		LIST_REMOVE(&freeBlocksList, (struct BlockElement*)(next_block_header + 1));

		// re-set the freed block data to new size
		uint32 block_new_size = get_block_size(freed_block) + (*next_block_header);
		set_block_data(freed_block, block_new_size, DYN_ALLOC_FREE);
	}

	// Merge with previous block
	if (!IS_ALLOCATED(*prev_block_footer)) {
		// remove freed block from list
		LIST_REMOVE(&freeBlocksList, freed_block);

		// re-set the prev_block data to new size
		uint32 block_new_size = get_block_size(freed_block) + (*prev_block_footer);


		/**
		 * The resulting block from the merge will be starting with the header of previous
		 * and ending with the footer of the current.
		 *
		 * Calculating the block address:
		 * By subtracting the size of the entire previous block and adding the size of
		 * both header & footer to reach the start of where the data should be allocated.
		 */

		void* merged_block = (void*)((uint32)prev_block_footer - (*prev_block_footer) + DYN_ALLOC_HEADER_FOOTER_SIZE);

		// Re-setting the merged block data.
		set_block_data(merged_block, block_new_size, DYN_ALLOC_FREE);
	}

}

//=========================================
// [6] REALLOCATE BLOCK BY FIRST FIT:
//=========================================

// Checks if the block can be extended in place.
__inline__ uint8 can_extend_in_place(void* block, uint32 current_size, uint32 desired_size) {
	uint32 next_header = *(get_block_footer(block, current_size) + 1);
	return !IS_ALLOCATED(next_header) && next_header + current_size >= desired_size;
}

// Splits a block and add frees the remaining part
void split_block(void* block, uint32 total_size, uint32 requested_size) {
	set_block_data(block, requested_size, DYN_ALLOC_ALLOCATED);

	void* new_free_block = (void*)(block + requested_size);
	uint32 free_block_size = total_size - requested_size;

	set_block_data(new_free_block, free_block_size, DYN_ALLOC_FREE);
	free_block(new_free_block);
}

// Shrinks the block at the given address of the current_size to the new_size
void shrink_block(void* va, uint32 current_size, uint32 new_size) {
	if (is_splittable(current_size, new_size)) {
		split_block(va, current_size, new_size);
	}
}

// Extends a block of current_size to the new_size using the next block.
void extend_block(void* va, uint32 current_size, uint32 new_size) {
	// Merge and split if possible
	void* next_block = (void*)(get_block_footer(va, current_size) + 2);
	uint32 next_block_size = get_block_size(next_block); // Free block guaranteed by the condition
	uint32 total_available_size = current_size + next_block_size;

	// Removing from the free list as it will be used in the extension.
	LIST_REMOVE(&freeBlocksList, (struct BlockElement*) next_block);

	// Checking if it's possible to split and create a new free block.
	if (is_splittable(total_available_size, new_size)) {
		split_block(va, total_available_size, new_size);
	} else {
		set_block_data(va, total_available_size, DYN_ALLOC_ALLOCATED);
	}
}

// Relocates the block to a new block returned from Allocate.
void* relocate_block(void* va, uint32 current_size, uint32 new_size) {
	//cprintf("Relocating\n");
	void* new_block = alloc_block_FF(new_size); // Allocate to get a new block that can fit the new size
	if (new_block == NULL) { return NULL; } // Allocate and sbrk (within Allocate) can't allocate new.
	//cprintf("Relocating NEW ADDRESS: %x\n", new_block);
	memcpy(new_block, va, current_size); // Copying data from old address to the new one, memcpy is safe hence relocate, no overlap risk.
	free_block(va); // Freeing the old block, no longer needed.
	return new_block; // returning the new address for the re-sized block.
}

void *realloc_block_FF(void* va, uint32 new_size)
{
	//TODO: [PROJECT'24.MS1 - #08] [3] DYNAMIC ALLOCATOR - realloc_block_FF [DONE]

	if (va == NULL) { // Nothing to reallocate, either allocate intended or dummy case
		if (new_size == 0) { return NULL; }
		else { return alloc_block_FF(new_size); }
	}

	if (!valid_block_address(va)) return NULL;

	if (new_size == 0) { // Free intended
		free_block(va);
		return NULL;
	}

	if (new_size % 2 != 0) new_size++;

	uint32 blk_size = get_block_size(va);
	new_size += DYN_ALLOC_HEADER_FOOTER_SIZE; // Adding size for both header & footer

	// Decreasing the size
	if (new_size <= blk_size) {
		shrink_block(va, blk_size, new_size);
		return va;
	}

	if (can_extend_in_place(va, blk_size, new_size)) {
		extend_block(va, blk_size, new_size);
		return va;
	}

	// Removing the added size for both header & footer, as relocate will be calling allocate
	// Allocate will be re-adding them when trying to allocate new.
	new_size -= DYN_ALLOC_HEADER_FOOTER_SIZE;
	return relocate_block(va, blk_size, new_size); // Only option left is to relocate.
}

/*********************************************************************************************/
/*********************************************************************************************/
/*********************************************************************************************/
//=========================================
// [7] ALLOCATE BLOCK BY WORST FIT:
//=========================================
void *alloc_block_WF(uint32 size)
{
	panic("alloc_block_WF is not implemented yet");
	return NULL;
}

//=========================================
// [8] ALLOCATE BLOCK BY NEXT FIT:
//=========================================
void *alloc_block_NF(uint32 size)
{
	panic("alloc_block_NF is not implemented yet");
	return NULL;
}
