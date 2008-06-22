/* thread.h - Thread object interface.
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

#ifndef RM_THREAD_H
#define RM_THREAD_H

#include <l4.h>
#include <errno.h>

#include "list.h"

/* Forward.  */
struct folio;
struct activity;

/* Number of capability slots at the start of the thread
   structure.  */
enum
  {
    THREAD_SLOTS = 3,
  };

enum
  {
    /* THREAD is blocked on an object wait for a futex.
       WAIT_REASON_ARG holds the byte offset in the object on which it
       is waiting.  */
    THREAD_WAIT_FUTEX,
    /* THREAD is blocked on an object waiting for the object to be
       destroyed.  */
    THREAD_WAIT_DESTROY,
    /* THREAD is blocked on an activity waiting for the stats for a
       particular period, which is stored in
       thread->wait_reason_arg.  */
    THREAD_WAIT_STATS,
  };

struct thread
{
  /* User accessible fields.  */

  /* Address space.  */
  struct cap aspace;

  /* The current associated activity.  (Not the activity out of which
     this thread's storage is allocated!)  */
  struct cap activity;

  /* Capability identifying a page to use to store exceptions.  */
  struct cap exception_page;

  /* Non-user accessible fields.  */

  /* Allocated thread id.  */
  l4_thread_id_t tid;

  /* XXX: Register state, blah, blah, blah.  */
  l4_word_t sp;
  l4_word_t ip;
  l4_word_t eflags;
  l4_word_t user_handle;

  /* Whether the thread data structure has been initialized.  */
  uint32_t init : 1;
  /* Whether the thread has been commissioned (a tid allocated).  */
  uint32_t commissioned : 1;

  /* Whether the object is attached to a wait queue.  (This is
     different from the value of folio_object_wait_queue_p which
     specifies if there are objects on this thread's wait queue.)  */
  bool wait_queue_p;

  /* Whether this thread is the head of the wait queue.  If so,
     WAIT_QUEUE.PREV designates the object.  */
  uint32_t wait_queue_head : 1;

  /* Whether this thread is the tail of the wait queue.  If so,
     WAIT_QUEUE.NEXT designates the object.  */
  uint32_t wait_queue_tail : 1;

  /* The event the thread is interested in.  */
  uint32_t wait_reason : 28;
  /* More information about the reason.  */
  uint32_t wait_reason_arg;

  /* The object the thread is waiting on.  Only meaningful if
     WAIT_QUEUE_P is true.  */
  struct
  {
    /* We don't need versioning as we automatically collect on object
       destruction.  */
    oid_t next;
    oid_t prev;
  } wait_queue;

#ifndef NDEBUG
  struct list_node futex_waiter_node;
#endif
};

#ifndef NDEBUG
LIST_CLASS(futex_waiter, struct thread, futex_waiter_node, true)
/* List of threads waiting on a futex.  */
extern struct futex_waiter_list futex_waiters;
#endif

/* The hardwired base of the UTCB (2.5GB).  */
#define UTCB_AREA_BASE (0xA0000000)
/* The size of the UTCB.  */
#define UTCB_AREA_SIZE (2 * l4_utcb_area_size ())
/* The hardwired base of the KIP.  */
#define KIP_BASE (UTCB_AREA_BASE + UTCB_AREA_SIZE)

/* Create a new thread.  Uses the object THREAD to store the thread
   information.  */
extern void thread_init (struct thread *thread);

/* Destroy the thread object THREAD (and the accompanying thread).  */
extern void thread_deinit (struct activity *activity,
			   struct thread *thread);

/* Prepare the thread object THREAD to run.  (Called after bringing a
   thread object into core.)  */
extern void thread_commission (struct thread *thread);

/* Save any state of the thread THREAD and destroy any ephemeral
   resources.  (Called before sending the object to backing
   store.)  */
extern void thread_decommission (struct thread *thread);

/* Perform an exregs on thread THREAD.  CONTROL, SP, IP, EFLAGS and
   USER_HANDLER are as per l4_exchange_regs, however, the caller may
   not set the pager.  */
extern error_t thread_exregs (struct activity *principal,
			      struct thread *thread, l4_word_t control,
			      struct cap *aspace,
			      l4_word_t flags, struct cap_properties properties,
			      struct cap *activity,
			      struct cap *exception_page,
			      l4_word_t *sp, l4_word_t *ip,
			      l4_word_t *eflags, l4_word_t *user_handle,
			      struct cap *aspace_out,
			      struct cap *activity_out,
			      struct cap *exception_page_out);

/* Send thread THREAD an exception.  */
extern void thread_raise_exception (struct activity *activity,
				    struct thread *thread,
				    l4_msg_t *msg);

/* Given the L4 thread id THREADID, find the associated thread.  */
extern struct thread *thread_lookup (l4_thread_id_t threadid);

#endif
