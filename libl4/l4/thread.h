/* thread.h - Public interface to L4 threads.
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

#ifndef _L4_THREAD_H
#define _L4_THREAD_H	1

#include <l4/types.h>
#include <l4/vregs.h>
#include <l4/syscall.h>

/* FIXME: These are compound statements and can not be used for
   initialization of global variables in C99.  */
#define l4_nilthread	((l4_thread_id_t) { .raw = 0 })
#define l4_anythread	((l4_thread_id_t) { .raw = (l4_word_t) -1 })
/* FIXME: When gcc supports unnamed fields as initializers, use them.  */
#define l4_anylocalthread \
	((l4_thread_id_t) { .local.local = -1, .local._all_zero = 0 })


static inline  l4_thread_id_t
__attribute__((__always_inline__))
l4_global_id (l4_word_t thread_no, l4_word_t version)
{
  l4_thread_id_t thread;

  thread.thread_no = thread_no;
  thread.version = version;

  return thread;
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_version (l4_thread_id_t thread)
{
  return thread.version;
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_thread_no (l4_thread_id_t thread)
{
  return thread.thread_no;
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_is_thread_equal (l4_thread_id_t thread1, l4_thread_id_t thread2)
{
  return thread1.raw == thread2.raw;
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_is_thread_not_equal (l4_thread_id_t thread1, l4_thread_id_t thread2)
{
  return thread1.raw != thread2.raw;
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_is_nil_thread (l4_thread_id_t thread)
{
  return thread.raw == 0;
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_is_local_id (l4_thread_id_t thread)
{
  return thread._all_zero == 0;
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_is_global_id (l4_thread_id_t thread)
{
  return thread._all_zero != 0;
}


static inline l4_thread_id_t
__attribute__((__always_inline__))
l4_myself (void)
{
  return l4_my_global_id ();
}


static inline l4_thread_id_t
__attribute__((__always_inline__))
l4_global_id_of (l4_thread_id_t thread)
{
  if (l4_is_global_id (thread))
    return thread;
  else
    {
      l4_thread_id_t dest = thread;
      l4_word_t control = 0;
      l4_word_t dummy = 0;
      l4_thread_id_t pager = l4_nilthread;

      l4_exchange_registers (&dest, &control, &dummy, &dummy, &dummy,
			     &dummy, &pager);
      return dest;
    }
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_same_threads (l4_thread_id_t thread1, l4_thread_id_t thread2)
{
  l4_thread_id_t global1 = l4_global_id_of (thread1);
  l4_thread_id_t global2 = l4_global_id_of (thread2);

  return global1.raw == global2.raw;
}


static inline l4_thread_id_t
__attribute__((__always_inline__))
l4_local_id_of (l4_thread_id_t thread)
{
  if (l4_is_local_id (thread))
    return thread;
  else
    {
      l4_thread_id_t dest = thread;
      l4_word_t control = 0;
      l4_word_t dummy = 0;
      l4_thread_id_t pager = l4_nilthread;

      l4_exchange_registers (&dest, &control, &dummy, &dummy, &dummy,
			     &dummy, &pager);
      return dest;
    }
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_user_defined_handle_of (l4_thread_id_t thread)
{
  l4_thread_id_t dest = thread;
  l4_word_t control = 0;
  l4_word_t user_handle = 0;
  l4_word_t dummy = 0;
  l4_thread_id_t pager = l4_nilthread;

  l4_exchange_registers (&dest, &control, &dummy, &dummy, &dummy,
			 &user_handle, &pager);
  return user_handle;
}


static inline void
__attribute__((__always_inline__))
l4_set_user_defined_handle_of (l4_thread_id_t thread, l4_word_t handle)
{
  l4_thread_id_t dest = thread;
  l4_word_t control = L4_XCHG_REGS_SET_USER_HANDLE;
  l4_word_t user_handle = handle;
  l4_word_t dummy = 0;
  l4_thread_id_t pager = l4_nilthread;

  l4_exchange_registers (&dest, &control, &dummy, &dummy, &dummy,
			 &user_handle, &pager);
}


static inline l4_thread_id_t
__attribute__((__always_inline__))
l4_pager_of (l4_thread_id_t thread)
{
  l4_thread_id_t dest = thread;
  l4_word_t control = 0;
  l4_thread_id_t pager = l4_nilthread;
  l4_word_t dummy = 0;

  l4_exchange_registers (&dest, &control, &dummy, &dummy, &dummy,
			 &dummy, &pager);
  return pager;
}


static inline void
__attribute__((__always_inline__))
l4_set_pager_of (l4_thread_id_t thread, l4_thread_id_t pager_thread)
{
  l4_thread_id_t dest = thread;
  l4_word_t control = L4_XCHG_REGS_SET_PAGER;
  l4_thread_id_t pager = pager_thread;
  l4_word_t dummy = 0;

  l4_exchange_registers (&dest, &control, &dummy, &dummy, &dummy,
			 &dummy, &pager);
}


static inline void
__attribute__((__always_inline__))
l4_start (l4_thread_id_t thread)
{
  l4_thread_id_t dest = thread;
  l4_word_t control = L4_XCHG_REGS_SET_HALT;
  l4_word_t dummy = 0;
  l4_thread_id_t pager = l4_nilthread;

  l4_exchange_registers (&dest, &control, &dummy, &dummy, &dummy,
			 &dummy, &pager);
}


static inline void
__attribute__((__always_inline__))
l4_start_sp_ip (l4_thread_id_t thread, l4_word_t sp_data, l4_word_t ip_data)
{
  l4_thread_id_t dest = thread;
  l4_word_t control = L4_XCHG_REGS_SET_HALT | L4_XCHG_REGS_SET_SP
    | L4_XCHG_REGS_SET_IP;
  l4_word_t sp = sp_data;
  l4_word_t ip = ip_data;
  l4_word_t dummy = 0;
  l4_thread_id_t pager = l4_nilthread;

  l4_exchange_registers (&dest, &control, &sp, &ip, &dummy, &dummy, &pager);
}


static inline void
__attribute__((__always_inline__))
l4_start_sp_ip_flags (l4_thread_id_t thread, l4_word_t sp_data,
		      l4_word_t ip_data, l4_word_t flags_data)
{
  l4_thread_id_t dest = thread;
  l4_word_t control = L4_XCHG_REGS_SET_HALT | L4_XCHG_REGS_SET_SP
    | L4_XCHG_REGS_SET_IP | L4_XCHG_REGS_SET_FLAGS;
  l4_word_t sp = sp_data;
  l4_word_t ip = ip_data;
  l4_word_t flags = flags_data;
  l4_word_t dummy = 0;
  l4_thread_id_t pager = l4_nilthread;

  l4_exchange_registers (&dest, &control, &sp, &ip, &flags, &dummy, &pager);
}


#define __L4_STOP(name, extra_control)				\
static inline l4_word_t						\
__attribute__((__always_inline__))				\
name (l4_thread_id_t thread)					\
{								\
  l4_thread_id_t dest = thread;					\
  l4_word_t control = L4_XCHG_REGS_SET_HALT | L4_XCHG_REGS_HALT	\
    | (extra_control);						\
  l4_word_t dummy = 0;						\
  l4_thread_id_t pager = l4_nilthread;				\
								\
  l4_exchange_registers (&dest, &control, &dummy, &dummy,	\
			 &dummy, &dummy, &pager);		\
  return control;						\
}								\
								\
								\
static inline l4_word_t						\
__attribute__((__always_inline__))				\
name ## _sp_ip_flags (l4_thread_id_t thread, l4_word_t *sp,	\
                      l4_word_t *ip, l4_word_t *flags)		\
{								\
  l4_thread_id_t dest = thread;					\
  l4_word_t control = L4_XCHG_REGS_SET_HALT | L4_XCHG_REGS_HALT	\
    | L4_XCHG_REGS_SET_SP | L4_XCHG_REGS_SET_IP			\
    | (extra_control);						\
  l4_word_t dummy = 0;						\
  l4_thread_id_t pager = l4_nilthread;				\
								\
  l4_exchange_registers (&dest, &control, sp, ip, flags,	\
			 &dummy, &pager);			\
  return control;						\
}

__L4_STOP (l4_stop, 0)
__L4_STOP (l4_abort_receive_and_stop, L4_XCHG_REGS_CANCEL_RECV)
__L4_STOP (l4_abort_send_and_stop, L4_XCHG_REGS_CANCEL_SEND)
__L4_STOP (l4_abort_ipc_and_stop, L4_XCHG_REGS_CANCEL_IPC)


/* Convenience interface for l4_thread_control.  */ 

static inline l4_word_t
__attribute__((__always_inline__))
l4_associate_interrupt (l4_thread_id_t irq, l4_thread_id_t handler)
{
  return l4_thread_control (irq, irq, l4_nilthread, handler, (void *) -1);
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_deassociate_interrupt (l4_thread_id_t irq)
{
  return l4_thread_control (irq, irq, l4_nilthread, irq, (void *) -1);
}


#endif	/* l4/thread.h */
