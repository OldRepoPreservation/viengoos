/* map.c - Map management.
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

#include <stdint.h>
#include <string.h>
#include <compiler.h>
#include <assert.h>

#include <hurd/startup.h>
#include <hurd/btree.h>
#include <hurd/slab.h>

#include "vm.h"
#include "priv.h"

/* Initialized by the machine-specific startup-code.  */
extern struct hurd_startup_data *__hurd_startup_data;

/* We don't want to start mapping things at 0 as then we don't catch
   NULL pointer dereferences.  Reserving about 16k makes sense.  */
#define VIRTUAL_MEMORY_START 0x4000

struct as as;

void
map_init (void)
{
  int i;
  struct map *map;

  pthread_mutex_init (&as.lock, NULL);
  hurd_btree_map_tree_init (&as.mappings);

  /* Add the startup code.  */
  map = map_alloc ();
  assert (map);

  map->store = NULL;
  map->store_offset = (uintptr_t) HURD_STARTUP_ADDR;
  map->vm.start = (uintptr_t) HURD_STARTUP_ADDR;
  map->vm.size = HURD_STARTUP_SIZE;

  /* Add the program code, data segment, etc which can be found in the
     start up data.  */
  for (i = 0; i < __hurd_startup_data->mapc; i ++)
    {
      struct hurd_startup_map *mapv = &__hurd_startup_data->mapv[i];

      map = map_alloc ();
      assert (map);

      /* XXX: Wrong, wrong, wrong.  Right now we just make sure we
	 know about the virtual address region the mapping uses and
	 assume that there won't be any page faults.  */
      map->store = NULL;
      // lookup store (mapv->cont.server, mapv->cont.cap_handle) add
      // these frames to it (but we would need to copy them to the
      // default container.
      map->store_offset = mapv->offset & ~0x7;
      map->vm.start = (l4_word_t) mapv->vaddr;
      map->vm.size = mapv->size;

      /* FIXME: Do something with the access rights (in the lower
	 three bits of offset).  */

      map_insert (map);
    }
}

bool map_spare_integrate;
struct map map_spare;
struct frame frame_spare;

/* This is the allocation_buffer call back for the map slab.  If this
   is called then someone has called map_alloc and hence map_lock is
   locked.  */
error_t
mem_slab_allocate_buffer (void *hook, size_t size, void **ptr)
{
  error_t err;
  int i;
  uintptr_t vaddr;

  assert ((size & (size - 1)) == 0);

  /* Find an unused offset.  */
  if (EXPECT_FALSE (! mm_init_done))
    {
      /* We are not yet initialized.  Initialize the mapping database.  */

      /* The mapping database is not yet up so we cannot use the
	 normal functions to search it for a free virtual memory
	 region.

	 FIXME: We assume that the only things mapped into memory are
	 the mappings done by the start up code which we can find out
	 about by looking in __hurd_startup_data.  But this is just
	 not the case!  We need to make sure we don't overwrite the
	 startup code as well as discount regions covered by the KIP
	 and the UTCB. */
      bool vaddr_free;
      vaddr = VIRTUAL_MEMORY_START;
      do
	{
	  struct map *map;

	  /* The proposed index is free unless proven not to be.  */
	  vaddr_free = true;

	  /* Check the startup data.  */
	  if (overlap (vaddr, size,
		       (uintptr_t) HURD_STARTUP_ADDR, HURD_STARTUP_SIZE))
	    {
	      vaddr_free = false;
	      vaddr = (((uintptr_t) HURD_STARTUP_ADDR + HURD_STARTUP_SIZE
			+ size - 1)
		       & ~(size - 1));
	      continue;
	    }

	  /* Loop over the startup maps.  */
	  for (i = 0; i < __hurd_startup_data->mapc; i ++)
	    {
	      struct hurd_startup_map *mapv = &__hurd_startup_data->mapv[i];

	      /* Does the index overlap with a virtual address?  */
	      if (overlap (vaddr, size, (uintptr_t) mapv->vaddr, mapv->size))
		{
		  /* Seems to.  This is a bad index.  */
		  vaddr_free = false;
		  /* Calculate the next candidate virtual memory
		     address.  Remember to mask out the rights which
		     are in the lower three bits of the offset.  */
		  vaddr = (((mapv->offset & ~0x7) + mapv->size + size - 1)
			   & ~(size - 1));
		  break;
		}
	    }

	  if (! vaddr_free)
	    continue;

	  /* We may have already allocated a map structure which
	     search would not find.  This may do some extra work but
	     that's life.  */
	  map = map_find (vaddr, size);
	  if (map)
	    {
	      vaddr_free = false;
	      vaddr = ((map->vm.start + map->vm.size + size - 1)
		       & ~(size - 1));
	    }
	}
      while (! vaddr_free);
    }
  else
    vaddr = map_find_free (size, size);

  /* Save the mapping in the spare map.  We cannot call map_alloc as
     it is likely (indirectly) invoking us because its slab is out of
     memory.  But once we return, it will noticed that we set
     map_spare_integrate and add this mapping to the mapping
     database.  */
  assert (! map_spare_integrate);

  /* We allocate and map the data ourselves even in the case where the
     pager is already up: clearly we are going to use the memory as
     soon as we return.  */
  err = frame_alloc_into (&frame_spare, vaddr, size);
  if (err)
    {
      debug ("frame_alloc_into failed! %d\n", err);
      return err;
    }

  map_spare.store = &swap_store;
  map_spare.store_offset = frame_spare.dc_start;
  map_spare.vm.start = vaddr;
  map_spare.vm.size = size;
  map_spare_integrate = true;

  err = frame_map (&frame_spare, 0, size, vaddr);
  if (err)
    /* Hmm, failure.  Nevertheless we managed to allocate the memory.
       Let's just ignore it and assume that whatever went wrong will
       go away when the memory is eventually faulted in.  */
    debug ("frame_map failed! %d\n", err);

  *ptr = (void *) vaddr;
  return 0;
}

error_t
mem_slab_deallocate_buffer (void *hook, void *buffer, size_t size)
{
  error_t err = hurd_vm_deallocate ((uintptr_t) buffer, size);
  assert_perror (err);
  return 0;
}

struct hurd_slab_space map_slab
  = HURD_SLAB_SPACE_INITIALIZER (struct map,
				 mem_slab_allocate_buffer,
				 mem_slab_deallocate_buffer,
				 NULL, NULL, NULL);

struct map *
map_alloc (void)
{
  error_t err;
  struct map *map;

 start:
  err = hurd_slab_alloc (&map_slab, (void *) &map);
  if (err)
    return 0;

  /* If map_spare_integrate is true then a new slab was just
     allocated.  We use the new map to copy over the data and allocate
     another one for the caller.  */
  if (EXPECT_FALSE (map_spare_integrate))
    {
      struct frame frame, *f;

      memcpy (map, &map_spare, sizeof (struct map));
      map_insert (map);

      /* We can't allocate it using frame_alloc() as that might call
	 the slab's buffer allocation routine and overwrite
	 FRAME_SPARE.  Copy it here, then allocate it and only then
	 copy over.  */
      memcpy (&frame, &frame_spare, sizeof (struct frame));
      map_spare_integrate = false;

      err = hurd_slab_alloc (&frame_slab, (void *) &f);
      assert_perror (err);

      memcpy (f, &frame, sizeof (struct frame));
      frame_insert (&swap_store, map->store_offset, f);

      goto start;
    }

  return map;
}

void
map_insert (struct map *map)
{
  error_t err = hurd_btree_map_insert (&as.mappings, map);
  assert_perror (err);
}

void
map_detach (struct map *map)
{
  assert (hurd_btree_map_find (&as.mappings, &map->vm));
  hurd_btree_map_detach (&as.mappings, map);
}

void
map_dealloc (struct map *map)
{
  map_detach (map);
  hurd_slab_dealloc (&map_slab, (void *) map);
}

struct map *
map_find (uintptr_t address, size_t size)
{
  struct region region = { address, size };
  struct map *map = hurd_btree_map_find (&as.mappings, &region);

  if (! map)
    return NULL;

  for (;;)
    {
      struct map *prev = hurd_btree_map_prev (map);

      if (prev && overlap (address, size, prev->vm.start, prev->vm.size))
	map = prev;
      else
	return map;
    }
}

/* Find a free region of the virtual address space for a region of
   size SIZE with alignment ALIGN.  Must be called with the map lock.
   The lock msut not be dropped until the virtual address is entered
   into the mapping database (and this function should not be called
   again until that has happened).  */
uintptr_t
map_find_free (size_t size, size_t align)
{
  /* Start the probe at the lowest address aligned address after
     VIRTUAL_MEMORY_START.  FIXME: We should use a hint.  But then, we
     shouldn't use linked lists either.  */
  l4_word_t start = (VIRTUAL_MEMORY_START + align - 1) & ~(align - 1);
  bool ok;
  struct map *map;

  do
    {
      /* The proposed start is free unless proven not to be.  */
      ok = true;

      /* Iterate over all of the maps and see if any intersect.  If
	 none do, then we have a good address.  */
      for (map = hurd_btree_map_first (&as.mappings); map;
	   map = hurd_btree_map_next (map))
	if (overlap (start, size, map->vm.start, map->vm.size))
	  {
	    ok = false;
	    /* Use the next aligned virtual address after MAP.  */
	    /* FIXME: We need to check that we don't overflow.  */
	    start = (map->vm.start + map->vm.size + align - 1) & ~(align - 1);
	    break;
	  }
    }
  while (! ok);

  return start;
}

/* Find a free region in a container for a region of size SIZE with
   alignment ALIGN.  Must be called with the map lock.  The lock must
   not be dropped until the virtual address is entered into the
   mapping database (and this function should not be called again
   until that has happened).  */
uintptr_t
store_find_free (struct store *store, size_t size, size_t align)
{
  /* FIXME: Optimize this search!  */
  l4_word_t offset = 0;
  bool ok;
  struct frame *frame;

  assert (! map_spare_integrate);

  do
    {
      /* The proposed offset is free unless proven not to be.  */
      ok = true;

      for (frame = hurd_btree_frame_first (&store->frames); frame;
	   frame = hurd_btree_frame_next (frame))
	if (overlap (offset, size, frame->dc_start, frame->store.size))
	  {
	    ok = false;
	    /* FIXME: Check for overflow.  */
	    offset = (frame->dc_start + frame->store.size + align - 1)
	      & ~(align - 1);
	    break;
	  }
    }
  while (! ok);

  debug ("Sought range covering %x bytes with alignment %x.  Using %x\n",
	 size, align, offset);

  return offset;
}
