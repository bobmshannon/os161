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
#include <addrspace.h>
#include <vm.h>

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */

struct addrspace *
as_create(void)
{
	struct addrspace *as;

	as = kmalloc(sizeof(struct addrspace));
	if (as == NULL) {
		return NULL;
	}

	/* Allocate page table */
	as->pages = kmalloc(sizeof(struct page_table));
	if(as->pages == NULL) {
		return NULL;
	}
	
	/* Initialize */
	as->heap_page = NULL;
	as->heap_break = -1;
	as->heap_max = -1;

	as->pages->firstentry = kmalloc(sizeof(struct page_table_entry));
	as->pages->firstentry->next = NULL;
	as->pages->firstentry->prev = NULL;
	as->pages->firstentry->page = NULL;
	
	return as;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *newas;

	newas = as_create();
	if (newas==NULL) {
		return ENOMEM;
	}
	
	if(old == NULL) {
		return 0;
	}
	
	/*
	 * Copy page table from old address space into new one.
	 * Note that the page table is just a doubly linked list 
	 * whose data element is a pointer to a page in the coremap.
	 */
	struct page_table_entry *oldentry = old->pages->firstentry;
	struct page_table_entry *newentry = newas->pages->firstentry;
	int src, dst;
	
	if(oldentry == NULL) {
		return 0;		// Page table is empty, nothing to copy.
	}
		
	while(oldentry != NULL) {
		src = get_coremap_index(oldentry->page->vbase);
		dst = alloc_page();
		copy_page(src, dst);
		
		/* Copy the page */
		newentry->page = &coremap[dst];
		newentry->next = kmalloc(sizeof(struct page_table_entry));
		newentry->next->prev = newentry;
		newentry->next->next = NULL;
		
		/* Update address space pointer in coremap entry */
		if(!spinlock_do_i_hold(&coremap_lock)) {
			spinlock_acquire(&coremap_lock);
		}
		spinlock_acquire(&coremap[dst].lock);
			
		coremap[dst].as = newas; /* Do the update */
			
		spinlock_release(&coremap[dst].lock);
		if(spinlock_do_i_hold(&coremap_lock)) {
			spinlock_release(&coremap_lock);
		}
		
		/* Go to next element each linked list */
		newentry = newentry->next;
		oldentry = oldentry->next;
	}
	
	*ret = newas;
	return 0;
}

void
as_destroy(struct addrspace *as)
{
	/* Mark each page free and then call kfree() where necessary */
	struct page_table_entry *ptentry;
	struct page_table_entry *tempentry;
	
	ptentry = as -> pages -> firstentry;
	while(ptentry != NULL){
		tempentry = ptentry;
		free_page(ptentry -> page -> vbase);
		ptentry = ptentry -> next;
		kfree(tempentry);
	}
	
	kfree(as->pages);
	kfree(as->pages->firstentry);
	kfree(as);
}

void
as_activate(struct addrspace *as)
{
	/*
	 * Write this.
	 */

	(void)as;  // suppress warning until code gets written
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
		 int readable, int writeable, int executable)
{
	int n, i, index;
	struct coremap_entry *page;
	struct page_table_entry *pte;
	struct region *nregion;
	
	/* Determine how many pages to allocate for this region */
	n = ROUNDUP(sz, PAGE_SIZE);
	n = n / PAGE_SIZE;
	
	/* Allocate each page */
	for(i = 0; i < n; i++) {
			index = alloc_page();
			if(index == -1) {
				return ENOMEM;
			}
			page = &coremap[index];			/* Get pointer to coremap entry */
			
				if(!spinlock_do_i_hold(&coremap_lock)) {
					spinlock_acquire(&coremap_lock);
				}				
				spinlock_acquire(&page->lock);
			
					pte = add_pte(as, page);										/* Add entry to page table */
					page->permissions = readable | writeable | executable;			/* Set page permission flags*/
					page->as_vbase = vaddr;											/* Update base virtual address that page corresponds to (for TLB management) */
					vaddr += PAGE_SIZE;												/* Increment base virtual address (for when a region takes up multiple pages)s */
					// modify additional fields here where necessary
					
				spinlock_release(&page->lock);
				if(spinlock_do_i_hold(&coremap_lock)) {
					spinlock_release(&coremap_lock);
				}
	}
	
	/*
	if(as -> regions != NULL){
		nregion = as -> regions -> prev;
		nregion -> next = (struct region *)kmalloc(sizeof(struct region));
		nregion = nregion -> next;
		nregion -> next = NULL;
		
		as -> regions -> prev = nregion;
	}
	else if(as -> regions == NULL){
		as -> regions = (struct region *)kmalloc(sizeof(struct region));
		as -> regions -> next = NULL;
		as -> regions -> prev = as -> regions;
		nregion = as -> regions;
	}
	
	nregion -> vaddr = vaddr;
	nregion -> permissions = (readable | writeable | executable);
	nregion -> npages = n;
	*/

	return 0;
}

struct page_table_entry *
add_pte(struct addrspace *as, struct coremap_entry *page) {
	struct page_table_entry *entry = as->pages->firstentry;
	
	/* Traverse to the end of the linked list */
	while(entry->next != NULL) {
		entry = entry->next;
	}
	
	/* Add the page table entry */
	entry->next = kmalloc(sizeof(struct page_table_entry));
	entry->next->prev = entry;
	entry->next->next = NULL;
	entry->next->page = page;
	
	entry = entry->next;
	
	return entry;
}

int
as_prepare_load(struct addrspace *as)
{
	 struct page_table_entry *ptentry;
	
	 /* 
	  * Set permissions of each page to READ/WRITE to allow data
	  * to be loaded into each segment of the address space.
	  * i.e. arguments on the stack, the program code, etc.
	  */
	 ptentry = as -> pages -> firstentry;
	 while(ptentry != NULL){
	 	ptentry -> page -> permissions = PAGE_READABLE | PAGE_WRITABLE;
	 	
	 	ptentry = ptentry -> next;
	 }

	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */

	(void)as;
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	/*
	 * Write this.
	 */

	(void)as;

	/* Initial user-level stack pointer */
	*stackptr = USERSTACK;
	
	return 0;
}

