#include <inc/lib.h>

/*Added tracking structure*/
int32 uh_pgs_status[ADDRESS_SPACE_PAGES]; // This array covers the entire address space of a 32-bit system given the illusionist role.
uint8 uh_pgs_init = 0;

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//

//=============================================
// [1] CHANGE THE BREAK LIMIT OF THE USER HEAP:
//=============================================
/*2023*/
void* sbrk(int increment)
{
	return (void*) sys_sbrk(increment);
}

//=================================
// [2] ALLOCATE SPACE IN USER HEAP:
//=================================
void* malloc(uint32 size)
{
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	if (size == 0) return NULL ;
	//==============================================================
	//TODO: [PROJECT'24.MS2 - #12] [3] USER HEAP [USER SIDE] - malloc() [DONE]

	if (sys_isUHeapPlacementStrategyFIRSTFIT()) {
		return malloc_ff(size);
	} else if (sys_isUHeapPlacementStrategyBESTFIT()){
		return malloc_bf(size);
	} else {
		panic("Placement strategy not implemented, Try FF");
		return NULL;
	}
}


void* malloc_ff(unsigned int size) {
	if (size <= DYN_ALLOC_MAX_BLOCK_SIZE) {
		return alloc_block_FF(size); // Already implemented and working
	}

	size = ROUNDUP(size, PAGE_SIZE);
	uint32 pages_requested_num = size / PAGE_SIZE;
	uint32 curr_consecutive_pgs = 0;

	if (uh_pgs_init == 0) {
		memset(uh_pgs_status, 0, sizeof(uh_pgs_status));
		uh_pgs_init = 1;
	}

	// Iterate through the UHEAP pages space for enough consecutive free pages.
	for (uint32 iter = myEnv->uh_pages_start; iter < USER_HEAP_MAX; iter += PAGE_SIZE) {
		if (uh_pgs_status[iter/PAGE_SIZE]) {
			// Resets consecutive pages tracking
			curr_consecutive_pgs = 0;

			// This line takes a few days to figure out
			// Skips over the allocated area, no point in iterating them, also time-consuming for no reason.
			iter += (uh_pgs_status[iter/PAGE_SIZE] - 1) * PAGE_SIZE;
			continue;
		}

		curr_consecutive_pgs++;

		if (curr_consecutive_pgs == pages_requested_num) {
			uint32 alloc_start_addr = iter - size + PAGE_SIZE;
			uh_pgs_status[alloc_start_addr/PAGE_SIZE] = pages_requested_num;
			sys_allocate_user_mem(alloc_start_addr, size);
			return (void*)alloc_start_addr;
		}
	}

	return NULL; // Me no can do.
}

void* malloc_bf(unsigned int size) {
	if (size <= DYN_ALLOC_MAX_BLOCK_SIZE) {
		return alloc_block_BF(size);
	} else { // Allocate page
		panic("Placement strategy not implemented");
		return NULL;
	}
}


//=================================
// [3] FREE SPACE FROM USER HEAP:
//=================================
void free(void* virtual_address)
{
	//TODO: [PROJECT'24.MS2 - #14] [3] USER HEAP [USER SIDE] - free() [DOING]
	if (!sys_isUHeapPlacementStrategyFIRSTFIT()) panic("ME NO CAN DO, only ff implemented for UH free\n");

	uint32 casted_address = (uint32)virtual_address; // casting the address to be comparable, instead of casting multiple times.

	if (casted_address >= myEnv->uh_alloc_base && casted_address < myEnv->uh_soft_cap) { // Block allocation range
		free_block(virtual_address);
		return;
	}

	if (casted_address >= myEnv->uh_pages_start && casted_address < USER_HEAP_MAX) { // Page allocation range
		uint32 freed_size = uh_pgs_status[casted_address/PAGE_SIZE] * PAGE_SIZE;
		uh_pgs_status[casted_address/PAGE_SIZE] = 0;
		sys_free_user_mem(casted_address, freed_size);
		return;
	}

	panic("KFREE: INVALID ADDRESS\n");
}


//=================================
// [4] ALLOCATE SHARED VARIABLE:
//=================================
void* smalloc(char *sharedVarName, uint32 size, uint8 isWritable)
{
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	if (size == 0) return NULL ;
	//==============================================================
	//TODO: [PROJECT'24.MS2 - #18] [4] SHARED MEMORY [USER SIDE] - smalloc()
	// Write your code here, remove the panic and write your code
	panic("smalloc() is not implemented yet...!!");
	return NULL;
}

//========================================
// [5] SHARE ON ALLOCATED SHARED VARIABLE:
//========================================
void* sget(int32 ownerEnvID, char *sharedVarName)
{
	//TODO: [PROJECT'24.MS2 - #20] [4] SHARED MEMORY [USER SIDE] - sget()
	// Write your code here, remove the panic and write your code
	panic("sget() is not implemented yet...!!");
	return NULL;
}


//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//==================================================================================//

//=================================
// FREE SHARED VARIABLE:
//=================================
//	This function frees the shared variable at the given virtual_address
//	To do this, we need to switch to the kernel, free the pages AND "EMPTY" PAGE TABLES
//	from main memory then switch back to the user again.
//
//	use sys_freeSharedObject(...); which switches to the kernel mode,
//	calls freeSharedObject(...) in "shared_memory_manager.c", then switch back to the user mode here
//	the freeSharedObject() function is empty, make sure to implement it.

void sfree(void* virtual_address)
{
	//TODO: [PROJECT'24.MS2 - BONUS#4] [4] SHARED MEMORY [USER SIDE] - sfree()
	// Write your code here, remove the panic and write your code
	panic("sfree() is not implemented yet...!!");
}


//=================================
// REALLOC USER SPACE:
//=================================
//	Attempts to resize the allocated space at "virtual_address" to "new_size" bytes,
//	possibly moving it in the heap.
//	If successful, returns the new virtual_address, in which case the old virtual_address must no longer be accessed.
//	On failure, returns a null pointer, and the old virtual_address remains valid.

//	A call with virtual_address = null is equivalent to malloc().
//	A call with new_size = zero is equivalent to free().

//  Hint: you may need to use the sys_move_user_mem(...)
//		which switches to the kernel mode, calls move_user_mem(...)
//		in "kern/mem/chunk_operations.c", then switch back to the user mode here
//	the move_user_mem() function is empty, make sure to implement it.
void *realloc(void *virtual_address, uint32 new_size)
{
	//[PROJECT]
	// Write your code here, remove the panic and write your code
	panic("realloc() is not implemented yet...!!");
	return NULL;

}


//==================================================================================//
//========================== MODIFICATION FUNCTIONS ================================//
//==================================================================================//

void expand(uint32 newSize)
{
	panic("Not Implemented");

}
void shrink(uint32 newSize)
{
	panic("Not Implemented");

}
void freeHeap(void* virtual_address)
{
	panic("Not Implemented");

}
