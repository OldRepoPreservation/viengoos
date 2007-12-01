/* thread.h - Thread definitions.
   Copyright (C) 2007 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU Hurd is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#ifndef _HURD_THREAD_H
#define _HURD_THREAD_H 1

#include <hurd/types.h>
#include <hurd/startup.h>
#include <hurd/addr-trans.h>
#include <l4/syscall.h>

enum
  {
    RM_thread_exregs = 600,
  };

enum
  {
    THREAD_ASPACE_SLOT = 0,
    THREAD_ACTIVITY_SLOT = 1,
  };

enum
{
  HURD_EXREGS_EXCEPTION_THREAD = 0x1000,

  HURD_EXREGS_SET_ASPACE = 0x800,
  HURD_EXREGS_SET_ACTIVITY = 0x400,
  HURD_EXREGS_SET_SP = _L4_XCHG_REGS_SET_SP,
  HURD_EXREGS_SET_IP = _L4_XCHG_REGS_SET_IP,
  HURD_EXREGS_SET_SP_IP = _L4_XCHG_REGS_SET_SP | _L4_XCHG_REGS_SET_IP,
  HURD_EXREGS_SET_EFLAGS = _L4_XCHG_REGS_SET_FLAGS,
  HURD_EXREGS_SET_USER_HANDLE = _L4_XCHG_REGS_SET_USER_HANDLE,
  HURD_EXREGS_SET_REGS = (HURD_EXREGS_SET_ASPACE
			  | HURD_EXREGS_SET_ACTIVITY
			  | HURD_EXREGS_SET_SP
			  | HURD_EXREGS_SET_IP
			  | HURD_EXREGS_SET_EFLAGS
			  | HURD_EXREGS_SET_USER_HANDLE),

  HURD_EXREGS_GET_REGS = _L4_XCHG_REGS_DELIVER,

  HURD_EXREGS_START = _L4_XCHG_REGS_SET_HALT,
  HURD_EXREGS_STOP = _L4_XCHG_REGS_SET_HALT | _L4_XCHG_REGS_HALT,

  HURD_EXREGS_ABORT_SEND = _L4_XCHG_REGS_CANCEL_SEND,
  HURD_EXREGS_ABORT_RECEIVE = _L4_XCHG_REGS_CANCEL_RECV,
  HURD_EXREGS_ABORT_IPC = HURD_EXREGS_ABORT_SEND | _L4_XCHG_REGS_CANCEL_RECV,
};

#define RPC_STUB_PREFIX rm
#define RPC_ID_PREFIX RM
#undef RPC_TARGET_NEED_ARG
#define RPC_TARGET \
  ({ \
    extern struct hurd_startup_data *__hurd_startup_data; \
    __hurd_startup_data->rm; \
  })

#include <hurd/rpc.h>

/* l4_exregs wrapper.  */
RPC (thread_exregs, 13, 4, addr_t, principal, addr_t, thread,
     l4_word_t, control,
     addr_t, aspace, l4_word_t, flags, struct cap_addr_trans, aspace_trans,
     addr_t, activity,
     l4_word_t, sp, l4_word_t, ip, l4_word_t, eflags,
     l4_word_t, user_handler, 
     addr_t, aspace_out, addr_t, activity_out,
     l4_word_t *, sp_out, l4_word_t *, ip_out, l4_word_t *, eflags_out,
     l4_word_t *, user_handler_out)
     
#undef RPC_STUB_PREFIX
#undef RPC_ID_PREFIX
#undef RPC_TARGET

static inline error_t
thread_stop (addr_t thread)
{
  l4_word_t dummy = 0;
  return rm_thread_exregs (ADDR_VOID, thread,
			   HURD_EXREGS_STOP | HURD_EXREGS_ABORT_IPC,
			   ADDR_VOID, 0, CAP_ADDR_TRANS_VOID, ADDR_VOID,
			   0, 0, 0, 0,
			   ADDR_VOID, ADDR_VOID,
			   &dummy, &dummy, &dummy, &dummy);
}

#endif
