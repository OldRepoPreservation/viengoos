/* as.h - Address space composition helper functions interface.
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

#ifndef RM_AS_H
#define RM_AS_H

#include <l4.h>
#include <hurd/cap.h>

struct as_insert_rt
{
  struct cap cap;
  addr_t storage;
};

/* Callback used by the following function.  When called must return a
   cap designating an object of type TYPE.  */
typedef struct as_insert_rt allocate_object_callback_t (enum cap_type type,
							addr_t addr);

/* Insert the object described by entry ENTRY at address ADDR into the
   address space rooted at ROOT.  ALLOC_OBJECT is a callback to
   allocate an object of type TYPE at address ADDR.  The callback
   should NOT insert the allocated object into the addresss space.  */
extern void as_insert (activity_t activity,
		       struct cap *root, addr_t addr,
		       struct cap entry, addr_t entry_addr,
		       allocate_object_callback_t alloc);

/* If debugging is enabled dump the address space described by ROOT.
   PREFIX is prefixed to each line of output.  */
extern void as_dump_from (activity_t activity,
			  struct cap *root, const char *prefix);

#endif
