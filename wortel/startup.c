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


/* Initialize libl4, setup the task, and pass control over to the main
   function.  */
void
cmain (l4_thread_id_t wortel_thread, l4_word_t wortel_cap_id,
       l4_thread_id_t physmem_server, 
       hurd_cap_handle_t startup_cont, hurd_cap_handle_t mem_cont,
       l4_word_t entry_point, l4_word_t header_loc, l4_word_t header_size)
{
  l4_msg_t msg;
  l4_msg_tag_t tag;

  /* Initialize the system call stubs.  */
  l4_init ();
  l4_init_stubs ();

  /* Probably replace this with direct l4_mr_load statements.  */
  l4_msg_clear (msg);
  l4_set_msg_label (msg, 1 /* PUTCHAR */);
  l4_msg_append_word (msg, wortel_cap_id);
  l4_msg_append_word (msg, '@');
  l4_msg_load (msg);

  tag = l4_call (wortel_thread);
  while (1)
    l4_sleep (L4_NEVER);
}
