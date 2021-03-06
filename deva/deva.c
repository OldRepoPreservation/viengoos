/* deva.c - Main function for the deva server.
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

#include "deva.h"
#include "task-user.h"


/* Initialized by the machine-specific startup-code.  */
extern struct hurd_startup_data *__hurd_startup_data;


/* The program name.  */
char program_name[] = "deva";


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
  hurd_cap_obj_t console;

  l4_accept (L4_UNTYPED_WORDS_ACCEPTOR);

  /* FIXME: Allocate a system console driver.  */
  err = device_alloc (&console, DEVICE_CONSOLE);
  if (err)
    panic ("device_alloc: %i\n", err);
  hurd_cap_obj_unlock (console);

  while (1)
    {
      hurd_task_id_t task_id;
      bool master;

      task_id = wortel_get_deva_cap_request (&master);

      if (master)
	{
	  hurd_cap_obj_t serial;
	  hurd_cap_handle_t cap;

	  /* This requests the master control capability.  */

	  debug ("Creating console master device cap for 0x%x:", task_id);

	  /* FIXME: For now, we allocate a serial device for the "deva
	     master cap".  This is bogus, of course, but makes it easy
	     to use it in the initial server application without
	     having a proper deva interface to open new devices.  */
	  err = device_alloc (&serial, DEVICE_SERIAL);
	  if (err)
	    panic ("device_alloc: %i\n", err);
	  hurd_cap_obj_unlock (serial);

	  err = hurd_cap_bucket_inject (bucket, serial, task_id, &cap);
	  if (err)
	    panic ("hurd_cap_bucket_inject: %i\n", err);

	  debug (" 0x%x\n", cap);

	  /* Return CAP.  */
	  wortel_get_deva_cap_reply (cap);

	  /* This is the last request made.  */
	  break;
	}
      else
	{
	  hurd_cap_handle_t cap;

	  debug ("Creating console device cap for 0x%x:", task_id);

	  err = hurd_cap_bucket_inject (bucket, console, task_id, &cap);
	  if (err)
	    panic ("hurd_cap_bucket_inject: %i\n", err);

	  debug (" 0x%x\n", cap);

	  /* Return CAP.  */
	  wortel_get_deva_cap_reply (cap);
	}
    }

  hurd_cap_obj_lock (console);
  hurd_cap_obj_drop (console);
}


/* Get our task ID.  */
static l4_word_t
get_task_id ()
{
  return l4_version (l4_my_global_id ());
}


/* Initialize the thread support, and return the L4 thread ID to be
   used for the server thread.  */
static l4_thread_id_t
setup_threads (void)
{
  error_t err;
  l4_thread_id_t server_thread;
  l4_thread_id_t tid;
  l4_thread_id_t pager;
  pthread_t thread;
  struct hurd_startup_cap *task = &__hurd_startup_data->task;

  server_thread = l4_my_global_id ();
  err = task_thread_alloc (task->server, task->cap_handle,
			   (void *)
			   (l4_address (__hurd_startup_data->utcb_area)
			   + l4_utcb_size ()),
			   &tid);
  if (err)
    panic ("could not create main task thread: %i", err);

  /* Switch threads.  We still need the current main thread as the
     server thread.  */
  pager = l4_pager ();
  switch_thread (server_thread, tid);
  l4_set_pager (pager);

  /* Create the main thread.  */
  err = pthread_create (&thread, 0, 0, 0);
  if (err)
    panic ("could not create main thread: %i\n", err);

  /* FIXME: This is unecessary as soon as we implement this properly
     in pthread (of course, within the task server, we will use an
     override to not actually make an RPC to ourselves.  */

  /* Now add the remaining extra threads to the pool.  */
  err = task_thread_alloc (task->server, task->cap_handle,
			   (void *)
			   (l4_address (__hurd_startup_data->utcb_area)
			    + 2 * l4_utcb_size ()),
			   &tid);
  if (err)
    panic ("could not create first extra thread: %i", err);

  pthread_pool_add_np (tid);

  err = task_thread_alloc (task->server, task->cap_handle,
			   (void *)
			   (l4_address (__hurd_startup_data->utcb_area)
			    + 3 * l4_utcb_size ()),
			   &tid);
  if (err)
    panic ("could not create first extra thread: %i", err);

  pthread_pool_add_np (tid);

  /* One more for irq handlers (blech).  */
  err = task_thread_alloc (task->server, task->cap_handle,
			   (void *)
			   (l4_address (__hurd_startup_data->utcb_area)
			    + 4 * l4_utcb_size ()),
			   &tid);
  if (err)
    panic ("could not create first extra thread: %i", err);

  pthread_pool_add_np (tid);

  return server_thread;
}


void *
deva_server (void *arg)
{
  hurd_cap_bucket_t bucket = (hurd_cap_bucket_t) arg;
  error_t err;

  /* No (anonymous) root object is provided by the task server.  */
  /* FIXME: Use a worker timeout.  */
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

  output_debug = 1;

  debug ("%s " PACKAGE_VERSION "\n", program_name);

  server_thread = setup_threads ();

  err = device_class_init ();
  if (err)
    panic ("device_class_init: %i\n", err);

  err = hurd_cap_bucket_create (&bucket);
  if (err)
    panic ("bucket_create: %i\n", err);

  create_bootstrap_caps (bucket);

  /* Create the server thread and start serving RPC requests.  */
  err = pthread_create_from_l4_tid_np (&manager, NULL, server_thread,
				       deva_server, bucket);

  if (err)
    panic ("pthread_create_from_l4_tid_np: %i\n", err);
  pthread_detach (manager);

  bootstrap_final ();

  /* FIXME: get root filesystem cap (for loading drivers).  */

  /* FIXME: Eventually, add shutdown support on wortels(?)
     request.  */
  while (1)
    l4_sleep (L4_NEVER);

  return 0;
}
