/* task-death.c - Task death notifications, implementation.
   Copyright (C) 2004 Free Software Foundation, Inc.
   Written by Marcus Brinkmann <marcus@gnu.org>

   This file is part of the GNU Hurd.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this program; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <pthread.h>

#include <hurd/types.h>
#include <hurd/task-death.h>


/* A lock that protects the linked list.  It also is held when
   callback handlers are called.  */
pthread_mutex_t hurd_task_death_notify_lock = PTHREAD_MUTEX_INITIALIZER;

/* The linked list of callback handlers.  */
struct hurd_task_death_notify_list_item *hurd_task_death_notify_list;


static void *
task_death_manager (void *unused)
{
  /* FIXME.  Needs to be implement when the task server supports
     it.  Do the following:

     unsigned int nr_task_ids;
     unsigned int i;
     hurd_task_id_t task_ids[nr_task_ids];

     struct hurd_task_death_notify_list_item *item;

     pthread_mutex_lock (&hurd_task_death_notify_lock);
     item = hurd_task_death_notify_list;
     while (item)
       {
         for (i = 0; i < nr_task_ids; i++)
           (*item->notify_handler) (item->hook, task_id[i]);
	 item = item->next;
       }
     pthread_mutex_unlock (&hurd_task_death_notify_lock);
     
  The only bit missing is the RPC loop to retrieve the dead task ids
  from the task server.  This can be a tight loop.  */

  return 0;
}


/* Start task death notifications.  Must be called once at startup.  */
error_t
hurd_task_death_notify_start (void)
{
  /* FIXME.  Needs to be implement when the task server supports it.
     Start the task_death_manager thread.  */

  return 0;
}
