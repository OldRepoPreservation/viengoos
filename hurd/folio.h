/* folio.h - Folio definitions.
   Copyright (C) 2007, 2008 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

   This file is part of the GNU Hurd.

   GNU Hurd is free software: you can redistribute it and/or modify it
   under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   GNU Hurd is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with GNU Hurd.  If not, see
   <http://www.gnu.org/licenses/>.  */

#ifndef _HURD_FOLIO_H
#define _HURD_FOLIO_H 1

#include <hurd/types.h>
#include <hurd/addr.h>
#include <hurd/cap.h>
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

/* User settable folio policy.  */

/* The range of valid folio priorities.  A lower numerical value
   corresponds to a lower priority.  */
#define FOLIO_PRIORITY_BITS 15
#define FOLIO_PRIORITY_MIN (-(1 << (FOLIO_PRIORITY_BITS - 1)))
#define FOLIO_PRIORITY_LRU (0)
#define FOLIO_PRIORITY_MAX ((1 << (FOLIO_PRIORITY_BITS - 1)) - 1)

/* The folio group range.  */
#define FOLIO_GROUP_BITS 15
#define FOLIO_GROUP_NONE 0
#define FOLIO_GROUP_MIN 0
#define FOLIO_GROUP_MAX ((1 << FOLIO_BITS) - 1)

struct folio_policy
{
  union
  {
    struct
    {
      /* Whether a folio is discardable.  If an activity reaches it
	 quota, rather than returning an out of memory error, the
	 system may reclaim storage with the discardable bit set.  It
	 performs the equivalent of calling folio_free on the
	 folio.  */
      int32_t discardable : 1;

      /* The following are only used if DISCARABLE is true.  */

      /* Folios can belong to a group.  When one folio is discarded,
	 all folios in that group are discarded, unless GROUP is
	 FOLIO_GROUP_NONE.  */
      uint32_t group : FOLIO_GROUP_BITS;

      /* By default, the system tries to discard folios according to
	 an LRU policy.  This can be overridden using this field.  In
	 this case, folios from the lowest priority group are
	 discarded.  */
      int32_t priority : FOLIO_PRIORITY_BITS;
    };
    uint32_t raw;
  };
};

#define FOLIO_POLICY_INIT { { raw: 0 } }
#define FOLIO_POLICY_VOID (struct folio_policy) FOLIO_POLICY_INIT
/* The default policy is not discardable.  */
#define FOLIO_POLICY_DEFAULT FOLIO_POLICY_VOID

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

  struct folio_policy policy;

  struct cap wait_queue;

  struct
  {
    /* Each object in the folio Disk version of each object.  */
    l4_uint32_t version : CAP_VERSION_BITS;

    /* The type.  */
    l4_uint32_t type : CAP_TYPE_BITS;

    /* Whether a page has any content (i.e., if it is not
       uninitialized).  */
    l4_uint32_t content : 1;

    /* We only need to bump the object's version when we can't easily
       reclaim all references.  If there are no references or if the
       only references are in memory and thus easy to find, we can just
       destroy them.  */
    /* Where a capability pointing to this object is written to disk, we
       set the hazard bit.  If it is clear, then we know that references
       (if there are any) are only in-memory.  */
    l4_uint32_t dhazard : 1;

    /* In memory only.  If clear, we know that references (if there
       are any) are only in-memory.  */
    l4_uint32_t mhazard : 1;

    struct object_policy policy;

    /* List of objects waiting for some event on this object.  The
       list is a circular list.  HEAD->PREV points to the tail.
       TAIL->NEXT points to the OBJECT (NOT HEAD).  */
    struct cap wait_queue;

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
    RM_folio_policy
  };

/* Allocate a folio against PRINCIPAL.  Store a capability in the
   caller's cspace in slot FOLIO.  POLICY specifies the storage
   policy.  */
RPC(folio_alloc, 3, 0, addr_t, principal, addr_t, folio,
    struct folio_policy, policy)
  
/* Free the folio designated by FOLIO.  PRINCIPAL pays.  */
RPC(folio_free, 2, 0, addr_t, principal, addr_t, folio)

/* Allocate INDEXth object in folio FOLIO as an object of type TYPE.
   POLICY specifies the object's policy when accessed via the folio.
   If OBJECT_SLOT is not ADDR_VOID, then stores a capability to the
   allocated object in OBJECT_SLOT.  If OBJECT_WEAK_SLOT is not
   ADDR_VOID, stores a weaken reference to the created object.  */
RPC(folio_object_alloc, 7, 0, addr_t, principal,
    addr_t, folio, l4_word_t, index, l4_word_t, type,
    struct object_policy, policy,
    addr_t, object_slot, addr_t, object_weak_slot)

/* Flags for folio_policy.  */
enum
{
  FOLIO_POLICY_DELIVER = 1 << 0,

  FOLIO_POLICY_DISCARDABLE_SET = 1 << 1,
  FOLIO_POLICY_GROUP_SET = 1 << 2,
  FOLIO_POLICY_PRIORITY_SET = 1 << 3,

  FOLIO_POLICY_SET = (FOLIO_POLICY_DISCARDABLE_SET
		      | FOLIO_POLICY_GROUP_SET
		      | FOLIO_POLICY_PRIORITY_SET)
};

/* Get and set the management policy for folio FOLIO.

   If FOLIO_POLICY_DELIVER is set in FLAGS, then return FOLIO's
   current paging policy in VALUE.  Then, if any of the set flags are
   set, set the corresponding values based on the value of POLICY.  */
RPC(folio_policy, 4, 1,
    addr_t, principal, addr_t, folio,
    l4_word_t, flags, struct folio_policy, policy,
    /* Out: */
    struct folio_policy, value)

#undef RPC_STUB_PREFIX
#undef RPC_ID_PREFIX
#undef RPC_TARGET

#endif
