/* thread.c - Thread object implementation.
   Copyright (C) 2007, 2008 Free Software Foundation, Inc.
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
#include "zalloc.h"

#define THREAD_VERSION 2

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
static struct hurd_ihash tid_to_thread;

struct thread *
thread_lookup (l4_thread_id_t threadid)
{
  struct thread *thread = hurd_ihash_find (&tid_to_thread,
					   l4_thread_no (threadid));
  if (! thread)
    debug (1, "(%x) => NULL", threadid);
  assert (thread);

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
  static bool have_tid_to_thread_hash;
  if (! have_tid_to_thread_hash)
    {
      size_t size = PAGESIZE * 10;
      void *buffer = (void *) zalloc (size);
      if (! buffer)
	panic ("Failed to allocate memory for thread has.");

      memset (buffer, 0, size);

      hurd_ihash_init_with_buffer (&tid_to_thread, false, HURD_IHASH_NO_LOCP,
				   buffer, size);

      have_tid_to_thread_hash = 1;
    }

  /* Allocate a thread id.  */
  /* Find the next free thread id starting at thread_id_next.  */
  int tid;
  tid = bit_alloc (thread_ids, THREAD_IDS_MAX / 8, thread_id_next);
  if (tid == -1)
    panic ("No thread ids left!");
  thread_id_next = (tid + 1) % THREAD_IDS_MAX;
  tid = tid + THREAD_ID_BASE;

  thread->tid = l4_global_id (tid, THREAD_VERSION);
  debug (4, "Allocated thread 0x%x.%x (%llx/%p)",
	 l4_thread_no (thread->tid), l4_version (thread->tid),
	 object_to_object_desc ((struct object *) thread)->oid, thread);

  bool had_value;
  error_t err = hurd_ihash_replace (&tid_to_thread, tid, thread,
				    &had_value, NULL);
  assertx (err == 0, "%d", err);
  assert (had_value == false);

  thread->init = true;
}

void
thread_deinit (struct activity *activity, struct thread *thread)
{
  debug (4, "Destroying thread 0x%x.%x",
	 l4_thread_no (thread->tid), l4_version (thread->tid));

  if (! thread->init)
    return;

  if (thread->commissioned)
    thread_decommission (thread);

  if (thread->wait_queue_p)
    /* THREAD is attached to a wait queue.  Detach it.  */
    object_wait_queue_dequeue (activity, thread);

  /* Free the thread id.  */
  bit_dealloc (thread_ids,
	       l4_thread_no (thread->tid) - THREAD_ID_BASE);

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

  /* XXX: Restore the register state!  (See comment above for the
     plan.)  */

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

  thread->commissioned = 0;
}

static void
control_to_string (l4_word_t control, char string[33])
{
  int i = 0;
  string[i ++] = (control & _L4_XCHG_REGS_DELIVER) ? 'd' : '-';
  string[i ++] = (control & _L4_XCHG_REGS_SET_HALT) ? 'h' : '-';
  string[i ++] = (control & _L4_XCHG_REGS_SET_PAGER) ? 'p' : '-';
  string[i ++] = (control & _L4_XCHG_REGS_SET_USER_HANDLE) ? 'u' : '-';
  string[i ++] = (control & _L4_XCHG_REGS_SET_FLAGS) ? 'f' : '-';
  string[i ++] = (control & _L4_XCHG_REGS_SET_IP) ? 'i' : '-';
  string[i ++] = (control & _L4_XCHG_REGS_SET_SP) ? 's' : '-';
  string[i ++] = (control & _L4_XCHG_REGS_CANCEL_SEND) ? 'S' : '-';
  string[i ++] = (control & _L4_XCHG_REGS_CANCEL_RECV) ? 'R' : '-';
  string[i ++] = (control & _L4_XCHG_REGS_HALT) ? 'H' : '-';
  string[i] = 0;
}

error_t
thread_exregs (struct activity *principal,
	       struct thread *thread, l4_word_t control,
	       struct cap *aspace,
	       l4_word_t flags, struct cap_properties properties,
	       struct cap *activity,
	       struct cap *exception_page,
	       l4_word_t *sp, l4_word_t *ip,
	       l4_word_t *eflags, l4_word_t *user_handle,
	       struct cap *aspace_out, struct cap *activity_out,
	       struct cap *exception_page_out)
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
    cap_copy (principal,
	      ADDR_VOID, aspace_out, ADDR_VOID,
	      ADDR_VOID, thread->aspace, ADDR_VOID);

  if ((control & HURD_EXREGS_SET_ASPACE))
    cap_copy_x (principal,
		ADDR_VOID, &thread->aspace, ADDR_VOID,
		ADDR_VOID, *aspace, ADDR_VOID,
		flags, properties);

  if ((control & HURD_EXREGS_GET_REGS) && activity_out)
    cap_copy (principal,
	      ADDR_VOID, activity_out, ADDR_VOID,
	      ADDR_VOID, thread->activity, ADDR_VOID);

  if ((control & HURD_EXREGS_SET_ACTIVITY))
    cap_copy (principal,
	      ADDR_VOID, &thread->activity, ADDR_VOID,
	      ADDR_VOID, *activity, ADDR_VOID);

  if ((control & HURD_EXREGS_GET_REGS) && exception_page_out)
    cap_copy (principal,
	      ADDR_VOID, exception_page_out, ADDR_VOID,
	      ADDR_VOID, thread->exception_page, ADDR_VOID);

  if ((control & HURD_EXREGS_SET_EXCEPTION_PAGE))
    cap_copy (principal,
	      ADDR_VOID, &thread->exception_page, ADDR_VOID,
	      ADDR_VOID, *exception_page, ADDR_VOID);

  if (thread->commissioned)
    {
      l4_thread_id_t tid = thread->tid;

      /* Clear hurd specific bits so that l4_exchange_registers works.  */
      control = control & ~(HURD_EXREGS_SET_ACTIVITY | HURD_EXREGS_SET_ASPACE);

      do_debug (4)
	{
	  char string[33];
	  control_to_string (control, string);
	  debug (0, "Calling exregs on %x.%x with control: %s (%x)",
		 l4_thread_no (tid), l4_version (tid),
		 string, control);
	}

      l4_word_t dummy = 0;
      l4_thread_id_t targ = tid;
      _L4_exchange_registers (&targ,
			      &control, sp, ip, eflags,
			      user_handle, &dummy);
      if (targ == l4_nilthread)
	/* XXX: This can happen if the user changes a thread's pager.
	   We could change it back.  */
	{
	  int err = l4_error_code ();
	  debug (1, "Failed to exregs %x.%x: %s (%d)",
		 l4_thread_no (tid), l4_version (tid),
		 l4_strerror (err), err);
	  return EINVAL;
	}

      do_debug (4)
	{
	  char string[33];
	  control_to_string (control, string);
	  debug (0, "exregs on %x.%x returned control: %s (%x)",
		 l4_thread_no (tid), l4_version (tid),
		 string, control);
	}
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
	  if (! cap_types_compatible (desc->type, cap_activity))
	    {
	      debug (1, "Thread not schedulable: activity slot contains a %s",
		     cap_type_string (desc->type));
	      return 0;
	    }

	  thread_commission (thread);
	  debug (4, "Starting thread %x.%x",
		 l4_thread_no (thread->tid), l4_version (thread->tid));

	  l4_thread_id_t tid = thread->tid;

	  l4_word_t c = _L4_XCHG_REGS_SET_SP | _L4_XCHG_REGS_SET_IP
	    | _L4_XCHG_REGS_SET_FLAGS | _L4_XCHG_REGS_SET_USER_HANDLE
	    | _L4_XCHG_REGS_SET_HALT;

	  if ((control & HURD_EXREGS_STOP) == HURD_EXREGS_STOP)
	    c |= _L4_XCHG_REGS_HALT;
	  if ((control & HURD_EXREGS_ABORT_SEND) == HURD_EXREGS_ABORT_SEND)
	    c |= _L4_XCHG_REGS_CANCEL_SEND;
	  if ((control & HURD_EXREGS_ABORT_RECEIVE)
	      == HURD_EXREGS_ABORT_RECEIVE)
	    c |= _L4_XCHG_REGS_CANCEL_RECV;

	  l4_thread_id_t targ = tid;
	  l4_word_t sp = thread->sp;
	  l4_word_t ip = thread->ip;
	  l4_word_t eflags = thread->eflags;
	  l4_word_t user_handle = thread->user_handle;
	  l4_word_t dummy = 0;

	  do_debug (4)
	    {
	      char string[33];
	      control_to_string (c, string);
	      debug (0, "Calling exregs on %x.%x with control: %s (%x)",
		     l4_thread_no (tid), l4_version (tid),
		     string, c);
	    }
	  _L4_exchange_registers (&targ, &c,
				  &sp, &ip, &eflags, &user_handle,
				  &dummy);
	  if (targ == l4_nilthread)
	    /* XXX: This can happen if the user changes a thread's
	       pager.  We could change it back.  */
	    {
	      int err = l4_error_code ();
	      debug (1, "Failed to exregs %x.%x: %s (%d)",
		     l4_thread_no (tid), l4_version (tid),
		     l4_strerror (err), err);
	      return EINVAL;
	    }
	  do_debug (4)
	    {
	      char string[33];
	      control_to_string (c, string);
	      debug (0, "exregs on %x.%x returned control: %s (%x)",
		     l4_thread_no (tid), l4_version (tid),
		     string, control);
	    }
	}
    }

  return 0;
}

void
thread_raise_exception (struct activity *activity,
			struct thread *thread,
			l4_msg_t *msg)
{
  struct object *page = cap_to_object (activity, &thread->exception_page);
  if (! page)
    {
      debug (1, "Malformed thread: no exception page");
      return;
    }

  if (object_type (page) != cap_page)
    {
      debug (1, "Malformed thread: exception page slot contains a %s, "
	     "not a cap_page",
	     cap_type_string (object_type (page)));
      return;
    }

  struct exception_page *exception_page = (struct exception_page *) page;

  if (exception_page->activated_mode)
    {
      l4_word_t c = _L4_XCHG_REGS_DELIVER;
      l4_thread_id_t targ = thread->tid;
      l4_word_t sp = 0;
      l4_word_t ip = 0;
      l4_word_t dummy = 0;
      _L4_exchange_registers (&targ, &c,
			      &sp, &ip, &dummy, &dummy, &dummy);

      debug (1, "Deferring exception delivery: thread in activated mode!"
	     "(sp: %x, ip: %x)", sp, ip);

      /* XXX: Sure, we could note that an exception is pending but we
	 need to queue the event.  */
      // exception_page->pending_message = 1;

      return;
    }

  /* Copy the message.  */
  memcpy (&exception_page->exception, msg,
	  (1 + l4_untyped_words (l4_msg_msg_tag (*msg))) * sizeof (l4_word_t));

  l4_word_t c = HURD_EXREGS_STOP | _L4_XCHG_REGS_DELIVER
    | _L4_XCHG_REGS_CANCEL_SEND | _L4_XCHG_REGS_CANCEL_RECV;
  do_debug (4)
    {
      char string[33];
      control_to_string (c, string);
      debug (0, "Calling exregs on %x.%x with control: %s (%x)",
	     l4_thread_no (thread->tid), l4_version (thread->tid),
	     string, c);
    }
  l4_thread_id_t targ = thread->tid;
  l4_word_t sp = 0;
  l4_word_t ip = 0;
  l4_word_t dummy = 0;
  _L4_exchange_registers (&targ, &c,
			  &sp, &ip, &dummy, &dummy, &dummy);
  if (targ == l4_nilthread)
    /* XXX: This can happen if the user changes a thread's
       pager.  We could change it back.  */
    {
      int err = l4_error_code ();
      debug (1, "Failed to exregs %x.%x: %s (%d)",
	     l4_thread_no (thread->tid), l4_version (thread->tid),
	     l4_strerror (err), err);
      return;
    }
  do_debug (4)
    {
      char string[33];
      control_to_string (c, string);
      debug (0, "exregs on %x.%x returned control: %s (%x)",
	     l4_thread_no (thread->tid), l4_version (thread->tid),
	     string, c);
    }

  exception_page->saved_thread_state = c;

  exception_page->activated_mode = 1;

  if (exception_page->exception_handler_ip <= ip
      && ip < exception_page->exception_handler_end)
    /* Thread is transitioning.  Don't save sp and ip.  */
    {
      debug (1, "Fault while interrupt in transition!");
      exception_page->interrupt_in_transition = 1;
    }
  else
    {
      exception_page->interrupt_in_transition = 0;
      exception_page->saved_sp = sp;
      exception_page->saved_ip = ip;
    }

  c = HURD_EXREGS_START | _L4_XCHG_REGS_SET_SP | _L4_XCHG_REGS_SET_IP;
  sp = exception_page->exception_handler_sp;
  ip = exception_page->exception_handler_ip;
  targ = thread->tid;
  do_debug (4)
    {
      char string[33];
      control_to_string (c, string);
      debug (0, "Calling exregs on %x.%x with control: %s (%x)",
	     l4_thread_no (thread->tid), l4_version (thread->tid),
	     string, c);
    }
  _L4_exchange_registers (&targ, &c,
			  &sp, &ip,
			  &dummy, &dummy, &dummy);
  if (targ == l4_nilthread)
    /* XXX: This can happen if the user changes a thread's
       pager.  We could change it back.  */
    {
      int err = l4_error_code ();
      debug (1, "Failed to exregs %x.%x: %s (%d)",
	     l4_thread_no (thread->tid), l4_version (thread->tid),
	     l4_strerror (err), err);
      return;
    }
  do_debug (4)
    {
      char string[33];
      control_to_string (c, string);
      debug (0, "exregs on %x.%x returned control: %s (%x)",
	     l4_thread_no (thread->tid), l4_version (thread->tid),
	     string, c);
    }
}
