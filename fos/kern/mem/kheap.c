#include "kheap.h"

#include <inc/memlayout.h>
#include <inc/dynamic_allocator.h>
#include "memory_manager.h"

//Initialize the dynamic allocator of kernel heap with the given start address, size & limit
//All pages in the given range should be allocated
//Remember: call the initialize_dynamic_allocator(..) to complete the initialization
//Return:
//	On success: 0
//	Otherwise (if no memory OR initial size exceed the given limit): PANIC
int initialize_kheap_dynamic_allocator(uint32 daStart, uint32 initSizeToAllocate, uint32 daLimit)
{
	//TODO: [PROJECT'24.MS2 - #01] [1] KERNEL HEAP - initialize_kheap_dynamic_allocator [DONE]

	// Initialize tracking variables
	kh_soft_cap = kh_alloc_base = ROUNDDOWN(daStart, PAGE_SIZE); // ensuring that they align with first page boundary.
	kh_hard_cap = daLimit;

	initSizeToAllocate = ROUNDUP(initSizeToAllocate, PAGE_SIZE); // Aligning the requested size to a page boundary

	// Initial size exceeds the given limit
	if (daStart + initSizeToAllocate > daLimit) {
		panic("INIT KHEAP DYNAMIC ALLOCATOR FAILED: Initial size exceeds the given limit\n");
	}

	kh_soft_cap += initSizeToAllocate; // Extending the soft cap to the requested size as it's ensured to be feasible with the given limit.

	// Allocate the pages in the given range and map them.
	for (uint32 i = kh_alloc_base; i < kh_soft_cap; i+=PAGE_SIZE) {
		struct FrameInfo* new_frame;
		allocate_frame(&new_frame); // Panics if no memory available
		map_frame(ptr_page_directory, new_frame, i, PERM_WRITEABLE);
		new_frame->virtual_address = i;
	}

	// Dynamic Allocator manages the block allocation, hence initialized.
	initialize_dynamic_allocator(kh_alloc_base, initSizeToAllocate);
	// cprintf("h9\n");
	return 0;
}

void* sbrk(int numOfPages)
{
	/* numOfPages > 0: move the segment break of the kernel to increase the size of its heap by the given numOfPages,
	 * 				you should allocate pages and map them into the kernel virtual address space,
	 * 				and returns the address of the previous break (i.e. the beginning of newly mapped memory).
	 * numOfPages = 0: just return the current position of the segment break
	 *
	 * NOTES:
	 * 	1) Allocating additional pages for a kernel dynamic allocator will fail if the free frames are exhausted
	 * 		or the break exceed the limit of the dynamic allocator. If sbrk fails, return -1
	 */

	//TODO: [PROJECT'24.MS2 - #02] [1] KERNEL HEAP - sbrk [DONE]

	// This is not defined in the requirements, however, it's a corner case, had to handle it.
	if (numOfPages < 0) panic("KHeap SBRK: CANNOT ALLOCATE NEGATIVE NUM OF PAGES");
	if (numOfPages == 0) return (void*) kh_soft_cap;

	if (kh_soft_cap + (numOfPages * PAGE_SIZE) < kh_hard_cap) { // Could be <=
		// move soft cap
		uint32 new_start = kh_soft_cap;
		kh_soft_cap += (numOfPages * PAGE_SIZE);

		// allocate and map
		for (uint32 i = new_start; i < kh_soft_cap; i+=PAGE_SIZE) {
			struct FrameInfo* new_frame;
			allocate_frame(&new_frame); // Panics if no memory available
			map_frame(ptr_page_directory, new_frame, i, PERM_WRITEABLE);
			new_frame->virtual_address = i;
		}

		// return start of allocated space
		return (void*) new_start;
	}

	return (void*)-1 ;
}

//TODO: [PROJECT'24.MS2 - BONUS#2] [1] KERNEL HEAP - Fast Page Allocator

/**
 * Holds meta-data for the allocated pages or 0 for free ones.
 *
 * Page entries where chunk allocation starts, holds allocation full size
 * Page entries inside chunk allocation, holds reference to the start allocation entry.
 */
uint32 is_allocated[MAX_ENTRIES][MAX_ENTRIES] = {0};

void* kmalloc(unsigned int size)
{
	//TODO: [PROJECT'24.MS2 - #03] [1] KERNEL HEAP - kmalloc [DOING]

	if (isKHeapPlacementStrategyFIRSTFIT()) {
		return kmalloc_ff(size);
	} else if (isKHeapPlacementStrategyBESTFIT()){
		return kmalloc_bf(size);
	} else {
		panic("Placement strategy not implemented");
	}
}

void* kmalloc_ff(unsigned int size) {
	if (size <= DYN_ALLOC_MAX_BLOCK_SIZE) {
		return alloc_block_FF(size); // Already implemented and working
	}

	uint16 pages_requested_num = ROUNDUP(size, PAGE_SIZE) / PAGE_SIZE;
	uint16 curr_consecutive_pgs = 0;
	void* alloc_start_addr = NULL;

	// Iterate through the KHEAP pages space for enough consecutive free pages.
	for (uint32 iter = KH_PG_ALLOC_START; iter < KERNEL_HEAP_MAX; iter += PAGE_SIZE) {
		if (!is_allocated[PDX(iter)][PTX(iter)]) {
			if (alloc_start_addr == NULL) alloc_start_addr = (void*) iter; // Assign address if it was at start
			curr_consecutive_pgs++;
			if (curr_consecutive_pgs == pages_requested_num) break;
		} else { // Space needs to be contagious, so any allocated pages, resets the tracking vars.
			alloc_start_addr = NULL;
			curr_consecutive_pgs = 0;
		}
	}

	if (curr_consecutive_pgs == pages_requested_num) {
		for (uint32 iter = (uint32)alloc_start_addr;
				iter < KERNEL_HEAP_MAX && pages_requested_num > 0; iter += PAGE_SIZE) {

			struct FrameInfo* new_frame;
			allocate_frame(&new_frame);
			map_frame(ptr_page_directory, new_frame, iter, PERM_WRITEABLE);

			// Add full allocation base address to is_allocated as meta-data to be used when freeing.
			is_allocated[PDX(iter)][PTX(iter)] = (uint32)alloc_start_addr;
			--pages_requested_num;
		}

		// Add full allocation size to is_allocated as meta-data to be used when freeing.
		is_allocated[PDX(alloc_start_addr)][PTX(alloc_start_addr)] = ROUNDUP(size, PAGE_SIZE);
		return alloc_start_addr;
	}

	return NULL; // Me no can do.
}

void* kmalloc_bf(unsigned int size) {
	if (size <= DYN_ALLOC_MAX_BLOCK_SIZE) {
		return alloc_block_BF(size); // Already implemented and working
	} else { // Allocate page
		return NULL;
	}
}

void kfree(void* virtual_address)
{
	//TODO: [PROJECT'24.MS2 - #04] [1] KERNEL HEAP - kfree
	// Write your code here, remove the panic and write your code
	panic("kfree() is not implemented yet...!!");

	//you need to get the size of the given allocation using its address
	//refer to the project presentation and documentation for details

}

unsigned int kheap_physical_address(unsigned int virtual_address)
{
	//TODO: [PROJECT'24.MS2 - #05] [1] KERNEL HEAP - kheap_physical_address [DONE]

	uint32 *page_table_address;
	uint32 page_table_status = get_page_table(ptr_page_directory, virtual_address, &page_table_address);

	if (page_table_status == TABLE_IN_MEMORY) {
		uint32 page_table_entry = page_table_address[PTX(virtual_address)];
		return EXTRACT_ADDRESS(page_table_entry) + PGOFF(virtual_address);
	}

	return 0;
}

unsigned int kheap_virtual_address(unsigned int physical_address)
{
	//TODO: [PROJECT'24.MS2 - #06] [1] KERNEL HEAP - kheap_virtual_address [DONE]
	if (PPN(physical_address) >= number_of_frames) return 0;

	return frames_info[PPN(physical_address)].virtual_address;
}
//=================================================================================//
//============================== BONUS FUNCTION ===================================//
//=================================================================================//
// krealloc():

//	Attempts to resize the allocated space at "virtual_address" to "new_size" bytes,
//	possibly moving it in the heap.
//	If successful, returns the new virtual_address, if moved to another loc: the old virtual_address must no longer be accessed.
//	On failure, returns a null pointer, and the old virtual_address remains valid.

//	A call with virtual_address = null is equivalent to kmalloc().
//	A call with new_size = zero is equivalent to kfree().

void *krealloc(void *virtual_address, uint32 new_size)
{
	//TODO: [PROJECT'24.MS2 - BONUS#1] [1] KERNEL HEAP - krealloc
	// Write your code here, remove the panic and write your code
	return NULL;
	panic("krealloc() is not implemented yet...!!");
}
