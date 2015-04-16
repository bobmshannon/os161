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
	
	if(!spinlock_do_i_hold(&coremap_lock)) {
		spinlock_acquire(&coremap_lock);
	}
	
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
	
	if(spinlock_do_i_hold(&coremap_lock)) {
		spinlock_release(&coremap_lock);
	}
	
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
		return;                    // An invalid vaddr was passed in, exit. 
	}
	
	if(!spinlock_do_i_hold(&coremap_lock)) {
		spinlock_acquire(&coremap_lock);
	}
	
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
	
	if(spinlock_do_i_hold(&coremap_lock)) {
		spinlock_release(&coremap_lock);
	}
}

int get_coremap_index(vaddr_t vbase) {
	int i;
	
	for(i = 0; i < npages; i++) {
		if(coremap[i].vbase == vbase) {
			return i;
		}
	}

	//DEBUG(DB_VM, "could not find a coremap entry corresponding to virtual address 0x%08x\n", vbase);
	return -1;
}

/*
 * Deep copy a single page in the coremap and its corresponding
 * contents in memory.
 */
void copy_page(int src, int dst) {
	int vsrc, vdst, i;
	char *srcptr, *dstptr;
	
	if(!spinlock_do_i_hold(&coremap_lock)) {
		spinlock_acquire(&coremap_lock);
	}
	
		spinlock_acquire(&coremap[src].lock);
		spinlock_acquire(&coremap[dst].lock);
		
			/* Copy coremap entry information */
			coremap[dst] = coremap[src];
			vsrc = coremap[src].vbase;
			vdst = coremap[dst].vbase;
			
			/* 
			 * Copy memory contents from old page into new one.
			 * We cast it to a char here because a char is a byte,
			 * which allows us to copy a single byte at a time.
			 * Thus the for loop only needs to loop PAGE_SIZE times
			 * which is 4096 bytes.
			 */
			dstptr = (char *)vdst;
			srcptr = (char *)vsrc;
			for(i = 0; i < PAGE_SIZE; i++) {
				dstptr[i] = srcptr[i];
			}
		
		spinlock_release(&coremap[dst].lock);
		spinlock_release(&coremap[src].lock);
		
	if(spinlock_do_i_hold(&coremap_lock)) {
		spinlock_release(&coremap_lock);
	}
}

/*
 * Allocate single page in the coremap.
 * Upon successful allocation, returns an index to the new page 
 * in the coremap. Otherwise returns -1.
 */
 int alloc_page(void) {
	int i;
	
	if(!spinlock_do_i_hold(&coremap_lock)) {
		spinlock_acquire(&coremap_lock);
	}
	
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
			if(spinlock_do_i_hold(&coremap_lock)) {
				spinlock_release(&coremap_lock);
			}
			return i;
		}
		spinlock_release(&coremap[i].lock);
	}
	
	if(spinlock_do_i_hold(&coremap_lock)) {
		spinlock_release(&coremap_lock);
	}
	
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
	int spl, i;
	uint32_t hi, lo;
	
	vaddr_t lowerbound, upperbound;
	struct page_table_entry *entry;
	
	switch(faulttype) {
	    case VM_FAULT_READ:  // 0
			
	    case VM_FAULT_WRITE: // 1
			entry = curthread->t_addrspace->pages->firstentry;
			while(entry != NULL && entry->page != NULL) {
				lowerbound = (entry->page->as_vbase);
				upperbound = lowerbound + PAGE_SIZE;
					if(faultaddress < upperbound && faultaddress >= lowerbound) { 
/*  * This is the page we are looking for. Now, update the TLB with the proper virtual
    * to physical address mapping, i.e. faultaddress --> (entry->page->vbase)*/
						spl = splhigh();/* Walk through TLB and find an open slot */
						for (i=0; i<NUM_TLB; i++) {
							tlb_read(&hi, &lo, i);
							if (lo & TLBLO_VALID) {
								continue;
							}
							hi = faultaddress & 0xFFFFF000;
							lo = entry->page->pbase | TLBLO_DIRTY | TLBLO_VALID; // Mark each entry as valid and writable for now.
							DEBUG(DB_VM, "vm: 0x%08x -> 0x%08x; hi=0x%08x lo=0x%08x\n", faultaddress, entry->page->pbase, hi, lo);
							tlb_write(hi, lo, i);
							break;
						}
					
						splx(spl);
						return 0;
					}
					entry = entry->next;
				}
			return EINVAL;
	    case VM_FAULT_READONLY: // 2
				// Kill thread for violating access permissions.
			return 0;
	    default:
			return EINVAL;	
	}
	
	return 0;
}

/* 
 * Generate a TLB Lo entry according to the below format
 * as defined by the MIPS architecture.
 * -----------------------------------------------------
 * |       PPN        |     MODE     |     RESERVED    |
 * -----------------------------------------------------
 * 31               12  11          8  7               0
 *
 * Here, PPN is the the pbase of the corresponding page
 * in the coremap.
 * The MODE bits set READ/WRITE/EXECUTION permissions, and should
 * match the permissions previously set in the coremap entry. 
 */

/* 
 * Generate a TLB Hi entry according to the below format
 * as defined by the MIPS architecture.
 * -----------------------------------------------------
 * |       VPN        |     PID     |     RESERVED     |
 * -----------------------------------------------------
 * 31               12  11          8  7               0
 *
 * Here, VPN is the upper 20 bits of the virtual address that
 * the user program accessed which caused the TLB fault. 
 * The PID bits represent the process ID of the process to which
 * this TLB entry belongs to.
 */


void vm_tlbshootdown_all(void) {

}
void vm_tlbshootdown(const struct tlbshootdown *t) {
	(void)t;
}
