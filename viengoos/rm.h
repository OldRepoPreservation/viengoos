/* rm.h - Resource manager interface.
   Copyright (C) 2007 Free Software Foundation, Inc.
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

#include <assert.h>
#include <l4.h>
#include <errno.h>

#include <hurd/types.h>
#include <hurd/addr.h>
#include <hurd/addr-trans.h>
#include <hurd/startup.h>

#define RPC_STUB_PREFIX rm
#define RPC_ID_PREFIX RM
#undef RPC_TARGET_NEED_ARG
#define RPC_TARGET \
  ({ \
    extern struct hurd_startup_data *__hurd_startup_data; \
    __hurd_startup_data->rm; \
  })

#include <hurd/rpc.h>

enum rm_method_id
  {
    RM_putchar = 100,

    RM_folio_alloc = 200,
    RM_folio_free,
    RM_folio_object_alloc,

    RM_cap_copy = 300,
    RM_cap_read,

    RM_object_slot_copy_out = 400,
    RM_object_slot_copy_in,
    RM_object_slot_read,
  };

static inline const char *
rm_method_id_string (enum rm_method_id id)
{
  switch (id)
    {
    case RM_putchar:
      return "putchar";
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
    default:
      return "unknown method id";
    }
}

/* Echo the character CHR on the manager console.  */
static inline void
__attribute__((always_inline))
rm_putchar (int chr)
{
  extern struct hurd_startup_data *__hurd_startup_data;

  l4_msg_tag_t tag;

  l4_accept (L4_UNTYPED_WORDS_ACCEPTOR);

  tag = l4_niltag;
  l4_msg_tag_set_label (&tag, RM_putchar);
  l4_msg_tag_set_untyped_words (&tag, 1);
  l4_msg_tag_set_typed_words (&tag, 0);
  l4_set_msg_tag (tag);
  l4_load_mr (1, (l4_word_t) chr);
  /* XXX: We should send data to the log server.  */
  tag = l4_send (__hurd_startup_data->rm);
}

/* Allocate a folio against PRINCIPAL.  Store a capability in
   the caller's cspace in slot FOLIO.  */
RPC2(folio_alloc, addr_t, principal, addr_t, folio)
  
/* Free the folio designated by FOLIO.  PRINCIPAL pays.  */
RPC2(folio_free, addr_t, principal, addr_t, folio)

/* Allocate INDEXth object in folio FOLIO as an object of type TYPE.
   PRINCIPAL is charged.  If OBJECT_SLOT is not ADDR_VOID, then stores
   a capability to the allocated object in OBJECT_SLOT.  */
RPC5(folio_object_alloc, addr_t, principal,
     addr_t, folio, l4_word_t, index, l4_word_t, type, addr_t, object_slot)

enum
{
  /* Use subpage in CAP_ADDR_TRANS (must be a subset of subpage in
     SOURCE).  */
  CAP_COPY_COPY_SUBPAGE = 1 << 0,
  /* Use guard in TARGET, not the guard in CAP_ADDR_TRANS.  */
  CAP_COPY_COPY_GUARD = 1 << 1,
};

#define THREAD_ASPACE_SLOT 0
#define THREAD_ACTIVITY_SLOT 1

/* Copy capability SOURCE to the capability slot TARGET.
   ADDR_TRANS_FLAGS is a subset of CAP_COPY_GUARD, CAP_COPY_SUBPAGE,
   and CAP_COPY_PRESERVE_GUARD, bitwise-ored.  If CAP_COPY_GUARD is
   set, the guard descriptor in CAP_ADDR_TRANS is used, if
   CAP_COPY_PRESERVE_GUARD, the guard descriptor in TARGET, otherwise,
   the guard descriptor is copied from SOURCE.  If CAP_COPY_SUBPAGE is
   set, the subpage descriptor in CAP_ADDR_TRANS is used, otherwise,
   the subpage descriptor is copied from SOURCE.  */
RPC5(cap_copy, addr_t, principal, addr_t, target, addr_t, source,
     l4_word_t, addr_trans_flags, struct cap_addr_trans, cap_addr_trans)

/* Store the public bits of the capability CAP in *TYPE and
   *CAP_ADDR_TRANS.  */
RPC22(cap_read, addr_t, principal, addr_t, cap,
      l4_word_t *, type, struct cap_addr_trans *, cap_addr_trans)

/* Copy the capability from slot SLOT of the object OBJECT (relative
   to the start of the object's subpage) to slot TARGET.  */
RPC6(object_slot_copy_out, addr_t, principal,
     addr_t, object, l4_word_t, slot, addr_t, target,
     l4_word_t, flags, struct cap_addr_trans, cap_addr_trans)

/* Copy the capability from slot SOURCE to slot INDEX of the object
   OBJECT (relative to the start of the object's subpage).  */
RPC6(object_slot_copy_in, addr_t, principal,
     addr_t, object, l4_word_t, index, addr_t, source,
     l4_word_t, flags, struct cap_addr_trans, cap_addr_trans)

/* Store the public bits of the capability slot SLOT of object
   OBJECT in *TYPE and *CAP_ADDR.  */
RPC32(object_slot_read, addr_t, principal,
      addr_t, object, l4_word_t, slot,
      l4_word_t *, type, struct cap_addr_trans *, cap_addr_trans)

#undef RPC_STUB_PREFIX
#undef RPC_ID_PREFIX
#undef RPC_TARGET

#endif
