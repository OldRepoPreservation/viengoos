/* bucket-manage-mt.c - Manage RPCs on a bucket.
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


/* Start managing RPCs on the bucket BUCKET.  The BOOTSTRAP capability
   object, which must be unlocked and have one reference throughout
   the whole time this function runs, is used for bootstrapping client
   connections.  The BOOTSTRAP capability object must be a capability
   object in one of the classes that have been added to the bucket.
   The GLOBAL_TIMEOUT parameter specifies the number of seconds until
   the manager times out (if there are no active users of capability
   objects in precious classes).  The WORKER_TIMEOUT parameter
   specifies the number of seconds until each worker thread times out
   (if there are no RPCs processed by the worker thread).

   If this returns ECANCELED, then hurd_cap_bucket_end was called with
   the force flag being true while there were still active users of
   capability objects in precious classes.  If this returns without
   any error, then the timeout expired, or hurd_cap_bucket_end was
   called without active users of capability objects in precious
   classes.  */
error_t
hurd_cap_bucket_manage_mt (hurd_cap_bucket_t bucket,
			   hurd_cap_obj_t bootstrap,
			   unsigned int global_timeout,
			   unsigned int worker_timeout)
{
  /* FIXME: Implement me, of course.  */
  return ENOSYS;
}
