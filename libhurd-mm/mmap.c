/* mmap.c - mmap implementation.
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


#include <hurd/stddef.h>
#include <hurd/addr.h>
#include <hurd/as.h>
#include <hurd/storage.h>
#include <hurd/anonymous.h>
#include <hurd/map.h>

#include <sys/mman.h>
#include <stdint.h>

void *
mmap (void *addr, size_t length, int protect, int flags,
      int filedes, off_t offset)
{
  if (length == 0)
    return MAP_FAILED;

  if ((flags & ~(MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED)))
    panic ("mmap called with invalid flags");


  enum map_access access = 0;
  if ((PROT_READ & protect) || (PROT_EXEC & protect))
    access |= MAP_ACCESS_READ;
  if ((PROT_READ & protect))
    access |= MAP_ACCESS_WRITE;

  if ((protect & ~(PROT_READ | PROT_WRITE)))
    panic ("mmap called with unsupported protection: %x", protect);


  if ((flags & MAP_FIXED))
    /* Interpret ADDR exactly.  */
    {
      if (((uintptr_t) addr & (PAGESIZE - 1)))
	{
	  debug (0, "MAP_FIXED passed but address not page aligned: %x",
		 addr);
	  return MAP_FAILED;
	}

      /* Unmap any existing mapping.  */
      if (munmap (addr, length) == -1)
	return MAP_FAILED;
    }
  else
    {
      /* Round the address down to a multiple of the pagesize.  */
      addr = (void *) ((uintptr_t) addr & ~(PAGESIZE - 1));
    }

  /* Round length up.  */
  length = (length + PAGESIZE - 1) & ~(PAGESIZE - 1);

  if (addr)
    debug (5, "Trying to allocate memory %x-%x", addr, addr + length);

  struct anonymous_pager *pager;
  pager = anonymous_pager_alloc (ADDR_VOID, addr, length, access,
				 OBJECT_POLICY_DEFAULT,
				 (flags & MAP_FIXED) ? ANONYMOUS_FIXED: 0,
				 NULL, &addr);
  if (! pager)
    {
      debug (0, "Failed to create pager!");
      return MAP_FAILED;
    }

  debug (5, "Allocated memory %x-%x", addr, addr + length);

  return addr;
}

int
munmap (void *addr, size_t length)
{
  uintptr_t start = (uintptr_t) addr;
  uintptr_t end = start + length - 1;

  debug (5, "(%p, %x (%p))", addr, length, end);

  struct region region = { (uintptr_t) addr, length };

  /* There is a race. We can't hold MAPS_LOCK when we call
     PAGER->DESTROY.  In particular, the destroy function may also
     want to unmap some memory, which requires the maps_lock.  We
     can't grab one, drop the lock, destroy and repeat until all the
     pagers in the region are gone: another call to mmap could reuse a
     region we unmaped.  Then we'd unmap that new region.

     To make unmap atomic, we grab the lock, disconnect all maps in
     the region, add each to a list, drop the lock and then destroy
     each map on the list.  */
  struct map *list = NULL;

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
     but scan backwards.  As we disconnect PAGER, we need to remember
     its previous pointer.  */
  struct map *prev = hurd_btree_map_prev (map);

  int dir;
  for (dir = 0; dir < 2; dir ++, map = prev)
    while (map)
      {
	struct map *next = (dir == 0 ? hurd_btree_map_next (map)
			    : hurd_btree_map_prev (map));

	uintptr_t map_start = map->region.start;
	uintptr_t map_end = map_start + map->region.length - 1;

	debug (5, "(%x-%x): considering %x-%x",
	       start, end, map_start, map_end);

	if (map_start > end || map_end < start)
	  break;

	if (map_start < start)
	  /* We only want to unmap the latter part of the region.
	     Split it and set map to that part.  */
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
	  /* We only want to unmap the former part of the region.
	     Split it and set map to that part.  */
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

	/* Unlink and add to the list.  */
	debug (5, "(%x-%x): removing %x-%x",
	       start, end, map_start, map_end);

	map_disconnect (map);
	map->next = list;
	list = map;

	map = next;
      }

  maps_lock_unlock ();

  /* Destroy the maps.  */
  map = list;
  while (map)
    {
      struct map *next = map->next;

      uintptr_t map_start = map->region.start;
      uintptr_t map_end = map_start + map->region.length - 1;
      debug (5, "Detroying pager covering %x-%x (%d pages)",
	     map_start, map_end,
	     (int) (map_end - map_start + 1) / PAGESIZE);

      map_destroy (map);

      map = next;
    }

  return 0;
}
