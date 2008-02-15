/* object.h - Object store interface.
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

#ifndef RM_OBJECT_H
#define RM_OBJECT_H

#include <l4.h>
#include <error.h>
#include <string.h>
#include <assert.h>
#include <hurd/cap.h>
#include <hurd/folio.h>
#include "mutex.h"
#include <hurd/btree.h>
#include <stdint.h>

#include "cap.h"
#include "memory.h"
#include "list.h"

/* Forward.  */
struct activity;
struct thread;

/* Objects
   -------

   A folio is a unit of disk storage.  Objects are allocated out of a
   folio.  Each folio consists of exactly FOLIO_OBJECTS objects each
   PAGESIZE bytes in size.  A folio also includes a 4 kb header (Thus
   a folio consists of a total of FOLIO_OBJECTS + 1 pages of storage).
   The header also describes the folio:

	version
	the activity to which the folio is allocated

   It also describes each object, including:

	the object's type,
	whether it contains content,
	whether it is discardable,
	its version, and
	its checksum

   Object Versioning
   -----------------

   When an object (or a folio) is allocated we guarantee that the only
   reference to the object is the newly created one.  We achieve this
   using object versioning.  Each capability includes a version in
   addition to the OID.  For the capability to be valid, its version
   must match the object's version.  Further, the following variants
   must hold:

     A capability is valid if its version matches the designated
     object's version, and

     When an object is allocated, it is given a version that no
     capability that designates the object has.

   The implementation ensures these invariants.  When a storage device
   is initialized, all objects are set to have a version of 0 and a
   type of cap_void.  As all objects are new, there can be no
   capabilities designating them.  When an object is deallocated, if
   the object's type is void, nothing is done.  Otherwise, the
   object's version is incremented and its type is set to void.  When
   an object is allocated, if the object's type is not void, the
   object is deallocated.  A reference is then generated using the
   object's version.  It is not possible to allocate an object of type
   void.

   So long as the version space is infinite, this suffices.  However,
   as we only use a dedicated number of version bits, we must
   occasionally find a version number that satisfies the above
   invariants.  Without further help, this requires scanning storage.
   To avoid this, when an object is initially allocated, we keep track
   of all in-memory capabilities that reference an object (TODO).  If
   the object is deallocate and no capabilities have been written to
   disk, then we just invalidate the in-memory capabilities, and there
   is no need to increment the on-disk version.  We know this since
   when we allocated the object, there we no capabilities that had the
   object's version and as we kept track of and invalidated all of the
   capabilities that referenced the object and had that version.

   If it happens that an object's version does overflow, then the
   object cannot be immediately reused.  We must scan storage and find
   all capabilities that reference this object and invalidate them (by
   changing their type to void).  To avoid this, we can relocate the
   entire folio.  All of the other objects in the folio are replaced
   by forwarders which transparently forward to the new folio (and
   rewrite indirected capabilities).


   When dereferencing a capability, the version of the folio is not
   check.  When a folio is deallocated, we need to guarantee that any
   capabilities referencing not only the folio but also the objects
   contained in the folio are invalidated.  To achieve this, we
   implicitly deallocate each object contained in the folio according
   to the above rules.  */

/* An object descriptor.  There is one for each in-memory object.  */
struct object_desc
{
  /* Protects the following memorys and the object's data.  */
  ss_mutex_t lock;

  /* The version and OID of the object.  */
  oid_t oid;
  l4_word_t version : CAP_VERSION_BITS;
  l4_word_t type : CAP_TYPE_BITS;

  /* Whether the page is dirty.  */
  l4_word_t dirty: 1;

  /* Whether the object has been selected for eviction.  */
  l4_word_t eviction_candidate : 1;

  /* Whether the object is live.  */
  l4_word_t live: 1;

  /* The object's policy.  Set when the object is claimed using the
     value in the capability referencing the object.  */
  struct object_policy policy;

  /* The object's age.  */
  uint16_t age;

  /* The activity to which the *memory* (not the storage) is
     attributed.  If none, then NULL.  (The owner of the storage can
     be found by looking at the folio.)  */
  struct activity *activity;

  /* The following members are protected by LRU_LOCK.  */

  /* Every in-memory object lives in a hash hashed on its OID.  */
  void *locp;

  union
  {
    /* ACTIVITY is valid, EVICTION_CANDIDATE is false, POLICY.PRIORITY
       != OBJECT_PRIORITY_LRU.

         => attached to ACTIVITY->PRIORITIES.  */
    hurd_btree_node_t priority_node;

    /* ACTIVITY is valid, EVICTION_CANDIDATE is false, POLICY.PRIORITY
       == OBJECT_PRIORITY_LRU,

         => attached to one of ACTIVITY's LRU lists.

      EVICTION_CANDIDATE is true

         => attached to either ACTIVITY->EVICTION_CLEAN or
            ACTIVITY->EVICTION_DIRTY.  */
    struct list_node activity_node;
  };

  union
  {
    /* If EVICTION_CANDIDATE is true and DIRTY and
       !POLICY.DISCARDABLE, then attached to LAUNDRY.

       If EVICTION_CANDIDATE is false, MAY be attached to LAUNDRY if
       DIRTY and !POLICY.DISCARDABLE.  (In this case, it is a
       optimistic sync.)  */
    struct list_node laundry_node;

    /* If EVICTION_CANDIDATE is true and either !DIRTY or
       POLICY.DISCARDABLE, then attached to AVAILABLE.  */
    struct list_node available_node;
  };
};

/* We keep an array of object descriptors.  There is a linear mapping
   between object desciptors and physical memory addresses.  XXX: This
   is cheap but problematic if there are large holes in the physical
   memory map.  */
extern struct object_desc *object_descs;

LIST_CLASS(activity_lru, struct object_desc, activity_node, true)
LIST_CLASS(eviction, struct object_desc, activity_node, true)

LIST_CLASS(available, struct object_desc, available_node, true)
LIST_CLASS(laundry, struct object_desc, laundry_node, true)

/* Lock protecting the following lists as well as an activity's
   lists.  */
extern ss_mutex_t lru_lock;

/* List of objects that need syncing to backing store.  */
extern struct laundry_list laundry;
/* List of clean objects available for reallocation.  */
extern struct available_list available;

/* Sort lower priority objects towards the start.  */
static int
priority_compare (const struct object_policy *a,
		  const struct object_policy *b)
{
  /* XXX: We should actually compare on priority and then on age.  To
     allowing finding an object with a particular priority but any
     age, we need to have a special key.  */
  return (int) a->priority - (int) b->priority;
}

BTREE_CLASS (priorities, struct object_desc,
	     struct object_policy, policy, priority_node,
	     priority_compare, true);

/* Initialize the object sub-system.  Must be called after grabbing
   all of the memory.  */
extern void object_init (void);

/* Return the address of the object corresponding to object OID,
   reading it from backing store if required.  */
extern struct object *object_find (struct activity *activity, oid_t oid,
				   struct object_policy policy);

/* If the object corresponding to object OID is in-memory, return it.
   Otherwise, return NULL.  Does not go to disk.  */
extern struct object *object_find_soft (struct activity *activity,
					oid_t oid,
					struct object_policy policy);

/* Destroy the object OBJECT.  Any changes must have already been
   flushed to disk.  LRU_LOCK must not be held, this function will
   take it.  Does NOT release the memory.  It is the caller's
   responsibility to ensure that memory_frame_free is eventually
   called.  */
extern void memory_object_destroy (struct activity *activity,
				   struct object *object);

/* Return the object corresponding to the object descriptor DESC.  */
#define object_desc_to_object(desc_)					\
  ({									\
    struct object_desc *desc__ = (desc_);				\
    /* There is only one legal area for descriptors.  */		\
    assert ((uintptr_t) object_descs <= (uintptr_t) (desc__));		\
    assert ((uintptr_t) (desc__)					\
	    <= (uintptr_t) &object_descs[(last_frame - first_frame)	\
					 / PAGESIZE]);			\
									\
    (struct object *) (first_frame					\
		       + (((uintptr_t) (desc__) - (uintptr_t) object_descs) \
			  / sizeof (struct object_desc)) * PAGESIZE);	\
  })

/* Return the object descriptor corresponding to the object
   OBJECT.  */
#define object_to_object_desc(object_)					\
  ({									\
    struct object *object__ = (object_);				\
    /* Objects better be on a page boundary.  */			\
    assert (((uintptr_t) (object__) & (PAGESIZE - 1)) == 0);		\
    /* And they better be in memory.  */				\
    assert (first_frame <= (uintptr_t) (object__));			\
    assert ((uintptr_t) (object__) <= last_frame);			\
									\
    &object_descs[((uintptr_t) (object__) - first_frame) / PAGESIZE];	\
  })

/* Return a cap referencing the object designated by OBJECT_DESC.  */
static inline struct cap
object_desc_to_cap (struct object_desc *desc)
{
  struct cap cap;

  cap.type = desc->type;
  cap.oid = desc->oid;
  cap.version = desc->version;
  cap.addr_trans = CAP_ADDR_TRANS_VOID;
  cap.discardable = desc->policy.discardable;
  cap.priority = desc->policy.priority;

  return cap;
}

/* Return a cap referencing the object OBJECT.  */
static inline struct cap
object_to_cap (struct object *object)
{
  return object_desc_to_cap (object_to_object_desc (object));
}

static inline oid_t
object_oid (struct object *object)
{
  return object_to_object_desc (object)->oid;
}

static inline enum cap_type
object_type (struct object *object)
{
  return object_to_object_desc (object)->type;
}

/* Unmaps the object corresponding to DESC from all clients.  */
static inline void
object_desc_unmap (struct object_desc *desc)
{
#ifndef _L4_TEST_ENVIRONMENT
  struct object *object = object_desc_to_object (desc);

  l4_fpage_t flush = l4_fpage ((l4_word_t) object, PAGESIZE);
  l4_fpage_t unmap = l4_fpage_add_rights (flush, L4_FPAGE_FULLY_ACCESSIBLE);

  desc->dirty |= !!l4_was_written (l4_unmap_fpage (unmap));
#endif
}

/* Unmaps the object corresponding to DESC from all clients and also
   retrieves the status bits for this address space.  */
static inline void
object_desc_flush (struct object_desc *desc)
{
#ifndef _L4_TEST_ENVIRONMENT
  assert (desc->live);

  struct object *object = object_desc_to_object (desc);

  l4_fpage_t flush = l4_fpage ((l4_word_t) object, PAGESIZE);
  l4_fpage_t unmap = l4_fpage_add_rights (flush, L4_FPAGE_FULLY_ACCESSIBLE);

  desc->dirty |= !!l4_was_written (l4_unmap_fpage (unmap));

  if (! desc->dirty)
    /* We may have dirty it.  */
    desc->dirty |= !!l4_was_written (l4_flush (flush));
#endif
}

/* Transfer ownership of DESC to the activity ACTIVITY.  If ACTIVITY
   is NULL, detaches DESC from lists (this functionality should only
   be used by memory_object_destroy).  If UPDATE_ACCOUNTING is not
   true, it is the caller's responsibility to update the accounting
   information for the old owner and the new owner.  LRU_LOCK must be
   held.  */
extern void object_desc_claim (struct activity *activity,
			       struct object_desc *desc,
			       struct object_policy policy,
			       bool update_accounting);

/* See object_desc_claim.  */
static inline void
object_claim (struct activity *activity, struct object *object,
	      struct object_policy policy, bool update_accounting)
{
  object_desc_claim (activity, object_to_object_desc (object), policy,
		     update_accounting);
}

static inline void
object_age (struct object_desc *desc, bool referenced)
{
  desc->age >>= 1;
  if (referenced)
    desc->age |= 1 << (sizeof (desc->age) * 8 - 1);
}

/* Return whether the object is active.  */
static inline bool
object_active (struct object_desc *desc)
{
  return desc->age;
}

/* Allocate a folio to activity ACTIVITY.  POLICY is the new folio's
   initial storage policy.  Returns NULL if not possible.  Otherwise a
   pointer to the in-memory folio.  */
extern struct folio *folio_alloc (struct activity *activity,
				  struct folio_policy policy);

/* Assign the storage designated by FOLIO to the activity ACTIVITY.  */
extern void folio_parent (struct activity *activity, struct folio *folio);

/* Destroy the folio FOLIO.  */
extern void folio_free (struct activity *activity, struct folio *folio);

/* Allocate an object of type TYPE using the PAGE page from the folio
   FOLIO.  This implicitly destroys any existing object in that page.
   If there were any waiters waiting for the descruction, they are
   woken and passed RETURN_CODE.  If TYPE is cap_void, this is
   equivalent to calling folio_object_free.  If OBJECTP is not-NULL,
   then the in-memory location of the object is returned in
   *OBJECTP.  */
extern void folio_object_alloc (struct activity *activity,
				struct folio *folio, int page,
				enum cap_type type,
				struct object_policy policy,
				uintptr_t return_code,
				struct object **objectp);

/* Deallocate the object stored in page PAGE of folio FOLIO.  */
static inline void
folio_object_free (struct activity *activity,
		   struct folio *folio, int page)
{
  folio_object_alloc (activity, folio, page, cap_void,
		      OBJECT_POLICY_VOID, 0, NULL);
}

/* Return an object's position within its folio.  */
static inline int
objects_folio_offset (struct object *object)
{
  struct object_desc *desc = object_to_object_desc (object);

  return (desc->oid % (1 + FOLIO_OBJECTS)) - 1;
}

/* Return the folio corresponding to the object OBJECT.  */
static inline struct folio *
objects_folio (struct activity *activity, struct object *object)
{
  struct object_desc *odesc = object_to_object_desc (object);

  int page = objects_folio_offset (object);
  oid_t foid = odesc->oid - page - 1;

  struct folio *folio = (struct folio *) object_find (activity, foid,
						      OBJECT_POLICY_VOID);
  assert (folio);

  return folio;
}

/* Deallocate the object OBJECT.  */
static inline void
object_free (struct activity *activity, struct object *object)
{
  folio_object_free (activity, objects_folio (activity, object),
		     objects_folio_offset (object));
}

/* Get and set folio FOLIO's storage policy according to flags FLAGS,
   IN and OUT.  */
extern void folio_policy (struct activity *activity,
			  struct folio *folio,
			  uintptr_t flags, struct folio_policy in,
			  struct folio_policy *out);

/* Return the first waiter queued on object OBJECT.  */
extern struct thread *object_wait_queue_head (struct activity *activity,
					      struct object *object);

/* Return the last waiter queued on object OBJECT.  */
extern struct thread *object_wait_queue_tail (struct activity *activity,
					      struct object *object);

/* Return the waiter following THREAD.  */
extern struct thread *object_wait_queue_next (struct activity *activity,
					      struct thread *thread);

/* Return the waiter preceding THREAD.  */
extern struct thread *object_wait_queue_prev (struct activity *activity,
					      struct thread *thread);

/* Enqueue thread on object OBJECT's wait queue.  */
extern void object_wait_queue_enqueue (struct activity *activity,
				       struct object *object,
				       struct thread *thread);

/* Dequeue thread THREAD from its wait queue.  */
extern void object_wait_queue_dequeue (struct activity *activity,
				       struct thread *thread);


/* Iterate over each thread waiting on the object at IDX in FOLIO.  It
   is safe to call object_wait_queue_dequeue.  */
#define folio_object_wait_queue_for_each(__owqfe_activity,		\
					 __owqfe_folio, __owqfe_idx,	\
					 __owqfe_thread)		\
  for (struct thread *__owqfe_next					\
	 = (struct thread *)						\
	 (folio_object_wait_queue_p (__owqfe_folio, __owqfe_idx)	\
	  ? object_find (__owqfe_activity,				\
			 folio_object_wait_queue (__owqfe_folio,	\
						  __owqfe_idx),		\
			 OBJECT_POLICY_VOID)				\
	  : NULL);							\
       (__owqfe_thread = __owqfe_next)					\
	 && ((__owqfe_next = object_wait_queue_next (__owqfe_activity,	\
						     __owqfe_thread))	\
	     || 1); /* do nothing.  */)

#define object_wait_queue_for_each(__owqfe_activity, __owqfe_object,	\
				   __owqfe_thread)			\
  for (struct thread *__owqfe_next					\
	 = object_wait_queue_head (__owqfe_activity, __owqfe_object);	\
       (__owqfe_thread = __owqfe_next)					\
	 && ((__owqfe_next = object_wait_queue_next (__owqfe_activity,	\
						     __owqfe_thread))	\
	     || 1); /* do nothing.  */)

#endif
