/* client-create.c - Create a capability client.
   Copyright (C) 2004, 2005 Free Software Foundation, Inc.
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

#include "cap-server-intern.h"


/* Client management code.  */

/* Allocate a new capability client structure for the slab cache.  */
static error_t
_hurd_cap_client_constructor (void *hook, void *buffer)
{
  _hurd_cap_client_t client = (_hurd_cap_client_t) buffer;
  error_t err;

  err = pthread_mutex_init (&client->lock, NULL);
  if (err)
    return err;

  client->state = _HURD_CAP_STATE_GREEN;
  client->pending_rpcs = NULL;

  /* Capabilities are mapped to clients many to many, so we can not
     use a location pointer.  However, this is not critical as
     removing an entry only blocks out RPCs for the same client, and
     not others.  */
  hurd_ihash_init (&client->caps_reverse, HURD_IHASH_NO_LOCP);

  return 0;

  /* This is provided here in case you add more initialization to the
     end of the above code.  */
#if 0
  pthread_mutex_destroy (&client->lock);

  return err;
#endif
}


/* Allocate a new capability client structure for the slab cache.  */
static void
_hurd_cap_client_destructor (void *hook, void *buffer)
{
  _hurd_cap_client_t client = (_hurd_cap_client_t) buffer;

  hurd_ihash_destroy (&client->caps_reverse);
  hurd_table_destroy (&client->caps);
  pthread_mutex_destroy (&client->lock);
}


/* The global slab for all capability clients.  */
struct hurd_slab_space _hurd_cap_client_space
  = HURD_SLAB_SPACE_INITIALIZER (struct _hurd_cap_client, NULL, NULL,
				 _hurd_cap_client_constructor,
				 _hurd_cap_client_destructor, NULL);


static error_t
_hurd_cap_client_alloc (hurd_task_id_t task_id,
			_hurd_cap_client_t *r_client)
{
  error_t err;
  void *new_client;
  _hurd_cap_client_t client;

  err = hurd_slab_alloc (&_hurd_cap_client_space, &new_client);
  if (err)
    return err;

  client = new_client;

  /* CLIENT->id will be initialized by the caller when adding the
     client to the client table of the class.  */
  client->task_id = task_id;

  err = hurd_table_init (&client->caps, sizeof (_hurd_cap_obj_entry_t));
  if (err)
    {
      free (client);
      return err;
    }

  /* FIXME: We need to acquire a task info capability here.  The task
     death notifications have been suspended by the caller.  */

  *r_client = client;
  return 0;
}


/* Look up the client with the task ID TASK in the bucket BUCKET, and
   return it in R_CLIENT, with one additional reference.  If it is not
   found, create it.  */
error_t
__attribute__((visibility("hidden")))
_hurd_cap_client_create (hurd_cap_bucket_t bucket,
			 hurd_task_id_t task_id,
			 _hurd_cap_client_t *r_client)
{
  error_t err = 0;
  _hurd_cap_client_t client;

  pthread_mutex_lock (&bucket->lock);
  client = (_hurd_cap_client_t) hurd_ihash_find (&bucket->clients_reverse,
						 task_id);
  if (client)
    {
      if (client->dead)
	err = EINVAL;	/* FIXME: A more appropriate code?  */
      else
	{
	  client->refs++;
	  *r_client = client;
	}
      pthread_mutex_unlock (&bucket->lock);
      return err;
    }
  pthread_mutex_unlock (&bucket->lock);

  /* The client is not yet registered.  Block out processing task
     death notifications, create a new client structure, and then
     enter it into the table before resuming task death
     notifications.  */
  hurd_task_death_notify_suspend ();
  err = _hurd_cap_client_alloc (task_id, r_client);
  if (err)
    {
      hurd_task_death_notify_resume ();
      return err;
    }

  pthread_mutex_lock (&bucket->lock);
  /* Since we dropped the bucket lock during the allocation (which is
     potentially long) we need to check that somebody else didn't
     already allocate a client data structure.  If so, we can just use
     that.  Otherwise, we continue.  */
  client = (_hurd_cap_client_t) hurd_ihash_find (&bucket->clients_reverse,
						 task_id);
  if (client)
    {
      if (client->dead)
	{
	  err = EINVAL;	/* FIXME: A more appropriate code?  */
	  pthread_mutex_unlock (&bucket->lock);
	}
      else
	{
	  /* Somebody else was indeed faster.  Use the existing entry.  */
	  client->refs++;
	  pthread_mutex_unlock (&bucket->lock);
	  _hurd_cap_client_dealloc (bucket, *r_client);
	  *r_client = client;
	}
      return err;
    }

  client = *r_client;

  /* One reference for the fact that the client task lives, one for
     the caller. */
  client->refs = 2;

  err = hurd_table_enter (&bucket->clients, &client, &client->id);
  if (!err)
    {
      err = hurd_ihash_add (&bucket->clients_reverse, task_id, client);
      if (err)
	hurd_table_remove (&bucket->clients, client->id);
    }
  if (err)
    {
      pthread_mutex_unlock (&bucket->lock);
      hurd_task_death_notify_resume ();
      
      _hurd_cap_client_dealloc (bucket, client);
      return err;
    }
  pthread_mutex_unlock (&bucket->lock);
  hurd_task_death_notify_resume ();

  *r_client = client;

  return 0;
}
