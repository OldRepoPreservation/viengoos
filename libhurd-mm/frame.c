/* frame.c - Frame management.
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

#include <string.h>
#include <assert.h>
#include <hurd/slab.h>
#include <compiler.h>

#include "priv.h"

struct store swap_store;

struct hurd_slab_space frame_slab
  = HURD_SLAB_SPACE_INITIALIZER (struct frame,
				 mem_slab_allocate_buffer,
				 mem_slab_deallocate_buffer,
				 NULL, NULL, NULL);

error_t
frame_alloc_into (struct frame *frame, uintptr_t dc_start, size_t size)
{
  error_t err;
  size_t amount;

  frame->store.size = size;
  frame->dc_start = dc_start;

  err = hurd_pm_container_allocate (default_container,
				    dc_start, size, 0, &amount);
  if (err)
    {
      /* XXX: We failed to allocate the physical memory.  Invoke the
	 pageout daemon and then try again.  */
      debug ("hurd_pm_container_allocate failed! %d\n", err);
      return err;
    }
  assert (size == amount);

  return 0;
}

struct frame *
frame_alloc (size_t size)
{
  error_t err;
  struct frame *frame;
  uintptr_t dc_start;

 start:
  err = hurd_slab_alloc (&frame_slab, (void *) &frame);
  if (err)
    return 0;

  if (EXPECT_FALSE (map_spare_integrate))
    {
      struct map map, *m;

      memcpy (frame, &frame_spare, sizeof (struct frame));
      frame_insert (&swap_store, map_spare.store_offset, frame);

      /* We cannot allocate the map structure using map_alloc() as
	 that might call the slab's buffer allocation routine and
	 overwrite MAP_SPARE.  Copy it here, then allocate it and only
	 then copy over.  */
      memcpy (&map, &map_spare, sizeof (struct map));

      /* But if we end up triggering another allocationg then we have
	 to make sure that the range we just allocated is excluded.
	 So, we have to insert this map and then remove it my
	 hand.  */
      map_insert (&map);

      map_spare_integrate = false;

      err = hurd_slab_alloc (&map_slab, (void *) &m);
      assert_perror (err);

      map_detach (&map);
      memcpy (m, &map, sizeof (struct map));
      map_insert (m);

      goto start;
    }

  dc_start = store_find_free (&swap_store, size, getpagesize ());

  err = frame_alloc_into (frame, dc_start, size);
  if (err)
    {
      hurd_slab_dealloc (&frame_slab, frame);
      return 0;
    }

  return frame;
}

void
frame_insert (struct store *store, uintptr_t store_start, struct frame *frame)
{
  error_t err;

  frame->store.start = store_start;
  err = hurd_btree_frame_insert (&store->frames, frame);
  assert_perror (err);
}

struct frame *
frame_find_first (struct store *store, uintptr_t store_start, size_t length)
{
  struct region region = { store_start, length };
  struct frame *frame = hurd_btree_frame_find (&store->frames, &region);

  if (! frame)
    return NULL;

  for (;;)
    {
      struct frame *prev;

      /* If the beginning of this frame is before or at where
	 STORE_START then the frame before this one cannot overlap.
	 Hence, we are done.  */
      if (frame->store.start <= store_start)
	return frame;

      prev = hurd_btree_frame_prev (frame);

      if (prev
	  && overlap (store_start, length,
		      prev->store.start, prev->store.size))
	frame = prev;
      else
	return frame;
    }
}

error_t
frame_map (struct frame *frame, size_t offset, size_t length, uintptr_t addr)
{
  error_t err;

  assert ((offset & (getpagesize () - 1)) == 0);
  assert ((length & (getpagesize () - 1)) == 0);
  assert ((addr & (getpagesize () - 1)) == 0);
  assert (offset + length <= frame->store.size);

  err = hurd_pm_container_map (default_container,
			       frame->dc_start + offset, length, addr,
			       L4_FPAGE_FULLY_ACCESSIBLE);
  if (err)
    debug ("hurd_pm_container_map failed: %d\n", err);

  return err;
}

void
frame_dealloc (struct store *store, uintptr_t store_start, size_t length)
{
  error_t err;
  struct frame *frame;

  assert ((store_start & (getpagesize () - 1)) == 0);
  assert ((length & (getpagesize () - 1)) == 0);

  frame = frame_find_first (store, store_start, length);
  if (frame)
    for (;;)
      {
	int done;
	struct frame *next;

	/* We deallocate the entire frame even if that is more than we
	   were asked to deallocate.  This is not a problem as */
	err = hurd_pm_container_deallocate (default_container,
					    frame->dc_start,
					    frame->store.size);
	assert_perror (err);

	/* If this frame extends at least as far region we are
	   deallocating, then the next frame couldn't be covered by
	   the region we are deallocating.  Hence, we are
	   finished.  */
	done = frame->store.start + frame->store.size > store_start + length;

	if (! done)
	  next = hurd_btree_frame_next (frame);
	else
	  /* Elide a gcc warning.  */
	  next = 0;

	hurd_btree_frame_detach (&store->frames, frame);
	hurd_slab_dealloc (&frame_slab, frame);

	if (! done && next && overlap (store_start, length,
				       next->store.start, next->store.size))
	  frame = next;
	else
	  break;
      }
}
