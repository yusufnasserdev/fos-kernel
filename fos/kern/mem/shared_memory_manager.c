#include <inc/memlayout.h>
#include "shared_memory_manager.h"

#include <inc/mmu.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/queue.h>
#include <inc/environment_definitions.h>

#include <kern/proc/user_environment.h>
#include <kern/trap/syscall.h>
#include "kheap.h"
#include "memory_manager.h"

//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//
struct Share* get_share(int32 ownerID, char* name);

//===========================
// [1] INITIALIZE SHARES:
//===========================
//Initialize the list and the corresponding lock
void sharing_init()
{
#if USE_KHEAP
	LIST_INIT(&AllShares.shares_list) ;
	init_spinlock(&AllShares.shareslock, "shares lock");
#else
	panic("not handled when KERN HEAP is disabled");
#endif
}

//==============================
// [2] Get Size of Share Object:
//==============================
int getSizeOfSharedObject(int32 ownerID, char* shareName)
{
	//[PROJECT'24.MS2] DONE
	// This function should return the size of the given shared object
	// RETURN:
	//	a) If found, return size of shared object
	//	b) Else, return E_SHARED_MEM_NOT_EXISTS
	//
	struct Share* ptr_share = get_share(ownerID, shareName);
	if (ptr_share == NULL)
		return E_SHARED_MEM_NOT_EXISTS;
	else
		return ptr_share->size;

	return 0;
}

//===========================================================


//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//
//===========================
// [1] Create frames_storage:
//===========================
// Create the frames_storage and initialize it by 0
inline struct FrameInfo** create_frames_storage(int numOfFrames)
{
	//TODO: [PROJECT'24.MS2 - #16] [4] SHARED MEMORY - create_frames_storage() [DONE]
	struct FrameInfo** frames_array = (struct FrameInfo**) kmalloc(numOfFrames * sizeof(struct FrameInfo*));
	if (frames_array == NULL) return NULL;
	for (int i = 0; i < numOfFrames; i++) frames_array[i] = NULL;
	return frames_array;
}

//=====================================
// [2] Alloc & Initialize Share Object:
//=====================================
//Allocates a new shared object and initialize its member
//It dynamically creates the "framesStorage"
//Return: allocatedObject (pointer to struct Share) passed by reference
struct Share* create_share(int32 ownerID, char* shareName, uint32 size, uint8 isWritable)
{
	//TODO: [PROJECT'24.MS2 - #16] [4] SHARED MEMORY - create_share() [DONE]
	struct Share* new_share = (struct Share*) kmalloc(sizeof(struct Share));
	if (new_share == NULL) return NULL;

	new_share->ownerID = ownerID;
	new_share->name = shareName;
	new_share->size = size;
	new_share->isWritable = isWritable;
	new_share->references = 1;
	new_share->ID = ((uint32)new_share | 0x80000000);
	new_share->framesStorage = create_frames_storage(ROUNDUP(size, PAGE_SIZE));

	// Frames storage allocation failed, undoing previous allocation
	if (new_share->framesStorage == NULL)
	{
		kfree(new_share);
		return NULL;
	}

	return new_share;
}

//=============================
// [3] Search for Share Object:
//=============================
//Search for the given shared object in the "shares_list"
//Return:
//	a) if found: ptr to Share object
//	b) else: NULL
struct Share* get_share(int32 ownerID, char* name)
{
	//TODO: [PROJECT'24.MS2 - #17] [4] SHARED MEMORY - get_share() [DONE]
	struct Share* iterator_share = NULL;

	acquire_spinlock(&AllShares.shareslock);
	LIST_FOREACH(iterator_share, &AllShares.shares_list) {
		if (iterator_share->ownerID == ownerID && strcmp(iterator_share->name, name) == 0) {
			release_spinlock(&AllShares.shareslock);
			return iterator_share;
		}
	}

	release_spinlock(&AllShares.shareslock);
	return NULL;
}

//=========================
// [4] Create Share Object:
//=========================
int createSharedObject(int32 ownerID, char* shareName, uint32 size, uint8 isWritable, void* virtual_address)
{
	//TODO: [PROJECT'24.MS2 - #19] [4] SHARED MEMORY [KERNEL SIDE] - createSharedObject() [DONE]
	if (get_share(ownerID, shareName) != NULL) return E_SHARED_MEM_EXISTS;

	struct Share* new_share = create_share(ownerID, shareName, size, isWritable);
	if (new_share == NULL) return E_NO_SHARE;

	struct Env* myenv = get_cpu_proc();
	uint32 casted_address = (uint32) virtual_address;
	for (uint32 iter = 0, limit = casted_address + size; casted_address < limit; casted_address += PAGE_SIZE) {
		struct FrameInfo* new_frame = NULL;
		allocate_frame(&new_frame);
		map_frame(myenv->env_page_directory, new_frame, casted_address, PERM_USER | PERM_WRITEABLE);
		new_share->framesStorage[iter] = (struct FrameInfo*) casted_address;
	}

	acquire_spinlock(&AllShares.shareslock);
	LIST_INSERT_TAIL(&AllShares.shares_list, new_share);
	release_spinlock(&AllShares.shareslock);

	return new_share->ID;
}


//======================
// [5] Get Share Object:
//======================
int getSharedObject(int32 ownerID, char* shareName, void* virtual_address)
{
	//TODO: [PROJECT'24.MS2 - #21] [4] SHARED MEMORY [KERNEL SIDE] - getSharedObject()
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	panic("getSharedObject is not implemented yet");
	//Your Code is Here...

	struct Env* myenv = get_cpu_proc(); //The calling environment
}

//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//==================================================================================//

//==========================
// [B1] Delete Share Object:
//==========================
//delete the given shared object from the "shares_list"
//it should free its framesStorage and the share object itself
void free_share(struct Share* ptrShare)
{
	//TODO: [PROJECT'24.MS2 - BONUS#4] [4] SHARED MEMORY [KERNEL SIDE] - free_share()
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	panic("free_share is not implemented yet");
	//Your Code is Here...

}
//========================
// [B2] Free Share Object:
//========================
int freeSharedObject(int32 sharedObjectID, void *startVA)
{
	//TODO: [PROJECT'24.MS2 - BONUS#4] [4] SHARED MEMORY [KERNEL SIDE] - freeSharedObject()
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	panic("freeSharedObject is not implemented yet");
	//Your Code is Here...

}
