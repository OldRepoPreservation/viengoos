/* memory.c - Basic memory management routines.
   Copyright (C) 2007, 2008 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 3 of the
   License, or (at your option) any later version.

   The GNU Hurd is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see
   <http://www.gnu.org/licenses/>.  */

#include "memory.h"
#include "pager.h"
#include "activity.h"
#include "zalloc.h"

#include <string.h>

#include <l4.h>
#include <hurd/btree.h>
#include <hurd/stddef.h>

#ifdef _L4_TEST_ENVIRONMENT
#include <sys/mman.h>
#else
#include "sigma0.h"
#endif

/* This can account 4G pages * 4k = 16TB of memory.  */
uint32_t memory_total;

l4_word_t first_frame;
l4_word_t last_frame;

struct region
{
  l4_word_t start;
  l4_word_t end;
};

struct reservation
{
  struct region region;
  enum memory_reservation type;
  /* If this descriptor is allocated.  */
  int used;

  hurd_btree_node_t node;
};

#define MAX_RESERVATIONS 128
static struct reservation reservation_pool[128];

static int
reservation_node_compare (const struct region *a,
			  const struct region *b)
{
  if (a->end < b->start)
    /* A ends before B starts.  */
    return -1;
  if (b->end < a->start)
    /* B ends before A starts.  */
    return 1;
  /* Overlap.  */
  return 0;
}

BTREE_CLASS (reservation, struct reservation, struct region, region, node,
	     reservation_node_compare, false);

static hurd_btree_reservation_t reservations;

bool
memory_reserve (l4_word_t start, l4_word_t end,
		enum memory_reservation type)
{
  assert (start < end);

  struct region region = { start, end };

  debug (5, "Reserving region 0x%x-0x%x (%d)", start, end, type);

  /* Check for overlap.  */
  struct reservation *overlap
    = hurd_btree_reservation_find (&reservations, &region);
  if (overlap)
    {
      debug (5, "Region 0x%x-0x%x overlaps with region 0x%x-0x%x",
	     start, end, overlap->region.start, overlap->region.end);
      return false;
    }

  /* See if we can coalesce.  */
  region.start --;
  region.end ++;
  overlap = hurd_btree_reservation_find (&reservations, &region);
  if (overlap)
    {
      struct reservation *right;
      struct reservation *left;

      if (overlap->region.start == end + 1)
	/* OVERLAP starts just after END.  */
	{
	  right = overlap;
	  left = hurd_btree_reservation_prev (overlap);
	}
      else
	/* OVERLAP starts just before START.  */
	{
	  assert (overlap->region.end + 1 == start);
	  left = overlap;
	  right = hurd_btree_reservation_next (overlap);
	}

      int coalesced = 0;

      if (right->region.start == end + 1
	  && right->type == type)
	/* We can coalesce with RIGHT.  */
	{
	  debug (5, "Coalescing with region 0x%x-0x%x",
		 right->region.start, right->region.end);
	  right->region.start = start;
	  coalesced = 1;
	}
      else
	right = NULL;

      if (left->region.end + 1 == start
	  && left->type == type)
	/* We can coalesce with LEFT.  */
	{
	  debug (5, "Coalescing with region 0x%x-0x%x",
		 left->region.start, left->region.end);

	  if (right)
	    {
	      /* We coalesce with LEFT and RIGHT.  */
	      left->region.end = overlap->region.end;

	      /* Deallocate RIGHT.  */
	      hurd_btree_reservation_detach (&reservations, right);
	      right->used = 0;
	    }
	  left->region.end = end;

	  coalesced = 1;
	}

      if (coalesced)
	return true;
    }

  /* There are no regions with which we can coalesce.  Allocate a new
     descriptor.  */
  int i;
  for (i = 0; i < MAX_RESERVATIONS; i ++)
    if (! reservation_pool[i].used)
      {
	reservation_pool[i].used = 1;
	reservation_pool[i].region.start = start;
	reservation_pool[i].region.end = end;
	reservation_pool[i].type = type;

	struct reservation *r
	  = hurd_btree_reservation_insert (&reservations,
					   &reservation_pool[i]);
	if (r)
	  panic ("Error inserting reservation!");
	return true;
      }

  panic ("No reservation descriptors available.");
  return false;
}

void
memory_reserve_dump (void)
{
  debug (3, "Reserved regions:");
  struct reservation *r;
  for (r = hurd_btree_reservation_first (&reservations);
       r; r = hurd_btree_reservation_next (r))
    debug (3, " 0x%x-0x%x", r->region.start, r->region.end);
}

bool
memory_is_reserved (l4_word_t start, l4_word_t end,
		    l4_word_t *start_reservation,
		    l4_word_t *end_reservation)
{
  assert (start < end);

  struct region region = { start, end };

  struct reservation *overlap = hurd_btree_reservation_find (&reservations,
							     &region);
  if (! overlap)
    /* No overlap.  */
    return false;

  /* Find the first region that overlaps with REGION.  */
  struct reservation *prev = overlap;
  do
    {
      overlap = prev;
      prev = hurd_btree_reservation_prev (overlap);
    }
  while (prev && prev->region.end > start);

  debug (5, "Region 0x%x-0x%x overlaps with reserved region 0x%x-0x%x",
	 start, end, overlap->region.start, overlap->region.end);

  *start_reservation
    = overlap->region.start > start ? overlap->region.start : start;
  *end_reservation = overlap->region.end < end ? overlap->region.end : end;
  return true;
}

/* Add the memory starting at byte START and continuing until byte END
   to the free pool.  START must name the first byte in a page and END
   the last.  */
static void
memory_add (l4_word_t start, l4_word_t end)
{
  assert ((start & (PAGESIZE - 1)) == 0);
  assert ((end & (PAGESIZE - 1)) == (PAGESIZE - 1));

  debug (5, "Request to add physical memory 0x%x-0x%x", start, end);

  if (start == 0)
    /* Just drop the page at address 0.  */
    {
      start += PAGESIZE;
      debug (5, "Ignoring page at address 0");
    }

  l4_word_t start_reservation;
  l4_word_t end_reservation;

  while (start < end)
    {
      if (! memory_is_reserved (start, end,
				&start_reservation, &end_reservation))
	start_reservation = end_reservation = end + 1;
      else
	/* Round the start of the reservation down.  */
	{
	  start_reservation &= ~(PAGESIZE - 1);
	  debug (5, "Not adding reserved memory 0x%x-0x%x",
		 start_reservation, end_reservation);
	}

      if (start_reservation - start > 0)
	{
	  debug (5, "Adding physical memory 0x%x-0x%x",
		 start, start_reservation - 1);
	  zfree (start, start_reservation - start);

	  memory_total += (start_reservation - start) / PAGESIZE;
	}

      /* Set START to first page after the end of the reservation.  */
      start = (end_reservation + PAGESIZE - 1)
	& ~(PAGESIZE - 1);
    }
}

void
memory_reservation_clear (enum memory_reservation type)
{
  struct reservation *r = hurd_btree_reservation_first (&reservations);
  while (r)
    {
      struct reservation *next = hurd_btree_reservation_next (r);

      if (r->type == type)
	/* We can clear this reserved region.  */
	{
	  hurd_btree_reservation_detach (&reservations, r);
	  r->used = 0;

	  memory_add (r->region.start & ~(PAGESIZE - 1),
		      (r->region.end & ~(PAGESIZE - 1))
		      + PAGESIZE - 1);
	}

      r = next;
    }
}

void
memory_grab (void)
{
  bool first = true;

  void add (l4_word_t addr, l4_word_t length)
    {
      if (first || addr < first_frame)
	first_frame = addr;
      if (first || addr + length - PAGESIZE > last_frame)
	last_frame = addr + length - PAGESIZE;
      if (first)
	first = false;

      memory_add (addr, addr + length - 1);
    }

#ifdef _L4_TEST_ENVIRONMENT
#define SIZE 8 * 1024 * 1024
  void *m = mmap (NULL, SIZE, PROT_READ | PROT_WRITE,
		  MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (m == MAP_FAILED)
    panic ("No memory: %m");
  assert_perror (errno);
  add ((l4_word_t) m, SIZE);

#else
  l4_word_t s;
  l4_fpage_t fpage;

#if 0
  /* We need the dirty bits at a page granularity due to our own
     references.  This unfortunately means no large pages.  */

  /* Try with the largest fpage possible.  */
  for (s = L4_WORDSIZE - 1; s >= l4_min_page_size_log2 (); s --)
    ...;
#endif

  s = l4_min_page_size_log2 ();
  /* Keep getting pages of size 2^S.  */
  while (! l4_is_nil_fpage (fpage = sigma0_get_any (s)))
    /* FPAGE is an fpage of size 2^S.  Add each non-reserved base
       frame to the free list.  */
    add (l4_address (fpage), l4_size (fpage));
#endif

#ifndef NDEBUG
  do_debug (3)
    zalloc_dump_zones (__func__);
#endif
}

uintptr_t
memory_frame_allocate (struct activity *activity)
{
  uintptr_t f = zalloc (PAGESIZE);
  if (! f)
    {
      bool collected = false;

      for (;;)
	{
	  /* Check if there are any pages on the available list.  */
	  ss_mutex_lock (&lru_lock);

	  /* XXX: We avoid objects that require special treatment.
	     Realize this special treatment.  */
	  struct object_desc *desc = available_list_tail (&available);
	  while (desc)
	    {
	      if (desc->type != cap_activity_control
		  && desc->type != cap_thread)
		break;

	      desc = available_list_prev (desc);
	    }

	  if (desc)
	    {
	      assert (desc->live);
	      assert (desc->eviction_candidate);
	      assert (desc->activity);
	      assert (object_type ((struct object *) desc->activity)
		      == cap_activity_control);
	      assert (! desc->dirty || desc->policy.discardable);

	      struct object *object = object_desc_to_object (desc);
	      memory_object_destroy (activity, object);

	      f = (uintptr_t) object;
	    }

	  ss_mutex_unlock (&lru_lock);

	  if (f || collected)
	    break;

	  /* Try collecting.  */
	  pager_collect ();
	  collected = true;
	}
    }

  if (! f)
    panic ("Out of memory");

  memset ((void *) f, 0, PAGESIZE);
  return f;
}

void
memory_frame_free (l4_word_t addr)
{
  /* It better be page aligned.  */
  assert ((addr & (PAGESIZE - 1)) == 0);
  /* It better be memory we know about.  */
  assert (first_frame <= addr);
  assert (addr <= last_frame);

  zfree (addr, PAGESIZE);
}
