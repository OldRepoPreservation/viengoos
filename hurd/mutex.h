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

#ifndef NDEBUG
#include <hurd/stddef.h>

struct ss_lock_trace
{
  __const char *caller;
  int line;
  ss_mutex_t *lock;
  int func;
  l4_thread_id_t tid;
};
#define SS_LOCK_TRACE_COUNT 1000
extern struct ss_lock_trace ss_lock_trace[SS_LOCK_TRACE_COUNT];
extern int ss_lock_trace_count;

#define SS_MUTEX_LOCK 1
#define SS_MUTEX_LOCK_WAIT 2
#define SS_MUTEX_UNLOCK 3
#define SS_MUTEX_TRYLOCK 4
#define SS_MUTEX_TRYLOCK_BLOCKED 5

#endif /* NDEBUG */

static inline void
ss_lock_trace_dump (ss_mutex_t *lock)
{
#ifndef NDEBUG
  int c = 0;
  int i;
  for (i = 0; i < SS_LOCK_TRACE_COUNT; i ++)
    {
      struct ss_lock_trace *trace
	= &ss_lock_trace[(ss_lock_trace_count - i - 1) % SS_LOCK_TRACE_COUNT];

      if (! trace->func)
	break;

      if (trace->lock != lock)
	continue;

      __const char *func = NULL;
      switch (trace->func)
	{
	case SS_MUTEX_LOCK:
	  func = "lock";
	  break;
	case SS_MUTEX_LOCK_WAIT:
	  func = "lock(wait)";
	  break;
	case SS_MUTEX_UNLOCK:
	  func = "unlock";
	  break;
	case SS_MUTEX_TRYLOCK:
	  func = "trylock";
	  break;
	case SS_MUTEX_TRYLOCK_BLOCKED:
	  func = "trylock(blocked)";
	  break;
	}
      assert (func);

      printf ("%d: %s(%p) from %s:%d by %x\n",
	      c --, func, lock, trace->caller, trace->line, trace->tid);
      if (c < -30)
	/* Show about the last 30 transactions.  */
	break;
    }
#endif  /* NDEBUG */
}

static inline void
ss_mutex_trace_add (int func, __const char *caller, int line, ss_mutex_t *lock)
{
#ifndef NDEBUG
  int i = (ss_lock_trace_count ++) % SS_LOCK_TRACE_COUNT;

  ss_lock_trace[i].func = func;
  ss_lock_trace[i].caller = caller;
  ss_lock_trace[i].line = line;
  ss_lock_trace[i].lock = lock;
  ss_lock_trace[i].tid = l4_myself ();
#endif  /* NDEBUG */
}

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

      /* We didn't get the lock.  */
      l4_accept (L4_UNTYPED_WORDS_ACCEPTOR);
      l4_msg_tag_t tag = l4_receive (owner);
      if (l4_ipc_failed (tag))
	debug (0, "Waiting on %x failed: %d (offset: %x)",
	       owner, (l4_error_code () >> 1) & 0x7,
	       l4_error_code () >> 4);

      assert (! l4_ipc_failed (tag));
    }
}

#define ss_mutex_lock(__sml_lockp)					\
  ss_mutex_lock (__func__, __LINE__, __sml_lockp)

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
  l4_msg_t msg;
  l4_msg_clear (msg);
  l4_msg_set_untyped_words (msg, 0);
  l4_msg_load (msg);
  l4_send (waiter);
}

#define ss_mutex_unlock(__sml_lockp)					\
  ss_mutex_unlock (__func__, __LINE__, __sml_lockp)

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
  ss_mutex_trylock (__func__, __LINE__, __sml_lockp)

#endif
