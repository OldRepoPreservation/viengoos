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
#include <pthread.h>
#include <stdlib.h>

#include <hurd/cap-server.h>


/* Destroy the connection client CLIENT.  If the client connection is
   still used for anything, this function returns EBUSY.  */
static error_t
_hurd_cap_client_try_destroy (hurd_cap_class_t cap_class,
			      _hurd_cap_client_entry_t client_entry)
{
  error_t err;
  hurd_cap_client_t client = client_entry->client;

  /* FIXME: First, this accesses the table structure directly, and not
     via an official interface.  Second, if a user connection is busy
     or not will need to be revised when more features are added.  */
  if (client->caps.used || client_entry->refs > 1)
    return EBUSY;

  _hurd_cap_client_release (cap_class, client->id);
  return 0;
}


/* Destroy the capability class CAP_CLASS and release all associated
   resources.  Note that this is only allowed if there are no
   capability objects in use, and if the capability class is not used
   by anything else (ie, all RPCs must be inhibited, no manager thread
   must run.  This does assume the class has been created with
   hurd_cap_class_init.  */
error_t
hurd_cap_class_destroy (hurd_cap_class_t cap_class)
{
  error_t err = 0;

  /* FIXME: This function needs to be revised.  We need to take the
     locks, and if only for memory synchronization.  */

  /* This will fail if there are still pending user connections.  */
  HURD_TABLE_ITERATE (&cap_class->clients, idx)
    {
      err = _hurd_cap_client_try_destroy
	(cap_class,
	 (_hurd_cap_client_entry_t) HURD_TABLE_LOOKUP (&cap_class->clients,
						       idx));
      if (err)
	break;
    }
  if (err)
    return err;

  /* This will fail if there are still allocated capability
     objects.  */
  err = hurd_slab_destroy (cap_class->obj_slab);
  if (err)
    return err;

  /* At this point, destruction will succeed.  */
  hurd_task_death_notify_remove (&cap_class->client_death_notify);
  pthread_cond_destroy (&cap_class->cond);
  hurd_table_destroy (&cap_class->clients);
  pthread_mutex_destroy (&cap_class->lock);
  pthread_mutex_destroy (&cap_class->client_cond_lock);
  pthread_cond_destroy (&cap_class->client_cond);
  /* This can not fail at this point.  */
  hurd_slab_destroy (cap_class->client_slab);
  pthread_mutex_destroy (&cap_class->obj_cond_lock);
  pthread_cond_destroy (&cap_class->obj_cond);

  return 0;
}


/* Destroy the capability class CAP_CLASS and release all associated
   resources.  Note that this is only allowed if there are no
   capability objects in use, and if the capability class is not used
   by anything else (ie, all RPCs must be inhibited, no manager thread
   must run.  This does assume that the class was created with
   hurd_cap_class_create.  */
error_t
hurd_cap_class_free (hurd_cap_class_t cap_class)
{
  error_t err;

  err = hurd_cap_class_destroy (cap_class);
  if (err)
    return err;

  free (cap_class);
}
