/* ia32-crt0.S - Startup code for ia32.
   Copyright (C) 2003 Free Software Foundation, Inc.
   Written by Marcus Brinkmann.

   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2, or (at
   your option) any later version.

   The GNU Hurd is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA. */

#define ASM	1
#include "multiboot.h"

/* The size of our stack (16*4KB).  */
#define STACK_SIZE			0x10000
	
	.text

	.globl	start, _start
start:
_start:
	/* Initialize the stack pointer.  */
	movl	$(stack + STACK_SIZE), %esp

	/* Reset EFLAGS.  */
	pushl	$0
	popf

	/* Now enter the cmain function.  Instead of using call, we
	jump and put a return address of 0 on the stack.  We also zeor
	the frame pointer.  In this way, __builtin_return_address
	works more reliably.  */
	pushl $0
	xor %ebp, %ebp
	jmp cmain

	/* Not reached.  */
loop:	hlt
	jmp	loop

	/* Our stack area.  */
	.comm	stack, STACK_SIZE
