/* class-create.c - Create a capability class.
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


/* Initialize the slab object pointed to by BUFFER.  HOOK is as
   provided to hurd_slab_create.  */
static error_t
_hurd_cap_obj_constructor (void *hook, void *buffer)
{
  hurd_cap_class_t cap_class = (hurd_cap_class_t) hook;
  hurd_cap_obj_t obj = (hurd_cap_obj_t) buffer;
  error_t err;

  /* First do our own initialization.  */
  err = pthread_mutex_init (&obj->lock, 0);
  if (err)
    return err;

  hurd_ihash_init (&obj->clients, 0);
  obj->refs = 1;
  obj->state = _HURD_CAP_STATE_GREEN;
  obj->pending_rpcs = NULL;

  /* Then do the user part, if necessary.  */
  if (cap_class->obj_init)
    {
      err = (*cap_class->obj_init) (cap_class, obj);
      if (err)
	{
	  hurd_ihash_destroy (&obj->clients);
	  pthread_mutex_destroy (&obj->lock);
	  return err;
	}
    }
  return 0;
}


/* Destroy the slab object pointed to by BUFFER.  HOOK is as provided
   to hurd_slab_create.  This might be called with the */
static void
_hurd_cap_obj_destructor (void *hook, void *buffer)
{
  hurd_cap_class_t cap_class = (hurd_cap_class_t) hook;
  hurd_cap_obj_t obj = (hurd_cap_obj_t) buffer;
  error_t err;

  if (cap_class->obj_destroy)
    (*cap_class->obj_destroy) (cap_class, obj);

  hurd_ihash_destroy (&obj->clients);
  pthread_mutex_destroy (&obj->lock);
}


/* Client management code.  */

/* Allocate a new capability client structure for the slab cache.  */
static error_t
_hurd_cap_client_constructor (void *hook, void *buffer)
{
  hurd_cap_class_t cap_class = (hurd_cap_class_t) hook;
  hurd_cap_client_t client = (hurd_cap_client_t) buffer;
  error_t err;

  err = pthread_mutex_init (&client->lock, NULL);
  if (err)
    return err;

  client->state = _HURD_CAP_STATE_GREEN;
  client->pending_rpcs = NULL;

  err = hurd_table_init (&client->caps, sizeof (struct _hurd_cap_entry));
  if (err)
    goto err_cap_client_caps;

  /* Capabilities are mapped to clients many to many, so we can not
     use a location pointer.  However, this is not critical as
     removing an entry only blocks out RPCs for the same client, and
     not others.  */
  hurd_ihash_init (&client->caps_reverse, HURD_IHASH_NO_LOCP);

  return 0;

  /* This is provided here in case you add more initialization to the
     end of the above code.  */
#if 0
  hurd_table_destroy (&client->caps);
#endif

 err_cap_client_caps:
  pthread_mutex_destroy (&client->lock);

  return err;
}


/* Allocate a new capability client structure for the slab cache.  */
static void
_hurd_cap_client_destructor (void *hook, void *buffer)
{
  hurd_cap_class_t cap_class = (hurd_cap_class_t) hook;
  hurd_cap_client_t client = (hurd_cap_client_t) buffer;

  hurd_ihash_destroy (&client->caps_reverse);
  hurd_table_destroy (&client->caps);
  pthread_mutex_destroy (&client->lock);
}


/* Process the task death event for TASK_ID.  */
static void
_hurd_cap_client_death (void *hook, hurd_task_id_t task_id)
{
  hurd_cap_class_t cap_class = (hurd_cap_class_t) hook;
  hurd_cap_client_t client;
  _hurd_cap_client_entry_t entry;

  pthread_mutex_lock (&cap_class->lock);
  client = (hurd_cap_client_t) hurd_ihash_find (&cap_class->clients_reverse,
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
	HURD_TABLE_LOOKUP (&cap_class->clients, client->id);
      entry->dead = 1;
    }
  pthread_mutex_unlock (&cap_class->lock);

  if (client)
    {
      error_t err;

      /* Inhibit all RPCs on this client.  This can only fail if we
	 are canceled.  However, we are the task death manager thread,
	 and nobody should cancel us.  (FIXME: If it turns out that we
	 can be canceled, we should just return on error).  */
      err = hurd_cap_client_inhibit (cap_class, client);
      assert (!err);

      /* End RPCs on this client.  There shouldn't be any (the client
	 is dead), but due to small races, there is a slight chance we
	 still have a worker thread blocked on an incoming message
	 from the now dead client task.  */
      _hurd_cap_client_end (cap_class, client);

#ifndef NDEBUG
      pthread_mutex_lock (&cap_class->lock);
      /* Reacquire the table entry for this client.  Table entry
	 addresses are not stable under table resize operations.  */
      entry = (_hurd_cap_client_entry_t)
	HURD_TABLE_LOOKUP (&cap_class->clients, client->id);

      /* Now, we should have the last reference for this client.  */
      assert (entry->refs = 1);
      pthread_mutex_unlock (&cap_class->lock);
#endif

      /* Release our, the last, reference and deallocate all
	 resources, most importantly this will remove us from the
	 client table of the class and release the task info
	 capability.  */
      _hurd_cap_client_release (cap_class, client->id);
    }
}


/* Same as hurd_cap_class_create, but doesn't allocate the storage for
   CAP_CLASS.  Instead, you have to provide it.  */
error_t
hurd_cap_class_init (hurd_cap_class_t cap_class,
		     size_t size, hurd_cap_obj_init_t obj_init,
		     hurd_cap_obj_alloc_t obj_alloc,
		     hurd_cap_obj_reinit_t obj_reinit,
		     hurd_cap_obj_destroy_t obj_destroy)
{
  error_t err;

  /* Capability object management.  */

  assert (size >= sizeof (struct hurd_cap_obj));
  cap_class->obj_size = size;

  cap_class->obj_init = obj_init;
  cap_class->obj_alloc = obj_alloc;
  cap_class->obj_reinit = obj_reinit;
  cap_class->obj_destroy = obj_destroy;

  err = hurd_slab_create (size, 0, _hurd_cap_obj_constructor,
			  _hurd_cap_obj_destructor, cap_class,
			  &cap_class->obj_slab);
  if (err)
    goto err_obj_slab;

  err = pthread_cond_init (&cap_class->obj_cond, NULL);
  if (err)
    goto err_obj_cond;

  err = pthread_mutex_init (&cap_class->obj_cond_lock, NULL);
  if (err)
    goto err_obj_cond_lock;


  /* Client management.  */

  err = hurd_slab_create (sizeof (struct hurd_cap_client), 0,
			  _hurd_cap_client_constructor,
			  _hurd_cap_client_destructor, cap_class,
			  &cap_class->client_slab);
  if (err)
    goto err_client_slab;

  err = pthread_cond_init (&cap_class->client_cond, NULL);
  if (err)
    goto err_client_cond;

  err = pthread_mutex_init (&cap_class->client_cond_lock, NULL);
  if (err)
    goto err_client_cond_lock;

  /* The notify handler is added last (below), so we do not process
     notifications before all parts of the class have been fully
     initialized.  */


  /* Class management.  */

  err = pthread_mutex_init (&cap_class->lock, NULL);
  if (err)
    goto err_lock;

  err = hurd_table_init (&cap_class->clients,
			 sizeof (struct _hurd_cap_client_entry));
  if (err)
    goto err_clients;

  hurd_ihash_init (&cap_class->clients_reverse,
		   offsetof (struct hurd_cap_client, locp));

  cap_class->state = _HURD_CAP_STATE_GREEN;

  err = pthread_cond_init (&cap_class->client_cond, NULL);
  if (err)
    goto err_cond;

  /* The cond_waiter member doesn't need to be initialized.  It is set
     whenever the state changes to _HURD_CAP_STATE_YELLOW by the
     inhibitor.  */

  cap_class->pending_rpcs = NULL;

  hurd_ihash_init (&cap_class->client_threads,
		   offsetof (struct _hurd_cap_list_item, locp));
  
  /* Finally, add the notify handler.  */
  cap_class->client_death_notify.notify_handler = _hurd_cap_client_death;
  cap_class->client_death_notify.hook = cap_class;
  hurd_task_death_notify_add (&cap_class->client_death_notify);

  return 0;

  /* This is provided here in case you add more initialization to the
     end of the above code.  */
#if 0
  pthread_cond_destroy (&cap_class->cond);
#endif

 err_cond:
  hurd_table_destroy (&cap_class->clients);

 err_clients:
  pthread_mutex_destroy (&cap_class->lock);

 err_lock:
  pthread_mutex_destroy (&cap_class->client_cond_lock);

 err_client_cond_lock:
  pthread_cond_destroy (&cap_class->client_cond);

 err_client_cond:
  /* This can not fail at this point.  */
  hurd_slab_destroy (cap_class->client_slab);

 err_client_slab:
  pthread_mutex_destroy (&cap_class->obj_cond_lock);

 err_obj_cond_lock:
  pthread_cond_destroy (&cap_class->obj_cond);

 err_obj_cond:
  /* This can not fail at this point.  */
  hurd_slab_destroy (cap_class->obj_slab);

 err_obj_slab:
  return err;
}


/* Create a new capability class for objects with the size SIZE,
   including the struct hurd_cap_obj, which has to be placed at the
   beginning of each capability object.

   The callback OBJ_INIT is used whenever a capability object in this
   class is created.  The callback OBJ_REINIT is used whenever a
   capability object in this class is deallocated and returned to the
   slab.  OBJ_REINIT should return a capability object that is not
   used anymore into the same state as OBJ_INIT does for a freshly
   allocated object.  OBJ_DESTROY should deallocate all resources for
   this capablity object.  Note that if OBJ_INIT or OBJ_REINIT fails,
   the object is considered to be fully destroyed.  No extra call to
   OBJ_DESTROY will be made for such objects.

   The new capability class is returned in R_CLASS.  If the creation
   fails, an error value will be returned.  */
error_t
hurd_cap_class_create (size_t size, hurd_cap_obj_init_t obj_init,
		       hurd_cap_obj_alloc_t obj_alloc,
		       hurd_cap_obj_reinit_t obj_reinit,
		       hurd_cap_obj_destroy_t obj_destroy,
		       hurd_cap_class_t *r_class)
{
  error_t err;
  hurd_cap_class_t cap_class = malloc (sizeof (struct hurd_cap_class));

  if (!cap_class)
    return errno;

  err = hurd_cap_class_init (cap_class, size, obj_init, obj_alloc, obj_reinit,
			     obj_destroy);
  if (err)
    {
      free (cap_class);
      return err;
    }

  return 0;
}
