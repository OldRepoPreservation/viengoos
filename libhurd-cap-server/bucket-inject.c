/* bucket-inject.c - Copy out a capability to a client.
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

#include "cap-server-intern.h"


/* Copy out a capability for the capability OBJ to the client with the
   task ID TASK_ID.  Returns the capability (valid only for this user)
   in *R_CAP, or an error.  It is not safe to call this from outside
   an RPC on OBJ while the manager is running.  */
error_t
hurd_cap_bucket_inject (hurd_cap_bucket_t bucket, hurd_cap_obj_t obj,
			hurd_task_id_t task_id, hurd_cap_handle_t *r_cap)
{
  error_t err;
  _hurd_cap_client_t client;
  _hurd_cap_id_t cap_id;

  err = _hurd_cap_client_create (bucket, task_id, &client);
  if (err)
    return err;

  pthread_mutex_lock (&obj->lock);
  err = _hurd_cap_obj_copy_out (obj, bucket, client, &cap_id);
  pthread_mutex_unlock (&obj->lock);
  _hurd_cap_client_release (bucket, client->id);
  if (err)
    return err;

  *r_cap = _hurd_cap_handle_make (client->id, cap_id);
  return 0;
}
