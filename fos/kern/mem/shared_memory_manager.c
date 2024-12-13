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
	strcpy(new_share->name, shareName);
	new_share->size = size;
	new_share->isWritable = isWritable;
	new_share->references = 1;
	new_share->ID = ((uint32)new_share | 0x80000000);
	new_share->framesStorage = create_frames_storage(size / PAGE_SIZE);

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

struct Share* get_share_by_id(int32 ID)
{
	struct Share* iterator_share = NULL;

	acquire_spinlock(&AllShares.shareslock);
	LIST_FOREACH(iterator_share, &AllShares.shares_list) {
		if (iterator_share->ID == ID) {
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
		new_share->framesStorage[iter] = new_frame;
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
	//TODO: [PROJECT'24.MS2 - #21] [4] SHARED MEMORY [KERNEL SIDE] - getSharedObject() [DONE]
	struct Share* share_obj = get_share(ownerID, shareName);
	if (share_obj == NULL) return E_SHARED_MEM_NOT_EXISTS;

	// Setting up the permissions as per the shared objects predefined isWritable.
	int perms = PERM_USER;
	if (share_obj->isWritable) perms |= PERM_WRITEABLE;

	struct Env* myenv = get_cpu_proc(); //The calling environment
	uint32 casted_address = (uint32) virtual_address, size = share_obj->size;
	for (uint32 iter = 0, limit = casted_address + size; casted_address < limit; casted_address += PAGE_SIZE) {
		struct FrameInfo* allocated_frame = share_obj->framesStorage[iter];
		map_frame(myenv->env_page_directory, allocated_frame, casted_address, perms);
	}

	share_obj->references++;
	return share_obj->ID;
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
	//TODO: [PROJECT'24.MS2 - BONUS#4] [4] SHARED MEMORY [KERNEL SIDE] - free_share() [DONE]
	acquire_spinlock(&AllShares.shareslock);
	LIST_REMOVE(&AllShares.shares_list, ptrShare);
	release_spinlock(&AllShares.shareslock);

	kfree(ptrShare->framesStorage);
	kfree(ptrShare);
}
//========================
// [B2] Free Share Object:
//========================
void cleanup_unused_page_tables(uint32 casted_address, uint32 size, struct Env *myenv) {
	uint32 tbl_low = ROUNDDOWN(casted_address, PAGE_TABLE_SPACE);
	uint32 tbl_high = ROUNDUP(casted_address + size, PAGE_TABLE_SPACE);

	for (uint32 table_addr = tbl_low; table_addr < tbl_high; table_addr += PAGE_TABLE_SPACE) {
		uint8 is_table_used = 0;

		// Retrieve the page table corresponding to the current address
		uint32* page_table = NULL;
		get_page_table(myenv->env_page_directory, table_addr, &page_table);

		if (page_table != NULL) {
			for (uint32 page_addr = table_addr; page_addr < table_addr + PAGE_TABLE_SPACE; page_addr += PAGE_SIZE) {
				struct FrameInfo *frame_info = get_frame_info(myenv->env_page_directory, page_addr, &page_table);

				if (frame_info != NULL) {
					is_table_used = 1;
					break;
				}
			}

			if (!is_table_used) {
				// Free the unused page table
				kfree((void *)kheap_virtual_address(myenv->env_page_directory[PDX(table_addr)]));
				myenv->env_page_directory[PDX(table_addr)] = 0;
			}
		}
	}
}


int freeSharedObject(int32 sharedObjectID, void *startVA)
{
	//TODO: [PROJECT'24.MS2 - BONUS#4] [4] SHARED MEMORY [KERNEL SIDE] - freeSharedObject() [DONE]
	struct Share* share_obj = get_share_by_id(sharedObjectID);
	if (share_obj == NULL) {
		cprintf("\nShare ID is not valid\n");
		return -1;
	}
	int size = share_obj->size;
	struct Env* myenv = get_cpu_proc();
	uint32 casted_address = (uint32) startVA;

	for (uint32 iter = casted_address, limit = casted_address + size; iter < limit; iter += PAGE_SIZE) {
		unmap_frame(myenv->env_page_directory, iter);
	}

	cleanup_unused_page_tables(casted_address, size, myenv);

	share_obj->references--;
	if (share_obj->references == 0) free_share(share_obj);
	tlbflush();
	return 0;
}
