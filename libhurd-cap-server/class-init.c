/* class-init.c - Initialize a capability class.
   Copyright (C) 2004, 2005 Free Software Foundation, Inc.
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
#include <assert.h>
#include <stdlib.h>
#include <pthread.h>

#include <hurd/slab.h>
#include <hurd/cap-server.h>


/* Initialize the slab object pointed to by BUFFER.  HOOK is as
   provided to hurd_slab_create.  */
static error_t
_hurd_cap_obj_constructor (void *hook, void *buffer)
{
  hurd_cap_class_t cap_class = (hurd_cap_class_t) hook;
  hurd_cap_obj_t obj = (hurd_cap_obj_t) buffer;
  error_t err;

  /* First do our own initialization.  */
  obj->cap_class = cap_class;

  err = pthread_mutex_init (&obj->lock, 0);
  if (err)
    return err;

  obj->refs = 1;
  obj->state = _HURD_CAP_STATE_GREEN;
  obj->pending_rpcs = NULL;
  /* The member COND_WAITER does not need to be initialized.  */
  obj->clients = NULL;

  /* Then do the user part, if necessary.  */
  if (cap_class->obj_init)
    {
      err = (*cap_class->obj_init) (cap_class, obj);
      if (err)
	{
	  pthread_mutex_destroy (&obj->lock);
	  return err;
	}
    }

  return 0;
}


/* Destroy the slab object pointed to by BUFFER.  HOOK is as provided
   to hurd_slab_create.  */
static void
_hurd_cap_obj_destructor (void *hook, void *buffer)
{
  hurd_cap_class_t cap_class = (hurd_cap_class_t) hook;
  hurd_cap_obj_t obj = (hurd_cap_obj_t) buffer;

  if (cap_class->obj_destroy)
    (*cap_class->obj_destroy) (cap_class, obj);

  pthread_mutex_destroy (&obj->lock);
}


/* Same as hurd_cap_class_create, but doesn't allocate the storage for
   CAP_CLASS.  Instead, you have to provide it.  */
error_t
hurd_cap_class_init_untyped (hurd_cap_class_t cap_class,
			     size_t size, size_t alignment,
			     hurd_cap_obj_init_t obj_init,
			     hurd_cap_obj_alloc_t obj_alloc,
			     hurd_cap_obj_reinit_t obj_reinit,
			     hurd_cap_obj_destroy_t obj_destroy,
			     hurd_cap_class_demuxer_t demuxer)
{
  error_t err;

  /* The alignment requirements must be a power of 2.  */
  assert ((alignment & (alignment - 1)) == 0
	  || ! "hurd_cap_class_init_untyped: "
	  "requested alignment not a power of 2");

  /* Find the smallest alignment requirement common to the user object
     and a struct hurd_cap_obj.  Since both are required to be a power
     of 2, we need simply take the larger one.  */
  if (alignment < __alignof__(struct hurd_cap_obj))
    alignment = __alignof__(struct hurd_cap_obj);

  size += hurd_cap_obj_user_offset (alignment);

  /* Capability object management.  */

  cap_class->obj_init = obj_init;
  cap_class->obj_alloc = obj_alloc;
  cap_class->obj_reinit = obj_reinit;
  cap_class->obj_destroy = obj_destroy;

  err = hurd_slab_init (&cap_class->obj_space, size, alignment, NULL, NULL,
			_hurd_cap_obj_constructor, _hurd_cap_obj_destructor,
			cap_class);
  if (err)
    goto err_obj_space;

  err = pthread_cond_init (&cap_class->obj_cond, NULL);
  if (err)
    goto err_obj_cond;

  err = pthread_mutex_init (&cap_class->obj_cond_lock, NULL);
  if (err)
    goto err_obj_cond_lock;


  /* Class management.  */

  cap_class->demuxer = demuxer;

  err = pthread_mutex_init (&cap_class->lock, NULL);
  if (err)
    goto err_lock;

  cap_class->state = _HURD_CAP_STATE_GREEN;

  err = pthread_cond_init (&cap_class->cond, NULL);
  if (err)
    goto err_cond;

  /* The cond_waiter member doesn't need to be initialized.  It is
     only valid when CAP_CLASS->STATE is _HURD_CAP_STATE_YELLOW.  */

  cap_class->pending_rpcs = NULL;

  /* FIXME: Add the class to the list of classes to be served by
     RPCs.  */

  return 0;

  /* This is provided here in case you add more initialization to the
     end of the above code.  */
#if 0
  pthread_cond_destroy (&cap_class->cond);
#endif

 err_cond:
  pthread_mutex_destroy (&cap_class->lock);

 err_lock:
  pthread_mutex_destroy (&cap_class->obj_cond_lock);

 err_obj_cond_lock:
  pthread_cond_destroy (&cap_class->obj_cond);

 err_obj_cond:
  /* This can not fail at this point.  */
  hurd_slab_destroy (&cap_class->obj_space);

 err_obj_space:
  return err;
}
