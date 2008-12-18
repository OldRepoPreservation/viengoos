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
#include <viengoos/cap.h>
#include <viengoos/thread.h>

/* Forward.  */
struct activity;

struct thread
{
  /* User accessible fields.  */

  /* Address space.  */
  struct vg_cap aspace;

  /* The current associated activity.  (Not the activity out of which
     this thread's storage is allocated!)  */
  struct vg_cap activity;

  /* A capability designating a messenger to which to deliver
     exceptions.  */
  struct vg_cap exception_messenger;

  /* A capability the page that contains the thread's UTCB.  */
  struct vg_cap utcb;

  /* Non-user-accessible fields.  */

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

  struct vg_object_name name;
};

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
			      struct thread *thread, uintptr_t control,
			      struct vg_cap aspace,
			      uintptr_t flags, struct vg_cap_properties properties,
			      struct vg_cap activity,
			      struct vg_cap utcb,
			      struct vg_cap exception_messenger,
			      uintptr_t *sp, uintptr_t *ip,
			      uintptr_t *eflags, uintptr_t *user_handle);

/* Deliver the message carried by messenger MESSENGER to thread
   thread.  If thread is not activated, activate the thread.  Returns
   whether the message was delivered or the messenger was enqueued on
   the thread.  */
extern bool thread_activate (struct activity *activity,
			     struct thread *thread,
			     struct messenger *messenger,
			     bool may_block);

/* Send thread THREAD's exception messenger the exception described by
   MESSAGE.  If this would block, silently discards MESSAGE.  */
extern void thread_raise_exception (struct activity *activity,
				    struct thread *thread,
				    struct vg_message *message);

/* Deliver a pending message, if any and if possible.  */
extern void thread_deliver_pending (struct activity *activity,
				    struct thread *thread);

/* Given the L4 thread id THREADID, find the associated thread.  */
extern struct thread *thread_lookup (l4_thread_id_t threadid);

#endif
