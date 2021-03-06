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

#ifndef _VIENGOOS_MISC_H
#define _VIENGOOS_MISC_H

#include <hurd/startup.h>
#include <viengoos/folio.h>
#include <viengoos/thread.h>
#include <viengoos/activity.h>
#include <viengoos/futex.h>
#include <l4/message.h>

enum rm_method_id
  {
    VG_write = 100,
    VG_read,
    VG_as_dump,
    VG_fault,
  };

static inline const char *
vg_method_id_string (int id)
{
  switch (id)
    {
    case VG_write:
      return "write";
    case VG_read:
      return "read";
    case VG_as_dump:
      return "as_dump";
    case VG_fault:
      return "fault";
    case VG_folio_alloc:
      return "folio_alloc";
    case VG_folio_free:
      return "folio_free";
    case VG_folio_object_alloc:
      return "folio_object_alloc";
    case VG_folio_policy:
      return "folio_policy";
    case VG_cap_copy:
      return "cap_copy";
    case VG_cap_rubout:
      return "cap_rubout";
    case VG_cap_read:
      return "cap_read";
    case VG_object_discarded_clear:
      return "object_discarded_clear";
    case VG_object_discard:
      return "object_discard";
    case VG_object_status:
      return "object_status";
    case VG_object_reply_on_destruction:
      return "object_reply_on_destruction";
    case VG_object_name:
      return "object_name";
    case VG_thread_exregs:
      return "thread_exregs";
    case VG_thread_id:
      return "thread_id";
    case VG_thread_activation_collect:
      return "thread_activation_collect";
    case VG_activity_policy:
      return "activity_policy";
    case VG_activity_info:
      return "activity_info";
    case VG_futex:
      return "futex";
    case VG_messenger_id:
      return "messenger id";
    default:
      return "unknown method id";
    }
}

#define RPC_STUB_PREFIX vg
#define RPC_ID_PREFIX VG

#include <viengoos/rpc.h>

struct io_buffer
{
  /* The length.  */
  unsigned char len;
  char data[(L4_NUM_BRS - 2) * sizeof (uintptr_t)];
};

/* Echo the character CHR on the manager console.  */
RPC(write, 1, 0, 0, struct io_buffer, io)

/* Read up to MAX characters from the console's input device.  */
RPC(read, 1, 1, 0,
    int, max, struct io_buffer, io)

/* Dump the address space rooted at ROOT.  */
RPC(as_dump, 0, 0, 0,
    /* cap_t, principal, cap_t, object */)

/* Fault up to COUNT pages starting at START.  Returns the number
   actually faulted in OCOUNT.  */
RPC(fault, 2, 1, 0,
    /* cap_t, principal, cap_t thread, */
    uintptr_t, start, int, count,
    /* Out: */
    int, ocount)

#undef RPC_STUB_PREFIX
#undef RPC_ID_PREFIX

#endif
