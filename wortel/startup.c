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


struct map_item
{
  /* Container offset and access permission.  */
  l4_word_t offset;

  l4_word_t size;

  l4_word_t vaddr;

  l4_word_t cont;
};


void
physmem_map (l4_thread_id_t physmem, hurd_cap_id_t cont,
	     l4_word_t offset, l4_word_t size,
	     l4_word_t vaddr)
{
  l4_msg_t msg;
  l4_msg_tag_t tag;

  l4_msg_clear (msg);
  l4_set_msg_label (msg, 128 /* PHYSMEM_MAP */);
  l4_msg_append_word (msg, cont);
  l4_msg_append_word (msg, offset);
  l4_msg_append_word (msg, size);
  l4_msg_append_word (msg, vaddr);
  l4_msg_load (msg);
  tag = l4_call (physmem);
  /* Check return.  */
}


/* Initialize libl4, setup the task, and pass control over to the main
   function.  */
void
cmain (int argc, char *argv[],
       l4_thread_id_t wortel_thread, l4_word_t wortel_id,
       l4_thread_id_t physmem_server, 
       hurd_cap_handle_t startup_cont, hurd_cap_handle_t mem_cont,
       l4_word_t mapc, struct map_item mapv[],
       l4_word_t entry_point, l4_word_t header_loc, l4_word_t header_size)
{
  int i;

  /* Initialize the system call stubs.  */
  l4_init ();
  l4_init_stubs ();

  /* Let physmem take over the address space completely.  */
  l4_accept (l4_map_grant_items (L4_COMPLETE_ADDRESS_SPACE));

  physmem_map (physmem_server, startup_cont,
	       L4_FPAGE_FULLY_ACCESSIBLE, 32*1024, 32*1024);

  for (i = 0; i < mapc; i++)
    physmem_map (physmem_server, mapv[i].cont,
		 mapv[i].offset, mapv[i].size, mapv[i].vaddr);

  (*(void (*) (void)) entry_point) ();

  while (1)
    l4_sleep (L4_NEVER);
}
