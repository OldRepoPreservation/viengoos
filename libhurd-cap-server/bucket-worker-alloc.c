/* bucket-worker-alloc.c - Set the worker allocation policy.
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

#include "cap-server-intern.h"


/* If ASYNC is true, allocate worker threads asynchronously whenever
   the number of worker threads is exhausted.  This is only actually
   required for physmem (the physical memory server), to allow to
   break out of a dead-lock between physmem and the task server.  It
   should be unnecessary for any other server.

   The default is to false, which means that worker threads are
   allocated synchronously by the manager thread.

   This function should be called before the manager is started with
   hurd_cap_bucket_manage_mt.  It is only used for the multi-threaded
   RPC manager.  */
error_t
hurd_cap_bucket_worker_alloc (hurd_cap_bucket_t bucket, bool async)
{
  pthread_mutex_lock (&bucket->lock);
  bucket->is_worker_alloc_async = async;
  pthread_mutex_unlock (&bucket->lock);

  return 0;
}
