/* thread.c - Thread object implementation.
   Copyright (C) 2007 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 3 of the
   License, or (at your option) any later version.

   The GNU Hurd is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see
   <http://www.gnu.org/licenses/>.  */

#include <l4.h>
#include <l4/thread-start.h>
#include <hurd/ihash.h>
#include <hurd/cap.h>
#include <hurd/stddef.h>
#include <hurd/exceptions.h>
#include <hurd/thread.h>
#include <bit-array.h>

#include "cap.h"
#include "object.h"
#include "thread.h"
#include "activity.h"

/* Number of user thread ids.  */
#ifndef THREAD_IDS_MAX
# define THREAD_IDS_MAX 1 << 16
#endif

/* Thread ids that are in use.  We use one bit per thread id.  */
/* XXX: We need to save what thread ids are allocated on backing
   store.  In fact, we need to save pointers to the thread objects as
   well so that we can restart all the threads when the system
   restarts.  See comment below for some problems.  */
static unsigned char thread_ids[THREAD_IDS_MAX / 8];
/* The next thread id to consider for allocation.  */
static int thread_id_next;

#define THREAD_ID_BASE (l4_thread_user_base () + 10)

/* When a client invokes us, we need to be able to find its
   corresponding thread structure.  XXX: This hash is marginally
   problematic the memory requirement is dynamic.  In a kernel that
   supported an end point abstraction with protected payload, we
   wouldn't need this.  */
static struct hurd_ihash tid_to_thread
  = HURD_IHASH_INITIALIZER (HURD_IHASH_NO_LOCP);

struct thread *
thread_lookup (l4_thread_id_t threadid)
{
  struct thread *thread = hurd_ihash_find (&tid_to_thread,
					   l4_thread_no (threadid));
  if (! thread)
    debug (1, "(%x.%x) => NULL",
	   l4_thread_no (threadid), l4_version (threadid));

  /* Sanity check.  */
  if (thread->tid != threadid)
    debug (1, "hash inconsistent: threadid: (%x.%x) "
	   "!= thread (%llx/0x%p)->tid: (%x.%x)",
	   l4_thread_no (threadid), l4_version (threadid),
	   object_to_object_desc ((struct object *) thread)->oid, thread,
	   l4_thread_no (thread->tid), l4_version (thread->tid));
  assert (thread->tid == threadid);

  return thread;
}

void
thread_init (struct thread *thread)
{
  /* Allocate a thread id.  */
  /* Find the next free thread id starting at thread_id_next.  */
  int tid;
  tid = bit_alloc (thread_ids, THREAD_IDS_MAX / 8, thread_id_next);
  if (tid == -1)
    panic ("No thread ids left!");
  thread_id_next = (tid + 1) % THREAD_IDS_MAX;
  tid = tid * 2 + THREAD_ID_BASE;

  thread->tid = l4_global_id (tid, HURD_THREAD_MAIN_VERSION);
  debug (3, "Allocated thread 0x%x.%x (%llx/%p)",
	 l4_thread_no (thread->tid), l4_version (thread->tid),
	 object_to_object_desc ((struct object *) thread)->oid, thread);

  bool had_value;
  error_t err = hurd_ihash_replace (&tid_to_thread, tid, thread,
				    &had_value, NULL);
  assert (err == 0);
  assert (had_value == false);

  thread->init = true;
}

void
thread_deinit (struct activity *activity, struct thread *thread)
{
  debug (3, "Destroying thread 0x%x.%x",
	 l4_thread_no (thread->tid), l4_version (thread->tid));

  if (! thread->init)
    return;

  if (thread->commissioned)
    thread_decommission (thread);

  /* Free the thread id.  */
  bit_dealloc (thread_ids,
	       (l4_thread_no (thread->tid) - THREAD_ID_BASE) / 2);

  int removed = hurd_ihash_remove (&tid_to_thread,
				   l4_thread_no (thread->tid));
  assert (removed);

  thread->init = false;
}

/* FIXME:
   Saving and restoring register state.

   Here's the plan: when a thread is decommissioned, we do a space
   control and bring the thread into our own address space.  Then we
   do an exregs to get its register state.  We need then need to set
   the thread running to grab its floating point state.  Also, we need
   to copy the utcb.  We can do the same in reverse when commissioning
   a thread.

   There is one complication: if a thread is targetting by an IPC,
   then we don't get a fault!  The simpliest solution appears to be to
   keep the kernel state around.  And when the system starts to
   restart all threads.  (Just restarting threads in the receive phase
   is not enough: when another thread does a send, the thread should
   block.)  */

void
thread_commission (struct thread *thread)
{
  assert (! thread->commissioned);

  if (! thread->init)
    thread_init (thread);

  debug (5, "Commissioning thread 0x%x.%x",
	 l4_thread_no (thread->tid), l4_version (thread->tid));

  /* Create the AS.  */
  l4_word_t ret;
  ret = l4_thread_control (thread->tid, thread->tid,
			   l4_myself (), l4_nilthread, (void *) -1);
  if (! ret)
    panic ("Could not create thread (id=%x.%x): %s",
	   l4_thread_no (thread->tid), l4_version (thread->tid),
	   l4_strerror (l4_error_code ()));

  l4_word_t control;
  ret = l4_space_control (thread->tid, l4_nilthread,
			  l4_fpage_log2 (KIP_BASE,
					 l4_kip_area_size_log2 ()),
			  l4_fpage (UTCB_AREA_BASE,
				    UTCB_AREA_SIZE),
			  l4_anythread, &control);
  if (! ret)
    panic ("Could not create address space: %s",
	   l4_strerror (l4_error_code ()));

  ret = l4_thread_control (thread->tid, thread->tid,
			   l4_myself (),
			   l4_myself (),
			   (void *) UTCB_AREA_BASE);
  if (! ret)
    {
      int err = l4_error_code ();
      panic ("Failed to create thread %x.%x: %s (%d)",
	     l4_thread_no (thread->tid), l4_version (thread->tid),
	     l4_strerror (err), err);
    }

  ret = l4_thread_control (hurd_exception_thread (thread->tid), thread->tid,
			   l4_myself (),
			   l4_myself (),
			   (void *) UTCB_AREA_BASE + l4_utcb_area_size ());
  if (! ret)
    {
      l4_thread_id_t tid = hurd_exception_thread (thread->tid);
      int err = l4_error_code ();
      panic ("Failed to create exception thread %x.%x: %s (%d)",
	     l4_thread_no (tid), l4_version (tid),
	     l4_strerror (err), err);
    }

  /* XXX: Restore the register state!  (See comment above for the
     plan.)  */

  /* Start the thread.  */
  if (thread->sp || thread->ip)
    {
      l4_word_t ret = l4_thread_start (thread->tid, thread->sp, thread->ip);
      assert (ret == 1);
    }

  thread->commissioned = 1;
}

void
thread_decommission (struct thread *thread)
{
  assert (thread->commissioned);

  debug (5, "Decommissioning thread 0x%x.%x",
	 l4_thread_no (thread->tid), l4_version (thread->tid));

  /* XXX: Save the register state!  (See comment above for the
     plan.)  */

  l4_thread_id_t tid = thread->tid;
  l4_word_t ret;
  ret = l4_thread_control (tid, l4_nilthread,
			   l4_nilthread, l4_nilthread, (void *) -1);
  if (! ret)
    panic ("Failed to delete main thread %x.%x",
	   l4_thread_no (tid), l4_version (tid));

  tid = l4_global_id (l4_thread_no (thread->tid) + 1,
		      HURD_THREAD_EXCEPTION_VERSION);

  /* Free the thread.  */
  ret = l4_thread_control (tid, l4_nilthread,
			   l4_nilthread, l4_nilthread, (void *) -1);
  if (! ret)
    panic ("Failed to delete exception thread %d",
	   l4_thread_no (tid));

  thread->commissioned = 0;
}

error_t
thread_exregs (struct activity *principal,
	       struct thread *thread, l4_word_t control,
	       struct cap *aspace, struct cap *activity,
	       l4_word_t *sp, l4_word_t *ip,
	       l4_word_t *eflags, l4_word_t *user_handle,
	       struct cap *aspace_out, struct cap *activity_out)
{
  if ((control & ~(HURD_EXREGS_SET_REGS
		   | HURD_EXREGS_GET_REGS
		   | HURD_EXREGS_START
		   | HURD_EXREGS_STOP
		   | HURD_EXREGS_ABORT_IPC)))
    {
      debug (1, "Control word contains invalid bits");
      return EINVAL;
    }

  if ((control & HURD_EXREGS_GET_REGS) && aspace_out)
    cap_copy (principal, aspace_out, ADDR_VOID, thread->aspace, ADDR_VOID);

  if ((control & HURD_EXREGS_SET_ASPACE))
    cap_copy (principal, &thread->aspace, ADDR_VOID, *aspace, ADDR_VOID);

  if ((control & HURD_EXREGS_GET_REGS) && activity_out)
    cap_copy (principal, activity_out, ADDR_VOID, thread->activity, ADDR_VOID);

  if ((control & HURD_EXREGS_SET_ACTIVITY))
    cap_copy (principal, &thread->activity, ADDR_VOID, *activity, ADDR_VOID);

  /* Clear hurd specific bits so that l4_exchange_registers works.  */
  control = control & ~(HURD_EXREGS_SET_ACTIVITY | HURD_EXREGS_SET_ASPACE);

  if (thread->commissioned)
    {
      l4_word_t dummy = 0;
      l4_thread_id_t tid = thread->tid;
      _L4_exchange_registers (&tid,
			      &control, sp, ip, eflags,
			      user_handle, &dummy);
    }
  else
    {
      l4_word_t t = thread->sp;
      if ((control & HURD_EXREGS_SET_SP))
	thread->sp = *sp;
      if ((control & HURD_EXREGS_GET_REGS))
	*sp = t;

      t = thread->ip;
      if ((control & HURD_EXREGS_SET_IP))
	thread->ip = *ip;
      if ((control & HURD_EXREGS_GET_REGS))
	*ip = t;

      t = thread->eflags;
      if ((control & HURD_EXREGS_SET_EFLAGS))
	thread->eflags = *eflags;
      if ((control & HURD_EXREGS_GET_REGS))
	*eflags = t;

      t = thread->user_handle;
      if ((control & HURD_EXREGS_SET_USER_HANDLE))
	thread->user_handle = *user_handle;
      if ((control & HURD_EXREGS_GET_REGS))
	*user_handle = t;

      if ((control & HURD_EXREGS_START) == HURD_EXREGS_START)
	{
	  struct object *a = cap_to_object (principal, &thread->activity);
	  if (! a)
	    {
	      debug (1, "Thread not schedulable: no activity");
	      return 0;
	    }

	  struct object_desc *desc = object_to_object_desc (a);
	  if (desc->type != cap_activity)
	    {
	      debug (1, "Thread not schedulable: activity slot contains a %s",
		     cap_type_string (desc->type));
	      return 0;
	    }

	  thread_commission (thread);

	  control = _L4_XCHG_REGS_SET_SP | _L4_XCHG_REGS_SET_IP
	    | _L4_XCHG_REGS_SET_FLAGS | _L4_XCHG_REGS_SET_USER_HANDLE;

	  l4_word_t dummy = 0;
	  l4_thread_id_t tid = thread->tid;
	  _L4_exchange_registers (&tid,
				  &control, &thread->sp, &thread->ip,
				  &thread->eflags, &thread->user_handle,
				  &dummy);
	}
    }

  return 0;
}
