/* container.c - container class for physical memory server.
   Copyright (C) 2003, 2004, 2005 Free Software Foundation, Inc.
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

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <compiler.h>
#include <l4.h>

#include <hurd/cap-server.h>
#include <hurd/btree.h>

#include "priv.h"
#include "physmem.h"
#include "zalloc.h"

#include "output.h"


static struct hurd_cap_class container_class;

static inline void
container_dump (struct container *cont)
{
  struct frame_entry *fe;

  printf ("Container %x: ", cont);
  for (fe = hurd_btree_frame_entry_first (&cont->frame_entries); fe;
       fe = hurd_btree_frame_entry_next (fe))
    printf ("fe:%x %x+%x@%x on %x:%x+%x ",
	    fe, fe->region.start, fe->region.size, fe->frame_offset,
	    fe->frame, l4_address (fe->frame->memory),
	    l4_size (fe->frame->memory));
  printf ("\n");
}

error_t
container_attach (struct container *cont, struct frame_entry *frame_entry)
{
  error_t err;

  assert (pthread_mutex_trylock (&cont->lock) == EBUSY);

  err = hurd_btree_frame_entry_insert (&cont->frame_entries, frame_entry);
  if (! err)
    frame_entry->container = cont;

  return err;
}

void
container_detach (struct container *cont, struct frame_entry *frame_entry)
{
  assert (pthread_mutex_trylock (&cont->lock) == EBUSY);
  assert (hurd_btree_frame_entry_find (&cont->frame_entries,
				       &frame_entry->region));
  assert (cont == frame_entry->container);

  hurd_btree_frame_entry_detach (&cont->frame_entries, frame_entry);
}

/* CTX->obj should be a memory control object, not a container.  */
static error_t
container_create (hurd_cap_rpc_context_t ctx)
{
  error_t err;
  hurd_cap_obj_t obj;
  hurd_cap_handle_t handle;

  l4_msg_clear (ctx->msg);

  err = hurd_cap_class_alloc (&container_class, &obj);
  if (err)
    return err;
  hurd_cap_obj_unlock (obj);

  err = hurd_cap_bucket_inject (ctx->bucket, obj, ctx->sender, &handle);
  if (err)
    {
      hurd_cap_obj_lock (obj);
      hurd_cap_obj_drop (obj);
      return err;
    }

  /* The reply message consists of a single word, a capability handle
     which the client can use to refer to the container.  */

  l4_msg_append_word (ctx->msg, handle);

  return 0;
}

static error_t
container_share (hurd_cap_rpc_context_t ctx)
{
  return EOPNOTSUPP;
}

static error_t
container_allocate (hurd_cap_rpc_context_t ctx)
{
  error_t err;
  struct container *cont = hurd_cap_obj_to_user (struct container *, ctx->obj);
  l4_word_t flags = l4_msg_word (ctx->msg, 1);
  uintptr_t start = l4_msg_word (ctx->msg, 2);
  size_t size = l4_msg_word (ctx->msg, 3);
  size_t amount;
  int i;

  /* We require three arguments (in addition to the cap id): the
     flags, the start and the size.  */
  if (l4_untyped_words (l4_msg_msg_tag (ctx->msg)) != 4)
    {
      debug ("incorrect number of arguments passed.  require 4 but got %d\n",
	     l4_untyped_words (l4_msg_msg_tag (ctx->msg)));
      l4_msg_clear (ctx->msg);
      return EINVAL;
    }

  /* Allocate the memory.  */
  l4_fpage_t fpages[L4_FPAGE_SPAN_MAX];
  int nr_fpages = l4_fpage_span (0, size - 1, fpages);

  pthread_mutex_lock (&cont->lock);

  for (err = 0, amount = 0, i = 0; i < nr_fpages; i ++)
    {
      /* FIXME: Check to make sure that the memory control object that
	 this container refers to has enough memory to do each
	 allocation.  */
      struct frame_entry *fe = frame_entry_alloc ();
      assert (fe);

      err = frame_entry_create (cont, fe, start + l4_address (fpages[i]),
				l4_size (fpages[i]));
      if (err)
	{
	  frame_entry_free (fe);
	  break;
	}

      amount += l4_size (fpages[i]);

      /* XXX: Use the flags.
         frame->flags = flags; */

      pthread_mutex_unlock (&fe->frame->lock);
    }

  pthread_mutex_unlock (&cont->lock);

  l4_msg_clear (ctx->msg);
  l4_msg_append_word (ctx->msg, amount);

  return err;
}

static error_t
container_deallocate (hurd_cap_rpc_context_t ctx)
{
  error_t err = 0;
  struct container *cont = hurd_cap_obj_to_user (struct container *, ctx->obj);
  uintptr_t start = l4_msg_word (ctx->msg, 1);
  const size_t size = l4_msg_word (ctx->msg, 2);
  size_t remaining = size;

  /* We require two arguments (in addition to the cap id): the start
     and the size.  */
  if (l4_untyped_words (l4_msg_msg_tag (ctx->msg)) != 3)
    {
      debug ("incorrect number of arguments passed.  require 3 but got %d\n",
	     l4_untyped_words (l4_msg_msg_tag (ctx->msg)));
      l4_msg_clear (ctx->msg);
      return EINVAL;
    }

  l4_msg_clear (ctx->msg);

  pthread_mutex_lock (&cont->lock);

  if ((size & (L4_MIN_PAGE_SIZE - 1)) != 0)
    {
      err = EINVAL;
      goto out;
    }

  struct frame_entry *next = frame_entry_find (cont, start, 1);
  if (! next)
    goto out;

  if (((start - next->region.start) & (L4_MIN_PAGE_SIZE - 1)) != 0)
    {
      err = EINVAL;
      goto out;
    }

  while (next && remaining > 0)
    {
      struct frame_entry *fe = next;

      /* We must get the region after FE before we potentially
	 deallocate FE.  */
      if (fe->region.start + fe->region.size < start + remaining)
	/* The region to deallocate extends beyond FE.  */
	{
	  next = hurd_btree_frame_entry_next (fe);
	  if (next && fe->region.start + fe->region.size != next->region.start)
	    /* NEXT does not immediately follow FE.  */
	    next = 0;
	}
      else
	/* FE is the last frame entry to process.  */
	next = 0;

      /* The number of bytes to deallocate in this frame entry.  */
      size_t length = fe->region.size - (start - fe->region.start);
      if (length > remaining)
	length = remaining;
      assert (length > 0);

      pthread_mutex_lock (&fe->frame->lock);
      err = frame_entry_deallocate (cont, fe, start, length);
      if (err)
	goto out;

      start += length;
      remaining -= length;
    }

 out:
  pthread_mutex_unlock (&cont->lock);

  if (remaining > 0)
    debug ("no frame entry at %x (of container %x) but %x bytes "
	   "left to deallocate!\n", start, cont, remaining);

  /* Return the amount actually deallocated.  */
  l4_msg_append_word (ctx->msg, size - remaining);

  return err;
}

static error_t
container_map (hurd_cap_rpc_context_t ctx)
{
  error_t err = 0;
  struct container *cont = hurd_cap_obj_to_user (struct container *, ctx->obj);
  l4_word_t flags = l4_msg_word (ctx->msg, 1);
  uintptr_t vaddr = l4_msg_word (ctx->msg, 2);
  uintptr_t index = l4_msg_word (ctx->msg, 3);
  size_t size = l4_msg_word (ctx->msg, 4);

  /* We require four arguments (in addition to the cap id).  */
  if (l4_untyped_words (l4_msg_msg_tag (ctx->msg)) != 5)
    {
      debug ("incorrect number of arguments passed.  require 5 but got %d\n",
	     l4_untyped_words (l4_msg_msg_tag (ctx->msg)));
      l4_msg_clear (ctx->msg);
      return EINVAL;
    }

  l4_msg_clear (ctx->msg);

#if 0
  printf ("container_map (index:%x, size:%x, vaddr:%x, flags: %x)\n",
	  index, size, vaddr, flags);
#endif

  /* SIZE must be a multiple of the minimum page size and VADDR must
     be aligned on a base page boundary.  */
  if ((size & (L4_MIN_PAGE_SIZE - 1)) != 0
      || (vaddr & (L4_MIN_PAGE_SIZE - 1)) != 0)
    return EINVAL;

  pthread_mutex_lock (&cont->lock);

  struct frame_entry *fe;
  for (fe = frame_entry_find (cont, index, 1);
       fe && size > 0;
       fe = hurd_btree_frame_entry_next (fe))
    {
      if (index < fe->region.start)
	/* Hole between last frame and this one.  */
	{
	  err = EINVAL;
	  break;
	}

      uintptr_t offset = index - fe->region.start;
      if ((offset & (getpagesize () - 1)))
	/* Not properly aligned.  */
	{
	  err = EINVAL;
	  break;
	}

      size_t len = fe->region.size - offset;
      if (len > size)
	len = size;

      size_t amount;

      pthread_mutex_lock (&fe->frame->lock);
      err = frame_entry_map (fe, offset, len, extract_access (flags), vaddr,
			     ctx->msg, &amount);
      pthread_mutex_unlock (&fe->frame->lock);

      assert (! err || err == ENOSPC);

      index += amount;
      size -= amount;
      vaddr += amount;

      if (err == ENOSPC)
	{
	  err = 0;
	  break;
	}
    }

  pthread_mutex_unlock (&cont->lock);

  return err;
}

static error_t
container_copy (hurd_cap_rpc_context_t ctx)
{
  error_t err = 0;
  struct hurd_cap_ctx_cap_use *cap_use;
  struct container *src_cont = hurd_cap_obj_to_user (struct container *,
						     ctx->obj);

  /* SRC_START will move as we copy data; SRC_START_ORIG stays
     constant so that we can figure out how much we have copied.  */
  uintptr_t src_start = l4_msg_word (ctx->msg, 1);
  const uintptr_t src_start_orig = src_start;

  l4_word_t dest_cont_handle = l4_msg_word (ctx->msg, 2);
  hurd_cap_obj_t dest_cap;
  struct container *dest_cont;

  uintptr_t dest_start = l4_msg_word (ctx->msg, 3);

  size_t count = l4_msg_word (ctx->msg, 4);
  size_t flags = l4_msg_word (ctx->msg, 5);

  struct frame_entry *sfe_next;
  int nr_fpages;
  l4_fpage_t fpages[L4_FPAGE_SPAN_MAX];
  int i;

  /* We require five arguments (in addition to the cap id).  */
  if (l4_untyped_words (l4_msg_msg_tag (ctx->msg)) != 6)
    {
      debug ("incorrect number of arguments passed.  require 6 but got %d\n",
	     l4_untyped_words (l4_msg_msg_tag (ctx->msg)));
      l4_msg_clear (ctx->msg);
      return EINVAL;
    }

  l4_msg_clear (ctx->msg);

  if (ctx->handle == dest_cont_handle)
    /* The source container is the same as the destination
       container.  */
    {
      dest_cont = src_cont;
      pthread_mutex_lock (&src_cont->lock);
    }
  else
    /* Look up the destination container.  */
    {
      cap_use = alloca (hurd_cap_ctx_size ());
      err = hurd_cap_ctx_start_cap_use (ctx,
					dest_cont_handle, &container_class,
					cap_use, &dest_cap);
      if (err)
	goto out;

      hurd_cap_obj_unlock (dest_cap);

      dest_cont = hurd_cap_obj_to_user (struct container *, dest_cap); 

      /* There is a possible dead lock scenario here: one thread
	 copies from SRC to DEST and another from DEST to SRC.  We
	 lock based on the lexical order of the container
	 pointers.  */
      if (src_cont < dest_cont)
	{
	  pthread_mutex_lock (&src_cont->lock);
	  pthread_mutex_lock (&dest_cont->lock);
	}
      else
	{
	  pthread_mutex_lock (&dest_cont->lock);
	  pthread_mutex_lock (&src_cont->lock);
	}
    }

  if ((flags & HURD_PM_CONT_ALL_OR_NONE))
    /* Don't accept a partial copy.  */
    {
      /* XXX: Make sure that all of the source is defined and has
	 enough permission.  */

      /* Check that no frames are located in the destination
	 region.  */
      struct frame_entry *fe = frame_entry_find (dest_cont, dest_start,
						 count);
      if (fe)
	{
	  err = EEXIST;
	  goto clean_up;
	}
    }

  /* Find the frame entry in the source container which contains the
     start of the region to copy.  */
  sfe_next = frame_entry_find (src_cont, src_start, 1);
  if (! sfe_next)
    {
      err = ENOENT;
      goto clean_up;
    }

  /* Make sure that SRC_START is aligned on a frame boundary.  */
  if (((sfe_next->region.start - src_start) & (L4_MIN_PAGE_SIZE - 1)) != 0)
    {
      err = EINVAL;
      goto clean_up;
    }

  while (sfe_next && count)
    {
      struct frame_entry *sfe, *dfe;
      uintptr_t src_end;

      sfe = sfe_next;

      /* Does the source frame entry cover all of the memory that we
	 need to copy?  */
      if (src_start + count > sfe->region.start + sfe->region.size)
	/* No.  We will have to read the following frame as well.  */
	{
	  src_end = sfe->region.start + sfe->region.size - 1;

	  /* Get the next frame entry.  */
	  sfe_next = hurd_btree_frame_entry_next (sfe);
	  if (sfe_next && sfe_next->region.start != src_end + 1)
	    /* There is a gap between SFE and the next frame
	       entry.  */
	    sfe_next = NULL;
	}
      else
	/* The end of the region to copy is contained within SFE.  */
	{
	  src_end = src_start + count - 1;

	  /* Once we process this frame entry, we will be done.  */
	  sfe_next = NULL;
	}

      pthread_mutex_lock (&sfe->frame->lock);

      /* Get the frames we'll have in the destination container.  */
      nr_fpages
	= l4_fpage_span (src_start - sfe->region.start + sfe->frame_offset,
			 src_end - sfe->region.start + sfe->frame_offset,
			 fpages);
      assert (nr_fpages > 0);

      for (i = 0; i < nr_fpages; i ++)
	{
	  dfe = frame_entry_alloc ();
	  if (! dfe)
	    {
	      pthread_mutex_unlock (&sfe->frame->lock);
	      err = ENOMEM;
	      goto clean_up;
	    }

	  /* XXX: We need to check the user's quota.  */
	  err = frame_entry_copy (dest_cont, dfe,
				  dest_start, l4_size (fpages[i]),
				  sfe,
				  sfe->frame_offset
				  + src_start - sfe->region.start,
				  flags & HURD_PM_CONT_COPY_SHARED);
	  if (err)
	    {
	      pthread_mutex_unlock (&sfe->frame->lock);
	      frame_entry_free (dfe);
	      goto clean_up;
	    }

	  src_start += l4_size (fpages[i]);
	  dest_start += l4_size (fpages[i]);
	  count -= l4_size (fpages[i]);
	}

      if (! (flags & HURD_PM_CONT_COPY_SHARED)
	  && (sfe->frame->may_be_mapped & HURD_PM_CONT_WRITE))
	/* We just created a COW copy of SFE->FRAME and we have given
	   out at least one map with write access.  Revoke any write
	   access to the frame.  */
	{
	  l4_fpage_t fpage = sfe->frame->memory;
	  l4_set_rights (&fpage, L4_FPAGE_WRITABLE);
	  l4_unmap_fpage (fpage);
	  sfe->frame->may_be_mapped &= L4_FPAGE_EXECUTABLE|L4_FPAGE_READABLE;
	}

      pthread_mutex_unlock (&sfe->frame->lock);
    }

  if (count > 0)
    {
      assert (! sfe_next);
      err = ESRCH;
    }

 clean_up:
  assert (count == 0 || err);

  if (dest_cont == src_cont)
    /* If the source and destination are the same then don't unlock
       the same lock twice.  */
    pthread_mutex_unlock (&src_cont->lock);
  else
    {
      /* Unlike with locking, the unlock order doesn't matter.  */
      pthread_mutex_unlock (&src_cont->lock);
      pthread_mutex_unlock (&dest_cont->lock);

      hurd_cap_obj_lock (dest_cap);
      hurd_cap_ctx_end_cap_use (ctx, cap_use);
    }

 out:
  /* Return the amount actually copied.  */
  l4_msg_append_word (ctx->msg, src_start - src_start_orig);
  return err;
}

error_t
container_demuxer (hurd_cap_rpc_context_t ctx)
{
  error_t err = 0;

  switch (l4_msg_label (ctx->msg))
    {
    case hurd_pm_container_create_id:
      err = container_create (ctx);
      break;

    case hurd_pm_container_share_id:
      err = container_share (ctx);
      break;

    case hurd_pm_container_allocate_id:
      err = container_allocate (ctx);
      break;

    case hurd_pm_container_deallocate_id:
      err = container_deallocate (ctx);
      break;

    case 128: /* The old container map implementation.  */
    case hurd_pm_container_map_id:
      err = container_map (ctx);
      break;

    case hurd_pm_container_copy_id:
      err = container_copy (ctx);
      break;

    default:
      err = EOPNOTSUPP;
    }

  /* If the stub returns EOPNOTSUPP then we clear the message buffer,
     otherwise we assume that the message buffer contains a valid
     reply message and in which case we set the error code returned by
     the stub and have the demuxer succeed.  */
  if (EXPECT_FALSE (err == EOPNOTSUPP))
    l4_msg_clear (ctx->msg);

  l4_set_msg_label (ctx->msg, err);

  if (err)
    debug ("%s: Returning %d to %x\n", __FUNCTION__, err, ctx->from);

  return 0;
}

error_t
container_alloc (l4_word_t nr_fpages, l4_word_t *fpages,
		 struct container **r_cont)
{
  error_t err;
  hurd_cap_obj_t obj;
  struct container *cont;
  l4_word_t start;
  int i;

  err = hurd_cap_class_alloc (&container_class, &obj);
  if (err)
    return err;

  cont = hurd_cap_obj_to_user (struct container *, obj);

#ifndef NDEBUG
  /* We just allocated CONT and we haven't given it to anyone else,
     however, frame_entry_create requires that CONT be locked and if
     it isn't, will trigger an assert.  Make it happy.  */
  pthread_mutex_lock (&cont->lock);
#endif

  hurd_btree_frame_entry_tree_init (&cont->frame_entries);
  start = l4_address (fpages[0]);
  for (i = 0; i < nr_fpages; i ++)
    {
      struct frame_entry *fe = frame_entry_alloc ();
      if (! fe)
	{
	  err = ENOMEM;
	  break;
	}

      err = frame_entry_create (cont, fe,
				l4_address (fpages[i]) - start,
				l4_size (fpages[i]));
      if (err)
	{
	  frame_entry_free (fe);
	  break;
	}

      fe->frame->memory = fpages[i];
      pthread_mutex_unlock (&fe->frame->lock);
    }

#ifndef NDEBUG
  pthread_mutex_unlock (&cont->lock);
#endif

  if (! err)
    *r_cont = cont;
  return err;
}


/* Initialize a new container object.  */
static error_t
container_init (hurd_cap_class_t cap_class, hurd_cap_obj_t obj)
{
  struct container *cont = hurd_cap_obj_to_user (struct container *, obj);

  pthread_mutex_init (&cont->lock, 0);
  hurd_btree_frame_entry_tree_init (&cont->frame_entries);

  return 0;
}

/* Reinitialize a container object.  */
static void
container_reinit (hurd_cap_class_t cap_class, hurd_cap_obj_t obj)
{
  struct container *cont = hurd_cap_obj_to_user (struct container *, obj);
  struct frame_entry *fe, *next;

  assert (pthread_mutex_trylock (&cont->lock));
  pthread_mutex_unlock (&cont->lock);

  for (fe = hurd_btree_frame_entry_first (&cont->frame_entries);
       fe; fe = next)
    {
      next = hurd_btree_frame_entry_next (fe);
      pthread_mutex_lock (&fe->frame->lock);
      frame_entry_destroy (cont, fe, true);
      frame_entry_free (fe);
    }

  hurd_btree_frame_entry_tree_init (&cont->frame_entries);
}

/* Initialize the container class subsystem.  */
error_t
container_class_init ()
{
  return hurd_cap_class_init (&container_class, struct container *,
			      container_init, NULL, container_reinit, NULL,
			      container_demuxer);
}
