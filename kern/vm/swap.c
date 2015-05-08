#include <types.h>
#include <kern/fcntl.h>
#include <kern/stat.h>
#include <lib.h>
#include <uio.h>
#include <bitmap.h>
#include <synch.h>
#include <thread.h>
#include <current.h>
#include <mainbus.h>
#include <addrspace.h>
#include <vm.h>
#include <vfs.h>
#include <vnode.h>

///Name of swap disk.
static const char swapdiskname[] = "lhd0raw:";

static struct vnode *swapnode;

///Records status of physical pages.
static unsigned long num_swap_pages;

static struct bitmap *swap_map;
static struct lock *swap_lock;

void
swap_bootstrap(void)
{
	struct stat st;
	size_t memsize;
	char path[sizeof(swapdiskname)];

	memsize = mainbus_ramsize();
	
	strcpy(path, swapdiskname);
	vfs_open(path, O_RDWR, 0, &swapnode);
	VOP_STAT(swapnode, &st);

	num_swap_pages = st.st_size / PAGE_SIZE;

	swap_map = bitmap_create(num_swap_pages);
	
	swap_lock = lock_create("swaplock");

	bitmap_mark(swap_map, 0);
}

/* Allocates a fixed space on disk to swap to. */
off_t
swap_alloc(void)
{
	uint32_t index;
	off_t returnoff;

	lock_acquire(swap_lock);

	bitmap_alloc(swap_map, &index);

	lock_release(swap_lock);

	returnoff = index * PAGE_SIZE;
	return returnoff;
}

/* Deallocates place on disk */
void
swap_free(off_t swap_addr)
{
	uint32_t index;

	index = swap_addr / PAGE_SIZE;

	lock_acquire(swap_lock);

	bitmap_unmark(swap_map, index);

	lock_release(swap_lock);
}


void swap_to_mem(paddr_t pa, off_t swap_addr)
{
	(void)pa;
	(void)swap_addr;
}

void swap_to_disk(paddr_t pa, off_t swap_addr)
{
	(void)pa;
	(void)swap_addr;
}












