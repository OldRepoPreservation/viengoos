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
hurd_vm_allocate (uintptr_t *address, size_t size, uintptr_t flags,
		  int map_now)
{
  error_t err = 0;
  struct map *map;
  struct frame *frame;

  assert (mm_init_done);

  /* If VM_HERE is set, assert that the address is page aligned.  */
  if ((flags & VM_HERE) && (*address & (getpagesize () - 1)))
    return EINVAL;

  /* Is size a multiple of the page size?  */
  if ((size & (getpagesize () - 1)))
    return EINVAL;

  /* We don't support VM_HERE yet.  */
  assert (! (flags & VM_HERE));

  pthread_mutex_lock (&as.lock);

  /* Create the mapping.  */
  map = map_alloc ();
  if (! map)
    {
      pthread_mutex_unlock (&as.lock);
      return ENOMEM;
    }

  /* This is anonymous memory.  */
  map->store = &swap_store;
  map->vm.start = map_find_free (size, getpagesize ());
  map->vm.size = size;
  map->store_offset = map->vm.start;
  map_insert (map);

  /* We have established the mapping.  The pager will the physical
     memory and map it in when required.  */

  if (map_now)
    /* But the caller wants the map now.  This is likely because this
       is start up code and the pager is not yet up but it could also
       be that the caller knows that this region will be used
       immediately in which case the fault is pure overhead.  */
    {
      frame = frame_alloc (size);
      if (! frame)
	{
	  debug ("frame_alloc failed!\n");
	  pthread_mutex_unlock (&as.lock);
	  return ENOMEM;
	}
      frame_insert (&swap_store, map->store_offset, frame);

      err = frame_map (frame, 0, map->vm.size, map->vm.start);
      if (err)
	debug ("frame_map failed: %d\n", err);
    }

  *address = map->vm.start;
  pthread_mutex_unlock (&as.lock);

  return err;
}

error_t
hurd_vm_deallocate (uintptr_t start, size_t size)
{
  struct map *map;

  if (start & (getpagesize () - 1))
    return EINVAL;

  if (size & (getpagesize () - 1))
    return EINVAL;

  pthread_mutex_lock (&as.lock);

  map = map_find (start, size);
  if (map)
    for (;;)
      {
	/* We may deallocate MAP.  Get the next one now just in
	   case.  */
	struct map *next = hurd_btree_map_next (map);

	/* What part of MAP do we need to deallocate?  */
	if (start <= map->vm.start)
	  /* At least the start.  */
	  {
	    if (map->vm.start + map->vm.size <= start + size)
	      /* MAP is contained entirely within the region to
		 deallocate.  */
	      {
		frame_dealloc (map->store, map->store_offset,
			       map->vm.size);
		map_dealloc (map);
	      }
	    else
	      /* Only the start of MAP is to be deallocated.  We move
		 the start of the map forward and reduce the size
		 accordingly.  */
	      {
		size_t lost = start + size - map->vm.start;
		assert (lost > 0);
		assert (lost < map->vm.size);

		frame_dealloc (map->store, map->store_offset, lost);

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
                       |<-map->size->|
	                         |<-size->|
                   map->start  start 
	       */
	      {
		size_t lost = map->vm.start + map->vm.size - start;
		assert (lost > 0);
		assert (lost <= size);

		frame_dealloc (map->store, map->store_offset + lost,
			       map->vm.size - lost);

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

		frame_dealloc (map->store,
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

	if (! next || ! overlap (next->vm.start, next->vm.size,
				 start, size))
	  break;
	map = next;
      }

  pthread_mutex_unlock (&as.lock);
  return 0;
}
