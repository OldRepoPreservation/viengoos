/* l4/compat/thread.h - Public interface for L4 threads.
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
# error "Never use <l4/compat/thread.h> directly; include <l4/thread.h> instead."
#endif


/* 2.1 ThreadID [Data Type]  */

/* Generic Programming Interface.  */

#define L4_nilthread		((L4_ThreadId_t) { .raw = _L4_nilthread })
#define L4_anythread		((L4_ThreadId_t) { .raw = _L4_anythread })
#define L4_anylocalthread	((L4_ThreadId_t) { .raw = _L4_anylocalthread })


static inline L4_ThreadId_t
_L4_attribute_always_inline
L4_GlobalId (L4_Word_t threadno, L4_Word_t version)
{
  L4_ThreadId_t tid;

  tid.raw = _L4_global_id (threadno, version);
  return tid;
}


static inline L4_Word_t
_L4_attribute_always_inline
L4_Version (L4_ThreadId_t t)
{
  return _L4_version (t.raw);
}


static inline L4_Word_t
_L4_attribute_always_inline
L4_ThreadNo (L4_ThreadId_t t)
{
  return _L4_thread_no (t.raw);
}


/* Convenience Programming Interface.  */

static inline L4_Bool_t
_L4_attribute_always_inline
#if defined(__cplusplus)
operator == (L4_ThreadId_t l, L4_ThreadId_t r)
#else
L4_IsThreadEqual (L4_ThreadId_t l, L4_ThreadId_t r)
#endif
{
  return _L4_is_thread_equal (l.raw, r.raw);
}


static inline L4_Bool_t
_L4_attribute_always_inline
#if defined(__cplusplus)
operator != (L4_ThreadId_t l, L4_ThreadId_t r)
#else
L4_IsThreadNotEqual (L4_ThreadId_t l, L4_ThreadId_t r)
#endif
{
  return _L4_is_thread_not_equal (l.raw, r.raw);
}


static inline L4_Bool_t
_L4_attribute_always_inline
L4_SameThreads (L4_ThreadId_t l, L4_ThreadId_t r)
{
  return _L4_same_threads (l.raw, r.raw);
}


static inline L4_Bool_t
_L4_attribute_always_inline
L4_IsNilThread (L4_ThreadId_t t)
{
  return _L4_is_thread_equal (_L4_nilthread, t.raw);
}


static inline L4_Bool_t
_L4_attribute_always_inline
L4_IsLocalId (L4_ThreadId_t t)
{
  return _L4_is_local_id (t.raw);
}


static inline L4_Bool_t
_L4_attribute_always_inline
L4_IsGlobalId (L4_ThreadId_t t)
{
  return _L4_is_global_id (t.raw);
}


static inline L4_ThreadId_t
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_LocalId (L4_ThreadId_t t)
#else
L4_LocalIdOf (L4_ThreadId_t t)
#endif
{
  L4_ThreadId_t local_tid;

  local_tid.raw = _L4_local_id_of (t.raw);
  return local_tid;
}


static inline L4_ThreadId_t
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_GlobalId (L4_ThreadId_t t)
#else
L4_GlobalIdOf (L4_ThreadId_t t)
#endif
{
  L4_ThreadId_t global_tid;

  global_tid.raw = _L4_global_id_of (t.raw);
  return global_tid;
}


static inline L4_ThreadId_t
_L4_attribute_always_inline
L4_MyLocalId (void)
{
  L4_ThreadId_t tid;

  tid.raw = _L4_my_local_id ();
  return tid;
}


static inline L4_ThreadId_t
_L4_attribute_always_inline
L4_MyGlobalId (void)
{
  L4_ThreadId_t tid;

  tid.raw = _L4_my_global_id ();
  return tid;
}


static inline L4_ThreadId_t
_L4_attribute_always_inline
L4_Myself (void)
{
  return L4_MyGlobalId ();
}


/* 2.2 Thread Control Registers (TCRs) [Virtual Registers]  */

/* Generic Programming Interface.  */

/* MyLocalId, MyGlobalID and Myself are defined above.  */

static inline int
_L4_attribute_always_inline
L4_ProcessorNo (void)
{
  return _L4_processor_no ();
}


static inline L4_Word_t
_L4_attribute_always_inline
L4_UserDefinedHandle (void)
{
  return _L4_user_defined_handle ();
}


static inline void
_L4_attribute_always_inline
L4_Set_UserDefinedHandle (L4_Word_t NewValue)
{
  _L4_set_user_defined_handle (NewValue);
}


static inline L4_ThreadId_t
_L4_attribute_always_inline
L4_Pager (void)
{
  L4_ThreadId_t tid;

  tid.raw = _L4_pager ();
  return tid;
}


static inline void
_L4_attribute_always_inline
L4_Set_Pager (L4_ThreadId_t NewPager)
{
  _L4_set_pager (NewPager.raw);
}


static inline L4_ThreadId_t
_L4_attribute_always_inline
L4_ExceptionHandler (void)
{
  L4_ThreadId_t tid;

  tid.raw = _L4_exception_handler ();
  return tid;
}


static inline void
_L4_attribute_always_inline
L4_Set_ExceptionHandler (L4_ThreadId_t NewHandler)
{
  _L4_set_exception_handler (NewHandler.raw);
}


static inline void
_L4_attribute_always_inline
L4_Set_CopFlag (L4_Word_t n)
{
  _L4_set_cop_flag (n);
}


static inline void
_L4_attribute_always_inline
L4_Clr_CopFlag (L4_Word_t n)
{
  _L4_clr_cop_flag (n);
}


static inline L4_Word_t
_L4_attribute_always_inline
L4_ErrorCode (void)
{
  return _L4_error_code ();
}


static inline L4_Word_t
_L4_attribute_always_inline
L4_XferTimeouts (void)
{
  return _L4_xfer_timeouts ();
}


static inline void
_L4_attribute_always_inline
L4_Set_XferTimeouts (L4_Word_t NewValue)
{
  _L4_set_xfer_timeouts (NewValue);
}


static inline L4_ThreadId_t
_L4_attribute_always_inline
L4_IntendedReceiver (void)
{
  L4_ThreadId_t tid;

  tid.raw = _L4_intended_receiver ();
  return tid;
}


static inline L4_ThreadId_t
_L4_attribute_always_inline
L4_ActualSender (void)
{
  L4_ThreadId_t tid;

  tid.raw = _L4_actual_sender ();
  return tid;
}


static inline void
_L4_attribute_always_inline
L4_Set_VirtualSender (L4_ThreadId_t t)
{
  _L4_set_virtual_sender (t.raw);
}


/* 2.3 ExchangeRegisters [Systemcall]  */

/* Generic Programming Interface.  */

static inline L4_ThreadId_t
_L4_attribute_always_inline
L4_ExchangeRegisters (L4_ThreadId_t dest, L4_Word_t control, L4_Word_t sp,
		      L4_Word_t ip, L4_Word_t flags,
		      L4_Word_t UserDefinedHandle, L4_ThreadId_t pager,
		      L4_Word_t *old_control, L4_Word_t *old_sp,
		      L4_Word_t *old_ip, L4_Word_t *old_flags,
		      L4_Word_t *old_UserDefinedHandle,
		      L4_ThreadId_t *old_pager)
{
  L4_ThreadId_t dest_tid;

  dest_tid = dest;
  *old_control = control;
  *old_sp = sp;
  *old_ip = ip;
  *old_flags = flags;
  *old_UserDefinedHandle = UserDefinedHandle;
  *old_pager = pager;

  _L4_exchange_registers (&dest_tid.raw, old_control, old_sp, old_ip,
			  old_flags, old_UserDefinedHandle, &old_pager->raw);
  return dest_tid;
}


/* Convenience Programming Interface.  */

/* GlobalId, LocalId, UserDefinedHandle, Set_UserDefinedHandle, Pager
   and Set_Pager are defined above.  */

static inline void
_L4_attribute_always_inline
L4_Start (L4_ThreadId_t t)
{
  _L4_start (t.raw);
}

static inline void
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_Start (L4_ThreadId_t t, L4_Word_t sp, L4_Word_t ip)
#else
L4_Start_SpIp (L4_ThreadId_t t, L4_Word_t sp, L4_Word_t ip)
#endif
{
  _L4_start_sp_ip (t.raw, sp, ip);
}


static inline void
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_Start (L4_ThreadId_t t, L4_Word_t sp, L4_Word_t ip, L4_Word_t flags)
#else
L4_Start_SpIpFlags (L4_ThreadId_t t, L4_Word_t sp, L4_Word_t ip,
		    L4_Word_t flags)
#endif
{
  _L4_start_sp_ip_flags (t.raw, sp, ip, flags);
}


/* This data type is part of the support functions section, but
   required here.  */
typedef struct
{
  L4_Word_t raw;
} L4_ThreadState_t;


static inline L4_ThreadState_t
_L4_attribute_always_inline
L4_Stop (L4_ThreadId_t t)
{
  L4_ThreadState_t s;

  s.raw = _L4_stop (t.raw);
  return s;
}


static inline L4_ThreadState_t
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_Stop (L4_ThreadId_t t, L4_Word_t *sp, L4_Word_t *ip, L4_Word_t *flags)
#else
L4_Stop_SpIpFlags (L4_ThreadId_t t, L4_Word_t *sp, L4_Word_t *ip,
		    L4_Word_t *flags)
#endif
{
  L4_ThreadState_t s;

  s.raw = _L4_stop_sp_ip_flags (t.raw, sp, ip, flags);
  return s;
}


static inline L4_ThreadState_t
_L4_attribute_always_inline
L4_AbortReceive_and_stop (L4_ThreadId_t t)
{
  L4_ThreadState_t s;

  s.raw = _L4_abort_receive_and_stop (t.raw);
  return s;
}


static inline L4_ThreadState_t
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_AbortReceive_and_stop (L4_ThreadId_t t, L4_Word_t *sp, L4_Word_t *ip,
			   L4_Word_t *flags)
#else
L4_AbortReceive_and_stop_SpIpFlags (L4_ThreadId_t t, L4_Word_t *sp,
				    L4_Word_t *ip, L4_Word_t *flags)
#endif
{
  L4_ThreadState_t s;

  s.raw = _L4_abort_receive_and_stop_sp_ip_flags (t.raw, sp, ip, flags);
  return s;
}


static inline L4_ThreadState_t
_L4_attribute_always_inline
L4_AbortSend_and_stop (L4_ThreadId_t t)
{
  L4_ThreadState_t s;

  s.raw = _L4_abort_send_and_stop (t.raw);
  return s;
}


static inline L4_ThreadState_t
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_AbortSend_and_stop (L4_ThreadId_t t, L4_Word_t *sp, L4_Word_t *ip,
			   L4_Word_t *flags)
#else
L4_AbortSend_and_stop_SpIpFlags (L4_ThreadId_t t, L4_Word_t *sp,
				    L4_Word_t *ip, L4_Word_t *flags)
#endif
{
  L4_ThreadState_t s;

  s.raw = _L4_abort_send_and_stop_sp_ip_flags (t.raw, sp, ip, flags);
  return s;
}


static inline L4_ThreadState_t
_L4_attribute_always_inline
L4_AbortIpc_and_stop (L4_ThreadId_t t)
{
  L4_ThreadState_t s;

  s.raw = _L4_abort_ipc_and_stop (t.raw);
  return s;
}


static inline L4_ThreadState_t
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_AbortIpc_and_stop (L4_ThreadId_t t, L4_Word_t *sp, L4_Word_t *ip,
			   L4_Word_t *flags)
#else
L4_AbortIpc_and_stop_SpIpFlags (L4_ThreadId_t t, L4_Word_t *sp,
				    L4_Word_t *ip, L4_Word_t *flags)
#endif
{
  L4_ThreadState_t s;

  s.raw = _L4_abort_ipc_and_stop_sp_ip_flags (t.raw, sp, ip, flags);
  return s;
}


/* Support Functions.  */

/* L4_ThreadState_t defined above.  */

static inline L4_Bool_t
_L4_attribute_always_inline
L4_ThreadWasHalted (L4_ThreadState_t s)
{
  return s.raw & _L4_XCHG_REGS_HALTED;
}


static inline L4_Bool_t
_L4_attribute_always_inline
L4_ThreadWasReceiving (L4_ThreadState_t s)
{
  return s.raw & _L4_XCHG_REGS_RECEIVING;
}


static inline L4_Bool_t
_L4_attribute_always_inline
L4_ThreadWasSending (L4_ThreadState_t s)
{
  return s.raw & _L4_XCHG_REGS_SENDING;
}


static inline L4_Bool_t
_L4_attribute_always_inline
L4_ThreadWasIpcing (L4_ThreadState_t s)
{
  return s.raw & _L4_XCHG_REGS_IPCING;
}

/* L4_ErrorCode is defined above.  */

/* Error codes are defined in <l4/compat/syscall.h>.  */


/* 2.4 Thread Control [Privileged Systemcall]  */

/* Generic Programming Interface.  */

static inline L4_Word_t
_L4_attribute_always_inline
L4_ThreadControl (L4_ThreadId_t dest, L4_ThreadId_t SpaceSpecifier, L4_ThreadId_t Scheduler,
		  L4_ThreadId_t Pager, void *UtcbLocation)
{
  return _L4_thread_control (dest.raw, SpaceSpecifier.raw, Scheduler.raw, Pager.raw,
			     UtcbLocation);
}


/* Convenience Interface.  */

static inline L4_Word_t
_L4_attribute_always_inline
L4_AssociateInterrupt (L4_ThreadId_t InterruptThread, L4_ThreadId_t InterruptHandler)
{
  return _L4_associate_interrupt (InterruptThread.raw, InterruptHandler.raw);
}


static inline L4_Word_t
_L4_attribute_always_inline
L4_DeassociateInterrupt (L4_ThreadId_t InterruptThread)
{
  return _L4_deassociate_interrupt (InterruptThread.raw);
}

/* L4_ErrorCode is defined above.  */

/* Error codes are defined in <l4/compat/syscall.h>.  */
