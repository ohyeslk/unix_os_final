/******************************************************************************/
/* Important Fall 2014 CSCI 402 usage information:                            */
/*                                                                            */
/* This fils is part of CSCI 402 kernel programming assignments at USC.       */
/* Please understand that you are NOT permitted to distribute or publically   */
/*         display a copy of this file (or ANY PART of it) for any reason.    */
/* If anyone (including your prospective employer) asks you to post the code, */
/*         you must inform them that you do NOT have permissions to do so.    */
/* You are also NOT permitted to remove or alter this comment block.          */
/******************************************************************************/

#include "globals.h"
#include "errno.h"

#include "util/debug.h"

#include "proc/kthread.h"
#include "proc/kmutex.h"

/*
 * IMPORTANT: Mutexes can _NEVER_ be locked or unlocked from an
 * interrupt context. Mutexes are _ONLY_ lock or unlocked from a
 * thread context.
 */

void
kmutex_init(kmutex_t *mtx)
{
	dbg(DBG_PRINT,"(GRADING1B) Initializing the mutex\n");
	mtx->km_holder = NULL;
	sched_queue_init(&mtx->km_waitq);
}

/*
 * This should block the current thread (by sleeping on the mutex's
 * wait queue) if the mutex is already taken.
 *
 * No thread should ever try to lock a mutex it already has locked.
 */
void
kmutex_lock(kmutex_t *mtx)
{
	KASSERT(curthr && (curthr != mtx->km_holder));
	dbg(DBG_PRINT,"(GRADING1A 5.a) Current thread is not null and doesn't hold the mutex\n");
	if(NULL == mtx->km_holder)
	{

		mtx->km_holder = curthr;
		dbg(DBG_PRINT,"(GRADING1B) Current thread holds the mutex\n");
	}
	else
	{
		dbg(DBG_PRINT,"(GRADING1C 7) Mutex is already taken.Sleep on the mutex's wait queue\n");
		sched_sleep_on(&mtx->km_waitq);
		
	}
}

/*
 * This should do the same as kmutex_lock, but use a cancellable sleep
 * instead.
 */
int
kmutex_lock_cancellable(kmutex_t *mtx)
{
	KASSERT(curthr && (curthr != mtx->km_holder));
	dbg(DBG_PRINT,"(GRADING1A 5.b) Current thread is not null and doesn't hold the mutex\n");
	if(NULL == mtx->km_holder)
	{
		mtx->km_holder = curthr;
		dbg(DBG_PRINT,"(GRADING1C 7) Current thread holds the mutex\n");
	}
	else 
	{
		int sleep_return = sched_cancellable_sleep_on(&mtx->km_waitq);
		dbg(DBG_PRINT,"(GRADING1C 7) Mutex is already taken.Cancellable sleep on the mutex's wait queue\n");
		if(sleep_return == -EINTR)
		{
			dbg(DBG_PRINT,"(GRADING1C 7) Returns -EINTR\n");
			return -EINTR;
		}
	}

        return 0;
}

/*
 * If there are any threads waiting to take a lock on the mutex, one
 * should be woken up and given the lock.
 *
 * Note: This should _NOT_ be a blocking operation!
 *
 * Note: Don't forget to add the new owner of the mutex back to the
 * run queue.
 *
 * Note: Make sure that the thread on the head of the mutex's wait
 * queue becomes the new owner of the mutex.
 *
 * @param mtx the mutex to unlock
 */
void
kmutex_unlock(kmutex_t *mtx)
{
	KASSERT(curthr && (curthr == mtx->km_holder));
	dbg(DBG_PRINT,"(GRADING1A 5.c) Current thread is not null and holds the mutex\n");
	mtx->km_holder = NULL;
	if(((mtx->km_waitq).tq_size!=0))
	{
		mtx->km_holder = sched_wakeup_on(&mtx->km_waitq);
		KASSERT(curthr != mtx->km_holder);
		dbg(DBG_PRINT,"(GRADING1A 5.c) Current thread doesn't have the mutex\n");

	}
	
}
