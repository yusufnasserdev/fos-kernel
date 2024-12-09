#include "kheap.h"

#include <inc/memlayout.h>
#include <inc/dynamic_allocator.h>
#include "memory_manager.h"

#define NO_FRAMES -4

/**
 * Tracks meta-data for the pages status
 *
 * Page entries UN-allocated UN-mapped: 0
 * Page entries used for block allocation: -1
 * Page entries used for page allocation: holds number of allocated pages.
 */

int32 kh_pgs_status[NUM_OF_KHEAP_PAGES];

__inline__ uint32 get_page_idx(uint32 virtual_address) {
	return ((virtual_address - KERNEL_HEAP_START) / PAGE_SIZE);
}

int8 allocate_map_track_page(uint32 virt_add, uint16 status) {
	struct FrameInfo *new_frame = NULL;
	allocate_frame(&new_frame);

	if (new_frame == NULL) return NO_FRAMES;
	map_frame(ptr_page_directory, new_frame, virt_add, PERM_WRITEABLE);

	new_frame->virtual_address = virt_add;
	kh_pgs_status[get_page_idx(virt_add)] = status;
	return 0;
}


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
	kh_hard_cap = ROUNDUP(daLimit, PAGE_SIZE);
	kh_pages_start = kh_hard_cap + PAGE_SIZE;


	initSizeToAllocate = ROUNDUP(initSizeToAllocate, PAGE_SIZE); // Aligning the requested size to a page boundary
	// Initial size exceeds the given limit
	if (daStart + initSizeToAllocate > daLimit) {
		panic("INIT KHEAP DYNAMIC ALLOCATOR FAILED: Initial size exceeds the given limit\n");
	}
	kh_soft_cap += initSizeToAllocate; // Extending the soft cap to the requested size as it's ensured to be feasible with the given limit.

	// initialize page allocation tracking array to zeros.
	memset(kh_pgs_status, 0, sizeof(kh_pgs_status));

	// Allocate the pages in the given range and map them.
	for (uint32 iter = kh_alloc_base; iter < kh_soft_cap; iter += PAGE_SIZE) {
		allocate_map_track_page(iter, -1);
	}

	// Dynamic Allocator manages the block allocation, hence initialized.
	initialize_dynamic_allocator(kh_alloc_base, initSizeToAllocate);
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
		for (uint32 iter = new_start; iter < kh_soft_cap; iter+=PAGE_SIZE) {
			allocate_map_track_page(iter, -1);
		}

		// return start of allocated space
		return (void*) new_start;
	}

	return SBRK_FAIL;
}

//TODO: [PROJECT'24.MS2 - BONUS#2] [1] KERNEL HEAP - Fast Page Allocator

void* kmalloc(unsigned int size)
{
	//TODO: [PROJECT'24.MS2 - #03] [1] KERNEL HEAP - kmalloc [DONE]

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
	for (uint32 iter = kh_pages_start; iter < KERNEL_HEAP_MAX; iter += PAGE_SIZE) {
		if (!kh_pgs_status[get_page_idx(iter)]) {
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
			allocate_map_track_page(iter, curr_consecutive_pgs);
			--pages_requested_num;
		}

		return alloc_start_addr;
	}

	return NULL; // Me no can do.
}

void* kmalloc_bf(unsigned int size) {
	if (size <= DYN_ALLOC_MAX_BLOCK_SIZE) {
		return alloc_block_BF(size);
	} else { // Allocate page
		return NULL;
	}
}

void unmap_untrack_page(uint32 virt_add) {
	kh_pgs_status[get_page_idx(virt_add)] = 0;
	unmap_frame(ptr_page_directory, virt_add);
}

void kfree(void* virtual_address)
{
	//TODO: [PROJECT'24.MS2 - #04] [1] KERNEL HEAP - kfree [DONE]
	if (!isKHeapPlacementStrategyFIRSTFIT()) panic("ME NO CAN DO, only ff implemented for free\n");

	uint32 casted_address = (uint32)virtual_address; // casting the address to be comparable, instead of casting multiple times.

	if (casted_address >= kh_alloc_base && casted_address < kh_soft_cap) { // Block allocation range
		free_block(virtual_address);
		return;
	}

	if (casted_address >= kh_pages_start && casted_address < KERNEL_HEAP_MAX) { // Page allocation range
		uint32 pages_free_cnt = kh_pgs_status[get_page_idx(casted_address)];
		while (pages_free_cnt--) {
			unmap_untrack_page(casted_address);
			casted_address += PAGE_SIZE;
		}
		return;
	}

	panic("KFREE: INVALID ADDRESS\n");
}

unsigned int kheap_physical_address(unsigned int virtual_address)
{
	//TODO: [PROJECT'24.MS2 - #05] [1] KERNEL HEAP - kheap_physical_address [DONE]
	uint32 *dummy_ptr_page_table; // breaks if null used, so a dummy used.
	struct FrameInfo* frame_info_address = get_frame_info(ptr_page_directory, virtual_address, &dummy_ptr_page_table);

	if (frame_info_address == NULL) return 0; // Invalid address or not mapped.

	// Frame physical address ORed with the offset from the passed virtual address
	return to_physical_address(frame_info_address) | PGOFF(virtual_address);
}

unsigned int kheap_virtual_address(unsigned int physical_address)
{
	//TODO: [PROJECT'24.MS2 - #06] [1] KERNEL HEAP - kheap_virtual_address [DONE]
	struct FrameInfo* check_frame = to_frame_info(physical_address);
	if (!check_frame->references) return 0; // to check if invalid address after using kfree.

	// Frame mapped virtual address ORed with the offset from the passed physical address
	return frames_info[PPN(physical_address)].virtual_address | PGOFF(physical_address);
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
