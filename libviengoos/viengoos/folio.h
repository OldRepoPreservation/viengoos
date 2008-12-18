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

#ifndef _VIENGOOS_FOLIO_H
#define _VIENGOOS_FOLIO_H 1

#include <stdint.h>
#include <bit-array.h>
#include <viengoos/addr.h>
#include <viengoos/cap.h>

/* Number of user objects per folio.  */
enum
  {
    VG_FOLIO_OBJECTS = 128,
  };
enum
  {
    VG_FOLIO_OBJECTS_LOG2 = 7,
  };

/* User settable folio policy.  */

/* The range of valid folio priorities.  A lower numerical value
   corresponds to a lower priority.  */
#define VG_FOLIO_PRIORITY_BITS 15
#define VG_FOLIO_PRIORITY_MIN (-(1 << (VG_FOLIO_PRIORITY_BITS - 1)))
#define VG_FOLIO_PRIORITY_LRU (0)
#define VG_FOLIO_PRIORITY_MAX ((1 << (VG_FOLIO_PRIORITY_BITS - 1)) - 1)

/* The folio group range.  */
#define VG_FOLIO_GROUP_BITS 15
#define VG_FOLIO_GROUP_NONE 0
#define VG_FOLIO_GROUP_MIN 0
#define VG_FOLIO_GROUP_MAX ((1 << FOLIO_BITS) - 1)

struct vg_folio_policy
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
	 VG_FOLIO_GROUP_NONE.  */
      uint32_t group : VG_FOLIO_GROUP_BITS;

      /* By default, the system tries to discard folios according to
	 an LRU policy.  This can be overridden using this field.  In
	 this case, folios from the lowest priority group are
	 discarded.  */
      int32_t priority : VG_FOLIO_PRIORITY_BITS;
    };
    uint32_t raw;
  };
};

#define VG_FOLIO_POLICY_INIT { { raw: 0 } }
#define VG_FOLIO_POLICY_VOID (struct vg_folio_policy) VG_FOLIO_POLICY_INIT
/* The default policy is not discardable.  */
#define VG_FOLIO_POLICY_DEFAULT VG_FOLIO_POLICY_VOID

/* The format of the first page of a folio.  This page is followed (on
   disk) by VG_FOLIO_OBJECTS pages.  */
struct vg_folio
{
#ifdef RM_INTERN
  /* Folios are the unit of storage accounting.  Every folio belongs
     to exactly one activity.  To track what folios belong to a
     particular activity, each folio is attached to a doubly-linked
     list originating at its owner activity.  */
  struct vg_cap activity;
  struct vg_cap next;
  struct vg_cap prev;

  /* The storage policy.  */
  struct vg_folio_policy policy;

  struct
  {
    /* Each object in the folio Disk version of each object.  */
    uint32_t version : VG_CAP_VERSION_BITS;

    /* Whether a page has any content (i.e., if it is not
       uninitialized).  */
    uint32_t content : 1;

    /* The object's memory policy when accessed via the folio.  */
    uint32_t discardable : 1;
    int32_t priority : VG_OBJECT_PRIORITY_BITS;
  } misc[1 + VG_FOLIO_OBJECTS];

  /* The type.  */
  uint8_t types[VG_FOLIO_OBJECTS];

  /* Bit array indicating whether the an object has a non-empty wait
     queue.  */
  uint8_t wait_queues_p[(1 + VG_FOLIO_OBJECTS + (8 - 1)) / 8];

  uint8_t discarded[(VG_FOLIO_OBJECTS + (8 - 1)) / 8];

  /* User reference and dirty bits.  Optionally reset on read.  Set
     respectively when an object is referenced or modified.  Flushing
     the object to disk does not clear this.  */
  uint8_t dirty[(1 + VG_FOLIO_OBJECTS + (8 - 1)) / 8];
  uint8_t referenced[(1 + VG_FOLIO_OBJECTS + (8 - 1)) / 8];

  /* Head of the list of objects waiting for some event on this
     object.  An element of this array is only valid if the
     corresponding element of WAIT_QUEUES_P is true.  The list is a
     circular list.  HEAD->PREV points to the tail.  TAIL->NEXT points
     to the OBJECT (NOT HEAD).  */
  vg_oid_t wait_queues[1 + VG_FOLIO_OBJECTS];

  uint64_t checksums[1 + VG_FOLIO_OBJECTS][2];
#else
  /* User-space folio.  */
  struct vg_cap objects[VG_FOLIO_OBJECTS];
#endif
};

#ifdef RM_INTERN
typedef struct vg_folio *vg_folio_t;
#else
typedef vg_addr_t vg_folio_t;
#endif

/* OBJECT is from -1 to VG_FOLIO_OBJECTS.  */
static inline enum vg_cap_type
vg_folio_object_type (struct vg_folio *folio, int object)
{
#ifdef RM_INTERN
  assert (object >= -1 && object < VG_FOLIO_OBJECTS);

  if (object == -1)
    return vg_cap_folio;
  return folio->types[object];
#else
  assert (object >= 0 && object < VG_FOLIO_OBJECTS);
  return folio->objects[object].type;
#endif
}

static inline void
vg_folio_object_type_set (struct vg_folio *folio, int object,
			  enum vg_cap_type type)
{
  assert (object >= 0 && object < VG_FOLIO_OBJECTS);

#ifdef RM_INTERN
  folio->types[object] = type;
#else
  folio->objects[object].type = type;
#endif
}

static inline struct vg_object_policy
vg_folio_object_policy (struct vg_folio *folio, int object)
{
  struct vg_object_policy policy;

#ifdef RM_INTERN
  assert (object >= -1 && object < VG_FOLIO_OBJECTS);

  policy.discardable = folio->misc[object + 1].discardable;
  policy.priority = folio->misc[object + 1].priority;
#else
  assert (object >= 0 && object < VG_FOLIO_OBJECTS);

  policy.discardable = folio->objects[object].discardable;
  policy.priority = folio->objects[object].priority;
#endif

  return policy;
}

static inline void
vg_folio_object_policy_set (struct vg_folio *folio, int object,
			    struct vg_object_policy policy)
{
#ifdef RM_INTERN
  assert (object >= -1 && object < VG_FOLIO_OBJECTS);

  folio->misc[object + 1].discardable = policy.discardable;
  folio->misc[object + 1].priority = policy.priority;
#else
  assert (object >= 0 && object < VG_FOLIO_OBJECTS);

  folio->objects[object].discardable = policy.discardable;
  folio->objects[object].priority = policy.priority;
#endif
}

#ifdef RM_INTERN
#include <bit-array.h>

static inline bool
folio_object_wait_queue_p (struct vg_folio *folio, int object)
{
  assert (object >= -1 && object < VG_FOLIO_OBJECTS);

  return bit_test (folio->wait_queues_p, object + 1);
}

static inline void
folio_object_wait_queue_p_set (struct vg_folio *folio, int object,
			       bool valid)
{
  assert (object >= -1 && object < VG_FOLIO_OBJECTS);

  bit_set_to (folio->wait_queues_p, sizeof (folio->wait_queues_p),
	      object + 1, valid);
}

static inline vg_oid_t
folio_object_wait_queue (struct vg_folio *folio, int object)
{
  assert (object >= -1 && object < VG_FOLIO_OBJECTS);

  return folio->wait_queues[object + 1];
}

static inline void
folio_object_wait_queue_set (struct vg_folio *folio, int object,
			     vg_oid_t head)
{
  assert (object >= -1 && object < VG_FOLIO_OBJECTS);

  folio->wait_queues[object + 1] = head;
}

static inline uint32_t
folio_object_version (struct vg_folio *folio, int object)
{
  assert (object >= -1 && object < VG_FOLIO_OBJECTS);

  return folio->misc[object + 1].version;
}

static inline void
folio_object_version_set (struct vg_folio *folio, int object,
			  uint32_t version)
{
  assert (object >= -1 && object < VG_FOLIO_OBJECTS);

  folio->misc[object + 1].version = version;
}

static inline bool
folio_object_content (struct vg_folio *folio, int object)
{
  assert (object >= -1 && object < VG_FOLIO_OBJECTS);

  return folio->misc[object + 1].content;
}

static inline void
folio_object_content_set (struct vg_folio *folio, int object,
			  bool content)
{
  assert (object >= -1 && object < VG_FOLIO_OBJECTS);

  folio->misc[object + 1].content = content;
}

static inline bool
folio_object_discarded (struct vg_folio *folio, int object)
{
  assert (object >= 0 && object < VG_FOLIO_OBJECTS);

  return bit_test (folio->discarded, object);
}

static inline void
folio_object_discarded_set (struct vg_folio *folio, int object, bool valid)
{
  assert (object >= 0 && object < VG_FOLIO_OBJECTS);

  bit_set_to (folio->discarded, sizeof (folio->discarded),
	      object, valid);
}

static inline bool
folio_object_referenced (struct vg_folio *folio, int object)
{
  assert (object >= -1 && object < VG_FOLIO_OBJECTS);

  return bit_test (folio->referenced, object + 1);
}

static inline void
folio_object_referenced_set (struct vg_folio *folio, int object, bool p)
{
  assert (object >= -1 && object < VG_FOLIO_OBJECTS);

  bit_set_to (folio->referenced, sizeof (folio->referenced), object + 1, p);
}

static inline bool
folio_object_dirty (struct vg_folio *folio, int object)
{
  assert (object >= -1 && object < VG_FOLIO_OBJECTS);

  return bit_test (folio->dirty, object + 1);
}

static inline void
folio_object_dirty_set (struct vg_folio *folio, int object, bool p)
{
  assert (object >= -1 && object < VG_FOLIO_OBJECTS);

  bit_set_to (folio->dirty, sizeof (folio->dirty), object + 1, p);
}
#endif /* RM_INTERN  */

/* Return a vg_cap designating folio FOLIO's OBJECT'th object.  */
#ifdef RM_INTERN
/* This needs to be a macro as we use object_to_object_desc which is
   made available by object.h but object.h includes this file.  */
#define vg_folio_object_cap(__foc_folio, __foc_object)			\
  ({									\
    struct vg_cap __foc_cap;						\
									\
    __foc_cap.type = vg_folio_object_type (__foc_folio, __foc_object);	\
    __foc_cap.version = folio_object_version (__foc_folio,		\
						 __foc_object);		\
    									\
    struct vg_cap_properties __foc_cap_properties			\
      = VG_CAP_PROPERTIES (vg_folio_object_policy (__foc_folio, __foc_object), \
			   VG_CAP_ADDR_TRANS_VOID);			\
    VG_CAP_PROPERTIES_SET (&__foc_cap, __foc_cap_properties);		\
									\
    __foc_cap.oid							\
      = object_to_object_desc ((struct vg_object *) __foc_folio)->oid	\
      + 1 + __foc_object;						\
									\
    __foc_cap;								\
  })
#else
static inline struct vg_cap
vg_folio_object_cap (struct vg_folio *folio, int object)
{
  assert (0 <= object && object < VG_FOLIO_OBJECTS);
  return folio->objects[object];
}
#endif

#define RPC_STUB_PREFIX vg
#define RPC_ID_PREFIX VG

#include <viengoos/rpc.h>

enum
  {
    VG_folio_alloc = 200,
    VG_folio_free,
    VG_folio_object_alloc,
    VG_folio_policy
  };

/* Allocate a folio against ACTIVITY.  Return a capability in the
   caller's cspace in slot FOLIO.  POLICY specifies the storage
   policy.  */
RPC(folio_alloc, 1, 0, 1,
    /* cap_t, principal, cap_t, activity, */
    struct vg_folio_policy, policy, cap_t, folio)
  
/* Free the folio designated by FOLIO.  */
RPC(folio_free, 0, 0, 0
    /* cap_t, principal, cap_t, folio */)

/* Destroys the INDEXth object in folio FOLIO and allocate in its
   place an object of tye TYPE.  If TYPE is VG_CAP_VOID, any existing
   object is destroyed, however, no object is instantiated in its
   place.  POLICY specifies the object's policy when accessed via the
   folio.  If an object is destroyed and there are waiters, they are
   passed the return code RETURN_CODE.

   Returns a capability to the allocated object in OBJECT.  Returns a
   weak capability to the object in OBJECT_WEAK.  */
RPC(folio_object_alloc, 4, 0, 2,
    /* cap_t, principal, cap_t, folio, */
    uintptr_t, index, uintptr_t, type,
    struct vg_object_policy, policy, uintptr_t, return_code,
    /* Out: */
    cap_t, object, cap_t, object_weak)

/* Flags for folio_policy.  */
enum
{
  VG_FOLIO_POLICY_DELIVER = 1 << 0,

  VG_FOLIO_POLICY_DISCARDABLE_SET = 1 << 1,
  VG_FOLIO_POLICY_GROUP_SET = 1 << 2,
  VG_FOLIO_POLICY_PRIORITY_SET = 1 << 3,

  VG_FOLIO_POLICY_SET = (VG_FOLIO_POLICY_DISCARDABLE_SET
		      | VG_FOLIO_POLICY_GROUP_SET
		      | VG_FOLIO_POLICY_PRIORITY_SET)
};

/* Get and set the management policy for folio FOLIO.

   If VG_FOLIO_POLICY_DELIVER is set in FLAGS, then return FOLIO's
   current paging policy in OLD.  Then, if any of the set flags are
   set, set the corresponding values based on the value of POLICY.  */
RPC(folio_policy, 2, 1, 0,
    /* cap_t, principal, cap_t, folio, */
    uintptr_t, flags, struct vg_folio_policy, policy,
    /* Out: */
    struct vg_folio_policy, old)

#undef RPC_STUB_PREFIX
#undef RPC_ID_PREFIX

#endif
