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

#ifndef _VM_H_
#define _VM_H_


#include <synch.h>
#include <uio.h>

/* Define a default page size of 4KB */
#define PAGE_SIZE 4096

/* Permission masks */
#define PAGE_READABLE 4
#define PAGE_WRITABLE 2
#define PAGE_EXECUTABLE 1

/*
 * VM system-related definitions.
 */
 
/* Coremap entry */
struct coremap_entry {
	/* Syncrhonization */
	bool is_locked;
	struct spinlock lock;
	
	/* Page Management */
	bool referenced;                /* Is this page referenced? */
	paddr_t pbase;					/* Base physical address of page */
	vaddr_t vbase;					/* Base virtual address of page */
	bool is_free;					/* Is the page free? */
	bool is_permanent;				/* Can the page be swapped out? */
	bool is_last;					/* Is the page the last one in a chunk? */
	int permissions;				/* Permission flags */
	
	/* TLB Management Fields */
	struct addrspace *as;			/* Corresponding address space that page belongs to */
	vaddr_t as_vbase;				/* Base virtual address in address space */
	int cpuid;						/* CPU where TLB entry resides */
	int state;                      /* Current state of page (dirty, clean, etc.) */
};

/* Coremap structure */
struct coremap_entry *coremap;

/* Coremap synchronization */
struct spinlock coremap_lock;

/* Global VM system fields */
int npages;                          /* Number of pages on system. */
int nfreepages;
bool vm_bootstrapped;                /* Is the VM system bootstrapped? */
paddr_t free_start;                  /* Pointer to first page's location in physical memory */

#include <machine/vm.h>

/* Fault-type arguments to vm_fault() */
#define VM_FAULT_READ        0    /* A read was attempted */
#define VM_FAULT_WRITE       1    /* A write was attempted */
#define VM_FAULT_READONLY    2    /* A write to a readonly page was attempted*/


/* Initialization function */
void vm_bootstrap(void);

/* Fault handling function called by trap code */
int vm_fault(int faulttype, vaddr_t faultaddress);

/* Allocate/free kernel pages (called by kmalloc/kfree) */
vaddr_t alloc_kpages(int npages);
void free_kpages(vaddr_t addr);

/* Single page utility functions */
int alloc_page(void);
void free_page(vaddr_t addr);
void copy_page(int src, int dst);
int get_coremap_index(vaddr_t vbase);

/* TLB shootdown handling called from interprocessor_interrupt */
void vm_tlbshootdown_all(void);
void vm_tlbshootdown(const struct tlbshootdown *t);

/* Swapping */
void swap_bootstrap(void);
off_t swap_alloc(void);
void swap_free(off_t swap_addr);
void swap_to_mem(paddr_t pa, off_t swap_addr);
void swap_to_disk(paddr_t pa, off_t swap_addr);


#endif /* _VM_H_ */
