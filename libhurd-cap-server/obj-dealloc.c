/* obj-dealloc.c - Deallocate a capability object.
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

#include <assert.h>
#include <pthread.h>

#include "cap-server-intern.h"


/* Deallocate the capability object OBJ, which must be locked and have
   no more references.  */
void
_hurd_cap_obj_dealloc (hurd_cap_obj_t obj)
{
  hurd_cap_class_t cap_class = obj->cap_class;

  /* First let the user do their reinitialization.  */
  (*cap_class->obj_reinit) (cap_class, obj);

  /* Now do our part of the reinitialization.  */
  assert (obj->refs == 0);
  assert (obj->state == _HURD_CAP_STATE_GREEN);
  assert (obj->pending_rpcs == NULL);
  assert (obj->clients == NULL);
  hurd_cap_obj_unlock (obj);

  hurd_slab_dealloc (&cap_class->obj_space, obj);
}
