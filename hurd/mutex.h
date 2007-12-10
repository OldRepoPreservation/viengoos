/* mutex.c - Small, simple mutex LIFO mutex implementation.
   Copyright (C) 2007 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU Hurd is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#ifndef _HURD_MUTEX_H
#define _HURD_MUTEX_H

#include <l4/thread.h>
#include <l4/ipc.h>
#include <atomic.h>
#include <assert.h>

typedef l4_thread_id_t ss_mutex_t;

static inline void
ss_mutex_lock (ss_mutex_t *lock)
{
  l4_thread_id_t owner;

  for (;;)
    {
      owner = atomic_exchange_acq (lock, l4_myself ());
      if (owner == l4_nilthread)
	return;
  
      assert (owner != l4_myself ());

      /* We didn't get the lock.  */
      l4_wait (&owner);
    }
}

static inline void
ss_mutex_unlock (ss_mutex_t *lock)
{
  l4_thread_id_t waiter;

  waiter = atomic_exchange_acq (lock, l4_nilthread);
  if (waiter == l4_myself ())
    /* No waiter.  */
    return;

  assert (waiter != l4_nilthread);

  /* Signal the waiter.  */
  l4_send (waiter);
}

static inline bool
ss_mutex_trylock (ss_mutex_t *lock)
{
  l4_thread_id_t owner;

  owner = atomic_compare_and_exchange_val_acq (lock, l4_myself (),
					       l4_nilthread);
  if (owner == l4_nilthread)
    return true;

  assert (owner != l4_myself ());
  return false;
}

#endif
