/* task-death.h - Task death notifications, interface.
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

#ifndef _HURD_TASK_DEATH_H
#define _HURD_TASK_DEATH_H	1

#include <pthread.h>


/* We need to keep track of task deaths, because for IPC security we
   hold task info capabilities which block reuse of the respective
   task ID.  At task death, we have to release these task info
   capabilities so they become free for reuse.  The task server
   provides an interface to get the task IDs of all dead tasks to
   which we still hold task info capabilities.

   The following convention applies: Before you start allocating task
   info capabilities, you must register a task death notify handler.
   While you are requesting new task info capabilities and registering
   it with your notify handler, you must take the
   hurd_task_death_notify_lock to prevent task death notifications
   from being processed (FIXME: Write a wrapper function for the task
   server RPC to do this).  You can release task info capabilities at
   any time.  However, if your notify handler is called, you MUST
   release any task info capability you hold for that task ID.  */


/* The type of a function callback that you can use to be informed
   about task deaths.  */
typedef void (*task_death_notify_t) (void *hook, hurd_task_id_t task_id);

/* The struct you have to use to add your own notification
   handler.  */
struct hurd_task_death_notify_list_item
{
  /* The following two members are internal.  */
  struct hurd_task_death_notify_list_item *next;
  struct hurd_task_death_notify_list_item **prevp;

  /* Your callback handler.  */
  task_death_notify_t *notify_handler;

  /* This is passed as the first argument to your callback
     handler.  */
  void *hook;
};


/* A lock that protects the linked list.  It also is held when
   callback handlers are called.  */
extern pthread_mutex_t hurd_task_death_notify_lock;

/* The linked list of callback handlers.  */
extern struct hurd_task_death_notify_list_item *hurd_task_death_notify_list;


/* Start task death notifications.  Must be called once at startup.  */
error_t hurd_task_death_notify_start (void);


/* Add the callback handler ITEM to the list.  */
static inline void
hurd_task_death_notify_add (struct hurd_task_death_notify_list_item *item)
{
  pthread_mutex_lock (&hurd_task_death_notify_lock);
  if (hurd_task_death_notify_list)
    hurd_task_death_notify_list->prevp = &item->next;
  item->prevp = &hurd_task_death_notify_list;
  item->next = hurd_task_death_notify_list;
  hurd_task_death_notify_list = item;
  pthread_mutex_unlock (&hurd_task_death_notify_lock);
};


/* Remove the callback handler ITEM from the list.  */
static inline void
hurd_task_death_notify_remove (struct hurd_task_death_notify_list_item *item)
{
  pthread_mutex_lock (&hurd_task_death_notify_lock);
  if (item->next)
    item->next->prevp = item->prevp;
  *(item->prevp) = item->next;
  pthread_mutex_unlock (&hurd_task_death_notify_lock);
};


/* Suspend processing task death notifications.  Call this while
   acquiring new task info capabilities and registering them with your
   notify handler.  */
static inline void
hurd_task_death_notify_suspend (void)
{
  pthread_mutex_lock (&hurd_task_death_notify_lock);
}

/* Resumes processing task death notifications.  Call this after
   acquiring new task info capabilities and registering them with your
   notify handler.  */
static inline void
hurd_task_death_notify_resume (void)
{
  pthread_mutex_unlock (&hurd_task_death_notify_lock);
}

#endif	/* _HURD_TASK_DEATH_H */
