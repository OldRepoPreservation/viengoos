/* client-inhibit.c - Inhibit RPCs on a capability client.
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


/* Return true if there are still outstanding RPCs in this capability
   client, and fails if not.  This is only valid if
   hurd_cap_client_inhibit is in progress (ie, if client->state is
   _HURD_CAP_STATE_YELLOW).  FIXME: We will need this in the RPC
   worker thread code, where the last worker will get false as return
   value and then has to change the state to RED and broadcast the
   condition.  */
static inline int
_hurd_cap_client_cond_busy (_hurd_cap_client_t client)
{
  /* We have to remain in the state yellow until there are no pending
     RPC threads except maybe the waiter.  */
  return client->pending_rpcs
    && (client->pending_rpcs->thread != client->cond_waiter
	|| client->pending_rpcs->next);
}


/* Inhibit all RPCs on the capability client CLIENT (which must not be
   locked) in the capability bucket BUCKET.  You _must_ follow up
   with a hurd_cap_client_resume operation, and hold at least one
   reference to the object continuously until you did so.  */
error_t
_hurd_cap_client_inhibit (hurd_cap_bucket_t bucket, _hurd_cap_client_t client)
{
  error_t err;

  /* First take the bucket-wide lock for conditions on capability
     client states.  */
  pthread_mutex_lock (&bucket->client_cond_lock);

  /* Then lock the client to check its state.  */
  pthread_mutex_lock (&client->lock);

  /* First wait until any other inhibitor has resumed the capability
     client.  This ensures that capability client inhibitions are
     fully serialized (per capability client).  */
  while (client->state != _HURD_CAP_STATE_GREEN)
    {
      pthread_mutex_unlock (&client->lock);
      err = hurd_cond_wait (&bucket->client_cond,
			    &bucket->client_cond_lock);
      if (err)
	{
	  /* We have been canceled.  */
	  pthread_mutex_unlock (&bucket->client_cond_lock);
	  return err;
	}
      pthread_mutex_lock (&client->lock);
    }

  /* Now it is our turn to inhibit the capability client.  */
  client->cond_waiter = pthread_self ();

  if (_hurd_cap_client_cond_busy (client))
    {
      _hurd_cap_list_item_t pending_rpc = client->pending_rpcs;

      /* There are still pending RPCs (beside us).  Cancel them.  */
      while (pending_rpc)
	{
	  if (pending_rpc->thread != client->cond_waiter)
	    pthread_cancel (pending_rpc->thread);
	  pending_rpc = pending_rpc->next;
	}

      /* Indicate that we would like to know when they have gone.  */
      client->state = _HURD_CAP_STATE_YELLOW;

      /* The last one will shut the door.  */
      do
	{
	  pthread_mutex_unlock (&client->lock);
	  err = hurd_cond_wait (&bucket->client_cond,
				&bucket->client_cond_lock);
	  if (err)
	    {
	      /* We have been canceled ourselves.  Give up.  */
	      client->state = _HURD_CAP_STATE_GREEN;
	      pthread_mutex_unlock (&bucket->client_cond_lock);
	      return err;
	    }
	  pthread_mutex_lock (&client->lock);
	}
      while (client->state != _HURD_CAP_STATE_RED);
    }
  else
    client->state = _HURD_CAP_STATE_RED;

  /* Now all pending RPCs have been canceled and are completed (except
     us), and all incoming RPCs are inhibited.  */
  pthread_mutex_unlock (&client->lock);
  pthread_mutex_unlock (&bucket->client_cond_lock);

  return 0;
}


/* Resume RPCs on the capability client CLIENT in the bucket BUCKET
   and wake-up all waiters.  */
void
_hurd_cap_client_resume (hurd_cap_bucket_t bucket, _hurd_cap_client_t client)
{
  pthread_mutex_lock (&bucket->client_cond_lock);
  pthread_mutex_lock (&bucket->lock);

  client->state = _HURD_CAP_STATE_GREEN;

  /* Broadcast the change to all potential waiters.  */
  pthread_cond_broadcast (&bucket->client_cond);

  pthread_mutex_unlock (&bucket->lock);
  pthread_mutex_unlock (&bucket->client_cond_lock);
}


/* End RPCs on the capability client CLIENT in the bucket BUCKET and
   wake-up all waiters.  */
void
_hurd_cap_client_end (hurd_cap_bucket_t bucket, _hurd_cap_client_t client)
{
  pthread_mutex_lock (&bucket->client_cond_lock);
  pthread_mutex_lock (&bucket->lock);

  client->state = _HURD_CAP_STATE_BLACK;

  /* Broadcast the change to all potential waiters.  Even though the
     task is dead now, there is a race condition where we will process
     one spurious incoming RPC which is blocked on the inhibited
     state.  So we wake up such threads, they will then go away
     quickly.

     Note that this does not work reliably for still living clients:
     They may bombard us with RPCs and thus keep the reference count
     of the client in the bucket table above 0 all the time, even in
     the _HURD_CAP_STATE_BLACK state.  This is the reason that this
     interface is only for internal use (by
     _hurd_cap_client_death).  */
  pthread_cond_broadcast (&bucket->client_cond);

  pthread_mutex_unlock (&bucket->lock);
  pthread_mutex_unlock (&bucket->client_cond_lock);
}
