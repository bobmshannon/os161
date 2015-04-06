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

void vm_bootstrap() {
	paddr_t lo, hi, i, free;
	int npages, j;
	
	/* Determine number of pages to allocate */
	npages = 0;
	ram_getsize(&lo, &hi);
	for(i = 0; i < hi - lo; i += PAGE_SIZE) {
		if(i < hi - lo) {
			npages++;
		}
	}
	
	/* Allocate the coremap.
	 * (npages * sizeof(struct coremap_entry)) bytes is allocated for the coremap it self.
	 * The rest of the free memory is used to store each page.
	 */
	coremap = (struct coremap_entry *)PADDR_TO_KVADDR(lo);
	free = lo + npages * sizeof(struct coremap_entry);
	
	/* Initialize each page in the coremap */
	for(j = 0; j < npages; j++) {
		coremap[j].is_free = true;
		coremap[j].referenced = false;
		coremap[j].vbase = PADDR_TO_KVADDR(free);
		coremap[j].pbase = free;
		coremap[j].cpuid = -1;
		coremap[j].is_permanent = false;
		coremap[j].is_last = false;
		free += PAGE_SIZE;
	}
}
	
int vm_fault(int faulttype, vaddr_t faultaddress) {
	(void)faulttype;
	(void)faultaddress;
	return 0;
}

vaddr_t alloc_kpages(int n) {
	(void)n;
}

/*
vaddr_t alloc_kpages(int n) {
	int i, start, end, match;
	
	KASSERT(n > 0);
	
	for(i = 0; i < n; i++) {
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
			if(npages == match) {
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
	
	for(i = start; i <= end; i++) {
		coremap[i].is_free = false;
		coremap[i].referenced = true;
		coremap[i].is_permanent = true;
		if(i == end) {
			coremap[i].is_last = true;
		}
		// modify additional fields where necessary here 
	}
	
	return coremap[start].vbase;
}*/

void free_kpages(vaddr_t addr) {
	(void)addr;
}

void vm_tlbshootdown_all(void) {

}
void vm_tlbshootdown(const struct tlbshootdown *t) {
	(void)t;
}
