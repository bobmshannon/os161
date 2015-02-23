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
#include <lib.h>
#include <array.h>
#include <synch.h>
#include <vnode.h>
#include <vfs.h>
#include <copyinout.h>
#include <kern/fcntl.h>
#include <current.h>
#include <syscall.h>
#include <types.h>

int 
sys_open(const_userptr_t path, int flags, int mode) {
	struct vnode *v;
	int i;
	size_t actual;
	char *pathname = (char *)kmalloc(sizeof(char) * NAME_MAX);
	int err;
	
	/* Copy in path name from user space */
	err = copyinstr(path, pathname, NAME_MAX, &actual);
	if(err != 0) {
		return err;
	}
	
	/* Open vnode and assign pointer to v */
	vfs_open(pathname, flags, mode, &v);
	
	/* Find open slot in file descriptor table */
	if(curthread->t_fd_table == NULL) {
		init_fd_table();
	}
	
	for(i = 0; i < OPEN_MAX; i++) {
		if(curthread->t_fd_table[i] == NULL) {
			break;
		}
	}
	
	/* Initialize file descriptor */
	curthread->t_fd_table[i]->flags = flags;
	curthread->t_fd_table[i]->offset = 0;
	curthread->t_fd_table[i]->ref_count = 1;
	curthread->t_fd_table[i]->lock = lock_create(pathname);
	curthread->t_fd_table[i]->vn = v;
	
	/* Acquire lock */
	//lock_acquire(curthread->t_fd_table[i]->lock);
	
	return i;
}

int
sys_close(int fd) {
	if(curthread->t_fd_table[fd] != NULL) {
		/* Close vnode and free memory */
		vfs_close(curthread->t_fd_table[fd]->vn);
		lock_destroy(curthread->t_fd_table[fd]->lock);
		kfree(curthread->t_fd_table[fd]->vn);
		kfree(curthread->t_fd_table[fd]);	
		return 0;
	}
	return -1;
}


