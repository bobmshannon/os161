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
#include <mips/trapframe.h>
#include <addrspace.h>

int 
sys_waitpid(pid_t pid, userptr_t status, int options, int *errcode) {
	int err;
	(void)options;		// Not yet implemented, nor is it required to be
						// for this class.
	/* Error checking. */
	if(pid < 0 || pid >= OPEN_MAX) {
		DEBUG(DB_PROCESS_SYSCALL, "\n ERROR: pid #%d calling waitpid() on child process #%d, but process id is invalid\n", curthread->t_pid, pid);
		(*errcode) = ESRCH;
		return -1;
	}
	else if(pid == 0) {
		DEBUG(DB_PROCESS_SYSCALL, "\n ERROR: pid #%d calling waitpid() on child process #0, but process is not their child\n", curthread->t_pid);
		(*errcode) = EINVAL;
		return -1;
	}
	/*else if(curthread->t_pid != process_table[pid]->ppid) {
		DEBUG(DB_PROCESS_SYSCALL, "\n ERROR: pid #%d calling waitpid() on child process #%d, but process is not their child\n", curthread->t_pid, pid);
		(*errcode) = ECHILD;
		return -1;
	}*/
	
	DEBUG(DB_PROCESS_SYSCALL, "\nprocess #%d waiting for pid #%d", curthread->t_pid, pid);
	
	if(process_table[pid]->has_exited) {
		/* Skip P'ing the semaphore because the process has already exited 
		 * and we do not need to wait. This will prevent a deadlock.
		 */
	}
	else {
		P(process_table[pid]->wait_sem);
	}
	
	/* Send exit code back to waitpid caller. */
	int *exitcode;
	exitcode = kmalloc(sizeof(int));
	if(exitcode == NULL) {
		(*errcode) = EFAULT;
		return -1;		
	}
	*exitcode = process_table[pid]->exitcode;
	
	err = copyout(exitcode, status, sizeof(int));
	if(err) {
		(*errcode) = EFAULT;
		return -1;
	}
	
	kfree(exitcode);
	
	DEBUG(DB_PROCESS_SYSCALL, "\nprocess #%d no longer waiting for pid #%d", curthread->t_pid, pid);
	
	/* Free up slot in process table. */
	remove_process_entry(pid);
	
	return pid;
}

void
sys__exit(int code) {
	DEBUG(DB_PROCESS_SYSCALL, "\nkernel: pid #%d exiting...\n", sys_getpid());
	pid_t pid = sys_getpid();
	process_table[pid]->has_exited = true;
	process_table[pid]->exitcode = _MKWAIT_EXIT(code);
	
	V(process_table[pid]->wait_sem);
	
	thread_exit();
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
	
	/* Free up slot in process table. */
	remove_process_entry(pid);
	
	return pid;
}

pid_t
sys_getpid() {
	return curthread->t_pid;
}

pid_t
sys_fork(struct trapframe *tf, int *errcode) {
	struct thread **ret;		// (*ret) is a pointer to the child.
	struct addrspace **retaddr; // (*retaddr) is a pointer to address space copy.
	
	struct thread *newthread;
	struct trapframe *tf_child;
	
	int i, dummy, err;
	
	/* Copy trapframe */
	tf_child = kmalloc(sizeof(struct trapframe));
	if(tf_child == NULL) {
		return -1;
		*errcode = ENOMEM;	
	}
	memcpy(tf_child, tf, sizeof(struct trapframe));
	
	/* Call thread_fork */
	err = thread_fork("forked pid", (void *)enter_forked_process, tf_child, dummy, ret);
	if(err) {
		return -1;
		*errcode = ENOMEM;
	}
	newthread = *ret;
	
	DEBUG(DB_PROCESS_SYSCALL, "\nprocess #%d calling fork(), new thread spawned with pid #%d\n", curthread->t_pid, newthread->t_pid);
	
	(void)i;
	(void)retaddr;
	*errcode = 0;
	return newthread->t_pid;
}

void 
enter_forked_process(struct trapframe *tf_child) {
	struct trapframe tf;
	
	tf_child->tf_v0 = 0;
	tf_child->tf_a0 = 0;
	tf_child->tf_epc += 4;
	
	as_activate(curthread->t_addrspace);
	
	tf = *tf_child;
	
	DEBUG(DB_PROCESS_SYSCALL, "\nprocess #%d entering user mode\n", curthread->t_pid);
	
	mips_usermode(&tf);
}

int 
sys_execv(userptr_t progname, userptr_t args, int *errcode)
{
	size_t bytescopied;
	char dest[PATH_MAX + 1];
	int err;

	(void)progname;
	(void)args;
	(void)errcode;
	
	/* Error checking. */
	err = copyinstr(progname, dest, PATH_MAX, &bytescopied);
	if(err != 0) {
		(*errcode) = EFAULT;
		return -1;
	}
	
	err = copyinstr(args, dest, PATH_MAX, &bytescopied);
	if(err != 0) {
		(*errcode) = EFAULT;
		return -1;
	}
	
	return 0;
}

