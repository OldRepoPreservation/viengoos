/* exceptions.h - Exception handling definitions.
   Copyright (C) 2007, 2008 Free Software Foundation, Inc.
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

#include <stdint.h>
#include <hurd/cap.h>
#include <hurd/thread.h>
#include <l4/thread.h>
#include <hurd/error.h>

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
      uintptr_t access: 3;
      /* Type of object that was attempting to be accessed.  */
      uintptr_t type : CAP_TYPE_BITS;
      /* Whether the page was discarded.  */
      uintptr_t discarded : 1;
    };
    uintptr_t raw;
  };
};

/* Raise a fault at address FAULT_ADDRESS.  If IP is not 0, then IP is
   the value of the IP of the faulting thread at the time of the fault
   and SP the value of the stack pointer at the time of the fault.  */
RPC (fault, 4, 0, addr_t, fault_address, uintptr_t, sp, uintptr_t, ip,
     struct exception_info, exception_info)

#undef RPC_STUB_PREFIX
#undef RPC_ID_PREFIX
#undef RPC_TARGET_NEED_ARG
#undef RPC_TARGET_ARG_TYPE
#undef RPC_TARGET

#define RPC_STUB_PREFIX rm
#define RPC_ID_PREFIX RM
#undef RPC_TARGET_NEED_ARG
#define RPC_TARGET \
  ({ \
    extern struct hurd_startup_data *__hurd_startup_data; \
    __hurd_startup_data->rm; \
  })

#include <hurd/rpc.h>

/* Exception message ids.  */
enum
  {
    RM_exception_collect = 500,
  };

/* Cause the delivery of a pending event, if any.  */
RPC(exception_collect, 1, 0, addr_t, principal)

#undef RPC_STUB_PREFIX
#undef RPC_ID_PREFIX
#undef RPC_TARGET

/* Initialize the exception handler.  */
extern void exception_handler_init (void);


/* When a thread causes an exception, the kernel invokes the thread's
   exception handler.  This points to the low-level exception handler,
   which invokes exception_handler_activated.  (It is passed a pointer
   to the exception page.)

   This function must determine how to continue.  It may, but need
   not, immediately handle the fault.  The problem with handling the
   fault immediately is that this function runs on the exception
   handler's tiny stack (~3kb) and it runs in activated mode.  The
   latter means that it may not fault (which generally precludes
   accessing any dynamically allocated storage).  To allow an easy
   transition to another function in normal-mode, if the function
   returns an exception_frame, then the exception handler will call
   exception_handler_normal passing it that argument.  This function
   runs in normal mode and on the normal stack.  When this function
   returns, the interrupted state is restored.  */
extern struct exception_frame *
  exception_handler_activated (struct exception_page *exception_page);

extern void exception_handler_normal (struct exception_frame *exception_frame);

/* Should be called before destroyed the exception page associated
   with a thread.  */
extern void exception_page_cleanup (struct exception_page *exception_page);

/* The first instruction of exception handler dispatcher.  */
extern char exception_handler_entry;
/* The instruction immediately following the last instruction of the
   exception handler dispatcher.  */
extern char exception_handler_end;

#endif
