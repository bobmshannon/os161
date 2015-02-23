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
#include <string.h>
#include <unistd.h>
#include <err.h>
#include "f_hdr.h"

#define SECTOR_SIZE  512


#define BUFFER_SIZE  (2 * SECTOR_SIZE + 1)
#define BIGFILE_SIZE (270 * BUFFER_SIZE)
#define BIGFILE_NAME "large-f"

#define LETTER(x) ('a' + (x % 31))

char fbuffer[BUFFER_SIZE];
char ibuffer[32];


#define DIR_DEPTH      8
#define DIR_NAME       "/t"
#define DIRFILE_NAME   "a"


#define FNAME        "f-testfile"
#define TMULT        50
#define FSIZE        ((SECTOR_SIZE + 1) * TMULT)

#define READCHAR     'r'
#define WRITECHAR    'w'

char cbuffer[SECTOR_SIZE + 1];


/* ===================================================

 */

int
main(int argc, char * argv[])
{
	(void)argc;
	(void)argv;
	int ret;
	ret = open("con:", O_RDONLY, 0666);
	
	if(ret) {
		printf("Successfully opened con:, fd %d\n", ret);
		return 1;
	}
	
	printf("Could not open con: \n");
	return 1;
}


