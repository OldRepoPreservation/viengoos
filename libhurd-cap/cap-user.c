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

#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include <errno.h>

#include <hurd/cap.h>

#include "cap-intern.h"


static struct hurd_ihash server_to_sconn = HURD_IHASH_INITIALIZER;
static pthread_mutex_t server_to_sconn_lock = PTHREAD_MUTEX_INITIALIZER;


/* Deallocate one reference for SCONN, which must be locked.
   SERVER_TO_SCONN_LOCK is not locked.  Afterwards, SCONN is
   unlocked.  */
void
_hurd_cap_sconn_dealloc (hurd_cap_sconn_t sconn)
{
  assert (sconn->refs > 0);

  sconn->refs--;
  if (sconn->refs > 0)
    {
      pthread_mutex_unlock (&sconn->lock);
      return;
    }

  /* We have to get SERVER_TO_SCONN_LOCK, but the locking order does
     not allow us to do it directly.  So we release SCONN
     temporarily.  */
  sconn->refs = 1;
  pthread_mutex_unlock (&sconn->lock);
  pthread_mutex_lock (&server_to_sconn_lock);
  pthread_mutex_lock (&sconn->lock);
  assert (sconn->refs > 0);
  sconn->refs--;
  if (sconn->refs > 0)
    {
      pthread_mutex_unlock (&sconn->lock);
      return;
    }

  /* Now we can remove the object.  */
  hurd_ihash_remove (&server_to_sconn, sconn->server_thread);
  pthread_mutex_unlock (&server_to_sconn_lock);

  /* Finally, we can destroy it.  */
  pthread_mutex_unlock (&sconn->lock);
  pthread_mutex_destroy (&sconn->lock);
  hurd_ihash_destroy (&sconn->id_to_cap);
  hurd_cap_deallocate (sconn->server_task_id);
  free (sconn);
}


/* Remove the entry for the capability ID SCID from the server
   connection SCONN.  SCONN is locked.  Afterwards, SCONN is
   unlocked.  */
void
_hurd_cap_sconn_remove (sconn, scid)
{
  /* Remove the capability object pointer, which is now invalid.  */
  hurd_ihash_remove (&sconn->id_to_cap, scid);
  /* FIXME: The following should be some low level RPC to deallocate
     the capability on the server side.  If it fails, then what can we
     do at this point?  */
  hurd_cap_server_deallocate (sconn->server_thread, scid);

  _hurd_cap_sconn_dealloc (sconn);
}


/* Enter a new send capability provided by the server SERVER_THREAD
   (with the task ID reference SERVER_TASK_ID) and the cap ID SCID.
   The SERVER_TASK_ID reference is _not_ consumed but copied if
   necessary.  If successful, the locked capability is returned with
   one (additional) reference in CAP.  The server connection and
   capability object are created if necessary.  */
error_t
_hurd_cap_sconn_enter (l4_thread_id_t server_thread, uint32_t scid,
		       task_id_t server_task_id, hurd_cap_t *cap)
{
  hurd_cap_sconn_t sconn;
  int sconn_created = 0;

  pthread_mutex_lock (&server_to_sconn_lock);
  sconn = hurd_ihash_find (&server_to_sconn, server_thread);
  if (!sconn)
    {
      error_t err;

      sconn = malloc (sizeof (*sconn));
      if (!sconn)
	{
	  pthread_mutex_unlock (&server_to_sconn_lock);
	  return errno;
	}
      err = pthread_mutex_init (&sconn->lock, NULL);
      if (err)
	{
	  free (sconn);
	  pthread_mutex_unlock (&server_to_sconn_lock);
	  return errno;
	}
      hurd_ihash_init (&sconn->id_to_cap);

      sconn->server_thread = server_thread;
      sconn->server_task_id = server_task_id;
      sconn->refs = 0;

      /* Enter the new server connection object.  */
      err = hurd_ihash_add (&server_to_sconn, server_thread, sconn, NULL);
      if (err)
	{
	  pthread_mutex_destroy (&sconn->lock);
	  hurd_ihash_destroy (&sconn->id_to_cap);
	  free (sconn);
	  pthread_mutex_unlock (&server_to_sconn_lock);
	  return errno;
	}

      /* FIXME: This could, theoretically, overflow.  */
      hurd_cap_mod_refs (server_task_id, +1);
    }
  pthread_mutex_lock (&sconn->lock);
  sconn->refs++;
  pthread_mutex_unlock (&server_to_sconn_lock);

  cap = hurd_ihash_find (&sconn->id_to_cap, scid);
  if (!cap)
    {
      error_t err = hurd_slab_alloc (cap_space, &cap);
      if (err)
	{
	  _hurd_cap_sconn_dealloc (sconn);
	  return err;
	}

      cap->sconn = sconn;
      cap->scid = scid;
      cap->dead_cb = NULL;

      err = hurd_ihash_add (&sconn->id_to_cap, scid, cap, 0);
      if (err)
	{
	  _hurd_cap_sconn_dealloc (sconn);
	  hurd_slab_dealloc (cap_space, cap);
	  return err;
	}
    }
  pthread_mutex_lock (&cap->lock);
  cap->srefs++;
  pthread_mutex_unlock (&sconn->lock);

  return 0;
}
