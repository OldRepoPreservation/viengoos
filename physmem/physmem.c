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
#include <pthread.h>

#include "physmem.h"
#include "zalloc.h"


/* The program name.  */
char program_name[] = "physmem";


/* The following functions are required by pthread.  */

void
__attribute__ ((__noreturn__))
exit (int __status)
{
  panic ("exit() called");
}


void
abort (void)
{
  panic ("abort() called");
}


#define WORTEL_MSG_PUTCHAR		1
#define WORTEL_MSG_PANIC		2
#define WORTEL_MSG_GET_MEM		3
#define WORTEL_MSG_GET_CAP_REQUEST	4
#define WORTEL_MSG_GET_CAP_REPLY	5
#define WORTEL_MSG_GET_THREADS		6

void
get_all_memory (void)
{
  l4_fpage_t fpage;

  l4_accept (l4_map_grant_items (L4_COMPLETE_ADDRESS_SPACE));

  do
    {
      l4_msg_t msg;
      l4_msg_tag_t tag;
      l4_grant_item_t grant_item;

      l4_msg_clear (msg);
      l4_set_msg_label (msg, WORTEL_MSG_GET_MEM);
      /* FIXME: Use real cap_id.  */
      l4_msg_append_word (msg, 0);
      l4_msg_load (msg);
      /* FIXME: Hard coded wortel thread.  */
      tag = l4_call (l4_global_id (l4_thread_user_base () + 2, 1));
      if (l4_ipc_failed (tag))
	panic ("get_mem request failed during %s: %u",
	       l4_error_code () & 1 ? "receive" : "send",
	       (l4_error_code () >> 1) & 0x7);

      if (l4_untyped_words (tag) != 0
	  || l4_typed_words (tag) != 2)
	panic ("Invalid format of wortel get_mem reply");

      l4_msg_store (tag, msg);
      l4_msg_get_grant_item (msg, 0, &grant_item);
      fpage = l4_grant_item_snd_fpage (grant_item);

      if (fpage != L4_NILPAGE)
	zfree (l4_address (fpage), l4_size (fpage));
    }
  while (fpage != L4_NILPAGE);
}


void
create_bootstrap_caps (void)
{
  l4_accept (l4_map_grant_items (L4_COMPLETE_ADDRESS_SPACE));

  while (1)
    {
      l4_msg_t msg;
      l4_msg_tag_t tag;
      unsigned int i;

      l4_msg_clear (msg);
      l4_set_msg_label (msg, WORTEL_MSG_GET_CAP_REQUEST);
      /* FIXME: Use real cap_id.  */
      l4_msg_append_word (msg, 0);
      l4_msg_load (msg);
      /* FIXME: Hard coded wortel thread.  */
      tag = l4_call (l4_global_id (l4_thread_user_base () + 2, 1));

      if (l4_ipc_failed (tag))
	panic ("get cap request failed during %s: %u",
	       l4_error_code () & 1 ? "receive" : "send",
	       (l4_error_code () >> 1) & 0x7);

      l4_msg_store (tag, msg);

      if (l4_untyped_words (tag) == 1)
	{
	  /* This requests the master control capability.  */
	  if (l4_typed_words (tag))
	    panic ("Invalid format of wortel get cap request reply "
		   "for master control");

	  /* FIXME: Create capability.  */
	  l4_msg_clear (msg);
	  l4_set_msg_label (msg, WORTEL_MSG_GET_CAP_REPLY);
	  /* FIXME: Use our wortel cap here.  */
	  l4_msg_append_word (msg, 0);
	  /* FIXME: Use our control cap for this task here.  */
	  l4_msg_append_word (msg, 0xf00);
	  l4_msg_load (msg);
	  /* FIXME: Hard coded thread ID.  */
	  l4_send (l4_global_id (l4_thread_user_base () + 2, 1));

	  /* This is the last request made.  */
	  return;
	}
      else if (l4_untyped_words (tag) != 3
	       || l4_typed_words (tag) == 0)
	panic ("Invalid format of wortel get cap request reply");

      debug ("Creating cap for 0x%x covering 0x%x to 0x%x:",
	     l4_msg_word (msg, 0), l4_msg_word (msg, 1),
	     l4_msg_word (msg, 2));

      for (i = 0; i < l4_typed_words (tag); i += 2)
	{
	  l4_fpage_t fpage;
	  l4_grant_item_t grant_item;
	  l4_msg_get_grant_item (msg, i, &grant_item);

	  fpage = l4_grant_item_snd_fpage (grant_item);
	  if (fpage == L4_NILPAGE)
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
	  debug ("0x%x ", fpage);
	}
      debug ("\n");

      l4_msg_clear (msg);
      l4_set_msg_label (msg, WORTEL_MSG_GET_CAP_REPLY);

      /* FIXME: Use our wortel cap here.  */
      l4_msg_append_word (msg, 0);
      /* FIXME: This must return the real capability ID.  */
      l4_msg_append_word (msg, 0xa00);
      l4_msg_load (msg);
      /* FIXME: Hard coded thread ID.  */
      l4_send (l4_global_id (l4_thread_user_base () + 2, 1));
    }
}


/* Initialize the thread support, and return the L4 thread ID to be
   used for the server thread.  */
static l4_thread_id_t
setup_threads (void)
{
  int err;
  pthread_t thread;
  l4_thread_id_t server_thread;
  l4_thread_id_t main_thread;
  l4_word_t extra_threads;

  {
    l4_msg_t msg;
    l4_msg_tag_t tag;

    l4_accept (L4_UNTYPED_WORDS_ACCEPTOR);

    l4_msg_clear (msg);
    l4_set_msg_label (msg, WORTEL_MSG_GET_THREADS);
    /* FIXME: Use real cap_id.  */
    l4_msg_append_word (msg, 0);

    l4_msg_load (msg);
    /* FIXME: Hard coded wortel thread.  */
    tag = l4_call (l4_global_id (l4_thread_user_base () + 2, 1));

    if (l4_ipc_failed (tag))
      panic ("get_thread request failed during %s: %u",
	     l4_error_code () & 1 ? "receive" : "send",
	     (l4_error_code () >> 1) & 0x7);

    if (l4_untyped_words (tag) != 1
	|| l4_typed_words (tag) != 0)
      panic ("invalid format of wortel get_thread reply");

    l4_msg_store (tag, msg);
    l4_msg_get_word (msg, 0, &extra_threads);
    if (extra_threads < 3)
      panic ("at least three extra threads required for physmem");
  }

  /* Use the first extra thread as main thread.  */
  main_thread = l4_global_id (l4_thread_no (l4_my_global_id ()) + 1,
			      l4_version (l4_my_global_id ()));
  server_thread = l4_my_global_id ();

  /* Switch threads.  We still need the main thread as the server
     thread.  */
  l4_set_pager_of (main_thread, l4_pager ());
  switch_thread (server_thread, main_thread);

  /* Create the main thread.  */
  err = pthread_create (&thread, 0, 0, 0);

  if (err)
    panic ("could not create main thread: %i\n", err);

  /* Now add the remaining extra threads to the pool.  */
  while (--extra_threads > 0)
    {
      l4_thread_id_t tid;
      tid = l4_global_id (l4_thread_no (l4_my_global_id ()) + extra_threads,
			  l4_version (l4_my_global_id ()));
      pthread_pool_add_np (tid);
    }

  return server_thread;
}


int
main (int argc, char *argv[])
{
  l4_thread_id_t server_thread;

  output_debug = 1;

  debug ("%s " PACKAGE_VERSION "\n", program_name);

  get_all_memory ();

  server_thread = setup_threads ();

  create_bootstrap_caps ();

  while (1)
    l4_sleep (L4_NEVER);

  return 0;
}
