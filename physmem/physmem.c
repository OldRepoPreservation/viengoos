/* Main function for physical memory server.
   Copyright (C) 2003 Free Software Foundation, Inc.
   Written by Marcus Brinkmann.

   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU Hurd is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>

#include "physmem.h"
#include "zalloc.h"


/* The program name.  */
char program_name[] = "physmem";


#define WORTEL_MSG_PUTCHAR		1
#define WORTEL_MSG_PANIC		2
#define WORTEL_MSG_GET_MEM		3
#define WORTEL_MSG_GET_CAP_REQUEST	4
#define WORTEL_MSG_GET_CAP_REPLY	5

void
get_all_memory (void)
{
  l4_fpage_t fpage;

  l4_accept (l4_map_grant_items (l4_complete_address_space));

  do
    {
      l4_msg_t msg;
      l4_msg_tag_t tag;
      l4_grant_item_t grant_item;

      l4_msg_clear (&msg);
      l4_set_msg_label (&msg, WORTEL_MSG_GET_MEM);
      /* FIXME: Use real cap_id.  */
      l4_msg_append_word (&msg, 0);
      l4_msg_load (&msg);
      /* FIXME: Hard coded wortel thread.  */
      tag = l4_call (l4_global_id (l4_thread_user_base () + 2, 1));
      if (l4_ipc_failed (tag))
	panic ("get_mem request failed during %s: %u",
	       l4_error_code () & 1 ? "receive" : "send",
	       (l4_error_code () >> 1) & 0x7);

      if (l4_untyped_words (tag) != 0
	  || l4_typed_words (tag) != 2)
	panic ("Invalid format of wortel get_mem reply");

      l4_msg_store (tag, &msg);
      l4_msg_get_grant_item (&msg, 0, &grant_item);
      fpage = grant_item.send_fpage;

      if (fpage.raw != l4_nilpage.raw)
	zfree (l4_address (fpage), l4_size (fpage));
    }
  while (fpage.raw != l4_nilpage.raw);
}


void
create_bootstrap_caps (void)
{
  l4_accept (l4_map_grant_items (l4_complete_address_space));

  while (1)
    {
      l4_msg_t msg;
      l4_msg_tag_t tag;
      unsigned int i;

      l4_msg_clear (&msg);
      l4_set_msg_label (&msg, WORTEL_MSG_GET_CAP_REQUEST);
      /* FIXME: Use real cap_id.  */
      l4_msg_append_word (&msg, 0);
      l4_msg_load (&msg);
      /* FIXME: Hard coded wortel thread.  */
      tag = l4_call (l4_global_id (l4_thread_user_base () + 2, 1));

      if (l4_ipc_failed (tag))
	panic ("get cap request failed during %s: %u",
	       l4_error_code () & 1 ? "receive" : "send",
	       (l4_error_code () >> 1) & 0x7);

      l4_msg_store (tag, &msg);

      if (l4_untyped_words (tag) == 1)
	{
	  /* This requests the master control capability.  */
	  if (l4_typed_words (tag))
	    panic ("Invalid format of wortel get cap request reply "
		   "for master control");

	  /* FIXME: Create capability.  */
	  l4_msg_clear (&msg);
	  l4_set_msg_label (&msg, WORTEL_MSG_GET_CAP_REPLY);
	  /* FIXME: Use our wortel cap here.  */
	  l4_msg_append_word (&msg, 0);
	  /* FIXME: Use our control cap for this task here.  */
	  l4_msg_append_word (&msg, 0xf00);
	  l4_msg_load (&msg);
	  /* FIXME: Hard coded thread ID.  */
	  l4_send (l4_global_id (l4_thread_user_base () + 2, 1));

	  /* This is the last request made.  */
	  return;
	}
      else if (l4_untyped_words (tag) != 3
	       || l4_typed_words (tag) == 0)
	panic ("Invalid format of wortel get cap request reply");

      debug ("Creating cap for 0x%x covering 0x%x to 0x%x:",
	     l4_msg_word (&msg, 0), l4_msg_word (&msg, 1),
	     l4_msg_word (&msg, 2));

      for (i = 0; i < l4_typed_words (tag); i += 2)
	{
	  l4_fpage_t fpage;
	  l4_grant_item_t grant_item;
	  l4_msg_get_grant_item (&msg, i, &grant_item);

	  fpage = grant_item.send_fpage;
	  if (l4_nilpage.raw == fpage.raw)
	    {
	      if (l4_typed_words (tag) == 2)
		{
		  /* FIXME: Create control capability for this one
		     task.  */
		  debug ("Can't create task control capability yet");
		}
	      else
		panic ("Invalid fpage in create bootstrap cap call");
	    }
	  debug ("0x%x ", fpage.raw);
	}
      debug ("\n");

      l4_msg_clear (&msg);
      l4_set_msg_label (&msg, WORTEL_MSG_GET_CAP_REPLY);

      /* FIXME: Use our wortel cap here.  */
      l4_msg_append_word (&msg, 0);
      /* FIXME: This must return the real capability ID.  */
      l4_msg_append_word (&msg, 0xa00);
      l4_msg_load (&msg);
      /* FIXME: Hard coded thread ID.  */
      l4_send (l4_global_id (l4_thread_user_base () + 2, 1));
    }
}


int
main (int argc, char *argv[])
{
  output_debug = 0;

  debug ("%s " PACKAGE_VERSION "\n", program_name);

  get_all_memory ();

  create_bootstrap_caps ();

  while (1)
    l4_sleep (l4_never);

  return 0;
}
