/* l4/gnu/syscall.h - Public GNU interface for L4 system calls.
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

#ifndef _L4_SYSCALL_H
# error "Never use <l4/gnu/syscall.h> directly; include <l4/syscall.h> instead."
#endif


/* _L4_exchange_registers control argument.  */

/* Input.  */
#define L4_XCHG_REGS_HALT		_L4_XCHG_REGS_HALT
#define L4_XCHG_REGS_CANCEL_RECV	_L4_XCHG_REGS_CANCEL_RECV
#define L4_XCHG_REGS_CANCEL_SEND	_L4_XCHG_REGS_CANCEL_SEND
#define L4_XCHG_REGS_CANCEL_IPC		_L4_XCHG_REGS_CANCEL_IPC
#define L4_XCHG_REGS_SET_SP		_L4_XCHG_REGS_SET_SP
#define L4_XCHG_REGS_SET_IP		_L4_XCHG_REGS_SET_IP
#define L4_XCHG_REGS_SET_FLAGS		_L4_XCHG_REGS_SET_FLAGS
#define L4_XCHG_REGS_SET_USER_HANDLE	_L4_XCHG_REGS_SET_USER_HANDLE
#define L4_XCHG_REGS_SET_PAGER		_L4_XCHG_REGS_SET_PAGER
#define L4_XCHG_REGS_SET_HALT		_L4_XCHG_REGS_SET_HALT

/* Output.  */
#define L4_XCHG_REGS_HALTED		_L4_XCHG_REGS_HALTED
#define L4_XCHG_REGS_RECEIVING		_L4_XCHG_REGS_RECEIVING
#define L4_XCHG_REGS_SENDING		_L4_XCHG_REGS_SENDING
#define L4_XCHG_REGS_IPCING		_L4_XCHG_REGS_IPCING


/* _L4_schedule return codes.  */
#define L4_SCHEDULE_ERROR		_L4_SCHEDULE_ERROR
#define L4_SCHEDULE_DEAD		_L4_SCHEDULE_DEAD
#define L4_SCHEDULE_INACTIVE		_L4_SCHEDULE_INACTIVE
#define L4_SCHEDULE_RUNNING		_L4_SCHEDULE_RUNNING
#define L4_SCHEDULE_PENDING_SEND	_L4_SCHEDULE_PENDING_SEND
#define L4_SCHEDULE_SENDING		_L4_SCHEDULE_SENDING
#define L4_SCHEDULE_WAITING		_L4_SCHEDULE_WAITING
#define L4_SCHEDULE_RECEIVING		_L4_SCHEDULE_RECEIVING


/* _L4_unmap flags.  */
#define L4_UNMAP_FLUSH			_L4_UNMAP_FLUSH
#define L4_UNMAP_COUNT_MASK		_L4_UNMAP_COUNT_MASK


#define L4_ERR_NO_PRIVILEGE		_L4_ERR_NO_PRIVILEGE
#define L4_ERR_INV_THREAD		_L4_ERR_INV_THREAD
#define L4_ERR_INV_SPACE		_L4_ERR_INV_SPACE
#define L4_ERR_INV_SCHEDULER		_L4_ERR_INV_SCHEDULER
#define L4_ERR_INV_PARAM		_L4_ERR_INV_PARAM
#define L4_ERR_UTCB_AREA		_L4_ERR_UTCB_AREA
#define L4_ERR_KIP_AREA			_L4_ERR_KIP_AREA
#define L4_ERR_NO_MEM			_L4_ERR_NO_MEM

static inline const char *const
_L4_attribute_always_inline
l4_strerror (l4_word_t err_code)
{
  return _L4_strerror (err_code);
}
