/* thread.c - Manage threads.
   Copyright (C) 2004 Free Software Foundation, Inc.
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

#include <assert.h>

#include <l4.h>

#include <hurd/slab.h>

#include "task.h"


/* Initialize the slab object pointed to by BUFFER.  HOOK is as
   provided to hurd_slab_create.  */
static error_t
thread_constructor (void *hook, void *buffer)
{
  thread_t thread = (thread_t) buffer;

  thread->next = NULL;
  thread->thread_id = l4_nilthread;

  return 0;
}


/* The slab space containing all thread objects.  As this is the only
   place where we keep track of used and free thread IDs, it must
   never be reaped (so no destructor is needed).  */
static struct hurd_slab_space threads
  = HURD_SLAB_SPACE_INITIALIZER (struct thread, thread_constructor,
				 NULL, NULL);

/* The lock protecting the threads slab.  */
static pthread_mutex_t threads_lock = PTHREAD_MUTEX_INITIALIZER;


/* The thread numbers are allocated sequentially starting from a first
   number and ending at a maximum number, which are set by
   thread_set_range.  */
static l4_thread_id_t next_thread_id = l4_nilthread;
static l4_thread_id_t last_thread_id;


/* Set the range of thread IDs that we are allowed to allocate.  */
void
thread_set_range (l4_thread_id_t first, l4_thread_id_t last)
{
  pthread_mutex_lock (&threads_lock);
  next_thread_id = first;
  last_thread_id = last;  
  pthread_mutex_unlock (&threads_lock);
}



/* Allocate a new thread object with the thread ID THREAD_ID and
   return it in THREAD.  Only used at bootstrap.  */
error_t
thread_alloc_with_id (l4_thread_id_t thread_id, thread_t *r_thread)
{
  error_t err;
  thread_t thread;
  union
  {
    void *buffer;
    thread_t thread;
  } u;

  pthread_mutex_lock (&threads_lock);
  err = hurd_slab_alloc (&threads, &u.buffer);
  thread = u.thread;
  if (!err)
    {
      assert (thread->thread_id == l4_nilthread);

      thread->thread_id = thread_id;
    }
  pthread_mutex_unlock (&threads_lock);

  *r_thread = thread;
  return err;
}


/* Allocate a new thread object and return it in THREAD.  */
error_t
thread_alloc (thread_t *r_thread)
{
  error_t err;
  thread_t thread;
  union
  {
    void *buffer;
    thread_t thread;
  } u;

  pthread_mutex_lock (&threads_lock);
  err = hurd_slab_alloc (&threads, &u.buffer);
  thread = u.thread;
  if (__builtin_expect (!err, 1))
    {
      if (__builtin_expect (thread->thread_id == l4_nilthread, 0))
	{
	  if (__builtin_expect (next_thread_id == l4_nilthread, 0))
	    err = EAGAIN;
	  else
	    {
	      thread->thread_id = next_thread_id;

	      if (__builtin_expect (next_thread_id == last_thread_id, 0))
		next_thread_id = l4_nilthread;
	      else
		/* The version number is arbitrary here.  */
		next_thread_id
		  = l4_global_id (l4_thread_no (next_thread_id) + 1, 1);
	    }
	}
    }
  pthread_mutex_unlock (&threads_lock);

  *r_thread = thread;
  return err;
}


/* Deallocate the thread THREAD.  */
void
thread_dealloc (thread_t thread)
{
  pthread_mutex_lock (&threads_lock);
  hurd_slab_dealloc (&  threads, thread);
  pthread_mutex_unlock (&threads_lock);
}
