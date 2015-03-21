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
<<<<<<< HEAD
#include <wchan.h>
#include <addrspace.h>
#include <mips/trapframe.h>

pid_t
sys_getpid(void) {
	return curthread->t_pid;
}

int
sys_execv(const_userptr_t program, char **args, int *errcode) {

	(void)errcode;
	(void)program;
	(void)args;
	int err = 0;
	int argc = 0;
	char dest[PATH_MAX + 1];
	size_t len = 0;
	
	err = copyinstr(program, dest, PATH_MAX, &len);
	if(err != 0) {
		(*errcode) = EFAULT;
		return -1;
	}
	(void)argc;
	
	/*
	int errcheck = 0;
	char dest[PATH_MAX + 1];
	size_t len = 0;
	struct vnode *vn = 0;
	
	//Copy file name from userspace to kernelspace
	errcheck = copyinstr(program, dest, PATH_MAX, &len);
	if(errcheck != 0) {
		kprintf("copyinstr() failed.\n");
		return -1;
	}
	
	//If copied string is of length 0.
	if(len == 0) {
		(*errcode) = ENOENT;
		return -1;
	}
	
	errcheck = vfs_open(dest, O_RDONLY, 0, &vn);
	if(errcheck) {
		(*errcode) = ENOENT;
		return -1;
	}
	
	
	char kernel_args[NAME_MAX];
	char **kargs;
	
	//Kernel Buffer
	kargs = (char **)kmalloc(sizeof(char))
	
	//Copy arguments from userspace to kernel buffer
	if(args != 0){
		while(args[argc] != 0){
			if(argc >= ARG_MAX){
				(*errcode) = E2BIG;
				return -1;
			}
			
			len = 0;
			copyinstr((const_userptr_t)args[argc], kernel_args, NAME_MAX, &len);
			kernel_args[len] = 0;
			kargs[argc] = kernel_args;
			
			argc++;
		}			
	}
	*/
	
	return 0;
}

void
sys__exit(int code) {	
	pid_t pid = sys_getpid();
	process_table[pid]->has_exited = true;
	process_table[pid]->exitcode = code;
	
	/* Let's keep this really simple by assuming that only
	 * one process can wait on a specific PID, which in most
	 * cases would be the parent who called fork. Thus, we
	 * only decrement the waiting semaphore once.
	 */
=======
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
	else if(curthread->t_pid != process_table[pid]->ppid) {
		DEBUG(DB_PROCESS_SYSCALL, "\n ERROR: pid #%d calling waitpid() on child process #%d, but process is not their child\n", curthread->t_pid, pid);
		(*errcode) = ECHILD;
		return -1;
	}
	
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
	
>>>>>>> fspass
	V(process_table[pid]->wait_sem);
	
	thread_exit();
}

<<<<<<< HEAD
pid_t 
sys_waitpid(pid_t pid, userptr_t status, int options, int *errcode) {
	// TODO: handle options argument checking.
	int err;
	/* Error checking. */
	if(pid < 0 || pid == 0 || pid > RUNNING_MAX) {
		(*errcode) = ESRCH;
		return -1;
	}
	
	P(process_table[pid]->wait_sem);
	
	int *exitcode;
	*exitcode = _MKWAIT_EXIT(process_table[pid]->exitcode);

	/* Send exit code back to user */
	err = copyout(exitcode, status, sizeof(int));
	if(err) {
		(*errcode) = EFAULT;
		return -1;
	}
	
	remove_process_entry(pid);
	
	(void)status;
	(void)options;
=======
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
	
>>>>>>> fspass
	return pid;
}

pid_t
<<<<<<< HEAD
sys_fork(struct trapframe *tf) {
	struct thread **ret;		// (*newthread) is a pointer to the child.
=======
sys_getpid() {
	return curthread->t_pid;
}

pid_t
sys_fork(struct trapframe *tf, int *errcode) {
	struct thread **ret;		// (*ret) is a pointer to the child.
>>>>>>> fspass
	struct addrspace **retaddr; // (*retaddr) is a pointer to address space copy.
	
	struct thread *newthread;
	struct trapframe *tf_child;
	
	int i, dummy, err;
	
	/* Copy trapframe */
	tf_child = kmalloc(sizeof(struct trapframe));
<<<<<<< HEAD
=======
	if(tf_child == NULL) {
		return -1;
		*errcode = ENOMEM;	
	}
>>>>>>> fspass
	memcpy(tf_child, tf, sizeof(struct trapframe));
	
	/* Call thread_fork */
	err = thread_fork("forked pid", (void *)enter_forked_process, tf_child, dummy, ret);
	if(err) {
<<<<<<< HEAD
		panic("thread_fork failed.");
	}
	newthread = *ret;
	
	/* Copy parents address space 
	retaddr = kmalloc(sizeof(struct addrspace));
	err = as_copy(curthread->t_addrspace, retaddr);
	if(err) {
		panic("as_copy failed.");
	}
	newthread->t_addrspace = *retaddr;
	*/
	
	/* Copy file table from parent to child. 
	for(i = 0; i < OPEN_MAX; i++) {
		newthread->t_fd_table[i] = kmalloc(sizeof(struct fd));
		newthread->t_fd_table[i]->readable = curthread->t_fd_table[i]->readable;
		newthread->t_fd_table[i]->writable = curthread->t_fd_table[i]->writable;
		newthread->t_fd_table[i]->flags = curthread->t_fd_table[i]->flags;
		newthread->t_fd_table[i]->offset = curthread->t_fd_table[i]->offset;
		newthread->t_fd_table[i]->ref_count = curthread->t_fd_table[i]->ref_count;
		newthread->t_fd_table[i]->vn = curthread->t_fd_table[i]->vn;
		newthread->t_fd_table[i]->lock = curthread->t_fd_table[i]->lock;
		//newthread->t_fd_table[i]->lock = lock_create("lock");
	}*/
	
	(void)i;
	(void)retaddr;
	return newthread->t_pid;
=======
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
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result, argc, i, total_len, total_size, len, err, j;
	char **kargs;
	
	kargs = (char **)args;
	argc = 0;
	total_len = 0;
	
	(void)progname;
	(void)args;
	(void)errcode;
	(void)argc;
	(void)i;
	
	/* Determine argument count. */
	while(kargs[argc] != NULL) {
		len = strlen(kargs[argc]) + 1;		// Add one, since strlen does not take into account NULL terminator.
		len	= (len + 3) & ~(3);				// Round string length up to nearest multiple of 4.
		total_len += len;					// Keep track of the total length, so we know how much space to allocate later.
		argc++;
	}
	
	/* Determine number of bytes to allocate for kernel buffer. */
	total_size = total_len * sizeof(char);
	
	/* Copy in each argument one by one, making sure each string begins on boundaries that are evenly divisible by 4. */
	char kargbuf[total_size];
	char kargoffset[argc];
	int ptr_index, upper;
	size_t *actual;
	char *ptr;

	actual = kmalloc(sizeof(int));
	if(actual == NULL) {
		// something went wrong, return error.
	}
	ptr_index = 0;
	
	for(i = 0; i < argc; i++) {
		ptr = &kargbuf[ptr_index];										// Create a pointer to the next free position in the kargbuf array.
		err = copyinstr((const_userptr_t)kargs[i], ptr,
						strlen(kargs[i]), actual);						// Copy in the string.
		upper	= ( (*actual + ptr_index - 1) + 3) & ~(3);				// Determine the next free index in kargbuf that is evenly divisible by 4.		

		
		for(j = ptr_index + *actual; j < upper; j++) {
			kargbuf[j] = '\0';											// Pad slots that we left behind in the previous step with NULL characters.
		}
		
		*actual = 0;													// Reset number of characters copied in to zero.
		kargoffset[i] = ptr_index;										// Store offset. Used to locate the beginning of the i'th argument in the array.
		ptr_index = upper;												// Update pointer index to the next free slot evenly divisible by 4.
	}
	
	kfree(actual);

	/*
	char a[10];
	char *x = a;
	char *y = &a[0];
	
	copyinstr(const_userptr_t usersrc, char *dest, size_t len, size_t *actual)
	*/

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
	
	/* Copy arguments to the user stack. */
	stackptr -= total_size;
	copyout(&kargbuf, (userptr_t)stackptr, total_size);
	
	/* Warp to user mode. */
	enter_new_process(0 /*argc*/, NULL /*userspace addr of argv*/,
			  stackptr, entrypoint);
	
	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;

>>>>>>> fspass
}

void 
enter_forked_process(struct trapframe *tf_child) {
	//kprintf("kernel: entering forked process...\n");
	struct trapframe tf;
	
	tf_child->tf_v0 = 0;
	tf_child->tf_a0 = 0;
	tf_child->tf_epc += 4;
	
	as_activate(curthread->t_addrspace);
	
	tf = *tf_child;
	
	mips_usermode(&tf);

}
