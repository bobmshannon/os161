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
#include <uio.h>
#include <vfs.h>
#include <copyinout.h>
#include <kern/fcntl.h>
#include <current.h>
#include <syscall.h>
#include <types.h>
#include <kern/errno.h>
#include <stat.h>

int 
sys_open(const_userptr_t path, int flags, int mode) {
	struct vnode *v;
	struct stat *s;
	char pathname[NAME_MAX];
	size_t len;
	int err, i, filesize;
	
	/* Check flags */
		// vop_open should do this for us....
	
	/* Copy in path name from user space */
	err = copyinstr(path, pathname, NAME_MAX, &len);
	if(err) {
		kprintf("kernel: error copying %s from user space \n", pathname);
		return err;
	}
	
	/* Open vnode and assign pointer to v */
	err = vfs_open(pathname, flags, mode, &v);
	if(err) {
		kprintf("kernel: could not open %s, vop_open returned %d \n", pathname, err);
		return -1;
	}
	
	/* Find open slot in file descriptor table */
	int fd;
	for(i = 0; i < OPEN_MAX; i++) {
		if(curthread->t_fd_table[i]->vn == NULL) {
			fd = i;
			break;
		}
		
		if(i == (OPEN_MAX - 1) && curthread->t_fd_table[i] != NULL) {
			kprintf("kernel: could not open %s, open file limit reached\n", pathname);
			return EMFILE;	// Process's file descriptor table is full
		}
	}
	
	/* Initialize file descriptor */
	curthread->t_fd_table[fd]->flags = flags; 
	if(flags >= 32) {	// O_APPEND was passed in as a flag, set offset to end of file
		filesize = VOP_STAT(v, s);
		curthread->t_fd_table[fd]->offset = filesize;
		kfree(s);
	}
	else {
		curthread->t_fd_table[fd]->offset = 0;
	}
	curthread->t_fd_table[fd]->ref_count = 1;
	curthread->t_fd_table[fd]->lock = lock_create(pathname);
	curthread->t_fd_table[fd]->vn = v;

	kprintf("kernel: successfully opened %s, fd %d \n", pathname, fd);
	return fd;
}

int
sys_read(int fd, userptr_t buf, size_t buflen) {
	/* Initialize some stuff */
	struct uio read;
	struct iovec iov;
	int err;
	char *kbuf = (char *)kmalloc(buflen);
	
	/* Error checking */
	if(fd < 0 || curthread->t_fd_table[fd] == NULL) {
		return EBADF;
	}
	
	else if(buf == NULL) {
		return EFAULT;
	}
	
	else if(curthread->t_fd_table[fd]->flags != O_RDWR || curthread->t_fd_table[fd]->flags != O_RDONLY) {
		return EINVAL;
	}
	
	/* Acquire lock and do the read */
	lock_acquire(curthread->t_fd_table[fd]->lock);
	
		uio_kinit(&iov, &read, (void *)kbuf, buflen, curthread->t_fd_table[fd]->offset, UIO_READ);
		
		read.uio_segflg = UIO_USERSPACE;
		
		err = VOP_READ(curthread->t_fd_table[fd]->vn, &read);
		if(err) {
			lock_release(curthread->t_fd_table[fd]->lock);
			return -1;
		}
		
		curthread->t_fd_table[fd]->offset = read.uio_offset;
		
		copyout((const void *)kbuf, (userptr_t)buf, buflen);
	
	lock_release(curthread->t_fd_table[fd]->lock);
	
	/* Update offset */
	curthread->t_fd_table[fd]->offset += (buflen -  read.uio_resid);
	
	/* Return number of bytes read */
	return buflen - read.uio_resid;
}

int
sys_write(int fd, const_userptr_t buf, size_t nbytes) {
	/* Initialize some stuff */
	struct uio write;
	struct iovec iov;
	int err;
	char *kbuf = (char *)kmalloc(nbytes);
	
	/* Error checking */
	if(fd < 0 || curthread->t_fd_table[fd]->vn == NULL) {
		return EBADF;
	}
	
	else if(buf == NULL) {
		return EFAULT;
	}
	
	else if(curthread->t_fd_table[fd]->flags != O_RDWR || curthread->t_fd_table[fd]->flags != O_RDONLY) {
		return EINVAL;
	}
	
	/* Acquire lock and do the write */
	lock_acquire(curthread->t_fd_table[fd]->lock);
	
		uio_kinit(&iov, &write, kbuf, nbytes, curthread->t_fd_table[fd]->offset, UIO_WRITE);
		
		write.uio_segflg = UIO_USERSPACE;
		
		err = VOP_WRITE(curthread->t_fd_table[fd]->vn, &write);
		if(err) {
			lock_release(curthread->t_fd_table[fd]->lock);
			return -1;
		}
		
		curthread->t_fd_table[fd]->offset = write.uio_offset;
		
		copyout((const void *)kbuf, (userptr_t)buf, nbytes);
	
	lock_release(curthread->t_fd_table[fd]->lock);
	
	return 1;
}


int
sys_close(int fd) {
	if(curthread->t_fd_table[fd]->vn != NULL) {
		/* Close vnode and free memory */
		kprintf("vn_opencount: %d | vn_refcount: %d \n", curthread->t_fd_table[fd]->vn->vn_opencount,curthread->t_fd_table[fd]->vn->vn_refcount);
		//vfs_close(curthread->t_fd_table[fd]->vn);
		lock_destroy(curthread->t_fd_table[fd]->lock);
		kfree(curthread->t_fd_table[fd]->vn);
		kfree(curthread->t_fd_table[fd]);	
		kprintf("kernel: successfully closed fd %d", fd);
		return 0;
	}
	kprintf("kernel: could not close fd %d, it was probably never open \n", fd);
	return -1;
}


