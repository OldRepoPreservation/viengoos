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

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <l4.h>
#include <hurd/btree.h>
#include <hurd/slab.h>

#include <compiler.h>

#include "priv.h"
#include "zalloc.h"

void
frame_dump (struct frame *frame)
{
  struct frame_entry *fe;

  printf ("frame: %x (%d refs). memory: %x+%x\n",
	  frame, frame->refs, 
	  l4_address (frame->memory), l4_size (frame->memory));
  printf ("Frame entries: ");
  for (fe = frame->frame_entries; fe; fe = fe->next)
    printf ("fe %x:%x+%x@%x on %x", fe,
	    fe->region.start, fe->region.size, fe->frame_offset,
	    fe->frame);
  printf ("\n");
}

static error_t
frame_constructor (void *hook, struct frame *frame)
{
  frame->refs = 1;
  pthread_mutex_init (&frame->lock, 0);
  pthread_mutex_lock (&frame->lock);
  frame->frame_entries = 0;
  frame->cow = 0;

  return 0;
}

SLAB_CLASS(frame, struct frame)

static struct hurd_frame_slab_space frame_space;

void
frame_init (void)
{
  hurd_frame_slab_init (&frame_space, NULL, NULL,
			frame_constructor, NULL, NULL);
}

struct frame *
frame_alloc (size_t size)
{
  error_t err;
  struct frame *frame;

  /* The size must be a power of 2.  */
  assert ((size & (size - 1)) == 0);

  err = hurd_frame_slab_alloc (&frame_space, &frame);
  if (err)
    /* XXX: Free some memory and try again.  */
    assert_perror (err);

  assert (frame->refs == 1);
  frame->memory = l4_fpage (0, size);
  frame->may_be_mapped = 0;
  assert (frame->cow == 0);
  assert (pthread_mutex_trylock (&frame->lock) == EBUSY);
  assert (frame->frame_entries == 0);

  return frame;
}

/* Allocate the reserved physical memory for frame FRAME.  */
void
frame_memory_alloc (struct frame *frame)
{
  assert (! l4_address (frame->memory));

  frame->memory = l4_fpage (zalloc (l4_size (frame->memory)),
			    l4_size (frame->memory));
  if (! l4_address (frame->memory))
    /* We already have the memory reservation, we just need to find
       it.  zalloc may have failed for a number of reasons:

         - There is no contiguous block of memory frame->SIZE bytes
           currently in the pool.
	 - We may need to reclaim extra frames
     */
    {
      /* XXX: For now we just bail.  */
      assert (l4_address (frame->memory));
    }

  debug ("allocated physical memory: %x+%x\n",
	 l4_address (frame->memory), l4_size (frame->memory));
}

void
frame_deref (struct frame *frame)
{
  assert (pthread_mutex_trylock (&frame->lock) == EBUSY);

  if (EXPECT_FALSE (frame->refs == 1))
    /* Last reference.  Deallocate this frame.  */
    {
      /* There better not be any users.  */
      assert (! frame->frame_entries);

      if (frame->may_be_mapped)
	{
	  assert (l4_address (frame->memory));
	  l4_unmap_fpage (l4_fpage_add_rights (frame->memory,
					       L4_FPAGE_FULLY_ACCESSIBLE));
	}

      if (l4_address (frame->memory))
	zfree (l4_address (frame->memory), l4_size (frame->memory));

      assert (frame->frame_entries == 0);
      assert (frame->cow == 0);
#ifndef NDEBUG
      frame->memory = l4_fpage (0xDEAD000, 0);
#endif

      hurd_frame_slab_dealloc (&frame_space, frame);
    }
  else
    {
      frame->refs --;
      pthread_mutex_unlock (&frame->lock);
    }
}

void
frame_add_user (struct frame *frame, struct frame_entry *frame_entry)
{
  assert (pthread_mutex_trylock (&frame->lock) == EBUSY);
  assert (frame->refs > 0);

  /* Add FRAME_ENTRY to the list of the users of FRAME.  */
  frame_entry->next = frame->frame_entries;
  if (frame_entry->next)
    frame_entry->next->prevp = &frame_entry->next;
  frame_entry->prevp = &frame->frame_entries;
  frame->frame_entries = frame_entry;
}

void
frame_drop_user (struct frame *frame, struct frame_entry *frame_entry)
{
  assert (pthread_mutex_trylock (&frame->lock) == EBUSY);
  assert (frame->refs > 0);
  assert (frame_entry->frame == frame);

  *frame_entry->prevp = frame_entry->next;
  if (frame_entry->next)
    frame_entry->next->prevp = frame_entry->prevp;
}
