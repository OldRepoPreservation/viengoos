/* task.h - Generic definitions.
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

#ifndef TASK_H
#define TASK_H	1

#include <errno.h>

#include <l4.h>
#include <hurd/cap-server.h>
#include <hurd/ihash.h>

#include "output.h"


/* The program name.  */
extern char program_name[];

#define BUG_ADDRESS	"<bug-hurd@gnu.org>"

int main (int argc, char *argv[]);


/* The following function must be defined by the architecture
   dependent code.  */

/* Switch execution transparently to thread TO.  The thread FROM,
   which must be the current thread, will be halted.  */
void switch_thread (l4_thread_id_t from, l4_thread_id_t to);


/* Task objects.  */

struct task
{
  /* This is for fast removal from the task_id_to_task hash table.  */
  hurd_ihash_locp_t locp;

  /* The task ID is used in the version field of the global thread ID,
     so it is limited to L4_THREAD_VERSION_BITS (14/32) bits and must
     not have its lower 6 bits set to all zero (because that indicates
     a local thread ID).  */
  l4_word_t task_id;

  /* FIXME: Just for testing and dummy stuff: A small table of the
     threads in this task.  */
#define MAX_THREADS 4
  l4_thread_id_t threads[MAX_THREADS];
  unsigned int nr_threads;
};
typedef struct task *task_t;


/* Initialize the task class subsystem.  */
error_t task_class_init ();

/* Allocate a new task object with the task ID TASK_ID and the
   NR_THREADS threads listed in THREADS (which are already allocated
   for that task.  The object returned is locked and has one
   reference.  */
error_t task_alloc (l4_word_t task_id, unsigned int nr_threads,
		    l4_thread_id_t *threads, task_t *r_task);


extern pthread_mutex_t task_id_to_task_lock;

/* The hash table mapping task IDs to tasks.  */
extern struct hurd_ihash task_id_to_task;

/* Acquire a reference for the task with the task ID TASK_ID and
   return the task object.  If the task ID is not valid, return
   NULL.  */
static inline task_t
task_id_get_task (hurd_task_id_t task_id)
{
  task_t task;

  pthread_mutex_lock (&task_id_to_task_lock);
  task = hurd_ihash_find (&task_id_to_task, task_id);
  if (task)
    {
      hurd_cap_obj_t obj = hurd_cap_obj_from_user (task_t, task);
      hurd_cap_obj_ref (obj);
    }
  pthread_mutex_unlock (&task_id_to_task_lock);

  return task;
}


/* Enter the task TASK under its ID into the hash table, consuming one
   reference.  Mainly used by the bootstrap functions.  */
error_t task_id_enter (task_t task);

/* Find a free task ID, enter the task TASK (which must not be locked)
   into the hash table under this ID, acquiring reference.  The new
   task ID is returned in TASK_ID.  If no free task ID is available,
   EAGAIN is returned.  */
error_t task_id_add (task_t task, hurd_task_id_t *task_id_p);

#endif	/* TASK_H */
