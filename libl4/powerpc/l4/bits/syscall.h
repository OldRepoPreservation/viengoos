/* syscall.h - Public interface to the L4 system calls for powerpc.
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

/* Declare the system call stubs.  */
#define _L4_EXTERN_STUBS	1
#include <l4/bits/stubs.h>
#undef _L4_EXTERN_STUBS


/* These are the clobber registers.  R1, R2, R30, R31, and all
   floating point registers are preserved.  R3 to R10 are used in
   system calls and thus are not in this list.  Up to R17 is used in
   the IPC system calls.  */
#define __L4_PPC_XCLOB "r18", "r19",					\
  "r20", "r21", "r22", "r23", "r24", "r25", "r26", "r27", "r28", "r29",	\
  "cr0", "cr1", "cr2", "cr3", "cr4", "cr5", "cr6", "cr7", "xer",        \
  "ctr", "lr", "memory"
#define __L4_PPC_CLOB "r0", "r11", "r12", "r13", "r14", "r15", "r16",	\
  "r17", __L4_PPC_XCLOB


/* Return the pointer to the kernel interface page, the API version,
   the API flags, and the kernel ID.  */

static inline _L4_kip_t
_L4_attribute_always_inline _L4_attribute_const
_L4_kernel_interface (_L4_api_version_t *api_version,
		      _L4_api_flags_t *api_flags,
		      _L4_kernel_id_t *kernel_id)
{
  register void *kip asm ("r3");
  register _L4_word_t version asm ("r4");
  register _L4_word_t flags asm ("r5");
  register _L4_word_t id asm ("r6");

  __asm__ ("tlbia\n"
	   : "+r" (kip), "+r" (version), "+r" (flags), "+r" (id));

  *api_version = version;
  *api_flags = flags;
  *kernel_id = id;

  return kip;
}


static inline void
_L4_attribute_always_inline
_L4_exchange_registers (_L4_thread_id_t *dest_p, _L4_word_t *control_p,
			_L4_word_t *sp_p, _L4_word_t *ip_p,
			_L4_word_t *flags_p, _L4_word_t *user_handle_p,
			_L4_thread_id_t *pager_p)
{
  register _L4_word_t dest_result asm ("r3") = *dest_p;
  register _L4_word_t control asm ("r4") = *control_p;
  register _L4_word_t sp asm ("r5") = *sp_p;
  register _L4_word_t ip asm ("r6") = *ip_p;
  register _L4_word_t flags asm ("r7") = *flags_p;
  register _L4_word_t user_handle asm ("r8") = *user_handle_p;
  register _L4_word_t pager asm ("r9") = *pager_p;

  __asm__ __volatile__ ("mtctr %[addr]\n"
			"bctrl\n"
			: "+r" (dest_result), "+r" (control),
			"+r" (sp), "+r" (ip), "+r" (flags),
			"+r" (user_handle), "+r" (pager)
			: [addr] "r" (__l4_exchange_registers)
			: "r10", __L4_PPC_CLOB);

  *dest_p = dest_result;
  *control_p = control;
  *sp_p = sp;
  *ip_p = ip;
  *flags_p = flags;
  *user_handle_p = user_handle;
  *pager_p = pager;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_thread_control (_L4_thread_id_t dest, _L4_thread_id_t space,
		    _L4_thread_id_t scheduler, _L4_thread_id_t pager,
		    void *utcb_loc)
{
  register _L4_word_t dest_result asm ("r3") = dest;
  register _L4_word_t space_raw asm ("r4") = space;
  register _L4_word_t scheduler_raw asm ("r5") = scheduler;
  register _L4_word_t pager_raw asm ("r6") = pager;
  register _L4_word_t utcb asm ("r7") = (_L4_word_t) utcb_loc;

  __asm__ __volatile__ ("mtctr %[addr]\n"
			"bctrl\n"
			: "+r" (dest_result)
			: "r" (space_raw), "r" (scheduler_raw),
			"r" (pager_raw), "r" (utcb),
			[addr] "r" (__l4_thread_control)
			: "r8", "r9", "r10", __L4_PPC_CLOB);

  return dest_result;
}


static inline _L4_clock_t
_L4_attribute_always_inline
_L4_system_clock (void)
{
  register _L4_word_t time_low asm ("r3");
  register _L4_word_t time_high asm ("r4");

  __asm__ __volatile__ ("mtctr %[addr]\n"
			"bctrl\n"
			: "=r" (time_low), "=r" (time_high)
			: [addr] "r" (__l4_system_clock)
			: "r5", "r6", "r7", "r8", "r9", "r10",
			__L4_PPC_CLOB);

  return (((_L4_clock_t) time_high) << 32) | time_low;
}


static inline void
_L4_attribute_always_inline
_L4_thread_switch (_L4_thread_id_t dest)
{
  register _L4_word_t dest_raw asm ("r3") = dest;

  __asm__ __volatile__ ("mtctr %[addr]\n"
			"bctrl\n"
			:
			: "r" (dest_raw), [addr] "r" (__l4_thread_switch)
			: __L4_PPC_CLOB);
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_schedule (_L4_thread_id_t dest, _L4_word_t time_control,
	      _L4_word_t proc_control, _L4_word_t prio,
	      _L4_word_t preempt_control, _L4_word_t *old_time_control)
{
  register _L4_word_t dest_result asm ("r3") = dest;
  register _L4_word_t time asm ("r4") = time_control;
  register _L4_word_t proc asm ("r5") = proc_control;
  register _L4_word_t priority asm ("r6") = prio;
  register _L4_word_t preempt asm ("r7") = preempt_control;

  __asm__ __volatile__ ("mtctr %[addr]\n"
			"bctrl\n"
			: "+r" (dest_result), "+r" (time)
			: "r" (proc), "r" (priority), "r" (preempt),
			[addr] "r" (__l4_schedule)
			: "r8", "r9", "r10", __L4_PPC_CLOB);

  *old_time_control = time;
  return dest_result;
}


static inline void
_L4_attribute_always_inline
_L4_unmap (_L4_word_t control)
{
  register _L4_word_t ctrl asm ("r3") = control;

  __asm__ __volatile__ ("mtctr %[addr]\n"
			"bctrl\n"
			:
			: "r" (ctrl), [addr] "r" (__l4_unmap)
			: "r4", "r5", "r6", "r7", "r8", "r9", "r10",
			__L4_PPC_CLOB);
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_space_control (_L4_thread_id_t space, _L4_word_t control,
		   _L4_fpage_t kip_area, _L4_fpage_t utcb_area,
		   _L4_thread_id_t redirector, _L4_word_t *old_control)
{
  register _L4_word_t space_result asm ("r3") = space;
  register _L4_word_t ctrl asm ("r4") = control;
  register _L4_word_t kip asm ("r5") = kip_area;
  register _L4_word_t utcb asm ("r6") = utcb_area;
  register _L4_word_t redir asm ("r7") = redirector;

  __asm__ __volatile__ ("mtctr %[addr]\n"
			"bctrl\n"
			: "+r" (space_result), "+r" (ctrl)
			: "r" (kip), "r" (utcb), "r" (redir),
			[addr] "r" (__l4_space_control)
			: "r8", "r9", "r10", __L4_PPC_CLOB);

  *old_control = ctrl;
  return space_result;
}


static inline _L4_msg_tag_t
_L4_attribute_always_inline
_L4_ipc (_L4_thread_id_t to, _L4_thread_id_t from_spec,
	 _L4_word_t timeouts, _L4_thread_id_t *from)
{
  _L4_word_t *mr = _L4_utcb () + _L4_UTCB_MR0;
  register _L4_word_t mr9 asm ("r0") = mr[9];
  register _L4_word_t mr1 asm ("r3") = mr[1];
  register _L4_word_t mr2 asm ("r4") = mr[2];
  register _L4_word_t mr3 asm ("r5") = mr[3];
  register _L4_word_t mr4 asm ("r6") = mr[4];
  register _L4_word_t mr5 asm ("r7") = mr[5];
  register _L4_word_t mr6 asm ("r8") = mr[6];
  register _L4_word_t mr7 asm ("r9") = mr[7];
  register _L4_word_t mr8 asm ("r10") = mr[8];
  register _L4_word_t mr0 asm ("r14") = mr[0];
  register _L4_word_t to_raw asm ("r15") = to;
  register _L4_word_t from_spec_raw asm ("r16") = from_spec;
  register _L4_word_t time_outs asm ("r17") = timeouts;

  __asm__ __volatile__ ("mtctr %[addr]\n"
			"bctrl\n"
			: "+r" (mr9), "+r" (mr1), "+r" (mr2), "+r" (mr3),
			"+r" (mr4), "+r" (mr5), "+r" (mr6), "+r" (mr7),
			"+r" (mr8), "+r" (mr0), "+r" (from_spec_raw)
			: "r" (to_raw), "r" (time_outs),
			[addr] "r" (__l4_ipc)
			: "r11", "r12", "r13", __L4_PPC_XCLOB);

  /* FIXME: Make it so that we can use l4_is_nilthread?  */
  if (from_spec_raw)
    {
      *from = from_spec_raw;
      mr[1] = mr1;
      mr[2] = mr2;
      mr[3] = mr3;
      mr[4] = mr4;
      mr[5] = mr5;
      mr[6] = mr6;
      mr[7] = mr7;
      mr[8] = mr8;
      mr[9] = mr9;
    }
  return mr0;
}


static inline _L4_msg_tag_t
_L4_attribute_always_inline
_L4_lipc (_L4_thread_id_t to, _L4_thread_id_t from_spec,
	  _L4_word_t timeouts, _L4_thread_id_t *from)
{
  _L4_word_t *mr = _L4_utcb () + _L4_UTCB_MR0;
  register _L4_word_t mr9 asm ("r0") = mr[9];
  register _L4_word_t mr1 asm ("r3") = mr[1];
  register _L4_word_t mr2 asm ("r4") = mr[2];
  register _L4_word_t mr3 asm ("r5") = mr[3];
  register _L4_word_t mr4 asm ("r6") = mr[4];
  register _L4_word_t mr5 asm ("r7") = mr[5];
  register _L4_word_t mr6 asm ("r8") = mr[6];
  register _L4_word_t mr7 asm ("r9") = mr[7];
  register _L4_word_t mr8 asm ("r10") = mr[8];
  register _L4_word_t mr0 asm ("r14") = mr[0];
  register _L4_word_t to_raw asm ("r15") = to;
  register _L4_word_t from_spec_raw asm ("r16") = from_spec;
  register _L4_word_t time_outs asm ("r17") = timeouts;

  __asm__ __volatile__ ("mtctr %[addr]\n"
			"bctrl\n"
			: "+r" (mr9), "+r" (mr1), "+r" (mr2), "+r" (mr3),
			"+r" (mr4), "+r" (mr5), "+r" (mr6), "+r" (mr7),
			"+r" (mr8), "+r" (mr0), "+r" (from_spec_raw)
			: "r" (to_raw), "r" (time_outs),
			[addr] "r" (__l4_lipc)
			: "r11", "r12", "r13", __L4_PPC_XCLOB);

  /* FIXME: Make it so that we can use l4_is_nilthread?  */
  if (from_spec_raw)
    {
      *from = from_spec_raw;
      mr[1] = mr1;
      mr[2] = mr2;
      mr[3] = mr3;
      mr[4] = mr4;
      mr[5] = mr5;
      mr[6] = mr6;
      mr[7] = mr7;
      mr[8] = mr8;
      mr[9] = mr9;
    }
  return mr0;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_processor_control (_L4_word_t proc, _L4_word_t int_freq,
		       _L4_word_t ext_freq, _L4_word_t voltage)
{
  register _L4_word_t proc_result asm ("r3") = proc;
  register _L4_word_t internal_freq asm ("r4") = int_freq;
  register _L4_word_t external_freq asm ("r5") = ext_freq;
  register _L4_word_t volt asm ("r6") = voltage;

  __asm__ __volatile__ ("mtctr %[addr]\n"
			"bctrl\n"
			: "+r" (proc_result)
			: "r" (internal_freq), "r" (external_freq),
			"r" (volt),
			[addr] "r" (__l4_processor_control)
			: "r7", "r8", "r9", "r10", __L4_PPC_CLOB);

  return proc_result;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_memory_control (_L4_word_t control, _L4_word_t *attributes)
{
  register _L4_word_t ctrl_result asm ("r3") = control;
  register _L4_word_t attr0 asm ("r4") = attributes[0];
  register _L4_word_t attr1 asm ("r5") = attributes[1];
  register _L4_word_t attr2 asm ("r6") = attributes[2];
  register _L4_word_t attr3 asm ("r7") = attributes[3];

  __asm__ __volatile__ ("mtctr %[addr]\n"
			"bctrl\n"
			: "+r" (ctrl_result)
			: "r" (ctrl_result), "r" (attr0), "r" (attr1),
			"r" (attr2), "r" (attr3),
			[addr] "r" (__l4_memory_control)
			: "r8", "r9", "r10", __L4_PPC_CLOB);

  return ctrl_result;
}
