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
#include <l4.h>
#include <hurd/btree.h>
#include <hurd/slab.h>

#include <compiler.h>

#include "physmem.h"
#include "zalloc.h"

static error_t
frame_constructor (void *hook, void *buffer)
{
  struct frame *frame = buffer;

  frame->refs = 1;
  frame->frame_entries = 0;

  return 0;
}

static struct hurd_slab_space frame_space
  = HURD_SLAB_SPACE_INITIALIZER (struct frame_entry,
				 NULL, NULL,
				 frame_constructor, NULL,
				 NULL);

struct frame *
frame_alloc (size_t size)
{
  error_t err;
  struct frame *frame;

  /* The size must be a power of 2.  */
  assert ((size & (size - 1)) == 0);

  err = hurd_slab_alloc (&frame_space, (void *) &frame);
  if (err)
    return 0;

  assert (frame->refs == 1);
  frame->memory = l4_fpage (0, size);
  frame->may_be_mapped = false;
  assert (frame->frame_entries == 0);

  return frame;
}

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
  if (EXPECT_FALSE (frame->refs == 1))
    /* Last reference.  Deallocate this frame.  */
    {
      /* There better not be any users.  */
      assert (! frame->frame_entries);

      if (frame->may_be_mapped)
	{
	  debug ("unmapping frame %x+%x\n",
		 l4_address (frame->memory), l4_size (frame->memory));
	  l4_unmap_fpage (l4_fpage_add_rights (frame->memory,
					       L4_FPAGE_FULLY_ACCESSIBLE));
	}

      if (l4_address (frame->memory))
	zfree (l4_address (frame->memory), l4_size (frame->memory));

      hurd_slab_dealloc (&frame_space, frame);
    }
  else
    frame->refs --;
}

void
frame_use (struct frame *frame, struct frame_entry *frame_entry)
{
  /* We consume a reference.  */
  assert (frame->refs > 0);

  /* Add FRAME_ENTRY to the list of users of FRAME.  */
  frame_entry->next = frame->frame_entries;
  if (frame_entry->next)
    frame_entry->next->prevp = &frame_entry->next;
  frame_entry->prevp = &frame->frame_entries;
  frame->frame_entries = frame_entry;
}

void
frame_drop (struct frame *frame, struct frame_entry *frame_entry)
{
  assert (frame->refs > 0);

  /* Remove FRAME_ENTRY from the list of users of FRAME.  */
  *frame_entry->prevp = frame_entry->next;
  if (frame_entry->next)
    frame_entry->next->prevp = frame_entry->prevp;

  /* XXX: A frame entry may refer only to a piece of the underlying
     frame.  If all that remain now are partial users of FRAME then we
     may need to do some splitting.  */

  /* Release a reference.  */
  frame_deref (frame);
}
