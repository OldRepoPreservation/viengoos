/* pager.c - Page fault handler.
   Copyright (C) 2005 Free Software Foundation, Inc.
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
#include <l4/pagefault.h>
#include <compiler.h>

#include "priv.h"

void
pager (void)
{
  error_t err;
  l4_thread_id_t sender = l4_nilthread;
  l4_msg_tag_t tag;
  l4_msg_t msg;
  l4_word_t access;
  uintptr_t ip, addr;
  struct map *map;
  uintptr_t store_offset;
  struct frame *frame;

  /* Enter an infinite loop waiting for page faults.  */
  for (;;)
    {
      l4_accept (L4_UNTYPED_WORDS_ACCEPTOR);
      l4_msg_clear (msg);
      l4_msg_load (msg);

      tag = l4_ipc (sender, l4_anylocalthread,
		    l4_timeouts (L4_ZERO_TIME, L4_NEVER), &sender);
      assert (! l4_ipc_failed (tag));

      if (EXPECT_FALSE (! l4_is_pagefault (tag)))
	{
	  printf ("Pager ignoring non-pagefault ipc (tag: %x) from %x. "
		  "Komisch.\n",
		  tag, sender);
	  sender = l4_nilthread;
	  continue;
	}

      addr = l4_pagefault (tag, &access, &ip);

#if 0
      printf ("Page fault from %x at address %x (IP: %x) require rights %x\n",
	      sender, addr, ip, access);
#endif

      /* Round address down to the nearest base page address.  */
      addr &= ~(getpagesize () - 1);

      /* Find the mapping on which the region faulted.  */
      map = map_find (addr, 1);
      if (EXPECT_FALSE (! map))
	/* XXX: Segmentation fault.  */
	panic ("Virtual address %x is unmapped.  Cannot handle fault! "
	       "(segmentation fault?!)\n", addr);

      assert (map->store);

      store_offset = addr - map->vm.start + map->store_offset;

      /* The mapping might be cached in physical memory but not
	 properly mapped.  */
      frame = frame_find_first (map->store, store_offset, 1);
      if (! frame)
	/* It isn't.  We need to page the data in to physical memory
	   (or, in the case of anonymous memory, allocate physical
	   memory to cover the region).  */
	{
	  if (map->store == &swap_store)
	    {
	      /* XXX: Until we have a real swap server we (correctly)
		 assume that if there is no frame then memory has just
		 not yet been allocated (i.e. it is not just paged
		 out).  */
	      frame = frame_alloc (getpagesize ());
	      if (! frame)
		panic ("frame_alloc failed.");

	      frame_insert (map->store, store_offset, frame);
	    }
	  else
	    panic ("Unallocated non-swap memory?!");
	}

      err = frame_map (frame, 0, frame->store.size,
		       map->vm.start + frame->store.start - map->store_offset);
      assert_perror (err);
    }

  panic ("Page fault loop broken?");
}
