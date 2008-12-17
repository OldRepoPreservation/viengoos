/* as-build-custom.c - Address space composition helper functions.
   Copyright (C) 2008 Free Software Foundation, Inc.
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

#include <viengoos/activity.h>
#include <viengoos/addr.h>
#include <viengoos/cap.h>

/* Expose as_slot_ensure_full_custom and as_insert_custom.  */
#define ID_SUFFIX custom
#define OBJECT_INDEX_ARG_TYPE as_object_index_t

#include "as-build.c"

struct cap *
as_ensure_full_custom (activity_t activity,
		       addr_t as_root_addr, struct cap *root, addr_t addr,
		       as_allocate_page_table_t as_allocate_page_table,
		       as_object_index_t object_index)
{
  return as_build_custom (activity, as_root_addr, root, addr,
			  as_allocate_page_table, object_index,
			  true);
}

struct cap *
as_insert_custom (activity_t activity,
		  addr_t as_root_addr, struct cap *root, addr_t addr,
		  addr_t entry_as, struct cap entry, addr_t entry_addr,
		  as_allocate_page_table_t as_allocate_page_table,
		  as_object_index_t object_index)
{
  struct cap *slot = as_build_custom (activity, as_root_addr, root, addr,
				      as_allocate_page_table,
				      object_index, false);
  assert (slot);
  cap_copy (activity, as_root_addr, slot, addr, entry_as, entry, entry_addr);

  return slot;
}
