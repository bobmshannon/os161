/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Synchronization primitives.
 * The specifications of the functions are in synch.h.
 */

#include <types.h>
#include <lib.h>
#include <spinlock.h>
#include <wchan.h>
#include <thread.h>
#include <current.h>
#include <synch.h>
#include <spl.h>

////////////////////////////////////////////////////////////
//
// Semaphore.

struct semaphore *
sem_create(const char *name, int initial_count)
{
        struct semaphore *sem;

        KASSERT(initial_count >= 0);

        sem = kmalloc(sizeof(struct semaphore));
        if (sem == NULL) {
                return NULL;
        }

        sem->sem_name = kstrdup(name);
        if (sem->sem_name == NULL) {
                kfree(sem);
                return NULL;
        }

	sem->sem_wchan = wchan_create(sem->sem_name);
	if (sem->sem_wchan == NULL) {
		kfree(sem->sem_name);
		kfree(sem);
		return NULL;
	}

	spinlock_init(&sem->sem_lock);
        sem->sem_count = initial_count;

        return sem;
}

void
sem_destroy(struct semaphore *sem)
{
        KASSERT(sem != NULL);

	/* wchan_cleanup will assert if anyone's waiting on it */
	spinlock_cleanup(&sem->sem_lock);
	wchan_destroy(sem->sem_wchan);
        kfree(sem->sem_name);
        kfree(sem);
}

/*
 * Decrement semaphore, more commonly known as wait()
 * on some other systems.
 *
 * Gives calling thread access to protected resource, 
 * otherwise it is put to sleep and in the waiting queue.
 */
void 
P(struct semaphore *sem)
{
        KASSERT(sem != NULL);

        /*
         * May not block in an interrupt handler.
         *
         * For robustness, always check, even if we can actually
         * complete the P without blocking.
         */
        KASSERT(curthread->t_in_interrupt == false);

	spinlock_acquire(&sem->sem_lock);
        while (sem->sem_count == 0) {
		/*
		 * Bridge to the wchan lock, so if someone else comes
		 * along in V right this instant the wakeup can't go
		 * through on the wchan until we've finished going to
		 * sleep. Note that wchan_sleep unlocks the wchan.
		 *
		 * Note that we don't maintain strict FIFO ordering of
		 * threads going through the semaphore; that is, we
		 * might "get" it on the first try even if other
		 * threads are waiting. Apparently according to some
		 * textbooks semaphores must for some reason have
		 * strict ordering. Too bad. :-)
		 *
		 * Exercise: how would you implement strict FIFO
		 * ordering?
		 */
		wchan_lock(sem->sem_wchan);
		spinlock_release(&sem->sem_lock);
                wchan_sleep(sem->sem_wchan);

		spinlock_acquire(&sem->sem_lock);
        }
        KASSERT(sem->sem_count > 0);
        sem->sem_count--;
	spinlock_release(&sem->sem_lock);
}

/*
 * Increment semaphore, more commonly known as signal() on some
 * other systems.
 *
 * Wakes up a sleeping thread and moves it to the CPU ready queue.
 */
void
V(struct semaphore *sem)
{
        KASSERT(sem != NULL);

	spinlock_acquire(&sem->sem_lock);

        sem->sem_count++;
        KASSERT(sem->sem_count > 0);
		wchan_wakeone(sem->sem_wchan);
	
	spinlock_release(&sem->sem_lock);
}

////////////////////////////////////////////////////////////
//
// Lock.

/*
 * This simple lock is implemented using a binary
 * semaphore, or a counting semaphore initialized
 * to the value 1.
 */
struct lock *
lock_create(const char *name)
{
        struct lock *lock;

        lock = kmalloc(sizeof(struct lock));
        if (lock == NULL) {
                return NULL;
        }

        lock->lk_name = kstrdup(name);
		
        if (lock->lk_name == NULL) {
                kfree(lock);
                return NULL;
        }
		
		lock->lk_holder = NULL;
        
		lock->lk_sem = sem_create(name, 1);
		
		lock->lk_acquired = false;

        return lock;
}

void
lock_destroy(struct lock *lock)
{
        KASSERT(lock != NULL);
		
		lock->lk_holder = NULL;
		
        sem_destroy(lock->lk_sem);
		
        kfree(lock->lk_name);
        kfree(lock);
}

/*
 * Note that we do not need to disable interrupts
 * here, that is taken care of for us when we calling
 * the decrement function P().
 *
 * If the lock is not currently available at then
 * the calling thread will be put to sleep. Once
 * it wakes up, the lk_holder will be updated
 * accordingly
 */
void
lock_acquire(struct lock *lock)
{
	P(lock->lk_sem); 	// decrement semaphore

	lock->lk_holder = curthread;
}

/*
 * Note that we do not need to disable interrupts
 * here, that is taken care of for us when we calling
 * the decrement function P().
 *
 * The call to V() increments the semaphore, and if 
 * necessary wakes up another thread that was waiting
 * for the lock.
 */
void
lock_release(struct lock *lock)
{
	if (CURCPU_EXISTS()) {
		KASSERT(lock->lk_holder == curthread);
	}
	
	lock->lk_holder = NULL;

	V(lock->lk_sem);	// increment semaphore
}

bool
lock_do_i_hold(struct lock *lock)
{
	if (!CURCPU_EXISTS()) {
		return true;
	}

	/* Assume we can read lk_holder atomically enough for this to work */
	return (lock->lk_holder == curthread);
}

////////////////////////////////////////////////////////////
//
// CV


struct cv *
cv_create(const char *name)
{
        struct cv *cv;

        cv = kmalloc(sizeof(struct cv));
        if (cv == NULL) {
                return NULL;
        }

        cv->cv_name = kstrdup(name);
        if (cv->cv_name==NULL) {
                kfree(cv);
                return NULL;
        }
        
		cv->cv_wchan = wchan_create(cv->cv_name);
		if(cv->cv_wchan == NULL) {
				kfree(cv->cv_name);
				kfree(cv);
				return NULL;
		}
        return cv;
}

void
cv_destroy(struct cv *cv)
{
        KASSERT(cv != NULL);
        kfree(cv->cv_name);
		wchan_destroy(cv->cv_wchan);
        kfree(cv);
}

/*
 * As intended, we need to release the lock and put the 
 * calling thread to sleep. Then, once it is woken up
 * again with cv_signal() re-acquire that lock.
 */
void
cv_wait(struct cv *cv, struct lock *lock)
{
		/* Check that the current thread holds the lock */
		KASSERT(lock->lk_holder == curthread);
		
		lock_release(lock);
		
			wchan_lock(cv->cv_wchan);	// prevent race conditions if two threads call cv_wait() at the same time
				wchan_sleep(cv->cv_wchan);
			
		lock_acquire(lock);
}

/*
 * The primary difference to note between signal()
 * and broadcast() is that the former wakes up a
 * single thread, while the latter wakes up ALL
 * sleeping threads waiting for the condition.
 
 */
void
cv_signal(struct cv *cv, struct lock *lock)
{
		KASSERT(lock->lk_holder == curthread);
		
		wchan_wakeone(cv->cv_wchan);
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
		KASSERT(lock->lk_holder == curthread);
		
		wchan_wakeall(cv->cv_wchan);
}

////////////////////////////////////////////////////////////
//
// Reader Writer Locks

struct rwlock * 
rwlock_create(const char *name)
{
	struct rwlock *rwlock;
	(void)name;
	return rwlock;
}

void 
rwlock_destroy(struct rwlock *rwlock)
{
	(void)rwlock;
}

void 
rwlock_acquire_read(struct rwlock *rwlock)
{
	(void)rwlock;
}

void 
rwlock_release_read(struct rwlock *rwlock)
{
	(void)rwlock;
}

void 
rwlock_acquire_write(struct rwlock *rwlock)
{
	(void)rwlock;
}

void 
rwlock_release_write(struct rwlock *rwlock)
{
	(void)rwlock;
}
