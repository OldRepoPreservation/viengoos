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
#include <errno.h>
#include <pthread.h>
#include <compiler.h>
#include <l4.h>

#include <hurd/cap-server.h>
#include <hurd/btree.h>

#include "physmem.h"
#include "zalloc.h"

#include "output.h"


static struct hurd_cap_class container_class;


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
      hurd_cap_obj_drop (obj);
      return err;
    }

  /* The reply message consists of a single word, a capability handle
     which the client can use to refer to the container.  */

  l4_msg_append_word (ctx->msg, handle);

  debug ("Created handle %x\n", handle);

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
  struct container *container = hurd_cap_obj_to_user (struct container *,
						      ctx->obj);
  l4_word_t flags = l4_msg_word (ctx->msg, 1);
  uintptr_t start = l4_msg_word (ctx->msg, 2);
  size_t size = l4_msg_word (ctx->msg, 3);
  size_t amount;
  int i;

  /* We require at least three arguments (in addition to the cap id):
     the flags, the start and the size.  */
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

  for (err = 0, amount = 0, i = 0; i < nr_fpages; i ++)
    {
      /* FIXME: Check to make sure that the memory control object that
	 this container refers to has enough memory to do each
	 allocation.  */
      struct frame_entry *fe = frame_entry_alloc ();
      if (! fe)
	break;

      err = frame_entry_new (container, fe, start + l4_address (fpages[i]),
			     l4_size (fpages[i]));
      if (err)
	{
	  frame_entry_dealloc (fe);
	  break;
	}

      amount += l4_size (fpages[i]);

      /* XXX: Use the flags.
         frame->flags = flags; */
    }

  l4_msg_clear (ctx->msg);
  l4_msg_append_word (ctx->msg, amount);

  return err;
}

static error_t
container_deallocate (hurd_cap_rpc_context_t ctx)
{
  error_t err;
  struct container *container = hurd_cap_obj_to_user (struct container *,
						      ctx->obj);
  uintptr_t start = l4_msg_word (ctx->msg, 1);
  size_t size = l4_msg_word (ctx->msg, 2);
  struct frame_entry *fe;

  /* We require at least two arguments (in addition to the cap id):
     the start and the size.  */
  if (l4_untyped_words (l4_msg_msg_tag (ctx->msg)) != 3)
    {
      debug ("incorrect number of arguments passed.  require 3 but got %d\n",
	     l4_untyped_words (l4_msg_msg_tag (ctx->msg)));
      l4_msg_clear (ctx->msg);
      return EINVAL;
    }

  l4_msg_clear (ctx->msg);

  fe = frame_entry_find (container, start, size);
  if (! fe)
    /* Nothing was allocated in this region.  */
    return 0;

  /* Find the first region which overlaps with this one.  */
  while (1)
    {
      struct frame_entry *prev = hurd_btree_frame_entry_prev (fe);

      if (! prev
	  || ! overlap (prev->region.start, prev->region.size, start, size))
	break;

      fe = prev;
    }

  for (;;)
    {
      struct frame_entry *next = hurd_btree_frame_entry_next (fe);

      if (start <= fe->region.start
	  && start + size <= fe->region.start + fe->region.size)
	/* Deallocate the entire frame entry.  */
	{
	  frame_entry_drop (container, fe);
	  frame_entry_dealloc (fe);
	}
      else
	/* We keep part of the region.  */
	{
	  int i;
	  l4_fpage_t fpages[L4_FPAGE_SPAN_MAX];
	  int nr_fpages;

	  /* Detach the frame entry from its container: frame entries
	     in the same container cannot overlap and we are going to
	     replace it with a set of smaller frame entries covering
	     the physical memory which will not be deallocated.  */
	  frame_entry_detach (container, fe);

	  if (start >= fe->region.start)
	    /* We keep part of the start of the region.  */
	    {
	      nr_fpages = l4_fpage_span (0, start - fe->region.start, fpages);

	      for (i = 0; i < nr_fpages; i ++)
		{
		  struct frame_entry *subfe = frame_entry_alloc ();
		  assert (fe);

		  err = frame_entry_use (container, subfe,
					 start + l4_address (fpages[i]),
					 l4_size (fpages[i]),
					 fe->frame, l4_address (fpages[i]));
		  assert_perror (err);
		}
	    }

	  if (start + size < fe->region.start + fe->region.size)
	    /* We keep part of the end of the region.  */
	    {
	      nr_fpages = l4_fpage_span (0, size - start, fpages);

	      for (i = 0; i < nr_fpages; i ++)
		{
		  struct frame_entry *subfe = frame_entry_alloc ();
		  assert (fe);

		  err = frame_entry_use (container, subfe,
					 start + l4_address (fpages[i]),
					 l4_size (fpages[i]),
					 fe->frame, l4_address (fpages[i]));
		  assert_perror (err);
		}
	    }

	  frame_deref (fe->frame);
	  frame_entry_dealloc (fe);
	}

      if (! next || ! overlap (next->region.start, next->region.size,
			       start, size))
	break;
      fe = next;
    }

  return 0;
}

static error_t
container_map (hurd_cap_rpc_context_t ctx)
{
  struct container *container = hurd_cap_obj_to_user (struct container *,
						      ctx->obj);
  uintptr_t index = l4_page_trunc (l4_msg_word (ctx->msg, 1));
  l4_word_t rights = l4_msg_word (ctx->msg, 1) & 0x7;
  size_t size = l4_page_round (l4_msg_word (ctx->msg, 2));
  uintptr_t vaddr = l4_page_trunc (l4_msg_word (ctx->msg, 3));
  int nr_fpages;
#define MAX_MAP_ITEMS ((L4_NUM_MRS - 1) / 2)
  int i;

  l4_msg_clear (ctx->msg);
  
  pthread_mutex_lock (&container->lock);

  for (i = 0; i < MAX_MAP_ITEMS && size > 0; )
    {
      l4_fpage_t fpages[MAX_MAP_ITEMS];
      struct frame_entry *fe;

      /* Get the memory.  */
      fe = frame_entry_find (container, index, 1);
      if (! fe)
	{
	  pthread_mutex_unlock (&container->lock);
	  return errno;
	}

      /* Allocate the memory (if needed).  */
      if (! fe->frame)
	{
	  fe->frame = frame_alloc (fe->region.size);
	  if (! fe->frame)
	    {
	      pthread_mutex_unlock (&container->lock);
	      return errno;
	    }

	  frame_use (fe->frame, fe);
	}

      frame_memory_bind (fe->frame);
      fe->frame->may_be_mapped = true;

      /* This might only be a partial mapping of a memory.  Subtract
	 the desired index from where the memory actually starts and
	 add that to the address of the physical memory block.  */
      l4_word_t mem
	= l4_address (fe->frame->memory) + index - fe->region.start;
      l4_word_t amount = fe->region.size - (index - fe->region.start);

      /* Does the user want all that we can give?  */
      if (amount > size)
	amount = size;

      /* Adjust the index and the size.  */
      size -= amount;
      index += amount;

      nr_fpages = l4_fpage_xspan (mem, mem + amount - 1,
				  vaddr, &fpages[i], MAX_MAP_ITEMS - i);
      for (; i < MAX_MAP_ITEMS && amount > 0; i ++)
	{
	  l4_map_item_t map_item;

	  assert (nr_fpages > 0);

	  /* Set the desired permissions.  */
	  l4_set_rights (&fpages[i], rights);

	  /* Add the mad item to the message.  */
	  map_item = l4_map_item (fpages[i], vaddr);
	  l4_msg_append_map_item (ctx->msg, map_item);

	  mem += l4_size (fpages[i]);
	  vaddr += l4_size (fpages[i]);
	  amount -= l4_size (fpages[i]);
	  nr_fpages --;
	}

      assert (nr_fpages == 0);
    }

  pthread_mutex_unlock (&container->lock);

  return 0;
}

enum container_ops
  {
    container_create_id = 130,
    container_share_id,
    container_allocate_id,
    container_deallocate_id,
    container_map_id
  };

error_t
container_demuxer (hurd_cap_rpc_context_t ctx)
{
  error_t err = 0;

  debug ("Message %d from %x\n", l4_msg_label (ctx->msg), ctx->from);
  switch (l4_msg_label (ctx->msg))
    {
    case container_create_id:
      err = container_create (ctx);
      break;

    case container_share_id:
      err = container_share (ctx);
      break;

    case container_allocate_id:
      err = container_allocate (ctx);
      break;

    case container_deallocate_id:
      err = container_deallocate (ctx);
      break;

    case 128: /* The old container map implementation.  */
    case container_map_id:
      err = container_map (ctx);
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

  debug ("Returning error %d\n", err);

  return 0;
}

error_t
container_alloc (l4_word_t nr_fpages, l4_word_t *fpages,
		 struct container **r_container)
{
  error_t err;
  hurd_cap_obj_t obj;
  struct container *container;
  l4_word_t start;
  int i;

  err = hurd_cap_class_alloc (&container_class, &obj);
  if (err)
    return err;

  container = hurd_cap_obj_to_user (struct container *, obj);

  hurd_btree_frame_entry_tree_init (&container->frame_entries);
  start = l4_address (fpages[0]);
  for (i = 0; i < nr_fpages; i ++)
    {
      struct frame_entry *fe = frame_entry_alloc ();
      if (! fe)
	return ENOMEM;

      err = frame_entry_new (container, fe,
			     l4_address (fpages[i]) - start,
			     l4_size (fpages[i]));
      if (err)
	{
	  frame_entry_dealloc (fe);
	  return err;
	}

      fe->frame = frame_alloc (l4_size (fpages[i]));
      if (! fe->frame)
	{
	  frame_entry_drop (container, fe);
	  frame_entry_dealloc (fe);
	  return ENOMEM;
	}

      frame_use (fe->frame, fe);

      fe->frame->memory = fpages[i];
    }

  *r_container = container;
  return 0;
}


static void
container_reinit (hurd_cap_class_t cap_class, hurd_cap_obj_t obj)
{
  struct container *container = hurd_cap_obj_to_user (struct container *,
						      obj);
  struct frame_entry *fe, *next;

  /* We don't need to lock the container as we know we are the only
     ones who can now access it.  */
  for (fe = hurd_btree_frame_entry_first (&container->frame_entries);
       fe; fe = next)
    {
      next = hurd_btree_frame_entry_next (fe);
      frame_entry_drop (container, fe);
      frame_entry_dealloc (fe);
    }
  hurd_btree_frame_entry_tree_init (&container->frame_entries);
}


/* Initialize the container class subsystem.  */
error_t
container_class_init ()
{
  return hurd_cap_class_init (&container_class, struct container *,
			      NULL, NULL, container_reinit, NULL,
			      container_demuxer);
}

