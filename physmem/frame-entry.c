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
#include <string.h>
#include <errno.h>
#include <hurd/btree.h>
#include <hurd/slab.h>

#include <compiler.h>

#include "priv.h"
#include "physmem.h"
#include "zalloc.h"

static error_t
frame_entry_constructor (void *hook, struct frame_entry *frame_entry)
{
  frame_entry->shared_next = frame_entry;
  frame_entry->shared_prevp = &frame_entry->shared_next;

  return 0;
}

SLAB_CLASS(frame_entry, struct frame_entry)

static struct hurd_frame_entry_slab_space frame_entry_space;

void
frame_entry_init (void)
{
  hurd_frame_entry_slab_init (&frame_entry_space, NULL, NULL,
			      frame_entry_constructor, NULL, NULL);
}

void
frame_entry_dump (struct frame_entry *fe)
{
  printf ("frame_entry: %x:%x+%x@%x, frame: %x:%x+%x, ->cow: %d, ->refs: %d, shared list: ",
	  fe, fe->region.start, fe->region.size, fe->frame_offset,
	  fe->frame, l4_address (fe->frame->memory),
	  l4_size (fe->frame->memory),
	  fe->frame->cow, fe->frame->refs);

  int shares = 0;
  struct frame_entry *f = fe;
  do
    {
      printf ("%x:%x+%x->", f, f->frame_offset, f->region.size);
      shares ++;
      assert (f->frame == fe->frame);
      f = f->shared_next;
    }
  while (f != fe);
  printf ("(=%d)\n", shares);

  int entries = 0;
  for (f = fe->frame->frame_entries; f; f = f->next)
    entries ++;
  assert (entries == fe->frame->refs);
}

struct frame_entry *
frame_entry_alloc (void)
{
  error_t err;
  struct frame_entry *frame_entry;

  err = hurd_frame_entry_slab_alloc (&frame_entry_space, &frame_entry);
  if (err)
    return 0;

  assert (frame_entry->shared_next == frame_entry);
  assert (frame_entry->shared_prevp == &frame_entry->shared_next);

  return frame_entry;
}

void
frame_entry_free (struct frame_entry *frame_entry)
{
  assert (frame_entry->shared_next == frame_entry);
#ifndef NDEBUG
  memset (frame_entry, 0xfe, sizeof (struct frame_entry));
  frame_entry_constructor (0, frame_entry);
#endif
  hurd_frame_entry_slab_dealloc (&frame_entry_space, frame_entry);
}

/* If SHARE is non-NULL, add FRAME_ENTRY (which is not attach to any
   share list) to SHARE's share list.  Otherwise, remove FRAME_ENTRY
   from the share list to which it is currently attached.  */
static void
frame_entry_share_with (struct frame_entry *frame_entry,
			struct frame_entry *share)
{
  if (share)
    /* Add FRAME_ENTRY to SHARE's share list.  */
    {
      /* FRAME_ENTRY shouldn't be on a shared list.  */
      assert (frame_entry->shared_next == frame_entry);

      frame_entry->shared_next = share;
      frame_entry->shared_prevp = share->shared_prevp;
      *frame_entry->shared_prevp = frame_entry;
      share->shared_prevp = &frame_entry->shared_next;
    }
  else
    /* Remove FRAME_ENTRY from any share list.  */
    {
      *frame_entry->shared_prevp = frame_entry->shared_next;
      frame_entry->shared_next->shared_prevp = frame_entry->shared_prevp;

      frame_entry->shared_next = frame_entry;
      frame_entry->shared_prevp = &frame_entry->shared_next;
    }
}

error_t
frame_entry_create (struct container *cont,
		    struct frame_entry *frame_entry,
		    uintptr_t cont_addr, size_t size)
{
  error_t err;

  assert (pthread_mutex_trylock (&cont->lock) == EBUSY);
  /* Size must be a power of 2.  */
  assert (size > 0 && (size & (size - 1)) == 0);

  frame_entry->container = cont;
  frame_entry->region.start = cont_addr;
  frame_entry->region.size = size;
  frame_entry->frame_offset = 0;

  frame_entry->frame = frame_alloc (size);
  if (! frame_entry->frame)
    return errno;

  err = container_attach (cont, frame_entry);
  if (EXPECT_FALSE (err))
    {
      debug ("Overlap: %x+%x\n", cont_addr, size);
      frame_deref (frame_entry->frame);
      return EEXIST;
    }

  frame_add_user (frame_entry->frame, frame_entry);

  frame_entry_share_with (frame_entry, NULL);

  return 0;
}

error_t
frame_entry_copy (struct container *cont,
		  struct frame_entry *frame_entry,
		  uintptr_t cont_addr, size_t size,
		  struct frame_entry *source,
		  size_t frame_offset,
		  bool shared_memory)
{
  error_t err;

  assert (pthread_mutex_trylock (&cont->lock) == EBUSY);
  assert (pthread_mutex_trylock (&source->frame->lock) == EBUSY);
  /* Size must be a power of 2.  */
  assert (size > 0 && (size & (size - 1)) == 0);
  assert (source->frame);
  /* Make sure that the provided offset is valid.  */
  assert (frame_offset >= 0
	  && frame_offset <= l4_size (source->frame->memory) - size);
  /* The frame entry must refer to memory starting at a size aligned
     boundary.  */
  assert ((frame_offset & (size - 1)) == 0);

  frame_entry->container = cont;
  frame_entry->region.start = cont_addr;
  frame_entry->region.size = size;
  frame_entry->frame = source->frame;
  frame_entry->frame_offset = frame_offset;

  err = container_attach (cont, frame_entry);
  if (EXPECT_FALSE (err))
    {
      debug ("Overlap: %x+%x\n", cont_addr, size);
      return EEXIST;
    }

  frame_ref (source->frame);
  frame_add_user (source->frame, frame_entry);

  if (shared_memory)
    /* This is a copy of the entry but the physical memory is
       shared.  */
    frame_entry_share_with (frame_entry, source);
  else
    /* Copy on write.  */
    {
      source->frame->cow ++;
      frame_entry_share_with (frame_entry, NULL);
    }

  return 0;
}

error_t
frame_entry_use (struct container *cont,
		 struct frame_entry *frame_entry,
		 uintptr_t cont_addr, size_t size,
		 struct frame *source,
		 size_t frame_offset)
{
  error_t err;

  assert (pthread_mutex_trylock (&cont->lock) == EBUSY);
  assert (pthread_mutex_trylock (&source->lock) == EBUSY);
  /* SIZE must be a power of 2.  */
  assert (size > 0 && (size & (size - 1)) == 0);
  /* FRAME_OFFSET must be a multiple of SIZE.  */
  assert ((frame_offset & (size - 1)) == 0);
  /* The frame entry must actually cover the FRAME.  */
  assert (frame_offset + size <= l4_size (source->memory));

  frame_entry->container = cont;
  frame_entry->region.start = cont_addr;
  frame_entry->region.size = size;
  frame_entry->frame_offset = frame_offset;
  frame_entry->frame = source;

  err = container_attach (cont, frame_entry);
  if (EXPECT_FALSE (err))
    {
      debug ("Overlap: %x+%x\n", cont_addr, size);
      return EEXIST;
    }

  frame_ref (frame_entry->frame);
  frame_add_user (frame_entry->frame, frame_entry);

  frame_entry_share_with (frame_entry, 0);

  return 0;
}

void
frame_entry_destroy (struct container *cont, struct frame_entry *frame_entry,
		     bool do_unlock)
{
  if (cont)
    {
      assert (pthread_mutex_trylock (&cont->lock) == EBUSY);
      container_detach (cont, frame_entry);
    }

  assert (frame_entry->frame);
  assert (pthread_mutex_trylock (&frame_entry->frame->lock) == EBUSY);

  frame_drop_user (frame_entry->frame, frame_entry);

  if (frame_entry->shared_next != frame_entry)
    /* FRAME_ENTRY is on a share list, remove it.  */
    frame_entry_share_with (frame_entry, NULL);
  else
    /* FRAME_ENTRY is not on a share list and therefore holds a COW
       copy if there are other users of the underlying frame.  */
    {
      if (frame_entry->frame->frame_entries)
	{
	  assert (frame_entry->frame->cow > 0);
	  frame_entry->frame->cow --;
	}
      else
	assert (frame_entry->frame->cow == 0);
    }

  if (do_unlock)
    frame_deref (frame_entry->frame);
  else
    frame_release (frame_entry->frame);
}

struct frame_entry *
frame_entry_find (struct container *cont, uintptr_t cont_addr, size_t size)
{
  assert (pthread_mutex_trylock (&cont->lock) == EBUSY);

  struct region region = { cont_addr, size };
  return hurd_btree_frame_entry_find (&cont->frame_entries, &region);
}

error_t
frame_entry_map (struct frame_entry *fe,
		 size_t start, size_t len, int access,
		 uintptr_t vaddr, l4_msg_t msg,
		 size_t *amount)
{
  error_t err;

  assert (pthread_mutex_trylock (&fe->frame->lock) == EBUSY);
  assert (start < fe->region.size);
  assert (len <= fe->region.size);
  assert (start + len <= fe->region.size);
  assert ((access & ~HURD_PM_CONT_RWX) == 0);

  if (EXPECT_FALSE ((access & HURD_PM_CONT_WRITE) && fe->frame->cow))
    /* The caller requests a mapping with write access and the
       frame is marked COW; now's the time to do the copy.  */
    {
      /* If the frame has COW copies, there must be no extant
	 writable mappings.  */
      assert (! (fe->frame->may_be_mapped & HURD_PM_CONT_WRITE));
      /* If the frame has COW copies there has to be at least two
	 users.  */
      assert (fe->frame->refs > 1);

      /* If this is a shared memory copy, we need to allocate a
	 frame to cover the largest frame entry.  */
      struct frame_entry *base = fe;

      if (! (base->frame_offset == 0
	     && base->region.size == l4_size (fe->frame->memory)))
	for (struct frame_entry *s = fe->shared_next;
	     s != fe; s = s->shared_next)
	  {
	    assert (s->frame == fe->frame);

	    /* Does S contain BASE?  */
	    if (s->frame_offset <= base->frame_offset
		&& (base->frame_offset + base->region.size
		    <= s->frame_offset + s->region.size))
	      {
		base = s;

		if (base->frame_offset == 0
		    && base->region.size == l4_size (fe->frame->memory))
		  break;
	      }
	  }

      struct frame *frame = frame_alloc (base->region.size);

      /* The point of this function is to get a mapping of the
	 memory.  Even if ORIGINAL_FRAME doesn't have memory
	 allocated yet (and hence no copy needs to be done), FRAME
	 will need the memory shortly.  */
      frame_memory_bind (frame);

      /* We only have to do a memcpy if the source has memory
	 allocated.  */
      if (l4_address (fe->frame->memory))
	memcpy ((void *) l4_address (frame->memory),
		(void *) (l4_address (fe->frame->memory)
			  + base->frame_offset),
		base->region.size);

      /* Change everyone using this copy of FE->FRAME to use FRAME
	 (i.e. all frame entries on FE's shared frame list).  */

      struct frame *original_frame = fe->frame;

      /* Iterate over the shared list moving all but BASE.  */
      struct frame_entry *falsely_shared = 0;
      struct frame_entry *next = base->shared_next;
      while (next != base)
	{
	  /* S will be removed from the shared list.  Get the next
	     element now.  */
	  struct frame_entry *s = next;
	  next = s->shared_next;

	  if (s->frame_offset < base->frame_offset
	      || s->frame_offset >= base->frame_offset + base->region.size)
	    /* S is falsely sharing with FE.  This can happen
	       when, for instance, a client A shares the middle
	       8kb of a 16kb frame with a second client, B.
	       (Hence B references 2 4kb frame entries.)  If A
	       deallocates the 16kb region, B's two frame entries
	       are still marked as shared, however, they do not
	       actually share any physical memory.  If there is an
	       extant COW on the physical memory then we only want
	       to copy the memory that is actually shared (there
	       is no need to allocate more physical memory than
	       necessary).  */
	    {
	      assert ((s->frame_offset + base->region.size
		       <= base->frame_offset)
		      || (s->frame_offset
			  >= base->frame_offset + base->region.size));

	      /* Remove S from the shared list.  */
	      frame_entry_share_with (s, NULL);

	      if (! falsely_shared)
		/* First one.  Reset.  */
		falsely_shared = s;
	      else
		/* Add to the falsely shared list with its
		   possible real sharers.  */
		frame_entry_share_with (s, falsely_shared);
	    }
	  else
	    /* Migrate S from ORIGINAL_FRAME to the copy, FRAME.
	       (If S is BASE we migrate later.)  */
	    {
	      frame_drop_user (original_frame, s);
	      frame_release (original_frame);

	      frame_ref (frame);
	      frame_add_user (frame, s);
	      s->frame = frame;
	      s->frame_offset -= base->frame_offset;

	      assert (s->frame_offset >= 0);
	      assert (s->frame_offset + s->region.size
		      <= l4_size (s->frame->memory));
	    }
	}

      /* Of those on the shared list, only BASE still references
	 the original frame.  Removing BASE may case some of
	 ORIGINAL_FRAME to now be unreferenced.  Hence, we cannot
	 simply move it as we did with the others.  */
      uintptr_t bstart = base->region.start;
      size_t bsize = base->region.size;
      bool fe_is_base = (base == fe);

      frame_entry_share_with (base, NULL);

      /* Reallocate the frame entry.  */
      err = frame_entry_deallocate (base->container, base, bstart, bsize);
      assert_perror (err);
      base = frame_entry_alloc ();
      err = frame_entry_use (base->container, base, bstart, bsize, frame, 0);
      assert_perror (err);

      /* All the frame entries using FRAME are on the shared
	 list.  */
      frame_entry_share_with (base, frame->frame_entries);

      if (fe_is_base)
	fe = base;

      /* We managed to pick up an extra reference to FRAME in the
	 loop (we already had one before we entered the loop and
	 we added a reference for each entry which shares the
	 frame including FE).  Drop it now.  */
      frame_release (frame);
    }
  else
    /* Allocate the memory (if needed).  */
    frame_memory_bind (fe->frame);

  fe->frame->may_be_mapped |= access;

  /* Get the start of the mem.  */
  l4_word_t mem = l4_address (fe->frame->memory) + fe->frame_offset + start;

  l4_fpage_t fpages[(L4_NUM_MRS - (l4_untyped_words (l4_msg_msg_tag (msg))
				   + l4_untyped_words (l4_msg_msg_tag (msg))))
		    / 2];
  int nr_fpages = l4_fpage_xspan (mem, mem + len - 1, vaddr,
				  fpages, sizeof (fpages) / sizeof (*fpages));

  for (int i = 0; i < nr_fpages; i ++)
    {
      /* Set the desired permissions.  */
      l4_set_rights (&fpages[i], access);

      /* Add the map item to the message.  */
      l4_msg_append_map_item (msg, l4_map_item (fpages[i], vaddr));

      vaddr += l4_size (fpages[i]);
    }

  if (amount)
    *amount = l4_address (fpages[nr_fpages - 1])
      + l4_size (fpages[nr_fpages - 1]) - mem;

  return
    l4_address (fpages[nr_fpages]) + l4_size (fpages[nr_fpages]) - mem < len
    ? ENOSPC : 0;
}

error_t
frame_entry_deallocate (struct container *cont,
			struct frame_entry *frame_entry,
			const uintptr_t cont_addr,
			const size_t len)
{
  const uintptr_t cont_start
    = cont_addr - frame_entry->region.start + frame_entry->frame_offset;
  struct frame *frame;

  /* FE currently uses FE->FRAME.  Make the TODO bytes of memory FE
     references starting at byte SKIP (relative to the base of FE) use
     the array of FRAMES.

     Returns the last frame used (i.e. the one which contains byte
     SKIP+TODO).  */
  struct frame **move (struct frame_entry *fe, size_t skip, size_t todo,
		       l4_fpage_t *fpages, struct frame **frames)
    {
      error_t err;

      assert (todo > 0);
      /* The first byte of FE may not by the first byte of
         *FRAMES.  For instance, FRAMES may be 8kb long
	 but FE references only the second 4kb.  */
      assert (skip < fe->region.size);
      assert (fe->frame_offset + skip >= l4_address (*fpages));
      uintptr_t frame_offset = fe->frame_offset + skip - l4_address (*fpages);

      /* The container address of the first byte.  */
      uintptr_t addr = fe->region.start + skip;

      for (; todo > 0; frames ++, fpages ++)
	{
	  size_t count = l4_size (*fpages) - frame_offset;
	  if (count > todo)
	    count = todo;

	  l4_fpage_t subfpages[L4_FPAGE_SPAN_MAX];
	  int n = l4_fpage_span (frame_offset,
				 frame_offset + count - 1,
				 subfpages);

	  for (int i = 0; i < n; i ++)
	    {
	      struct frame_entry *n = frame_entry_alloc ();

	      err = frame_entry_use (cont, n, addr,
				     l4_size (subfpages[i]),
				     *frames, l4_address (subfpages[i]));
	      assert_perror (err);

	      /* Although they only falsely share FE->FRAME (which is
		 perfectly correct), the new frame entries are on a
		 share list to reduce the number of gratuitous COWs:
		 there is one COW for each shared copy; if there is
		 only a single shared copy then no COWs need to be
		 performed.  */
	      frame_entry_share_with (n, fe);

	      frame_offset = 0;
	      addr += l4_size (subfpages[i]);
	      todo -= l4_size (subfpages[i]);
	    }
	}

      frames --;

      return frames;
    }

  /* Migrate the frame entries using FRAME between byte START and END
     to new frame structures which use the same physical memory.  */
  void migrate (uintptr_t start, uintptr_t end)
    {
      assert (start < end);
      /* START must come before the end of the deallocation zone.  */
      assert (start <= cont_start + len);
      /* END must come after the start of the deallocation zone.  */
      assert (end + 1 >= cont_start);

      /* FRAME_ENTRY must cover all of the underlying frame.  */
      assert (frame_entry->frame_offset == 0);
      assert (frame_entry->region.size == l4_size (frame->memory));

      /* Allocate new frames and point them to their respective pieces
	 of FRAME.  */
      l4_fpage_t fpages[L4_FPAGE_SPAN_MAX];
      int nr_fpages = l4_fpage_span (start, end, fpages);
      struct frame **frames = alloca (sizeof (struct frame *) * nr_fpages);

      for (int i = 0; i < nr_fpages; i ++)
	{
	  frames[i] = frame_alloc (l4_size (fpages[i]));
	  frames[i]->memory = l4_fpage (l4_address (frame->memory)
					+ l4_address (fpages[i]),
					l4_size (fpages[i]));
	  frames[i]->may_be_mapped = frame->may_be_mapped;
	}

      /* Move the parts of FRAME_ENTRY which are not going to be
	 deallocated to the new frames.  */

      int i = 0;

      /* If START is before CONT_START then we need to relocate some
	 of FRAME_ENTRY.

                START END
                v     v
             [  |  |  |  |  |  ]  <- FRAME_ENTRY
	           ^     ^
	           |     CONT_START+LEN
	           CONT_START
                 \/
                keep

	  (END can be before CONT_START.)
       */
      if (start < cont_start)
	{
	  size_t todo;
	  if (end < cont_start)
	    todo = end - start + 1;
	  else
	    todo = cont_start - start;
	    
	  /* Find the frame which contains byte START.  */
	  for (; i < nr_fpages; i ++)
	    if (start < l4_address (fpages[i]) + l4_size (fpages[i]))
	      break;

	  struct frame **l
	    = move (frame_entry, start, todo, &fpages[i], &frames[i]);

	  int last = i + ((void *) l - (void *) &frames[i]) / sizeof (*l);
	  assert (last < nr_fpages);
	  for (int j = i; j <= last; j ++)
	    frames[j]->cow ++;
	  i = last;
	}

      /* If CONT_START+LEN is before END then we need to relocate some
	 of FRAME_ENTRY.

                      START    END
                      v        v
             [  |  |  |  |  |  ]  <- FRAME_ENTRY
	           ^     ^
	           |     CONT_START+LEN
	           CONT_START
                          \    /
                           keep  

	  (START can be after CONT_START+LEN.)
       */
      if (cont_start + len < end)
	{
	  size_t skip;

	  if (start < cont_start + len)
	    skip = cont_start + len;
	  else
	    skip = start;

	  /* Find the frame which contains the first byte referenced
	     by FRAME_ENTRY after the region to deallocate.  */
	  for (; i < nr_fpages; i ++)
	    if (skip >= l4_address (fpages[i]))
	      break;

	  struct frame **l
	    = move (frame_entry, skip, end - skip + 1, &fpages[i], &frames[i]);

	  int last = i + ((void *) l - (void *) &frames[i]) / sizeof (*l);
	  assert (last < nr_fpages);
	  for (int j = i; j <= last; j ++)
	    frames[j]->cow ++;
	}

      /* Change the rest of the frame entries referencing FRAME
	 between START and END to reference the respective frames in
	 FRAMES.  */

      struct frame_entry *n = frame->frame_entries;
      while (n)
	{
	  struct frame_entry *fe = n;
	  n = fe->next;

	  /* Any frame entries connected to FRAME_ENTRY should
	     reference the same frame.  */
	  assert (frame == fe->frame);

	  /* Any frames entries referencing memory before START should
	     have been relocated from FRAME_ENTRY in a prior pass
	     (except for FRAME_ENTRY, of course).  */
	  assert (fe == frame_entry || fe->frame_offset >= start);

	  if (fe == frame_entry)
	    continue;
	  else if (fe->frame_offset < end)
	    {
	      /* END is either the end of the frame or the memory
		 following END is completely unreferenced (and to be
		 deallocated).  Hence any frame entry which starts
		 before END ends before it as well.  */
	      assert (fe->frame_offset + fe->region.size - 1 <= end);

	      void adjust (struct frame_entry *fe, int i)
		{
		  assert (fe->frame == frame);
		  /* FE fits entirely in FRAMES[I].  */
		  assert (l4_address (fpages[i]) <= fe->frame_offset
			  && (fe->frame_offset + fe->region.size
			      <= (l4_address (fpages[i])
				  + l4_size (fpages[i]))));

		  /* Adjust the frame offset.  */
		  fe->frame_offset -= l4_address (fpages[i]);

		  /* Make sure N always points to an unprocessed frame
		     entry on FRAME->FRAME_ENTRIES.  */
		  if (fe == n)
		    n = fe->next;

		  /* Move from the old frame to the new one.  */
		  frame_drop_user (frame, fe);
		  frame_release (frame);

		  fe->frame = frames[i];

		  frame_ref (fe->frame);
		  frame_add_user (fe->frame, fe);
		}

	      /* Find the frame which holds the start of the
		 memory E references.  */
	      int i;
	      for (i = 0; i < nr_fpages; i ++)
		if (fe->frame_offset >= l4_address (fpages[i])
		    && (fe->frame_offset
			< l4_address (fpages[i]) + l4_size (fpages[i])))
		  break;

	      adjust (fe, i);

	      if (fe->shared_next == fe)
		/* FE was not on a shared list.  Remove its COW from
		   FRAME.  Add a COW to the new frame.  */
		{
		  assert (frame->cow > 0);
		  frame->cow --;
		  fe->frame->cow ++;
		}
	      else
		/* FE was on a shared list.  Fix it up.  */
		{
		  bool shares_old_cow = false;
		  bool shares_new_cow_with_frame_entry = false;

		  /* We need to use FE as an anchor hence we attach to
		     here and then at the end detach FE and attach it
		     to the resulting list.  */
		  struct frame_entry *shared_list = 0;

		  struct frame_entry *sn = fe->shared_next;
		  while (sn != fe)
		    {
		      struct frame_entry *s = sn;
		      sn = s->shared_next;

		      if (s == frame_entry)
			shares_old_cow = true;
		      else if (s->frame != frame)
			/* S was already relocated which means that it
			   was split off from FRAME_ENTRY which means
			   that the cow was already counted.  */
			{
			  shares_old_cow = true;

			  if (s->frame == fe->frame)
			    {
			      shares_new_cow_with_frame_entry = true;

			      frame_entry_share_with (s, NULL);
			      if (! shared_list)
				shared_list = s;
			      else
				frame_entry_share_with (s, shared_list);
			    }
			}
		      else if (l4_address (fpages[i]) <= s->frame_offset
			       && (s->frame_offset < l4_address (fpages[i])
				   + l4_size (fpages[i])))
			/* S and FE continue to share a copy of the
			   underlying frame (i.e. no false
			   sharing).  */
			{
			  adjust (s, i);

			  frame_entry_share_with (s, NULL);
			  if (! shared_list)
			    shared_list = s;
			  else
			    frame_entry_share_with (s, shared_list);
			}
		      else
			shares_old_cow = true;
		    }

		  frame_entry_share_with (fe, 0);
		  if (shared_list)
		    frame_entry_share_with (fe, shared_list);

		  if (! shares_old_cow)
		    /* There was no false sharing, i.e. there are no
		       frame entries still using this copy of the old
		       frame.  */
		    {
		      assert (frame->cow > 0);
		      frame->cow --;
		    }

		  if (! shares_new_cow_with_frame_entry)
		    /* Unless we share our copy of the underlying
		       frame with FRAME_ENTRY, we need to add a
		       COW.  */
		    fe->frame->cow ++;
		}
	    }
	  else
	    assert (fe->frame_offset > end);
	}

      /* Any new frame entries created from FRAME_ENTRY are put on its
	 shared list.  If they were not picked up above (because
	 FRAME_ENTRY is on a share list) then some of them may not
	 have been properly fixed up.  */
      if (frame_entry->shared_next != frame_entry)
	{
	  struct frame_entry *n = frame_entry->shared_next;
	  while (n != frame_entry)
	    {
	      struct frame_entry *fe = n;
	      n = fe->shared_next;

	      if (fe->frame != frame)
		/* This is a new frame entry.  */
		{
		  assert (l4_address (frame->memory)
			  <= l4_address (fe->frame->memory));
		  assert (l4_address (fe->frame->memory)
			  <= (l4_address (frame->memory)
			      + l4_size (frame->memory)));

		  struct frame_entry *shared_list = 0;
		  struct frame_entry *m = fe->shared_next;
		  while (m != fe)
		    {
		      struct frame_entry *b = m;
		      m = m->shared_next;

		      if (fe->frame == b->frame)
			{
			  if (b == n)
			    n = n->shared_next;

			  frame_entry_share_with (b, NULL);
			  if (! shared_list)
			    shared_list = b;
			  else
			    frame_entry_share_with (b, shared_list);
			}
		    }

		  frame_entry_share_with (fe, 0);
		  if (shared_list)
		    frame_entry_share_with (fe, shared_list);
		}
	    }
	}

      /* Tidy up the new frames.  */
      for (i = 0; i < nr_fpages; i ++)
	{
	  /* Each frame should have picked up at least one frame
	     entry.  */
	  assert (frames[i]->frame_entries);

	  /* Each user of FRAMES[i] added a cow.  That is one too
	     many.  Remove it now.  */
	  assert (frames[i]->cow > 0);
	  frames[i]->cow --;
	  if (frames[i]->cow > 0
	      && (frames[i]->may_be_mapped & L4_FPAGE_WRITABLE))
	    {
	      l4_unmap_fpage (l4_fpage_add_rights (frames[i]->memory,
						   L4_FPAGE_WRITABLE));
	      frames[i]->may_be_mapped
		&= L4_FPAGE_EXECUTABLE|L4_FPAGE_READABLE;
	    }

	  /* A new frame starts life with a single reference (even
	     though no frame entries use it).  We drop that extra one
	     now.  */
	  assert (frames[i]->refs > 1);
	  frame_deref (frames[i]);
	}
    }

  assert (pthread_mutex_trylock (&cont->lock) == EBUSY);
  /* Assert that the region to deallocate falls completely within
     FRAME_ENTRY.  */
  assert (cont_addr >= frame_entry->region.start
	  && (cont_addr + len
	      <= frame_entry->region.start + frame_entry->region.size));
  /* Assert that CONT_ADDR refers to memory which starts on a
     multiple of the base page size.  */
  assert ((cont_start & (L4_MIN_PAGE_SIZE - 1)) == 0);
  /* And that LEN is a multiple of the base page size.  */
  assert ((len & (L4_MIN_PAGE_SIZE - 1)) == 0);

  frame = frame_entry->frame;
  assert (pthread_mutex_trylock (&frame->lock) == EBUSY);
  assert (frame->frame_entries);

#if 0
  printf ("%s (cont:%x, fe: %x, dzone:%x+%x); ",
	  __FUNCTION__, cont, frame_entry, cont_start, len);
  frame_entry_dump (frame_entry);
#endif

  /* Before we do anything else, we need to make sure that any
     mappings via FRAME_ENTRY are removed: most importantly, if we
     zfree any memory and then reallocate (either internally or by
     another process) before it is unmapped, any extant mappers may
     have the opportunity to see (or modify) it; but also, any
     mappings made via FRAME_ENTRY of the region to deallocate must
     (eventually) be invalidated.  Unfortunately, this means
     invalidating all mappings of FRAME.  */
  if (frame->may_be_mapped)
    {
      l4_fpage_t fpage = l4_fpage (l4_address (frame->memory)
				   + frame_entry->frame_offset,
				   frame_entry->region.size);
      l4_unmap_fpage (l4_fpage_add_rights (fpage, frame->may_be_mapped));

      /* If we unmapped the whole frame then we can clear
	 FRAME->MAY_BE_MAPPED.  */
      if (frame_entry->frame_offset == 0
	  && frame_entry->region.size == l4_size (frame->memory))
	frame->may_be_mapped = 0;
    }

  /* Detach FRAME_ENTRY from its container: frame entries in the same
     container cannot overlap and we are going to replace the parts of
     FRAME_ENTRY with a set of smaller frame entries covering the
     physical memory which will not be deallocated.  */
  container_detach (cont, frame_entry);


  if (! frame->frame_entries->next)
    /* FRAME_ENTRY is the only frame entry using FRAME.  */
    {
      /* Make sure it is using the entire frame.  */
      assert (frame_entry->frame_offset == 0);
      assert (frame_entry->region.size == l4_size (frame->memory));

      if (cont_start > 0)
	migrate (0, cont_start - 1);
      if (cont_start + len < l4_size (frame->memory))
	migrate (cont_start + len, l4_size (frame->memory) - 1);

      assert (frame->refs == 1);

      /* If some of the frame entry was migrated, we manually free any
	 physical memory.  */
      if (cont_start > 0 || cont_start + len < l4_size (frame->memory))
	{
	  l4_fpage_t fpages[L4_FPAGE_SPAN_MAX];
	  int nr_fpages = l4_fpage_span (cont_start,
					 cont_start + len - 1, fpages);

	  for (int i = 0; i < nr_fpages; i ++)
	    {
#ifndef NDEBUG
	      memset ((void *) l4_address (frame->memory)
		      + l4_address (fpages[i]),
		      0xde, l4_size (fpages[i]));
#endif
	      zfree (l4_address (frame->memory) + l4_address (fpages[i]),
		     l4_size (fpages[i]));
	    }

	  frame->may_be_mapped = 0;
	  frame->memory = l4_fpage (0, l4_size (frame->memory));
	}

      frame_entry_destroy (NULL, frame_entry, true);
      frame_entry_free (frame_entry);
      return 0;
    }

  if (frame_entry->frame_offset > 0
      || frame_entry->region.size < l4_size (frame->memory))
    /* FRAME_ENTRY does not cover all of the underlying frame.  By
       definition, some other frame entry must.  As such, all we have
       to do is fix up the parts of FRAME_ENTRY which will not be
       deallocated and then drop it.  */
    {
#ifndef NDEBUG
      /* Assert that a frame entry covers all of FE.  */
      struct frame_entry *fe;
      for (fe = frame->frame_entries; fe; fe = fe->next)
	if (fe->frame_offset == 0
	    && fe->region.size == l4_size (frame->memory))
	  break;
      assert (fe);
#endif

      l4_fpage_t fpage = l4_fpage (0, l4_size (fe->frame->memory));
      struct frame **f;

      if (frame_entry->frame_offset < cont_start)
	{
	  f = move (frame_entry, 0, cont_start - frame_entry->frame_offset,
		    &fpage, &frame_entry->frame);
	  assert (f == &frame_entry->frame);
	}
	      
      if (cont_start + len
	  < frame_entry->frame_offset + frame_entry->region.size)
	{
	  f = move (frame_entry,
		    cont_start + len - frame_entry->frame_offset,
		    frame_entry->frame_offset + frame_entry->region.size
		    - (cont_start + len),
		    &fpage, &frame_entry->frame);
	  assert (f == &frame_entry->frame);
	}

      frame_entry_destroy (NULL, frame_entry, true);
      frame_entry_free (frame_entry);
      return 0;
    }


  /* Multiple frame entries reference FRAME_ENTRY->FRAME.  Since frame
     entries may reference only part of the underlying frame, by
     releasing FRAME_ENTRY, 1) no single frame entry may now reference
     all of the underlying frame and 2) some of FRAME_ENTRY->FRAME may
     no longer be referenced and thus can be freed.  For example, a
     client, A, may allocate a 16kb frame.  A may give a second
     client, B, a copy of the middle 8kb.  (Since the start address of
     the 8kb area is not a multiple of the areas size, we create two
     frame entries: one for each 4kb region.)  If A then deallocates
     the 16kb region, we would like to release the unreferenced
     physical memory.  By inspection, we see that the first 4kb and
     the last 4kb are no longer used and could be freed:
  
                         A
                  /             \
              0kb|   |   |   |   |16kb
                      \ / \ /
                      B.1 B.2
  
     This problem becomes slightly more complicated when only part of
     the frame entry is freed, e.g. if the client only deallocate the
     first 8kb of A.  Further, we must maintain the predicate that all
     frames have at least one frame entry which references them in
     their entirety.

     We take the following approach:
  
     Set A=(DEALLOC_START, LEN) to the region to deallocate (relative
     to the start of the underlying frame).  To identify unreferenced
     regions, iterate over the frame entries referencing the frame
     (excluding the one to deallocate).  If the intersection of the
     frame entry and A is non-empty (i.e. A contains any part of the
     frame entry), save the result in T.  If R is not set, set it to
     T.  Otherwise, if T occurs before R, set R to T.
  
     If R completely covers the region to deallocate, A, we are done.
     Otherwise, any memory between the start of A and the start of R
     is unreferenced and we can free it (doing any required frame
     splitting).  Move the start of A to end of R.  If the area is
     non-NULL, repeat from the beginning.  Otherwise, we are done.  */


  /* Start of the region to deallocate relative to the start of the
     frame which we still need to confirmed as referenced or not.  */
  uintptr_t dealloc_start = cont_start;
  size_t dealloc_len = len;
  
  /* Area within DEALLOC_START+DEALLOC_LEN which we've confirmed
     another frame entry references.

     Initially, we (consistent with the above predicate) set the start
     address to the end of the region to deallocate with a length of
     0.  Because we prefer earlier and larger regions, if this isn't
     changed after iterating over all of the frame entries, we know it
     is safe to free the region.  */
  uintptr_t refed_start = dealloc_start + dealloc_len;
  size_t refed_len = 0;

  /* Once we've identified a region which we can free (i.e. a region
     which no frame entry references), we will need to split memory
     which is still referenced into smaller frames.  This is
     complicated by the fact that there may be multiple holes.
     Consider:

                       FE
                 /             \
                [   |   |   |   ]
                0   4   8   C   F
                     \ /     \ /
                      A       B

     If all of frame entry FE is freed, frame entry A and B still
     reference two 4k segments of the frame.  We can free from the 4k
     regions starting at 0k and 8k.
     
     After each iteration, during which we've identified a region
     which is referenced, we free the memory between PROCESSED and
     REFED_START and relocate the frame entries between
     REFED_START+REFED_LEN.  We then set PROCESSED to
     REFED_START+REFED_LEN.  */
  uintptr_t processed = 0;

  for (;;)
    {
      /* Iterate over the frame entries checking to see if they
	 reference DEALLOC_START+DEALLOC_LEN and modifying
	 REFED_START+REFED_LEN appropriately.  */
      for (struct frame_entry *fe = frame->frame_entries; fe; fe = fe->next)
	{
	  assert (fe->frame == frame);

	  /* Don't consider the frame entry we are deallocating.  */
	  if (fe == frame_entry)
	    continue;

	  if (fe->frame_offset + fe->region.size <= dealloc_start)
	    /* FE ends before the region to deallocate begins.  */
	    continue;

	  if (fe->frame_offset >= dealloc_start + dealloc_len)
	    /* FE starts after the region to deallocate ends.  */
	    continue;

	  if (fe->frame_offset < refed_start)
	    /* FE covers at least part of the region to deallocate and
	       starts before what we've found so far.  */
	    {
	      refed_start = fe->frame_offset;
	      refed_len = fe->region.size;
	    }
	  else if (fe->frame_offset == refed_start
		   && fe->region.size > refed_len)
	    /* FE starts at REFED_START and is larger than
	       REFED_LEN.  */
	    refed_len = fe->region.size;
	}

      if (processed < refed_start && processed < dealloc_start)
	/* PROCESSED comes before both REFED_START and DEALLOC_START.
	   If there is memory to be freed, that memory is between
	   DEALLOC_START and REFED_START.  On the other hand,
	   REFED_START may come before DEALLOC_START if a frame
	   straddles DEALLOC_START.  There is no need to gratuitously
	   split it apart.  */
	migrate (processed,
		 refed_start < dealloc_start
		 ? refed_start - 1 : dealloc_start - 1);

      /* The area between DEALLOC_START and REFED_START is not
	 referenced.  Free it and adjust the frame entries.  */

      if (dealloc_start < refed_start && l4_address (frame->memory))
	{
	  l4_fpage_t fpages[L4_FPAGE_SPAN_MAX];
	  int nr_fpages = l4_fpage_span (dealloc_start,
					 refed_start - 1, fpages);

	  for (int i = 0; i < nr_fpages; i ++)
	    {
#ifndef NDEBUG
	      memset ((void *) l4_address (frame->memory)
		      + l4_address (fpages[i]),
		      0xde, l4_size (fpages[i]));
#endif
	      zfree (l4_address (frame->memory) + l4_address (fpages[i]),
		     l4_size (fpages[i]));
	    }
	}

      if (refed_len > 0)
	migrate (refed_start, refed_start + refed_len - 1);
      processed = refed_start + refed_len;

      if (refed_start + refed_len >= dealloc_start + dealloc_len)
	break;

      dealloc_len -= refed_start + refed_len - dealloc_start;
      dealloc_start = refed_start + refed_len;

      refed_start = dealloc_start + dealloc_len;
      refed_len = 0;
    }

  /* Move any remaining frame entries over.  */
  if (processed < l4_size (frame->memory))
    migrate (processed, l4_size (frame->memory) - 1);

  /* And destroy the now redundant FRAME_ENTRY.  But don't let it
     deallocate the physical memory!  */
  frame->memory = l4_fpage (0, l4_size (frame->memory));

  assert (frame->refs == 1);
  assert (frame->frame_entries == frame_entry);
  assert (! frame->frame_entries->next);
  assert (frame_entry->shared_next == frame_entry);
  frame_entry_destroy (NULL, frame_entry, true);
  frame_entry_free (frame_entry);
  return 0;
}
