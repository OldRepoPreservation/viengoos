/* Main function for the task server.
   Copyright (C) 2004 Free Software Foundation, Inc.
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

#include <hurd/startup.h>
#include <hurd/wortel.h>

#include "task.h"


/* Initialized by the machine-specific startup-code.  */
extern struct hurd_startup_data *__hurd_startup_data;


/* The program name.  */
char program_name[] = "task";


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


/* FIXME:  Should be elsewhere.  Needed by libhurd-slab.  */
int
getpagesize()
{
  return l4_min_page_size ();
}


void
create_bootstrap_caps (hurd_cap_bucket_t bucket)
{
  error_t err;
  hurd_cap_handle_t cap;
  hurd_cap_handle_t startup_cap;
  hurd_cap_obj_t obj;

  l4_accept (L4_UNTYPED_WORDS_ACCEPTOR);

  while (1)
    {
      hurd_task_id_t task_id;
      unsigned int nr_threads;
      l4_thread_id_t threads[L4_NUM_MRS];

      task_id = wortel_get_task_cap_request (&nr_threads, threads);

      if (nr_threads == 0)
	{
	  /* This requests the master control capability.  */

	  /* FIXME: Create capability.  */
	  /* FIXME: Use our control cap for this task here.  */
	  wortel_get_task_cap_reply (0xf00);

	  /* This is the last request made.  */
	  return;
	}
      else
	{
	  debug ("Creating task cap for 0x%x:", task_id);

	  err = task_alloc (task_id, nr_threads, threads, &obj);

	  if (err)
	    panic ("task_alloc: %i\n", err);
	  hurd_cap_obj_unlock (obj);

	  err = hurd_cap_bucket_inject (bucket, obj, task_id, &cap);
	  if (err)
	    panic ("hurd_cap_bucket_inject: %i\n", err);

	  hurd_cap_obj_lock (obj);
	  hurd_cap_obj_drop (obj);

	  debug (" 0x%x\n", cap);

	  /* Return CAP.  */
	  wortel_get_task_cap_reply (cap);
	}
    }
}


/* Get our task ID.  */
static l4_word_t
get_task_id ()
{
  return l4_version (l4_my_global_id ());
}


/* The first free thread number.  */
l4_word_t first_free_thread_no;

/* Initialize the thread support, and return the L4 thread ID to be
   used for the server thread.  */
static l4_thread_id_t
setup_threads (void)
{
  l4_word_t err;
  pthread_t thread;
  l4_thread_id_t server_thread;
  l4_thread_id_t main_thread;
  l4_thread_id_t extra_thread;
  l4_thread_id_t pager;

  first_free_thread_no = wortel_get_first_free_thread_no ();

  /* Use the first free thread as main thread.  */
  main_thread = l4_global_id (first_free_thread_no, get_task_id ());
  server_thread = l4_my_global_id ();

  /* Create the main thread as an active thread.  The scheduler is
     us.  */
  err = wortel_thread_control (main_thread, l4_myself (), l4_myself (),
			       main_thread,
			       (void *)
			       (l4_address (__hurd_startup_data->utcb_area)
				+ l4_utcb_size ()));
  if (err)
    panic ("could not create main task thread: %s", l4_strerror (err));

  /* Switch threads.  We still need the current main thread as the
     server thread.  */
  pager = l4_pager ();
  switch_thread (server_thread, main_thread);
  l4_set_pager (pager);

  /* Create the main thread.  */
  err = pthread_create (&thread, 0, 0, 0);

  if (err)
    panic ("could not create main thread: %i\n", err);

  /* FIXME: This is unecessary as soon as we implement this properly
     in pthread (of course, within the task server, we will use an
     override to not actually make an RPC to ourselves.  */

  /* Now add the remaining extra threads to the pool.  */
  extra_thread = l4_global_id (first_free_thread_no + 1, get_task_id ());
  err = wortel_thread_control (extra_thread, l4_myself (), l4_myself (),
			       extra_thread,
			       (void *)
			       (l4_address (__hurd_startup_data->utcb_area)
				+ 2 * l4_utcb_size ()));
  pthread_pool_add_np (extra_thread);

  extra_thread = l4_global_id (first_free_thread_no + 2, get_task_id ());
  err = wortel_thread_control (extra_thread, l4_myself (), l4_myself (),
			       extra_thread,
			       (void *)
			       (l4_address (__hurd_startup_data->utcb_area)
				+ 3 * l4_utcb_size ()));
  pthread_pool_add_np (extra_thread);

  return server_thread;
}


void *
task_server (void *arg)
{
  hurd_cap_bucket_t bucket = (hurd_cap_bucket_t) arg;
  error_t err;

  /* No root object is provided by the task server.  */
  /* FIXME: Use a worker timeout.  */
  err = hurd_cap_bucket_manage_mt (bucket, NULL, 0, 0);
  if (err)
    debug ("bucket_manage_mt failed: %i\n", err);

  panic ("bucket_manage_mt returned!");
}


int
main (int argc, char *argv[])
{
  error_t err;
  l4_thread_id_t server_thread;
  hurd_cap_bucket_t bucket;
  pthread_t manager;

  output_debug = 1;

  debug ("%s " PACKAGE_VERSION "\n", program_name);

  server_thread = setup_threads ();

  /* FIXME: Start the scheduler.  */

  err = task_class_init ();
  if (err)
    panic ("task_class_init: %i\n", err);

  err = hurd_cap_bucket_create (&bucket);
  if (err)
    panic ("bucket_create: %i\n", err);

  create_bootstrap_caps (bucket);

  /* Create the server thread and start serving RPC requests.  */
  err = pthread_create_from_l4_tid_np (&manager, NULL, server_thread,
				       task_server, bucket);

  if (err)
    panic ("pthread_create_from_l4_tid_np: %i\n", err);
  pthread_detach (manager);

  /* FIXME: get device cap.  */

  /* FIXME: Eventually, add shutdown support on wortels(?)
     request.  */
  while (1)
    l4_sleep (L4_NEVER);

  return 0;
}
