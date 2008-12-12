/* ia32-activation-entry.S - Activation handler dispatcher.
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

/* Offsets into a struct vg_utcb.  */
#define UTCB_MODE (0*4)
#define UTCB_SAVED_IP (1*4)
#define UTCB_SAVED_SP (2*4)
#define UTCB_SAVED_THREAD_STATE (3*4)
#define UTCB_ACTIVATION_FRAME_STACK (4*4)

/* The bits of the mode word.  */
#define ACTIVATED_MODE_BIT 0
#define PENDING_MESSAGE_BIT 1
#define INTERRUPT_IN_TRANSITION_BIT 2

/* Offsets into a struct activation_frame.  */
#define AF_SAVED_EAX 0
#define AF_SAVED_ECX 4
#define AF_SAVED_EDX 8
#define AF_SAVED_FLAGS 12
#define AF_SAVED_IP 16
#define AF_SAVED_EBX 20
#define AF_SAVED_EDI 24
#define AF_SAVED_ESI 28
#define AF_SAVED_EBP 32
#define AF_SAVED_ESP 36
#define AF_NORMAL_MODE_STACK 40
#define AF_NEXT 44

	/* Handle an activation.  */
	.globl	hurd_activation_handler_entry, _hurd_activation_handler_entry
hurd_activation_handler_entry:
_hurd_activation_handler_entry:

	/* How we will use the stack:

	    relative to entry sp
	     |  relative to sp after saving the register file.
	     |  |
	     v  v
	    +0 +24 pointer to utcb
	    -4 +20 entry eflags
	    -8 +16 entry edx
	   -12 +12 saved eax                     \
	   -16  +8 saved ecx                      \  save
	   -20  +4 saved flags                    /  area
	   -24  +0 saved edx                     /
	 */

	/* %ESP points to the top of the UTCB.  If the interrupt in
	transition flag is not set, then we need to save the
	caller-saved registers.  Otherwise, we were interrupted while
	returning to normal mode and the the saved state, not our
	registers, reflects the real user state (see big comment below
	for more information).  */

	/* Save the eflags before we do *anything*.  */
	pushf
	
	/* We need to check if the interrupt in transition flag is
	set.  Free up a register and make it a pointer to the UTCB.
	Recall: we stashed a pointer to the UTCB at the word following
	the botton of the stack.  */
	pushl %edx
	mov 8(%esp), %edx
	
	bt $INTERRUPT_IN_TRANSITION_BIT, UTCB_MODE(%edx)
	jc skip_save

	/* Nope; we need to save the current EAX and ECX and copy the
	entry eflags and EDX into the save area.  */

	pushl %eax
	pushl %ecx
	/* entry eflags.  */
	mov 12(%esp), %eax
	pushl %eax
	/* entry edx.  */
	mov 12(%esp), %eax
	pushl %eax

	jmp after_adjust
	
skip_save:
	/* We don't save the entry registers but use the saved values.
	Adjust the stack pointer to point to the start of the save area.  */
	sub $16, %esp

after_adjust:	
	
	/* We are going to call the activation handler.  According to
	the i386 ABI:

	  - caller saved registers are: eax, ecx, edx
	  - callee saved registers are: ebp, ebx, esi, edi

	We've already saved the original eax, ecx and edx.  The
	function will preserve the rest.

	The only value we care about is our pointer to the UTCB (which
	is in edx) and which we can save on the stack.  */
	pushl %edx

	/* The activation handler function takes a single argument:
	the UTCB.  */
	pushl %edx
	/* Clear the direction flag as per the calling conventions.  */
	cld
	call hurd_activation_handler_activated
	/* The activation frame, if any, is in EAX.  */
	/* Clear the arguments.  */
	add $4, %esp

	/* Restore UTCB pointer.  */
	popl %edx

	/* Check if hurd_activation_handler_activated returned an
	activation frame.  */
	test %eax, %eax
	jnz activation_frame_run

	/* There is no activation frame, transition immediately back to
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
	relies on the activation stack, a nested stack will leave us in
	an inconsistent state.  (This can also happen if we receive a
	message before returning to user code.)  To avoid this, we
	register our restore to normal mode function with the kernel.
	If the kernel transitions us back to activated mode while the
	EIP is in this range, then it does not save the EIP and ESP
	and invokes the activation handler with the
	interrupt_in_transition flag set.  */

	/* Reset the activation bit.  */
	and $(~1), UTCB_MODE(%edx)

	/* Check for pending messages.  This does not need to be
	   atomic as if we get interrupted here, we automatically
	   transition back to activated mode.  */
	bt $PENDING_MESSAGE_BIT, UTCB_MODE(%edx)
	jnc no_pending

	/* There is a pending activation.  Force its delivery.  As we
	are no longer in activated mode, either we'll be activated
	with the interrupt-in-transition bit set (and thus never
	return here) or we'll return.  In the latter case, we just
	resume execution.  */
	
	/* Save the UTCB.  */
	pushl %edx
	
	cld
	call hurd_activation_fetch

	popl %edx

no_pending:	
	
	/* Set EAX to the start of the save area.  */
	mov %esp, %eax

	/* Restore the user stack.  */
	mov UTCB_SAVED_SP(%edx), %esp

	/* Copy the saved EIP and saved flags to the user stack.  */
	mov UTCB_SAVED_IP(%edx), %ecx
	pushl %ecx
	mov 4(%eax), %ecx
	pushl %ecx

	/* Restore the general-purpose registers.  */
	mov 0(%eax), %edx
	mov 8(%eax), %ecx
	mov 12(%eax), %eax
	
	/* Restore the saved eflags.  */
	popf
	/* And finally, the saved EIP and in doing so the saved ESP.  */
	ret

	
activation_frame_run:
	/* EAX contains the activation frame, EDX the UTCB, and ESP
	points to the save area.  ECX has been saved in the save area.  */

	/* Copy all relevant register state from the UTCB
	and save area to the activation frame.  We use ecx as the
	intermediate.  */
	
	mov UTCB_SAVED_IP(%edx), %ecx
	mov %ecx, AF_SAVED_IP(%eax)
	mov UTCB_SAVED_SP(%edx), %ecx
	mov %ecx, AF_SAVED_ESP(%eax)

	/* edx.  */
	mov 0(%esp), %ecx
	mov %ecx, AF_SAVED_EDX(%eax)
	/* flags.  */
	mov 4(%esp), %ecx
	mov %ecx, AF_SAVED_FLAGS(%eax)
	/* ecx.  */
	mov 8(%esp), %ecx
	mov %ecx, AF_SAVED_ECX(%eax)
	/* eax.  */
	mov 12(%esp), %ecx
	mov %ecx, AF_SAVED_EAX(%eax)

	/* We save the rest for debugging purposes.  */
	mov %ebx, AF_SAVED_EBX(%eax)
	mov %edi, AF_SAVED_EDI(%eax)
	mov %esi, AF_SAVED_ESI(%eax)
	mov %ebp, AF_SAVED_EBP(%eax)

	/* Abandon the activation stack.  If AF->NORMAL_MODE_STACK is
	0, we use the top of the normal user stack.  Otherwise, we use
	the stack indicated by AF->NORMAL_MODE_STACK.  */
	
	mov AF_NORMAL_MODE_STACK(%eax), %esp
	test %esp, %esp
	jnz stack_setup
	mov UTCB_SAVED_SP(%edx), %esp
stack_setup:

	/* We've now stashed away all the state that was in the UTCB
	or on the activation stack that we need to restore the
	interrupted state.  */

	/* Clear the activation bit.  */
	and $(~1), UTCB_MODE(%edx)

	.global hurd_activation_handler_end, _hurd_activation_handler_end
hurd_activation_handler_end:
_hurd_activation_handler_end:

	/* We have now left activated mode.  We've saved all the state
	we need to return to the interrupted state in the activation
	frame and ESP points to another stack (i.e., not the activate
	stack).  If a fault now occurs, nothing bad can happend.  */

	/* Save the UTCB pointer.  */
	pushl %edx
	/* Save the activation frame pointer.  */
	pushl %eax

	/* Call the continuation (two arguments: activation frame
	pointer and the utcb).  */
	
	pushl %edx
	pushl %eax
	cld
	call hurd_activation_handler_normal
	
	/* Remove the arguments.  */
	add $8, %esp

	/* Restore the activation frame pointer.  */
	popl %eax
	/* And restore the UTCB pointer.  */
	popl %edx

	/* Restore the interrupted state.  */
	
	/* First, the interrupted stack.  */
	mov AF_SAVED_ESP(%eax), %esp

	/* Then, push the state onto that stack.  */
	mov AF_SAVED_IP(%eax), %ecx
	pushl %ecx
	mov AF_SAVED_FLAGS(%eax), %ecx
	pushl %ecx
	mov AF_SAVED_EDX(%eax), %ecx
	pushl %ecx
	mov AF_SAVED_ECX(%eax), %ecx
	pushl %ecx
	mov AF_SAVED_EAX(%eax), %ecx
	pushl %ecx

	/* Remove our activation frame, which is at the top
	of the activation frame stack.  */
	mov AF_NEXT(%eax), %ecx
	mov %ecx, UTCB_ACTIVATION_FRAME_STACK(%edx)

	/* Finally, restore the register file.  */
	popl %eax
	popl %ecx
	popl %edx
	popf
	/* And return.  */
	ret
