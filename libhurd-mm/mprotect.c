/* mprotect.c - mprotect implementation.
   Copyright (C) 2008 Free Software Foundation, Inc.
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

#include <hurd/stddef.h>
#include <viengoos/addr.h>
#include <hurd/as.h>
#include <hurd/storage.h>
#include <hurd/anonymous.h>
#include <hurd/map.h>

#include <sys/mman.h>
#include <stdint.h>

int
mprotect (void *addr, size_t length, int prot)
{
  uintptr_t start = (uintptr_t) addr;
  uintptr_t end = start + length - 1;

  debug (5, "(%p, %x (%p),%s%s)", addr, length, (void *) end,
	 prot == 0 ? " PROT_NONE" : (prot & PROT_READ ? " PROT_READ" : ""),
	 prot & PROT_WRITE ? " PROT_WRITE" : "");

  struct region region = { (uintptr_t) addr, length };

  enum map_access access = 0;
  if ((prot & PROT_READ))
    access = MAP_ACCESS_READ;
  if ((prot & PROT_WRITE))
    access |= MAP_ACCESS_WRITE;

  maps_lock_lock ();

  /* Find any pager that overlaps within the designated region.  */
  struct map *map = map_find (region);
  if (! map)
    /* There are none.  We're done.  */
    {
      maps_lock_unlock ();
      return 0;
    }

  /* There may be pagers that come lexically before as well as after
     PAGER.  We start with PAGER and scan forward and then do the same
     but scan backwards.  */
  struct map *prev = hurd_btree_map_prev (map);

  int dir;
  for (dir = 0; dir < 2; dir ++, map = prev)
    for (;
	 map;
	 map = (dir == 0 ? hurd_btree_map_next (map)
		: hurd_btree_map_prev (map)))
      {
	uintptr_t map_start = map->region.start;
	uintptr_t map_end = map_start + map->region.length - 1;

	debug (5, "(%x-%x): considering %x-%x",
	       start, end, map_start, map_end);

	if (map_start > end || map_end < start)
	  break;

	if (map->access == access)
	  /* The access is already correct.  Nothing to do.  */
	  continue;

	if (map_start < start)
	  /* We need to split.  */
	  {
	    debug (5, "(%x-%x): splitting %x-%x at offset %x",
		   start, end, map_start, map_end, start - map_start);

	    struct map *second = map_split (map, start - map_start);
	    if (second)
	      map = second;
	    else
	      {
		panic ("munmap (%x-%x) but cannot split map at %x-%x",
		       start, end, map_start, map_end);
	      }

	    map_start = start;
	  }

	if (map_end > end)
	  /* We need to split.  */
	  {
	    debug (5, "(%x-%x): splitting %x-%x at offset %x",
		   start, end, map_start, map_end, end - map_start + 1);
		
	    struct map *second = map_split (map, end - map_start + 1);
	    if (! second)
	      {
		panic ("munmap (%x-%x) but cannot split map at %x-%x",
		       start, end, map_start, map_end);
	      }

	    map_end = end;
	  }

	if (((map->access & MAP_ACCESS_WRITE) && ! (access & PROT_WRITE))
	    || ((map->access & MAP_ACCESS_READ) && ! (access & PROT_READ)))
	  /* We need to reduce the permission on all capabilities in
	     the area.  We don't have to do this if we are increasing
	     access: faulting will handle that.  */
	  {
	    map->access = access;

	    addr_t addr;
	    for (addr = ADDR (map_start, ADDR_BITS - PAGESIZE_LOG2);
		 addr_prefix (addr) < map_end;
		 addr = addr_add (addr, 1))
	      {
		/* This may fail if the page has not yet been faulted
		   in.  That's okay: it will get the right
		   permissions.  */
		as_slot_lookup_use
		  (addr,
		   ({
		     if (map->access == 0)
		       {
			 error_t err;
			 err = rm_cap_rubout (meta_data_activity,
					      ADDR_VOID, addr);
			 assert (! err);
			 slot->type = cap_void;
		       }
		     else
		       {
			 bool ret;
			 ret = cap_copy_x (meta_data_activity,
					   ADDR_VOID, slot, addr,
					   ADDR_VOID, *slot, addr,
					   CAP_COPY_WEAKEN,
					   CAP_PROPERTIES_VOID);
			 assert (ret);
		       }
		   }));
	      }
	  }
	else
	  map->access = access;
      }

  maps_lock_unlock ();

  return 0;
}

