/* l4/syscall.h - Public interface to the L4 system calls.
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
#define _L4_SYSCALL_H	1

#include <l4/features.h>
#include <l4/types.h>
#include <l4/vregs.h>

/* The system calls are defined by the architecture specific header file.  */
#include <l4/bits/syscall.h>


/* _L4_exchange_registers control argument.  */

/* Input.  */
#define _L4_XCHG_REGS_HALT		_L4_WORD_C(0x0001)
#define _L4_XCHG_REGS_CANCEL_RECV	_L4_WORD_C(0x0002)
#define _L4_XCHG_REGS_CANCEL_SEND	_L4_WORD_C(0x0004)
#define _L4_XCHG_REGS_CANCEL_IPC	(_L4_XCHG_REGS_CANCEL_RECV	\
					 | _L4_XCHG_REGS_CANCEL_SEND)
#define _L4_XCHG_REGS_SET_SP		_L4_WORD_C(0x0008)
#define _L4_XCHG_REGS_SET_IP		_L4_WORD_C(0x0010)
#define _L4_XCHG_REGS_SET_FLAGS		_L4_WORD_C(0x0020)
#define _L4_XCHG_REGS_SET_USER_HANDLE	_L4_WORD_C(0x0040)
#define _L4_XCHG_REGS_SET_PAGER		_L4_WORD_C(0x0080)
#define _L4_XCHG_REGS_SET_HALT		_L4_WORD_C(0x0100)
#define _L4_XCHG_REGS_DELIVER		_L4_WORD_C(0x0200)

/* Output.  */
#define _L4_XCHG_REGS_HALTED		_L4_WORD_C(0x01)
#define _L4_XCHG_REGS_RECEIVING		_L4_WORD_C(0x02)
#define _L4_XCHG_REGS_SENDING		_L4_WORD_C(0x04)
#define _L4_XCHG_REGS_IPCING		(_L4_XCHG_REGS_RECEIVING	\
					 | _L4_XCHG_REGS_SENDING)


/* _L4_schedule return codes.  */
#define _L4_SCHEDULE_ERROR		_L4_WORD_C(0)
#define _L4_SCHEDULE_DEAD		_L4_WORD_C(1)
#define _L4_SCHEDULE_INACTIVE		_L4_WORD_C(2)
#define _L4_SCHEDULE_RUNNING		_L4_WORD_C(3)
#define _L4_SCHEDULE_PENDING_SEND	_L4_WORD_C(4)
#define _L4_SCHEDULE_SENDING		_L4_WORD_C(5)
#define _L4_SCHEDULE_WAITING		_L4_WORD_C(6)
#define _L4_SCHEDULE_RECEIVING		_L4_WORD_C(7)


/* _L4_unmap flags.  */
#define _L4_UNMAP_FLUSH			_L4_WORD_C(0x40)
#define _L4_UNMAP_COUNT_MASK		_L4_WORD_C(0x3f)


/* IPC errors.  */
#define _L4_IPC_TIMEOUT			_L4_WORD_C(1)
#define _L4_IPC_NO_PARTNER		_L4_WORD_C(2)
#define _L4_IPC_CANCELED		_L4_WORD_C(3)
#define _L4_IPC_MSG_OVERFLOW		_L4_WORD_C(4)
#define _L4_IPC_XFER_TIMEOUT_INVOKER	_L4_WORD_C(5)
#define _L4_IPC_XFER_TIMEOUT_PARTNER	_L4_WORD_C(6)
#define _L4_IPC_ABORTED			_L4_WORD_C(7)


/* Error codes.  */
#define _L4_ERR_NO_PRIVILEGE		_L4_WORD_C(1)
#define _L4_ERR_INV_THREAD		_L4_WORD_C(2)
#define _L4_ERR_INV_SPACE		_L4_WORD_C(3)
#define _L4_ERR_INV_SCHEDULER		_L4_WORD_C(4)
#define _L4_ERR_INV_PARAM		_L4_WORD_C(5)
#define _L4_ERR_UTCB_AREA		_L4_WORD_C(6)
#define _L4_ERR_KIP_AREA		_L4_WORD_C(7)
#define _L4_ERR_NO_MEM			_L4_WORD_C(8)


static inline const char *
_L4_attribute_always_inline
_L4_strerror (_L4_word_t err_code)
{
  switch (err_code)
    {
    case _L4_ERR_NO_PRIVILEGE:
      return "no privilege";

    case _L4_ERR_INV_THREAD:
      return "invalid thread";

    case _L4_ERR_INV_SPACE:
      return "invalid space";

    case _L4_ERR_INV_PARAM:
      return "invalid parameter";

    case _L4_ERR_UTCB_AREA:
      return "invalid utcb area";

    case _L4_ERR_KIP_AREA:
      return "invalid kip area";

    case _L4_ERR_NO_MEM:
      return "out of memory";

    default:
      return "unknown error code";
    }
}


/* Now incorporate the public interfaces the user has selected.  */
#ifdef _L4_INTERFACE_L4
#include <l4/compat/syscall.h>
#endif
#ifdef _L4_INTERFACE_GNU
#include <l4/gnu/syscall.h>
#endif

#endif	/* l4/syscall.h */
