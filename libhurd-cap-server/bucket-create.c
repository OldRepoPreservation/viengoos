/* bucket-create.c - Create a capability bucket.
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
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>

#include "cap-server-intern.h"


/* Process the task death event for TASK_ID.  */
static void
_hurd_cap_client_death (void *hook, hurd_task_id_t task_id)
{
  hurd_cap_bucket_t bucket = (hurd_cap_bucket_t) hook;
  _hurd_cap_client_t client;
  _hurd_cap_client_entry_t entry;

  pthread_mutex_lock (&bucket->lock);
  client = (_hurd_cap_client_t) hurd_ihash_find (&bucket->clients_reverse,
						 task_id);
  if (client)
    {
      /* Found it.  We will consume the "client is still living"
	 reference, which can only be removed by us.  As client death
	 notifications are fully serialized, we don't need to take an
	 extra reference now.  However, we must mark the client entry
	 as dead, so that no further references are acquired by
	 anybody else.  */
      entry = (_hurd_cap_client_entry_t)
	HURD_TABLE_LOOKUP (&bucket->clients, client->id);
      entry->dead = 1;
    }
  pthread_mutex_unlock (&bucket->lock);

  if (client)
    {
      error_t err;

      /* Inhibit all RPCs on this client.  This can only fail if we
	 are canceled.  However, we are the task death manager thread,
	 and nobody should cancel us.  (FIXME: If it turns out that we
	 can be canceled, we should just return on error).  */
      err = _hurd_cap_client_inhibit (bucket, client);
      assert (!err);

      /* End RPCs on this client.  There shouldn't be any (the client
	 is dead), but due to small races, there is a slight chance we
	 still have a worker thread blocked on an incoming message
	 from the now dead client task.  */
      _hurd_cap_client_end (bucket, client);

#ifndef NDEBUG
      pthread_mutex_lock (&bucket->lock);
      /* Reacquire the table entry for this client.  Table entry
	 addresses are not stable under table resize operations.  */
      entry = (_hurd_cap_client_entry_t)
	HURD_TABLE_LOOKUP (&bucket->clients, client->id);

      /* Now, we should have the last reference for this client.  */
      assert (entry->refs = 1);
      pthread_mutex_unlock (&bucket->lock);
#endif

      /* Release our, the last, reference and deallocate all
	 resources, most importantly this will remove us from the
	 client table of the class and release the task info
	 capability.  */
      _hurd_cap_client_release (bucket, client->id);
    }
}


/* Create a new bucket and return it in R_BUCKET.  */
error_t
hurd_cap_bucket_create (hurd_cap_bucket_t *r_bucket)
{
  error_t err;
  hurd_cap_bucket_t bucket;

  bucket = malloc (sizeof (struct _hurd_cap_bucket));
  if (!bucket)
    return errno;


  /* Client management.  */

  err = pthread_cond_init (&bucket->client_cond, NULL);
  if (err)
    goto err_client_cond;

  err = pthread_mutex_init (&bucket->client_cond_lock, NULL);
  if (err)
    goto err_client_cond_lock;

  /* The client death notifications will be requested when we start to
     serve RPCs on the bucket.  */
  
  /* Bucket management.  */

  err = pthread_mutex_init (&bucket->lock, NULL);
  if (err)
    goto err_lock;

  bucket->is_managed = false;
  bucket->state = _HURD_CAP_STATE_GREEN;

  err = pthread_cond_init (&bucket->cond, NULL);
  if (err)
    goto err_cond;

  /* The member cond_waiter will be initialized when the state changes
     to _HURD_CAP_STATE_YELLOW.  */

  bucket->nr_caps = 0;
  bucket->pending_rpcs = NULL;
  bucket->waiting_rpcs = NULL;

  hurd_ihash_init (&bucket->senders,
		   offsetof (struct _hurd_cap_list_item, locp));

  err = hurd_table_init (&bucket->clients,
			 sizeof (struct _hurd_cap_client_entry));
  if (err)
    goto err_clients;

  hurd_ihash_init (&bucket->clients_reverse,
		   offsetof (struct _hurd_cap_client, locp));

  /* Do not use asynchronous thread allocation by default.  */
  bucket->is_worker_alloc_async = false;
  /* We have to leave bucket->worker_alloc uninitialized.  That field
     and bucket->worker_alloc_state will be initialized if
     asynchronous worker thread allocation is used.  */

  /* Finally, add the notify handler.  */
  bucket->client_death_notify.notify_handler = _hurd_cap_client_death;
  bucket->client_death_notify.hook = bucket;
  hurd_task_death_notify_add (&bucket->client_death_notify);

  *r_bucket = bucket;
  return 0;

#if 0
  /* Provided here in case more error cases are added.  */
  hurd_table_destroy (&bucket->clients);
#endif

 err_clients:
  pthread_cond_destroy (&bucket->cond);
 err_cond:
  pthread_mutex_destroy (&bucket->lock);
 err_lock:
  pthread_mutex_destroy (&bucket->client_cond_lock);
 err_client_cond_lock:
  pthread_cond_destroy (&bucket->client_cond);
 err_client_cond:
  free (bucket);

  return err;
}
