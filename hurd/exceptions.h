/* exceptions.h - Exception handling definitions.
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

#ifndef _HURD_EXCEPTIONS_H
#define _HURD_EXCEPTIONS_H 1

#include <hurd/cap.h>
#include <l4/thread.h>
#include <errno.h>

#define RPC_STUB_PREFIX exception
#define RPC_ID_PREFIX EXCEPTION
#define RPC_TARGET_NEED_ARG
#define RPC_TARGET_ARG_TYPE l4_thread_id_t
#define RPC_TARGET(x) (x)
#include <hurd/rpc.h>

/* Exception message ids.  */
enum
  {
    EXCEPTION_fault = 10,
  };

/* Return a string corresponding to a message id.  */
static inline const char *
exception_method_id_string (l4_word_t id)
{
  switch (id)
    {
    case EXCEPTION_fault:
      return "fault";
    default:
      return "unknown";
    }
}

struct exception_info
{
  union
  {
    struct
    {
      /* Type of access.  */
      l4_word_t access: 3;
      /* Type of object that was attempting to be accessed.  */
      l4_word_t type : CAP_TYPE_BITS;
    };
    l4_word_t raw;
  };
};

/* Raise a fault at address FAULT_ADDRESS.  If IP is not 0, then IP is
   the value of the IP of the faulting thread at the time of the
   fault.  */
RPC (fault, 3, 0, addr_t, fault_address, uintptr_t, ip,
     struct exception_info, exception_info)

#undef RPC_STUB_PREFIX
#undef RPC_ID_PREFIX
#undef RPC_TARGET_NEED_ARG
#undef RPC_TARGET_ARG_TYPE
#undef RPC_TARGET

/* Initialize the exception handler.  */
extern void exception_handler_init (void);

/* Handle an exception.  EXCEPTION_PAGE is the thread's exception
   page.  */
extern void exception_handler (struct exception_page *exception_page);


/* The first instruction of exception handler dispatcher.  */
extern char exception_handler_entry;
/* The instruction immediately following the last instruction of the
   exception handler dispatcher.  */
extern char exception_handler_end;

#endif
