/* startup.c - Startup glue code.
   Copyright (C) 2003, 2004 Free Software Foundation, Inc.
   Written by Marcus Brinkmann.

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
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA. */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <l4/globals.h>
#include <l4/init.h>
#include <l4/stubs.h>
#include <l4/stubs-init.h>

#include <l4.h>

#include <hurd/types.h>
#include <hurd/startup.h>


void
physmem_map (l4_thread_id_t physmem, hurd_cap_handle_t cont,
	     l4_word_t offset, l4_word_t size, void *vaddr)
{
  l4_msg_t msg;
  l4_msg_tag_t tag;

  l4_msg_clear (msg);
  l4_set_msg_label (msg, 128 /* PHYSMEM_MAP */);
  l4_msg_append_word (msg, cont);
  l4_msg_append_word (msg, offset);
  l4_msg_append_word (msg, size);
  l4_msg_append_word (msg, (l4_word_t) vaddr);
  l4_msg_load (msg);
  tag = l4_call (physmem);
  /* Check return.  */
}


/* Initialize libl4, setup the task, and pass control over to the main
   function.  */
void
cmain (struct hurd_startup_data *startup)
{
  unsigned int i;

  /* Initialize the system call stubs.  */
  l4_init ();
  l4_init_stubs ();

  /* Let physmem take over the address space completely.  */
  l4_accept (l4_map_grant_items (L4_COMPLETE_ADDRESS_SPACE));

  /* First map in the startup code from physmem, instead of having it
     mapped via the starter task.  FIXME: Consider using physmem as
     our pager via a specially marked container (see TODO).  */
  physmem_map (startup->startup.server, startup->startup.cap_handle,
	       L4_FPAGE_FULLY_ACCESSIBLE, HURD_STARTUP_SIZE,
	       HURD_STARTUP_ADDR);

  for (i = 0; i < startup->mapc; i++)
    {
      struct hurd_startup_map *mapv = &startup->mapv[i];

      physmem_map (mapv->cont.server, mapv->cont.cap_handle,
		   mapv->offset, mapv->size, mapv->vaddr);
    }

  (*(void (*) (struct hurd_startup_data *)) startup->entry_point) (startup);

  while (1)
    l4_sleep (L4_NEVER);
}
