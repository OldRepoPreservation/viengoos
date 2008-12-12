/* map.h - Generic map interface.
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

#ifndef _HURD_MAP_H
#define _HURD_MAP_H

#include <hurd/btree.h>
#include <hurd/addr.h>
#include <hurd/mutex.h>
#include <hurd/as.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

/* The managed part of the address space is a set of regions.  Each
   region corres to a map.  A map references a pager.  A pager obtains
   and maps data on demand.

   When a thread faults (accesses an address for which there is no
   valid translation, for which the access is not allowed by the
   mapping, or for which the object has been discarded), the thread is
   activated with a message describing the fault.  It finds the map
   that corresponds to the address in question and invokes the
   associated pager's fault handler.  The pager then creates valid
   mappings.  */

/* Regions.  */
struct region
{
  uintptr_t start;
  uintptr_t length;
};

#define REGION_FMT "%x-%x(%x)"
#define REGION_PRINTF(region) \
  (region).start, ((region).start + (region).length - 1), (region).length

/* Compare two regions.  Two regions are considered equal if there is
   any overlap at all.  */
static int
region_compare (const struct region *a, const struct region *b)
{
  uintptr_t a_end = a->start + (a->length - 1);
  uintptr_t b_end = b->start + (b->length - 1);

  if (a_end < b->start)
    return -1;
  if (a->start > b_end)
    return 1;
  /* Overlap.  */
  return 0;
}

/* Forward.  */
struct pager;
struct map;


enum map_access
  {
    MAP_ACCESS_NONE = 0,
    MAP_ACCESS_READ = 1 << 0,
    MAP_ACCESS_WRITE = 1 << 1,

    MAP_ACCESS_ALL = MAP_ACCESS_READ | MAP_ACCESS_WRITE,
  };

/* Call-back invoked when destroying a map.  */
typedef void (*map_destroy_t) (struct map *map);

struct map
{
  /* All fields are protected by the map_lock lock.  */

  union
  {
    /* Node */
    hurd_btree_node_t node;
    struct map *next;
  };
  /* Whether the map is connected to MAPS.  */
  bool connected;

  /* The corresponding pager.  */
  struct pager *pager;

  /* The offset into the pager.  */
  uintptr_t offset;

  /* The virtual addresses this map covers.  Only a mapping has been
     installed, this may only be changed using map_split.  */
  struct region region;

  /* The access this map grants.  */
  enum map_access access;

  /* Each map is attached to its pager's list of maps.  */
  struct map *map_list_next;
  struct map **map_list_prevp;


  map_destroy_t destroy;
};

#define MAP_FMT REGION_FMT " @ %p+%x (%s%s)"
#define MAP_PRINTF(map)						\
  REGION_PRINTF ((map)->region),				\
    (map)->pager, (map)->offset,				\
    (map)->access & MAP_ACCESS_READ ? "r" : "~",		\
    (map)->access & MAP_ACCESS_WRITE ? "w" : "~"

BTREE_CLASS (map, struct map, struct region, region, node,
	     region_compare, false)

/* All maps.  This data structure is protected by the map_lock
   lock.  */
extern hurd_btree_map_t maps;

#include <hurd/exceptions.h>

/* Ensure that using the next AMOUNT bytes of stack will not result in
   a fault.  */
static void __attribute__ ((noinline))
map_lock_ensure_stack (int amount)
{
  volatile int space[amount / sizeof (int)];

  /* XXX: The stack grows up case should reverse this loop.  (Think
     about the order of the faults and how the exception handler 1)
     special cases stack faults, and 2) uses this function when it
     inserts a page.)  */
  int i;
  for (i = sizeof (space) / sizeof (int) - 1;
       i > 0;
       i -= PAGESIZE / sizeof (int))
    space[i] = 0;
}

/* Protects MAPS and all map's connected to MAPS.  A pager's list of
   maps is protected by its lock field.  The locking order is
   MAPS_LOCK and then MAP->PAGER->LOCK.  */
static inline void
maps_lock_lock (void)
{
  extern ss_mutex_t maps_lock;

  map_lock_ensure_stack (AS_STACK_SPACE);

  ss_mutex_lock (&maps_lock);
}

static inline void
maps_lock_unlock (void)
{
  extern ss_mutex_t maps_lock;

  ss_mutex_unlock (&maps_lock);
}

/* Map the region REGION with access described by ACCESS to the pager
   PAGER starting at offset OFFSET.  DESTROY is a callback called just
   before the map is fully destroyed.  It may be NULL.  Maps may not
   overlap.  Returns true on success, false otherwise.  This function
   takes and releases MAPS_LOCK and PAGER->LOCK.  */
extern struct map *map_create (struct region region, enum map_access access,
			       struct pager *pager, uintptr_t offset,
			       map_destroy_t destroy);

/* Disconnect the map MAP from MAPS.  MAP will no longer resolve
   faults, however, any previously mapped pages may remain accessible.
   The caller must hold MAPS_LOCK.  This function does not remove the
   map from MAP->PAGERS list.  This is done by map_destroy
   function.  */
extern void map_disconnect (struct map *map);

/* Destroy the map MAP.  map_disconnect must have already been called.
   The caller must NOT hold MAPS_LOCK.  This function takes and
   releases MAP->PAGER->LOCK.  */
extern void map_destroy (struct map *map);

/* Return a map that overlaps with the region REGION.  The caller must
   hold MAPS_LOCK.  */
static inline struct map *
map_find (struct region region)
{
  extern ss_mutex_t maps_lock;
  assert (! ss_mutex_trylock (&maps_lock));

  struct map *map = hurd_btree_map_find (&maps, &region);
  if (! map)
    {
      debug (3, "No map covers %x-%x",
	     region.start, region.start + region.length - 1);
      return NULL;
    }

  return map;
}

/* Split the map MAP at offset OFFSET.  MAP will cover the region from
   [0, offset) and the returned map the region [offset, length).  The
   caller must hold MAPS_LOCK.  This function takes and releases
   MAP->PAGER->LOCK.  */
extern struct map *map_split (struct map *map, uintptr_t offset);

/* Join FIRST and SECOND if possible (i.e., they reference the same
   pager, one comes after the other, and they have the same access
   permissions).  Returns success on true, false otherwise.  On
   success, either FIRST or SECOND was destroyed.  Both pointers
   should be considered invalid.  The caller must hold MAPS_LOCK.
   This function takes and releases MAP->PAGER->LOCK.  */
extern bool map_join (struct map *first, struct map *second);

/* Raise a fault at address ADDR.  Returns true if the fault was
   handled, false otherwise.  */
extern bool map_fault (addr_t addr,
		       uintptr_t ip, struct activation_fault_info info);

#endif
