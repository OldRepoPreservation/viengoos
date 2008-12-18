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
#include <viengoos/cap.h>
#include <hurd/stddef.h>
#include <viengoos/thread.h>
#include <bit-array.h>
#include <backtrace.h>

#include "cap.h"
#include "object.h"
#include "thread.h"
#include "activity.h"
#include "zalloc.h"
#include "messenger.h"
#include <hurd/trace.h>

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
	   object_to_object_desc ((struct vg_object *) thread)->oid, thread,
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
	panic ("Failed to allocate memory for thread hash.");

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
	 object_to_object_desc ((struct vg_object *) thread)->oid, thread);

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
	       struct thread *thread, uintptr_t control,
	       struct vg_cap aspace,
	       uintptr_t flags, struct vg_cap_properties properties,
	       struct vg_cap activity,
	       struct vg_cap utcb,
	       struct vg_cap exception_messenger,
	       uintptr_t *sp, uintptr_t *ip,
	       uintptr_t *eflags, uintptr_t *user_handle)
{
  if ((control & ~(VG_EXREGS_SET_REGS
		   | VG_EXREGS_GET_REGS
		   | VG_EXREGS_START
		   | VG_EXREGS_STOP
		   | VG_EXREGS_ABORT_IPC)))
    {
      debug (1, "Control word contains invalid bits");
      return EINVAL;
    }

  if ((control & VG_EXREGS_SET_ASPACE))
    vg_cap_copy_x (principal,
		   VG_ADDR_VOID, &thread->aspace, VG_ADDR_VOID,
		   VG_ADDR_VOID, aspace, VG_ADDR_VOID,
		   flags, properties);

  if ((control & VG_EXREGS_SET_ACTIVITY))
    vg_cap_copy_simple
      (principal,
       VG_ADDR_VOID, &thread->activity, VG_ADDR_VOID,
       VG_ADDR_VOID, activity, VG_ADDR_VOID);

  if ((control & VG_EXREGS_SET_UTCB))
    vg_cap_copy_simple
      (principal,
       VG_ADDR_VOID, &thread->utcb, VG_ADDR_VOID,
       VG_ADDR_VOID, utcb, VG_ADDR_VOID);

  if ((control & VG_EXREGS_SET_EXCEPTION_MESSENGER))
    vg_cap_copy_simple
      (principal,
       VG_ADDR_VOID, &thread->exception_messenger, VG_ADDR_VOID,
       VG_ADDR_VOID, exception_messenger, VG_ADDR_VOID);

  if (thread->commissioned)
    {
      l4_thread_id_t tid = thread->tid;

      /* Clear hurd specific bits so that l4_exchange_registers works.  */
      control = control & ~(VG_EXREGS_SET_ACTIVITY | VG_EXREGS_SET_ASPACE);

      do_debug (4)
	{
	  char string[33];
	  control_to_string (control, string);
	  debug (0, "Calling exregs on %x with control: %s (%x)",
		 tid, string, control);
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
	  debug (0, "Failed to exregs %x: %s (%d)",
		 tid, l4_strerror (err), err);
	  return EINVAL;
	}

      do_debug (4)
	{
	  char string[33];
	  control_to_string (control, string);
	  debug (0, "exregs on %x returned control: %s (%x)",
		 tid, string, control);
	}
    }
  else
    {
      l4_word_t t = thread->sp;
      if ((control & VG_EXREGS_SET_SP))
	thread->sp = *sp;
      if ((control & VG_EXREGS_GET_REGS))
	*sp = t;

      t = thread->ip;
      if ((control & VG_EXREGS_SET_IP))
	thread->ip = *ip;
      if ((control & VG_EXREGS_GET_REGS))
	*ip = t;

      t = thread->eflags;
      if ((control & VG_EXREGS_SET_EFLAGS))
	thread->eflags = *eflags;
      if ((control & VG_EXREGS_GET_REGS))
	*eflags = t;

      t = thread->user_handle;
      if ((control & VG_EXREGS_SET_USER_HANDLE))
	thread->user_handle = *user_handle;
      if ((control & VG_EXREGS_GET_REGS))
	*user_handle = t;

      if ((control & VG_EXREGS_START) == VG_EXREGS_START)
	{
	  struct vg_object *a = vg_cap_to_object (principal, &thread->activity);
	  if (! a)
	    {
	      debug (0, "Thread not schedulable: no activity");
	      return 0;
	    }

	  struct object_desc *desc = object_to_object_desc (a);
	  if (! vg_cap_types_compatible (desc->type, vg_cap_activity))
	    {
	      debug (0, "Thread not schedulable: activity slot contains a %s",
		     vg_cap_type_string (desc->type));
	      return 0;
	    }

	  thread_commission (thread);
	  debug (4, "Starting thread %x",
		 thread->tid);

	  l4_thread_id_t tid = thread->tid;

	  l4_word_t c = _L4_XCHG_REGS_SET_SP | _L4_XCHG_REGS_SET_IP
	    | _L4_XCHG_REGS_SET_FLAGS | _L4_XCHG_REGS_SET_USER_HANDLE
	    | _L4_XCHG_REGS_SET_HALT;

	  if ((control & VG_EXREGS_STOP) == VG_EXREGS_STOP)
	    c |= _L4_XCHG_REGS_HALT;
	  if ((control & VG_EXREGS_ABORT_SEND) == VG_EXREGS_ABORT_SEND)
	    c |= _L4_XCHG_REGS_CANCEL_SEND;
	  if ((control & VG_EXREGS_ABORT_RECEIVE)
	      == VG_EXREGS_ABORT_RECEIVE)
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
	      debug (0, "Calling exregs on %x with control: %s (%x)",
		     tid, string, c);
	    }
	  _L4_exchange_registers (&targ, &c,
				  &sp, &ip, &eflags, &user_handle,
				  &dummy);
	  if (targ == l4_nilthread)
	    /* XXX: This can happen if the user changes a thread's
	       pager.  We could change it back.  */
	    {
	      int err = l4_error_code ();
	      debug (0, "Failed to exregs %x: %s (%d)",
		     tid, l4_strerror (err), err);
	      return EINVAL;
	    }
	  do_debug (4)
	    {
	      char string[33];
	      control_to_string (c, string);
	      debug (0, "exregs on %x returned control: %s (%x)",
		     tid, string, control);
	    }
	}
    }

  return 0;
}

bool
thread_activate (struct activity *activity,
		 struct thread *thread,
		 struct messenger *messenger,
		 bool may_block)
{
  assert (messenger);
  assert (object_type ((struct vg_object *) messenger) == vg_cap_messenger);


  uintptr_t ip = 0;
  uintptr_t sp = 0;
  {
    uintptr_t c = _L4_XCHG_REGS_DELIVER;
    l4_thread_id_t targ = thread->tid;
    uintptr_t dummy = 0;
    _L4_exchange_registers (&targ, &c,
			    &sp, &ip, &dummy, &dummy, &dummy);
  }

  struct vg_utcb *utcb
    = (struct vg_utcb *) vg_cap_to_object (activity, &thread->utcb);
  if (! utcb)
    {
#ifndef NDEBUG
      extern struct trace_buffer rpc_trace;
      trace_buffer_dump (&rpc_trace, 0);
#endif

      do_debug (4)
	as_dump_from (activity, &thread->aspace, "");
      debug (0, "Malformed thread (%x): no utcb (ip: %x, sp: %x)",
	     thread->tid, ip, sp);
      return false;
    }

  if (object_type ((struct vg_object *) utcb) != vg_cap_page)
    {
      debug (0, "Malformed thread: utcb slot contains a %s, not a page",
	     vg_cap_type_string (object_type ((struct vg_object *) utcb)));
      return false;
    }

  if (utcb->activated_mode)
    {
      debug (0, "Deferring exception delivery: thread in activated mode!"
	     "(sp: %x, ip: %x)", sp, ip);

      if (! may_block)
	return false;

      object_wait_queue_enqueue (activity,
				 (struct vg_object *) thread, messenger);
      messenger->wait_reason = MESSENGER_WAIT_TRANSFER_MESSAGE;

      utcb->pending_message = 1;

      return true;
    }

  debug (5, "Activating %x (ip: %p; sp: %p)",
	 thread->tid, (void *) ip, (void *) sp);

  utcb->protected_payload = messenger->protected_payload;
  utcb->messenger_id = messenger->id;

  if (! messenger->out_of_band)
    {
      memcpy (utcb->inline_words, messenger->inline_words,
	      messenger->inline_word_count * sizeof (uintptr_t));
      memcpy (utcb->inline_caps, messenger->inline_caps,
	      messenger->inline_cap_count * sizeof (vg_addr_t));
      utcb->inline_word_count = messenger->inline_word_count;
      utcb->inline_cap_count = messenger->inline_cap_count;
    }

  l4_word_t c = VG_EXREGS_STOP | _L4_XCHG_REGS_DELIVER
    | _L4_XCHG_REGS_CANCEL_SEND | _L4_XCHG_REGS_CANCEL_RECV;
  do_debug (4)
    {
      char string[33];
      control_to_string (c, string);
      debug (0, "Calling exregs on %x with control: %s (%x)",
	     thread->tid, string, c);
    }
  l4_thread_id_t targ = thread->tid;
  sp = 0;
  ip = 0;
  l4_word_t dummy = 0;
  _L4_exchange_registers (&targ, &c,
			  &sp, &ip, &dummy, &dummy, &dummy);
  if (targ == l4_nilthread)
    /* XXX: This can happen if the user changes a thread's
       pager.  We could change it back.  */
    {
      int err = l4_error_code ();
      debug (0, "Failed to exregs %x: %s (%d)",
	     thread->tid, l4_strerror (err), err);
      return false;
    }
  do_debug (4)
    {
      char string[33];
      control_to_string (c, string);
      debug (0, "exregs on %x returned control: %s (%x)",
	     thread->tid, string, c);
    }

  utcb->activated_mode = 1;

  if (utcb->activation_handler_ip <= ip
      && ip < utcb->activation_handler_end)
    /* Thread is transitioning.  Don't save sp and ip.  */
    {
      debug (0, "Fault while interrupt in transition (ip: %x)!",
	     ip);
      utcb->interrupt_in_transition = 1;
    }
  else
    {
      utcb->interrupt_in_transition = 0;
      utcb->saved_sp = sp;
      utcb->saved_ip = ip;
    }

  c = VG_EXREGS_START | _L4_XCHG_REGS_SET_SP | _L4_XCHG_REGS_SET_IP;
  sp = utcb->activation_handler_sp;
  ip = utcb->activation_handler_ip;
  targ = thread->tid;
  do_debug (4)
    {
      char string[33];
      control_to_string (c, string);
      debug (0, "Calling exregs on %x with control: %s (%x)",
	     thread->tid, string, c);
    }
  _L4_exchange_registers (&targ, &c,
			  &sp, &ip,
			  &dummy, &dummy, &dummy);
  if (targ == l4_nilthread)
    /* XXX: This can happen if the user changes a thread's
       pager.  We could change it back.  */
    {
      int err = l4_error_code ();
      debug (0, "Failed to exregs %x: %s (%d)",
	     thread->tid, l4_strerror (err), err);
      return false;
    }
  do_debug (4)
    {
      char string[33];
      control_to_string (c, string);
      debug (0, "exregs on %x returned control: %s (%x)",
	     thread->tid, string, c);
    }

  return true;
}

void
thread_raise_exception (struct activity *activity,
			struct thread *thread,
			struct vg_message *message)
{
  struct messenger *handler
    = (struct messenger *) vg_cap_to_object (activity,
					  &thread->exception_messenger);
  if (! handler)
    {
      backtrace_print ();
      debug (0, "Thread %x has no exception handler.", thread->tid);
    }
  else if (object_type ((struct vg_object *) handler) != vg_cap_messenger)
    debug (0, "%s is not a valid exception handler.",
	   vg_cap_type_string (object_type ((struct vg_object *) handler)));
  else
    {
      if (! messenger_message_load (activity, handler, message))
	debug (0, "Failed to deliver exception to thread's exception handler.");
      return;
    }
}

void
thread_deliver_pending (struct activity *activity,
			struct thread *thread)
{
  struct vg_utcb *utcb
    = (struct vg_utcb *) vg_cap_to_object (activity, &thread->utcb);
  if (! utcb)
    {
      debug (0, "Malformed thread (%x): no utcb",
	     thread->tid);
      return;
    }

  if (object_type ((struct vg_object *) utcb) != vg_cap_page)
    {
      debug (0, "Malformed thread: utcb slot contains a %s, not a page",
	     vg_cap_type_string (object_type ((struct vg_object *) utcb)));
      return;
    }

  if (utcb->activated_mode)
    {
      debug (0, "Deferring exception delivery: thread in activated mode!");
      return;
    }


  struct messenger *m;
  object_wait_queue_for_each (activity, (struct vg_object *) thread, m)
    if (m->wait_reason == MESSENGER_WAIT_TRANSFER_MESSAGE)
      {
	if (thread_activate (activity, thread, m, false))
	  object_wait_queue_unlink (activity, m);

	return;
      }

  utcb->pending_message = 0;
}
