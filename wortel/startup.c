/* startup.c - Startup glue code.
   Copyright (C) 2003, 2004, 2005 Free Software Foundation, Inc.
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
#include <hurd/physmem.h>


/* Allocate SIZE bytes according to FLAGS into container CONTAINER
   starting with index START.  *AMOUNT contains the number of bytes
   successfully allocated.  */
static error_t
allocate (l4_thread_id_t server, hurd_cap_handle_t container,
	  l4_fpage_t start, l4_word_t size,
	  l4_word_t flags, l4_word_t *amount)
{
  l4_msg_t msg;
  l4_msg_tag_t tag;

  l4_msg_clear (msg);
  l4_set_msg_label (msg, hurd_pm_container_allocate_id);
  l4_msg_append_word (msg, container);
  l4_msg_append_word (msg, flags);
  l4_msg_append_word (msg, start);
  l4_msg_append_word (msg, size);

  l4_msg_load (msg);

  tag = l4_call (server);
  l4_msg_store (tag, msg);

  *amount = l4_msg_word (msg, 0);

  return l4_msg_label (msg);
}

/* Map the memory at offset OFFSET with size SIZE at address VADDR
   from the container CONT in the physical memory server PHYSMEM.  */
static error_t
map (l4_thread_id_t server, hurd_cap_handle_t container,
     l4_word_t offset, size_t size,
     void *vaddr, l4_word_t rights)
{
  l4_msg_t msg;
  l4_msg_tag_t tag;

  /* Magic!  If the offset is above 0x8000000 then we need to allocate
     anonymous memory.  */
  if (offset >= 0x8000000)
    {
      l4_word_t amount;
      allocate (server, container, offset, size, 0, &amount);
    }

  /* SIZE better is a multiple of the page size!  */
  while (size > 0)
    {
      int i;

      l4_msg_clear (msg);
      l4_set_msg_label (msg, hurd_pm_container_map_id);
      l4_msg_append_word (msg, container);
      l4_msg_append_word (msg, rights);
      l4_msg_append_word (msg, (l4_word_t) vaddr);
      l4_msg_append_word (msg, offset);
      l4_msg_append_word (msg, size);
      l4_msg_load (msg);

      tag = l4_call (server);
      l4_msg_store (tag, msg);

      /* Now check how much we actually got.  */
      i = 0;
      while (i < l4_typed_words (tag))
	{
	  l4_map_item_t map_item;

	  i += l4_msg_get_map_item (msg, i, &map_item);

	  /* This better doesn't underflow!  */
	  size -= l4_size (l4_map_item_snd_fpage (map_item));
	  offset += l4_size (l4_map_item_snd_fpage (map_item));
	  vaddr = (void *)
	    (((l4_word_t) vaddr) + l4_size (l4_map_item_snd_fpage (map_item)));
	}
    }
  return 0;
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
  map (startup->startup.server, startup->startup.cap_handle,
       0, HURD_STARTUP_SIZE, HURD_STARTUP_ADDR,
       L4_FPAGE_FULLY_ACCESSIBLE);

  for (i = 0; i < startup->mapc; i++)
    {
      struct hurd_startup_map *mapv = &startup->mapv[i];

      map (mapv->cont.server, mapv->cont.cap_handle,
	   mapv->offset & ~0x7, mapv->size, mapv->vaddr,
	   mapv->offset & 0x7);
    }

  (*(void (*) (struct hurd_startup_data *)) startup->entry_point) (startup);

  while (1)
    l4_sleep (L4_NEVER);
}
