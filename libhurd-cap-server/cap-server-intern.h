/* cap-server-intern.h - Internal interface to the Hurd capability library.
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

#ifndef _HURD_CAP_SERVER_INTERN_H
#define _HURD_CAP_SERVER_INTERN_H	1

#include <hurd/types.h>
#include <hurd/ihash.h>
#include <hurd/cap-server.h>

#include <compiler.h>

#include "table.h"
#include "task-death.h"


/* FIXME: For now.  */
#define hurd_cond_wait pthread_cond_wait

/* Every capability hurd_cap_handle_t consists of two parts: The upper
   part is a client ID and the lower part is a capability object ID.
   The client ID is as long as the task ID (which is as long as the
   version ID).  The cap ID occupies the remainder.  We intimately
   know that even on 64 bit architectures, both fit into a 32 bit
   integer value.  */
#define _HURD_CAP_CLIENT_ID_BITS	HURD_TASK_ID_BITS
#define _HURD_CAP_ID_BITS ((sizeof (hurd_cap_handle_t) * 8) \
			   - HURD_TASK_ID_BITS)

#define _HURD_CAP_CLIENT_ID_MASK \
  ((L4_WORD_C(1) << _HURD_CAP_CLIENT_ID_BITS) - 1)
#define _HURD_CAP_ID_MASK ((L4_WORD_C(1) << _HURD_CAP_ID_BITS) - 1)

typedef l4_uint32_t _hurd_cap_id_t;
typedef l4_uint32_t _hurd_cap_client_id_t;


/* Get the capability ID from a user capability.  The capabililty ID
   is an index into the caps table of a client.  */
static inline _hurd_cap_client_id_t
__attribute__((always_inline))
_hurd_cap_client_id (hurd_cap_handle_t cap)
{
  return cap >> _HURD_CAP_ID_BITS;
}


/* Get the capability ID from a user capability.  The capabililty ID
   is an index into the caps table of a client.  */
static inline _hurd_cap_id_t
__attribute__((always_inline))
_hurd_cap_id (hurd_cap_handle_t cap)
{
  return cap & _HURD_CAP_ID_MASK;
}


/* Create a new capability handle from the client and cap ID.  */
static inline hurd_cap_handle_t
__attribute__((always_inline))
_hurd_cap_handle_make (_hurd_cap_client_id_t client_id, _hurd_cap_id_t cap_id)
{
  return ((client_id & _HURD_CAP_CLIENT_ID_MASK) << _HURD_CAP_ID_BITS)
    | (cap_id & _HURD_CAP_ID_MASK);
}


/* This is a simple list item, used to maintain lists of pending RPC
   worker threads in a class, client or capability object.  */
struct _hurd_cap_list_item
{
  _hurd_cap_list_item_t next;
  _hurd_cap_list_item_t *prevp;

  union
  {
    /* Used for maintaining lists of pending RPC worker threads in a
       class, client or capability object.  */
    struct
    {
      /* This location pointer is used for fast removal from the
	 BUCKET->senders.  */
      hurd_ihash_locp_t locp;

      /* The worker thread processing the RPC.  */
      pthread_t thread;

      /* The worker thread L4 thread ID.  */
      l4_thread_id_t tid;
    };

    /* Used for reverse lookup of capability clients using a
       capability object.  */
    struct
    {
      _hurd_cap_client_t client;
    };
  };
};

/* Add the list item ITEM to the list LIST.  */
static inline void
__attribute__((always_inline))
_hurd_cap_list_item_add (_hurd_cap_list_item_t *list,
			 _hurd_cap_list_item_t item)
{
  if (*list)
    (*list)->prevp = &item->next;
  item->prevp = list;
  item->next = *list;
  *list = item;
}


/* Remove the list item ITEM from the list.  */
static inline void
__attribute__((always_inline))
_hurd_cap_list_item_remove (_hurd_cap_list_item_t item)
{
  if (item->next)
    item->next->prevp = item->prevp;
  *(item->prevp) = item->next;
  item->prevp = NULL;
}


/* Check if the item ITEM is dequeued or not.  */
static inline bool
__attribute__((always_inline))
_hurd_cap_list_item_dequeued (_hurd_cap_list_item_t item)
{
  return item->prevp == NULL;
}


/* Deallocate the capability object OBJ, which must be locked and have
   no more references.  */
void _hurd_cap_obj_dealloc (hurd_cap_obj_t obj)
     __attribute__((visibility("hidden")));


/* Remove one reference for the capability object OBJ, which must be
   locked, and will be unlocked when the function returns.  If this
   was the last user of this object, the object is deallocated.  */
static inline void
_hurd_cap_obj_drop (hurd_cap_obj_t obj)
{
  if (EXPECT_TRUE (!atomic_decrement_and_test (&obj->refs)))
    hurd_cap_obj_unlock (obj);
  else
    _hurd_cap_obj_dealloc (obj);
}


/* Client capabilities.  */

/* The following data type is used pointed to by an entry in the caps
   table of a client.  Except noted otherwise, its members are
   protected by the same lock as the table.  */
struct _hurd_cap_obj_entry
{
  /* The capability object.  */
  hurd_cap_obj_t cap_obj;

  /* The index in the capability table.  */
  _hurd_cap_id_t id;
  
  /* A list item that is used for reverse lookup from the capability
     object to the client.  Protected by the lock of the capability
     object.  */
  struct _hurd_cap_list_item client_item;

  /* A flag that indicates if this capability is dead (revoked).  Note
     that CAP_OBJ is still valid until all internal references have
     been removed.  */
  unsigned int dead : 1;

  /* The number of internal references.  These are references taken by
     the server for its own purpose.  In fact, there is one such
     reference for all outstanding external references (if the dead
     flag is not set), and one for each pending RPC that uses this
     capability.  If this reference counter drops to zero, the one
     real capability object reference held by this capability entry is
     released, and CAP_OBJ becomes invalid.  */
  unsigned int internal_refs : 15;

  /* The number of external references.  These are references granted
     to the user.  For all these references, one internal reference is
     taken, unless the DEAD flag is set.  */
  unsigned int external_refs : 16;
};
typedef struct _hurd_cap_obj_entry *_hurd_cap_obj_entry_t;


/* The global slab for all capability entries.  */
extern struct hurd_slab_space _hurd_cap_obj_entry_space
	__attribute__((visibility("hidden")));
  
/* Copy out a capability for the capability OBJ to the user CLIENT.
   Returns the capability ID (valid only for this user) in *R_ID, or
   an error.  OBJ must be locked.  */
error_t _hurd_cap_obj_copy_out (hurd_cap_obj_t obj, hurd_cap_bucket_t bucket,
				_hurd_cap_client_t client, _hurd_cap_id_t *r_id)
     __attribute__((visibility("hidden")));


/* Client connections. */

/* This data type holds all the information about a client
   connection.  */
struct _hurd_cap_client
{
  /* The task ID of the client.  */
  hurd_task_id_t task_id;

  /* The index of the client in the client table of the bucket.
     This is here so that we can hash for the address of this struct
     in the clients_reverse hash of the bucket, and still get the
     index number.  This allows us to use a location pointer for
     removal (locp) for fast hash removal.  */
  _hurd_cap_client_id_t id;

  /* The location pointer for fast removal from the reverse lookup
     hash BUCKET->clients_reverse.  This is protected by the bucket
     lock.  */
  hurd_ihash_locp_t locp;

  /* The lock protecting all the following members.  Client locks can
     be taken while capability object locks are held!  This is very
     important when copying or removing capabilities.  On the other
     hand, this means you are not allowed to lock cabaility objects
     when holding a client lock.  */
  pthread_mutex_t lock;

  /* The state of the client.  If this is _HURD_CAP_STATE_GREEN, you
     can process RPCs for this client.  Otherwise, you should drop
     RPCs for this client.  If this is _HURD_CAP_STATE_YELLOW, and you
     are the last pending_rpc to finish, you have to broadcast the
     client_cond of the bucket.  */
  enum _hurd_cap_state state;

  /* The current waiter thread.  This is only valid if state is
     _HURD_CAP_STATE_YELLOW.  Used by _hurd_cap_client_cond_busy ().  */
  pthread_t cond_waiter;

  /* The pending RPC list.  Each RPC worker thread should add itself
     to this list, so it can be cancelled by the task death
     notification handler.  */
  struct _hurd_cap_list_item *pending_rpcs;

  /* The _hurd_cap_id_t to _hurd_cap_obj_entry_t mapping.  */
  struct hurd_table caps;

  /* Reverse lookup from hurd_cap_obj_t to _hurd_cap_obj_entry_t.  */
  struct hurd_ihash caps_reverse;
};


/* The global slab space for all capability clients.  */
extern struct hurd_slab_space _hurd_cap_client_space
	__attribute__((visibility("hidden")));


/* Look up the client with the task ID TASK in the class CLASS, and
   return it in R_CLIENT, with one additional reference.  If it is not
   found, create it.  */
error_t _hurd_cap_client_create (hurd_cap_bucket_t bucket,
				 hurd_task_id_t task_id,
				 _hurd_cap_client_t *r_client)
     __attribute__((visibility("hidden")));


/* Deallocate the connection client CLIENT.  */
void _hurd_cap_client_dealloc (hurd_cap_bucket_t bucket,
			       _hurd_cap_client_t client)
     __attribute__((visibility("hidden")));


/* Release a reference for the client with the ID IDX in class
   CLASS.  */
void _hurd_cap_client_release (hurd_cap_bucket_t bucket,
			       _hurd_cap_client_id_t idx)
     __attribute__((visibility("hidden")));


/* Inhibit all RPCs on the capability client CLIENT (which must not be
   locked) in the bucket BUCKET.  You _must_ follow up with a
   hurd_cap_client_resume operation, and hold at least one reference
   to the object continuously until you did so.  */
error_t _hurd_cap_client_inhibit (hurd_cap_bucket_t bucket,
				  _hurd_cap_client_t client)
     __attribute__((visibility("hidden")));


/* Resume RPCs on the capability client CLIENT in the bucket BUCKET
   and wake-up all waiters.  */
void _hurd_cap_client_resume (hurd_cap_bucket_t bucket,
			      _hurd_cap_client_t client)
     __attribute__((visibility("hidden")));


/* End RPCs on the capability client CLIENT in the bucket BUCKET and
   wake-up all waiters.  */
void _hurd_cap_client_end (hurd_cap_bucket_t bucket,
			   _hurd_cap_client_t client)
     __attribute__((visibility("hidden")));


/* The following data type is used as an entry in the client table of
   a bucket.  Its members are protected by the same lock as the
   table.  */
struct _hurd_cap_client_entry
{
  /* This pointer has to be the first element of the struct.  */
  _hurd_cap_client_t client;

  /* A flag that indicates if this capability client is dead.  CLIENT
     is valid until all references have been removed, though.  Note
     that because there is one reference for the task info capability,
     this means that CLIENT is valid until a task death notification
     has been processed for this client.  */
  unsigned int dead : 1;

  /* Reference counter.  There is one reference for the fact that the
     child lives, and one reference for each pending RPC.
     Temporarily, there can be additional references for RPC that have
     just yet started, but they will rip themselves when they see the
     DEAD flag.  */
  unsigned int refs : 31;
};
typedef struct _hurd_cap_client_entry *_hurd_cap_client_entry_t;


/* Buckets are a set of capabilities, on which RPCs are managed
   collectively.  */

struct _hurd_cap_bucket
{
  /* Client management.  */

  /* The following condition is used in conjunction with the state
     predicate of the client associated with the the currently
     processed death task notification.  */
  pthread_cond_t client_cond;

  /* The following mutex is associated with the client_cond condition.
     Note that this mutex does _not_ protect the predicate of the
     condition: The predicate is the state of the respective client
     and that is protected by the lock of each client itself.  In
     fact, this mutex has no purpose but to serve the condition.  The
     reason is that this way we avoid lock contention when checking
     the state of a client.

     So, first you lock the client structure.  You have to do this
     anyway.  Then you check the state.  If the state is
     _HURD_CAP_STATE_GREEN, you can unlock the client and continue
     normally.  However, if the state is _HURD_CAP_STATE_YELLOW, you
     have to unlock the client, lock this mutex, then lock the client
     again and reinvestigate the state.  If necessary (ie, you are the
     last RPC except the waiter) you can set the state to
     _HURD_CAP_STATE_RED and broadcast the condition.  This sounds
     cumbersome, but the important part is that the common case, the
     _HURD_CAP_STATE_GREEN, is handled quickly and without class-wide
     lock contention.  */
  pthread_mutex_t client_cond_lock;

  /* The following entry is protected by hurd_task_death_notify_lock.  */
  struct hurd_task_death_notify_list_item client_death_notify;


  /* Bucket management.  */

  /* The following members are protected by this lock.  */
  pthread_mutex_t lock;

  /* The manager thread for this capability class.  */
  pthread_t manager;

  /* True if MANAGER is valid and the bucket is managed.  */
  bool is_managed;

  /* If this is true, then the manager is waiting for the free worker
     list to become empty (at shutdown) or filled (else).  The first
     worker thread to notice that the condition is fulfilled now
     should broadcast the condition.  */
  bool is_manager_waiting;

  /* The state of the bucket.  */
  _hurd_cap_state_t state;

  /* The condition used to wait on state changes and changes in the
     worker thread list.  */
  pthread_cond_t cond;

  /* The thread waiting for the RPCs to be inhibited.  */
  pthread_t cond_waiter;

  /* The number of capabilities.  If this is not 0, then there are
     active users.  */
  unsigned int nr_caps;

  /* The pending RPCs in this bucket.  */
  _hurd_cap_list_item_t pending_rpcs;

  /* The waiting RPCs in this bucket.  */
  _hurd_cap_list_item_t waiting_rpcs;

  /* The free worker threads in this bucket.  */
  _hurd_cap_list_item_t free_worker;

  /* A hash from l4_thread_id_t to _hurd_cap_list_item_t (the list
     items in PENDING_RPCs).  This is used to limit each client thread
     to just one RPC at one time.  */
  struct hurd_ihash senders;

  /* Mapping from hurd_cap_client_id_t to _hurd_cap_client_entry_t.  */
  struct hurd_table clients;

  /* Reverse lookup from hurd_task_id_t to _hurd_cap_client_t.  */
  struct hurd_ihash clients_reverse;

  /* This is true if worker threads should be allocated
     asynchronously.  */
  bool is_worker_alloc_async;

  /* If WORKER_ALLOC_ASYNC is true, this is the state of the worker
     thread allocation thread.  If this is _HURD_CAP_STATE_GREEN, then
     a new thread should be allocated.  If this is
     _HURD_CAP_STATE_YELLOW, the worker thread has allocated a new
     thread, and is currently waiting for the thread to complete its
     startup.  If this is _HURD_CAP_STATE_RED, the new worker thread
     has completed its startup (if one was started) and no new thread
     will be allocated.  */
  _hurd_cap_state_t worker_alloc_state;

  /* If WORKER_ALLOC_ASYNC is true, this is the allocator thread.  */
  pthread_t worker_alloc;
};


/* Return true if there are still outstanding RPCs in this bucket
   BUCKET, and fails if not.  This is only valid if
   hurd_cap_bucket_inhibit is in progress (ie, if bucket->state is
   _HURD_CAP_STATE_YELLOW).  BUCKET must be locked.  */
static inline int
__attribute__((always_inline))
_hurd_cap_bucket_cond_busy (hurd_cap_bucket_t bucket)
{
  /* We have to remain in the state yellow until there are no pending
     RPC threads except maybe the waiter.  */
  return bucket->pending_rpcs
    && (bucket->pending_rpcs->thread != bucket->cond_waiter
	|| bucket->pending_rpcs->next);
}


/* Check if the inhibition state of the capability bucket BUCKET has
   to be changed.  BUCKET must be locked.  */
static inline void
__attribute__((always_inline))
_hurd_cap_bucket_cond_check (hurd_cap_bucket_t bucket)
{
  if (bucket->state == _HURD_CAP_STATE_YELLOW
      && !_hurd_cap_bucket_cond_busy (bucket))
    {
      bucket->state =_HURD_CAP_STATE_RED;
      pthread_cond_broadcast (&bucket->cond);
    }
}


/* Capability clients.  */

/* Return true if there are still outstanding RPCs in this capability
   client, and fails if not.  CLIENT must be locked.  This is only
   valid if hurd_cap_client_inhibit is in progress (ie, if
   client->state is _HURD_CAP_STATE_YELLOW).  */
static inline int
__attribute__((always_inline))
_hurd_cap_client_cond_busy (_hurd_cap_client_t client)
{
  /* We have to remain in the state yellow until there are no pending
     RPC threads except maybe the waiter.  */
  return client->pending_rpcs
    && (client->pending_rpcs->thread != client->cond_waiter
	|| client->pending_rpcs->next);
}


/* Check if the inhibited state of the capability client CLIENT has to
   be changed.  CLIENT must be locked.  */
static inline void
__attribute__((always_inline))
_hurd_cap_client_cond_check (hurd_cap_bucket_t bucket,
			     _hurd_cap_client_t client)
{
  if (client->state == _HURD_CAP_STATE_YELLOW
      && !_hurd_cap_client_cond_busy (client))
    {
      client->state = _HURD_CAP_STATE_RED;
      pthread_cond_broadcast (&bucket->client_cond);
    }
}


/* Capability classes.  */

/* Return true if there are still outstanding RPCs in this class, and
   fails if not.  CAP_CLASS must be locked.  This is only valid if
   hurd_cap_class_inhibit is in progress (ie, if cap_class->state is
   _HURD_CAP_STATE_YELLOW).  */
static inline int
__attribute__((always_inline))
_hurd_cap_class_cond_busy (hurd_cap_class_t cap_class)
{
  /* We have to remain in the state yellow until there are no pending
     RPC threads except maybe the waiter.  */
  return cap_class->pending_rpcs
    && (cap_class->pending_rpcs->thread != cap_class->cond_waiter
	|| cap_class->pending_rpcs->next);
}


/* Check if the inhibition state of the capability class CAP_CLASS has
   to be changed.  CAP_CLASS must be locked.  */
static inline void
__attribute__((always_inline))
_hurd_cap_class_cond_check (hurd_cap_class_t cap_class)
{
  if (cap_class->state == _HURD_CAP_STATE_YELLOW
      && !_hurd_cap_class_cond_busy (cap_class))
    {
      cap_class->state = _HURD_CAP_STATE_RED;
      pthread_cond_broadcast (&cap_class->cond);
    }
}


/* Capability objects.  */

/* Return true if there are still outstanding RPCs in this capability
   object, and fails if not.  OBJ must be locked.  This is only valid
   if hurd_cap_obj_inhibit is in progress (ie, if cap_obj->state is
   _HURD_CAP_STATE_YELLOW).  */
static inline int
__attribute__((always_inline))
_hurd_cap_obj_cond_busy (hurd_cap_obj_t obj)
{
  /* We have to remain in the state yellow until there are no pending
     RPC threads except maybe the waiter.  */
  return obj->pending_rpcs
    && (obj->pending_rpcs->thread != obj->cond_waiter
	|| obj->pending_rpcs->next);
}


/* Check if the inhibition state of the capability class CAP_CLASS has
   to be changed.  CAP_CLASS must be locked.  */
static inline void
__attribute__((always_inline))
_hurd_cap_obj_cond_check (hurd_cap_obj_t obj)
{
  if (obj->state == _HURD_CAP_STATE_YELLOW
      && !_hurd_cap_obj_cond_busy (obj))
    {
      obj->state = _HURD_CAP_STATE_RED;
      pthread_cond_broadcast (&obj->cap_class->cond);
    }
}


#endif	/* _HURD_CAP_SERVER_INTERN_H */
