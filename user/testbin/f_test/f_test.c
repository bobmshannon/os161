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
 * Razvan Surdulescu
 * abhi shelat
 * April 28 1997
 * 
 * Test suite for Nachos HW4--The Filesystem
 *
 * Modified by dholland 1/31/2001 for OS/161
 *
 * This should run successfully (on SFS) when the file system
 * assignment is complete.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <kern/limits.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include "f_hdr.h"
#include <stdlib.h>


int
main(int argc, char * argv[])
{
	int ret,fd0,fd1,fd2,i,err;
	char buf[] = "\nThis is some test text used for writing. Take a seat. \n";
	char rbuf[128];
	char path[] = "test.txt";
	char dir[] = "../";
	char cwd[128], *p;
	off_t pos;
	
	printf("\n------------------------------------------------\n");
	printf("       STARTING F_TEST WITH PID #%d\n", getpid());
	printf("------------------------------------------------\n");
	
	/* Open and read a file */
	fd0 = open(path, O_RDWR | O_CREAT, 0666);
	if(fd0 < 0) {
		printf("\nopen(%s) failed \n", path);
	}
	else {
		printf("\nopen(%s) succeeded \n", path);
	}
	
	err = write(fd0, &buf, strlen(buf)); 
	if(err == -1) {
		printf("\nwrite(%s) failed \n", path);
	}
	else {
		printf("\nWrote %d bytes to %s \n", err, path);
	}
	
	lseek(fd0, 0, SEEK_SET);
	
	err = read(fd0, &rbuf, 128);
	if(err == -1) {
		printf("\nread(%s) failed \n", path);
	}
	else {
		printf("\nRead %d bytes from %s:\n %s", err, path, rbuf);
	}
	
	/* Open & close a file
	fd0 = open(path, O_RDONLY, 0666);
	if(fd0 < 0) {
		printf("\nopen(%s) failed \n", path);
	}
	else {
		printf("\nopen(%s) succeeded \n", path);
	}
	ret = close(fd0);
	if(ret) {
		printf("close(%s) failed", path);
	}
	else {
		printf("close(%s) suceeded", path);
	} */
	
	/* chdir test 
	err = chdir(dir);
	if(err) {
		printf("Error changing directory to %s \n", dir);
	}
	else {
		printf("Successfully changed directory to %s \n", dir);
	}*/
	
	/* getcwd test 
	//getcwd(cwd, 128);
	//printf("Current working directory is %s \n", cwd);
	chdir(".");
	getcwd(cwd, 128);
	printf("Current working directory is %s \n", cwd);
	*/
	/* lseek test 
	fd0 = open(path, O_RDONLY, 0666);
	pos = 0;
	lseek(fd0, pos, SEEK_END);
*/
	/* Print some output 
	for(i = 0; i < 20; i++) {
		printf(buf);
	}*/

	(void)cwd;
	(void)dir;
	(void)err;
	(void)fd1;
	(void)fd2;
	(void)argc;
	(void)argv;
	(void)buf;
	(void)i;
	(void)path;
	(void)fd0;
	(void)ret;
	(void)pos;
	(void)p;
	
	return 0;
}


