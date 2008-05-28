/* as.h - Address space interface.
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

#ifndef _HURD_AS_H
#define _HURD_AS_H

#include <hurd/addr.h>
#include <hurd/cap.h>
#include <stdbool.h>

/* The address space allocator keeps track of which addresses are
   allocated and which are available.  The allocator supports the
   efficient allocation of both arbitrary and specific addresses.
   When an address is allocated, all address under that address are
   allocated as well.  */

/* Allocate COUNT contiguous subtree such that the root's depth of
   each is at least ADDR_BITS - WIDTH.  If DATA_MAPPABLE is true, then
   ensures that the leaves of each subtree are mappable in the region
   accessible to data instructions.  On success returns the address of
   the first subtree.  Otherwise, returns ADDR_VOID.  */
extern addr_t as_alloc (int width, l4_uint64_t count,
			bool data_mappable);

/* Like as_alloc but may be called before as_init is called.  Address
   is returned in the descriptor's object field.  The caller must fill
   in the rest.  */
extern struct hurd_object_desc *as_alloc_slow (int width);

/* Allocate the COUNT contiguous addresses strating at address ADDR.
   Returns true on success, false otherwise.  */
extern bool as_alloc_at (addr_t addr, l4_uint64_t count);

/* Free the COUNT contiguous addresses starting at ADDR.  Each ADDR
   must have been previously returned by a call to as_chunk_alloc or
   as_region_alloc.  All address returned by a call to as_chunk_alloc
   or as_region_alloc need not be freed by a single call to
   as_free.  */
extern void as_free (addr_t addr, l4_uint64_t count);

/* Whether as_init has completed.  */
extern bool as_init_done;

/* Initialize the address space manager.  */
extern void as_init (void);

/* Print the allocated areas.  */
extern void as_alloced_dump (const char *prefix);

/* Root of the shadow page tables.  */
extern struct cap shadow_root;

/* Because metadata is often a resource shared among the activities
   running in a particular address space, all metadata is built from a
   single activity.  This should dominate all activities running in
   this address space to avoid priority inversion.  */
extern addr_t meta_data_activity;

/* Ensure that the slot designated by ADDR is accessible.  Return that
   capability.  */
extern struct cap *as_slot_ensure (addr_t addr);

struct as_insert_rt
{
  struct cap cap;
  addr_t storage;
};

/* Ensure that the slot designated by ADDR in the address space root
   at AS is accessible by allocating any required page tables.  Return
   the cap associated with ADDR.  */
extern struct cap *as_slot_ensure_full
  (activity_t activity,
   addr_t as, struct cap *root, addr_t addr,
   struct as_insert_rt (*allocate_object) (enum cap_type type, addr_t addr));

/* Copy the capability located at SOURCE_ADDR in the address space
   rooted at SOURCE_AS to address ADDR in the address space rooted at
   TARGET_AS.  Allocates any necessary page-tables in the target
   address space.  ALLOC_OBJECT is a callback to allocate an object of
   type TYPE at address ADDR.  The callback should NOT insert the
   allocated object into the addresss space.  Returns the slot into
   which the capability was inserted.  */
extern struct cap *as_insert
  (activity_t activity,
   addr_t target_as, struct cap *t_as_cap, addr_t target,
   addr_t source_as, struct cap c_cap, addr_t source,
   struct as_insert_rt (*allocate_object) (enum cap_type type, addr_t addr));

/* Function signature of the call back used by
   as_slot_ensure_full_custom and as_insert_custom.

   PT designates a cappage or a folio.  The cappage or folio is at
   address PT_ADDR.  Index the object designed by PTE returning the
   location of the idx'th capability slot.  If the capability is
   implicit (in the case of a folio), return a fabricated capability
   in *FAKE_SLOT and return FAKE_SLOT.  Return NULL on failure.  */
typedef struct cap *(*as_object_index_t) (activity_t activity,
					  struct cap *pt,
					  addr_t pt_addr, int idx,
					  struct cap *fake_slot);

/* Variant of as_slot_ensure_full that doesn't use the shadow page
   tables but calls the callback OBJECT_INDEX to retrieve the
   capability slot.  */
extern struct cap *as_slot_ensure_full_custom
  (activity_t activity,
   addr_t as, struct cap *root, addr_t addr,
   struct as_insert_rt (*allocate_object) (enum cap_type type, addr_t addr),
   as_object_index_t object_index);

/* Variant of as_insert that doesn't use the shadow page tables but
   calls the callback OBJECT_INDEX to retrieve capability slots.  */
extern struct cap *as_insert_custom
  (activity_t activity,
   addr_t target_as, struct cap *t_as_cap, addr_t target,
   addr_t source_as, struct cap c_cap, addr_t source,
   struct as_insert_rt (*allocate_object) (enum cap_type type, addr_t addr),
   as_object_index_t object_index);

/* Return the value of the capability at ADDR or, if an object, the
   value of the capability designating the object.

   TYPE is the required type.  If the type is incompatible
   (cap_rcappage => cap_cappage and cap_rpage => cap_page), bails.  If
   TYPE is -1, then any type is acceptable.  May cause paging.  If
   non-NULL, returns whether the slot is writable in *WRITABLE.  */
extern struct cap cap_lookup (activity_t activity,
			      addr_t addr, enum cap_type type,
			      bool *writable);

/* Return the capability slot at ADDR or, if an object, the slot
   referencing the object.

   May cause paging.  If WRITABLE is non-NULL, returns whether the
   slot is writable in *WRITABLE (it is not writable if it was
   accessed via a read-only cappage).  */
extern struct cap *slot_lookup (activity_t activity, addr_t addr,
				bool *writable);

/* Return the value of the capability designating the object at ADDR.

   TYPE is the required type.  If the type is incompatible
   (cap_rcappage => cap_cappage and cap_rpage => cap_page), bails.  If
   TYPE is -1, then any type is acceptable.  May cause paging.  If
   non-NULL, returns whether the slot is writable in *WRITABLE.  */
extern struct cap object_lookup (activity_t activity,
				 addr_t addr, enum cap_type type,
				 bool *writable);

/* Walk the address space (without using the shadow page tables),
   depth first.  VISIT is called for each slot for which (1 <<
   reported capability type) & TYPES is non-zero.  TYPE is the
   reported type of the capability and PROPERTIES the value of its
   properties.  WRITABLE is whether the slot is writable.  If VISIT
   returns a non-zero value, the walk is aborted and that value is
   returned.  If the walk is not aborted, 0 is returned.  */
extern int as_walk (int (*visit) (addr_t cap,
				  l4_word_t type,
				  struct cap_properties properties,
				  bool writable,
				  void *cookie),
		    int types,
		    void *cookie);

/* Dump the address space structures.  */
extern void as_dump (const char *prefix);

#endif /* _HURD_AS_H  */
