/* memory.c - Memory management.
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
#include <compiler.h>
#include <hurd/slab.h>
#include <hurd/startup.h>

#include "priv.h"

struct hurd_slab_space memory_slab
  = HURD_SLAB_SPACE_INITIALIZER (struct hurd_memory,
				 core_slab_allocate_buffer,
				 core_slab_deallocate_buffer,
				 NULL, NULL, NULL);

/* By default, we allocate all anonymous memory in this container.  */
static hurd_pm_container_t default_container;

/* Return a (possibly) free location on the default container for SIZE
   bytes.  */
static uintptr_t
dc_offset (size_t size)
{
  static uintptr_t offset;

  offset += size;
  return offset - size;
}

void
memory_system_init (void)
{
  error_t err;

  /* Initialized by the machine-specific startup-code.  */
  extern struct hurd_startup_data *__hurd_startup_data;

  /* Create the default container.  */
  /* FIXME: We use the image cap handle as the memory control object.
     This is wrong.  */
  err = hurd_pm_container_create (__hurd_startup_data->image.cap_handle,
				  &default_container);
  assert_perror (err);
}

error_t
memory_alloc_into (struct hurd_memory *memory, size_t size)
{
  error_t err;
  size_t amount;

  /* SIZE must be a multiple of the base page size.  */
  assert ((size & (getpagesize () - 1)) == 0);

  memory->cont = default_container;
  memory->store.size = size;

  /* XXX: Evil, evil, evil.  We need to keep a list of all of the
     extant memory and check to make sure that there is no overlap
     with any extant memory.  Or we need to extend
     hurd_pm_container_allocate to dynamically select the address for
     us.  */
  do
    {
      memory->cont_start = dc_offset (size);
      err = hurd_pm_container_allocate (memory->cont, memory->cont_start,
					size, 0, &amount);
    }
  while (err == EEXIST);
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

/* Allocate memory for a hurd_memory structure.  */
static struct hurd_memory *
hurd_memory_new (void)
{
  error_t err;
  struct hurd_memory *memory;

 start:
  err = hurd_slab_alloc (&memory_slab, (void *) &memory);
  if (err)
    return 0;

  if (EXPECT_FALSE (map_spare_integrate))
    {
      struct map map, *m;

      memcpy (memory, &memory_spare, sizeof (struct hurd_memory));
      hurd_store_cache (&core_store, map_spare.store_offset, memory);

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

  return memory;
}

/* Release the memory used by MEMORY.  MEMORY should have been
   deinitialized.  */
static void
hurd_memory_delete (struct hurd_memory *memory)
{
  hurd_slab_dealloc (&memory_slab, memory);
}

struct hurd_memory *
hurd_memory_alloc (size_t size)
{
  error_t err;
  struct hurd_memory *memory;

  /* SIZE must be a multiple of the base page size.  */
  assert ((size & (getpagesize () - 1)) == 0);

  memory = hurd_memory_new ();
  if (! memory)
    return 0;

  err = memory_alloc_into (memory, size);
  if (err)
    {
      hurd_memory_delete (memory);
      return 0;
    }

  return memory;
}

hurd_memory_t *
hurd_memory_use (hurd_pm_container_t cont, uintptr_t cont_start, size_t size)
{
  struct hurd_memory *memory;

  /* SIZE must be a multiple of the base page size.  */
  assert ((size & (getpagesize () - 1)) == 0);

  memory = hurd_memory_new ();
  if (! memory)
    return 0;

  memory->cont = cont;
  memory->cont_start = cont_start;
  memory->store.size = size;

  return memory;
}

hurd_memory_t *
hurd_memory_transfer (hurd_pm_container_t cont, uintptr_t cont_start,
		      size_t size, bool move)
{
  error_t err;
  struct hurd_memory *memory;
  size_t amount;

  /* SIZE must be a multiple of the base page size.  */
  assert ((size & (getpagesize () - 1)) == 0);

  memory = hurd_memory_new ();
  if (! memory)
    return 0;

  do
    {
      memory->cont_start = dc_offset (size);
      err = hurd_pm_container_copy (cont, cont_start,
				    default_container, memory->cont_start,
				    size, HURD_PM_CONT_ALL_OR_NONE, &amount);
      if (err)
	/* XXX: if there is an error properly clean up.  */
	assert (amount == 0);
    }
  while (err == EEXIST);
  if (err)
    {
      assert (amount == 0);
      hurd_memory_delete (memory);
      return NULL;
    }

  assert (amount == size);

  memory->cont = default_container;
  memory->store.size = size;

  return memory;
}

void
hurd_memory_dealloc (hurd_store_t *store, hurd_memory_t *memory,
		     uintptr_t start, size_t length)
{
  error_t err;

  assert (start < memory->store.size);
  /* START and LENGTH are unsigned.  This assert catches the case
     where START is 4k and LENGTH is -4k.  */
  assert (length <= memory->store.size);
  assert (start + length <= memory->store.size);

  assert ((start & (getpagesize () - 1)) == 0);
  assert ((length & (getpagesize () - 1)) == 0);

  err = hurd_pm_container_deallocate (memory->cont,
				      memory->cont_start + start,
				      length);
  assert_perror (err);

  if (start == 0)
    /* Deallocate the beginning of the region.  */
    {
      if (length == memory->store.size)
	/* In fact, remove the whole thing.  */
	{
	  hurd_btree_memory_detach (&store->memory, memory);
	  hurd_memory_delete (memory);
	}
      else
	{
	  memory->cont_start += length;
	  memory->store.start += length;
	  memory->store.size -= length;
	}
    }
  else
    /* Keep the beginning of the memory.  */
    {
      if (start + length == memory->store.size)
	memory->store.size = start;
      else
	/* Deallocate the middle.  */
	{
	  hurd_memory_t *tail
	    = hurd_memory_use (memory->cont,
			       memory->cont_start + start + length,
			       memory->store.size - (start + length));

	  memory->store.size = start;
	  hurd_store_cache (store, memory->store.start + start + length,
			    tail);
	}
    }
}

error_t
hurd_memory_map (struct hurd_memory *memory, size_t offset, size_t length,
		 uintptr_t addr)
{
  error_t err = 0;
  size_t amount;

  assert ((offset & (getpagesize () - 1)) == 0);
  assert ((length & (getpagesize () - 1)) == 0);
  assert ((addr & (getpagesize () - 1)) == 0);
  assert (offset + length <= memory->store.size);

  while (length > 0)
    {
      err = hurd_pm_container_map (memory->cont, memory->cont_start + offset,
				   length, addr, L4_FPAGE_FULLY_ACCESSIBLE,
				   &amount);
      assert_perror (err);
      assert (amount > 0);

      length -= amount;
      offset += amount;
      addr += amount;
    }

  return err;
}
