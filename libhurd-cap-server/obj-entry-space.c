/* obj-entry-space.c - The capability object entry slab space.
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

#include <hurd/slab.h>

#include "cap-server-intern.h"


static error_t
_hurd_cap_obj_entry_constructor (void *hook, void *buffer)
{
  _hurd_cap_obj_entry_t entry = (_hurd_cap_obj_entry_t) buffer;

  /* The members cap_obj and client_item are initialized at
     instantiation time.  */

  entry->dead = 0;
  entry->internal_refs = 1;
  entry->external_refs = 1;

  return 0;
}


/* The global slab for all capability entries.  */
struct hurd_slab_space _hurd_cap_obj_entry_space
  = HURD_SLAB_SPACE_INITIALIZER (struct _hurd_cap_obj_entry, NULL, NULL,
				 _hurd_cap_obj_entry_constructor, NULL, NULL);
