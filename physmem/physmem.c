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
#include <sys/mman.h>
#include <pthread.h>

#include <hurd/wortel.h>

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


/* Initialized in main.  */
l4_thread_id_t wortel_thread_id;

/* FIXME: Hard coded cap ID.  */
wortel_cap_id_t wortel_cap_id = 0;


void
get_all_memory (void)
{
  l4_fpage_t fpage;

  do
    {
      fpage = wortel_get_mem ();

      if (fpage != L4_NILPAGE)
	zfree (l4_address (fpage), l4_size (fpage));
    }
  while (fpage != L4_NILPAGE);
}


void
create_bootstrap_caps (hurd_cap_bucket_t bucket)
{
  error_t err;
  hurd_cap_handle_t cap;
  hurd_cap_obj_t obj;

  l4_accept (l4_map_grant_items (L4_COMPLETE_ADDRESS_SPACE));

  while (1)
    {
      hurd_task_id_t task_id;
      unsigned int nr_fpages;
      l4_fpage_t fpages[L4_NUM_MRS / 2];

      task_id = wortel_get_cap_request (&nr_fpages, fpages);

      if (nr_fpages == 0)
	{
	  /* This requests the master control capability.  */

	  /* FIXME: Create capability.  */
	  /* FIXME: Use our control cap for this task here.  */
	  wortel_get_cap_reply (0xf00);

	  /* This is the last request made.  */
	  return;
	}
      else
	{
	  debug ("Creating cap for 0x%x:", task_id);

	  /* Create memory container for the provided grant items.  */
	  if (nr_fpages == 1 && fpages[0] == L4_NILPAGE)
	    {
	      /* FIXME: Create control capability for this one
		 task.  */
	      debug ("Can't create task control capability yet");
	    }
	  else
	    {
	      err = container_alloc (nr_fpages, fpages, &obj);

	      if (err)
		panic ("container_alloc: %i\n", err);
	      hurd_cap_obj_unlock (obj);

	      err = hurd_cap_bucket_inject (bucket, obj, task_id, &cap);
	      if (err)
		panic ("hurd_cap_bucket_inject: %i\n", err);

	      hurd_cap_obj_lock (obj);
	      hurd_cap_obj_drop (obj);

	      debug (" 0x%x\n", cap);

	      /* Return CAP.  */
	      wortel_get_cap_reply (cap);
	    }
	}
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

  extra_threads = wortel_get_threads ();
  if (extra_threads < 3)
    panic ("at least three extra threads required for physmem");

  /* Use the first extra thread as main thread.  */
  main_thread = l4_global_id (l4_thread_no (l4_my_global_id ()) + 1,
			      l4_version (l4_my_global_id ()));
  server_thread = l4_my_global_id ();

  /* Switch threads.  We still need the current main thread as the
     server thread.  */
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


/* FIXME:  Should be elsewhere.  Needed by libhurd-slab.  */
int
getpagesize ()
{
  return l4_min_page_size ();
}


void *
physmem_server (void *arg)
{
  hurd_cap_bucket_t bucket = (hurd_cap_bucket_t) arg;
  error_t err;

  /* The physical memory server can run out of threads at a time where
     the task server runs out of memory.  To avoid a dead-lock, we
     allocate worker threads asynchronously.  */
  hurd_cap_bucket_worker_alloc (bucket, true);

  /* No root object is provided by the physmem server.  */
  /* FIXME: Use a worker timeout (there is no issue: even if they
     timeout before the task server is up and running, the threads
     will be cached in pthread and are still available for
     allocation).  */
  err = hurd_cap_bucket_manage_mt (bucket, NULL, 0, 0);
  if (err)
    debug ("bucket_manage_mt failed: %i\n", err);

  panic ("bucket_manage_mt returned!");
}


static void
bootstrap_final (void)
{
  l4_thread_id_t task_server;
  hurd_cap_handle_t task_cap;
  l4_thread_id_t deva_server;
  hurd_cap_handle_t deva_cap;

  wortel_bootstrap_final (&task_server, &task_cap, &deva_server, &deva_cap);

  /* FIXME: Do something with the task cap.  */
}


int
main (int argc, char *argv[])
{
  error_t err;
  l4_thread_id_t server_thread;
  hurd_cap_bucket_t bucket;
  pthread_t manager;

  /* FIXME: Hard coded thread ID.  */
  wortel_thread_id = l4_global_id (l4_thread_user_base () + 2, 1);

  output_debug = 1;

  debug ("%s " PACKAGE_VERSION "\n", program_name);

  get_all_memory ();

  server_thread = setup_threads ();

  err = container_class_init ();
  if (err)
    panic ("container_class_init: %i\n", err);

  err = hurd_cap_bucket_create (&bucket);
  if (err)
    panic ("bucket_create: %i\n", err);

  create_bootstrap_caps (bucket);

  /* Create the server thread and start serving RPC requests.  */
  err = pthread_create_from_l4_tid_np (&manager, NULL, server_thread,
				       physmem_server, bucket);
  if (err)
    panic ("pthread_create_from_l4_tid_np: %i\n", err);
  pthread_detach (manager);

  bootstrap_final ();

  /* FIXME: Eventually, add shutdown support on wortels(?)
     request.  */
  while (1)
    l4_sleep (L4_NEVER);

  return 0;
}
