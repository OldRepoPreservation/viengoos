/* Copyright (C) 2003 Free Software Foundation, Inc.
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

#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <stdint.h>

#include <pthread.h>

#include <hurd/cap.h>

#include "cap-intern.h"


/* The slab space for capability objects.  */
hurd_slab_space_t cap_space;


/* Initialize a new capability, allocated by the slab allocator.  */
static error_t
cap_constructor (void *buffer)
{
  hurd_cap_t cap = (hurd_cap_t) buffer;
  error_t err;

  err = pthread_mutex_init (&cap->lock, NULL);
  if (err)
    return err;

  cap->srefs = 0;
  cap->orefs = 0;

  /* The other data is filled in by the creator.  */
  return 0;
}


/* Release all resources allocated by the capability, which is in its
   freshly initialized state.  */
static void
cap_destructor (void *buffer)
{
  hurd_cap_t cap = (hurd_cap_t) buffer;

  assert (cap->srefs == 0);
  assert (cap->orefs == 0);

  pthread_mutex_destroy (&cap->lock);
}


/* Initialize the capability system.  */
error_t
hurd_cap_init (void)
{
  return hurd_slab_create (sizeof (struct hurd_cap),
			   cap_constructor, cap_destructor, &cap_space);
}


/* Modify the number of send references for the capability CAP by
   DELTA.  */
error_t
hurd_cap_mod_refs (hurd_cap_t cap, int delta)
{
  hurd_cap_sconn_t sconn;

  pthread_mutex_lock (&cap->lock);

  /* Verify that CAP->srefs is not 0 and will not become negative.  */
  if (cap->srefs == 0 || (delta < 0 && cap->srefs < -delta))
    {
      pthread_mutex_unlock (&cap->lock);
      return EINVAL;
    }

  /* Verify that CAP->srefs will not overflow.  */
  if (delta > 0 && cap->srefs > UINT32_MAX - delta)
    {
      pthread_mutex_unlock (&cap->lock);
      return EOVERFLOW;
    }

  cap->srefs += delta;
  if (cap->srefs != 0)
    {
      pthread_mutex_unlock (&cap->lock);
      return 0;
    }

  /* This was the last send reference we held.  Deallocate the server
     capability.  This is not so easy, though, as some other thread
     might concurrently try to enter a new reference for this
     capability.  Instead of doing reference counting for the
     capability IDs in the server connection, we get a temporary
     reference and acquire the server connection lock while the
     capability is temporarily unlocked.  Then we can check if we
     still have to deallocate the capabilty.  */
  sconn = cap->sconn;

  cap->srefs = 1;
  pthread_mutex_unlock (&cap->lock);

  /* Now we can try to remove ourselve from the server capability
     list.  CAP->sconn will not change while we hold our
     reference.  */
  pthread_mutex_lock (&sconn->lock);
  pthread_mutex_lock (&cap->lock);

  assert (cap->sconn == sconn);
  assert (cap->srefs != 0);
  cap->srefs--;
  if (cap->srefs != 0)
    {
      /* Someone else came in and got a reference to the almost dead
	 send capability.  Give up.  */
      pthread_mutex_unlock (&cap->lock);
      pthread_mutex_unlock (&sconn->lock);
      return 0;
    }
  
  /* The capability can now finally be removed.  */
  _hurd_cap_sconn_remove (sconn, cap->scid);

  if (cap->orefs == 0)
    {
      /* Return the capability to the pool.  */
      pthread_mutex_unlock (&cap->lock);
      hurd_slab_dealloc (cap_space, (void *) cap);
    }
  else
    pthread_mutex_unlock (&cap->lock);

  return 0;
}


/* Modify the number of object references for the capability CAP by
   DELTA.  */
error_t
hurd_cap_obj_mod_refs (hurd_cap_t cap, int delta)
{
  hurd_cap_ulist_t ulist;

  pthread_mutex_lock (&cap->lock);

  /* Verify that CAP->orefs is not 0 and will not become negative.  */
  if (cap->orefs == 0 || (delta < 0 && cap->orefs < -delta))
    {
      pthread_mutex_unlock (&cap->lock);
      return EINVAL;
    }

  /* Verify that CAP->orefs will not overflow.  */
  if (delta > 0 && cap->orefs > UINT32_MAX - delta)
    {
      pthread_mutex_unlock (&cap->lock);
      return EOVERFLOW;
    }

  cap->orefs += delta;
  if (cap->orefs != 0)
    {
      pthread_mutex_unlock (&cap->lock);
      return 0;
    }

  /* The object is going to be destroyed.  Remove the capability from
     each user.  This is not so easy, though, as some other thread
     might concurrently try to enter a new reference for this
     capability (for example, because of an incoming RPC).  Instead of
     doing reference counting for the capability in each user list, we
     get a temporary reference and acquire the user list lock while
     the capability is temporarily unlocked.  Then we can check if we
     still have to deallocate the capabilty.  */
  ulist = cap->ouser;

  cap->orefs = 1;
  pthread_mutex_unlock (&cap->lock);

  /* Now we can try to remove ourselve from the user lists.
     CAP->ulist will not change while we hold our reference.  */
  pthread_mutex_lock (&ulist->lock);
  pthread_mutex_lock (&cap->lock);

  assert (cap->ouser == ulist);
  assert (cap->orefs != 0);
  cap->orefs--;
  if (cap->orefs != 0)
    {
      /* Someone else came in and got a reference to the almost dead
	 capability object.  Give up.  */
      pthread_mutex_unlock (&cap->lock);
      pthread_mutex_unlock (&ulist->lock);
      return 0;
    }
  
  /* The capability object can now finally be removed.  */
  _hurd_cap_ulist_remove (ulist, cap);
  pthread_mutex_unlock (&ulist->lock);

  if (cap->srefs == 0)
    {
      /* Return the capability to the pool.  */
      pthread_mutex_unlock (&cap->lock);
      hurd_slab_dealloc (cap_space, (void *) cap);
    }
  else
    pthread_mutex_unlock (&cap->lock);

  return 0;

}
