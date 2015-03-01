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
#include <kern/seek.h>

int 
sys_open(const_userptr_t path, int flags, int mode, int *errcode) {
	struct vnode *v;
	struct stat *s;
	char pathname[NAME_MAX];
	size_t len;
	int err, i, flagresult;
	
	/* Check flags */
		// vop_open should do most of this for us...
		
	/* Error checking */
	if(path == NULL) {
		(*errcode) = EFAULT;
		return -1;
	}
	
	/* Copy in path name from user space */
	err = copyinstr(path, pathname, NAME_MAX, &len);
	if(err) {
		(*errcode) = EFAULT;
		return -1;
	}
	
	/* Error checking */
	if(strlen(pathname) == 0) {
		(*errcode) = ENODEV;
		return -1;
	}
	
	/* Open vnode and assign pointer to v */
	err = vfs_open(pathname, flags, mode, &v);
	if(err) {
		(*errcode) = err;
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
			(*errcode) = EMFILE;	// Process's file descriptor table is full
			return -1;
		}
	}
	
	/* Initialize file descriptor */
	curthread->t_fd_table[fd]->flags = flags; 
	
	if(flags >= 32) {	// O_APPEND was passed in as a flag, set offset to end of file
		VOP_STAT(v, s);
		curthread->t_fd_table[fd]->offset = s->st_size;
		kfree(s);
	}
	else {
		curthread->t_fd_table[fd]->offset = 0;
	}
	
	curthread->t_fd_table[fd]->ref_count = 1;
	curthread->t_fd_table[fd]->lock = lock_create(pathname);
	curthread->t_fd_table[fd]->vn = v;
	
	/* Set whether the file descriptor should be in
	 * read, write or read-write mode
	 */
	flagresult = flags & O_ACCMODE;

	switch(flagresult){
		case O_RDONLY:
			curthread->t_fd_table[fd]->writable = false;
			curthread->t_fd_table[fd]->readable = true;
			break;
		case O_WRONLY:
			curthread->t_fd_table[fd]->writable = true;
			curthread->t_fd_table[fd]->readable = false;
			break;
		case O_RDWR:
			curthread->t_fd_table[fd]->writable = true;
			curthread->t_fd_table[fd]->readable = true;
			break;
		default:
			break;
	}

	return fd;
}

int
sys_read(int fd, userptr_t buf, size_t buflen, int *errcode) {
	/* Initialize some stuff */
	struct uio read;
	struct iovec iov;
	int err;
	char *kbuf;
	
	kbuf = kmalloc(buflen);
	
	/* Copy in data from user space pointer to kernel buffer */
	err = copyin(buf, kbuf, buflen);
	if(err) {
		(*errcode) = EFAULT;
		return -1;
	}
	
	/* Error checking */
	if(fd < 0 || fd >= OPEN_MAX || (curthread->t_fd_table[fd]->vn == NULL)) {
		(*errcode) = EBADF;
		return -1;
	}

	if(!curthread->t_fd_table[fd]->readable){
		(*errcode) = EBADF;
		return -1;
	}
	
	/* Do the read */
	lock_acquire(curthread->t_fd_table[fd]->lock);
		uio_kinit(&iov, &read, kbuf, buflen, curthread->t_fd_table[fd]->offset, UIO_READ);
		err = VOP_READ(curthread->t_fd_table[fd]->vn, &read);
		if(err) {
			(*errcode) = err
			kfree(kbuf);
			return -1;
		}
		
		if(fd == 3) {
			kprintf("\nkernel: read '%s' from fd %d \n", kbuf, fd);
		}
		
		curthread->t_fd_table[fd]->offset += (buflen - read.uio_resid);
	lock_release(curthread->t_fd_table[fd]->lock);
	
	/* Send data back to user's buffer */
	err = copyout((const void *)kbuf, buf, buflen);
	if(err) {
		(*errcode) = EFAULT;
		kfree(kbuf);
		return -1;
	}
	
	kfree(kbuf);
	
	return buflen - read.uio_resid;
}

int
sys_write(int fd, const_userptr_t buf, size_t nbytes, int *errcode) {
	/* Initialize some stuff */
	struct uio write;
	struct iovec iov;
	int err;
	char *kbuf;
	
	kbuf = kmalloc(nbytes);
	
	/* Copy in data from user space pointer to kernel buffer */
	err = copyin(buf, (void *)kbuf, nbytes);
	if(err) {
		(*errcode) = EFAULT;
		return -1;
	}
	
	/* Error checking */
	if((fd < 0) || (curthread->t_fd_table[fd]->vn == NULL) || (fd >= OPEN_MAX)) {
		(*errcode) = EBADF;
		return -1;
	}

	if(!curthread->t_fd_table[fd]->writable){
		(*errcode) = EBADF;
		return -1;
	}
	
	/* Do the write */
	lock_acquire(curthread->t_fd_table[fd]->lock);
		if(fd == 3) {
		kprintf("\nkernel: writing %s to fd %d \n", kbuf, fd);
		}
		uio_kinit(&iov, &write, kbuf, nbytes, curthread->t_fd_table[fd]->offset, UIO_WRITE);
		err = VOP_WRITE(curthread->t_fd_table[fd]->vn, &write);
		if(err) {
			(*errcode) = err;
			kfree(kbuf);
			return -1;
		}
	lock_release(curthread->t_fd_table[fd]->lock);
	
	curthread->t_fd_table[fd]->offset += (nbytes - write.uio_resid);
	
	kfree(kbuf);
	
	return nbytes - write.uio_resid;
}


int
sys_close(int fd, int *errcode) {
	/* Error checking */
	if((fd < 0) || (curthread->t_fd_table[fd]->vn == NULL) || (fd >= OPEN_MAX)) {
		(*errcode) = EBADF;
		return -1;
	}
	
	lock_acquire(curthread->t_fd_table[fd]->lock);
		curthread->t_fd_table[fd]->ref_count -= 1;
	lock_release(curthread->t_fd_table[fd]->lock);
	
	/* Close vnode and free memory */
	if(curthread->t_fd_table[fd]->ref_count == 0) {
		//lock_destroy(curthread->t_fd_table[fd]->lock);
		curthread->t_fd_table[fd]->vn = NULL;
		//kfree(curthread->t_fd_table[fd]->vn);
		//kfree(curthread->t_fd_table[fd]);	
	}
	return 0;
}

int
sys_dup2(int oldfd, int newfd, int *errcode) {
	/* Error Checking */
	if((oldfd < 0) || (curthread->t_fd_table[oldfd]->vn == NULL) || (oldfd >= OPEN_MAX)) {
		(*errcode) = EBADF;
		return -1;
	}

	if((newfd < 0) || (newfd >= OPEN_MAX)){
		(*errcode) = EBADF;
		return -1;
	}

	/* Close newfd if open */
	if(curthread->t_fd_table[newfd]->vn != NULL){
			sys_close(newfd, errcode);
	}

	curthread->t_fd_table[newfd] = curthread->t_fd_table[oldfd];

	return 0;	
}


int
sys__getcwd(userptr_t buf, size_t buflen, int *errcode) {
	/* Initialize some stuff */
	struct uio cwd;
	struct iovec iov;
	int err;
	size_t got;	// Total number of characters sent to user buffer
	char *kbuf;
	
	/* Allocate some memory for the kernel buffer */
	kbuf = kmalloc(buflen);
	
	/* Get the current working directory, store in kernel buffer */
	uio_kinit(&iov, &cwd, kbuf, buflen, 0, UIO_READ);
	
	err = vfs_getcwd(&cwd);
	
	if(err) {
		(*errcode) = err;
		kfree(kbuf);
		return -1; 
	}
	
	/* Send data back to user buffer */
	err = copyoutstr(kbuf, buf, buflen, &got);
	if(err) {
		(*errcode) = EFAULT;
		kfree(kbuf);
		return -1;
	}
	
	kfree(kbuf);
	
	return got;
}

int
sys_chdir(const_userptr_t path, int *errcode) {
	/* Initialize some stuff */
	int err;
	char *kbuf;
	size_t len;
	
	/* Allocate some memory for the kernel buffer */
	kbuf = kmalloc(PATH_MAX);
	
	/* Copy in path name from user space */
	err = copyinstr(path, kbuf, PATH_MAX, &len);
	
	if(err) {
		(*errcode) = EFAULT;
		return -1;
	}
	
	/* Change directory */
	err = vfs_chdir(kbuf);
	
	if(err) {
		kprintf("kernel: changing cwd failed \n");
		(*errcode) = err;
		kfree(kbuf);
		return -1;
	}
	
	kfree(kbuf);
	
	return 0;
}

off_t
sys_lseek(int fd, off_t pos, int whence, int *errcode) {
	off_t newpos;
	int err;
	struct stat filestats;
	
	/* Error Checking */
	if((fd < 0) || (curthread->t_fd_table[fd]->vn == NULL) || (fd >= OPEN_MAX)) {
		(*errcode) = EBADF;
		return -1;
	}
	
	/* Update seek position */
	lock_acquire(curthread->t_fd_table[fd]->lock);
	switch(whence) {
		case SEEK_SET:
			curthread->t_fd_table[fd]->offset = pos;
			newpos = pos;
			break;
		case SEEK_CUR:
			curthread->t_fd_table[fd]->offset += pos;
			newpos = curthread->t_fd_table[fd]->offset;
			break;
		case SEEK_END:
			err = VOP_STAT(curthread->t_fd_table[fd]->vn, &filestats);
			if(err) {
				(*errcode) = err;
				return -1;
			}
			newpos = pos + filestats.st_size;
			curthread->t_fd_table[fd]->offset = newpos;
			break;
		default:
			(*errcode) = EINVAL;
			lock_release(curthread->t_fd_table[fd]->lock);
			return -1;
	}
	lock_release(curthread->t_fd_table[fd]->lock);
	
	if(newpos < 0) {
		(*errcode) = EINVAL;
		return -1;
	}
	
	return newpos;
}
