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

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <spinlock.h>
#include <thread.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>

/* Wrap rma_stealmem in a spinlock. */
static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;

/* Declare static helper functions. */
static void init_coremap_entry(int index, paddr_t pbase);
static void zero_page(vaddr_t ptr);
static int get_coremap_index(vaddr_t ptr);

/*
 * Bootstrap OS/161 virtual memory system.
 *
 * Allocates and initializes the coremap, in additional
 * to calculating the number of pages we can fit in physical
 * memory.
 */
void vm_bootstrap() {
	paddr_t lo, hi, free;
	int j;
	
	/* Determine number of pages to allocate */
	ram_getsize(&lo, &hi);	
	npages = (hi - lo) / PAGE_SIZE;	
	
	/* 
	 * Allocate the coremap. (npages * sizeof(struct coremap_entry)) bytes 
	 * is allocated for the coremap it self. The rest of the free memory is 
	 * used to store each page.
	 */
	coremap = (struct coremap_entry *)PADDR_TO_KVADDR(lo);
	free = lo + npages * sizeof(struct coremap_entry);			/* pointer to free memory  */
	free = ROUNDUP(free, PAGE_SIZE);                            /* make sure coremap takes up a whole page or more */
	free_start = free;                                          /* update global VM field */
	npages -= (free - lo) / PAGE_SIZE;		                    /* subtract number of page(s) taken up from coremap */
	spinlock_init(&coremap_lock);								/* Synchronization */
	
	/* Initialize each page in the coremap */
	for(j = 0; j < npages; j++) {
		init_coremap_entry(j, free);
		free += PAGE_SIZE;
	}
	
	vm_bootstrapped = true;
}

static void init_coremap_entry(int index, paddr_t pbase) {
		coremap[index].is_free = true;
		coremap[index].referenced = false;
		coremap[index].vbase = PADDR_TO_KVADDR(pbase);
		coremap[index].pbase = pbase;
		coremap[index].cpuid = -1;
		coremap[index].is_permanent = false;
		coremap[index].is_last = false;
		coremap[index].is_locked = false;
		spinlock_init(&coremap[index].lock);
}

/*
 * Steal a single 4K page of memory.
 *
 * Used to allocate a page of memory before the VM system
 * is bootstrapped.
 */
static paddr_t getppages(unsigned long npages)
{
	paddr_t addr;
	spinlock_acquire(&stealmem_lock);
	addr = ram_stealmem(npages);
	spinlock_release(&stealmem_lock);
	return addr;
}

/*
 * Allocate a contigous chunk of N kernel pages.
 *
 * A page being marked as a kernel page indicates that itoa
 * cannot be swapped out. Thus, kernel pages must be allocated
 * in contiguous chunks.
 */
vaddr_t alloc_kpages(int n) {
	int i, start, end, match;
	match = 0;
	
	KASSERT(n > 0);
	
	if(!vm_bootstrapped) {
		return PADDR_TO_KVADDR(getppages(n));
	}
	
	spinlock_acquire(&coremap_lock);
	
	/* Find a chunk of N contiguous pages to allocate */
	for(i = 0; i < npages; i++) {
		if(coremap[i].is_free && match == 0) {
			start = i;
			match++;
			if(n == match) {
				end = i;
				break;
			}
			continue;
		}
		if(coremap[i].is_free && match != n) {
			match++;
			if(n == match) {
				end = i;
				break;
			}
			continue;
		}
		if(!coremap[i].is_free && match != n) {
			match = 0;
		}
		
		if(match != n && i == npages-1) {
			panic("could not allocate a contiguous block of %d pages", n);
		}
	}
	
	/* Update the state of the allocated page(s) before returning them */ 
	for(i = start; i <= end; i++) {
		coremap[i].is_free = false;
		coremap[i].referenced = true;
		coremap[i].is_permanent = true;
		if(i == end) {
			coremap[i].is_last = true;
		}
		zero_page(coremap[i].vbase);
		// modify additional fields where necessary here 
	}
	
	spinlock_release(&coremap_lock);
	
	return coremap[start].vbase;
}

static void zero_page(vaddr_t vaddr) {
	int i;
	char *ptr;
	ptr = (char *)vaddr;
	for(i = 0; i < PAGE_SIZE; i++) {
		*(ptr+i) = 0;
	}
}

/*
 * Free a contiguous chunk of kernel pages.
 */
void free_kpages(vaddr_t vaddr) {
	int index, start, end;
	
	index = get_coremap_index(vaddr);
	start = index;
	end = index;
	
	if(index == -1) {
		return;                    /* An invalid vaddr was passed in, exit. */
	}
	
	spinlock_acquire(&coremap_lock);
	
	while(!coremap[index].is_last) {
		spinlock_acquire(&coremap[index].lock);
		
		if(coremap[index].is_last) {
			end = index;
			break;
		}
		
		spinlock_release(&coremap[index].lock);
		
		index++;	
	}
	
	for(index = start; index <= end; index++) {
		spinlock_acquire(&coremap[index].lock);
		
		coremap[index].state = -1;
		coremap[index].referenced = false;
		coremap[index].is_free = true;
		coremap[index].is_permanent = false;
		coremap[index].is_last = false;	
		// modify additional fields here where necessary	
		
		spinlock_release(&coremap[index].lock);
	}
	
	spinlock_release(&coremap_lock);
}

static int get_coremap_index(vaddr_t ptr) {
	int i;
	
	for(i = 0; i < npages; i++) {
		if(coremap[i].vbase == ptr) {
			return i;
		}
	}

	DEBUG(DB_VM, "could not find a coremap entry corresponding to virtual address 0x%08x while freeing page \n", ptr);
	return -1;
}

/*
 * Allocate single page in the coremap.
 * Upon successful allocation, returns an index to the new page 
 * in the coremap. Otherwise returns -1.
 */
 int alloc_page(void) {
	int i;
	
	spinlock_acquire(&coremap_lock);
	
	for(i = 0; i < npages; i++) {
		spinlock_acquire(&coremap[i].lock);
		
		if(coremap[i].is_free) {
			coremap[i].is_free = false;
			coremap[i].referenced = true;
			coremap[i].is_permanent = false;
			coremap[i].is_last = true;
			zero_page(coremap[i].vbase);
			// modify additional fields here where necessary
			
			spinlock_release(&coremap[i].lock);
			spinlock_release(&coremap_lock);
			return i;
		}
		spinlock_release(&coremap[i].lock);
	}
	
	spinlock_release(&coremap_lock);
	
	return -1;
 }
 
 /*
 * Free single page in the coremap.
 * Do not use this to free kernel pages that have been allocated
 * in a chunk, otherwise it will corrupt it. Use free_kpages instead.
 */
void free_page(vaddr_t addr) {
			int i = get_coremap_index(addr);
			
			if(i == -1) {
				return ;			/* Not a valid page. */
			}
			
			spinlock_acquire(&coremap[i].lock);
			
			coremap[i].is_free = true;
			coremap[i].referenced = false;
			coremap[i].is_permanent = false;
			coremap[i].is_last = false;
			// modify additional fields here where necessary	
			
			spinlock_release(&coremap[i].lock);
}
	
int vm_fault(int faulttype, vaddr_t faultaddress) {
	(void)faulttype;
	(void)faultaddress;
	return 0;
}

void vm_tlbshootdown_all(void) {

}
void vm_tlbshootdown(const struct tlbshootdown *t) {
	(void)t;
}
