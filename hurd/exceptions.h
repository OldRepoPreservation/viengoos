/* activations.h - Activation handling definitions.
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

#ifndef _HURD_ACTIVATIONS_H
#define _HURD_ACTIVATIONS_H 1

#include <hurd/stddef.h>

#include <stdint.h>
#include <viengoos/addr.h>
#include <viengoos/thread.h>
#include <hurd/error.h>

/* Initializes the activation handler to allow receiving IPCs (but
   does not handle other faults).  This must be called exactly once
   before any IPCs are sent.  */
extern void hurd_activation_handler_init_early (void);

/* Initialize the activation handler.  This must be called after the
   storage sub-system has been initialized.  At this point, the
   activation handler is able to handle exceptions.  */
extern void hurd_activation_handler_init (void);


/* Return the calling thread's UTCB.  Threading libraries should set
   this to their own implementation once they are up and running.  */
extern struct hurd_utcb *(*hurd_utcb) (void);

/* Allocate a utcb buffer and associated data structures (including an
   exception messenger) for the thread THEAD (which must already exist
   but should not be running).  Installs the UTCB and exception
   messenger in the thread object.  Returns the new UTCB in *UTCB.
   Returns 0 on success, otherwise an error code.  */
extern error_t hurd_activation_state_alloc (vg_addr_t thread,
					    struct hurd_utcb **utcb);

/* Release the state allocated by hurd_activation_state_alloc.  May
   not be called by a thread on its own UTCB!  */
extern void hurd_activation_state_free (struct hurd_utcb *utcb);


/* When a thread causes an activation, the kernel invokes the thread's
   activation handler.  This points to the low-level activation handler,
   which invokes activation_handler_activated.  (It is passed a pointer
   to the utcb.)

   This function must determine how to continue.  It may, but need
   not, immediately handle the activation.  The problem with handling
   an activation immediately is that this function runs on the
   activation handler's tiny stack and it runs in activated mode.  The
   latter means that it may not fault (which generally precludes
   accessing any dynamically allocated storage) or even properly send
   IPC (as it has no easy way to determine when the IPC has been
   received and when a reply is available--this information is
   delivered by activations!). 

   To allow an easy transition to another function in normal-mode, if
   the function returns an activation_frame, then the activation
   handler will call hurd_activation_handler_normal passing it that
   argument.  This function runs in normal mode and on the normal
   stack.  When this function returns, the interrupted state is
   restored.  */
extern struct activation_frame *hurd_activation_handler_activated
  (struct hurd_utcb *utcb);

extern void hurd_activation_handler_normal
  (struct activation_frame *activation_frame, struct hurd_utcb *utcb);


/* The first instruction of activation handler dispatcher.  */
extern char hurd_activation_handler_entry;
/* The instruction immediately following the last instruction of the
   activation handler dispatcher.  */
extern char hurd_activation_handler_end;


/* Register the current extant IPC.  */
extern void hurd_activation_message_register (struct hurd_message_buffer *mb);

/* Unregister the current extant IPC.  This is normally done
   automatically when a reply is receive.  However, if the IPC is
   aborted, then this function must be called before the next IPC may
   be sent.  */
extern void hurd_activation_message_unregister (struct hurd_message_buffer *mb);

/* Dump the activation stack to stdout.  */
extern void hurd_activation_stack_dump (void);

#endif
