/* mm-init.h - Memory management initialization.
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
#include <stdbool.h>
#include <l4.h>
#include <hurd/startup.h>

#include "mm.h"
#include "vm.h"

#include "priv.h"

/* Physmem's server thread.  */
l4_thread_id_t physmem;

/* By default, we allocate all anonymous memory in this container.  */
hurd_pm_container_t default_container;

/* True once the MM is up and running.  */
int mm_init_done;

/* Initialized by the machine-specific startup-code.  */
extern struct hurd_startup_data *__hurd_startup_data;

/* Initialize the memory management subsystem.  */
void
hurd_mm_init (l4_thread_id_t pager_tid)
{
  error_t err;
  uintptr_t stack;

  /* FIXME: The image server may not be (and will likely not be)
     physmem.  */
  physmem = __hurd_startup_data->image.server;

  /* Create the primary anonymous memory container.  */
  /* FIXME: We use the image cap handle as the memory control object.
     This is wrong.  */
  err = hurd_pm_container_create (__hurd_startup_data->image.cap_handle,
				  &default_container);
  assert_perror (err);

  swap_store.server = physmem;
  swap_store.handle = default_container;
  hurd_btree_frame_tree_init (&swap_store.frames);

  /* Initialize the mapping database.  */
  map_init ();

  /* The mapping database is now bootstrapped.  */
  mm_init_done = 1;

  /* Allocate a stack for the pager thread.  */
  err = hurd_vm_allocate (&stack, getpagesize (), 0, 1);
  if (err)
    panic ("Unable to allocate a stack for the pager thread.\n");

  /* Start it up.  */
  l4_start_sp_ip (pager_tid, stack + getpagesize (), (l4_word_t) pager);

  /* XXX: EVIL.  If a thread causes a page fault before its pager is
     waiting for faults, the pager will not get the correct message
     contents.  This is an evil work around to make sure that the
     pager is up before we return.  */
  int i;
  for (i = 0; i < 100; i ++)
    l4_yield ();

  l4_set_pager (pager_tid);
}
