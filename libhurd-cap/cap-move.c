/* cap-move.c - Moving a capability reference from one task to another.
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


/* FIXME:  This is only some pseudo code to get the hang of it.  */

/* Sender side.  */

/* Send the capability CAP to DEST.  */
error_t
cap_send (hurd_cap_t cap, hurd_cap_t dest, int copy)
{
  error_t err;
  hurd_cap_scid_t cont_id;

  /* This is a low-level RPC to create a new reference container.  */
  err = hurd_cap_server_create_ref_cont
    (cap,
     hurd_task_id_from_thread_id (hurd_cap_get_server_thread (dest)),
     &cont_id);
  if (err)
    return err;

  /* This is the actual RPC sending the reference in a message to DEST.  */
  err = hurd_SOME_RPC (dest, ...,
		       hurd_cap_get_server_thread (cap), cont_id,
		       ...);
  if (err)
    {
      /* FIXME: If this fails, then we can only ignore it.  */
      hurd_cap_server_destroy_ref_cont (cap, cont_id);
      return err;
    }

  /* We have to deallocate the reference container under all
     circumstances.  In general, we could trust the server to do it
     automatically when the reference container got accepted, and the
     receiver of the capability indicates success.  However, if there
     were a failure we would not know if the reference was received
     already, and as the container ID could be reused once it is
     released, we would have no way to find out.  So this is a
     robustness issue, and not a security issue, that forces us to
     keep the container ID alive in the server and destroy it here
     unconditionally.  */
  err = hurd_cap_server_destroy_ref_cont (cap, cont_id);
  if (err)
    return err;

  /* If we are moving the capability, we can deallocate it now.  If we
     are copying the capability, we should of course not do that.  */
  if (!copy)
    {
      err = hurd_cap_mod_refs (cap, -1);
      if (err)
	return err;
    }

  return 0;
}


/* Receiver side.  */

/* Capabilities are received in normal message RPCs.  This is the
   server stub of one of them.

   FIXME: For now this assumes that server_thread is not implemented
   by this task.  */
error_t
cap_receive (l4_thread_id_t sender_thread,
	     void *some_object, ..., l4_thread_id_t server_thread,
	     hurd_cap_scid_t cont_id, ...)
{
  error_t err;
  hurd_cap_sconn_t sconn;
  hurd_cap_t server_task_info = HURD_CAP_NULL;
  hurd_task_id_t sender_task = hurd_task_id_from_thread_id (sender_thread);
  hurd_cap_scid_t obj_id;

  /* We have a chance to inspect the thread ID now and decide if we
     want to accept the handle.  */
#if FOR_EXAMPLE_A_REAUTHENTICATION_REQUEST
  if (server_thread != hurd_cap_get_server_thread (hurd_my_auth ()))
    return EINVAL;
#endif

  /* Acquire a reference to the server connection if it exists.  */
  sconn = _hurd_cap_sconn_find (server_thread);
  if (sconn)
    pthread_mutex_unlock (&sconn->lock);
  else
    {
      /* If no connection to this server exists already, prepare to
	 create a new one.  The sender task ID tells the task server
	 that we only want to get the info cap if the sender task
	 still lives at the time it is created (because we rely on the
	 sender to secure the server task ID).  */

      /* FIXME: This probably should check for the server being the
	 task server.  However, the implementation could always
	 guarantee that the task server has an SCONN object
	 already, so the lookup above would be successful.  */
      err = hurd_task_info_create (hurd_task_self (),
				   hurd_task_id_from_thread_id (server_thread),
				   sender_task,
				   &server_task_info);
      if (err)
	return err;
    }

  /* This is a very low-level RPC to accept the reference from the
     server.  */
  err = hurd_cap_server_accept_ref_cont (server_thread, sender_task,
					 cont_id, &obj_id);
  if (err)
    {
      if (sconn)
	{
	  pthread_mutex_lock (&sconn->lock);
	  _hurd_cap_sconn_dealloc (sconn);
	}
      else
	hurd_cap_mod_refs (server_task_info, -1);

      return err;
    }

  /* This consumes our references to sconn and server_task_info, if
     any.  */
  return _hurd_cap_sconn_enter (sconn, server_task_info,
				server_thread, obj_id);
}


/* The server side.  */

/* Containers are created in the sender's user list, but hold a
   reference for the receiver user list.  The receiver user list also
   has a section with backreferences to all containers for this
   receiver.

   At look up time, the receiver user list can be looked up.  Then the
   small list can be searched to verify the request.  If it is
   verified, the entry can be removed.  Then the sender can be looked
   up to access the real reference container item.  The reference for
   the receiver user list should not be released because it will
   usually be consumed for the capability.

   Without the small list on the receiver user list side there could
   be DoS attacks where a malicious receiver constantly claims it
   wants to accept a container from another client and keeps the user
   list of that client locked during the verification attempts.

   First the sender container is created.

   Then the receiver container is created.

   Removal of containers works the other way round.  This is the most
   robust way to do it, in case the sender destroys the container
   asynchronously with the receiver trying to accept the container.
   First an invalid send container (id) has to be allocated, then the
   receiver container has to be created, and then the sender container
   can be filled with the right receiver container id.  */

/* Create a reference container for DEST.  */
error_t
hurd_cap_server_create_ref_cont_S (l4_thread_id_t sender_thread,
				   void *object,
				   hurd_task_id_t dest,
				   hurd_cap_scid_t *cont_id)
{
  error_t err;

  /* FIXME:  Needs to be written in more detail.  Here is a list:

  1. Check if a user list for dest exists, if not, create one.  Use
  the senders task ID as a constraint to the task_info_create call, to
  ensure we don't create a task info cap if the sender dies in the
  meantime.

  Note: Order matters in the following two steps:

  2. Register the container as a sending container with the sender user list.

  3. Register the container as a receiving container with the dest
  user list.  The user dest list should have its own small list just
  with containers, and this list should have its own lock.  There is
  no precondition for this lock.  A non-empty list implies one
  reference to the task info cap (no explicit reference is taken to
  avoid locking problems).

  This is how task death notifications should be handled:

  If the sender dies, remove the container on both sides, first on the
  receiver side, then on the sender side.

  If the receiver dies, remove the container on the receiving side,
  but keep the container on the sender side.  Invalidate the container
  on the sender side, so it can not be used anymore (only
  destroyed).  */
}

/* Accept a reference.  */

/* This is a very low-level RPC to accept the reference from the
   server.  */
error_t
hurd_cap_server_accept_ref_cont (l4_thread_id_t sender_thread,
				 hurd_task_id_t source,
				 hurd_cap_scid_t cont_id,
				 hurd_cap_scid_t *obj_id)
{
  /* FIXME: Write this one.  This is what should be done:

  1. Look up the sender user list.  In that, lookup the container with
  the given cont id.  Check that this containter comes from the task
  SOURCE.  (This information should be stored in the sender user list!
  so no lookup of the SOURCE user list is required up to this point.

  2. Now that the validity of the request is confirmed, remove the
  container ID from the receiver side.  Unlock the sender user list.
  
  3. Look up the SOURCE user list.  Look up the container.  Look up
  the capability that is wrapped by the container.  Increase its
  reference.  Invalidate the container, but keep it around.  Unlock
  the SOURCE user list.

  4. Enter the capability into the SENDER user list.  Return its
  ID.  */
}


error_t
hurd_cap_server_destroy_ref_cont (hurd_cap_t cap, hurd_cap_scid_t cont_id)
{
  /* To be written */
}
