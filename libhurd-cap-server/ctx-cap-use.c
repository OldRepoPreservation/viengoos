/* ctx-cap-use.c - Use capabilities within an RPC context.
   Copyright (C) 2005 Free Software Foundation, Inc.
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
#include <stdlib.h>

#include <hurd/cap-server.h>

#include <compiler.h>

#include "cap-server-intern.h"


/* Return the number of bytes required for a hurd_cap_ctx_cap_use
   structure.  */
size_t
hurd_cap_ctx_size (void)
{
  return sizeof (struct hurd_cap_ctx_cap_use);
}

/* If you want to use other capabilities in an RPC handler beside the
   one on which the RPC was invoked, you need to make sure that
   inhibition works on those other capabilities and cancel your
   operation.  For this, the following interfaces are provided.  */

/* The calling thread wishes to execute an RPC on the the handle
   HANDLE.  The calling thread must already be registered as executing
   an RPC.  RPC_CTX is the cooresponding RPC context.  The function
   uses the structure CAP_USE, which must point to the number of bytes
   returned by hurd_cap_ctx_size, to store data required by
   hurd_cap_ctx_end_cap_use.  The capability object corresponding to
   HANDLE is locked and returned in *OBJP.

   Returns EINVAL if the capability handle is invalid for the client.

   Returns ENOENT if there is no object associated with handle HANDLE.

   Returns EBAD if the capability is dead.

   Returns EDOM if the object associated with HANDLE is not in class
   REQUIRED_CLASS.  If no type check is required, it will be skipped
   if REQURIED_CLASS is NULL.  */
error_t
hurd_cap_ctx_start_cap_use (hurd_cap_rpc_context_t rpc_ctx,
			    hurd_cap_handle_t handle,
			    hurd_cap_class_t required_class,
			    struct hurd_cap_ctx_cap_use *cap_use,
			    hurd_cap_obj_t *objp)
{
  error_t err = 0;
  hurd_cap_bucket_t bucket = rpc_ctx->bucket;
  _hurd_cap_client_t client = rpc_ctx->client;
  hurd_cap_obj_t obj;
  hurd_cap_class_t cap_class;
  _hurd_cap_obj_entry_t obj_entry;
  _hurd_cap_obj_entry_t *obj_entryp;


  /* HANDLE must belong to the same client as RPC_CTX->HANDLE.  */
  if (_hurd_cap_client_id (handle) != _hurd_cap_client_id (rpc_ctx->handle))
    return EINVAL;

  pthread_mutex_lock (&client->lock);

  /* Look up the object.  */
  obj_entryp = (_hurd_cap_obj_entry_t *)
    hurd_table_lookup (&client->caps, _hurd_cap_id (handle));
  if (!obj_entryp)
    err = ENOENT;
  else
    {
      cap_use->_obj_entry = obj_entry = *obj_entryp;

      if (EXPECT_FALSE (!obj_entry->external_refs))
	err = ENOENT;
      else if (EXPECT_FALSE (obj_entry->dead))
	err = EBADF;
      else
	{
	  obj_entry->internal_refs++;
	  *objp = obj = obj_entry->cap_obj;
	}
    }
  pthread_mutex_unlock (&client->lock);

  if (err)
    /* Either the capability ID is invalid, or it was revoked.  */
    return err;

  /* If HANDLE and RPC_CTX->HANDLE are the same, we are done.  */
  if (EXPECT_FALSE (_hurd_cap_id (handle) == _hurd_cap_id (rpc_ctx->handle)))
    {
      assert (obj == rpc_ctx->obj);
      return 0;
    }

  /* At this point, CAP and OBJ are valid and we have one internal
     reference to the capability entry.  */

  cap_class = obj->cap_class;

  if (required_class && cap_class != required_class)
    {
      err = EINVAL;
      goto client_cleanup;
    }

  if (cap_class != rpc_ctx->obj->cap_class)
    /* The capability class is not the same as the first caps.  We
       need to add ourself to the cap class pending rpc list.  */
    {
      pthread_mutex_lock (&cap_class->lock);
      /* First, we have to check if the class is inhibited, and if it is,
	 we have to wait until it is uninhibited.  */
      while (!err && cap_class->state != _HURD_CAP_STATE_GREEN)
	err = hurd_cond_wait (&cap_class->cond, &cap_class->lock);
      if (err)
	{
	  /* Canceled.  */
	  pthread_mutex_unlock (&cap_class->lock);
	  goto client_cleanup;
	}

      /* Now add ourself to the pending rpc list of the class  */
      cap_use->_worker_class.thread = pthread_self ();
      cap_use->_worker_class.tid = l4_myself ();
      _hurd_cap_list_item_add (&cap_class->pending_rpcs,
			       &cap_use->_worker_class);

      pthread_mutex_unlock (&cap_class->lock);
    }

  pthread_mutex_lock (&obj->lock);
  /* First, we have to check if the object is inhibited, and if it is,
     we have to wait until it is uninhibited.  */
  if (obj->state != _HURD_CAP_STATE_GREEN)
    {
      pthread_mutex_unlock (&obj->lock);
      pthread_mutex_lock (&cap_class->obj_cond_lock);
      pthread_mutex_lock (&obj->lock);
      while (!err && obj->state != _HURD_CAP_STATE_GREEN)
	{
	  pthread_mutex_unlock (&obj->lock);
	  err = hurd_cond_wait (&cap_class->obj_cond,
				&cap_class->obj_cond_lock);
	  pthread_mutex_lock (&obj->lock);
	}
      pthread_mutex_unlock (&cap_class->obj_cond_lock);
    }
  if (err)
    {
      /* Canceled.  */
      pthread_mutex_unlock (&obj->lock);
      goto class_cleanup;
    }

  /* Now check if the client still has the capability, or if it was
     revoked.  */
  pthread_mutex_lock (&client->lock);
  if (obj_entry->dead)
    err = EBADF;
  pthread_mutex_unlock (&client->lock);
  if (err)
    {
      /* The capability was revoked in the meantime.  */
      pthread_mutex_unlock (&obj->lock);
      goto class_cleanup;
    }

  cap_use->_worker_obj.thread = pthread_self ();
  cap_use->_worker_obj.tid = l4_myself ();
  _hurd_cap_list_item_add (&cap_class->pending_rpcs, &cap_use->_worker_obj);
  
  /* At this point, we have looked up the capability, acquired an
     internal reference for its entry in the client table (which
     implicitely keeps a reference acquired for the object itself),
     acquired a reference for the capability client in the bucket, and
     have added an item to the pending_rpcs lists in the class (if
     necessary) and object.  The object is locked.  */

  return 0;

 class_cleanup:
  if (cap_use->_obj_entry->cap_obj->cap_class != rpc_ctx->obj->cap_class)
    /* Different classes.  */
    {
      pthread_mutex_lock (&cap_class->lock);
      _hurd_cap_list_item_remove (&cap_use->_worker_class);
      _hurd_cap_class_cond_check (cap_class);
      pthread_mutex_unlock (&cap_class->lock);
    }

 client_cleanup:
  pthread_mutex_lock (&client->lock);

  /* You are not allowed to revoke a capability while there are
     pending RPCs on it.  This is the reason we know that there must
     be at least one extra internal reference.  FIXME: For
     cleanliness, this could still call some inline function that does
     the decrement.  The assert can be a hint to the compiler to
     optimize the inline function expansion anyway.  */
  assert (!obj_entry->dead);
  assert (obj_entry->internal_refs > 1);
  obj_entry->internal_refs--;
  pthread_mutex_unlock (&client->lock);

  return err;
}


/* End the use of the object CAP_USE->OBJ, which must be locked.  */
void
hurd_cap_ctx_end_cap_use (hurd_cap_rpc_context_t rpc_ctx,
			  struct hurd_cap_ctx_cap_use *cap_use)
{
  _hurd_cap_obj_entry_t entry = cap_use->_obj_entry;
  hurd_cap_obj_t obj = entry->cap_obj;
  _hurd_cap_client_t client = rpc_ctx->client;
  
  /* Is this an additional use of the main capability object?  */
  if (EXPECT_TRUE (obj != rpc_ctx->obj))
    /* No.  */
    {
      hurd_cap_class_t cap_class = obj->cap_class;

      /* End the RPC on the object.  */
      _hurd_cap_list_item_remove (&cap_use->_worker_obj);
      _hurd_cap_obj_cond_check (obj);
  
      if (cap_class != rpc_ctx->obj->cap_class)
	/* The capability object is in a different class from the primary
	   capability object.  */
	{
	  pthread_mutex_lock (&cap_class->lock);
	  _hurd_cap_list_item_remove (&cap_use->_worker_class);
	  _hurd_cap_class_cond_check (cap_class);
	  pthread_mutex_unlock (&cap_class->lock);
	}
    }

  hurd_cap_obj_unlock (obj);

  /* You are not allowed to revoke a capability while there are
     pending RPCs on it.  This is the reason why we know that there
     must be at least one extra internal reference.  FIXME: For
     cleanliness, this could still call some inline function that does
     the decrement.  The assert can be a hint to the compiler to
     optimize the inline function expansion anyway.  */

  pthread_mutex_lock (&client->lock);
  assert (!entry->dead);
  assert (entry->internal_refs > 1);
  entry->internal_refs--;
  pthread_mutex_unlock (&client->lock);
}
