/* core.c - Core memory management.
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

#include "stdint.h"
#include "priv.h"

struct hurd_store core_store;

error_t
core_allocate (uintptr_t *address, size_t size,
	       uintptr_t flags, int map_now)
{
  /* XXX: This is wrong!  */
  return hurd_anonymous_allocate (address, size, flags, map_now);
}

static void
handle_fault (hurd_store_t *store, void *hook, l4_thread_id_t thread,
	      struct region vm_region, off64_t store_offset,
	      uintptr_t addr, l4_word_t access)
{
  panic ("handle_fault called for core store!");
}

void
core_system_init (void)
{
  static bool init_done;

  assert (! init_done);
  init_done = true;

  hurd_store_init (&core_store, NULL, handle_fault);
}
