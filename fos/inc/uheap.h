#ifndef FOS_INC_UHEAP_H
#define FOS_INC_UHEAP_H 1

//Values for user heap placement strategy
#define UHP_PLACE_FIRSTFIT 	0x1
#define UHP_PLACE_BESTFIT 	0x2
#define UHP_PLACE_NEXTFIT 	0x3
#define UHP_PLACE_WORSTFIT 	0x4

//2020
#define UHP_USE_BUDDY 0

void *malloc(uint32 size);
void* smalloc(char *sharedVarName, uint32 size, uint8 isWritable);
void* sget(int32 ownerEnvID, char *sharedVarName);
void free(void* virtual_address);
void sfree(void* virtual_address);
void *realloc(void *virtual_address, uint32 new_size);

/*Added Helpers*/
void *malloc_ff(uint32 size);
void *malloc_bf(uint32 size);

//struct ShareTrack
//{
//	uint32 virt_addr;
//	int32 ID;
//
//	LIST_ENTRY(ShareTrack) prev_next_info;	/* linked list links */
//};
//
//LIST_HEAD(ShareTrack_LIST, ShareTrack);
//struct ShareTrack_LIST shareTrackList;




#endif
