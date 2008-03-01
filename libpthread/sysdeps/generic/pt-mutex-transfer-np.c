/* Transfer ownership of a mutex.  Generic version.
   Copyright (C) 2008 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License
   as published by the Free Software Foundation; either version 3 of
   the License, or (at your option) any later version.

   The GNU Hurd is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this program.  If not, see
   <http://www.gnu.org/licenses/>.  */

#include <pthread.h>
#include <assert.h>

#include <pt-internal.h>

int
__pthread_mutex_transfer_np (struct __pthread_mutex *mutex, pthread_t tid)
{
  assert (mutex->owner == _pthread_self ());

  struct __pthread *thread = __pthread_getid (tid);
  if (! thread)
    return ESRCH;

  if (thread == _pthread_self ())
    return 0;

  if (mutex->attr && mutex->attr->mutex_type == PTHREAD_MUTEX_ERRORCHECK)
    {

      if (mutex->owner != _pthread_self ())
	return EPERM;

      mutex->owner = thread;
    }

#ifndef NDEBUG
  mutex->owner = thread;
#endif

  return 0;
}

strong_alias (__pthread_mutex_transfer_np, pthread_mutex_transfer_np)
