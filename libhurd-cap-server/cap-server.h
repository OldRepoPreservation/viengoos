/* cap-server.h - Server interface to the Hurd capability library.
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

#ifndef _HURD_CAP_SERVER_H
#define _HURD_CAP_SERVER_H	1

#include <stdbool.h>
#include <errno.h>
#include <pthread.h>

/* FIXME: This is not a public header file!  So we may have to ship
   a <hurd/atomic.h>.  */
#include <atomic.h>

#include <hurd/slab.h>
#include <hurd/types.h>


/* Internal declarations.  */

/* This is a simple list item, used to maintain lists of pending RPC
   worker threads in a capability class, client, or object.  */
struct _hurd_cap_list_item;
typedef struct _hurd_cap_list_item *_hurd_cap_list_item_t;


/* The state of a capability class, client, or object.  */
typedef enum _hurd_cap_state
  {
    _HURD_CAP_STATE_GREEN,
    _HURD_CAP_STATE_YELLOW,
    _HURD_CAP_STATE_RED,
    _HURD_CAP_STATE_BLACK
  }
_hurd_cap_state_t;


/* Public interface.  */

/* Forward declarations.  */
struct _hurd_cap_bucket;
typedef struct _hurd_cap_bucket *hurd_cap_bucket_t;
struct _hurd_cap_client;
typedef struct _hurd_cap_client *_hurd_cap_client_t;
struct hurd_cap_class;
typedef struct hurd_cap_class *hurd_cap_class_t;
struct hurd_cap_obj;
typedef struct hurd_cap_obj *hurd_cap_obj_t;


typedef error_t (*hurd_cap_obj_init_t) (hurd_cap_class_t cap_class,
					hurd_cap_obj_t obj);
typedef error_t (*hurd_cap_obj_alloc_t) (hurd_cap_class_t cap_class,
					 hurd_cap_obj_t obj);
typedef void (*hurd_cap_obj_reinit_t) (hurd_cap_class_t cap_class,
				       hurd_cap_obj_t obj);
typedef void (*hurd_cap_obj_destroy_t) (hurd_cap_class_t cap_class,
					hurd_cap_obj_t obj);


/* The RPC context contains various information for the RPC handler
   and the support functions.  */
struct hurd_cap_rpc_context
{
  /* Public members.  */

  /* The task which contained the sender of the message.  */
  hurd_task_id_t sender;

  /* The bucket through which the message was received.  */
  hurd_cap_bucket_t bucket;

  /* The capability object on which the RPC was invoked.  */
  hurd_cap_obj_t obj;


  /* Private members.  */

  /* The sender of the message.  */
  l4_thread_id_t from;

  /* The client corresponding to FROM.  */
  _hurd_cap_client_t client;

  /* The message.  */
  l4_msg_t msg;
};
typedef struct hurd_cap_rpc_context *hurd_cap_rpc_context_t;

/* FIXME: Add documentation.  */
typedef error_t (*hurd_cap_class_demuxer_t) (hurd_cap_rpc_context_t ctx);


/* A capability class is a group of capability objects of the same
   type.  */
struct hurd_cap_class
{
  /* Capability object management for the class.  */

  /* The following callbacks are used to adjust the state of an object
     during its lifetime:

     1. Object is constructed in the cache			OBJ_INIT
     2.1. Object is instantiated and removed from the free list	OBJ_ALLOC
     2.2. Object is deallocated and put back on the free list	OBJ_REINIT
     3. Object is destroyed and removed from the cache		OBJ_DESTROY

     Note that step 2 can occur several times, or not at all.
     This is the state diagram for each object:

         (START) --(1.)-> initialized --(3.)--> destroyed (END)
			    |     ^
			    |     |
                         (2.1.)  (2.2.)
			    |     |
			    v     |
		           allocated

     Note that OBJ_INIT will be called in bursts for pre-allocation of
     several objects.  */

  /* This callback is invoked whenever a new object is pre-allocated
     in the cache.  It is usually called in bursts when a new slab
     page is allocated.  You can put all initialization in it that
     should be cached.  */
  hurd_cap_obj_init_t obj_init;

  /* This callback is called whenever an object in the cache is going
     to be instantiated and used.  You can put further initialization
     in it that is not suitable for caching (for example, because it
     can not be safely reinitialized by OBJ_REINIT).  If OBJ_ALLOC
     fails, then it must leave the object in its initialized
     state!  */
  hurd_cap_obj_alloc_t obj_alloc;

  /* This callback is invoked whenever a used object is deallocated
     and returned to the cache.  It should revert the used object to
     its initialized state, this means as if OBJ_INIT had been called
     on a freshly constructed object.  This also means that you have
     to deallocate all resources that have been allocated by
     OBJ_ALLOC.  Note that this function can not fail.  Initialization
     that can not be safely (error-free) reverted to its original
     state must be put into the OBJ_ALLOC callback, rather than in the
     OBJ_INIT callback.  */
  hurd_cap_obj_reinit_t obj_reinit;

  /* This callback is invoked whenever an initialized, but unused
     object is removed from the cache and destroyed.  You should
     release all resources that have been allocated for this object by
     a previous OBJ_INIT invocation.  */
  hurd_cap_obj_destroy_t obj_destroy;

  /* The slab space containing the capabilities in this class.  */
  struct hurd_slab_space obj_space;

  /* The following condition is used in conjunction with the state
     predicate of a capability object.  */
  pthread_cond_t obj_cond;

  /* The following mutex is associated with the obj_cond
     condition.  Note that this mutex does _not_ protect the predicate
     of the condition: The predicate is the state of the respective
     client and that is protected by the lock of each capability
     object itself.  */
  pthread_mutex_t obj_cond_lock;


  /* The class management.  */

  /* The demuxer for this class.  */
  hurd_cap_class_demuxer_t demuxer;

  /* The lock protecting all the following members.  */
  pthread_mutex_t lock;

  /* The state of the class.  */
  _hurd_cap_state_t state;

  /* The condition used for waiting on state changes.  The associated
     mutex is LOCK.  */
  pthread_cond_t cond;

  /* The current waiter thread.  This is only valid if state is
     _HURD_CAP_STATE_YELLOW.  Used by _hurd_cap_class_cond_busy ().  */
  pthread_t cond_waiter;

  /* The pending RPC worker threads for this class.  */
  _hurd_cap_list_item_t pending_rpcs;
};


/* Server-side objects that are accessible via capabilities.  */
struct hurd_cap_obj
{
  /* The class which contains this capability.  */
  hurd_cap_class_t cap_class;

  /* The lock protecting all the members of the capability object.  */
  pthread_mutex_t lock;

  /* The reference counter for this object.  */
  uatomic32_t refs;

  /* The state of this object.  If this is _HURD_CAP_STATE_GREEN, you
     can use the capability object.  Otherwise, you should refrain
     from using it.  You can sleep on the obj_state_cond condition in
     the capability class until the state goes back to
     _HURD_CAP_STATE_GREEN again.

     If the state is _HURD_CAP_STATE_YELLOW, this means that there is
     some thread who wants the state to be _HURD_CAP_STATE_RED, and
     this thread canceled all other pending RPC threads on this
     object.  If you are the last worker thread for this capability
     object (except for the thread waiting for the condition to become
     _HURD_CAP_STATE_RED), you have to broadcast the obj_state_cond
     condition.

     Every worker thread that blocks on the capability object state
     until it reverts to _HURD_CAP_STATE_GREEN must perform a
     reauthentication when it is unblocked (ie, verify that the client
     still has access to the capability object), in case the
     capability of the client for this object was revoked in the
     meantime.

     _HURD_CAP_STATE_BLACK is not used for capability objects.  */
  _hurd_cap_state_t state;

  /* The pending RPC worker threads for this capability object.  */
  _hurd_cap_list_item_t pending_rpcs;

  /* The current waiter thread.  This is only valid if STATE is
     _HURD_CAP_STATE_YELLOW.  Used by _hurd_cap_obj_cond_busy ().  */
  pthread_t cond_waiter;

  /* The list items in the capability entries of the clients using
     this capability.  */
  _hurd_cap_list_item_t clients;
};


/* Operations on capability classes.  */

/* Create a new capability class for objects with the size SIZE,
   including the struct hurd_cap_obj, which has to be placed at the
   beginning of each capability object.

   The callback OBJ_INIT is used whenever a capability object in this
   class is created.  The callback OBJ_REINIT is used whenever a
   capability object in this class is deallocated and returned to the
   slab.  OBJ_REINIT should bring back a capability object that is not
   used anymore into the same state as OBJ_INIT does for a freshly
   allocated object.  OBJ_DESTROY should deallocate all resources for
   this capablity object.  Note that OBJ_REINIT can not fail: If you
   have resources that can not safely be restored into their initial
   state, you cannot use OBJ_INIT to allocate them.  Furthermore, note
   that OBJ_INIT will usually be called in bursts for advanced
   allocation.

   The new capability class is returned in R_CLASS.  If the creation
   fails, an error value will be returned.  */
error_t hurd_cap_class_create (size_t size, size_t alignment,
			       hurd_cap_obj_init_t obj_init,
			       hurd_cap_obj_alloc_t obj_alloc,
			       hurd_cap_obj_reinit_t obj_reinit,
			       hurd_cap_obj_destroy_t obj_destroy,
			       hurd_cap_class_demuxer_t demuxer,
			       hurd_cap_class_t *r_class);


/* Destroy the capability class CAP_CLASS and release all associated
   resources.  Note that this is only allowed if there are no
   capability objects in use, and if the capability class is not used
   by a capability server.  This function assumes that the class was
   created with hurd_cap_class_create.  */
error_t hurd_cap_class_free (hurd_cap_class_t cap_class);


/* Same as hurd_cap_class_create, but doesn't allocate the storage for
   CAP_CLASS.  Instead, you have to provide it.  */
error_t hurd_cap_class_init (hurd_cap_class_t cap_class,
			     size_t size, size_t alignment,
			     hurd_cap_obj_init_t obj_init,
			     hurd_cap_obj_alloc_t obj_alloc,
			     hurd_cap_obj_reinit_t obj_reinit,
			     hurd_cap_obj_destroy_t obj_destroy,
			     hurd_cap_class_demuxer_t demuxer);


/* Destroy the capability class CAP_CLASS and release all associated
   resources.  Note that this is only allowed if there are no
   capability objects in use, and if the capability class is not used
   by a capability server.  This function assumes that the class has
   been initialized with hurd_cap_class_init.  */
error_t hurd_cap_class_destroy (hurd_cap_class_t cap_class);


/* Allocate a new capability object in the class CAP_CLASS.  The new
   capability object is locked and has one reference.  It will be
   returned in R_OBJ.  If the allocation fails, an error value will be
   returned.  The object will be destroyed as soon as its last
   reference is dropped.  */
error_t hurd_cap_class_alloc (hurd_cap_class_t cap_class,
			      hurd_cap_obj_t *r_obj);


/* Inhibit all RPCs on the capability class CAP_CLASS (which must not
   be locked).  You _must_ follow up with a hurd_cap_class_resume
   operation, and hold at least one reference to the object
   continuously until you did so.  */
error_t hurd_cap_class_inhibit (hurd_cap_class_t cap_class);


/* Resume RPCs on the class CAP_CLASS and wake-up all waiters.  */
void hurd_cap_class_resume (hurd_cap_class_t cap_class);


/* Operations on capability objects.  */

/* Lock the object OBJ, which must be locked.  */
static inline void
hurd_cap_obj_lock (hurd_cap_obj_t obj)
{
  pthread_mutex_lock (&obj->lock);
}


/* Unlock the object OBJ, which must be locked.  */
static inline void
hurd_cap_obj_unlock (hurd_cap_obj_t obj)
{
  pthread_mutex_unlock (&obj->lock);
}


/* Add a reference for the capability object OBJ.  */
static inline void
hurd_cap_obj_ref (hurd_cap_obj_t obj)
{
  atomic_increment (&obj->refs);
}


/* Remove one reference for the capability object OBJ.  Note that the
   caller must have at least two references for this capability object
   when using this function.  To release the last reference,
   hurd_cap_obj_drop must be used instead.  */
static inline void
hurd_cap_obj_rele (hurd_cap_obj_t obj)
{
  atomic_decrement (&obj->refs);
}


/* Remove one reference for the capability object OBJ, which must be
   locked, and will be unlocked when the function returns.  If this
   was the last user of this object, the object is deallocated.  */
void hurd_cap_obj_drop (hurd_cap_obj_t obj);


/* Inhibit all RPCs on the capability object OBJ (which must not be
   locked).  You _must_ follow up with a hurd_cap_obj_resume
   operation, and hold at least one reference to the object
   continuously until you did so.  */
error_t hurd_cap_obj_inhibit (hurd_cap_obj_t obj);


/* Resume RPCs on the capability object OBJ and wake-up all
   waiters.  */
void hurd_cap_obj_resume (hurd_cap_obj_t obj);


/* Buckets are a set of capabilities, on which RPCs are managed
   collectively.  */

/* Create a new bucket and return it in R_BUCKET.  */
error_t hurd_cap_bucket_create (hurd_cap_bucket_t *r_bucket);


/* Free the bucket BUCKET, which must not be used.  */
void hurd_cap_bucket_free (hurd_cap_bucket_t bucket);


/* Copy out a capability for the capability OBJ to the client with the
   task ID TASK_ID.  Returns the capability (valid only for this user)
   in *R_CAP, or an error.  It is not safe to call this from outside
   an RPC on OBJ while the manager is running.  */
error_t hurd_cap_bucket_inject (hurd_cap_bucket_t bucket, hurd_cap_obj_t obj,
				hurd_task_id_t task_id,
				hurd_cap_handle_t *r_cap);


/* If ASYNC is true, allocate worker threads asynchronously whenever
   the number of worker threads is exhausted.  This is only actually
   required for physmem (the physical memory server), to allow to
   break out of a dead-lock between physmem and the task server.  It
   should be unnecessary for any other server.

   The default is to false, which means that worker threads are
   allocated synchronously by the manager thread.

   This function should be called before the manager is started with
   hurd_cap_bucket_manage_mt.  It is only used for the multi-threaded
   RPC manager.  */
error_t hurd_cap_bucket_worker_alloc (hurd_cap_bucket_t bucket, bool async);


/* Start managing RPCs on the bucket BUCKET.  The ROOT capability
   object, which must be unlocked and have one reference throughout
   the whole time this function runs, is used for bootstrapping client
   connections.  The GLOBAL_TIMEOUT parameter specifies the number of
   seconds until the manager times out (if there are no active users
   of capability objects in precious classes).  The WORKER_TIMEOUT
   parameter specifies the number of seconds until each worker thread
   times out (if there are no RPCs processed by the worker thread).

   If this returns ECANCELED, then hurd_cap_bucket_end was called with
   the force flag being true while there were still active users.  If
   this returns without any error, then the timeout expired, or
   hurd_cap_bucket_end was called without active users of capability
   objects in precious classes.  */
error_t hurd_cap_bucket_manage_mt (hurd_cap_bucket_t bucket,
				   hurd_cap_obj_t root,
				   unsigned int global_timeout,
				   unsigned int worker_timeout);


/* Inhibit all RPCs on the capability bucket BUCKET (which must not be
   locked).  You _must_ follow up with a hurd_cap_bucket_resume (or
   hurd_cap_bucket_end) operation.  */
error_t hurd_cap_bucket_inhibit (hurd_cap_bucket_t bucket);


/* Resume RPCs on the class CAP_CLASS and wake-up all waiters.  */
void hurd_cap_bucket_resume (hurd_cap_bucket_t bucket);


/* Exit from the server loop of the managed capability bucket BUCKET.
   This will only succeed if there are no active users, or if the
   FORCE flag is set (otherwise it will fail with EBUSY).  The bucket
   must be inhibited.  */
error_t hurd_cap_bucket_end (hurd_cap_bucket_t bucket, bool force);


#endif	/* _HURD_CAP_SERVER_H */
