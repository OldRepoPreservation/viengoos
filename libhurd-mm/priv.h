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

#include "vm.h"
#include "mm.h"
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

#define here() debug ("\n");

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

/* Physical memory which is caching backing store (referred to by
   stores).  */
struct hurd_memory
{
  hurd_btree_node_t node;

  /* Container containing the memory.  */
  hurd_pm_container_t cont;
  /* The start of the memory in CONT.  */
  uintptr_t cont_start;

  /* The location of the memory block on the backing store.  */
  struct region store;
};

BTREE_CLASS(memory, struct hurd_memory, struct region, store, node,
	    region_compare)

extern struct hurd_slab_space memory_slab;

/* Initialize the memory structure in *MEMORY to reference SIZE bytes of
   physical memory.  The returned memory needs to be integrated (using
   memory_insert) into store whose data it is caching.

   (NB: this function is only used when the MEMORY allocated from the
   slab because we are in the process of expanding the slab.)  */
extern error_t memory_alloc_into (struct hurd_memory *memory, size_t size);

/* A store.  */
struct hurd_store
{
  /* A hook for the call backs.  */
  void *hook;

  /* Store call backs.  */
  hurd_store_fault_t fault;

  /* The parts of this backing store currently cached in core.  */
  hurd_btree_memory_t memory;
};

/* A memory map.  */
struct map
{
  hurd_btree_node_t node;

  /* The virtual address the map covers.  */
  struct region vm;
  /* The store for this region.  */
  struct hurd_store *store;
  /* Offset of start of region on the store (only meaningful to the
     store?).  */
  off64_t store_offset;
};

BTREE_CLASS(map, struct map, struct region, vm, node, region_compare)

struct as
{
  /* Map from the address space to backing store.  */
  hurd_btree_map_t mappings;

  pthread_mutex_t lock;
};

/* The physical memory manager's server thread.  */
extern l4_thread_id_t physmem;

/* Set by map_init to true immediately before it returns.  */
extern int mm_init_done;

extern struct hurd_slab_space map_slab;

/* Allocate a map structure.  MAP_LOCK must be held.  The returned map
   structure is uninitialized.  */
extern struct map *map_alloc (void);

/* Free an unused map structure.  */
extern void map_free (struct map *map);

/* Initialize the previously uninitialized map structure MAP.
   Associate the virtual memory starting at address VM_ADDR and
   continuing for SIZE bytes with store STORE starting at byte
   STORE_OFFSET and continuing for SIZE bytes.  If vm_addr is 0, an
   address is allocated dynamically.  Inserts MAP into the address
   space.  If VM_USED_ADDR is not NULL, the virtual memory address is
   saved in *VM_USED_ADDR.  */
extern error_t map_init (struct map *map,
			 uintptr_t vm_addr, size_t size,
			 hurd_store_t *store, size_t store_offset,
			 uintptr_t *vm_used_addr);

/* Attach the map structure MAP (previously allocated by map_alloc) to
   address space.  MAP_LOCK must be held.  */
extern void map_insert (struct map *map);

/* Detach the map structre MAP from the address space.  This does not
   deallocate the memory the data structure is using.  */
extern void map_detach (struct map *map);

/* Find the first map (i.e. with lowest start address) which includes
   some part of the region starting at ADDRESS and spanning SIZE
   bytes.  If no map covers any of the region, returns NULL.

   NB: No two established maps will overlap internally, however, it is
   possible that the region one is looking for covers multiple
   maps.  */
extern struct map *map_find_first (uintptr_t address, size_t size);

/* We don't want to start mapping things at 0 as then we don't catch
   NULL pointer dereferences.  Reserving about 16k makes sense.  */
#define VIRTUAL_MEMORY_START 0x4000

/* The task's address space.  */
extern struct as as;

/* Find a free region of the virtual address space for a region of
   size SIZE with alignment ALIGN.  Must be called with the map lock.
   The lock msut not be dropped until the virtual address is entered
   into the mapping database (and this function should not be called
   again until that has happened).  */
extern uintptr_t as_find_free (size_t size, size_t align);

/* By default, the slab allocator uses mmap to allocate its buffers.
   This won't work for us as we have special requirements: our data
   structures cannot be paged and they must be able to allocate memory
   before mmap even works.  */
extern error_t core_slab_allocate_buffer (void *hook, size_t size,
					  void **ptr);

extern error_t core_slab_deallocate_buffer (void *hook, void *buffer,
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
extern struct hurd_memory memory_spare;

/* Pager fault handler.  */
extern void pager (void);

/* Memory used by the pager proper.  Completely unpaged.  */
extern struct hurd_store core_store;
