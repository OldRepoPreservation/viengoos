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

#include <pthread.h>
#include <error.h>

#include <hurd/ihash.h>
#include <hurd/slab.h>
#include <l4/types.h>


/* Initialize the capability system.  */
error_t hurd_cap_init (void);


/* Capabilities provided by other servers.  */
struct hurd_cap_sconn
{
  /* The server thread to which messages should be sent.  */
  l4_thread_id_t server_thread;

  /* A reference for the servers task ID to prevent reuse.  */
  task_id_t server_task_id;

  /* The lock protecting the variable members of the server connection
     object.  */
  pthread_mutex_t lock;

  /* The number of references to this server connection object.  */
  unsigned int refs;

  /* A hash mapping the capability IDs to capability objects.  */
  hurd_ihash_t id_to_cap;
};


/* User capabilities.  */

/* The task-specific ID for this capability.  */
typedef l4_word_t hurd_cap_scid_t;


/* Remove the entry for the capability CAP from the user list ULIST.
   ULIST (and the capability CAP) are locked.  */
void _hurd_cap_ulist_remove (ulist, cap);


/* The capability structure.  */
struct hurd_cap
{
  /* The lock protecting the capability.  This lock protects all the
     members in the capability structure.  */
  pthread_mutex_t lock;


  /* Information for send capabilities.  */

  /* The number of send references for this capability.  If this is 0,
     then this capability can not be used to send messages to the
     server providing the capability.  */
  unsigned int srefs;

  /* The server connection for this capability.  If this is NULL, then
     the capability is dead.  */
  hurd_cap_sconn_t sconn;

  /* The task-specific ID for this capability.  Only valid id SCONN is
     not NULL.  */
  hurd_cap_scid_t scid;

  /* A callback for the user of the capability, invoked when the
     capability is destroyed.  */
  hurd_cap_dead_t dead_cb;


  /* Information for local capabilities.  */

  /* The number of object references for this capability.  If this is
     0, the this capability is not implemented locally.  */
  unsigned int orefs;

  /* The object that is behind this capability.  */
  void *odata;

  /* A list of remote users.  */
  hurd_cap_ulist_t ouser;

  /* A callback invoked when the capability is destroyed.  */
  hurd_cap_odead_cb_t odead_cb;

  /* A callback to be invoked when the capability has no more
     senders.  */
  hurd_cap_no_sender_cb_t no_sender_cb;
};
typedef struct hurd_cap hurd_cap_t;
