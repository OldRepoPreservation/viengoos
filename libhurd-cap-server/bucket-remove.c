/* bucket-remove.c - Remove a capability class from a bucket.
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
#include <assert.h>
#include <pthread.h>

#include "cap-server-intern.h"


/* Remove the capability class CAP_CLASS from the bucket BUCKET.  */
error_t
hurd_cap_bucket_remove (hurd_cap_bucket_t bucket,
			hurd_cap_class_t cap_class)
{
  error_t err;

  /* FIXME: More work to do when removing capability classes?  At
     least ensure it is not in use.  */

  pthread_mutex_lock (&bucket->lock);
  HURD_TABLE_ITERATE (&bucket->classes, idx)
    {
      _hurd_cap_class_entry_t entry;

      entry = (_hurd_cap_class_entry_t)
	HURD_TABLE_LOOKUP (&bucket->classes, idx);

      if (entry->cap_class == cap_class)
	{
	  hurd_table_remove (&bucket->classes, idx);
	  break;
	}
    }
  pthread_mutex_unlock (&bucket->lock);
}
