/* class-alloc.c - Allocate a capability object.
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <assert.h>
#include <pthread.h>

#include <hurd/cap-server.h>


/* Allocate a new capability object in the class CAP_CLASS.  The new
   capability object is locked and has one reference.  It will be
   returned in R_OBJ.  If the allocation fails, an error value will be
   returned.  */
error_t
hurd_cap_class_alloc (hurd_cap_class_t cap_class, hurd_cap_obj_t *r_obj)
{
  error_t err;
  hurd_cap_obj_t obj;

  err = hurd_slab_alloc (cap_class->obj_slab, (void **) &obj);
  if (err)
    return err;

  /* Let the user do their extra initialization.  */
  if (cap_class->obj_alloc)
    {
      err = (*cap_class->obj_alloc) (cap_class, obj);
      if (err)
	{
	  hurd_slab_dealloc (cap_class->obj_slab, obj);
	  return err;
	}
    }

  /* Now take the lock.  */
  hurd_cap_obj_lock (cap_class, obj);

  *r_obj = obj;
  return 0;
}
