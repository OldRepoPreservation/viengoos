/* rmutex.c - Small, simple LIFO recursive mutex implementation.
   Copyright (C) 2008 Free Software Foundation, Inc.
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

#ifndef __hurd_rmutex_have_type
#define __hurd_rmutex_have_type

/* For ss_mutex_t.  */
#define __need_ss_mutex_t
#include <hurd/mutex.h>
/* For l4_thread_id_t.  */
#include <l4/thread.h>

struct ss_rmutex
{
  ss_mutex_t lock;
  int count;
  l4_thread_id_t owner;
};
typedef struct ss_rmutex ss_rmutex_t;

#endif /* !__hurd_rmutex_have_type */

/* If __need_ss_rmutex_t is defined, then we only export the type
   definition.  */
#ifdef __need_ss_rmutex_t
# undef __need_ss_rmutex_t
#else

#ifndef _HURD_RMUTEX_H
#define _HURD_RMUTEX_H

#define SS_RMUTEX_INIT { 0, 0, l4_nilthread }

static inline void
ss_rmutex_lock (const char *caller, int line, ss_rmutex_t *lockp)
{
  while (1)
    {
      ss_mutex_lock (&lockp->lock);

      if (lockp->owner == l4_myself ())
	{
	  assert (lockp->count != 0);

	  ss_mutex_trace_add (SS_RMUTEX_LOCK_INC, caller, line, lockp);

	  if (lockp->count > 0)
	    lockp->count ++;
	  else
	    /* Negative means there may be waiters.  */
	    lockp->count --;

	  ss_mutex_unlock (&lockp->lock);
	  return;
	}

      if (lockp->owner == l4_nilthread)
	{
	  assert (lockp->count == 0);

	  ss_mutex_trace_add (SS_RMUTEX_LOCK, caller, line, lockp);

	  lockp->count = 1;
	  lockp->owner = l4_myself ();
	  ss_mutex_unlock (&lockp->lock);
	  return;
	}

      assert (lockp->count != 0);

      if (lockp->count > 0)
	/* Note that there are waiters.  */
	lockp->count = -lockp->count;

      ss_mutex_unlock (&lockp->lock);

      ss_mutex_trace_add (SS_RMUTEX_LOCK, caller, line, lockp);
      futex_wait (&lockp->count, lockp->count);
    }
}

#define ss_rmutex_lock(__srl_lockp)					\
  do									\
    {									\
      debug (5, "ss_rmutex_lock (%p)", __srl_lockp);			\
      ss_rmutex_lock (__func__, __LINE__, __srl_lockp);			\
    }									\
  while (0)

static inline void
ss_rmutex_unlock (const char *caller, int line, ss_rmutex_t *lockp)
{
  ss_mutex_lock (&lockp->lock);

  assert (lockp->owner == l4_myself ());
  assert (lockp->count != 0);

  int waiters = lockp->count < 0;
  if (lockp->count > 0)
    lockp->count --;
  else
    lockp->count ++;

  if (lockp->count == 0)
    ss_mutex_trace_add (SS_RMUTEX_UNLOCK, caller, line, lockp);
  else
    ss_mutex_trace_add (SS_RMUTEX_UNLOCK_DEC, caller, line, lockp);

  lockp->owner = l4_nilthread;

  ss_mutex_unlock (&lockp->lock);

  if (waiters)
    futex_wake (&lockp->count, 1);
}

#define ss_rmutex_unlock(__sru_lockp)					\
  do									\
    {									\
      debug (5, "ss_rmutex_unlock (%p)", __sru_lockp);			\
      ss_rmutex_unlock (__func__, __LINE__, __sru_lockp);		\
    }									\
  while (0)

static inline bool
ss_rmutex_trylock (const char *caller, int line, ss_rmutex_t *lockp)
{
  /* If someone holds the meta-lock then the lock is either held or
     about to be held.  */
  if (ss_mutex_trylock (&lockp->lock) == 1)
    /* Busy.  */
    {
      ss_mutex_trace_add (SS_RMUTEX_TRYLOCK_BLOCKED, caller, line, lockp);

      return false;
    }

  if (lockp->owner == l4_myself ())
    {
      assert (lockp->count != 0);

      ss_mutex_trace_add (SS_RMUTEX_TRYLOCK_INC, caller, line, lockp);

      if (lockp->count > 0)
	lockp->count ++;
      else
	/* Negative means there may be waiters.  */
	lockp->count --;

      ss_mutex_unlock (&lockp->lock);
      return true;
    }

  if (lockp->owner == l4_nilthread)
    {
      ss_mutex_trace_add (SS_RMUTEX_TRYLOCK, caller, line, lockp);

      assert (lockp->count == 0);
      lockp->count = 1;
      lockp->owner = l4_myself ();
      ss_mutex_unlock (&lockp->lock);
      return true;
    }

  assert (lockp->count != 0);

  ss_mutex_trace_add (SS_RMUTEX_TRYLOCK_BLOCKED, caller, line, lockp);

  return false;
}

#define ss_rmutex_trylock(__srl_lockp)					\
  ({									\
    bool __srl_r = ss_rmutex_trylock (__func__, __LINE__, __srl_lockp);	\
    debug (5, "ss_rmutex_trylock (%p) -> %s",				\
	   __srl_lockp, __srl_r ? "t" : "f");				\
    __srl_r;								\
  })

#endif

#endif /* __need_ss_rmutex_t */
