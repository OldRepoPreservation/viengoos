/* obj-inhibit.c - Inhibit RPCs on a capability object.
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
   object, and fails if not.  This is only valid if
   hurd_cap_obj_inhibit is in progress (ie, if cap_obj->state is
   _HURD_CAP_STATE_YELLOW).  FIXME: We will need this in the RPC
   worker thread code, where the last worker will get false as return
   value and then has to change the state to RED and broadcast the
   condition.  */
static inline int
_hurd_cap_obj_cond_busy (hurd_cap_obj_t obj)
{
  /* We have to remain in the state yellow until there are no pending
     RPC threads except maybe the waiter.  */
  return obj->pending_rpcs
    && (obj->pending_rpcs->thread != obj->cond_waiter
	|| obj->pending_rpcs->next);
}


/* Inhibit all RPCs on the capability object CAP_OBJ (which must not
   be locked).  You _must_ follow up with a hurd_cap_obj_resume
   operation, and hold at least one reference to the object
   continuously until you did so.  */
error_t
hurd_cap_obj_inhibit (hurd_cap_obj_t obj)
{
  hurd_cap_class_t cap_class = obj->cap_class;
  error_t err;

  /* First take the class-wide lock for conditions on capability
     object states.  */
  pthread_mutex_lock (&cap_class->obj_cond_lock);

  /* Then lock the object to check its state.  */
  pthread_mutex_lock (&obj->lock);

  /* First wait until any other inhibitor has resumed the capability
     object.  This ensures that capability object inhibitions are
     fully serialized (per capability object).  */
  while (obj->state != _HURD_CAP_STATE_GREEN)
    {
      pthread_mutex_unlock (&obj->lock);
      err = hurd_cond_wait (&cap_class->obj_cond,
			    &cap_class->obj_cond_lock);
      if (err)
	{
	  /* We have been canceled.  */
	  pthread_mutex_unlock (&cap_class->obj_cond_lock);
	  return err;
	}
      pthread_mutex_lock (&obj->lock);
    }

  /* Now it is our turn to inhibit the capability object.  */
  obj->cond_waiter = pthread_self ();

  if (_hurd_cap_obj_cond_busy (obj))
    {
      _hurd_cap_list_item_t pending_rpc = obj->pending_rpcs;

      /* There are still pending RPCs (beside us).  Cancel them.  */
      while (pending_rpc)
	{
	  if (pending_rpc->thread != obj->cond_waiter)
	    pthread_cancel (pending_rpc->thread);
	  pending_rpc = pending_rpc->next;
	}

      /* Indicate that we would like to know when they have gone.  */
      obj->state = _HURD_CAP_STATE_YELLOW;

      /* The last one will shut the door.  */
      do
	{
	  pthread_mutex_unlock (&obj->lock);
	  err = hurd_cond_wait (&cap_class->obj_cond,
				&cap_class->obj_cond_lock);
	  if (err)
	    {
	      /* We have been canceled ourselves.  Give up.  */
	      obj->state = _HURD_CAP_STATE_GREEN;
	      pthread_mutex_unlock (&cap_class->obj_cond_lock);
	      return err;
	    }
	  pthread_mutex_lock (&obj->lock);
	}
      while (obj->state != _HURD_CAP_STATE_RED);
    }
  else
    obj->state = _HURD_CAP_STATE_RED;

  /* Now all pending RPCs have been canceled and are completed (except
     us), and all incoming RPCs are inhibited.  */
  pthread_mutex_unlock (&obj->lock);
  pthread_mutex_unlock (&cap_class->obj_cond_lock);

  return 0;
}


/* Resume RPCs on the capability object OBJ and wake-up all
   waiters.  */
void
hurd_cap_obj_resume (hurd_cap_obj_t obj)
{
  hurd_cap_class_t cap_class = obj->cap_class;

  pthread_mutex_lock (&cap_class->obj_cond_lock);
  pthread_mutex_lock (&cap_class->lock);

  obj->state = _HURD_CAP_STATE_GREEN;

  /* Broadcast the change to all potential waiters.  */
  pthread_cond_broadcast (&cap_class->obj_cond);

  pthread_mutex_unlock (&cap_class->lock);
  pthread_mutex_unlock (&cap_class->obj_cond_lock);
}
