/* priv.h - Internal definition.
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

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <l4.h>
#include <hurd/btree.h>
#include <hurd/slab.h>

#include "physmem-user.h"

/* XXX: Debugging output is nice.  We know that all users of this
   library provide this so we make use of it.  */
extern int printf (const char *fmt, ...);

/* True if debug mode is enabled.  */
extern int output_debug;

/* Print a debug message.  */
#define debug(fmt, ...)					\
  ({							\
    if (output_debug)					\
      printf ("%s:%s: " fmt, __FILE__,			\
	      __FUNCTION__, ##__VA_ARGS__);		\
  })

#define panic(fmt, ...)					\
  ({							\
    printf ("Panic in %s:%s: " fmt, __FILE__,		\
	    __FUNCTION__, ##__VA_ARGS__);		\
    while (1)						\
      l4_yield ();					\
  })

/* Return true if INDEX lies within (START, START+SIZE-1)
   inclusive.  */
static inline bool
within (uintptr_t index, uintptr_t start, size_t size)
{
  return index >= start && index < start + size;
}

/* Return true if (INDEX1, INDEX1+SIZE1-1) inclusive overlaps with
   (INDEX2, INDEX2+SIZE2-1) inclusive.  */
static inline bool
overlap (uintptr_t index1, size_t size1, uintptr_t index2, size_t size2)
{
  return
    /* Is the start of the first region within the second?
          2  1  2  1
       or 2  1  1  2  */
    within (index1, index2, size2)
    /* Is the end of the first region within the second?
          1  2  1  2
       or 2  1  1  2  */
    || within (index1 + size1 - 1, index2, size2)
    /* Is start of the second region within the first?
          1  2  1  2
       or 1  2  2  1 */
    || within (index2, index1, size1);

  /* We have implicitly checked if the end of the second region is
     within the first (i.e. within (index2 + size2 - 1, index1, size1))
          2  1  2  1
       or 1  2  2  1
     in check 1 and check 3.  */
}

/* A region of memory.  */
struct region
{
  /* Start of the region.  */
  uintptr_t start;
  /* And its extent.  */
  size_t size;
};

/* If A and B overlap at all they are equal.  A region is less than a
   second region if the start of the first if before the second.  */
static int
region_compare (const struct region *a, const struct region *b)
{
  if (overlap (a->start, a->size, b->start, b->size))
    return 0;
  else
    return a->start - b->start;
}

/* Forward.  */
struct store;

/* Memory which is in core (referred to by struct stores).  */
struct frame
{
  hurd_btree_node_t node;

  /* The start of the memory block in the DEFAULT_CONTAINER.  */
  uintptr_t dc_start;

  /* The start of the data that this memory block caches on the
     containing store.  */
  struct region store;

  /* The number of users.  We need to keep track of this so that if a
     piece of memory is mapped twice we don't deallocate the memory
     after the first time it is unmapped.  This rarely happens and
     likely wouldn't have horrible consequences if we just dropped the
     memory and it was on backing store, however, for anonymous memory
     this might not be the case.  */
  int refs;
};

BTREE_CLASS(frame, struct frame, struct region, store, node,
	    region_compare)

extern struct hurd_slab_space frame_slab;

/* Initialize the frame structure in *FRAME to reference SIZE bytes of
   physical memory.  DC_START specifies the start of a free region in
   the default container continuing for SIZE bytes.  The returned
   frame needs to be integrated into the appropriate backing store
   (using frame_insert).

   (NB this function is only used when the start cannot be calculated
   using the normal algorithms because the mapping database is not yet
   up.)  */
extern error_t frame_alloc_into (struct frame *frame, uintptr_t dc_start,
				 size_t size);

/* Allocate a frame structure and set it up to reference size SIZE
   bytes of physical memory (somewhere in the default container).
   Return NULL on failure.  The returned frame needs to be integrated
   into the appropriate backing store (using frame_insert).  */
extern struct frame *frame_alloc (size_t size);

/* Record that frame FRAME caches data on store STORE starting at
   byte STORE_START.  */
extern void frame_insert (struct store *store,
			  uintptr_t store_start, struct frame *frame);

/* Find the first frame (i.e. with lowest start address) which
   includes some part of the region in store STORE starting at
   STORE_START and spanning LENGTH bytes.  If no frame covers any of
   the region, return NULL.

   NB: No two established maps will overlap internally, however, it is
   possible that the region one is looking for covers multiple
   maps.  */
extern struct frame *frame_find_first (struct store *store,
				       uintptr_t store_start, size_t length);

/* Map the contents of frame FRAME starting at offset OFFSET extending
   LENGTH bytes to address virtual ADDR.  (NB: OFFSET is relative to
   the frame base.  Hence to map the entire frame, one would pass
   OFFSET=0 and LENGTH=FRAME->STORE.SIZE.)  OFFSET must be aligned on
   a multiple of the base page size and LENGTH must be a multiple of
   the base page size.  */
extern error_t frame_map (struct frame *frame, size_t offset, size_t length,
			  uintptr_t addr);

/* Deallocate any physical memory caching the region on store STORE
   cover the byte START and continuing for LENGTH bytes.  STORE_START
   must be aligned on a base page size boundary and size must be a
   multiple of the base page size.  */
extern void frame_dealloc (struct store *store,
			   uintptr_t store_start, size_t length);

/* A backing store.  */
struct store
{
  /* The server.  */
  l4_thread_id_t server;
  /* The capability handle.  */
  hurd_cap_handle_t handle;

  /* The parts of this backing store current in core.  */
  hurd_btree_frame_t frames;
};

extern struct store swap_store;

/* Find a free region in a container for a region of size SIZE with
   alignment ALIGN.  Must be called with the map lock.  The lock must
   not be dropped until the virtual address is entered into the
   mapping database (and this function should not be called again
   until that has happened).  */
extern uintptr_t store_find_free (struct store *store, size_t size,
				  size_t align);

/* A memory map.  */
struct map
{
  hurd_btree_node_t node;

  /* The virtual address the map covers.  */
  struct region vm;
  /* The backing store.  */
  struct store *store;
  /* The starting address on the backing store of the start of the data.  */
  size_t store_offset;
};

BTREE_CLASS(map, struct map, struct region, vm, node, region_compare)

struct as
{
  /* Map from the address space to backing store.  */
  hurd_btree_map_t mappings;

  pthread_mutex_t lock;
};

/* The task's address space.  */
extern struct as as;

/* The physical memory manager's server thread.  */
extern l4_thread_id_t physmem;

/* By default, we allocate all anonymous memory in this container.  */
extern hurd_pm_container_t default_container;

/* Set by map_init to true immediately before it returns.  */
extern int mm_init_done;

extern struct hurd_slab_space map_slab;

/* Initialize the mapping database.  */
extern void map_init (void);

/* Allocate a map structure.  MAP_LOCK must be held.  The returned
   map structure is not part of the map list; it may be added by
   calling map_insert.  */
extern struct map *map_alloc (void);

extern void map_dealloc (struct map *map);

/* Add the map structure MAP (previously allocated by map_alloc) to
   the mapping database.  MAP_LOCK must be held.  */
extern void map_insert (struct map *map);

/* Remove the map structre MAP from the mapping database.  This does
   not deallocate the memory the data structure is using.  */
extern void map_detach (struct map *map);

/* Find the first map (i.e. with lowest start address) which includes
   some part of the region starting at ADDRESS and spanning SIZE
   bytes.  If no map covers any of the region, returns NULL.

   NB: No two established maps will overlap internally, however, it is
   possible that the region one is looking for covers multiple
   maps.  */
extern struct map *map_find (uintptr_t address, size_t size);

/* Find a free region of the virtual address space for a region of
   size SIZE with alignment ALIGN.  Must be called with the map lock.
   The lock msut not be dropped until the virtual address is entered
   into the mapping database (and this function should not be called
   again until that has happened).  */
extern uintptr_t map_find_free (size_t size, size_t align);

/* Find a free region in a container for a region of size SIZE with
   alignment ALIGN.  Must be called with the map lock.  The lock msut
   not be dropped until the virtual address is entered into the
   mapping database (and this function should not be called again
   until that has happened).  */
extern uintptr_t container_find_free (l4_thread_id_t server,
				      hurd_cap_handle_t handle,
				      size_t size, size_t align);


/* By default, the slab allocator uses mmap to allocate its buffers.
   This won't work for us as we have special requirements: our data
   structures cannot be paged and they must be able to allocate memory
   before mmap even works.  */
extern error_t mem_slab_allocate_buffer (void *hook, size_t size,
					 void **ptr);

extern error_t mem_slab_deallocate_buffer (void *hook, void *buffer,
					   size_t size);

/* In order to initialize the mapping database, we need to get memory
   from physmem for the map slab.  This means that the allocated
   buffer cannot be added to the mapping database until after the
   database has been initialized.  We keep a single static buffer here
   which should be enough to get up and running.

   We have a similar problem when we allocate a map item and there is
   no more memory in the slab for it.  In this case, we need to
   allocate memory from physmem and at the same time allocate an
   additional map structure to hold the new mapping.  MAP_SPARE is
   used for this and MAP_SPARE_INTEGRATE is set to true.  When
   map_alloc calls hurd_slab_alloc, it first checks
   MAP_SPARE_INTEGRATE.  If set it is set, it copies the data from the
   spare to this map, adds it to the map list and allocates another
   map.  */
extern bool map_spare_integrate;
extern struct map map_spare;
extern struct frame frame_spare;

/* Pager fault handler.  */
extern void pager (void);
