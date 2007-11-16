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

/* Like as_alloc but may be called before as_init is called.  If
   MAY_ALLOC is false, may be called before storage_init is done.  */
extern addr_t as_alloc_slow (int width, bool data_mappable,
			     bool may_alloc);

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

/* Allocate an area covering of width WIDTH.  */
extern addr_t as_alloc_slow (int width, bool data_mappable, bool may_alloc);

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

/* Insert the object described by the capability ENTRY into the
   address space at address ADDR.  Creates any required page
   tables.  */
extern void as_insert (activity_t activity,
		       struct cap *root, addr_t addr,
		       struct cap entry, addr_t entry_addr,
		       struct as_insert_rt (*allocate_object)
		         (enum cap_type type, addr_t addr));

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

   TYPE is the required type.  If the type is incompatible
   (cap_rcappage => cap_cappage and cap_rpage => cap_page), bails.  If
   TYPE is -1, then any type is acceptable.  May cause paging.  If
   non-NULL, returns whether the slot is writable in *WRITABLE.  */
extern struct cap *slot_lookup (activity_t activity,
				addr_t addr, enum cap_type type,
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
   reported type of the capability and CAP_ADDR_TRANS the value of its
   address translation fields.  WRITABLE is whether the slot is
   writable.  If VISIT returns a non-zero value, the walk is aborted
   and that value is returned.  If the walk is not aborted, 0 is
   returned.  */
extern int as_walk (int (*visit) (addr_t cap,
				  l4_word_t type,
				  struct cap_addr_trans cap_addr_trans,
				  bool writable,
				  void *cookie),
		    int types,
		    void *cookie);

/* Dump the address space structures.  */
extern void as_dump (const char *prefix);

#endif /* _HURD_AS_H  */
