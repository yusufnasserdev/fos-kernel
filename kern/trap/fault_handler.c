/*
 * fault_handler.c
 *
 *  Created on: Oct 12, 2022
 *      Author: HP
 */

#include "trap.h"
#include <kern/proc/user_environment.h>
#include <kern/cpu/sched.h>
#include <kern/cpu/cpu.h>
#include <kern/disk/pagefile_manager.h>
#include <kern/mem/memory_manager.h>

//2014 Test Free(): Set it to bypass the PAGE FAULT on an instruction with this length and continue executing the next one
// 0 means don't bypass the PAGE FAULT
uint8 bypassInstrLength = 0;

//===============================
// REPLACEMENT STRATEGIES
//===============================
//2020
void setPageReplacmentAlgorithmLRU(int LRU_TYPE)
{
	assert(LRU_TYPE == PG_REP_LRU_TIME_APPROX || LRU_TYPE == PG_REP_LRU_LISTS_APPROX);
	_PageRepAlgoType = LRU_TYPE ;
}
void setPageReplacmentAlgorithmCLOCK(){_PageRepAlgoType = PG_REP_CLOCK;}
void setPageReplacmentAlgorithmFIFO(){_PageRepAlgoType = PG_REP_FIFO;}
void setPageReplacmentAlgorithmModifiedCLOCK(){_PageRepAlgoType = PG_REP_MODIFIEDCLOCK;}
/*2018*/ void setPageReplacmentAlgorithmDynamicLocal(){_PageRepAlgoType = PG_REP_DYNAMIC_LOCAL;}
/*2021*/ void setPageReplacmentAlgorithmNchanceCLOCK(int PageWSMaxSweeps){_PageRepAlgoType = PG_REP_NchanceCLOCK;  page_WS_max_sweeps = PageWSMaxSweeps;}

//2020
uint32 isPageReplacmentAlgorithmLRU(int LRU_TYPE){return _PageRepAlgoType == LRU_TYPE ? 1 : 0;}
uint32 isPageReplacmentAlgorithmCLOCK(){if(_PageRepAlgoType == PG_REP_CLOCK) return 1; return 0;}
uint32 isPageReplacmentAlgorithmFIFO(){if(_PageRepAlgoType == PG_REP_FIFO) return 1; return 0;}
uint32 isPageReplacmentAlgorithmModifiedCLOCK(){if(_PageRepAlgoType == PG_REP_MODIFIEDCLOCK) return 1; return 0;}
/*2018*/ uint32 isPageReplacmentAlgorithmDynamicLocal(){if(_PageRepAlgoType == PG_REP_DYNAMIC_LOCAL) return 1; return 0;}
/*2021*/ uint32 isPageReplacmentAlgorithmNchanceCLOCK(){if(_PageRepAlgoType == PG_REP_NchanceCLOCK) return 1; return 0;}

//===============================
// PAGE BUFFERING
//===============================
void enableModifiedBuffer(uint32 enableIt){_EnableModifiedBuffer = enableIt;}
uint8 isModifiedBufferEnabled(){  return _EnableModifiedBuffer ; }

void enableBuffering(uint32 enableIt){_EnableBuffering = enableIt;}
uint8 isBufferingEnabled(){  return _EnableBuffering ; }

void setModifiedBufferLength(uint32 length) { _ModifiedBufferLength = length;}
uint32 getModifiedBufferLength() { return _ModifiedBufferLength;}

//===============================
// FAULT HANDLERS
//===============================

//==================
// [1] MAIN HANDLER:
//==================
/*2022*/
uint32 last_eip = 0;
uint32 before_last_eip = 0;
uint32 last_fault_va = 0;
uint32 before_last_fault_va = 0;
int8 num_repeated_fault  = 0;

struct Env* last_faulted_env = NULL;
void fault_handler(struct Trapframe *tf)
{
	/******************************************************/
	// Read processor's CR2 register to find the faulting address
	uint32 fault_va = rcr2();
	//	cprintf("\n************Faulted VA = %x************\n", fault_va);
	//	print_trapframe(tf);
	/******************************************************/

	//If same fault va for 3 times, then panic
	//UPDATE: 3 FAULTS MUST come from the same environment (or the kernel)
	struct Env* cur_env = get_cpu_proc();
	if (last_fault_va == fault_va && last_faulted_env == cur_env)
	{
		num_repeated_fault++ ;
		if (num_repeated_fault == 3)
		{
			print_trapframe(tf);
			panic("Failed to handle fault! fault @ at va = %x from eip = %x causes va (%x) to be faulted for 3 successive times\n", before_last_fault_va, before_last_eip, fault_va);
		}
	}
	else
	{
		before_last_fault_va = last_fault_va;
		before_last_eip = last_eip;
		num_repeated_fault = 0;
	}
	last_eip = (uint32)tf->tf_eip;
	last_fault_va = fault_va ;
	last_faulted_env = cur_env;
	/******************************************************/
	//2017: Check stack overflow for Kernel
	int userTrap = 0;
	if ((tf->tf_cs & 3) == 3) {
		userTrap = 1;
	}
	if (!userTrap)
	{
		struct cpu* c = mycpu();
		//cprintf("trap from KERNEL\n");
		if (cur_env && fault_va >= (uint32)cur_env->kstack && fault_va < (uint32)cur_env->kstack + PAGE_SIZE)
			panic("User Kernel Stack: overflow exception!");
		else if (fault_va >= (uint32)c->stack && fault_va < (uint32)c->stack + PAGE_SIZE)
			panic("Sched Kernel Stack of CPU #%d: overflow exception!", c - CPUS);
#if USE_KHEAP
		if (fault_va >= KERNEL_HEAP_MAX)
			panic("Kernel: heap overflow exception!");
#endif
	}
	//2017: Check stack underflow for User
	else
	{
		//cprintf("trap from USER\n");
		if (fault_va >= USTACKTOP && fault_va < USER_TOP)
			panic("User: stack underflow exception!");
	}

	//get a pointer to the environment that caused the fault at runtime
	//cprintf("curenv = %x\n", curenv);
	struct Env* faulted_env = cur_env;
	if (faulted_env == NULL)
	{
		print_trapframe(tf);
		panic("faulted env == NULL!");
	}
	//check the faulted address, is it a table or not ?
	//If the directory entry of the faulted address is NOT PRESENT then
	if ( (faulted_env->env_page_directory[PDX(fault_va)] & PERM_PRESENT) != PERM_PRESENT)
	{
		// we have a table fault =============================================================
		//		cprintf("[%s] user TABLE fault va %08x\n", curenv->prog_name, fault_va);
		//		print_trapframe(tf);

		faulted_env->tableFaultsCounter ++ ;

		table_fault_handler(faulted_env, fault_va);
	}
	else
	{
		if (userTrap)
		{
			/*============================================================================================*/
			//TODO: [PROJECT'24.MS2 - #08] [2] FAULT HANDLER I - Check for invalid pointers [DONE]
			//(e.g. pointing to unmarked user heap page, kernel or wrong access rights),
			//your code is here

			//			cprintf("\nUser Trap\n");

			// Accessing Kernel Memory
			if (fault_va >= KERNEL_BASE && fault_va <= KERNEL_HEAP_MAX) {
				//				cprintf("\npointing to kernel\n");
				env_exit();
			}

			int pg_perms = pt_get_page_permissions(faulted_env->env_page_directory, fault_va);
			// Address in user heap BUT Unmarked User Heap Page
			if (fault_va >= USER_HEAP_START && fault_va < USER_HEAP_MAX && !(pg_perms & NO_PERM_MARKED)) {
				//				cprintf("\nNot marked as user\n");
				env_exit();
			}

			// Read-Only Page Violation
			if ((pg_perms & PERM_PRESENT) && (pg_perms & ~PERM_WRITEABLE)) {
				//				cprintf("\nread only\n");
				env_exit();
			}

			/*============================================================================================*/
		}

		/*2022: Check if fault due to Access Rights */
		int perms = pt_get_page_permissions(faulted_env->env_page_directory, fault_va);
		if (perms & PERM_PRESENT)
			panic("Page @va=%x is exist! page fault due to violation of ACCESS RIGHTS\n", fault_va) ;
		/*============================================================================================*/


		// we have normal page fault =============================================================
		faulted_env->pageFaultsCounter ++ ;

		//		cprintf("[%08s] user PAGE fault va %08x\n", curenv->prog_name, fault_va);
		//		cprintf("\nPage working set BEFORE fault handler...\n");
		//		env_page_ws_print(curenv);

		if(isBufferingEnabled())
		{
			__page_fault_handler_with_buffering(faulted_env, fault_va);
		}
		else
		{
			//page_fault_handler(faulted_env, fault_va);
			page_fault_handler(faulted_env, fault_va);
		}
		//		cprintf("\nPage working set AFTER fault handler...\n");
		//		env_page_ws_print(curenv);


	}

	/*************************************************************/
	//Refresh the TLB cache
	tlbflush();
	/*************************************************************/
}

//=========================
// [2] TABLE FAULT HANDLER:
//=========================
void table_fault_handler(struct Env * curenv, uint32 fault_va)
{
	//panic("table_fault_handler() is not implemented yet...!!");
	//Check if it's a stack page
	uint32* ptr_table;
#if USE_KHEAP
	{
		ptr_table = create_page_table(curenv->env_page_directory, (uint32)fault_va);
	}
#else
	{
		__static_cpt(curenv->env_page_directory, (uint32)fault_va, &ptr_table);
	}
#endif
}

//=========================
// [3] PAGE FAULT HANDLER:
//=========================
void page_fault_handler(struct Env * faulted_env, uint32 fault_va)
{
#if USE_KHEAP
	struct WorkingSetElement *victimWSElement = NULL;
	uint32 wsSize = LIST_SIZE(&(faulted_env->page_WS_list));
#else
	int iWS =faulted_env->page_last_WS_index;
	uint32 wsSize = env_page_ws_get_size(faulted_env);
#endif

    fault_va = ROUNDDOWN(fault_va, PAGE_SIZE); // Rounding down to a page boundary

	if(wsSize < (faulted_env->page_WS_max_size))
	{
		//cprintf("PLACEMENT=========================WS Size = %d\n", wsSize );
		//TODO: [PROJECT'24.MS2 - #09] [2] FAULT HANDLER I - Placement [DONE]

		struct FrameInfo* faulted_pg_frame = NULL;
		allocate_frame(&faulted_pg_frame);
		map_frame(faulted_env->env_page_directory, faulted_pg_frame, fault_va, NO_PERM_MARKED | PERM_WRITEABLE | PERM_USER);

		if (pf_read_env_page(faulted_env, (void *) fault_va) != 0) {
			if (!(fault_va >= USER_HEAP_START && fault_va < USER_HEAP_MAX)
					&& !(fault_va >= USTACKBOTTOM && fault_va < USTACKTOP)) {
				env_exit();
			}
		}

		struct WorkingSetElement *new_ws_element = env_page_ws_list_create_element(faulted_env, fault_va);
		LIST_INSERT_TAIL(&faulted_env->page_WS_list, new_ws_element);

		if (LIST_SIZE(&(faulted_env->page_WS_list)) == faulted_env->page_WS_max_size)
			faulted_env->page_last_WS_element = LIST_FIRST(&faulted_env->page_WS_list);
		else faulted_env->page_last_WS_element = NULL;

	}
	else
	{
		//TODO: [PROJECT'24.MS3 - #01] [1] FAULT HANDLER II - Replacement [DONE]
        struct WorkingSetElement *current_ws_element = faulted_env->page_last_WS_element; // iterator
        if (current_ws_element == NULL) current_ws_element = LIST_FIRST(&faulted_env->page_WS_list); // wraps around

		int32 max_sweeps = page_WS_max_sweeps;
		if (page_WS_max_sweeps < 0) max_sweeps = page_WS_max_sweeps * -1;

        while (1) // Loop till an element meets the criteria.
        {
            uint32 va = current_ws_element->virtual_address;
            uint32 perm = pt_get_page_permissions(faulted_env->env_page_directory, va);

            if (!(perm & PERM_USED))
            {
            	current_ws_element->sweeps_counter++;
                int32 curr_max_sweeps = max_sweeps;
                if ((perm & PERM_MODIFIED) && (page_WS_max_sweeps < 0)) curr_max_sweeps++;

                if (current_ws_element->sweeps_counter >= curr_max_sweeps)
                {

					if (perm & PERM_MODIFIED) {
						uint32 *faulted_pg_tbl = NULL;
						get_page_table(faulted_env->env_page_directory, va, &faulted_pg_tbl);
						struct FrameInfo *replaced_frame = get_frame_info(faulted_env->env_page_directory, va, &faulted_pg_tbl);
						pf_update_env_page(faulted_env, va, replaced_frame);
					}

                    // Unmap victim page frame
                    unmap_frame(faulted_env->env_page_directory, va);
					tlb_invalidate(faulted_env->env_page_directory, (void*)va);

                    // Prepare new element
                    struct FrameInfo* faulted_pg_frame = NULL;
                    allocate_frame(&faulted_pg_frame);
                    map_frame(faulted_env->env_page_directory, faulted_pg_frame, fault_va, NO_PERM_MARKED | PERM_WRITEABLE | PERM_USER | PERM_USED);

                    if (pf_read_env_page(faulted_env, (void *) fault_va) != 0) { // Check validity
                        if (!(fault_va >= USER_HEAP_START && fault_va < USER_HEAP_MAX)
                                && !(fault_va >= USTACKBOTTOM && fault_va < USTACKTOP)) {
                            env_exit();
                        }
                    }

					current_ws_element->virtual_address = fault_va; // Replace with the fault values
					current_ws_element->sweeps_counter = 0;

					faulted_env->page_last_WS_element = LIST_NEXT(current_ws_element); // Maintain correct order
					if (faulted_env->page_last_WS_element == NULL)
						faulted_env->page_last_WS_element = LIST_FIRST(&faulted_env->page_WS_list);

                    break; /*if (current_ws_element->sweeps_counter > curr_max_sweeps)*/
                }
            }
            else
            {
                pt_set_page_permissions(faulted_env->env_page_directory, va, NO_PERM_MARKED | PERM_WRITEABLE | PERM_USER, PERM_USED);
                current_ws_element->sweeps_counter = 0;
            }

            current_ws_element = LIST_NEXT(current_ws_element);
            if (current_ws_element == NULL) current_ws_element = LIST_FIRST(&faulted_env->page_WS_list);
        }
	}
}


void __page_fault_handler_with_buffering(struct Env * curenv, uint32 fault_va)
{
	//[PROJECT] PAGE FAULT HANDLER WITH BUFFERING
	// your code is here, remove the panic and write your code
	panic("__page_fault_handler_with_buffering() is not implemented yet...!!");
}

