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
 * Sample/test code for running a user program.  You can use this for
 * reference when implementing the execv() system call. Remember though
 * that execv() needs to do more than this function does.
 */

#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <lib.h>
#include <thread.h>
#include <synch.h>
#include <current.h>
#include <addrspace.h>
#include <vm.h>
#include <vfs.h>
#include <syscall.h>
#include <test.h>
#include <copyinout.h>

/*
 * Load program "progname" and start running it in usermode.
 * Does not return except on error. Supports argument passing
 * from the kernel menu.
 */
int
runprogram(char *progname, userptr_t args, unsigned long nargs)
{
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result, argc, i, total_len, total_size, len, j;
	char **kargs;
	unsigned long count;
	
	kargs = (char **)args;
	
	/* Determine argument count. */
	argc = 0;
	total_len = 0;
	count = 0;
	
	while(count < nargs) {
		len = strlen(kargs[argc]) + 1;		// Add one, since strlen does not take into account NULL terminator.
		len	= (len + 3) & ~(3);				// Round string length up to nearest multiple of 4.
		total_len += len;					// Keep track of the total length, so we know how much space to allocate later.
		argc++;
		count++;
	}
	
	/* Determine number of bytes to allocate for kernel buffer. */
	total_size = total_len * sizeof(char);
	
	/* Copy in each argument one by one, making sure each string begins on boundaries that are evenly divisible by 4. */
	char kargbuf[total_size];
	int kargoffset[argc];
	int ptr_index, upper;
	size_t *actual;
	char *ptr;

	actual = kmalloc(sizeof(int));
	if(actual == NULL) {
		return -1;
	}
	
	ptr_index = 0;
	
	for(i = 0; i < argc; i++) {
		ptr = &kargbuf[ptr_index];                             // Create a pointer to the next free position in the kargbuf array.
		
		len = strlen(kargs[i]) + 1;                            // Determine the total length of the string, including NULL terminator.
		strcpy(ptr,kargs[i]);                                  // Copy in the string.
		
		upper	= ( (len + ptr_index) + 3) & ~(3);             // Determine the next free index in kargbuf that is evenly divisible by 4.

		for(j = ptr_index + len; j < upper; j++) {
			kargbuf[j] = '\0';                                 // Pad slots that we left behind in the previous step with NULL characters.
		}
		
		*actual = 0;                                           // Reset number of characters copied in to zero.
		kargoffset[i] = ptr_index;                             // Store offset. Used to locate the beginning of the i'th argument in the array.
		ptr_index = upper;                                     // Update pointer index to the next free slot evenly divisible by 4.
	}
	
	kfree(actual);

	/* Open the file. */
	result = vfs_open((char *)progname, O_RDONLY, 0, &v);
	if (result) {
		return result;
	}

	/* We should be a new thread. */
	KASSERT(curthread->t_addrspace == NULL);

	/* Create a new address space. */
	curthread->t_addrspace = as_create();
	if (curthread->t_addrspace==NULL) {
		vfs_close(v);
		return ENOMEM;
	}
	
	/* File descriptor table. */
	init_fd_table();

	/* Activate it. */
	as_activate(curthread->t_addrspace);

	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		/* thread_exit destroys curthread->t_addrspace */
		vfs_close(v);
		return result;
	}

	/* Done with the file now. */
	vfs_close(v);

	/* Define the user stack in the address space */
	result = as_define_stack(curthread->t_addrspace, &stackptr);
	if (result) {
		/* thread_exit destroys curthread->t_addrspace */
		return result;
	}
	
	/* Push arguments array to the stack */
	stackptr -= total_size;
	copyout(&kargbuf[0], (userptr_t)stackptr, total_size);
	
	/* Push pointers to each argument in the array to the stack */ 
	int base = stackptr;
	int ptr_value;

	for(i = argc - 1; i >= 0; i--) {
		stackptr -= sizeof(void *);	                           // Decrement the stack pointer and free up space to store the pointer (which is 4 bytes in size on MIPS).
		ptr_value = base + kargoffset[i];
		copyout(&ptr_value, (userptr_t)stackptr, sizeof(void *));
	}

	/* Warp to user mode. */
	enter_new_process(argc-1 /*argc*/, (userptr_t)stackptr /*userspace addr of argv*/,
			  stackptr, entrypoint);
	
	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;
}

