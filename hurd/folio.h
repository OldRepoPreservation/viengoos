/* folio.h - Folio definitions.
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

#ifndef _HURD_FOLIO_H
#define _HURD_FOLIO_H 1

#include <hurd/types.h>
#include <hurd/addr.h>
#include <hurd/startup.h>
#include <stdint.h>

/* Number of user objects per folio.  */
enum
  {
    FOLIO_OBJECTS = 128,
  };
enum
  {
    FOLIO_OBJECTS_LOG2 = 7,
  };

/* The format of the first page of a folio.  This page is followed (on
   disk) by FOLIO_OBJECTS pages.  */
struct folio
{
#ifdef RM_INTERN
  /* Folios are the unit of storage accounting.  Every folio belongs
     to exactly one activity.  To track what folios belong to a
     particular activity, each folio is attached to a doubly-linked
     list originating at its owner activity.  */
  struct cap activity;
  struct cap next;
  struct cap prev;

  /* The folio's version.  */
  l4_word_t folio_version;

  struct
  {
    /* Each object in the folio Disk version of each object.  */
    l4_uint32_t version : CAP_VERSION_BITS;

    /* The type Page type.  */
    l4_uint32_t type : CAP_TYPE_BITS;

    /* Whether a page is has any content (i.e., if it is not
       uninitialized).  */
    l4_uint32_t content : 1;

    /* Whether a page is discardable (if so and the page is not zero,
       trying to read the page from disk generates a first fault
       fault).  */
    l4_uint32_t discardable : 1;

    /* We only need to bump the object's version when we can't easily
       reclaim all references.  If there are no references or if the
       only references are in memory and thus easy to find, we can just
       destroy them.  */
    /* Where a capability pointing to this object is written to disk, we
       set the hazard bit.  If it is clear, then we know that references
       (if there are any) are only in-memory.  */
    l4_uint32_t dhazard : 1;

    /* In memory only.  */

    /* If clear, we know that references (if there are any)
       are only in-memory.  */
    l4_uint32_t mhazard : 1;

    /* 128-bit md5sum of each object.  */
    l4_uint64_t checksum[2];

    /* In memory only.  */

    /* The in-memory version.  */
    l4_word_t mversion;
  } objects[FOLIO_OBJECTS];
#else
  /* User-space folio.  */
  struct cap objects[FOLIO_OBJECTS];
#endif
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

enum
  {
    RM_folio_alloc = 200,
    RM_folio_free,
    RM_folio_object_alloc,
  };

/* Allocate a folio against PRINCIPAL.  Store a capability in
   the caller's cspace in slot FOLIO.  */
RPC(folio_alloc, 2, 0, addr_t, principal, addr_t, folio)
  
/* Free the folio designated by FOLIO.  PRINCIPAL pays.  */
RPC(folio_free, 2, 0, addr_t, principal, addr_t, folio)

/* Allocate INDEXth object in folio FOLIO as an object of type TYPE.
   PRINCIPAL is charged.  If OBJECT_SLOT is not ADDR_VOID, then stores
   a capability to the allocated object in OBJECT_SLOT.  If
   OBJECT_WEAK_SLOT is not ADDR_VOID, stores a weaken reference to the
   created object.  */
RPC(folio_object_alloc, 6, 0, addr_t, principal,
    addr_t, folio, l4_word_t, index, l4_word_t, type,
    addr_t, object_slot, addr_t, object_weak_slot)

#undef RPC_STUB_PREFIX
#undef RPC_ID_PREFIX
#undef RPC_TARGET

#endif
