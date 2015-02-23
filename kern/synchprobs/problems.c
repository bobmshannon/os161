/*
 * Copyright (c) 2001, 2002, 2009
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
 * Driver code for whale mating problem
 */
#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>

/*
 * 08 Feb 2012 : GWA : Driver code is in kern/synchprobs/driver.c. We will
 * replace that file. This file is yours to modify as you see fit.
 *
 * You should implement your solution to the whalemating problem below.
 */

// 13 Feb 2012 : GWA : Adding at the suggestion of Isaac Elbaz. These
// functions will allow you to do local initialization. They are called at
// the top of the corresponding driver code.

struct cv *cv_matchmaker;
struct cv *cv_male;
struct cv *cv_female;

struct lock *lock_female;
struct lock *lock_male;
struct lock *lock_matchmaker;

volatile int male_wqueue;
volatile int female_wqueue;
volatile int matchmaker_wqueue;

void whalemating_init() {
	
	cv_matchmaker = cv_create("matchmaker");
	cv_male = cv_create("male");
	cv_female = cv_create("female");
	
	lock_male = lock_create("male lock");
	lock_female = lock_create("female lock");
	lock_matchmaker = lock_create("matchmaker lock");
	
	male_wqueue = 0;
	female_wqueue = 0;
	matchmaker_wqueue = 0;
}

// 20 Feb 2012 : GWA : Adding at the suggestion of Nikhil Londhe. We don't
// care if your problems leak memory, but if you do, use this to clean up.

void whalemating_cleanup() {
	
  return;
}

void
male(void *p, unsigned long which)
{
	struct semaphore * whalematingMenuSemaphore = (struct semaphore *)p;
  (void)which;
  
  male_start();
	lock_acquire(lock_male);
	if(female_wqueue >= 1 && matchmaker_wqueue >= 1) {
		lock_acquire(lock_female);
			female_wqueue -= 1;
			cv_signal(cv_female, lock_female);
		lock_release(lock_female);
		
		lock_acquire(lock_matchmaker);
			matchmaker_wqueue -= 1;
			cv_signal(cv_matchmaker, lock_matchmaker);
		lock_release(lock_matchmaker);
	}
	else {
		male_wqueue += 1;
		cv_wait(cv_male, lock_male);
	}
	lock_release(lock_male);
  male_end();

  // 08 Feb 2012 : GWA : Please do not change this code. This is so that your
  // whalemating driver can return to the menu cleanly.
  V(whalematingMenuSemaphore);
  return;
}

void
female(void *p, unsigned long which)
{
	struct semaphore * whalematingMenuSemaphore = (struct semaphore *)p;
  (void)which; 
  
  female_start();
	lock_acquire(lock_female);
	if(male_wqueue >= 1 && matchmaker_wqueue >= 1) {
		lock_acquire(lock_male);
			male_wqueue -= 1;
			cv_signal(cv_male, lock_male);
		lock_release(lock_male);
		
		lock_acquire(lock_matchmaker);
			matchmaker_wqueue -= 1;
			cv_signal(cv_matchmaker, lock_matchmaker);
		lock_release(lock_matchmaker);
	}
	else {
		female_wqueue += 1;
		cv_wait(cv_female, lock_female);
	}
	lock_release(lock_female);
  female_end();
  
  // 08 Feb 2012 : GWA : Please do not change this code. This is so that your
  // whalemating driver can return to the menu cleanly.
  V(whalematingMenuSemaphore);
  return;
}

void
matchmaker(void *p, unsigned long which)
{
	struct semaphore * whalematingMenuSemaphore = (struct semaphore *)p;
  (void)which;
  
  matchmaker_start();
	lock_acquire(lock_matchmaker);
	if(male_wqueue >= 1 && female_wqueue >= 1) {
		lock_acquire(lock_male);
			male_wqueue -= 1;
			cv_signal(cv_male, lock_male);
		lock_release(lock_male);
		
		lock_acquire(lock_female);
			female_wqueue -= 1;
			cv_signal(cv_female, lock_female);
		lock_release(lock_female);
	}
	else {
		matchmaker_wqueue += 1;
		cv_wait(cv_matchmaker, lock_matchmaker);
	}
	lock_release(lock_matchmaker);
  matchmaker_end();
  
  // 08 Feb 2012 : GWA : Please do not change this code. This is so that your
  // whalemating driver can return to the menu cleanly.
  V(whalematingMenuSemaphore);
  return;
}

/*
 * You should implement your solution to the stoplight problem below. The
 * quadrant and direction mappings for reference: (although the problem is,
 * of course, stable under rotation)
 *
 *   | 0 |
 * --     --
 *    0 1
 * 3       1
 *    3 2
 * --     --
 *   | 2 | 
 *
 * As way to think about it, assuming cars drive on the right: a car entering
 * the intersection from direction X will enter intersection quadrant X
 * first.
 *
 * You will probably want to write some helper functions to assist
 * with the mappings. Modular arithmetic can help, e.g. a car turning
 * LEFT through the intersection entering from direction X will leave to
 * direction (X + 2) % 4. If going straight you will pass through quadrants 
 * X and (X + 3) % 4.
 * Boo-yah.
 *
 * Your solutions below should call the inQuadrant() and leaveIntersection()
 * functions in drivers.c.
 */
 
 struct lock *quadrant_zero;
 struct lock *quadrant_one;
 struct lock *quadrant_two;
 struct lock *quadrant_three;
 struct lock *intersection_stats;
 
 volatile bool car_in_zero;
 volatile bool car_in_one;
 volatile bool car_in_two;
 volatile bool car_in_three;
 
 volatile int active_cars;
 
 struct semaphore *intersection;
// 13 Feb 2012 : GWA : Adding at the suggestion of Isaac Elbaz. These
// functions will allow you to do local initialization. They are called at
// the top of the corresponding driver code.

void stoplight_init() {
  quadrant_zero = lock_create("quadrant 0");
  quadrant_one = lock_create("quadrant 1");
  quadrant_two = lock_create("quadrant 2");
  quadrant_three = lock_create("quadrant 3");
  
  intersection_stats = lock_create("stats");
  
  active_cars = 0;

  intersection = sem_create("intersection sem", 3);

  return;
}

// 20 Feb 2012 : GWA : Adding at the suggestion of Nikhil Londhe. We don't
// care if your problems leak memory, but if you do, use this to clean up.

void stoplight_cleanup() {
  return;
}

void
gostraight(void *p, unsigned long direction)
{
	struct semaphore * stoplightMenuSemaphore = (struct semaphore *)p;
  (void)direction;
  
  
  /* For a car proceeding STRAIGHT through the intersection, it will pass
   * through the zones as followed.
   *
   *  Origin | Zones Passed Through | Exit Zone (before calling leaveIntersection)
   *     x   |       (x+3)mod4      |                   (x+3)mod4
   * ----------------------------------------------------------------------------
   *     0              3								3
   *	 1			    0								0
   *	 2			    1								1
   *	 3			    2								2
   */
   
   switch(direction) {
	case 0:
		P(intersection);
		while(active_cars >= 3) {
			// wait
		}
		lock_acquire(quadrant_zero);
			active_cars += 1;
			kprintf("number of cars now in intersection: %d \n", active_cars);
			
			kprintf("car moving straight entering from direction %lu\n", direction);
			inQuadrant(0);
			
			lock_acquire(quadrant_three);
			
				lock_release(quadrant_zero);
				
				inQuadrant(3);
				
				leaveIntersection();
				
			lock_release(quadrant_three);
			
			active_cars -= 1;
			kprintf("number of cars now in intersection: %d \n", active_cars);
		V(intersection);
		break;
	case 1:
		P(intersection);
		while(active_cars >= 3) {
			// wait
		}
		lock_acquire(quadrant_one);
			active_cars += 1;
			kprintf("number of cars now in intersection: %d \n", active_cars);
		kprintf("car moving straight entering from direction %lu\n", direction);	
			inQuadrant(1);
			
			lock_acquire(quadrant_zero);
				
				lock_release(quadrant_one);
				
				inQuadrant(0);
				
				leaveIntersection();
			
			lock_release(quadrant_zero);
			
			active_cars -= 1;
			kprintf("number of cars now in intersection: %d \n", active_cars);

		V(intersection);
		break;
	case 2:
		P(intersection);
		while(active_cars >= 3) {
			// wait
		}
		lock_acquire(quadrant_two);
			active_cars += 1;
			kprintf("number of cars now in intersection: %d \n", active_cars);
			
		kprintf("car moving straight entering from direction %lu\n", direction);	
			inQuadrant(2);
			
			lock_acquire(quadrant_one);
				
				lock_release(quadrant_two);
				
				inQuadrant(1);
				
			lock_release(quadrant_one);
			
			active_cars -= 1;
			kprintf("number of cars now in intersection: %d \n", active_cars);
		V(intersection);
		break;
	case 3:
		P(intersection);
		while(active_cars >= 3) {
			// wait
		}
		lock_acquire(quadrant_three);
		active_cars += 1;
		kprintf("number of cars now in intersection: %d \n", active_cars);
		kprintf("car moving straight entering from direction %lu\n", direction);	
			inQuadrant(3);
			
			lock_acquire(quadrant_two);
			
				lock_release(quadrant_three);
				
				inQuadrant(2);
				
				leaveIntersection();
			
			lock_release(quadrant_two);
		active_cars -= 1;
		kprintf("number of cars now in intersection: %d \n", active_cars);
		V(intersection);
		break;
   }
  V(stoplightMenuSemaphore);
  return;
}

void
turnleft(void *p, unsigned long direction)
{
	struct semaphore * stoplightMenuSemaphore = (struct semaphore *)p;
  (void)direction;
  
  // 08 Feb 2012 : GWA : Please do not change this code. This is so that your
  // stoplight driver can return to the menu cleanly.
  
  /* For a car proceeding LEFT through the intersection, it will pass
   * through the zones as followed.
   *
   *  Origin | Zones Passed Through | Exit Zone (before calling leaveIntersection)
   *     x   |       (x+3)mod4      |                   (x+2)mod4
   * ----------------------------------------------------------------------------
   *     0             3,2								2
   *	 1			   0,3								3
   *	 2			   1,0								0
   *	 3			   2,1								1
   */
   
   switch(direction) {
	case 0:
		P(intersection);
		while(active_cars >= 3) {
			// wait
		}
		
		lock_acquire(quadrant_zero);
		
		active_cars += 1;
		kprintf("number of cars now in intersection: %d \n", active_cars);
		kprintf("car moving straight entering from direction %lu\n", direction);	
			inQuadrant(0);
			
			lock_acquire(quadrant_three);
			
			inQuadrant(3);
			
			lock_release(quadrant_zero);
			
					lock_acquire(quadrant_two);
					
					inQuadrant(2);
					
					lock_release(quadrant_three);
					
						leaveIntersection();
						
					lock_release(quadrant_two);
		active_cars -= 1;
		kprintf("number of cars now in intersection: %d \n", active_cars);
		V(intersection);
	case 1:
		P(intersection);
		while(active_cars >= 3) {
			// wait
		}
		lock_acquire(quadrant_one);
		active_cars += 1;
		kprintf("number of cars now in intersection: %d \n", active_cars);
		kprintf("car moving straight entering from direction %lu\n", direction);	
			inQuadrant(1);
			
			lock_acquire(quadrant_zero);
			
			inQuadrant(0);
			
			lock_release(quadrant_one);
			
					lock_acquire(quadrant_three);
					
					inQuadrant(3);
					
					lock_release(quadrant_zero);
					
						leaveIntersection();
						
					lock_release(quadrant_three);
		active_cars -= 1;
		kprintf("number of cars now in intersection: %d \n", active_cars);
		V(intersection);
	case 2:
		P(intersection);
		while(active_cars >= 3) {
			// wait
		}
		lock_acquire(quadrant_two);
		active_cars += 1;
		kprintf("number of cars now in intersection: %d \n", active_cars);
		kprintf("car moving straight entering from direction %lu\n", direction);	
			inQuadrant(2);
			
			lock_acquire(quadrant_one);
			
			inQuadrant(1);
			
			lock_release(quadrant_two);
			
					lock_acquire(quadrant_zero);
					
					inQuadrant(0);
					
					lock_release(quadrant_one);
					
						leaveIntersection();
						
					lock_release(quadrant_zero);
		active_cars -= 1;
		kprintf("number of cars now in intersection: %d \n", active_cars);
		V(intersection);
	case 3:
		P(intersection);
		while(active_cars >= 3) {
			// wait
		}
		lock_acquire(quadrant_three);
		active_cars += 1;
		kprintf("number of cars now in intersection: %d \n", active_cars);
		kprintf("car moving straight entering from direction %lu\n", direction);	
			inQuadrant(3);
			
			lock_acquire(quadrant_two);
			
			inQuadrant(2);
			
			lock_release(quadrant_three);
			
					lock_acquire(quadrant_one);
					
					inQuadrant(1);
					
					lock_release(quadrant_two);
					
						leaveIntersection();
						
					lock_release(quadrant_one);		
		active_cars -= 1;	
		kprintf("number of cars now in intersection: %d \n", active_cars);
		V(intersection);
   }
   
  V(stoplightMenuSemaphore);
  return;
}

void
turnright(void *p, unsigned long direction)
{
	struct semaphore * stoplightMenuSemaphore = (struct semaphore *)p;
  (void)direction;

  // 08 Feb 2012 : GWA : Please do not change this code. This is so that your
  // stoplight driver can return to the menu cleanly.
  
  /* For a car proceeding RIGHT through the intersection, it will pass
   * through the zones as followed.
   *
   *  Origin | Zones Passed Through | Exit Zone (before calling leaveIntersection)
   *     x   |          X           |                   X
   * ----------------------------------------------------------------------------
   *     0              0								0
   *	 1			    1								1
   *	 2			    2								2
   *	 3			    3								3
   */

   switch(direction) {
	case 0:
		P(intersection);
		lock_acquire(quadrant_zero);
		active_cars += 1;
		kprintf("car moving straight entering from direction %lu\n", direction);
			inQuadrant(0);
			
			leaveIntersection();
			
		lock_release(quadrant_zero);
		active_cars -= 1;
		V(intersection);
	case 1:
		P(intersection);
		
		lock_acquire(quadrant_one);
		active_cars += 1;
		kprintf("number of cars now in intersection: %d \n", active_cars);
		kprintf("car moving straight entering from direction %lu\n", direction);
			inQuadrant(1);
			
			leaveIntersection();
		active_cars -= 1;
		kprintf("number of cars now in intersection: %d \n", active_cars);		
		lock_release(quadrant_one);	
		
		V(intersection);
	case 2:
		P(intersection);
		
		lock_acquire(quadrant_two);
		active_cars += 1;
		kprintf("number of cars now in intersection: %d \n", active_cars);
		kprintf("car moving straight entering from direction %lu\n", direction);
			inQuadrant(2);
			
			leaveIntersection();
			
		lock_release(quadrant_two);
		active_cars -= 1;
		kprintf("number of cars now in intersection: %d \n", active_cars);
		V(intersection);
	case 3:
		P(intersection);
		
		lock_acquire(quadrant_three);
		active_cars += 1;
		kprintf("car moving straight entering from direction %lu\n", direction);
		kprintf("number of cars now in intersection: %d \n", active_cars);
			inQuadrant(3);
			
			leaveIntersection();
			
		lock_release(quadrant_three);
		active_cars -= 1;
		kprintf("number of cars now in intersection: %d \n", active_cars);
		V(intersection);
	}
  V(stoplightMenuSemaphore);
  return;
}
