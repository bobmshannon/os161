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
#include <wchan.h>

int 
sys_execv(const_userptr_t program, char **args, int *errcode) {
	(void)program;
	(void)args;
	(void)errcode;
	
	/* Error checking. */
	if(program == NULL) {
		(*errcode) = ENOENT;
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
	
	/* Error checking. */
	if(pid < 0) {
		(*errcode) = ESRCH;
		return -1;
	}
	else if(pid == 0) {
		(*errcode) = ECHILD;
		return -1;
	}
	
	return 0;
}

pid_t
sys_getpid(void) {
	return curthread->t_pid;
}

void
sys__exit(int code) {
	//kprintf("kernel: pid #%d exiting...\n", sys_getpid());

	pid_t pid = sys_getpid();
	process_table[pid]->has_exited = true;
	process_table[pid]->exitcode = code;
	
	process_table[pid]->waiting = 0;
	
	thread_exit();
}

pid_t
waitpid(pid_t pid, int *status, int options) {
	/* Implement error checking stuff here.
	 * Refer to man page and OS/161 blog
	 * for more information. */
	 
	process_table[pid]->waiting += 1;

	(void)status;
	(void)options;
	return pid;
}
