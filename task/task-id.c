/* task-id.c - Manage task IDs.
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

#include <stddef.h>

#include "task.h"


/* The hash table mapping task IDs to tasks.  */
struct hurd_ihash task_id_to_task
  = HURD_IHASH_INITIALIZER (offsetof (struct task, locp));

/* The lock protecting the task_it_to_task hash table and associated
   data.  */
pthread_mutex_t task_id_to_task_lock = PTHREAD_MUTEX_INITIALIZER;

#define task_id_is_free(task_id) \
  (hurd_ihash_find (&task_id_to_task, (task_id)) == NULL)


/* Enter the task TASK under its ID into the hash table, consuming one
   reference.  Mainly used by the bootstrap functions.  */
error_t
task_id_enter (task_t task)
{
  error_t err;

  pthread_mutex_lock (&task_id_to_task_lock);
  err = hurd_ihash_add (&task_id_to_task, task->task_id, task);  
  pthread_mutex_unlock (&task_id_to_task_lock);

  return err;
}


/* Increment the task_id_next marker.  */
static inline hurd_task_id_t
task_id_inc (hurd_task_id_t task_id)
{
  /* We know that either the next task ID or the one after it is
     valid.  So we manually unroll the loop here.  */

  task_id++;
  if (! L4_THREAD_VERSION_VALID (task_id))
    task_id++;

  return task_id;
}


/* Find a free task ID, enter the task TASK into the hash table under
   this ID, consuming one reference, and return the new task ID.  If
   no free task ID is available, EAGAIN is returned.  */
error_t
task_id_add (task_t task, hurd_task_id_t *task_id_p)
{
  /* Zero is an invalid task ID.  But last_task_id will be incremented
     to the next valid task ID before the first allocation takes
     place.  This variable is protected by task_id_to_task_lock.  */
  static hurd_task_id_t last_task_id;
  error_t err = 0;
  hurd_task_id_t task_id;

  pthread_mutex_lock (&task_id_to_task_lock);

  /* Find next valid task ID.  */
  task_id = task_id_inc (last_task_id);

  if (__builtin_expect (! task_id_is_free (task_id), 0))
    {
      /* Slow path.  The next task ID is taken.  Skip forward until we
	 find a free one.  */

      /* The first task ID we tried.  */
      hurd_task_id_t first_task_id = task_id;

      do
	task_id = task_id_inc (task_id);
      while (task_id != first_task_id && !task_id_is_free (task_id));

      /* Check if we wrapped over and ended up where we started.  */
      if (task_id == first_task_id)
	err = EAGAIN;
    }

  if (__builtin_expect (!err, 1))
    {
      err = hurd_ihash_add (&task_id_to_task, task_id, task);
      if (__builtin_expect (!err, 1))
	{
	  task->task_id = task_id;
	  *task_id_p = task_id;
	  last_task_id = task_id;
	}
    }

  pthread_mutex_unlock (&task_id_to_task_lock);

  return err;
}
