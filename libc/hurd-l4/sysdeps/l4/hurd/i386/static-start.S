/* Startup code for statically linked Hurd/i386 binaries.
   Copyright (C) 1998 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

	.text
	.globl _start
_start:
	/* We start out with the following stack layout:
	   4(%esp):	struct hurd_startup_data *startup_data
	   0(%esp):	return address to startup code.   */
	popl %eax	/* Pop return address.  */

	/* _hurd_pre_start has the signature:
	   void *_hurd_pre_start (struct hurd_startup_data *data)
	   The function returns the new stack pointer.
	   The argument is already in place on the stack.  */
	call _hurd_pre_start

	mov %eax, %esp	/* Switch stack.  */
	xorl %edx, %edx	/* Function to be registered with atexit.  */
	jmp _start1	/* And now for something completely different! */

#define _start _start1
#include <sysdeps/i386/elf/start.S>
