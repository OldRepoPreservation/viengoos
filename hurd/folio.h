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

  /* The storage policy.  */
  struct folio_policy policy;

  struct
  {
    /* Each object in the folio Disk version of each object.  */
    uint32_t version : CAP_VERSION_BITS;

    /* Whether a page has any content (i.e., if it is not
       uninitialized).  */
    uint32_t content : 1;

    /* The object's memory policy when accessed via the folio.  */
    uint32_t discardable : 1;
    int32_t priority : OBJECT_PRIORITY_BITS;
  } misc[1 + FOLIO_OBJECTS];

  /* The type.  */
  uint8_t types[FOLIO_OBJECTS];

  /* Bit array indicating whether the an object has a non-empty wait
     queue.  */
  uint8_t wait_queues_p[(1 + FOLIO_OBJECTS + 8) / 8];

  /* Head of the list of objects waiting for some event on this
     object.  An element of this array is only valid if the
     corresponding element of WAIT_QUEUES_P is true.  The list is a
     circular list.  HEAD->PREV points to the tail.  TAIL->NEXT points
     to the OBJECT (NOT HEAD).  */
  oid_t wait_queues[1 + FOLIO_OBJECTS];

  uint64_t checksums[1 + FOLIO_OBJECTS][2];
#else
  /* User-space folio.  */
  struct cap objects[FOLIO_OBJECTS];
#endif
};

#ifdef RM_INTERN
typedef struct folio *folio_t;
#else
typedef addr_t folio_t;
#endif

/* OBJECT is from -1 to FOLIO_OBJECTS.  */
static inline enum cap_type
folio_object_type (struct folio *folio, int object)
{
#ifdef RM_INTERN
  assert (object >= -1 && object <= FOLIO_OBJECTS);

  if (object == -1)
    return cap_folio;
  return folio->types[object];
#else
  assert (object >= 0 && object <= FOLIO_OBJECTS);
  return folio->objects[object].type;
#endif
}

static inline void
folio_object_type_set (struct folio *folio, int object, enum cap_type type)
{
  assert (object >= 0 && object <= FOLIO_OBJECTS);

#ifdef RM_INTERN
  folio->types[object] = type;
#else
  folio->objects[object].type = type;
#endif
}

static inline struct object_policy
folio_object_policy (struct folio *folio, int object)
{
  struct object_policy policy;

#ifdef RM_INTERN
  assert (object >= -1 && object <= FOLIO_OBJECTS);

  policy.discardable = folio->misc[object + 1].discardable;
  policy.priority = folio->misc[object + 1].priority;
#else
  assert (object >= 0 && object <= FOLIO_OBJECTS);

  policy.discardable = folio->objects[object].discardable;
  policy.priority = folio->objects[object].priority;
#endif

  return policy;
}

static inline void
folio_object_policy_set (struct folio *folio, int object,
			 struct object_policy policy)
{
#ifdef RM_INTERN
  assert (object >= -1 && object <= FOLIO_OBJECTS);

  folio->misc[object + 1].discardable = policy.discardable;
  folio->misc[object + 1].priority = policy.priority;
#else
  assert (object >= 0 && object <= FOLIO_OBJECTS);

  folio->objects[object].discardable = policy.discardable;
  folio->objects[object].priority = policy.priority;
#endif
}

#ifdef RM_INTERN
#include <bit-array.h>

static inline bool
folio_object_wait_queue_p (struct folio *folio, int object)
{
  assert (object >= -1 && object <= FOLIO_OBJECTS);

  return bit_test (folio->wait_queues_p, object);
}

static inline void
folio_object_wait_queue_p_set (struct folio *folio, int object,
			       bool valid)
{
  assert (object >= -1 && object <= FOLIO_OBJECTS);

  bit_set_to (folio->wait_queues_p, sizeof (folio->wait_queues_p),
	      object, valid);
}

static inline oid_t
folio_object_wait_queue (struct folio *folio, int object)
{
  assert (object >= -1 && object <= FOLIO_OBJECTS);

  return folio->wait_queues[object + 1];
}

static inline void
folio_object_wait_queue_set (struct folio *folio, int object,
			     oid_t head)
{
  assert (object >= -1 && object <= FOLIO_OBJECTS);

  folio->wait_queues[object + 1] = head;
}

static inline uint32_t
folio_object_version (struct folio *folio, int object)
{
  assert (object >= -1 && object <= FOLIO_OBJECTS);

  return folio->misc[object + 1].version;
}

static inline void
folio_object_version_set (struct folio *folio, int object,
			  uint32_t version)
{
  assert (object >= -1 && object <= FOLIO_OBJECTS);

  folio->misc[object + 1].version = version;
}

static inline bool
folio_object_content (struct folio *folio, int object)
{
  assert (object >= -1 && object <= FOLIO_OBJECTS);

  return folio->misc[object + 1].content;
}

static inline void
folio_object_content_set (struct folio *folio, int object,
			  bool content)
{
  assert (object >= -1 && object <= FOLIO_OBJECTS);

  folio->misc[object + 1].content = content;
}
#endif /* RM_INTERN  */

/* Return a cap designating folio FOLIO's OBJECT'th object.  */
#ifdef RM_INTERN
/* This needs to be a macro as we use object_to_object_desc which is
   made available by object.h but object.h includes this file.  */
#define folio_object_cap(__foc_folio, __foc_object)			\
  ({									\
    struct cap __foc_cap;						\
									\
    __foc_cap.type = folio_object_type (__foc_folio, __foc_object);	\
    __foc_cap.version = folio_object_version (__foc_folio,		\
					      __foc_object);		\
    									\
    struct cap_properties __foc_cap_properties				\
      = CAP_PROPERTIES (folio_object_policy (__foc_folio, __foc_object), \
			CAP_ADDR_TRANS_VOID);				\
    CAP_PROPERTIES_SET (&__foc_cap, __foc_cap_properties);		\
									\
    __foc_cap.oid							\
      = object_to_object_desc ((struct object *) __foc_folio)->oid	\
      + 1 + __foc_object;						\
									\
    __foc_cap;								\
  })
#else
static inline struct cap
folio_object_cap (struct folio *folio, int object)
{
  assert (0 <= object && object < FOLIO_OBJECTS);
  return folio->objects[object];
}
#endif

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
   (Passing cap_void as the value of type simply destroys any existing
   object without allocating a new object.)  POLICY specifies the
   object's policy when accessed via the folio.  If OBJECT_SLOT is not
   ADDR_VOID, then stores a capability to the allocated object in
   OBJECT_SLOT.  If OBJECT_WEAK_SLOT is not ADDR_VOID, stores a weaken
   reference to the created object.  If an object is destroyed and
   there are waiters, they are passed the return code RETURN_CODE.  */
RPC(folio_object_alloc, 8, 0, addr_t, principal,
    addr_t, folio, l4_word_t, index, l4_word_t, type,
    struct object_policy, policy, uintptr_t, return_code,
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
