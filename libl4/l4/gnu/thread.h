/* l4/gnu/thread.h - Public GNU interface for L4 threads.
   Copyright (C) 2004 Free Software Foundation, Inc.
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
# error "Never use <l4/gnu/thread.h> directly; include <l4/thread.h> instead."
#endif

/* l4_thread_id_t is defined in <l4/gnu/types.h>.  */

#define l4_nilthread		_L4_nilthread
#define l4_anythread		_L4_anythread
#define l4_anylocalthread	_L4_anylocalthread

/* The theoretical maximum number of thread no bits.  The real limit
   is l4_thread_id_bits().  */
#define L4_THREAD_NO_BITS	_L4_THREAD_NO_BITS
#define L4_THREAD_VERSION_BITS	_L4_THREAD_VERSION_BITS


static inline l4_thread_id_t
_L4_attribute_always_inline
l4_global_id (l4_word_t thread_no, l4_word_t version)
{
  return _L4_global_id (thread_no, version);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_version (l4_thread_id_t thread)
{
  return _L4_version (thread);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_thread_no (l4_thread_id_t thread)
{
  return _L4_thread_no (thread);
}


static inline bool
_L4_attribute_always_inline
l4_is_thread_equal (l4_thread_id_t thread1, l4_thread_id_t thread2)
{
  return _L4_is_thread_equal (thread1, thread2);
}


static inline bool
_L4_attribute_always_inline
l4_is_thread_not_equal (l4_thread_id_t thread1, l4_thread_id_t thread2)
{
  return _L4_is_thread_not_equal (thread1, thread2);
}


static inline bool
_L4_attribute_always_inline
l4_same_threads (l4_thread_id_t thread1, l4_thread_id_t thread2)
{
  return _L4_same_threads (thread1, thread2);
}


static inline bool
_L4_attribute_always_inline
l4_is_nil_thread (l4_thread_id_t thread)
{
  return _L4_is_nil_thread (thread);
}


static inline bool
_L4_attribute_always_inline
l4_is_local_id (l4_thread_id_t thread)
{
  return _L4_is_local_id (thread);
}


static inline bool
_L4_attribute_always_inline
l4_is_global_id (l4_thread_id_t thread)
{
  return _L4_is_global_id (thread);
}


static inline l4_thread_id_t
_L4_attribute_always_inline
l4_my_local_id (void)
{
  return _L4_my_local_id ();
}


static inline l4_thread_id_t
_L4_attribute_always_inline
l4_my_global_id (void)
{
  return _L4_my_global_id ();
}


static inline l4_thread_id_t
_L4_attribute_always_inline
l4_myself (void)
{
  return _L4_myself ();
}


static inline l4_thread_id_t
_L4_attribute_always_inline
l4_global_id_of (l4_thread_id_t thread)
{
  return _L4_global_id_of (thread);
}


static inline l4_thread_id_t
_L4_attribute_always_inline
l4_local_id_of (l4_thread_id_t thread)
{
  return _L4_local_id_of (thread);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_processor_no (void)
{
  return _L4_processor_no ();
}


static inline l4_word_t
_L4_attribute_always_inline
l4_user_defined_handle_of (l4_thread_id_t thread)
{
  return _L4_user_defined_handle_of (thread);
}


static inline void
_L4_attribute_always_inline
l4_set_user_defined_handle_of (l4_thread_id_t thread, l4_word_t handle)
{
  return _L4_set_user_defined_handle_of (thread, handle);
}


static inline l4_thread_id_t
_L4_attribute_always_inline
l4_pager_of (l4_thread_id_t thread)
{
  return _L4_pager_of (thread);
}


static inline void
_L4_attribute_always_inline
l4_set_pager_of (l4_thread_id_t thread, l4_thread_id_t pager_thread)
{
  return _L4_set_pager_of (thread, pager_thread);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_user_defined_handle (void)
{
  return _L4_user_defined_handle ();
}


static inline void
_L4_attribute_always_inline
l4_set_user_defined_handle (l4_word_t new)
{
  _L4_set_user_defined_handle (new);
}


static inline l4_thread_id_t
_L4_attribute_always_inline
l4_pager (void)
{
  return _L4_pager ();
}


static inline void
_L4_attribute_always_inline
l4_set_pager (l4_thread_id_t pager)
{
  _L4_set_pager (pager);
}


static inline l4_thread_id_t
_L4_attribute_always_inline
l4_exception_handler (void)
{
  return _L4_exception_handler ();
}


static inline void
_L4_attribute_always_inline
l4_set_exception_handler (l4_thread_id_t handler)
{
  _L4_set_exception_handler (handler);
}


static inline void
_L4_attribute_always_inline
l4_set_cop_flag (l4_word_t n)
{
  _L4_set_cop_flag (n);
}


static inline void
_L4_attribute_always_inline
l4_clr_cop_flag (l4_word_t n)
{
  _L4_clr_cop_flag (n);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_error_code (void)
{
  return _L4_error_code ();
}


static inline l4_word_t
_L4_attribute_always_inline
l4_xfer_timeouts (void)
{
  return _L4_xfer_timeouts ();
}


static inline void
_L4_attribute_always_inline
l4_set_xfer_timeouts (l4_word_t timeouts)
{
  _L4_set_xfer_timeouts (timeouts);
}


static inline l4_thread_id_t
_L4_attribute_always_inline
l4_intended_receiver (void)
{
  return _L4_intended_receiver ();
}


static inline l4_thread_id_t
_L4_attribute_always_inline
l4_actual_sender (void)
{
  return _L4_actual_sender ();
}


static inline void
_L4_attribute_always_inline
l4_set_virtual_sender (l4_thread_id_t t)
{
  _L4_set_virtual_sender (t);
}



static inline void
_L4_attribute_always_inline
l4_exchange_registers (l4_thread_id_t *dest, l4_word_t *control,
		       l4_word_t *sp, l4_word_t *ip, l4_word_t *flags,
		       l4_word_t *user_defined_handle, l4_thread_id_t *pager)
{
  _L4_exchange_registers (dest, control, sp, ip, flags,
			  user_defined_handle, pager);
}


static inline void
_L4_attribute_always_inline
l4_start (l4_thread_id_t thread)
{
  _L4_start (thread);
}


static inline void
_L4_attribute_always_inline
l4_start_sp_ip (l4_thread_id_t thread, l4_word_t sp, l4_word_t ip)
{
  _L4_start_sp_ip (thread, sp, ip);
}


static inline void
_L4_attribute_always_inline
l4_start_sp_ip_flags (l4_thread_id_t thread, l4_word_t sp,
		      l4_word_t ip, l4_word_t flags)
{
  return _L4_start_sp_ip_flags (thread, sp, ip, flags);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_stop (l4_thread_id_t thread)
{
  return _L4_stop (thread);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_stop_sp_ip_flags (l4_thread_id_t thread, l4_word_t *sp, l4_word_t *ip,
		     l4_word_t *flags)
{
  return _L4_stop_sp_ip_flags (thread, sp, ip, flags);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_abort_receive_and_stop (l4_thread_id_t thread)
{
  return _L4_abort_receive_and_stop (thread);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_abort_receive_and_stop_sp_ip_flags (l4_thread_id_t thread, l4_word_t *sp,
				       l4_word_t *ip, l4_word_t *flags)
{
  return _L4_abort_receive_and_stop_sp_ip_flags (thread, sp, ip, flags);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_abort_send_and_stop (l4_thread_id_t thread)
{
  return _L4_abort_send_and_stop (thread);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_abort_send_and_stop_sp_ip_flags
(l4_thread_id_t thread, l4_word_t *sp, l4_word_t *ip, l4_word_t *flags)
{
  return _L4_abort_send_and_stop_sp_ip_flags (thread, sp, ip, flags);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_abort_ipc_and_stop (l4_thread_id_t thread)
{
  return _L4_abort_ipc_and_stop (thread);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_abort_ipc_and_stop_sp_ip_flags
(l4_thread_id_t thread, l4_word_t *sp, l4_word_t *ip, l4_word_t *flags)
{
  return _L4_abort_ipc_and_stop_sp_ip_flags (thread, sp, ip, flags);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_thread_control (l4_thread_id_t dest, l4_thread_id_t space,
		   l4_thread_id_t scheduler, l4_thread_id_t pager, void *utcb)
{
  return _L4_thread_control (dest, space, scheduler, pager, utcb);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_associate_interrupt (l4_thread_id_t irq, l4_thread_id_t handler)
{
  return _L4_associate_interrupt (irq, handler);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_deassociate_interrupt (l4_thread_id_t irq)
{
  return _L4_deassociate_interrupt (irq);
}
