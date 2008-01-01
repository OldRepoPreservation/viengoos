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
#include <hurd/mutex.h>
#include <hurd/btree.h>
#include <stdint.h>

#include "cap.h"
#include "memory.h"
#include "list.h"

/* Forward.  */
struct activity;

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
  /* Every in-memory object lives in a hash hashed on its OID.  */
  void *locp;

  ss_mutex_t lock;

  /* The version and OID of the object.  */
  oid_t oid;

  l4_word_t version : CAP_VERSION_BITS;
  l4_word_t type : CAP_TYPE_BITS;

  l4_word_t dirty: 1;

  struct object_policy policy;

  /* The object's age.  */
  uint16_t age;

  /* Ordered list of all owned objects whose priority is other than
     OBJECT_PRIORITY_LRU.  */
  hurd_btree_node_t priority_node;

  /* The activity to which the *memory* (not the storage) is
     attributed.  If none, then NULL.  (The owner of the storage can
     be found by looking at the folio.)  */
  struct activity *activity;

  /* Each allocated object is attached to the activity that pays for
     it.  If ACTIVITY is NULL, then attached to DISOWNED.  Protected
     by LRU_LOCK.  */
  struct list_node activity_lru;

  /* Each allocated object is attached to either the global active or
     the global inactive list.  */
  struct list_node global_lru;
};

/* We keep an array of object descriptors.  There is a linear mapping
   between object desciptors and physical memory addresses.  XXX: This
   is cheap but problematic if there are large holes in the physical
   memory map.  */
extern struct object_desc *object_descs;

LIST_CLASS(object_activity_lru, struct object_desc, activity_lru)
LIST_CLASS(object_global_lru, struct object_desc, global_lru)

/* Lock protecting the following lists and each object descriptor's
   activity_lru, global_lru and priority_node fields.  */
extern ss_mutex_t lru_lock;

/* The global LRU lists.  Every allocated frame is on one of these
   two.  */
extern struct object_global_lru_list global_active;
extern struct object_global_lru_list global_inactive_dirty;
extern struct object_global_lru_list global_inactive_clean;
/* Objects that are not accounted to any activity (e.g., if the
   activity to which an object is attached is destroyed, it is
   attached to this list).  */
extern struct object_activity_lru_list disowned;

/* Sort lower priority objects towards the start.  */
static int
priority_compare (const struct object_policy *a,
		  const struct object_policy *b)
{
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

/* Return the object corresponding to the object descriptor DESC.  */
#define object_desc_to_object(desc_) \
  ({ \
    struct object_desc *desc__ = (desc_); \
    /* There is only one legal area for descriptors.  */ \
    assert ((uintptr_t) object_descs <= (uintptr_t) (desc__)); \
    assert ((uintptr_t) (desc__) \
	    <= (uintptr_t) &object_descs[(last_frame - first_frame) \
					 / PAGESIZE]); \
  \
    (struct object *) (first_frame \
		       + (((uintptr_t) (desc__) - (uintptr_t) object_descs) \
			  / sizeof (struct object_desc)) * PAGESIZE); \
  })

/* Return the object descriptor corresponding to the object
   OBJECT.  */
#define object_to_object_desc(object_) \
  ({ \
    struct object *object__ = (object_); \
    /* Objects better be on a page boundary.  */ \
    assert (((uintptr_t) (object__) & (PAGESIZE - 1)) == 0); \
    /* And they better be in memory.  */ \
    assert (first_frame <= (uintptr_t) (object__)); \
    assert ((uintptr_t) (object__) <= last_frame); \
  \
    &object_descs[((uintptr_t) (object__) - first_frame) / PAGESIZE]; \
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

static inline enum cap_type
object_type (struct object *object)
{
  return object_to_object_desc (object)->type;
}

/* Like object_disown but does not adjust DESC->ACTIVITY->FRAMES.
   (This is useful when removing multiple frames at once.)  */
extern void object_desc_disown_simple (struct object_desc *desc);

static inline void
object_disown_simple (struct object *object)
{
  object_desc_disown_simple (object_to_object_desc (object));
}

extern inline void object_desc_disown_ (struct object_desc *desc);
#define object_desc_disown(d)						\
  ({ debug (5, "object_desc_disown: %p (%d)",				\
	    d->activity, d->activity->frames_total);			\
    assert (! ss_mutex_trylock (&lru_lock));				\
    object_desc_disown_ (d); })

/* The activity to which OBJECT is accounted should no longer be
   accounted OJBECT.  Attaches object to the DISOWNED list.  */
static inline void
object_disown_ (struct object *object)
{
  object_desc_disown_ (object_to_object_desc (object));
}
#define object_disown(o)						\
  ({									\
    debug (5, "object_disown: ");					\
    assert (! ss_mutex_trylock (&lru_lock));				\
    object_disown_ (o);							\
  })

/* Transfer ownership of DESC to the activity ACTIVITY.  */
extern void object_desc_claim_ (struct activity *activity,
				struct object_desc *desc,
				struct object_policy policy);
#define object_desc_claim(__odc_a, __odc_o, __odc_p)			\
  ({									\
    debug (5, "object_desc_claim: %p (%d)",				\
	   (__odc_a), (__odc_a)->frames_total);				\
    assert (! ss_mutex_trylock (&lru_lock));				\
    object_desc_claim_ ((__odc_a), (__odc_o), (__odc_p));		\
  })

/* Transfer ownership of OBJECT to the activity ACTIVITY.  */
static inline void
object_claim_ (struct activity *activity, struct object *object,
	       struct object_policy policy)
{
  object_desc_claim_ (activity, object_to_object_desc (object), policy);
}
#define object_claim(__oc_a, __oc_o, __oc_p)				\
  ({									\
    debug (5, "object_claim: %p (%d)",					\
	   (__oc_a), (__oc_a)->frames_total);				\
    assert (! ss_mutex_trylock (&lru_lock));				\
    object_claim_ ((__oc_a), (__oc_o));					\
  })

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
   If TYPE is cap_void, this is equivalent to calling
   folio_object_free.  If OBJECTP is not-NULL, then the in-memory
   location of the object is returned in *OBJECTP.  */
extern void folio_object_alloc (struct activity *activity,
				struct folio *folio, int page,
				enum cap_type type,
				struct object_policy policy,
				struct object **objectp);

/* Deallocate the object stored in page PAGE of folio FOLIO.  */
static inline void
folio_object_free (struct activity *activity,
		   struct folio *folio, int page)
{
  folio_object_alloc (activity, folio, page, cap_void,
		      OBJECT_POLICY_VOID, NULL);
}

/* Deallocate the object OBJECT.  */
static inline void
object_free (struct activity *activity, struct object *object)
{
  struct object_desc *odesc = object_to_object_desc (object);

  int page = (odesc->oid % (1 + FOLIO_OBJECTS)) - 1;
  oid_t foid = odesc->oid - page - 1;

  struct folio *folio = (struct folio *) object_find (activity, foid,
						      OBJECT_POLICY_VOID);
  assert (folio);

  folio_object_free (activity, folio, page);
}

/* Get and set folio FOLIO's storage policy according to flags FLAGS,
   IN and OUT.  */
extern void folio_policy (struct activity *activity,
			  struct folio *folio,
			  uintptr_t flags, struct folio_policy in,
			  struct folio_policy *out);

#endif
