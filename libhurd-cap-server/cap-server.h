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

#include <errno.h>
#include <assert.h>
#include <pthread.h>
#include <limits.h>

#include <hurd/slab.h>


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

  /* FIXME: Inhibiting a single object is not implemented yet.  We
     will need a list of users for that (using doubly-linked list
     pointers in the user connection structure, maybe) and a state
     variable.  Plus a class-global condition (per-object conditions
     would be superfluous I think).  */

  /* The object data.  */
  char data[0];
};
typedef struct hurd_cap_obj *hurd_cap_obj_t;


/* Return the address of the user data associated with the capability
   object CAP_OBJ.  */
static inline void *
hurd_cap_obj_data (hurd_cap_obj_t cap_obj)
{
  return cap_obj->data;
}


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

	START ==(1.)== OBJ_INIT --(3.)--> OBJ_DESTROY = END
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
  hurd_slab_space_t slab;
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


/* Destroy the capability class CAP_CLASS and release all associated
   resources.  Note that this is only allowed if there are no
   capability objects in use.  */
error_t hurd_cap_class_destroy (hurd_cap_class_t cap_class);

/* Allocate a new capability object in the class CAP_CLASS.  The new
   capability object is locked and has one reference.  It will be
   returned in R_OBJ.  If the allocation fails, an error value will be
   returned.  */
error_t hurd_cap_class_alloc (hurd_cap_class_t cap_class,
			      hurd_cap_obj_t *r_obj);


/* Deallocate the capability object OBJ in the class CAP_CLASS.  OBJ
   must be locked and have no more references.  */
void hurd_cap_class_dealloc (hurd_cap_class_t cap_class, hurd_cap_obj_t obj);


/* Lock the object OBJ in class CAP_CLASS.  OBJ must be locked.  */
static inline error_t
hurd_cap_obj_lock (hurd_cap_class_t cap_class, hurd_cap_obj_t obj)
{
  return pthread_mutex_lock (&obj->lock);
}


/* Unlock the object OBJ in class CAP_CLASS.  OBJ must be locked.  */
static inline error_t
hurd_cap_obj_unlock (hurd_cap_class_t cap_class, hurd_cap_obj_t obj)
{
  return pthread_mutex_unlock (&obj->lock);
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
    hurd_cap_class_dealloc (cap_class, obj);
}
