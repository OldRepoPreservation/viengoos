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
  int tid = l4_thread_no (threadid);
  struct thread *thread = hurd_ihash_find (&tid_to_thread, tid);
  if (! thread)
    debug (1, "(%x.%x) => NULL", tid, l4_version (threadid));
  return thread;
}

void
thread_create_in (struct activity *activity,
		  struct thread *thread)
{
  /* Allocate a thread id.  */
  /* Find the next free thread id starting at thread_id_next.  */
  int tid = bit_alloc (thread_ids, THREAD_IDS_MAX / 8, thread_id_next);
  if (tid == -1)
    panic ("No thread ids left!");
  tid += THREAD_ID_BASE;
  debug (2, "Allocated thread id 0x%x", tid);
  thread_id_next = (tid + 1) % THREAD_IDS_MAX;
  /* We don't assign any semantic meaning to the version field.  We
     use a version of 1 (0 is not legal for global thread ids).  */
  thread->tid = l4_global_id (tid, 1);

  /* Set the initial activity to ACTIVITY.  */
  thread->activity = object_to_cap ((struct object *) activity);

  bool had_value;
  error_t err = hurd_ihash_replace (&tid_to_thread, tid, thread,
				    &had_value, NULL);
  assert (err == 0);
  assert (had_value == false);
}

error_t
thread_create (struct activity *activity,
	       struct thread *caller,
	       addr_t faddr, l4_word_t index,
	       addr_t taddr,
	       struct thread **threadp)
{
  if (! (0 <= index && index < FOLIO_OBJECTS))
    return EINVAL;

  /* Find the folio to use.  */
  struct cap folio_cap = object_lookup_rel (activity, &caller->aspace,
					    faddr, cap_folio, NULL);
  if (folio_cap.type == cap_void)
    return ENOENT;
  struct object *folio = cap_to_object (activity, &folio_cap);
  if (! folio)
    return ENOENT;

  /* And the thread capability slot.  */
  bool writable;
  struct cap *tcap = slot_lookup_rel (activity, &caller->aspace, taddr,
				      -1, &writable);
  if (! tcap)
    {
      debug (1, "No capability at 0x%llx/%d",
	     addr_prefix (taddr), addr_depth (taddr));
      return ENOENT;
    }
  if (! writable)
    {
      debug (1, "No permission to store at 0x%llx/%d",
	     addr_prefix (taddr), addr_depth (taddr));
      return EPERM;
    }

  /* Allocate the page from the folio.  */
  struct object *o;
  folio_object_alloc (activity, (struct folio *) folio, index,
		      cap_thread, &o);
  struct thread *thread;
  *threadp = thread = (struct thread *) o;

  thread_create_in (activity, thread);
  return 0;
}


void
thread_destroy (struct activity *activity, struct thread *thread)
{
  /* Free the thread id.  */
  bit_dealloc (thread_ids,
	       l4_thread_no (thread->tid) - THREAD_ID_BASE);

  int removed = hurd_ihash_remove (&tid_to_thread, thread->tid);
  assert (removed == 0);

  object_free (activity, (struct object *) thread);
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

  /* Create the thread.  */
  l4_word_t ret;
  ret = l4_thread_control (thread->tid, thread->tid,
			   l4_myself (), l4_nilthread, (void *) -1);
  if (! ret)
    panic ("Could not create initial thread (id=%x.%x): %s",
	   l4_thread_no (thread->tid), l4_version (thread->tid),
	   l4_strerror (l4_error_code ()));

  l4_word_t control;
  ret = l4_space_control (thread->tid, l4_nilthread,
			  l4_fpage_log2 (KIP_BASE,
					 l4_kip_area_size_log2 ()),
			  l4_fpage (UTCB_AREA_BASE, UTCB_AREA_SIZE),
			  l4_anythread, &control);
  if (! ret)
    panic ("Could not create address space: %s",
	   l4_strerror (l4_error_code ()));

  ret = l4_thread_control (thread->tid, thread->tid,
			   l4_nilthread,
			   l4_myself (),
			   (void *) UTCB_AREA_BASE);
  if (! ret)
    panic ("Failed to create thread: %s", l4_strerror (l4_error_code ()));

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

  /* XXX: Save the register state!  (See comment above for the
     plan.)  */

  /* Free the thread.  */
  l4_word_t ret;
  ret = l4_thread_control (thread->tid, l4_nilthread,
			   l4_nilthread, l4_nilthread, (void *) -1);
  if (! ret)
    panic ("Failed to delete thread %d",
	   l4_thread_no (thread->tid));

  thread->commissioned = 0;
}

error_t
thread_send_sp_ip (struct activity *activity,
		   struct thread *caller, addr_t taddr,
		   l4_word_t sp, l4_word_t ip)
{
  struct cap cap = object_lookup_rel (activity, &caller->aspace, taddr,
				      cap_thread, NULL);
  if (cap.type == cap_void)
    return ENOENT;
  struct object *object = cap_to_object (activity, &cap);
  if (! object)
    return ENOENT;
  struct thread *thread = (struct thread *) object;
  
  /* After this point nothing may block or fail (of course, user
     errors are okay).  */

  if (thread->commissioned)
    return EINVAL;

  thread->sp = sp;
  thread->ip = ip;

  thread_commission (thread);

  return 0;
}
