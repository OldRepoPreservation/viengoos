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

#ifdef RM_INTERN
# error "This implementation is not appropriate for the kernel."
#endif

#ifndef __hurd_mutex_have_type
#define __hurd_mutex_have_type
typedef int ss_mutex_t;
#endif

/* If __need_ss_mutex_t is defined, then we only export the type
   definition.  */
#ifdef __need_ss_mutex_t
# undef __need_ss_mutex_t
#else

#ifndef _HURD_MUTEX_H
#define _HURD_MUTEX_H

#include <l4/thread.h>
#include <atomic.h>
#include <assert.h>
#include <hurd/lock.h>
#include <hurd/futex.h>

/* Unlocked.  */
#define _MUTEX_UNLOCKED 0
/* There the lock is locked.  */
#define _MUTEX_LOCKED 1
/* There there are waiters.  */
#define _MUTEX_WAITERS 2


static inline void
ss_mutex_lock (__const char *caller, int line, ss_mutex_t *lockp)
{
  int c;
  c = atomic_compare_and_exchange_val_acq (lockp, _MUTEX_LOCKED,
					   _MUTEX_UNLOCKED);
  if (c != _MUTEX_UNLOCKED)
    /* Someone else owns the lock.  */
    {
      ss_mutex_trace_add (SS_MUTEX_LOCK_WAIT, caller, line, lockp);

      if (c != _MUTEX_WAITERS)
	/* Note that there are waiters.  */
	c = atomic_exchange_acq (lockp, _MUTEX_WAITERS);

      /* Try to sleep but only if LOCKP is _MUTEX_WAITERS.  */
      while (c != _MUTEX_UNLOCKED)
	{
	  if (futex_wait (lockp, _MUTEX_WAITERS) == -1 && errno == EDEADLK)
	    {
	      debug (0, "Possible deadlock: %p!", lockp);
	      extern int backtrace (void **array, int size);
	      void *a[20];
	      int c = backtrace (a, sizeof (a) / sizeof (a[0]));
	      int i;
	      for (i = 0; i < c; i ++)
		debug (0, "%p", a[i]);
	      ss_lock_trace_dump (lockp);
	    }
	  c = atomic_exchange_acq (lockp, _MUTEX_WAITERS);
	}
    }

  ss_mutex_trace_add (SS_MUTEX_LOCK, caller, line, lockp);
}

#define ss_mutex_lock(__sml_lockp)					\
  do									\
    {									\
      debug (5, "ss_mutex_lock (%p)", __sml_lockp);			\
      ss_mutex_lock (__func__, __LINE__, __sml_lockp);			\
    }									\
  while (0)

static inline void
ss_mutex_unlock (__const char *caller, int line, ss_mutex_t *lockp)
{
  /* We rely on the knowledge that unlocked is 0, locked and no
     waiters is 1 and locked with waiters is 2.  Thus if *lockp is 1,
     an atomic dec yields 1 (the old value) and we know that there are
     no waiters.  */
  if (atomic_decrement_and_test (lockp) != _MUTEX_LOCKED)
    /* There are waiters.  */
    {
      *lockp = 0;
      futex_wake (lockp, 1);
    }

  ss_mutex_trace_add (SS_MUTEX_UNLOCK, caller, line, lockp);
}

#define ss_mutex_unlock(__smu_lockp)					\
  do									\
    {									\
      debug (5, "ss_mutex_unlock (%p)", __smu_lockp);			\
      ss_mutex_unlock (__func__, __LINE__, __smu_lockp);		\
    }									\
  while (0)

static inline bool
ss_mutex_trylock (__const char *caller, int line, ss_mutex_t *lockp)
{
  int c;
  c = atomic_compare_and_exchange_val_acq (lockp, _MUTEX_LOCKED,
					   _MUTEX_UNLOCKED);
  if (c == _MUTEX_UNLOCKED)
    /* Got the lock.  */
    {
      ss_mutex_trace_add (SS_MUTEX_TRYLOCK, caller, line, lockp);
      return true;
    }

  // ss_mutex_trace_add (SS_MUTEX_TRYLOCK_BLOCKED, caller, line, lockp);

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

#endif /* __need_ss_mutex_t */

