/* client-release.c - Release a capability client.
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


/* Deallocate the connection client CLIENT.  */
void
__attribute__((visibility("hidden")))
_hurd_cap_client_dealloc (hurd_cap_class_t cap_class, hurd_cap_client_t client)
{
  unsigned int done;
  unsigned int current_idx;

  /* This function is only invoked if the reference count for the
     client entry in the client table of the class drops to 0, and
     after the table entry was removed.  Usually, the last reference
     is removed by the task death notification handler.

     At that point, there are no more pending RPCs for this client (if
     there were, there would be a reference for each of them).  This
     also means that all capability IDs have at most one internal
     reference, the one for all external references.  */

  /* Note that although the client has been removed from the clients
     table in the class, there are still back-references for each and
     every capability object in our capability table caps.  These
     capability entries all count as references to ourself.  They are
     used for example if a capability is revoked.  It is important to
     realize that such a revocation can happen anytime as long as
     there are still valid capability objects in the caps table of the
     client.

     So, to correctly release those references, we have to look up
     each capability object properly, acquiring our own internal
     reference for it, then we have to unlock the client to lock the
     capability object, to finally revoke our own capability and
     release the capability object reference.  Only then can we
     reacquire our own lock and go on to the next capability.  While
     we do not hold our lock, more capabilities can be revoked by
     other concurrent operations.  However, no new capabilities are
     added, so one pass through the table is enough.  */

  pthread_mutex_lock (&client->lock);

  /* Release all capability objects held by this user.  Because we
     have to honor the locking order, this takes a while.  */
  HURD_TABLE_ITERATE (&client->caps, idx)
    {
      _hurd_cap_entry_t entry;

      entry = (_hurd_cap_entry_t) HURD_TABLE_LOOKUP (&client->caps, idx);

      /* If there were no external references, the last internal
	 reference would have been released before we get here.  */
      assert (entry->external_refs);

      /* The number of internal references is either one or zero.  If
	 it is one, then the capability is not revoked yet, so we have
	 to do it.  If it is zero, then the capability is revoked
	 (dead), and we only have to clear the table entry.  */
      if (!entry->dead)
	{
	  hurd_cap_obj_t cap_obj = entry->cap_obj;

	  assert (entry->internal_refs == 1);

	  /* Acquire an internal reference to prevent that our own
	     reference to the capability object is removed by a
	     concurrent revocation as soon as we unlock the client.
	     After all, the existing internal reference belongs to the
	     capability object, and not to us.  */
	  entry->internal_refs++;
	  pthread_mutex_unlock (&client->lock);

	  pthread_mutex_lock (&cap_obj->lock);
	  /* Check if we should revoke it, or if somebody else did already.  */
	  if (hurd_ihash_remove (&cap_obj->clients, (hurd_ihash_key_t) client))
	    {
	      int found;

	      /* We should revoke it.  */
	      pthread_mutex_lock (&client->lock);
	      found = hurd_ihash_remove (&client->caps_reverse,
					 (hurd_ihash_key_t) cap_obj);
	      assert (found);

	      /* Reaquire the table entry.  */
	      entry = (_hurd_cap_entry_t) HURD_TABLE_LOOKUP (&client->caps,
							     idx);
	      assert (!entry->dead);
	      entry->dead = 1;

	      assert (entry->internal_refs == 2);
	      entry->internal_refs--;
	      pthread_mutex_unlock (&client->lock);
	    }
	  pthread_mutex_unlock (&cap_obj->lock);

	  pthread_mutex_lock (&client->lock);
	  /* Reaquire the table entry.  It still exists because there
	     were external references.  */
	  entry = (_hurd_cap_entry_t) HURD_TABLE_LOOKUP (&client->caps, idx);
	  /* Now we can drop the capability object below.  */
	  assert (entry->dead);
	  assert (entry->internal_refs == 1);
	  assert (entry->external_refs);
	}
      else
	{
	  /* If the capability is dead, we can simply drop the
	     external references below.  */
	  assert (entry->internal_refs == 0);
	}

      /* Drop the external references, and remove the table entry.  */
      hurd_table_remove (&client->caps, idx);
    }

  /* After all this ugly work, the rest is trivial.  */
  if (client->state != _HURD_CAP_STATE_GREEN)
    client->state = _HURD_CAP_STATE_GREEN;

  assert (client->pending_rpcs == NULL);

  /* FIXME: Release the task info capability here.  */

  /* FIXME: It would be a good idea to shrink the empty table and
     empty hash here, to reclaim resources and be able to eventually
     enforce a per-client quota.  */
  pthread_mutex_unlock (&client->lock);

  hurd_slab_dealloc (cap_class->client_slab, client);
}


/* Release a reference for the client with the ID IDX in class
   CLASS.  */
void
__attribute__((visibility("hidden")))
_hurd_cap_client_release (hurd_cap_class_t cap_class, hurd_cap_client_id_t idx)
{
  _hurd_cap_client_entry_t entry;

  pthread_mutex_lock (&cap_class->lock);
  entry = (_hurd_cap_client_entry_t) HURD_TABLE_LOOKUP (&cap_class->clients,
							idx);

  if (__builtin_expect (entry->refs > 1, 1))
    {
      entry->refs--;
      pthread_mutex_unlock (&cap_class->lock);
    }
  else
    {
      int found;
      hurd_cap_client_t client = entry->client;

      hurd_table_remove (&cap_class->clients, idx);
      hurd_ihash_locp_remove (&cap_class->clients_reverse, client->locp);

      pthread_mutex_unlock (&cap_class->lock);
      _hurd_cap_client_dealloc (cap_class, client);
    }
}
