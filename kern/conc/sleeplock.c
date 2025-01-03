// Sleeping locks

#include "inc/types.h"
#include "inc/x86.h"
#include "inc/memlayout.h"
#include "inc/mmu.h"
#include "inc/environment_definitions.h"
#include "inc/assert.h"
#include "inc/string.h"
#include "sleeplock.h"
#include "channel.h"
#include "../cpu/cpu.h"
#include "../proc/user_environment.h"

void init_sleeplock(struct sleeplock *lk, char *name)
{
	init_channel(&(lk->chan), "sleep lock channel");
	init_spinlock(&(lk->lk), "lock of sleep lock");
	strcpy(lk->name, name);
	lk->locked = 0;
	lk->pid = 0;
}
int holding_sleeplock(struct sleeplock *lk)
{
	int r;
	acquire_spinlock(&(lk->lk));
	r = lk->locked && (lk->pid == get_cpu_proc()->env_id);
	release_spinlock(&(lk->lk));
	return r;
}
//==========================================================================

void acquire_sleeplock(struct sleeplock *lk)
{
	//TODO: [PROJECT'24.MS1 - #13] [4] LOCKS - acquire_sleeplock [DONE]
	
	if(holding_sleeplock(lk)) {
		panic("acquire_sleeplock: lock \"%s\" is already held by the same CPU.", lk->name);
	}

	// Acquire guard spin lock
	acquire_spinlock(&lk->lk);

	// While lock is held by another process
	while (lk->locked) {
		// Sleep on the channel
		sleep(&lk->chan, &lk->lk);
	}

	// Mark the sleep lock status as busy
	lk->locked = 1;
	lk->pid = get_cpu_proc()->env_id;

	// Release guard spin lock
	release_spinlock(&lk->lk);

}

void release_sleeplock(struct sleeplock *lk)
{
	//TODO: [PROJECT'24.MS1 - #14] [4] LOCKS - release_sleeplock [DONE]
	
	if(!holding_sleeplock(lk)) {
		panic("release_sleeplock: lock \"%s\" is either not held or held by another CPU!", lk->name);
	}

	// Acquire guard spin lock
	acquire_spinlock(&lk->lk);

	wakeup_all(&lk->chan);

	// Mark the sleep lock status as free
	lk->locked = 0;
	lk->pid = 0;

	// Release guard spin lock
	release_spinlock(&lk->lk);

}





