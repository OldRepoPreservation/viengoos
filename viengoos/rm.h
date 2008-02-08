/* rm.h - Resource manager interface.
   Copyright (C) 2007, 2008 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 3 of the
   License, or (at your option) any later version.

   The GNU Hurd is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see
   <http://www.gnu.org/licenses/>.  */

#ifndef RM_RM_H
#define RM_RM_H

#include <hurd/startup.h>
#include <hurd/folio.h>
#include <hurd/exceptions.h>
#include <hurd/thread.h>
#include <hurd/activity.h>
#include <hurd/futex.h>

enum rm_method_id
  {
    RM_putchar = 100,
    RM_as_dump,
  };

static inline const char *
rm_method_id_string (int id)
{
  switch (id)
    {
    case RM_putchar:
      return "putchar";
    case RM_as_dump:
      return "as_dump";
    case RM_folio_alloc:
      return "folio_alloc";
    case RM_folio_free:
      return "folio_free";
    case RM_folio_object_alloc:
      return "folio_object_alloc";
    case RM_cap_copy:
      return "cap_copy";
    case RM_cap_read:
      return "cap_read";
    case RM_object_slot_copy_out:
      return "object_slot_copy_out";
    case RM_object_slot_copy_in:
      return "object_slot_copy_in";
    case RM_object_slot_read:
      return "object_slot_read";
    case RM_exception_collect:
      return "exception_collect";
    case RM_thread_exregs:
      return "thread_exregs";
    case RM_thread_wait_object_destroyed:
      return "thread_wait_object_destroyed";
    case RM_activity_policy:
      return "activity_policy";
    case RM_futex:
      return "futex";
    default:
      return "unknown method id";
    }
}

#define RPC_STUB_PREFIX rm
#define RPC_ID_PREFIX RM
#undef RPC_TARGET_NEED_ARG
#define RPC_TARGET \
  ({ \
    extern struct hurd_startup_data *__hurd_startup_data; \
    __hurd_startup_data->rm; \
  })

#include <hurd/rpc.h>

/* Echo the character CHR on the manager console.  */
RPC_SIMPLE(putchar, 1, 0, int, chr)

/* Dump the address space rooted at ROOT.  */
RPC(as_dump, 2, 0, addr_t, principal, addr_t, root)

#endif
