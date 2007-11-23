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

/* Each thread object actually consists of two L4 threads.  The first
   is the main thread and the second is the exception thread.
   Exceptions raised by the first thread are forwarded to the
   exception thread.  Signals may also be sent to the exception
   thread.  If the exception thread faults, the game is over.  So, the
   exception thread should do its best to get the main thread into a
   consistent state and then hand off any message.

   The exception thread should sit in the following loop:

     while (1)
       {
         l4_call (rm);

         // Handle exception.
       }

   The server will only respond when there is a messages waiting.
   This allows the thread to pick up any messages it may have missed.  */


#define HURD_THREAD_MAIN_VERSION	2
#define HURD_THREAD_EXCEPTION_VERSION	3

/* Return whether TID is an exception thread.  */
static inline bool
hurd_thread_is_exception_thread (l4_thread_id_t tid)
{
  return l4_version (tid) == HURD_THREAD_EXCEPTION_VERSION;
}

/* Return whether TID is a main thread.  */
static inline bool
hurd_thread_is_main_thread (l4_thread_id_t tid)
{
  return l4_version (tid) == HURD_THREAD_MAIN_VERSION;
}

/* Return the thread id of the exception thread associated with thread
   THREAD.  */
static inline l4_thread_id_t
hurd_exception_thread (l4_thread_id_t tid)
{
  if (hurd_thread_is_main_thread (tid))
    return l4_global_id (l4_thread_no (tid) + 1,
			 HURD_THREAD_EXCEPTION_VERSION);
  else
    return tid;
}

/* Return the thread id of the exception thread associated with thread
   THREAD.  */
static inline l4_thread_id_t
hurd_main_thread (l4_thread_id_t tid)
{
  if (hurd_thread_is_exception_thread (tid))
    return l4_global_id (l4_thread_no (tid) - 1, HURD_THREAD_MAIN_VERSION);
  else
    return tid;
}

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
RPC3 (fault, addr_t, fault_address, uintptr_t, ip,
      struct exception_info, exception_info)

#undef RPC_STUB_PREFIX
#undef RPC_ID_PREFIX
#undef RPC_TARGET_NEED_ARG
#undef RPC_TARGET_ARG_TYPE
#undef RPC_TARGET

/* Initialize the exception handler.  */
extern void exception_handler_init (void);

/* The exception handler loop.  */
extern void exception_handler_loop (void);

#endif
