/* anonymous.c - Anonymous memory management.
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

#include <stdint.h>
#include "priv.h"

static struct hurd_store anonymous_store;

error_t
hurd_anonymous_allocate (uintptr_t *address, size_t size,
			 uintptr_t flags, int map_now)
{
  /* This is a bit frustrating.  The store cache is indexed by the
     store offset thus even though anonymous memory does not initially
     have a location on backing store, we need to fabricate one.
     Right now we just use a static variable.  The 64-bits should give
     us a fair amount of time before we need to come up with a better
     solution.  */
  static off64_t offset;
  error_t err;

  err = hurd_store_bind_to_vm (&anonymous_store, offset,
			       address ? *address : 0, size,
			       flags, 1, address);
  if (err)
    return err;

  offset += size;
  return 0;
}

static void
handle_fault (hurd_store_t *store, void *hook, l4_thread_id_t thread,
	      struct region vm_region, off64_t store_offset,
	      uintptr_t addr, l4_word_t access)
{
  /* XXX: Until we have a real swap server we (correctly) assume that
     if there is no memory then memory has just not yet been allocated
     (i.e. it is not just paged out).  */
  struct hurd_memory *memory = hurd_memory_alloc (vm_region.size);
  if (! memory)
    panic ("hurd_memory_alloc failed.");

  store_offset -= addr - vm_region.start;

  hurd_store_cache (store, store_offset, memory);
}

void
hurd_anonymous_system_init (void)
{
  static bool init_done;

  assert (! init_done);
  init_done = true;

  hurd_store_init (&anonymous_store, NULL, handle_fault);
}
