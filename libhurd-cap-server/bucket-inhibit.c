/* bucket-inhibit.c - Inhibit RPCs on a capability bucket.
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


/* Return true if there are still outstanding RPCs in this bucket, and
   fails if not.  This is only valid if hurd_cap_bucket_inhibit is in
   progress (ie, if bucket->state is _HURD_CAP_STATE_YELLOW).
   FIXME: We will need this in the RPC worker thread code, where the
   last worker will get false as return value and then has to change
   the state to RED and signal (broadcast?) the condition.  */
static inline int
_hurd_cap_bucket_cond_busy (hurd_cap_bucket_t bucket)
{
  /* We have to remain in the state yellow until there are no pending
     RPC threads except maybe the waiter.  */
  return bucket->pending_rpcs
    && (bucket->pending_rpcs->thread != bucket->cond_waiter
	|| bucket->pending_rpcs->next);
}


/* Inhibit all RPCs on the capability bucket BUCKET (which must not
   be locked).  You _must_ follow up with a hurd_cap_bucket_resume
   operation, and hold at least one reference to the object
   continuously until you did so.  */
error_t
hurd_cap_bucket_inhibit (hurd_cap_bucket_t bucket)
{
  error_t err;

  pthread_mutex_lock (&bucket->lock);

  /* First wait until any other inhibitor has resumed the bucket.  If
     this function is called within an RPC, we are going to be
     canceled anyway.  Otherwise, it ensures that bucket inhibitions
     are fully serialized (per bucket).  */
  while (bucket->state != _HURD_CAP_STATE_GREEN)
    {
      err = hurd_cond_wait (&bucket->cond, &bucket->lock);
      if (err)
	{
	  /* We have been canceled.  */
	  pthread_mutex_unlock (&bucket->lock);
	  return err;
	}
    }

  /* Now it is our turn to inhibit the bucket.  */
  bucket->cond_waiter = pthread_self ();

  if (_hurd_cap_bucket_cond_busy (bucket))
    {
      _hurd_cap_list_item_t pending_rpc = bucket->pending_rpcs;

      /* There are still pending RPCs (beside us).  Cancel them.  */
      while (pending_rpc)
	{
	  if (pending_rpc->thread != bucket->cond_waiter)
	    pthread_cancel (pending_rpc->thread);
	  pending_rpc = pending_rpc->next;
	}

      /* Indicate that we would like to know when they have gone.  */
      bucket->state = _HURD_CAP_STATE_YELLOW;

      /* The last one will shut the door.  */
      do
	{
	  err = hurd_cond_wait (&bucket->cond, &bucket->lock);
	  if (err)
	    {
	      /* We have been canceled ourselves.  Give up.  */
	      bucket->state = _HURD_CAP_STATE_GREEN;
	      pthread_mutex_unlock (&bucket->lock);
	      return err;
	    }
	}
      while (bucket->state != _HURD_CAP_STATE_RED);
    }
  else
    bucket->state = _HURD_CAP_STATE_RED;

  /* Now all pending RPCs have been canceled and are completed (except
     us), and all incoming RPCs are inhibited.  */
  pthread_mutex_unlock (&bucket->lock);

  return 0;
}


/* Resume RPCs on the bucket BUCKET and wake-up all waiters.  */
void
hurd_cap_bucket_resume (hurd_cap_bucket_t bucket)
{
  pthread_mutex_lock (&bucket->lock);

  bucket->state = _HURD_CAP_STATE_GREEN;

  /* Broadcast the change to all potential waiters.  */
  pthread_cond_broadcast (&bucket->cond);

  pthread_mutex_unlock (&bucket->lock);
}


/* End management of the bucket BUCKET.  */
error_t
hurd_cap_bucket_end (hurd_cap_bucket_t bucket, bool force)
{
  pthread_mutex_lock (&bucket->lock);

  /* FIXME: Check if we can go away while honoring the FORCE flag.  */

  bucket->state = _HURD_CAP_STATE_BLACK;

  /* Broadcast the change to all potential waiters.  */
  pthread_cond_broadcast (&bucket->cond);

  /* FIXME: This is not enough.  We need to cancel or otherwise notify
     the manager thread that it is supposed to go away, if it is
     currently blocking in an open receive.  For example, we could
     send it a special message.  However, the manager thread must
     ensure that our own thread is correctly finished (ie, if we are
     called from within an RPC, the RPC must finish before the manager
     thread is allowed to go away.  */

  pthread_mutex_unlock (&bucket->lock);
}
