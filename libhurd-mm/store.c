/* store.c - Store class implementation.
   Copyright (C) 2005 Free Software Foundation, Inc.
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

#include <assert.h>
#include <errno.h>
#include <compiler.h>

#include "priv.h"

void
store_dump (struct hurd_store *store)
{
  struct hurd_memory *mem;

  printf ("store %x: ", store);
  for (mem = hurd_btree_memory_first (&store->memory); mem;
       mem = hurd_btree_memory_next (mem))
    printf ("cont:%x@%x on store@%x+%x ",
	    mem->cont, mem->cont_start, mem->store.start, mem->store.size);
  printf ("\n");
}

void
hurd_store_init (struct hurd_store *store,
		 void *hook, hurd_store_fault_t fault)
{
  store->hook = hook;
  store->fault = fault;

  hurd_btree_memory_tree_init (&store->memory);
}

size_t
hurd_store_size (void)
{
  return sizeof (struct hurd_store);
}

void
hurd_store_destroy (hurd_store_t *store)
{
  /* XXX: Implement me.  What are the right semantics?  Do we
     implicitly drop all mappings or return EBUSY if there are extant
     mappings?  */
}

extern error_t
hurd_store_bind_to_vm (struct hurd_store *store, off64_t store_offset,
		       uintptr_t address, size_t size,
		       uintptr_t flags, int map_now,
		       uintptr_t *used_address)
{
  error_t err = 0;
  struct map *map;
  struct hurd_memory *memory;

  assert (mm_init_done);

  /* If HURD_VM_HERE is set, assert that the address is page aligned.  */
  if ((flags & HURD_VM_HERE) && (address & (getpagesize () - 1)))
    return EINVAL;

  /* If HURD_VM_HERE is set, assert that the address is not NULL.  */
  if ((flags & HURD_VM_HERE) && address == 0)
    return EINVAL;

  /* Is size a multiple of the page size?  */
  if ((size & (getpagesize () - 1)))
    return EINVAL;

  pthread_mutex_lock (&as.lock);

  /* Create the mapping.  */
  map = map_alloc ();
  if (! map)
    {
      pthread_mutex_unlock (&as.lock);
      return ENOMEM;
    }

  err = map_init (map, (flags & HURD_VM_HERE) ? address : 0, size,
		  store, store_offset, used_address);
  assert_perror (err);

  /* We have established the mapping.  The store will allocate the
     physical memory and map it in when required.  */

  if (map_now)
    /* But the caller wants the map now.  This is likely because this
       is start up code and the store is not yet up but it could also
       be that the caller knows that this region will be used
       immediately in which case the fault is pure overhead.  */
    {
      memory = hurd_memory_alloc (size);
      if (! memory)
	{
	  debug ("memory_alloc failed!\n");
	  pthread_mutex_unlock (&as.lock);
	  return ENOMEM;
	}
      hurd_store_cache (store, store_offset, memory);

      err = hurd_memory_map (memory, 0, map->vm.size, *used_address);
      if (err)
	debug ("memory_map failed: %d\n", err);
    }

  pthread_mutex_unlock (&as.lock);

  return err;
}

void
hurd_store_cache (struct hurd_store *store, uintptr_t store_start,
		  struct hurd_memory *memory)
{
  error_t err;

  memory->store.start = store_start;
  err = hurd_btree_memory_insert (&store->memory, memory);
  assert_perror (err);
}

struct hurd_memory *
hurd_store_find_cached (struct hurd_store *store, uintptr_t store_start,
			size_t length)
{
  struct region region = { store_start, length };
  struct hurd_memory *memory
    = hurd_btree_memory_find (&store->memory, &region);

  if (! memory)
    return NULL;

  for (;;)
    {
      struct hurd_memory *prev;

      /* If the beginning of this memory is before or at where
	 STORE_START then the memory before this one cannot overlap.
	 Hence, we are done.  */
      if (memory->store.start <= store_start)
	return memory;

      prev = hurd_btree_memory_prev (memory);

      if (prev
	  && overlap (store_start, length,
		      prev->store.start, prev->store.size))
	memory = prev;
      else
	return memory;
    }
}

void
hurd_store_flush (struct hurd_store *store,
		  uintptr_t store_start, size_t length)
{
  struct hurd_memory *next;

  assert ((store_start & (getpagesize () - 1)) == 0);
  assert ((length & (getpagesize () - 1)) == 0);

  next = hurd_store_find_cached (store, store_start, length);
  while (next)
    {
      struct hurd_memory *memory = next;

      if (memory->store.start + memory->store.size < store_start + length)
	{
	  next = hurd_btree_memory_next (memory);
	  if (next && store_start + length <= next->store.start)
	    /* We don't deallocate NEXT.  */
	    next = 0;
	}
      else
	/* MEMORY covers at least as much as we are deallocating.  */
	next = 0;

      uintptr_t start;
      if (store_start <= memory->store.start)
	start = 0;
      else
	start = store_start - memory->store.start;
      assert (start < memory->store.size);

      size_t size;
      if (store_start + length > memory->store.start + memory->store.size)
	size = memory->store.size - start;
      else
	size = store_start + length - (memory->store.start + start);

      assert (start + size <= memory->store.size);

      hurd_memory_dealloc (store, memory, start, size);
    }
}
