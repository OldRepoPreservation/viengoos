/* map.c - Generic map implementation.
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <hurd/storage.h>
#include <hurd/as.h>
#include <hurd/slab.h>
#include <l4.h>

#include <string.h>

#include "map.h"
#include "pager.h"

static error_t
slab_alloc (void *hook, size_t size, void **ptr)
{
  struct storage storage = storage_alloc (meta_data_activity, vg_cap_page,
					  STORAGE_LONG_LIVED,
					  VG_OBJECT_POLICY_DEFAULT, VG_ADDR_VOID);
  if (VG_ADDR_IS_VOID (storage.addr))
    panic ("Out of space.");
  *ptr = VG_ADDR_TO_PTR (vg_addr_extend (storage.addr, 0, PAGESIZE_LOG2));

  return 0;
}

static error_t
slab_dealloc (void *hook, void *buffer, size_t size)
{
  assert (size == PAGESIZE);

  vg_addr_t addr = vg_addr_chop (VG_PTR_TO_ADDR (buffer), PAGESIZE_LOG2);
  storage_free (addr, false);

  return 0;
}

/* Storage descriptors are alloced from a slab.  */
static struct hurd_slab_space map_slab
 = HURD_SLAB_SPACE_INITIALIZER (struct map,
				slab_alloc, slab_dealloc, NULL, NULL, NULL);

static struct map *
map_alloc (void)
{
  void *buffer;
  error_t err = hurd_slab_alloc (&map_slab, &buffer);
  if (err)
    panic ("Out of memory!");

  memset (buffer, 0, sizeof (struct map));
  return buffer;
}

static void
map_free (struct map *storage)
{
  hurd_slab_dealloc (&map_slab, storage);
}

static void
list_link (struct map **list, struct map *e)
{
  e->map_list_next = *list;
  if (e->map_list_next)
    e->map_list_next->map_list_prevp = &e->map_list_next;
  e->map_list_prevp = list;
  *list = e;
}

static void
list_unlink (struct map *e)
{
  assert (e->map_list_prevp);

  *e->map_list_prevp = e->map_list_next;
  if (e->map_list_next)
    e->map_list_next->map_list_prevp = e->map_list_prevp;

#ifndef NDEBUG
  /* Try to detect multiple unlink.  */
  e->map_list_next = NULL;
  e->map_list_prevp = NULL;
#endif
}

ss_mutex_t maps_lock;
hurd_btree_map_t maps;

static bool
map_install (struct map *map)
{
  assert (! ss_mutex_trylock (&maps_lock));
  assert (! ss_mutex_trylock (&map->pager->lock));

  /* No zero length maps.  */
  assert (map->region.length > 0);
  /* Multiples of page size.  */
  assert ((map->region.start & (PAGESIZE - 1)) == 0);
  assert ((map->region.length & (PAGESIZE - 1)) == 0);
  assert ((map->offset & (PAGESIZE - 1)) == 0);

  assert (map->offset + map->region.length <= map->pager->length);

  assert ((map->access & ~MAP_ACCESS_ALL) == 0);


  debug (5, "Installing %c%c map at %x+%x(%x) referencing %p starting at %x",
	 map->access & MAP_ACCESS_READ ? 'r' : '~',
	 map->access & MAP_ACCESS_WRITE ? 'w' : '~',
	 map->region.start, map->region.start + map->region.length,
	 map->region.length,
	 map->pager, map->offset);


  /* Insert into the mapping database.  */
  struct map *conflict = hurd_btree_map_insert (&maps, map);
  if (conflict)
    {
      debug (0, "Can't install map at %x+%d; conflicts "
	     "with map at %x+%d",
	     map->region.start, map->region.length,
	     conflict->region.start, conflict->region.length);
      return false;
    }
  map->connected = true;

  /* Attach to its pager.  */
  list_link (&map->pager->maps, map);

  return true;
}

struct map *
map_create (struct region region, enum map_access access,
	    struct pager *pager, uintptr_t offset,
	    map_destroy_t destroy)
{
  maps_lock_lock ();
  ss_mutex_lock (&pager->lock);

  struct map *map = map_alloc ();

  map->region = region;
  map->pager = pager;
  map->offset = offset;
  map->access = access;
  map->destroy = destroy;

  if (! map_install (map))
    {
      map_free (map);
      map = NULL;
    }

  ss_mutex_unlock (&pager->lock);
  maps_lock_unlock ();

  return map;
}


void
map_disconnect (struct map *map)
{
  assert (! ss_mutex_trylock (&maps_lock));

  assert (map->connected);
  hurd_btree_map_detach (&maps, map);
  map->connected = false;
}

void
map_destroy (struct map *map)
{
  ss_mutex_lock (&map->pager->lock);

  assert (! map->connected);

  /* Drop our reference.  */
  assert (map->pager->maps);

  if (map->destroy)
    map->destroy (map);

  if (map->pager->maps == map && ! map->map_list_next)
    /* This is the last reference.  */
    {
      map->pager->maps = NULL;

      if (map->pager->no_refs)
	map->pager->no_refs (map->pager);
      else
	ss_mutex_unlock (&map->pager->lock);
    }
  else
    {
      list_unlink (map);

      ss_mutex_unlock (&map->pager->lock);
    }

  map_free (map);
}


struct map *
map_split (struct map *map, uintptr_t offset)
{
  assert (! ss_mutex_trylock (&maps_lock));

  debug (5, "splitting %x+%x at %x", 
	 map->region.start, map->region.length, offset);

  if (offset <= 0 || offset >= map->region.length)
    {
      debug (0, "Invalid offset into map.");
      return NULL;
    }

  if (offset % PAGESIZE != 0)
    {
      debug (0, "Offset does not designate a sub-tree boundary.");
      return NULL;
    }

  struct map *second = map_alloc ();
  *second = *map;

  second->region.start = map->region.start + offset;
  second->region.length = map->region.length - offset;
  second->offset += offset;

  /* This is kosher: it does not change the ordering of the mapping in
     the tree.  */
  map->region.length = offset;

  debug (5, "%x+%x, %x+%x",
	 map->region.start, map->region.length,
	 second->region.start, second->region.length);


  /* This can't fail.  We have the lock and we just made the space
     available.  */
  ss_mutex_lock (&second->pager->lock);
#ifndef NDEBUG
  memset (&second->node, 0, sizeof (second->node));
#endif
  bool ret = map_install (second);
  assert (ret);
  ss_mutex_unlock (&second->pager->lock);

  return second;
}

bool
map_join (struct map *first, struct map *second)
{
  assert (! ss_mutex_trylock (&maps_lock));

  if (first->pager != second->pager)
    return false;

  if (first->access != second->access)
    return false;

  if (first->region.start + first->region.length != second->region.start)
    /* SECOND does not follow FIRST.  */
    {
      if (second->region.start + second->region.length == first->region.start)
	/* But FIRST does follow SECOND.  Swap.  */
	{
	  struct map *t = first;
	  first = second;
	  second = t;
	}
      else
	return false;
    }

  uintptr_t length = second->region.length;

  map_disconnect (second);

  /* It is kosher to call this with MAPS_LOCK held as FIRST is another
     reference.  */
  map_destroy (second);

  /* We can only update this after we detach SECOND from the tree.  */
  first->offset += length;

  return true;
}

bool
map_fault (vg_addr_t fault_addr, uintptr_t ip, struct vg_activation_fault_info info)
{
  /* Find the map.  */
  struct region region;

  if (vg_addr_depth (fault_addr) == VG_ADDR_BITS - PAGESIZE_LOG2)
    fault_addr = vg_addr_extend (fault_addr, 0, PAGESIZE_LOG2);

  region.start = (uintptr_t) VG_ADDR_TO_PTR (fault_addr);
  region.length = 1;

  maps_lock_lock ();

  struct map *map = map_find (region);
  if (! map)
    {
      do_debug (5)
	{
	  debug (0, "No map covers " VG_ADDR_FMT "(" VG_ACTIVATION_FAULT_INFO_FMT ")",
		 VG_ADDR_PRINTF (fault_addr),
		 VG_ACTIVATION_FAULT_INFO_PRINTF (info));
	  for (map = hurd_btree_map_first (&maps);
	       map;
	       map = hurd_btree_map_next (map))
	    debug (0, MAP_FMT, MAP_PRINTF (map));
	}

      maps_lock_unlock ();
      return false;
    }

  /* Note: write access implies read access.  */
  if (((info.access & L4_FPAGE_WRITABLE) && ! (map->access & MAP_ACCESS_WRITE))
      || ! map->access)
    {
      debug (0, "Invalid %s access at " VG_ADDR_FMT ": " MAP_FMT,
	     info.access & L4_FPAGE_WRITABLE ? "write" : "read",
	     VG_ADDR_PRINTF (fault_addr), MAP_PRINTF (map));

      maps_lock_unlock ();
      return false;
    }

  struct pager *pager = map->pager;
  uintptr_t offset = map->offset + (region.start - map->region.start);
  bool ro = (map->access & MAP_ACCESS_WRITE) ? false : true;

  maps_lock_unlock ();

  /* Propagate the fault.  */
  bool r = pager->fault (pager, offset, 1, ro,
			 (uintptr_t) VG_ADDR_TO_PTR (fault_addr), ip, info);
  if (! r)
    debug (5, "Map did not resolve fault at " VG_ADDR_FMT,
	   VG_ADDR_PRINTF (fault_addr));

  return r;
}
