/* Zone allocator for physical memory server.
   Copyright (C) 2003 Free Software Foundation, Inc.
   Written by Neal H Walfield.
   Modified by Marcus Brinkmann.

   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU Hurd is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#include <assert.h>
#include <string.h>

#include "output.h"

#include "zalloc.h"


#ifndef NDEBUG
#define DODEBUG(level, func) \
  do { if ((level) <= output_debug) { func; } } while (0)
#else
#define DODEBUG(level, func) ((void) (0))
#endif


/* Zalloc: A fast zone allocator.  This is not a general purpose
   allocator.  If you attempt to use it as such, you will find that it
   is very inefficient.  It is, however, designed to be very fast and
   to be used as a base for building a more general purpose allocator.

   Memory is kept in zones.  Zones are of sizes 2 ** N and all memory
   is aligned on a similar boundary.  Typically, the smallest zone
   will be the system page size.  Memory of any size can be added to
   the pool as long as it is a multiple of the smallest zone: it is
   broken up as necessary.

   Memory can be added to the pool by calling the zfree function with
   the address of the buffer and its size.  The buffer is broken up as
   a function of its alignment and size using the buddy system (as
   described by e.g. Knuth).  Consider the following: zfree (4k, 16k).
   This says that a buffer of size 16k starting at address 4k should
   be added to the system.  Although the size of the buffer is a power
   of 2 (2 ** 14 = 16k), it cannot be added to the 16k zone: it has
   the wrong alignment.  Instead, the initial 4k are broken off, added
   to the 4k zone, the next 8k to the 8k zone and the final 4k to the
   4k zone.  If, as memory is added to a zone, its buddy is present,
   the two buffers are buddied up and promoted to the next zone.  For
   instance, if the 4k buffer at address 20k was present during the
   previous zfree, the bufer at 16k would have been combined with this
   and the new larger buffer would have been added to the 8k zone.

   When allocating memory, the smallest zone that is larger than or
   equal to the desired size is selected.  If the zone is exhausted,
   the allocator will look in the next larger zone and break up a
   buffer to satisfy the request.  This continues recursively if
   necessary.  If the desired size is smaller than the buffer that is
   selected, the difference is returned to the system.  For instance,
   if an allocation request of 12k is made, the system will start
   looking in the 16k zone.  If it finds that that zone is exhausted,
   it will select a buffer from the 32k zone and place the top half in
   the 16k zone and use the lower half for the allocation.  However,
   as this is 4k too much, the extra is returned to the 4k zone.

   When making allocations, the system will not look for adjacent
   memory blocks: if an allocation request of e.g. 8k is issued and
   there is no memory in the 8k zones and above, the 4k zone will not
   be searched for false buddies.  That is, if in the 4k zone there is
   a buffer starting at 4k and 8k, the allocator will make no effort
   to search for them.  Note that they could not have been combined
   during the zfree as 4k's buddy is at 0k and 8k's buddy is at
   12k.  */


/* A free block list ordered by address.  Blocks are of size 2 ** N
   and aligned on a similar boundary.  Since the contents of a block
   does not matter (it is free), the block itself contains this
   structure at its start address.  */
struct block
{
  struct block *next;
  struct block *prev;
};


/* Given a zone, return its size.  */
#define ZONE_SIZE(x) (1 << ((x) + L4_MIN_PAGE_SIZE_LOG2))

/* Number of zones in the system.  */
#define ZONES (sizeof (L4_Word_t) * 8 - L4_MIN_PAGE_SIZE_LOG2)

/* The zones.  */
static struct block *zone[ZONES] = { 0, };


/* Add the block BLOCK to the zone ZONE_NR.  The block has the
   right size and alignment.  Buddy up if possible.  */
static inline void
add_block (struct block *block, unsigned int zone_nr)
{
  while (1)
    {
      struct block *left = 0;
      struct block *right = zone[zone_nr];

      /* Find the left and right neighbours of BLOCK.  */
      while (right && block > right)
	{
	  left = right;
	  right = right->next;
	}

      if (left && (((l4_word_t) left) ^ ((l4_word_t) block))
	  == ZONE_SIZE (zone_nr))
	{
	  /* Buddy on the left.  */

	  /* Remove left neighbour.  */
	  if (left->prev)
	    left->prev->next = left->next;
	  else
	    zone[zone_nr] = left->next;
	  if (left->next)
	    left->next->prev = left->prev;

	  block = left;
	  zone_nr++;
	}
      else if (right && (((l4_word_t) right) ^ ((l4_word_t) block))
	       == ZONE_SIZE (zone_nr))
	{
	  /* Buddy on the right.  */

	  /* Remove right neighbour from the list.  */
	  if (right->prev)
	    right->prev->next = right->next;
	  else
	    zone[zone_nr] = right->next;
	  if (right->next)
	    right->next->prev = right->prev;
		  
	  zone_nr++;
	}
      else
	{
	  /* Could not coalesce.  Just insert.  */

	  block->next = right;
	  if (block->next)
	    block->next->prev = block;

	  block->prev = left;	  
	  if (block->prev)
	    block->prev->next = block;
	  else
	    zone[zone_nr] = block;

	  /* This is the terminating case.  */
	  break;
	}
    }
}


/* Add the block BLOCK of size SIZE to the pool.  BLOCK must be
   aligned to the system's minimum page size.  SIZE must be a multiple
   of the system's minimum page size.  */
void
zfree (l4_word_t block, l4_word_t size)
{
  l4_word_t min_page_size = l4_min_page_size ();
  
  debug ("%s: freeing block 0x%x - 0x%x\n", __func__,
	 block, block + size);

  if (size & (min_page_size - 1))
    panic ("%s: size 0x%x of freed block 0x%x is not a multiple of "
	   "minimum page size", __func__, size, block);

  if (block & (min_page_size - 1))
    panic ("%s: freed block 0x%x of size 0x%x is not aligned to "
	   "minimum page size", __func__, block, size);

  do
    {
      /* All blocks must be stored aligned to their size.  */
      unsigned int block_align = l4_lsb (block) - 1;
      unsigned int size_align = l4_msb (size) - 1;
      unsigned int zone_nr = (block_align < size_align
			      ? block_align : size_align)
	- L4_MIN_PAGE_SIZE_LOG2;

      add_block ((struct block *) block, zone_nr);

      block += ZONE_SIZE (zone_nr);
      size -= ZONE_SIZE (zone_nr);
    }
  while (size > 0);
}


/* Allocate a block of memory of size SIZE and return its address.
   SIZE must be a multiple of the system's minimum page size.  If no
   block of the required size could be allocated, return 0.  */
l4_word_t
zalloc (l4_word_t size)
{
  l4_word_t min_page_size = l4_min_page_size ();
  unsigned int zone_nr;
  struct block *block;

  debug ("%s: request for 0x%x bytes\n", __func__, size);

  if (size & (min_page_size - 1))
    panic ("%s: requested size 0x%x is not a multiple of "
	   "minimum page size", __func__, size);

  /* Calculate the logarithm to base two of SIZE rounded up to the
     nearest power of two (actually, the MSB function returns one more
     than the logarithm to base two of its argument, rounded down to
     the nearest power of two - this is the same except for the border
     case where only one bit is set.  To adjust for this border case,
     we subtract one from the argument to the MSB function).  Calculate
     the zone number by subtracting page shift.  */
  zone_nr = l4_msb (size - 1) - L4_MIN_PAGE_SIZE_LOG2;

  /* Find the smallest zone which fits the request and has memory
     available.  */
  while (!zone[zone_nr] && zone_nr < ZONES)
    zone_nr++;

  if (zone_nr == ZONES)
    return 0;

  /* Found a zone.  Now bite off the beginning of the first block in
     this zone.  */
  block = zone[zone_nr];

  zone[zone_nr] = block->next;
  if (zone[zone_nr])
    zone[zone_nr]->prev = 0;

  /* And donate back the remainder of this block, if any.  */
  if (ZONE_SIZE (zone_nr) > size)
    zfree (((l4_word_t) block) + size, ZONE_SIZE (zone_nr) - size);

  /* Zero out the newly allocated block.  */
  memset (block, 0, size);

  return (l4_word_t) block;
}


/* Dump the internal data structures.  */
#ifndef NDEBUG
void
zalloc_dump_zones (const char *prefix)
{
  l4_word_t min_page_size = l4_min_page_size ();
  int i;
  struct block *block;
  l4_word_t available = 0;
  int print_empty = 0;

  for (i = ZONES - 1; ZONE_SIZE (i) >= min_page_size; i--)
    if (zone[i] || print_empty)
      {
	print_empty = 1;
	printf ("%s: 0x%x: { ", prefix, ZONE_SIZE (i));
	for (block = zone[i]; block; block = block->next)
	  {
	    available += ZONE_SIZE (i);
	    printf ("%p%s", block, (block->next ? ", " : " "));
	  }
	printf ("}\n");
      }

  printf ("%s: %llu (0x%llx) bytes available\n", prefix,
	  (unsigned long long) available, (unsigned long long) available);
}
#endif
