/* frame-entry.c - Frame entry management.
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
#include <hurd/btree.h>
#include <hurd/slab.h>

#include <compiler.h>

#include "physmem.h"

static struct hurd_slab_space frame_entry_space
  = HURD_SLAB_SPACE_INITIALIZER (struct frame_entry,
				 NULL, NULL,
				 NULL, NULL,
				 NULL);

static inline void
frame_entry_dump (struct container *cont)
{
  struct frame_entry *fe;

  for (fe = hurd_btree_frame_entry_first (&cont->frame_entries); fe;
       fe = hurd_btree_frame_entry_next (fe))
    printf ("%x+%x@%x ", fe->region.start, fe->region.size,
	    l4_address (fe->frame->memory));
  printf ("\n");
}

struct frame_entry *
frame_entry_alloc (void)
{
  error_t err;
  struct frame_entry *frame_entry;

  err = hurd_slab_alloc (&frame_entry_space, (void *) &frame_entry);
  if (err)
    return 0;

  return frame_entry;
}

void
frame_entry_dealloc (struct frame_entry *frame_entry)
{
  hurd_slab_dealloc (&frame_entry_space, frame_entry);
}

error_t
frame_entry_new (struct container *cont,
		 struct frame_entry *frame_entry,
		 uintptr_t start, size_t size)
{
  error_t err;

  /* Size must be a power of 2.  */
  assert (size > 0 && (size & (size - 1)) == 0);

  /* Initialize the frame_entry region.  */
  frame_entry->region.start = start;
  frame_entry->region.size = size;
  frame_entry->frame = 0;
  frame_entry->frame_offset = 0;

  err = frame_entry_attach (cont, frame_entry);
  if (EXPECT_FALSE (err))
    {
      debug ("Overlap: %x+%x\n", start, size);
      return EEXIST;
    }

  return 0;
}

error_t
frame_entry_use (struct container *cont, struct frame_entry *frame_entry,
		 uintptr_t start, size_t size,
		 struct frame *frame, size_t offset)
{
  error_t err;

  /* Size must be a power of 2.  */
  assert (size > 0 && (size & (size - 1)) == 0);
  assert (offset > 0 && offset <= l4_size (frame->memory) - size);

  /* Initialize the frame_entry region.  */
  frame_entry->region.start = start;
  frame_entry->region.size = size;
  frame_entry->frame = frame;
  /* Only refer to an offset in FRAME if FRAME already exists.  */
  frame_entry->frame_offset = frame ? offset : 0;

  err = frame_entry_attach (cont, frame_entry);
  if (EXPECT_FALSE (err))
    {
      debug ("Overlap: %x+%x\n", start, size);
      return EEXIST;
    }

  if (frame)
    {
      frame_ref (frame);
      frame_use (frame, frame_entry);
    }

  return 0;
}

struct frame_entry *
frame_entry_find (struct container *cont, uintptr_t start, size_t size)
{
  struct region region = { start, size };

  return hurd_btree_frame_entry_find (&cont->frame_entries, &region);
}

void
frame_entry_drop (struct container *cont, struct frame_entry *frame_entry)
{
  if (frame_entry->frame)
    frame_drop (frame_entry->frame, frame_entry);

  frame_entry_detach (cont, frame_entry);
}

error_t
frame_entry_attach (struct container *cont, struct frame_entry *frame_entry)
{
  return hurd_btree_frame_entry_insert (&cont->frame_entries, frame_entry);
}

void
frame_entry_detach (struct container *cont, struct frame_entry *frame_entry)
{
  assert (hurd_btree_frame_entry_find (&cont->frame_entries,
				       &frame_entry->region));
  hurd_btree_frame_entry_detach (&cont->frame_entries, frame_entry);
}
