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

#include <errno.h>
#include <assert.h>
#include <pthread.h>
#include <limits.h>

#include <hurd/types.h>
#include <hurd/slab.h>
#include <hurd/table.h>
#include <hurd/ihash.h>
#include <hurd/task-death.h>


/* FIXME: Cancellation is not implemented yet.  */
#define hurd_cond_wait pthread_cond_wait


/* Forward declaration.  */
struct _hurd_cap_list_item;

/* This is a simple list item, used to maintain lists of pending RPC
   worker threads in a class, client or capability object.  */
typedef struct _hurd_cap_list_item *_hurd_cap_list_item_t;
struct _hurd_cap_list_item
{
  _hurd_cap_list_item_t next;
  _hurd_cap_list_item_t *prevp;

  /* This location pointer is used for fast removal from the
     CAP_CLASS->client_thread hash.  */
  hurd_ihash_locp_t locp;

  /* The worker thread processing the RPC.  */
  pthread_t thread;
};


/* Add the list item ITEM to the list LIST.  */
static inline void
__attribute__((always_inline))
_hurd_cap_list_item_add (_hurd_cap_list_item_t *list,
			 _hurd_cap_list_item_t item)
{
  if (list)
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
}


/* The state of a class, client, or capability object can be green,
   yellow, or red.  Green means that RPCs involving this object can be
   processed normally.  Yellow means that somebody is waiting for all
   RPCs on this object to be inhibited.  Red means that RPCs are
   inhibited, and no RPCs involving this object should be processed.
   Note that the exact semantics differ, depending on the type of the
   object concerned.  Here is a summary:

   The capability class.  This is the most useful object to inhibit
   RPCs on.  It stops all generic RPCs on this class (FIXME: maybe:
   the only RPC allowed is the interrupt RPC, which allows you to
   interrupt an RPC inhibiting operation).  This is needed for
   remounting a filesystem as read-only, for example.

   The clients.  This is used internally to inhibit all pending RPCs
   from dead clients.  Not for external use.  In this case, yellow or
   red means that the client is already dead.  Any (in this case
   spurious) incoming RPCs will be dropped rather than blocked.

   The capability objects.  All RPCs on a specific capability object
   can be inhibited in order to revoke access rights to an object.  It
   is ensured that any blocked incoming RPC will revalidate its
   permission to use the object when going from red to green status
   (it also needs to check the state of the capability class, in case
   it is in yellow or red state by that time).

   All status fields are protected by the locks of the object they are
   contained in.  However, there is also a lock and a condition in the
   class for each object type (for the class, the two locks are the
   same).  These lock and conditions are used to wait on status
   changes, but you still need to take the object lock to check the
   status.  This allows to check the status without taking a
   class-wide lock, while the class-wide lock is only required when
   blocking because of waiting for a status change.

   For capability clients, one extra state, _HURD_CAP_STATE_BLACK, is
   defined.  This state is used to stop processing RPCs.  This can
   only be used for dead client tasks.  */
enum _hurd_cap_state
  {
    _HURD_CAP_STATE_GREEN,
    _HURD_CAP_STATE_YELLOW,
    _HURD_CAP_STATE_RED,
    _HURD_CAP_STATE_BLACK
  };


/* Server-side objects that are accessible via capabilities.  Note
   that a capability object is always embedded in a slab for a class
   of capabilities.  Storage, constructors and destructors are
   provided by the slab.  */
struct hurd_cap_obj
{
  /* The lock protecting all the members of the capability object.  */
  pthread_mutex_t lock;

  /* The reference count of this object.  */
  unsigned int refs;

  /* The state of this object.  If this is _HURD_CAP_STATE_GREEN, you
     can use the capability object.  Otherwise, you should refrain
     from using it.  You can sleep on the cap_obj_state_cond condition
     until the state goes back to _HURD_CAP_STATE_GREEN again.

     If the state is _HURD_CAP_STATE_YELLOW, this means that there is
     some thread who wants the state to be _HURD_CAP_STATE_RED, and
     this thread canceled all other pending RPC threads on this
     object.  If you are the last worker thread for this capability
     object (except for the thread waiting for the condition to become
     red), you have to broadcast the cap_obj_state_cond.

     Any worker thread that blocks on the cap object state until it
     reverts to _HURD_CAP_STATE_GREEN must perform a reauthentication
     when it is unblocked (ie, verify that the client still has access
     to the capability object).  */
  enum _hurd_cap_state state;

  /* The pending RPC worker threads for this capability object.  */
  _hurd_cap_list_item_t pending_rpcs;

  /* The current waiter thread.  This is only valid if state is
     _HURD_CAP_STATE_YELLOW.  Used by _hurd_cap_obj_cond_busy ().  */
  pthread_t cond_waiter;

  /* The mapping of hurd_cap_client_t to hurd_cap_id_t for all clients
     which hold a capability to this object.  */
  struct hurd_ihash clients;

  /* The object data.  */
  char data[0];
};
typedef struct hurd_cap_obj *hurd_cap_obj_t;


/* Client capabilities.  */

/* The following data type is used as an entry in the cap table of a
   client.  Its members are protected by the same lock as the
   client.  */
struct _hurd_cap_entry
{
  /* This pointer has to be the first element of the struct.  If this
     is HURD_CAP_ENTRY_DEAD, the capability was revoked.  Otherwise,
     there exists one reference for the capability object.  */
  hurd_cap_obj_t cap_obj;

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
typedef struct _hurd_cap_entry *_hurd_cap_entry_t;


/* Client connections. */

/* This data type holds all the information about a client
   connection.  */
struct hurd_cap_client
{
  /* The task ID of the client.  */
  hurd_task_id_t task_id;

  /* The index of the client in the client table of the cap class.
     This is here so that we can hash for the address of this struct
     in the clients_reverse hash of the cap class, and still get the
     index number.  This allows us to use a location pointer for
     removal (locp) for fast hash removal.  */
  hurd_cap_client_id_t id;

  /* The location pointer for fast removal from the reverse lookup
     hash CAP_CLASS->clients_reverse.  This is protected by the class
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
     client_cond of the class.  */
  enum _hurd_cap_state state;

  /* The current waiter thread.  This is only valid if state is
     _HURD_CAP_STATE_YELLOW.  Used by _hurd_cap_client_cond_busy ().  */
  pthread_t cond_waiter;

  /* The pending RPC list.  Each RPC worker thread should add itself
     to this list, so it can be cancelled by the task death
     notification handler.  */
  struct _hurd_cap_list_item *pending_rpcs;

  /* The hurd_cap_id_t to hurd_cap_obj_t mapping.  */
  struct hurd_table caps;

  /* Reverse lookup from hurd_cap_obj_t to hurd_cap_id_t.  */
  struct hurd_ihash caps_reverse;
};
typedef struct hurd_cap_client *hurd_cap_client_t;


/* The following data type is used as an entry in the client table of
   a capability class.  Its members are protected by the same lock as
   the table.  */
struct _hurd_cap_client_entry
{
  /* This pointer has to be the first element of the struct.  */
  hurd_cap_client_t client;

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


/* Forward declaration.  */
struct hurd_cap_class;
typedef struct hurd_cap_class *hurd_cap_class_t;

typedef error_t (*hurd_cap_obj_init_t) (hurd_cap_class_t cap_class,
					hurd_cap_obj_t obj);
typedef error_t (*hurd_cap_obj_alloc_t) (hurd_cap_class_t cap_class,
					 hurd_cap_obj_t obj);
typedef void (*hurd_cap_obj_reinit_t) (hurd_cap_class_t cap_class,
				       hurd_cap_obj_t obj);
typedef void (*hurd_cap_obj_destroy_t) (hurd_cap_class_t cap_class,
					hurd_cap_obj_t obj);

/* A capability class is a group of capability objects of the same
   type, plus all the infrastructure that is needed to grant
   capabilities to users and process the user RPCs on the capability
   objects.  */
struct hurd_cap_class
{
  /* Capability object management for the class.  */

  /* The size of the space (in bytes) following each capability object
     in the slab for arbitrary object data.  */
  size_t obj_size;

  /* The following callbacks are used to adjust the state of an object
     during its lifetime:

     1. Object is constructed in the cache			OBJ_INIT
     2.1. Object is instantiated and removed from the free list	OBJ_ALLOC
     2.2. Object is deallocated and put back on the free list	OBJ_REINIT
     3. Object is destroyed and removed from the cache		OBJ_DESTROY

     Note that step 2 can occur several times, or not at all.
     This is the state diagram for each object:

	  (1.) START == OBJ_INIT --(3.)--> OBJ_DESTROY == END
			  |		       ^
			  |		       |
			(2.1.)		      (3.)
			  |		       |
			  v		       |
		       OBJ_ALLOC -(2.2.)--> OBJ_REINIT
			  ^		       |
			  |		       |
			  +-------(2.1.)-------+

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
  hurd_slab_space_t obj_slab;

  /* The following condition is used in conjunction with the state
     predicate of a capability object.  */
  pthread_cond_t obj_cond;

  /* The following mutex is associated with the obj_cond
     condition.  Note that this mutex does _not_ protect the predicate
     of the condition: The predicate is the state of the respective
     client and that is protected by the lock of each capability
     object itself.  */
  pthread_mutex_t obj_cond_lock;


  /* Client management for the class.  */

  /* The slab space containing the clients in this class.  */
  hurd_slab_space_t client_slab;

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


  /* The class management.  */
  
  /* The lock protecting all the following members.  */
  pthread_mutex_t lock;

  /* The mapping of client task IDs to hurd_cap_client_t.  */
  struct hurd_table clients;

  /* Reverse lookup from hurd_task_id_t to hurd_cap_client_id_t.  */
  struct hurd_ihash clients_reverse;

  /* The state of the class.  */
  enum _hurd_cap_state state;

  /* The condition used for waiting on state changes.  The associated
     mutex is LOCK.  */
  pthread_cond_t cond;

  /* The current waiter thread.  This is only valid if state is
     _HURD_CAP_STATE_YELLOW.  Used by _hurd_cap_class_cond_busy ().  */
  pthread_t cond_waiter;

  /* The pending RPC worker threads for this class.  */
  _hurd_cap_list_item_t pending_rpcs;

  /* This maps client thread IDs to their entry in the pending_rpcs
     list.  This is used to prevent DoS attackes: Every client thread
     is only allowed to make one RPC at the same time, so that it can
     occupy at most one worker thread at a time.  */
  struct hurd_ihash client_threads;
};



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
error_t hurd_cap_class_create (size_t size, hurd_cap_obj_init_t obj_init,
			       hurd_cap_obj_alloc_t obj_alloc,
			       hurd_cap_obj_reinit_t obj_reinit,
			       hurd_cap_obj_destroy_t obj_destroy,
			       hurd_cap_class_t *r_class);

/* Same as hurd_cap_class_create, but doesn't allocate the storage for
   CAP_CLASS.  Instead, you have to provide it.  */
error_t hurd_cap_class_init (hurd_cap_class_t cap_class,
			     size_t size, hurd_cap_obj_init_t obj_init,
			     hurd_cap_obj_alloc_t obj_alloc,
			     hurd_cap_obj_reinit_t obj_reinit,
			     hurd_cap_obj_destroy_t obj_destroy);


/* Destroy the capability class CAP_CLASS and release all associated
   resources.  Note that this is only allowed if there are no
   capability objects in use, and if the capability class is not used
   by anything else (ie, all RPCs must be inhibited, no manager thread
   must run.  */
error_t _hurd_cap_class_destroy (hurd_cap_class_t cap_class);


/* Allocate a new capability object in the class CAP_CLASS.  The new
   capability object is locked and has one reference.  It will be
   returned in R_OBJ.  If the allocation fails, an error value will be
   returned.  */
error_t hurd_cap_class_alloc (hurd_cap_class_t cap_class,
			      hurd_cap_obj_t *r_obj);


/* Deallocate the capability object OBJ in the class CAP_CLASS.  OBJ
   must be locked and have no more references.  */
void _hurd_cap_class_dealloc (hurd_cap_class_t cap_class, hurd_cap_obj_t obj)
     __attribute__((visibility("hidden")));


/* Lock the object OBJ in class CAP_CLASS.  OBJ must be locked.  */
static inline void
hurd_cap_obj_lock (hurd_cap_class_t cap_class, hurd_cap_obj_t obj)
{
  pthread_mutex_lock (&obj->lock);
}


/* Unlock the object OBJ in class CAP_CLASS.  OBJ must be locked.  */
static inline void
hurd_cap_obj_unlock (hurd_cap_class_t cap_class, hurd_cap_obj_t obj)
{
  pthread_mutex_unlock (&obj->lock);
}


/* Add a reference for the capability object OBJ in class CAP_CLASS.
   OBJ must be locked.  */
static inline void
hurd_cap_obj_ref (hurd_cap_class_t cap_class, hurd_cap_obj_t obj)
{
  assert (obj->refs < UINT_MAX);

  obj->refs++;
}


/* Remove one reference for the capability object OBJ in class
   CAP_CLASS.  OBJ must be locked.  */
static inline void
hurd_cap_obj_rele (hurd_cap_class_t cap_class, hurd_cap_obj_t obj)
{
  assert (obj->refs > 1);

  obj->refs--;
}


/* Remove one reference for the capability object OBJ in class
   CAP_CLASS.  OBJ must be locked, and will be unlocked when the
   function returns.  If this was the last reference, the object is
   deallocated.  */
static inline void
hurd_cap_obj_drop (hurd_cap_class_t cap_class, hurd_cap_obj_t obj)
{
  if (__builtin_expect (obj->refs > 1, 1))
    {
      hurd_cap_obj_rele (cap_class, obj);
      hurd_cap_obj_unlock (cap_class, obj);
    }
  else
    _hurd_cap_class_dealloc (cap_class, obj);
}


/* Look up the client with the task ID TASK in the class CLASS, and
   return it in R_CLIENT, with one additional reference.  If it is not
   found, create it.  */
error_t _hurd_cap_client_create (hurd_cap_class_t cap_class,
				 hurd_task_id_t task,
				 hurd_cap_client_id_t *r_idx,
				 hurd_cap_client_t *r_client)
     __attribute__((visibility("hidden")));

/* Look up the client with the ID IDX in the class CLASS, and return
   it in R_CLIENT, with one additional reference.  Returns ESRCH if
   IDX is not valid.  */
static inline error_t
hurd_cap_client_lookup (hurd_cap_class_t cap_class,
			hurd_cap_client_id_t idx,
			hurd_cap_client_t *r_client)
{
  _hurd_cap_client_entry_t entry;

  pthread_mutex_lock (&cap_class->lock);
  entry = (_hurd_cap_client_entry_t) hurd_table_lookup (&cap_class->clients,
							idx);
  if (entry)
    {
      if (entry->dead)
	{
	  /* The client with this ID is dead.  For lookup purposes,
	     this is as good as if it does not exist at all.  */
	  entry = NULL;
	}
      else
	entry->refs++;
    }
  pthread_mutex_unlock (&cap_class->lock);

  if (entry)
    {
      *r_client = entry->client;
      return 0;
    }
  else
    return ESRCH;
}


/* Deallocate the connection client CLIENT.  */
void _hurd_cap_client_dealloc (hurd_cap_class_t cap_class,
			       hurd_cap_client_t client)
     __attribute__((visibility("hidden")));


/* Release a reference for the client with the ID IDX in class
   CLASS.  */
void _hurd_cap_client_release (hurd_cap_class_t cap_class,
			       hurd_cap_client_id_t idx)
     __attribute__((visibility("hidden")));


/* RPC inhibition.  */

/* Inhibit all RPCs on the capability class CAP_CLASS (which must not
   be locked).  You _must_ follow up with a hurd_cap_class_resume
   operation, and hold at least one reference to the object
   continuously until you did so.  */
error_t hurd_cap_class_inhibit (hurd_cap_class_t cap_class);

/* Resume RPCs on the class CAP_CLASS and wake-up all waiters.  */
void hurd_cap_class_resume (hurd_cap_class_t cap_class);


/* Inhibit all RPCs on the capability client CLIENT (which must not be
   locked) in the capability class CAP_CLASS.  You _must_ follow up
   with a hurd_cap_client_resume operation, and hold at least one
   reference to the object continuously until you did so.  */
error_t hurd_cap_client_inhibit (hurd_cap_class_t cap_class,
				 hurd_cap_client_t client);

/* Resume RPCs on the capability client CLIENT in the class CAP_CLASS
   and wake-up all waiters.  */
void hurd_cap_client_resume (hurd_cap_class_t cap_class,
			     hurd_cap_client_t client);

/* End RPCs on the capability client CLIENT in the class CAP_CLASS and
   wake-up all waiters.  */
void _hurd_cap_client_end (hurd_cap_class_t cap_class,
			   hurd_cap_client_t client)
     __attribute__((visibility("hidden")));

/* Inhibit all RPCs on the capability object CAP_OBJ (which must not
   be locked) in the capability class CAP_CLASS.  You _must_ follow up
   with a hurd_cap_obj_resume operation, and hold at least one
   reference to the object continuously until you did so.  */
error_t hurd_cap_obj_inhibit (hurd_cap_class_t cap_class,
			      hurd_cap_obj_t cap_obj);

/* Resume RPCs on the capability object CAP_OBJ in the class CAP_CLASS
   and wake-up all waiters.  */
void hurd_cap_obj_resume (hurd_cap_class_t cap_class, hurd_cap_obj_t cap_obj);


#endif	/* _HURD_CAP_SERVER_H */
