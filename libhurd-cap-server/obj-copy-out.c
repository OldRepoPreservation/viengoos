/* obj-copy-out.c - Copy out a capability to a client.
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


/* Copy out a capability for the capability OBJ to the user CLIENT.
   Returns the capability ID (valid only for this user) in *R_ID, or
   an error.  OBJ must be locked.  Note: No internal reference for
   this capability is allocated for the caller.  */
error_t
_hurd_cap_obj_copy_out (hurd_cap_obj_t obj, hurd_cap_bucket_t bucket,
			_hurd_cap_client_t client, hurd_cap_id_t *r_id)
{
  _hurd_cap_obj_entry_t entry;

  pthread_mutex_lock (&client->lock);
  entry = (_hurd_cap_obj_entry_t) hurd_ihash_find (&client->caps_reverse,
						   (hurd_ihash_key_t) obj);

  if (entry)
    {
      entry->external_refs++;
      *r_id = entry->id;
      pthread_mutex_unlock (&client->lock);
      return 0;
    }
  else
    {
      _hurd_cap_obj_entry_t entry_check;
      error_t err;

      pthread_mutex_unlock (&client->lock);
      err = hurd_slab_alloc (&_hurd_cap_obj_entry_space, (void **) &entry);
      if (err)
	return err;

      entry->cap_obj = obj;
      /* ID is filled in when adding the object to the table.  */
      /* CLIENT_ITEM is filled after the object has been entered.  */
      /* DEAD is 0 for initialized objects.  */
      /* INTERNAL_REFS is 1 for initialized objects.  */
      /* EXTERNAL_REFS is 1 for initialized objects.  */

      pthread_mutex_lock (&client->lock);
      entry_check = hurd_ihash_find (&client->caps_reverse,
				     (hurd_ihash_key_t) obj);
      if (entry_check)
	{
	  /* Somebody else was faster.  */
	  entry_check->external_refs++;
	  *r_id = entry_check->id;
	  pthread_mutex_unlock (&client->lock);
	  hurd_slab_dealloc (&_hurd_cap_obj_entry_space, entry);
	  return 0;
	}

      /* Add the entry to the caps table of the client.  */
      err = hurd_table_enter (&client->caps, &entry, &entry->id);
      if (err)
	{
	  pthread_mutex_unlock (&client->lock);
	  hurd_slab_dealloc (&_hurd_cap_obj_entry_space, entry);
	  return err;
	}
      err = hurd_ihash_add (&client->caps_reverse,
			    (hurd_ihash_key_t) obj, entry);
      if (err)
	{
	  hurd_table_remove (&client->caps, entry->id);
	  pthread_mutex_unlock (&client->lock);
	  hurd_slab_dealloc (&_hurd_cap_obj_entry_space, entry);
	  return err;
	}

      pthread_mutex_unlock (&client->lock);

      /* Add the object to the list.  */
      _hurd_cap_list_item_add (&obj->clients, &entry->client_item);

      /* Add a reference for the internal reference of the capability
	 entry to the capability object.  */
      obj->refs++;

      /* FIXME: Should probably use spin lock here, or so.  */
      pthread_mutex_lock (&bucket->lock);
      bucket->nr_caps++;
      pthread_mutex_unlock (&bucket->lock);

      return 0;
    }
}
