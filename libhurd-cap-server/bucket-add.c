/* bucket-add.c - Add a capability class to a bucket.
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


/* Add the capability class CAP_CLASS to the bucket BUCKET.  If
   PRECIOUS is true, then this class is inspected for active users
   whenever a timeout of un-forced "go away" request occurs.  */
error_t
hurd_cap_bucket_add (hurd_cap_bucket_t bucket,
		     hurd_cap_class_t cap_class,
		     bool precious)
{
  error_t err;
  struct _hurd_cap_class_entry entry;
  unsigned int id;

  entry.cap_class = cap_class;
  entry.precious = precious;

  pthread_mutex_lock (&bucket->lock);
  hurd_table_enter (&bucket->classes, &entry, &id);
  pthread_mutex_unlock (&bucket->lock);
}
