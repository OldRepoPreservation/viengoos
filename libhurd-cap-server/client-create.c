/* class-destroy.c - Destroy a capability class.
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

#include <errno.h>
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>

#include <hurd/cap-server.h>


static error_t
_hurd_cap_client_alloc (hurd_cap_class_t cap_class,
			hurd_task_id_t task_id,
			hurd_cap_client_t *r_client)
{
  error_t err;
  hurd_cap_client_t client;

  err = hurd_slab_alloc (cap_class->client_slab, (void **) &client);
  if (!client)
    return errno;

  /* CLIENT->id will be initialized by the caller when adding the
     client to the client table of the class.  */
  client->task_id = task_id;
  err = pthread_mutex_init (&client->lock, NULL);
  if (err)
    {
      free (client);
      return err;
    }

  client->state = _HURD_CAP_STATE_GREEN;
  client->pending_rpcs = NULL;

  err = hurd_table_init (&client->caps, sizeof (struct _hurd_cap_entry));
  if (err)
    {
      pthread_mutex_destroy (&client->lock);
      free (client);
      return err;
    }
  hurd_ihash_init (&client->caps_reverse, 0);

  *r_client = client;
  return 0;
}


/* Look up the client with the task ID TASK in the class CLASS, and
   return it in R_CLIENT, with one additional reference.  If it is not
   found, create it.  */
error_t
__attribute__((visibility("hidden")))
_hurd_cap_client_create (hurd_cap_class_t cap_class,
			 hurd_task_id_t task_id,
			 hurd_cap_id_t *r_idx,
			 hurd_cap_client_t *r_client)
{
  error_t err = 0;
  hurd_cap_client_t client;
  _hurd_cap_client_entry_t entry;
  struct _hurd_cap_client_entry new_entry;

  pthread_mutex_lock (&cap_class->lock);
  client = (hurd_cap_client_t) hurd_ihash_find (&cap_class->clients_reverse,
						task_id);
  if (client)
    {
      entry = (_hurd_cap_client_entry_t)
	HURD_TABLE_LOOKUP (&cap_class->clients, client->id);
      if (entry->dead)
	err = EINVAL;	/* FIXME: A more appropriate code?  */
      else
	{
	  entry->refs++;
	  *r_idx = client->id;
	  *r_client = entry->client;
	}
      pthread_mutex_unlock (&cap_class->lock);
      return err;
    }
  pthread_mutex_unlock (&cap_class->lock);

  /* The client is not yet registered.  Block out processing task
     death notifications, create a new client structure, and then
     enter it into the table before resuming task death
     notifications.  */
  hurd_task_death_notify_suspend ();
  err = _hurd_cap_client_alloc (cap_class, task_id, r_client);
  if (err)
    {
      hurd_task_death_notify_resume ();
      return err;
    }

  pthread_mutex_lock (&cap_class->lock);
  /* Somebody else might have been faster.  */
  client = (hurd_cap_client_t) hurd_ihash_find (&cap_class->clients_reverse,
						task_id);
  if (client)
    {
      entry = (_hurd_cap_client_entry_t)
	HURD_TABLE_LOOKUP (&cap_class->clients, client->id);
      if (entry->dead)
	err = EINVAL;	/* FIXME: A more appropriate code?  */
      else
	{
	  /* Somebody else was indeed faster.  Use the existing entry.  */
	  entry->refs++;
	  *r_idx = client->id;
	  *r_client = entry->client;
	}
      pthread_mutex_unlock (&cap_class->lock);
      _hurd_cap_client_dealloc (cap_class, *r_client);
      return err;
    }

  client = *r_client;

  /* Enter the new client.  */
  new_entry.client = client;
  /* One reference for the fact that the client task lives, one for
     the caller. */
  new_entry.refs = 2;

  err = hurd_table_enter (&cap_class->clients, &new_entry, &client->id);
  if (!err)
    {
      err = hurd_ihash_add (&cap_class->clients_reverse, task_id, client);
      if (err)
	hurd_table_remove (&cap_class->clients, client->id);
    }
  if (err)
    {
      pthread_mutex_unlock (&cap_class->lock);
      hurd_task_death_notify_resume ();
      
      _hurd_cap_client_dealloc (cap_class, client);
      return err;
    }
  pthread_mutex_unlock (&cap_class->lock);
  hurd_task_death_notify_resume ();

  *r_idx = client->id;

  return 0;
}
