/* lock.c - Locking helper functions.
   Copyright (C) 2007, 2008 Free Software Foundation, Inc.
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

#ifndef _HURD_LOCK_H
#define _HURD_LOCK_H

#include <l4/thread.h>
#include <l4/ipc.h>
#include <atomic.h>
#include <assert.h>

#ifndef NDEBUG
#include <hurd/stddef.h>

struct ss_lock_trace
{
  const char *caller;
  unsigned int line : 28;
  unsigned int func : 4;
  void *lock;
  l4_thread_id_t tid;
};
#define SS_LOCK_TRACE_COUNT 1000
extern struct ss_lock_trace ss_lock_trace[SS_LOCK_TRACE_COUNT];
extern int ss_lock_trace_count;

enum
  {
    SS_MUTEX_LOCK,
    SS_MUTEX_LOCK_WAIT,
    SS_MUTEX_UNLOCK,
    SS_MUTEX_TRYLOCK,
    SS_MUTEX_TRYLOCK_BLOCKED,

    SS_RMUTEX_LOCK,
    SS_RMUTEX_LOCK_INC,
    SS_RMUTEX_LOCK_WAIT,
    SS_RMUTEX_UNLOCK,
    SS_RMUTEX_UNLOCK_DEC,
    SS_RMUTEX_TRYLOCK,
    SS_RMUTEX_TRYLOCK_INC,
    SS_RMUTEX_TRYLOCK_BLOCKED,
  };

#endif /* NDEBUG */

static inline void
ss_lock_trace_dump (void *lock)
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

	case SS_RMUTEX_LOCK:
	  func = "ss_rmutex_lock";
	  break;
	case SS_RMUTEX_LOCK_INC:
	  func = "ss_rmutex_lock_inc";
	  break;
	case SS_RMUTEX_LOCK_WAIT:
	  func = "ss_rmutex_lock_wait";
	  break;
	case SS_RMUTEX_UNLOCK:
	  func = "ss_rmutex_unlock";
	  break;
	case SS_RMUTEX_UNLOCK_DEC:
	  func = "ss_rmutex_unlock_dec";
	  break;
	case SS_RMUTEX_TRYLOCK:
	  func = "ss_rmutex_trylock";
	  break;
	case SS_RMUTEX_TRYLOCK_INC:
	  func = "ss_rmutex_trylock_inc";
	  break;
	case SS_RMUTEX_TRYLOCK_BLOCKED:
	  func = "ss_rmutex_trylock_blocked";
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
ss_mutex_trace_add (int func, __const char *caller, int line, void *lock)
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

/* Wait for a wakeup message from FROM.  */
static inline void
__ss_lock_wait (l4_thread_id_t from)
{
  /* We didn't get the lock.  */
  l4_accept (L4_UNTYPED_WORDS_ACCEPTOR);
  l4_msg_tag_t tag = l4_receive (from);
  assertx (! l4_ipc_failed (tag),
	   "Waiting on %x failed: %d (offset: %x)",
	   from, (l4_error_code () >> 1) & 0x7, l4_error_code () >> 4);
}

/* Wake up target TARGET.  */
static inline void
__ss_lock_wakeup (l4_thread_id_t target)
{
  l4_msg_t msg;
  l4_msg_clear (msg);
  l4_msg_set_untyped_words (msg, 0);
  l4_msg_load (msg);
  l4_msg_tag_t tag = l4_send (target);
  assertx (! l4_ipc_failed (tag),
	   "Waking %x failed: %d (offset: %x)",
	   target, (l4_error_code () >> 1) & 0x7, l4_error_code () >> 4);
}

#endif /* _HURD_LOCK_H  */
