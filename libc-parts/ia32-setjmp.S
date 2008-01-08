/* ia32-setjmp.c - ia32 setjmp and longjmp implementation.
   Copyright (C) 2008 Free Software Foundation, Inc.
   Written by Neal H. Walfield.

   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU Hurd is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#define EBP 0
#define EBX 4
#define EDI 8
#define ESI 12
#define ESP 16
#define EIP 20

	.text

	/* extern int _setjmp (jmpbuf buf); */
	.globl	_setjmp
_setjmp:	
	/* The sole argument is a pointer to a struct jmp_buf.  We
	need to save the caller saved registers there.  */

	/* First parameter, the location of the jump buffer. */
	mov 4(%esp), %ecx

	movl %ebp, EBP(%ecx)
	movl %ebx, EBX(%ecx)
	movl %edi, EDI(%ecx)
	movl %esi, ESI(%ecx)
	movl %esp, %eax
	/* Remove the return address.  */
	add $4, %eax
	movl %eax, ESP(%ecx)
	movl 0(%esp), %eax
	mov %eax, EIP(%ecx)

	/* Return 0.  */
	xor %eax, %eax
	ret

	/* extern int _longjmp (jmpbuf buf, int ret); */
	.globl	_longjmp
_longjmp:	
	/* We have two arguments: the location of the jump buffer and
	the value to return.  */

	/* The function's return address: ignore.  */
	popl %ecx

	/* The location of the jump buffer.  */
	popl %ecx
	/* The return value.  */
	popl %eax

	/* Restore ESP.  */
	movl ESP(%ecx), %esp
	/* Read EIP.  */
	movl EIP(%ecx), %edi
	/* Push it on the stack.  We'll restore it at the end via the ret.  */
	pushl %edi

	movl EBP(%ecx), %ebp
	movl EBX(%ecx), %ebx
	movl EDI(%ecx), %edi
	movl ESI(%ecx), %esi

	/* Pop the return address restoring the expected ESP.  */
	ret
