/* ia32-exception-entry.S - Exception handler dispatcher.
   Copyright (C) 2007 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

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

	.text

#define PAGESIZE 0x1000

#define MODE (0*4)
#define SAVED_IP (1*4)
#define SAVED_SP (2*4)
#define SAVED_THREAD_STATE (3*4)
#define ORIG_FLAGS (PAGESIZE-1*4)
#define SAVED_EAX (PAGESIZE-2*4)
#define SAVED_ECX (PAGESIZE-3*4)
#define SAVED_FLAGS (PAGESIZE-4*4)

	/* Handle an exception.  */
	.globl	exception_handler_entry, _exception_handler_entry
exception_handler_entry:
_exception_handler_entry:
	/* Save the eflags before we do anything.  */
	pushf
	/* Adjust the stack: our saved state may be there. */
	sub $12, %esp
	
	/* %ESP points to the top of the exception page.  If the
	interrupt in transition flag is not set, then we need to save
	the caller saved registers.  Otherwise, we were interrupt
	while returning to normal mode and the the saved state, not
	our registers, reflects the real user state (see big comment
	below for more information).  */

	/* Whatever the case, EDX reflects the user's state.  Save it. */
	pushl %edx

	/* %EDX is the only register which we can touch.  Make it a
	pointer to the exception page.  */
	mov %esp, %edx
	and $0xfffff000, %edx

	/* Now check if the interrupt in transition flag is set.  */
	bt $0x2, MODE(%edx)
	jc after_save

	/* Nope; we need to save the current EAX and ECX and eflags.  */
	mov %eax, SAVED_EAX(%edx)
	mov %ecx, SAVED_ECX(%edx)
	mov ORIG_FLAGS(%edx), %ecx
	mov %ecx, SAVED_FLAGS(%edx)
after_save:	

	/* The exception function takes a single argument: the
	exception page.  */
	pushl %edx
	/* Call the exception handler.  */
	call exception_handler
	/* Clear the arguments.  */
	add $0x4, %esp

	/* Transition back to normal mode.
	
	To return to normal mode, we need to restore the saved
	registers, including the saved EAX, saved ESP and saved EIP.
	On x86, there is no way to atomically restore ESP and EIP from
	user code.  The solution we use is:

	  - save the saved EIP on the user stack
	  - restore the saved ESP minus 4
	  - execute a ret instruction

	Beyond not being atomic, this has the additional problem that
	writing on the user stack may cause a fault.

	To work around this latter problem, we only write on the user
	stack once we return to normal mode.  If this faults, the
	kernel can transition us back to activated mode.

	But this raises another problem: the IP and SP that the kernel
	see are not those that return us to user code.  As this code
	relies on the exception stack, a nested stack will leave us in
	an inconsistent state.  (This can also happen if we receive a
	message before returning to user code.)  To avoid this, we
	register our restore to normal mode function with the kernel.
	If the kernel transitions us back to activated while the EIP
	is in this range, then it does not save the EIP and ESP and
	invokes the exception handler with the interrupt_in_transition
	flag set.  */

	/* Restore the caller saved registers.  */
	popl %edx

	/* Point EAX at the exception page.  */
	mov %esp, %eax
	and $0xfffff000, %eax

	/* Reset the activation bit.  */
	and $0xfffffffe, MODE(%eax)

	/* Check for pending messages.  This does not need to be
	   atomic as if we get interrupted here, we automatically
	   transition back to activated mode.  */
	bt $0x1, MODE(%eax)
	jc process_pending

	/* Restore the user stack.  */
	mov SAVED_SP(%eax), %esp
	/* Copy the saved EIP, EAX and eflags to the stack.  */
	mov SAVED_IP(%eax), %ecx
	pushl %ecx
	mov SAVED_EAX(%eax), %ecx
	pushl %ecx
	mov SAVED_FLAGS(%eax), %ecx
	pushl %ecx

	/* Restore ECX.  */
	mov SAVED_ECX(%eax), %ecx
	
	/* Restore the saved eflags and EAX.  */
	popf
	popl %eax
	/* Restore the saved EIP and in doing so the saved ESP.  */
	ret
process_pending:	
	/* This code is called if after leaving activated mode, we
 	detect a pending message.  %EAX points to the exception page.  */

	/* Set activated mode and interrupt in transition.  */
	or $0x5, MODE(%eax)

	/* Set the ESP to the top of the stack.  */
	lea PAGESIZE(%eax), %esp

	/* Get the pending exception.  */
	call exception_fetch_exception

	jmp exception_handler_entry

	.global exception_handler_end, _exception_handler_end
exception_handler_end:
_exception_handler_end:
	nop
