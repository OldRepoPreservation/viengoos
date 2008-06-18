/* ia32-exception-entry.S - Exception handler dispatcher.
   Copyright (C) 2007, 2008 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 3 of the
   License, or (at your option) any later version.

   The GNU Hurd is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see
   <http://www.gnu.org/licenses/>.  */

#include <hurd/exceptions.h>

	.text

/* Offsets into a struct exception_page.  */
#define MODE (0*4)
#define SAVED_IP (1*4)
#define SAVED_SP (2*4)
#define SAVED_THREAD_STATE (3*4)

#define EXCEPTION_STACK (4*4)

/* Relative to one word beyond the bottom of the stack.  */
#define EXCEPTION_PAGE_PTR (-1*4)
#define SAVED_EAX (-2*4)
#define SAVED_ECX (-3*4)
#define SAVED_FLAGS (-4*4)
#define SAVED_EDX (-5*4)

/* Offsets into a struct exception_fault.  */
#define EF_SAVED_EAX 0
#define EF_SAVED_ECX 4
#define EF_SAVED_EDX 8
#define EF_SAVED_FLAGS 12
#define EF_SAVED_IP 16
#define EF_SAVED_EBX 20
#define EF_SAVED_EDI 24
#define EF_SAVED_ESI 28
#define EF_NEXT 32

#define ACTIVATED_MODE_BIT 0
#define PENDING_MESSAGE_BIT 1
#define INTERRUPT_IN_TRANSITION_BIT 2

	/* Handle an exception.  */
	.globl	exception_handler_entry, _exception_handler_entry
exception_handler_entry:
_exception_handler_entry:

	/* How we will use the stack:

	    relative to entry sp
	     |  relative to sp after saving edx
	     |  |
	     v  v
	    +0 +24 pointer to exception_page
	    -4 +20 saved eax                     \
	    -8 +16 saved ecx                      \  save
	   -12 +12 saved flags                    /  area
	   -16  +8 saved edx                     /
	   -20  +4 entry edx
	   -24  +0 entry eflags
	 */

	/* Adjust the stack: our saved EAX, ECX, EDX and FLAGS may be
	there.  */
	
	sub $16, %esp
	
	/* %ESP points to the top of the exception page.  If the
	interrupt in transition flag is not set, then we need to save
	the caller-saved registers.  Otherwise, we were interrupted
	while returning to normal mode and the the saved state, not
	our registers, reflects the real user state (see big comment
	below for more information).  */

	pushl %edx
	/* Save the eflags before we do anything serious.  */
	pushf
	
	/* %EDX is now the only register which we can touch.  Make it
	a pointer to the exception page.  Recall: we stashed a pointer
	to the exception page at the word following the botton of the
	stack.  */
	mov 24(%esp), %edx

	/* Now check if the interrupt in transition flag is set.  */
	bt $INTERRUPT_IN_TRANSITION_BIT, MODE(%edx)
	jc after_save

	/* Nope; we need to save the current EAX, ECX and eflags.  */
	mov %eax, 20(%esp)
	mov %ecx, 16(%esp)
	/* entry eflags.  */
	popl %ecx
	mov %ecx, (12-4)(%esp)
	/* entry edx.  */
	popl %ecx
	mov %ecx, (8-4-4)(%esp)
	jmp after_adjust
	
after_save:
	
	/* Adjust the stack: we don't need our entry flags or entry edx.  */
	add $8, %esp
	
after_adjust:

	/* We are going to call the exception handler.  But first save
	our pointer to the exception page on the stack.  */
	pushl %edx

	/* The exception handler function takes a single argument:
	the exception page.  */

	/* Push the exception page.  */
	pushl %edx
	/* Clear the direction flag as per the calling conventions.  */
	cld
	call exception_handler_activated
	/* The exception frame, if any, is in EAX.  */
	/* Clear the arguments.  */
	add $4, %esp

	/* Restore exception page pointer.  */
	popl %edx

	/* Check if there is an exception frame.  */
	test %eax, %eax
	jnz exception_frame_run

	/* There is no exception frame, transition immediately back to
	normal mode.
	
	To return to normal mode, we need to restore the saved
	registers, including the saved general registers, saved ESP
	and saved EIP.  On x86, there is no way to atomically restore
	ESP and EIP from user code.  The solution we use is:

	  - save the saved EIP on the user stack
	  - restore the saved ESP minus 4
	  - execute a ret instruction

	Beyond not being atomic, this has the additional problem that
	writing on the user stack may cause a fault.

	To work around this latter problem, we only write on the user
	stack once we return to normal mode.  If this faults, the
	kernel can transition us back to activated mode.

	But this raises another problem: the IP and SP that the kernel
	sees are not those that return us to user code.  As this code
	relies on the exception stack, a nested stack will leave us in
	an inconsistent state.  (This can also happen if we receive a
	message before returning to user code.)  To avoid this, we
	register our restore to normal mode function with the kernel.
	If the kernel transitions us back to activated mode while the
	EIP is in this range, then it does not save the EIP and ESP
	and invokes the exception handler with the
	interrupt_in_transition flag set.  */

	/* Reset the activation bit.  */
	and $(~1), MODE(%edx)

	/* Set EAX to one word beyond the bottom of the stack (i.e.,
	pointing at the pointer to the exception page.  */
	add $PAGESIZE, %esp
	and $(~(PAGESIZE-1)), %esp
	mov %esp, %eax

	/* Check for pending messages.  This does not need to be
	   atomic as if we get interrupted here, we automatically
	   transition back to activated mode.  */
	bt $PENDING_MESSAGE_BIT, MODE(%edx)
	jc process_pending

	/* Restore the user stack.  */
	mov SAVED_SP(%edx), %esp

	/* Copy the saved EIP and saved flags to the user stack.  */
	mov SAVED_IP(%edx), %ecx
	pushl %ecx
	mov SAVED_FLAGS(%eax), %ecx
	pushl %ecx

	/* Restore the general-purpose registers.  */
	mov SAVED_EDX(%eax), %edx
	mov SAVED_ECX(%eax), %ecx
	mov SAVED_EAX(%eax), %eax
	
	/* Restore the saved eflags.  */
	popf
	/* And finally, the saved EIP and in doing so the saved ESP.  */
	ret

process_pending:	
	/* This code is called if after leaving activated mode, we
 	detect a pending message.  %EDX points to the exception page
	and eax one word beyond the bottom of the exception stack.  */

	/* Set activated mode and interrupt in transition.  */
	or $(1 | 4), MODE(%edx)

	/* Set the ESP to the top of the stack.  */
	mov %eax, %esp
	add $4, %esp

	/* Get the pending exception.  */
	call exception_fetch_exception

	jmp exception_handler_entry


exception_frame_run:
	/* EAX contains the exception frame, EDX the exception page,
	and ESP points after the saved edx.  */

	/* Copy all relevant register state from the exception page
	and save area to the exception frame.  We use edx as the
	intermediate.  We can restore it from the exception stack
	(it's the word following the base).  */
	
	mov SAVED_IP(%edx), %edx
	mov %edx, EF_SAVED_IP(%eax)

	/* edx.  */
	mov 0(%esp), %edx
	mov %edx, EF_SAVED_EDX(%eax)
	/* flags.  */
	mov 4(%esp), %edx
	mov %edx, EF_SAVED_FLAGS(%eax)
	/* ecx.  */
	mov 8(%esp), %edx
	mov %edx, EF_SAVED_ECX(%eax)
	/* eax.  */
	mov 12(%esp), %edx
	mov %edx, EF_SAVED_EAX(%eax)

	mov %ebx, EF_SAVED_EBX(%eax)
	mov %edi, EF_SAVED_EDI(%eax)
	mov %esi, EF_SAVED_ESI(%eax)

	/* Restore the exception page pointer (edx).  */
	mov 16(%esp), %edx
	
	/* Restore the user ESP.  */
	mov SAVED_SP(%edx), %esp

	/* We've now stashed away all the state we need to restore to
	the interrupted state.  */

	/* Reset the activation bit.  */
	and $(~1), MODE(%edx)

	/* XXX: Check for pending message.  */

	.global exception_handler_end, _exception_handler_end
exception_handler_end:
_exception_handler_end:

	/* We have now left activated mode.  We've saved all the
	state we need to return to the interrupted state in the
	exception frame and ESP points to the normal stack.  If a
	fault now occurs, nothing bad can happend.  */

	/* Save the exception page pointer.  */
	pushl %edx
	/* Save the exception frame pointer.  */
	pushl %eax

	/* Call the continuation (single argument: exception frame
	pointer).  */
	
	pushl %eax
	cld
	call exception_handler_normal
	
	/* Remove the argument.  */
	add $4, %esp

	/* Restore the exception frame pointer.  */
	popl %eax
	/* And restore the exception page pointer.  */
	popl %edx

	/* Restore the user state.  */
	mov EF_SAVED_IP(%eax), %ecx
	pushl %ecx
	mov EF_SAVED_FLAGS(%eax), %ecx
	pushl %ecx
	mov EF_SAVED_EDX(%eax), %ecx
	pushl %ecx
	mov EF_SAVED_ECX(%eax), %ecx
	pushl %ecx
	mov EF_SAVED_EAX(%eax), %ecx
	pushl %ecx

	/* Remove our exception frame, which is at the top
	of the exception frame stack.  */
	mov EF_NEXT(%eax), %ecx
	mov %ecx, EXCEPTION_STACK(%edx)

	popl %eax
	popl %ecx
	popl %edx
	popf
	/* And return.  */
	ret
