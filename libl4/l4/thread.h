/* l4/thread.h - Public interface to L4 threads.
   Copyright (C) 2003, 2004 Free Software Foundation, Inc.
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


typedef _L4_RAW
(_L4_word_t, _L4_STRUCT2
 ({
   _L4_BITFIELD2
     (_L4_word_t,
      _L4_BITFIELD_32_64 (version, 14, 32),
      _L4_BITFIELD_32_64 (thread_no, 18, 32));
 },
 {
   _L4_BITFIELD2
     (_L4_word_t,
      _L4_BITFIELD (_all_zero, 6),
      _L4_BITFIELD_32_64 (local, 26, 58));
 })) __L4_thread_id_t;


#if _L4_WORDSIZE == 32
#define _L4_THREAD_VERSION_BITS	(14)
#define _L4_THREAD_NO_BITS	(32 - _L4_THREAD_VERSION_BITS)
#else
#define _L4_THREAD_VERSION_BITS	(32)
#define _L4_THREAD_NO_BITS	(64 - _L4_THREAD_VERSION_BITS)
#endif

#define _L4_THREAD_NO_MASK	((_L4_WORD_C(1) << _L4_THREAD_NO_BITS) - 1)
#define _L4_THREAD_VERSION_MASK	((_L4_WORD_C(1) << _L4_THREAD_VERSION_BITS) -1)

/* These define the raw versions of the special thread IDs.  */
#define _L4_nilthread		_L4_WORD_C(0)
#define _L4_anythread		(~ _L4_WORD_C(0))
#define _L4_anylocalthread	((~ _L4_WORD_C(0)) << 6)


static inline _L4_thread_id_t
_L4_attribute_always_inline
_L4_global_id (_L4_word_t thread_no, _L4_word_t version)
{
  __L4_thread_id_t tid;

  tid.thread_no = thread_no;
  tid.version = version;

  return tid.raw;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_version (_L4_thread_id_t thread)
{
  __L4_thread_id_t tid;

  tid.raw = thread;
  return tid.version;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_thread_no (_L4_thread_id_t thread)
{
  __L4_thread_id_t tid;

  tid.raw = thread;
  return tid.thread_no;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_is_thread_equal (_L4_thread_id_t thread1, _L4_thread_id_t thread2)
{
  return thread1 == thread2;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_is_thread_not_equal (_L4_thread_id_t thread1, _L4_thread_id_t thread2)
{
  return thread1 != thread2;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_is_nil_thread (_L4_thread_id_t thread)
{
  return thread == _L4_nilthread;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_is_local_id (_L4_thread_id_t thread)
{
  __L4_thread_id_t tid;

  tid.raw = thread;
  return tid._all_zero == 0;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_is_global_id (_L4_thread_id_t thread)
{
  __L4_thread_id_t tid;

  tid.raw = thread;
  return tid._all_zero != 0;
}


static inline _L4_thread_id_t
_L4_attribute_always_inline
_L4_myself (void)
{
  return _L4_my_global_id ();
}


static inline _L4_thread_id_t
_L4_attribute_always_inline
_L4_global_id_of (_L4_thread_id_t thread)
{
  if (_L4_is_global_id (thread))
    return thread;
  else
    {
      _L4_word_t control = 0;
      _L4_word_t dummy = 0;
      _L4_thread_id_t pager = _L4_nilthread;

      _L4_exchange_registers (&thread, &control, &dummy, &dummy, &dummy,
			      &dummy, &pager);
      return thread;
    }
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_same_threads (_L4_thread_id_t thread1, _L4_thread_id_t thread2)
{
  _L4_thread_id_t global1 = _L4_global_id_of (thread1);
  _L4_thread_id_t global2 = _L4_global_id_of (thread2);

  return global1 == global2;
}


static inline _L4_thread_id_t
_L4_attribute_always_inline
_L4_local_id_of (_L4_thread_id_t thread)
{
  if (_L4_is_local_id (thread))
    return thread;
  else
    {
      _L4_word_t control = 0;
      _L4_word_t dummy = 0;
      _L4_thread_id_t pager = _L4_nilthread;

      _L4_exchange_registers (&thread, &control, &dummy, &dummy, &dummy,
			      &dummy, &pager);
      return thread;
    }
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_user_defined_handle_of (_L4_thread_id_t thread)
{
  _L4_word_t control = 0;
  _L4_word_t user_handle = 0;
  _L4_word_t dummy = 0;
  _L4_thread_id_t pager = _L4_nilthread;

  _L4_exchange_registers (&thread, &control, &dummy, &dummy, &dummy,
			  &user_handle, &pager);
  return user_handle;
}


static inline void
_L4_attribute_always_inline
_L4_set_user_defined_handle_of (_L4_thread_id_t thread, _L4_word_t handle)
{
  _L4_word_t control = _L4_XCHG_REGS_SET_USER_HANDLE;
  _L4_word_t user_handle = handle;
  _L4_word_t dummy = 0;
  _L4_thread_id_t pager = _L4_nilthread;

  _L4_exchange_registers (&thread, &control, &dummy, &dummy, &dummy,
			  &user_handle, &pager);
}


static inline _L4_thread_id_t
_L4_attribute_always_inline
_L4_pager_of (_L4_thread_id_t thread)
{
  _L4_word_t control = 0;
  _L4_thread_id_t pager = _L4_nilthread;
  _L4_word_t dummy = 0;

  _L4_exchange_registers (&thread, &control, &dummy, &dummy, &dummy,
			  &dummy, &pager);
  return pager;
}


static inline void
_L4_attribute_always_inline
_L4_set_pager_of (_L4_thread_id_t thread, _L4_thread_id_t pager_thread)
{
  _L4_word_t control = _L4_XCHG_REGS_SET_PAGER;
  _L4_thread_id_t pager = pager_thread;
  _L4_word_t dummy = 0;

  _L4_exchange_registers (&thread, &control, &dummy, &dummy, &dummy,
			  &dummy, &pager);
}


static inline void
_L4_attribute_always_inline
_L4_start (_L4_thread_id_t thread)
{
  _L4_word_t control = _L4_XCHG_REGS_SET_HALT | _L4_XCHG_REGS_CANCEL_IPC;
  _L4_word_t dummy = 0;
  _L4_thread_id_t pager = _L4_nilthread;

  _L4_exchange_registers (&thread, &control, &dummy, &dummy, &dummy,
			  &dummy, &pager);
}


static inline void
_L4_attribute_always_inline
_L4_start_sp_ip (_L4_thread_id_t thread, _L4_word_t sp, _L4_word_t ip)
{
  _L4_word_t control = _L4_XCHG_REGS_SET_HALT | _L4_XCHG_REGS_SET_SP
    | _L4_XCHG_REGS_SET_IP | _L4_XCHG_REGS_CANCEL_IPC;
  _L4_word_t dummy = 0;
  _L4_thread_id_t pager = _L4_nilthread;

  _L4_exchange_registers (&thread, &control, &sp, &ip, &dummy, &dummy, &pager);
}


static inline void
_L4_attribute_always_inline
_L4_start_sp_ip_flags (_L4_thread_id_t thread, _L4_word_t sp,
		      _L4_word_t ip, _L4_word_t flags)
{
  _L4_word_t control = _L4_XCHG_REGS_SET_HALT | _L4_XCHG_REGS_SET_SP
    | _L4_XCHG_REGS_SET_IP | _L4_XCHG_REGS_SET_FLAGS
    | _L4_XCHG_REGS_CANCEL_IPC;
  _L4_word_t dummy = 0;
  _L4_thread_id_t pager = _L4_nilthread;

  _L4_exchange_registers (&thread, &control, &sp, &ip, &flags, &dummy, &pager);
}


#define __L4_STOP(name, extra_control)					\
static inline _L4_word_t						\
_L4_attribute_always_inline						\
name (_L4_thread_id_t thread)						\
{									\
  _L4_word_t control = _L4_XCHG_REGS_SET_HALT | _L4_XCHG_REGS_HALT	\
    | (extra_control);							\
  _L4_word_t dummy = 0;							\
  _L4_thread_id_t pager = _L4_nilthread;				\
									\
  _L4_exchange_registers (&thread, &control, &dummy, &dummy,		\
			  &dummy, &dummy, &pager);			\
  return control;							\
}									\
									\
									\
static inline _L4_word_t						\
_L4_attribute_always_inline						\
name ## _sp_ip_flags (_L4_thread_id_t thread, _L4_word_t *sp,		\
                      _L4_word_t *ip, _L4_word_t *flags)		\
{									\
  _L4_word_t control = _L4_XCHG_REGS_SET_HALT | _L4_XCHG_REGS_HALT	\
    | (extra_control);							\
  _L4_word_t dummy = 0;							\
  _L4_thread_id_t pager = _L4_nilthread;				\
									\
  _L4_exchange_registers (&thread, &control, sp, ip, flags,		\
			  &dummy, &pager);				\
  return control;							\
}

__L4_STOP (_L4_stop, 0)
__L4_STOP (_L4_abort_receive_and_stop, _L4_XCHG_REGS_CANCEL_RECV)
__L4_STOP (_L4_abort_send_and_stop, _L4_XCHG_REGS_CANCEL_SEND)
__L4_STOP (_L4_abort_ipc_and_stop, _L4_XCHG_REGS_CANCEL_IPC)
#undef __L4_STOP


/* Convenience interface for l4_thread_control.  */ 

static inline _L4_word_t
_L4_attribute_always_inline
_L4_associate_interrupt (_L4_thread_id_t irq, _L4_thread_id_t handler)
{
  return _L4_thread_control (irq, irq, _L4_nilthread, handler, (void *) -1);
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_deassociate_interrupt (_L4_thread_id_t irq)
{
  return _L4_thread_control (irq, irq, _L4_nilthread, irq, (void *) -1);
}


/* Now incorporate the public interfaces the user has selected.  */
#ifdef _L4_INTERFACE_L4
#include <l4/compat/thread.h>
#endif
#ifdef _L4_INTERFACE_GNU
#include <l4/gnu/thread.h>
#endif

#endif	/* l4/thread.h */
