/* madvise.c - madvise implementation.
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
#include <hurd/addr.h>
#include <hurd/as.h>
#include <hurd/storage.h>
#include <hurd/anonymous.h>
#include <hurd/map.h>

#include <sys/mman.h>
#include <stdint.h>

int
madvise (void *addr, size_t length, int advice)
{
  if (((uintptr_t) addr & (PAGESIZE - 1)) != 0)
    return EINVAL;
  if ((length & (PAGESIZE - 1)) != 0)
    return EINVAL;

  switch (advice)
    {
    case MADV_NORMAL:
      advice = pager_advice_normal;
      break;
    case MADV_RANDOM:
      advice = pager_advice_random;
      break;
    case MADV_SEQUENTIAL:
      advice = pager_advice_sequential;
      break;
    case MADV_WILLNEED:
      advice = pager_advice_willneed;
      break;
    case MADV_DONTNEED:
      advice = pager_advice_dontneed;
      break;
    default:
      return EINVAL;
    }

  uintptr_t start = (uintptr_t) addr;
  uintptr_t end = start + length - 1;

  debug (0, "(%p, %x (%p), %d)", addr, length, end, advice);

  struct region region = { (uintptr_t) addr, length };

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

	/*
	    map_start/map->offset                map_end
                   /   \                           /  \
                  v     v                         v    v
	          +------------------------------------+
	          |                                    | <- pager
	          +------------------------------------+

            ^     ^     ^                        ^     ^     ^
	     \    |    /                          \    |   /
                start                                 end

	 */
	uintptr_t s = map->offset;
	uintptr_t l = map->region.length;
	if (start > map_start)
	  {
	    s += start - map_start;
	    l -= start - map_start;
	  }
	if (end < map_end)
	  l -= map_end - end;

	if (map->pager->advise)
	  map->pager->advise (map->pager, s, l, advice);
      }

  maps_lock_unlock ();

  return 0;
}

int
posix_madvise (void *addr, size_t len, int advice)
{
  return madvise (addr, len, advice);
}
