/* syscall.h - Public interface to the L4 system calls for ia32.
   Copyright (C) 2003 Free Software Foundation, Inc.
   Written by Marcus Brinkmann <marcus@gnu.org>.

   This file is part of the GNU L4 library.
 
   The GNU L4 library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License
   as published by the Free Software Foundation; either version 2.1 of
   the License, or (at your option) any later version.
 
   The GNU L4 library is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.
 
   You should have received a copy of the GNU Lesser General Public
   License along with the GNU L4 library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#ifndef _L4_SYSCALL_H
# error "Never use <l4/bits/syscall.h> directly; include <l4/syscall.h> instead."
#endif


/* Return the pointer to the kernel interface page, the API version,
   the API flags, and the kernel ID.  */

static inline l4_kip_t
__attribute__((__always_inline__, __const__))
l4_kernel_interface (l4_api_version_t *api_version, l4_api_flags_t *api_flags,
		     l4_kernel_id_t *kernel_id)
{
  void *kip;

  /* The KernelInterface system call is invoked by "lock; nop" and
     returns a pointer to the kernel interface page in %eax, the API
     version in %ecx, the API flags in %edx, and the kernel ID in
     %esi.  */
  __asm__ ("  lock; nop\n"
	   : "=a" (kip), "=c" (api_version->raw),
	   "=d" (api_flags->raw), "=S" (kernel_id->raw));

  return kip;
}


static inline void
__attribute__((__always_inline__))
l4_exchange_registers (l4_thread_id_t *dest, l4_word_t *control,
		       l4_word_t *sp, l4_word_t *ip, l4_word_t *flags,
		       l4_word_t *user_defined_handle, l4_thread_id_t *pager)
{
  /* We can not invoke the system call directly using GCC inline
     assembler, as the system call requires the PAGER argument to be
     in %ebp, and that is used by GCC to locate the function arguments
     on the stack, and there is no other free register we can use to
     hold the PAGER argument temporarily.  So we must pass a pointer
     to an array of two function arguments in one register, one of
     them being the PAGER, the other being the one to be stored in the
     register used to hold the array pointer.  */
  struct
  {
    l4_word_t ebp;
    l4_word_t esi;
  } regs = { pager->raw, *ip };
 
  __asm__ __volatile__ ("pushl %%ebp\n"
			"pushl %%esi\n"
			/* After saving %ebp and &regs on the stack,
			   set up the remaining system call
			   arguments.  */
			"movl (%%esi), %%ebp\n"
			"movl 4(%%esi), %%esi\n"
			"call *__l4_exchange_registers\n"

			/* Temporarily exchange %esi with &regs on the
			   stack and store %ebp.  */
			"xchgl %%esi, (%%esp)\n"
			"movl %%ebp, (%%esi)\n"
			/* Pop %esi that was returned from the syscall.  */
			"popl %%esi\n"
			"popl %%ebp\n"
			: "=a" (dest->raw), "=c" (*control), "=d" (*sp),
			"=S" (*ip), "=D" (*flags), "=b" (*user_defined_handle)
			: "a" (dest->raw), "c" (*control), "d" (*sp),
			"S" (&regs), "D" (*flags), "b" (*user_defined_handle)
			: "memory");

  pager->raw = regs.ebp;
  return;
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_thread_control (l4_thread_id_t dest, l4_thread_id_t space,
		   l4_thread_id_t scheduler, l4_thread_id_t pager,
		   void *utcb_loc)
{
  l4_word_t result;
  l4_word_t dummy;

  __asm__ __volatile__ ("push %%ebp\n"
			"call *__l4_thread_control\n"
			"pop %%ebp\n"
			: "=a" (result), "=c" (dummy),
			"=d" (dummy), "=S" (dummy), "=D" (dummy)
			: "a" (dest.raw), "c" (pager.raw),
			"d" (scheduler.raw), "S" (space.raw),
			"D" (utcb_loc)
			: "ebx");
  return result;
}


static inline l4_clock_t
__attribute__((__always_inline__))
l4_system_clock (void)
{
  l4_clock_t time;

  __asm__ __volatile__ ("call *__l4_system_clock"
			: "=A" (time)
			:
			: "ecx", "esi");

  return time;
}


static inline void
__attribute__((__always_inline__))
l4_thread_switch (l4_thread_id_t dest)
{
  __asm__ __volatile__ ("call *__l4_thread_switch"
			:
			: "a" (dest.raw));
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_schedule (l4_thread_id_t dest, l4_word_t time_control,
	     l4_word_t proc_control, l4_word_t prio,
	     l4_word_t preempt_control, l4_word_t *old_time_control)
{
  l4_word_t result;
  l4_word_t dummy;

  __asm__ __volatile__ ("push %%ebp\n"
			"call *__l4_schedule\n"
			"pop %%ebp\n"
			: "=a" (result), "=d" (*old_time_control),
			"=c" (dummy), "=S" (dummy), "=D" (dummy)
			: "a" (dest), "c" (prio), "d" (time_control),
			"S" (proc_control), "D" (preempt_control)
			: "ebx");

  return result;
}


static inline void
__attribute__((__always_inline__))
l4_unmap (l4_word_t control)
{
  l4_word_t mr0;
  l4_word_t utcb;
  l4_word_t dummy;

  l4_store_mr (0, &mr0);
  utcb = (l4_word_t) __l4_utcb ();

  __asm__ __volatile__ ("push %%ebp\n"
			"call *__l4_unmap\n"
			"pop %%ebp\n"
			: "=a" (mr0), "=c" (dummy), "=d" (dummy)
			: "a" (mr0), "c" (utcb), "d" (control)
			: "esi", "edi", "ebx");
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_space_control (l4_thread_id_t space, l4_word_t control,
		  l4_fpage_t kip_area, l4_fpage_t utcb_area,
		  l4_thread_id_t redirector, l4_word_t *old_control)
{
  l4_word_t result;
  l4_word_t dummy;

  __asm__ __volatile__ ("push %%ebp\n"
			"call *__l4_space_control\n"
			"pop %%ebp\n"
			: "=a" (result), "=c" (*old_control),
			"=d" (dummy), "=S" (dummy), "=D" (dummy)
			: "a" (space.raw), "c" (control),
			"d" (kip_area.raw), "S" (utcb_area.raw),
			"D" (redirector.raw)
			: "ebx");
  return result;
}


static inline l4_msg_tag_t
__attribute__((__always_inline__))
l4_ipc (l4_thread_id_t to, l4_thread_id_t from_spec,
	l4_word_t timeouts, l4_thread_id_t *from)
{
  l4_word_t mr[2];
  l4_word_t utcb;
  l4_msg_tag_t tag;
  l4_thread_id_t result;
  l4_word_t dummy;

  utcb = (l4_word_t) __l4_utcb ();
  l4_store_mr (0, &tag.raw);
  l4_store_mr (1, &mr[0]);
  l4_store_mr (2, &mr[1]);

  __asm__ __volatile__ ("push %%ebp\n"
			"call *__l4_ipc\n"
			"movl %%ebp, %%ecx\n"
			"pop %%ebp\n"
			: "=a" (result.raw), "=c" (mr[1]), "=d" (dummy),
			"=S" (tag.raw), "=b" (mr[0])
			: "a" (to.raw), "c" (timeouts), "d" (from_spec.raw),
			"S" (tag.raw), "D" (utcb));
  /* FIXME: Make it so that we can use l4_is_nilthread?  */
  if (from_spec.raw)
    {
      *from = result;
      l4_load_mr (1, mr[0]);
      l4_load_mr (2, mr[1]);
    }
  return tag;
}


static inline l4_msg_tag_t
__attribute__((__always_inline__))
l4_lipc (l4_thread_id_t to, l4_thread_id_t from_spec,
	 l4_word_t timeouts, l4_thread_id_t *from)
{
  l4_word_t mr[2];
  l4_word_t utcb;
  l4_msg_tag_t tag;
  l4_thread_id_t result;
  l4_word_t dummy;

  utcb = (l4_word_t) __l4_utcb ();
  l4_store_mr (0, &tag.raw);
  l4_store_mr (1, &mr[0]);
  l4_store_mr (2, &mr[1]);

  __asm__ __volatile__ ("push %%ebp\n"
			"call *__l4_lipc\n"
			"movl %%ebp, %%ecx\n"
			"pop %%ebp\n"
			: "=a" (result.raw), "=c" (mr[1]), "=d" (dummy),
			"=S" (tag.raw), "=b" (mr[0])
			: "a" (to.raw), "c" (timeouts), "d" (from_spec.raw),
			"S" (tag.raw), "D" (utcb));
  /* FIXME: Make it so that we can use l4_is_nilthread?  */
  if (from_spec.raw)
    {
      *from = result;
      l4_load_mr (1, mr[0]);
      l4_load_mr (2, mr[1]);
    }
  return tag;
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_processor_control (l4_word_t proc, l4_word_t control, l4_word_t int_freq,
		      l4_word_t ext_freq, l4_word_t voltage)
{
  l4_word_t result;
  l4_word_t dummy;

  __asm__ __volatile__ ("push %%ebp\n"
			"call *__l4_processor_control\n"
			"pop %%ebp\n"
			: "=a" (result), "=c" (dummy),
			"=d" (dummy), "=S" (dummy), "=D" (dummy)
			: "a" (proc), "c" (control), "d" (int_freq),
			"S" (ext_freq), "D" (voltage)
			: "ebx");
  return result;
}


static inline void
__attribute__((__always_inline__))
l4_memory_control (l4_word_t control, l4_word_t *attributes)
{
  l4_word_t tag;
  l4_word_t dummy;

  l4_store_mr (0, &tag);

  __asm__ __volatile__ ("push %%ebp\n"
			"call *__l4_memory_control\n"
			"pop %%ebp\n"
			: "=a" (dummy), "=c" (dummy), "=d" (dummy),
			"=S" (dummy), "=D" (dummy), "=b" (dummy)
			: "a" (tag), "c" (control), "d" (attributes[0]),
			"S" (attributes[1]), "D" (attributes[2]),
			"b" (attributes[3]));
}
