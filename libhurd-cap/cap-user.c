/* cap-user.c - User side of the capability implementation.
   Copyright (C) 2003 Free Software Foundation, Inc.
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
#include <pthread.h>
#include <assert.h>
#include <errno.h>
#include <stdint.h>

#include <hurd/cap.h>

#include "cap-intern.h"


/* This hash table maps server thread IDs to server connections.  */
static struct hurd_ihash server_to_sconn
  = HURD_IHASH_INITIALIZER (HURD_IHASH_NO_LOCP);

/* This lock protects SERVER_TO_SCONN.  You can also lock server
   connection objects while holding this lock.  */
static pthread_mutex_t server_to_sconn_lock = PTHREAD_MUTEX_INITIALIZER;


/* Deallocate one reference for SCONN, which must be locked.
   SERVER_TO_SCONN_LOCK is not locked.  Afterwards, SCONN is unlocked
   (if it still exists).  */
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
  hurd_ihash_remove (&server_to_sconn, sconn->server_thread.raw);
  pthread_mutex_unlock (&server_to_sconn_lock);

  /* Finally, we can destroy it.  */
  pthread_mutex_unlock (&sconn->lock);
  pthread_mutex_destroy (&sconn->lock);
  hurd_ihash_destroy (&sconn->id_to_cap);
  if (sconn->server_task_info)
    hurd_cap_deallocate (sconn->server_task_info);
  free (sconn);
}


/* Remove the entry for the capability ID SCID from the server
   connection SCONN.  SCONN is locked.  Afterwards, SCONN is
   unlocked.  */
void
_hurd_cap_sconn_remove (hurd_cap_sconn_t sconn, l4_word_t scid)
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
   (with the task ID reference SERVER_TASK_INFO) and the cap ID SCID.
   SCONN is the server connection for SERVER_THREAD, if known.  It
   should be unlocked.  If SCONN is NULL, then SERVER_TASK_INFO should
   be the task info capability for the server SERVER_THREAD, otherwise
   it must be HURD_CAP_NULL.  Both, SCONN and SERVER_TASK_INFO, are
   consumed if used.  If successful, the locked capability is returned
   with one (additional) reference in CAP.  The server connection and
   capability object are created if necessary.  */
error_t
_hurd_cap_sconn_enter (hurd_cap_sconn_t sconn_provided,
		       hurd_task_info_t server_task_info,
		       l4_thread_id_t server_thread, uint32_t scid,
		       hurd_cap_t *cap)
{
  hurd_cap_sconn_t sconn = sconn_provided;
  int sconn_created = 0;

  if (sconn)
    assert (l4_is_thread_equal (sconn->server_thread, server_thread));
  else
    {
      /* It might have become available by now.  */
      pthread_mutex_lock (&server_to_sconn_lock);
      sconn = hurd_ihash_find (&server_to_sconn, server_thread.raw);
      if (sconn)
	hurd_cap_deallocate (server_task_info);
      else
	{
	  error_t err;

	  sconn = malloc (sizeof (*sconn));
	  if (!sconn)
	    {
	      pthread_mutex_unlock (&server_to_sconn_lock);
	      hurd_cap_deallocate (server_task_info);
	      return errno;
	    }
	  err = pthread_mutex_init (&sconn->lock, NULL);
	  if (err)
	    {
	      free (sconn);
	      pthread_mutex_unlock (&server_to_sconn_lock);
	      hurd_cap_deallocate (server_task_info);
	      return errno;
	    }

	  hurd_ihash_init (&sconn->id_to_cap, HURD_IHASH_NO_LOCP);
	  sconn->server_thread = server_thread;
	  sconn->server_task_info = server_task_info;
	  sconn->refs = 0;

	  /* Enter the new server connection object.  */
	  err = hurd_ihash_add (&server_to_sconn, server_thread.raw, sconn);
	  if (err)
	    {
	      pthread_mutex_destroy (&sconn->lock);
	      hurd_ihash_destroy (&sconn->id_to_cap);
	      free (sconn);
	      pthread_mutex_unlock (&server_to_sconn_lock);
	      hurd_cap_deallocate (server_task_info);
	      return errno;
	    }
	}
    }
  pthread_mutex_lock (&sconn->lock);
  pthread_mutex_unlock (&server_to_sconn_lock);

  (*cap) = hurd_ihash_find (&sconn->id_to_cap, scid);
  if (!cap)
    {
      error_t err = hurd_slab_alloc (cap_space, cap);
      if (err)
	{
	  _hurd_cap_sconn_dealloc (sconn);
	  return err;
	}

      (*cap)->sconn = sconn;
      (*cap)->scid = scid;
#if 0
      (*cap)->dead_cb = NULL;
#endif

      err = hurd_ihash_add (&sconn->id_to_cap, scid, *cap);
      if (err)
	{
	  _hurd_cap_sconn_dealloc (sconn);
	  hurd_slab_dealloc (cap_space, *cap);
	  return err;
	}
    }
  pthread_mutex_lock (&(*cap)->lock);
  (*cap)->srefs++;
  /* We have to add a reference for the capability we have added,
     unless we are consuming the reference that was provided.  */
  if (!sconn_provided)
    sconn->refs++;
  pthread_mutex_unlock (&sconn->lock);

  return 0;
}
