/* lock.h - Lock implementation for the Hurd.
   Copyright (C) 2008 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

   GNU Hurd is free software: you can redistribute it and/or modify it
   under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   GNU Hurd is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with GNU Hurd.  If not, see
   <http://www.gnu.org/licenses/>.  */

#include <hurd/mutex.h>
#include <hurd/rmutex.h>
#include <sys/lock.h>

void
__lock_release_ (_LOCK_T *lockp)
{
  ss_mutex_unlock (lockp);
}

void
__lock_acquire_ (_LOCK_T *lockp)
{
  ss_mutex_lock (lockp);
}

int
__lock_try_acquire_ (_LOCK_T *lockp)
{
  if (ss_mutex_trylock (lockp))
    /* Got the lock.  */
    return 0;
  return 1;
}

void
__lock_acquire_recursive_ (_LOCK_RECURSIVE_T *lockp)
{
  ss_rmutex_lock (lockp);
}

int
__lock_try_acquire_recursive_ (_LOCK_RECURSIVE_T *lockp)
{
  if (ss_rmutex_trylock (lockp))
    /* Got the lock.  */
    return 0;
  return 1;
}

void
__lock_release_recursive_ (_LOCK_RECURSIVE_T *lockp)
{
  ss_rmutex_unlock (lockp);
}
