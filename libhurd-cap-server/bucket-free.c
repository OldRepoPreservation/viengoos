/* bucket-create.c - Create a capability bucket.
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


/* Free the bucket BUCKET, which must not be used.  */
void
hurd_cap_bucket_free (hurd_cap_bucket_t bucket)
{
  /* FIXME: Add some assertions to ensure it is not used.
     Reintroduce _hurd_cap_client_try_destroy.  */
  hurd_table_destroy (&bucket->clients);
  pthread_cond_destroy (&bucket->cond);
  hurd_table_destroy (&bucket->classes);
  pthread_mutex_destroy (&bucket->lock);
  pthread_mutex_destroy (&bucket->client_cond_lock);
  pthread_cond_destroy (&bucket->client_cond);
  free (bucket);
}

