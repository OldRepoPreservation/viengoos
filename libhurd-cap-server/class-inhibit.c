/* class-inhibit.c - Inhibit RPCs on a capability class.
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


/* Inhibit all RPCs on the capability class CAP_CLASS (which must not
   be locked).  You _must_ follow up with a hurd_cap_class_resume
   operation, and hold at least one reference to the object
   continuously until you did so.  */
error_t
hurd_cap_class_inhibit (hurd_cap_class_t cap_class)
{
  error_t err;

  pthread_mutex_lock (&cap_class->lock);

  /* First wait until any other inhibitor has resumed the class.  If
     this function is called within an RPC, we are going to be
     canceled anyway.  Otherwise, it ensures that class inhibitions
     are fully serialized (per class).  */
  while (cap_class->state != _HURD_CAP_STATE_GREEN)
    {
      err = hurd_cond_wait (&cap_class->cond, &cap_class->lock);
      if (err)
	{
	  /* We have been canceled.  */
	  pthread_mutex_unlock (&cap_class->lock);
	  return err;
	}
    }

  /* Now it is our turn to inhibit the class.  */
  cap_class->cond_waiter = pthread_self ();

  if (_hurd_cap_class_cond_busy (cap_class))
    {
      _hurd_cap_list_item_t pending_rpc = cap_class->pending_rpcs;

      /* There are still pending RPCs (beside us).  Cancel them.  */
      while (pending_rpc)
	{
	  if (pending_rpc->thread != cap_class->cond_waiter)
	    pthread_cancel (pending_rpc->thread);
	  pending_rpc = pending_rpc->next;
	}

      /* Indicate that we would like to know when they have gone.  */
      cap_class->state = _HURD_CAP_STATE_YELLOW;

      /* The last one will shut the door.  */
      do
	{
	  err = hurd_cond_wait (&cap_class->cond, &cap_class->lock);
	  if (err)
	    {
	      /* We have been canceled ourselves.  Give up.  */
	      cap_class->state = _HURD_CAP_STATE_GREEN;
	      pthread_mutex_unlock (&cap_class->lock);
	      return err;
	    }
	}
      while (cap_class->state != _HURD_CAP_STATE_RED);
    }
  else
    cap_class->state = _HURD_CAP_STATE_RED;

  /* Now all pending RPCs have been canceled and are completed (except
     us), and all incoming RPCs are inhibited.  */
  pthread_mutex_unlock (&cap_class->lock);

  return 0;
}


/* Resume RPCs on the class CAP_CLASS and wake-up all waiters.  */
void
hurd_cap_class_resume (hurd_cap_class_t cap_class)
{
  pthread_mutex_lock (&cap_class->lock);

  cap_class->state = _HURD_CAP_STATE_GREEN;

  /* Broadcast the change to all potential waiters.  */
  pthread_cond_broadcast (&cap_class->cond);

  pthread_mutex_unlock (&cap_class->lock);
}
