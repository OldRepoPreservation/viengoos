/* class-create.c - Create a capability class.
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
#include <stdlib.h>

#include <hurd/cap-server.h>


/* Create a new capability class for objects with the size SIZE,
   including the struct hurd_cap_obj, which has to be placed at the
   beginning of each capability object.

   The callback OBJ_INIT is used whenever a capability object in this
   class is created.  The callback OBJ_REINIT is used whenever a
   capability object in this class is deallocated and returned to the
   slab.  OBJ_REINIT should return a capability object that is not
   used anymore into the same state as OBJ_INIT does for a freshly
   allocated object.  OBJ_DESTROY should deallocate all resources for
   this capablity object.  Note that if OBJ_INIT or OBJ_REINIT fails,
   the object is considered to be fully destroyed.  No extra call to
   OBJ_DESTROY will be made for such objects.

   The new capability class is returned in R_CLASS.  If the creation
   fails, an error value will be returned.  */
error_t
hurd_cap_class_create_untyped (size_t size, size_t alignment,
			       hurd_cap_obj_init_t obj_init,
			       hurd_cap_obj_alloc_t obj_alloc,
			       hurd_cap_obj_reinit_t obj_reinit,
			       hurd_cap_obj_destroy_t obj_destroy,
			       hurd_cap_class_demuxer_t demuxer,
			       hurd_cap_class_t *r_class)
{
  error_t err;
  hurd_cap_class_t cap_class = malloc (sizeof (struct hurd_cap_class));

  if (!cap_class)
    return errno;

  err = hurd_cap_class_init_untyped (cap_class, size, alignment, obj_init,
				     obj_alloc, obj_reinit, obj_destroy,
				     demuxer);
  if (err)
    {
      free (cap_class);
      return err;
    }

  *r_class = cap_class;
  return 0;
}
