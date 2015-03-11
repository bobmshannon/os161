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
#include <thread.h>
#include <process.h>
#include <kern/wait.h>

int 
sys_execv(const_userptr_t program, char **args, int *errcode) {
	(void)program;
	(void)args;
	(void)errcode;
	
	(void)errcode;
	(void)program;
	(void)args;
	int err = 0;
	char dest[PATH_MAX + 1];
	size_t len = 0;

	/* Error checking. */
	if(program == NULL) {
		(*errcode) = ENOENT;
		return -1;
	}
	
	err = copyinstr(program, dest, PATH_MAX, &len);
	if(err != 0) {
		(*errcode) = EFAULT;
		return -1;
	}
	
	return 0;
}

int 
sys_waitpid(pid_t pid, userptr_t status, int options, int *errcode) {
	(void)pid;
	(void)status;
	(void)options;
	(void)errcode;
	int err;
	(void)err;
	
	/* Error checking. */
	if(pid < 0 || pid >= OPEN_MAX) {
		(*errcode) = ESRCH;
		return -1;
	}
	else if(pid == 0) {
		(*errcode) = EINVAL;
		return -1;
	}
	
	P(process_table[pid]->wait_sem);
	
	/* Send exit code back to waitpid caller. */
	int *exitcode;
	*exitcode = process_table[pid]->exitcode;
	(void)exitcode;
	
	err = copyout(exitcode, status, sizeof(int));
	if(err) {
		(*errcode) = EFAULT;
		return -1;
	}
	// Free up slot in process table.
	remove_process_entry(pid);
	
	return pid;
}

int 
menu_wait(pid_t pid) {
	/* Error checking. */
	if(pid < 0 || pid >= OPEN_MAX) {
		return -1;
	}
	else if(pid == 0) {
		return -1;
	}
	
	P(process_table[pid]->wait_sem);
	
	// Free up slot in process table.
	remove_process_entry(pid);
	
	return pid;
}

pid_t
sys_getpid() {
	return curthread->t_pid;
}

pid_t
sys_fork(void) {
	return 0;
}
	
void
sys__exit(int code) {
	//kprintf("kernel: pid #%d exiting...\n", sys_getpid());
	pid_t pid = sys_getpid();
	process_table[pid]->has_exited = true;
	process_table[pid]->exitcode = code;
	
	V(process_table[pid]->wait_sem);
	
	thread_exit();
}

