/*
 * channel.c
 *
 *  Created on: Sep 22, 2024
 *      Author: HP
 */
#include "channel.h"
#include <kern/proc/user_environment.h>
#include <kern/cpu/sched.h>
#include <inc/string.h>
#include <inc/disk.h>

//===============================
// 1) INITIALIZE THE CHANNEL:
//===============================
// initialize its lock & queue
void init_channel(struct Channel *chan, char *name)
{
	strcpy(chan->name, name);
	init_queue(&(chan->queue));
}

//===============================
// 2) SLEEP ON A GIVEN CHANNEL:
//===============================
// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
// Ref: xv6-x86 OS code
void sleep(struct Channel *chan, struct spinlock* lk)
{
	//TODO: [PROJECT'24.MS1 - #10] [4] LOCKS - sleep

	// Getting the current process
	struct Env* curr_env = get_cpu_proc();

	// Protecting the queue while adding it to prevent a deadlock
	acquire_spinlock(&ProcessQueues.qlock);

	// Marking process as blocked & Adding the current process to queue
	curr_env->env_status = ENV_BLOCKED;
	enqueue(&(chan->queue), curr_env);

	// Releasing the guard lock for the original sleep lock to allow for other processes to sleep and join the wait.
	release_spinlock(lk);

	// Call the scheduler to resume with other processes
	sched();

	// Once woke up, re-acquire lock
	acquire_spinlock(lk);

	// Releasing the queue for other processes.
	release_spinlock(&ProcessQueues.qlock);
}

//==================================================
// 3) WAKEUP ONE BLOCKED PROCESS ON A GIVEN CHANNEL:
//==================================================
// Wake up ONE process sleeping on chan.
// The qlock must be held.
// Ref: xv6-x86 OS code
// chan MUST be of type "struct Env_Queue" to hold the blocked processes
void wakeup_one(struct Channel *chan)
{
	//TODO: [PROJECT'24.MS1 - #11] [4] LOCKS - wakeup_one
	// Protecting the queue while adding it to prevent a deadlock
	acquire_spinlock(&ProcessQueues.qlock);

	// Getting first process from queue
	if (queue_size(&(chan->queue)) != 0) {
		struct Env* env = dequeue(&(chan->queue));
		if (env) {
			// Mark process as ready
			env->env_status = ENV_READY;
			sched_insert_ready0(env);
		}
	}

	// Releasing the queue for other processes.
	release_spinlock(&ProcessQueues.qlock);
}

//====================================================
// 4) WAKEUP ALL BLOCKED PROCESSES ON A GIVEN CHANNEL:
//====================================================
// Wake up all processes sleeping on chan.
// The queues lock must be held.
// Ref: xv6-x86 OS code
// chan MUST be of type "struct Env_Queue" to hold the blocked processes

void wakeup_all(struct Channel *chan)
{
	//TODO: [PROJECT'24.MS1 - #12] [4] LOCKS - wakeup_all
	//COMMENT THE FOLLOWING LINE BEFORE START CODING
	panic("wakeup_all is not implemented yet");
	//Your Code is Here...

}

