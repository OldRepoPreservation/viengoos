/* syscall.h - Public interface to the L4 system calls for ia32.
   Copyright (C) 2003, 2005 Free Software Foundation, Inc.
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
static inline _L4_kip_t
_L4_attribute_always_inline _L4_attribute_const
_L4_kernel_interface (_L4_api_version_t *api_version,
		      _L4_api_flags_t *api_flags, _L4_kernel_id_t *kernel_id)
{
#ifndef _L4_TEST_ENVIRONMENT
  void *kip;

  /* The KernelInterface system call is invoked by "lock; nop" and
     returns a pointer to the kernel interface page in %eax, the API
     version in %ecx, the API flags in %edx, and the kernel ID in
     %esi.  */
  __asm__ ("  lock; nop\n"
	   : "=a" (kip), "=c" (*api_version),
	   "=d" (*api_flags), "=S" (*kernel_id));

  return kip;
#else
  _L4_TEST_KERNEL_INTERFACE_IMPL
#endif	/* _L4_TEST_ENVIRONMENT */
}


static inline void
_L4_attribute_always_inline
_L4_exchange_registers (_L4_thread_id_t *dest, _L4_word_t *control,
			_L4_word_t *sp, _L4_word_t *ip, _L4_word_t *flags,
			_L4_word_t *user_defined_handle, _L4_thread_id_t *pager)
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
    _L4_word_t ebp;
    _L4_word_t esi;
  } regs = { *pager, *ip };
 
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
			: "=a" (*dest), "=c" (*control), "=d" (*sp),
			"=S" (*ip), "=D" (*flags), "=b" (*user_defined_handle)
			: "a" (*dest), "c" (*control), "d" (*sp),
			"S" (&regs), "D" (*flags), "b" (*user_defined_handle)
			: "memory");

  *pager = regs.ebp;
  return;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_thread_control (_L4_thread_id_t dest, _L4_thread_id_t space,
		    _L4_thread_id_t scheduler, _L4_thread_id_t pager,
		    void *utcb_loc)
{
  _L4_word_t result;
  _L4_word_t dummy;

  __asm__ __volatile__ ("push %%ebp\n"
			"call *__l4_thread_control\n"
			"pop %%ebp\n"
			: "=a" (result), "=c" (dummy),
			"=d" (dummy), "=S" (dummy), "=D" (dummy)
			: "a" (dest), "c" (pager),
			"d" (scheduler), "S" (space),
			"D" (utcb_loc)
			: "ebx");
  return result;
}


static inline _L4_clock_t
_L4_attribute_always_inline
_L4_system_clock (void)
{
  _L4_clock_t time;

  __asm__ __volatile__ ("call *__l4_system_clock"
			: "=A" (time)
			:
			: "ecx", "esi", "edi");

  return time;
}


static inline void
_L4_attribute_always_inline
_L4_thread_switch (_L4_thread_id_t dest)
{
  __asm__ __volatile__ ("call *__l4_thread_switch"
			:
			: "a" (dest));
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_schedule (_L4_thread_id_t dest, _L4_word_t time_control,
	      _L4_word_t proc_control, _L4_word_t prio,
	      _L4_word_t preempt_control, _L4_word_t *old_time_control)
{
  _L4_word_t result;
  _L4_word_t dummy;

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
_L4_attribute_always_inline
_L4_unmap (_L4_word_t control)
{
  _L4_word_t mr0;
  _L4_word_t utcb;
  _L4_word_t dummy;

  _L4_store_mr (0, &mr0);
  utcb = (_L4_word_t) _L4_utcb ();

  __asm__ __volatile__ ("push %%ebp\n"
			"call *__l4_unmap\n"
			"pop %%ebp\n"
			: "=a" (dummy), "=S" (mr0), "=D" (dummy)
			: "a" (control), "S" (mr0), "D" (utcb)
			: "ecx", "edx", "ebx");
  _L4_load_mr (0, mr0);
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_space_control (_L4_thread_id_t space, _L4_word_t control,
		   _L4_fpage_t kip_area, _L4_fpage_t utcb_area,
		   _L4_thread_id_t redirector, _L4_word_t *old_control)
{
  _L4_word_t result;
  _L4_word_t dummy;

  __asm__ __volatile__ ("push %%ebp\n"
			"call *__l4_space_control\n"
			"pop %%ebp\n"
			: "=a" (result), "=c" (*old_control),
			"=d" (dummy), "=S" (dummy), "=D" (dummy)
			: "a" (space), "c" (control),
			"d" (kip_area), "S" (utcb_area),
			"D" (redirector)
			: "ebx");
  return result;
}


static inline _L4_msg_tag_t
_L4_attribute_always_inline
_L4_ipc (_L4_thread_id_t to, _L4_thread_id_t from_spec,
	 _L4_word_t timeouts, _L4_thread_id_t *from)
{
  _L4_word_t mr[2];
  _L4_word_t utcb;
  _L4_msg_tag_t tag;
  _L4_thread_id_t result;
  _L4_word_t dummy;

  utcb = (_L4_word_t) _L4_utcb ();
  _L4_store_mr (0, &tag);
  _L4_store_mr (1, &mr[0]);
  _L4_store_mr (2, &mr[1]);

  __asm__ __volatile__ ("push %%ebp\n"
			"call *__l4_ipc\n"
			"movl %%ebp, %%ecx\n"
			"pop %%ebp\n"
			: "=a" (result), "=c" (mr[1]), "=d" (dummy),
			"=S" (tag), "=b" (mr[0])
			: "a" (to), "c" (timeouts), "d" (from_spec),
			"S" (tag), "D" (utcb));
  /* FIXME: Make it so that we can use _L4_is_nilthread?  */
  if (from_spec)
    {
      *from = result;
      _L4_load_mr (1, mr[0]);
      _L4_load_mr (2, mr[1]);
    }
  return tag;
}


static inline _L4_msg_tag_t
_L4_attribute_always_inline
_L4_lipc (_L4_thread_id_t to, _L4_thread_id_t from_spec,
	  _L4_word_t timeouts, _L4_thread_id_t *from)
{
  _L4_word_t mr[2];
  _L4_word_t utcb;
  _L4_msg_tag_t tag;
  _L4_thread_id_t result;
  _L4_word_t dummy;

  utcb = (_L4_word_t) _L4_utcb ();
  _L4_store_mr (0, &tag);
  _L4_store_mr (1, &mr[0]);
  _L4_store_mr (2, &mr[1]);

  __asm__ __volatile__ ("push %%ebp\n"
			"call *__l4_lipc\n"
			"movl %%ebp, %%ecx\n"
			"pop %%ebp\n"
			: "=a" (result), "=c" (mr[1]), "=d" (dummy),
			"=S" (tag), "=b" (mr[0])
			: "a" (to), "c" (timeouts), "d" (from_spec),
			"S" (tag), "D" (utcb));
  /* FIXME: Make it so that we can use _L4_is_nilthread?  */
  if (from_spec)
    {
      *from = result;
      _L4_load_mr (1, mr[0]);
      _L4_load_mr (2, mr[1]);
    }
  return tag;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_processor_control (_L4_word_t proc, _L4_word_t int_freq,
		       _L4_word_t ext_freq, _L4_word_t voltage)
{
  _L4_word_t result;
  _L4_word_t dummy;

  __asm__ __volatile__ ("push %%ebp\n"
			"call *__l4_processor_control\n"
			"pop %%ebp\n"
			: "=a" (result), "=c" (dummy),
			"=d" (dummy), "=S" (dummy)
			: "a" (proc), "c" (int_freq), "d" (ext_freq),
			"S" (voltage)
			: "edi", "ebx");
  return result;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_memory_control (_L4_word_t control, _L4_word_t *attributes)
{
  _L4_word_t tag;
  _L4_word_t result;
  _L4_word_t dummy;

  _L4_store_mr (0, &tag);

  __asm__ __volatile__ ("push %%ebp\n"
			"call *__l4_memory_control\n"
			"pop %%ebp\n"
			: "=a" (result), "=c" (dummy), "=d" (dummy),
			"=S" (dummy), "=D" (dummy), "=b" (dummy)
			: "a" (tag), "c" (control), "d" (attributes[0]),
			"S" (attributes[1]), "D" (attributes[2]),
			"b" (attributes[3]));

  return result;
}


static inline void
_L4_attribute_always_inline
_L4_set_gs0 (_L4_word_t user_gs0)
{
  __asm__ __volatile__ ("push %%ebp\n"
			"call *__l4_set_gs0\n"
			"pop %%ebp\n"
			: 
			: "a" (user_gs0)
			: "ecx", "edx", "esi", "edi", "ebx");
}
