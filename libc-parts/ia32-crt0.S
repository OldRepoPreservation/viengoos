/* ia32-crt0.S - Startup code for ia32.
   Copyright (C) 2003, 2008 Free Software Foundation, Inc.
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

/* The size of our initial stack (32 pages).  */
#define STACK_SIZE			0x32000
	
	.text

	.globl	start, _start
start:
_start:
	/* The location of the Hurd startup data is stored in esp.  */
	movl	%esp, __hurd_startup_data

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

	.data

	/* Our stack area.  */
	.global stack, _stack
stack:
_stack:	
	.align 0x1000
	.fill STACK_SIZE, 1
	.global stack_end, _stack_end
stack_end:	
_stack_end:

	/* This variable holds a pointer to the Hurd startup data.  */
	.global __hurd_startup_data
__hurd_startup_data:
	.long	0
