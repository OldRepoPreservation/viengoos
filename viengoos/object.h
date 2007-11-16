/* object.h - Object store interface.
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

#ifndef RM_OBJECT_H
#define RM_OBJECT_H

#include <l4.h>
#include <error.h>
#include <string.h>
#include <assert.h>
#include <hurd/cap.h>
#include <hurd/folio.h>
#include <stdint.h>

#include "memory.h"
#include "cap.h"
#include "activity.h"
#include "thread.h"

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

  /* The version and OID of the object.  */
  l4_word_t version : CAP_VERSION_BITS;
  l4_word_t type : CAP_TYPE_BITS;

  oid_t oid;

  /* Each activity contains a list of the in-memory objects it is
     currently allocated.  */
  struct
  {
    struct object_desc *next;
    struct object_desc **prevp;
  } activity;

  /* Each allocated object is attached to either the global clean or
     the global dirty list.  */
  struct
  {
    struct object_desc *next;
    struct object_desc **prevp;
  } glru;

  /* Each allocated object is also kept on either its activity's clean
     or its activity's dirty list.  */
  struct
  {
    struct object_desc *next;
    struct object_desc **prevp;
  } alru;
};

/* We keep an array of object descriptors.  There is a linear mapping
   between object desciptors and physical memory addresses.  XXX: This
   is cheap but problematic if there are large holes in the physical
   memory map.  */
extern struct object_desc *object_descs;

/* The global LRU lists.  Every allocated frame is on one of these
   two.  */
extern struct object_desc *dirty;
extern struct object_desc *clean;

/* Initialize the object sub-system.  Must be called after grabbing
   all of the memory.  */
extern void object_init (void);

/* Return the address of the object corresponding to object OID,
   reading it from backing store if required.  */
extern struct object *object_find (struct activity *activity, oid_t oid);

/* If the object corresponding to object OID is in-memory, return it.
   Otherwise, return NULL.  Does not go to disk.  */
extern struct object *object_find_soft (struct activity *activity,
					  oid_t oid);

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

  return cap;
}

/* Return a cap referencing the object OBJECT.  */
static inline struct cap
object_to_cap (struct object *object)
{
  return object_desc_to_cap (object_to_object_desc (object));
}

/* Allocate a folio to activity ACTIVITY.  Returns NULL if not
   possible.  Otherwise a pointer to the in-memory folio.  */
extern struct folio *folio_alloc (struct activity *activity);

/* Reassign the storage designated by FOLIO to the principal
   NEW_PARENT.  */
extern void folio_reparent (struct activity *principal, struct folio *folio,
			    struct activity *new_parent);

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
				struct object **objectp);

/* Deallocate the object stored in page PAGE of folio FOLIO.  */
static inline void
folio_object_free (struct activity *activity,
		   struct folio *folio, int page)
{
  folio_object_alloc (activity, folio, page, cap_void, NULL);
}

/* Deallocate the object OBJECT.  */
static inline void
object_free (struct activity *activity, struct object *object)
{
  struct object_desc *odesc = object_to_object_desc (object);

  int page = odesc->oid % (1 + FOLIO_OBJECTS) - 1;
  oid_t foid = odesc->oid - page - 1;

  struct folio *folio = (struct folio *) object_find (activity, foid);
  assert (folio);

  folio_object_free (activity, folio, page);
}

#endif
