/* task-class.c - Task class for the task server.
   Copyright (C) 2003, 2004 Free Software Foundation, Inc.
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

#include <l4.h>
#include <hurd/cap-server.h>
#include <hurd/wortel.h>

#include "task.h"


static void
task_reinit (hurd_cap_class_t cap_class, hurd_cap_obj_t obj)
{
  task_t task = hurd_cap_obj_to_user (task_t, obj);
  unsigned int i;

  /* Destroy all threads.  */
  for (i = 0; i < task->nr_threads; i++)
    {
      /* FIXME: We are ignoring an error here.  */
      wortel_thread_control (task->threads[i], l4_nilthread, l4_nilthread,
			     l4_nilthread, (void *) -1);
      /* FIXME: Return the thread number to the list of free thread
	 numbers for future allocation.  */
    }

  /* FIXME: Return the task ID to the list of free task IDs for future
     allocation.  */
}


error_t
task_thread_alloc (hurd_cap_rpc_context_t ctx)
{
  return 0;
}


error_t
task_demuxer (hurd_cap_rpc_context_t ctx)
{
  error_t err = 0;

  switch (l4_msg_label (ctx->msg))
    {
      /* TASK_THREAD_ALLOC */
    case 256:
      err = task_thread_alloc (ctx);
      break;

    default:
      err = EOPNOTSUPP;
    }

  return err;
}



static struct hurd_cap_class task_class;

/* Initialize the task class subsystem.  */
error_t
task_class_init ()
{
  return hurd_cap_class_init (&task_class, task_t,
			      NULL, NULL, task_reinit, NULL,
			      task_demuxer);
}


/* Allocate a new task object with the task ID TASK_ID and the
   NR_THREADS threads listed in THREADS (which are already allocated
   for that task.  The object returned is locked and has one
   reference.  */
error_t
task_alloc (l4_word_t task_id, unsigned int nr_threads,
	    l4_thread_id_t *threads, task_t *r_task)
{
  error_t err;
  hurd_cap_obj_t obj;
  task_t task;

  err = hurd_cap_class_alloc (&task_class, &obj);
  if (err)
    return err;
  task = hurd_cap_obj_to_user (task_t, obj);

  task->task_id = task_id;
  assert (nr_threads <= MAX_THREADS);
  task->nr_threads = nr_threads;
  memcpy (task->threads, threads, sizeof (l4_thread_id_t) * nr_threads);

  *r_task = task;
  return 0;
}
