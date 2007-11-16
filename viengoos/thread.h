/* thread.h - Thread object interface.
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

#ifndef RM_THREAD_H
#define RM_THREAD_H

#include <l4.h>
#include <errno.h>

#include "cap.h"

/* Forward.  */
struct folio;
struct activity;

/* Number of capability slots at the start of the thread
   structure.  */
enum
  {
    THREAD_SLOTS = 2,
  };

struct thread
{
  /* Address space.  */
  struct cap aspace;

  /* The current associated activity.  (Not the activity out of which
     this thread's storage is allocated!)  */
  struct cap activity;

  /* Allocated thread id.  */
  l4_thread_id_t tid;

  /* XXX: Register state, blah, blah, blah.  */
  l4_word_t sp;
  l4_word_t ip;

  /* Debugging: whether the thread has been commissioned.  */
  int commissioned;

};

/* The hardwired base of the UTCB (2.5GB).  */
#define UTCB_AREA_BASE (0xA0000000)
/* The size of the UTCB.  */
#define UTCB_AREA_SIZE (l4_utcb_area_size ())
/* The hardwired base of the KIP.  */
#define KIP_BASE (UTCB_AREA_BASE + UTCB_AREA_SIZE)

/* Create a new thread.  Uses the object THREAD to store the thread
   information.  */
extern void thread_create_in (struct activity *activity,
			      struct thread *thread);

/* Create a new thread.  FOLIO designates a folio in CALLER's CSPACE.
   INDEX specifies which object in the folio to use for the new
   thread's storage.  Sets the thread's current activity to ACTIVITY.
   On success, a capability to this object is saved in the capability
   slot at address THREAD in CALLER's address space, the thread object
   is returned in *THREADP and 0 is returned.  Otherwise, an error
   code.  */
extern error_t thread_create (struct activity *activity,
			      struct thread *caller,
			      addr_t folio, l4_word_t index,
			      addr_t thread,
			      struct thread **threadp);

/* Destroy the thread object THREAD (and the accompanying thread).  */
extern void thread_destroy (struct activity *activity,
			    struct thread *thread);

/* Prepare the thread object THREAD to run.  (Called after bringing a
   thread object into core.)  */
extern void thread_commission (struct thread *thread);

/* Save any state of the thread THREAD and destroy any ephemeral
   resources.  (Called before sending the object to backing
   store.)  */
extern void thread_decommission (struct thread *thread);

/* Send a thread start message to the thread THREAD (in CALLER's
   address space).  This may be called at most once per thread.  If
   called multiple times, the results are undefined.  If thread is not
   decommissioned, returns EINVAL.  Commissions thread.  */
extern error_t thread_send_sp_ip (struct activity *activity,
				  struct thread *caller, addr_t thread,
				  l4_word_t sp, l4_word_t ip);

/* Given the L4 thread id THREADID, find the associated thread.  */
extern struct thread *thread_lookup (l4_thread_id_t threadid);

#endif
