/* mutex.c - Small, simple LIFO mutex implementation.
   Copyright (C) 2007 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

   This file is part of the GNU Hurd.

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

#ifndef _HURD_MUTEX_H
#define _HURD_MUTEX_H

#include <l4/thread.h>
#include <atomic.h>
#include <assert.h>
#include <hurd/lock.h>

typedef l4_thread_id_t ss_mutex_t;

static inline void
ss_mutex_lock (__const char *caller, int line, ss_mutex_t *lock)
{
  l4_thread_id_t owner;

  for (;;)
    {
      owner = atomic_exchange_acq (lock, l4_myself ());
      if (owner == l4_nilthread)
	{
	  ss_mutex_trace_add (SS_MUTEX_LOCK, caller, line, lock);
	  return;
	}
  
      ss_mutex_trace_add (SS_MUTEX_LOCK_WAIT, caller, line, lock);

      if (owner == l4_myself ())
	ss_lock_trace_dump (lock);
      assert (owner != l4_myself ());

      __ss_lock_wait (owner);
    }
}

#define ss_mutex_lock(__sml_lockp)					\
  do									\
    {									\
      debug (5, "ss_mutex_lock (%p)", __sml_lockp);			\
      ss_mutex_lock (__func__, __LINE__, __sml_lockp);			\
    }									\
  while (0)

static inline void
ss_mutex_unlock (__const char *caller, int line, ss_mutex_t *lock)
{
  l4_thread_id_t waiter;

  waiter = atomic_exchange_acq (lock, l4_nilthread);
  ss_mutex_trace_add (SS_MUTEX_UNLOCK, caller, line, lock);
  if (waiter == l4_myself ())
    /* No waiter.  */
    return;

  if (waiter == l4_nilthread)
    ss_lock_trace_dump (lock);
  assert (waiter != l4_nilthread);

  /* Signal the waiter.  */
  __ss_lock_wakeup (waiter);
}

#define ss_mutex_unlock(__smu_lockp)					\
  do									\
    {									\
      debug (5, "ss_mutex_unlock (%p)", __smu_lockp);			\
      ss_mutex_unlock (__func__, __LINE__, __smu_lockp);		\
    }									\
  while (0)

static inline bool
ss_mutex_trylock (__const char *caller, int line, ss_mutex_t *lock)
{
  l4_thread_id_t owner;

  owner = atomic_compare_and_exchange_val_acq (lock, l4_myself (),
					       l4_nilthread);
  if (owner == l4_nilthread)
    {
      ss_mutex_trace_add (SS_MUTEX_TRYLOCK, caller, line, lock);
      return true;
    }

  ss_mutex_trace_add (SS_MUTEX_TRYLOCK_BLOCKED, caller, line, lock);

  return false;
}

#define ss_mutex_trylock(__sml_lockp)					\
  ({									\
    bool __sml_r = ss_mutex_trylock (__func__, __LINE__, __sml_lockp);	\
    debug (5, "ss_mutex_trylock (%p) -> %s",				\
	   __sml_lockp, __sml_r ? "t" : "f");				\
    __sml_r;								\
  })

#endif
