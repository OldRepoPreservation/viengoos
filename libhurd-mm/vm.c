/* vm.c - Virtual memory management.
   Copyright (C) 2004, 2005 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2, or (at
   your option) any later version.
   
   The GNU Hurd is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with the GNU Hurd; see the file COPYING.  If not, write to
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139,
   USA.  */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <pthread.h>
#include <errno.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <l4.h>

#include <hurd/vm.h>

#include "physmem-user.h"
#include "priv.h"

error_t
hurd_vm_release (uintptr_t start, size_t size)
{
  struct map *map, *next;

  if (start & (getpagesize () - 1))
    return EINVAL;

  if (size & (getpagesize () - 1))
    return EINVAL;

  pthread_mutex_lock (&as.lock);

  next = map_find_first (start, size);
  while (next)
    {
      map = next;

      if (next->vm.start + next->vm.size < start + size)
	{
	  /* We may deallocate MAP.  Get the one following it now.  */
	  next = hurd_btree_map_next (map);
	  if (next && ! overlap (next->vm.start, next->vm.size, start, size))
	    next = 0;
	}
      else
	next = 0;

      printf ("%s (start: %x, size: %x) map->vm: %x+%x\n",
	      __FUNCTION__, start, size, map->vm.start, map->vm.size);

      /* What part of MAP do we need to deallocate?  */
      if (start <= map->vm.start)
	/* At least the start.  */
	{
	  if (map->vm.start + map->vm.size <= start + size)
	    /* MAP is contained entirely within the region to
	       deallocate.  */
	    {
	      hurd_store_flush (map->store, map->store_offset, map->vm.size);
	      map_free (map);
	    }
	  else
	    /* Only the start of MAP is to be deallocated.  We move
	       the start of the map forward and reduce the size
	       accordingly.  */
	    {
	      size_t lost = start + size - map->vm.start;
	      assert (lost > 0);
	      assert (lost < map->vm.size);

	      hurd_store_flush (map->store, map->store_offset, lost);

	      map->vm.start += lost;
	      map->store_offset += lost;
	      map->vm.size -= lost;
	    }
	}
      else
	/* The start of MAP remains.  */
	{
	  if (start + size >= map->vm.start + map->vm.size)
	    /* Deallocate the end of MAP.
	         |<- map->vm.size ->|
	                     |<- size ->|
	       map->start  start 
	    */
	    {
	      size_t lost = map->vm.start + map->vm.size - start;
	      assert (lost > 0);
	      assert (lost <= size);

	      hurd_store_flush (map->store,
				map->store_offset + map->vm.size - lost,
				lost);

	      map->vm.size -= lost;
	    }
	  else
	    /* Deallocate the middle of this mapping.  We have to
	       split it in two.  We keep MAP as the head and reduce
	       its size.  We allocate a new mapping, TAIL, and
	       insert it into the mapping database.

	           map->start  start 
	                         |<-size->|
	               |<--------map->size-------->|

	          =>
	               |<--map-->|        |<-tail->|
	    */
	    {
	      struct map *tail = map_alloc ();

	      hurd_store_flush (map->store,
				map->store_offset + start - map->vm.start,
				size);

	      memcpy (tail, map, sizeof (*tail));
	      tail->vm.start = start + size;
	      tail->store_offset += start + size - map->vm.start;
	      tail->vm.size -= start + size - map->vm.start;

	      map->vm.size = start - map->vm.start;

	      map_insert (tail);
	    }
	}
    }

  pthread_mutex_unlock (&as.lock);
  return 0;
}
