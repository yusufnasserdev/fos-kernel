#ifndef FOS_KERN_KHEAP_H_
#define FOS_KERN_KHEAP_H_

#ifndef FOS_KERNEL
# error "This is a FOS kernel header; user programs should not #include it"
#endif

#include <inc/types.h>
#include <inc/queue.h>


/*2017*/
uint32 _KHeapPlacementStrategy;
//Values for user heap placement strategy
#define KHP_PLACE_CONTALLOC 0x0
#define KHP_PLACE_FIRSTFIT 	0x1
#define KHP_PLACE_BESTFIT 	0x2
#define KHP_PLACE_NEXTFIT 	0x3
#define KHP_PLACE_WORSTFIT 	0x4

static inline void setKHeapPlacementStrategyCONTALLOC(){_KHeapPlacementStrategy = KHP_PLACE_CONTALLOC;}
static inline void setKHeapPlacementStrategyFIRSTFIT(){_KHeapPlacementStrategy = KHP_PLACE_FIRSTFIT;}
static inline void setKHeapPlacementStrategyBESTFIT(){_KHeapPlacementStrategy = KHP_PLACE_BESTFIT;}
static inline void setKHeapPlacementStrategyNEXTFIT(){_KHeapPlacementStrategy = KHP_PLACE_NEXTFIT;}
static inline void setKHeapPlacementStrategyWORSTFIT(){_KHeapPlacementStrategy = KHP_PLACE_WORSTFIT;}

static inline uint8 isKHeapPlacementStrategyCONTALLOC(){if(_KHeapPlacementStrategy == KHP_PLACE_CONTALLOC) return 1; return 0;}
static inline uint8 isKHeapPlacementStrategyFIRSTFIT(){if(_KHeapPlacementStrategy == KHP_PLACE_FIRSTFIT) return 1; return 0;}
static inline uint8 isKHeapPlacementStrategyBESTFIT(){if(_KHeapPlacementStrategy == KHP_PLACE_BESTFIT) return 1; return 0;}
static inline uint8 isKHeapPlacementStrategyNEXTFIT(){if(_KHeapPlacementStrategy == KHP_PLACE_NEXTFIT) return 1; return 0;}
static inline uint8 isKHeapPlacementStrategyWORSTFIT(){if(_KHeapPlacementStrategy == KHP_PLACE_WORSTFIT) return 1; return 0;}

//***********************************

void* kmalloc(unsigned int size);
void kfree(void* virtual_address);
void *krealloc(void *virtual_address, unsigned int new_size);

/* Added Helpers */
void* kmalloc_ff(unsigned int size);
void* kmalloc_bf(unsigned int size);

unsigned int kheap_virtual_address(unsigned int physical_address);
unsigned int kheap_physical_address(unsigned int virtual_address);

int numOfKheapVACalls;

//=============================================================================
struct PageElement
{
	LIST_ENTRY(PageElement) prev_next_info;	/* linked list links */

	uint16 pages_count;
	uint32 start_addr;
};

LIST_HEAD(MemPage_LIST, PageElement);
struct MemPage_LIST khFreePagesList;
//=============================================================================


//TODO: [PROJECT'24.MS2 - #01] [1] KERNEL HEAP - add suitable code here [DONE]

uint32 kh_alloc_base;	// Kernel heap base address
uint32 kh_soft_cap;		// Kernel heap segment break, 'soft' cap as it can be extended
uint32 kh_hard_cap;		// Kernel heap block allocation end, 'hard' cap as it CANNOT be breached.


#endif // FOS_KERN_KHEAP_H_
