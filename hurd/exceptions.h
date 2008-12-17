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

#ifndef ASM

#include <stdint.h>
#include <viengoos/cap.h>
#include <viengoos/thread.h>
#include <hurd/error.h>
#include <l4/space.h>

#define RPC_STUB_PREFIX activation
#define RPC_ID_PREFIX ACTIVATION
#include <viengoos/rpc.h>

/* Activation message ids.  */
enum
  {
    ACTIVATION_fault = 10,
  };

/* Return a string corresponding to a message id.  */
static inline const char *
activation_method_id_string (uintptr_t id)
{
  switch (id)
    {
    case ACTIVATION_fault:
      return "fault";
    default:
      return "unknown";
    }
}

struct activation_fault_info
{
  union
  {
    struct
    {
      /* Type of access.  */
      uintptr_t access: 3;
      /* Type of object that was attempting to be accessed.  */
      uintptr_t type : CAP_TYPE_BITS;
      /* Whether the page was discarded.  */
      uintptr_t discarded : 1;
    };
    uintptr_t raw;
  };
};

#define ACTIVATION_FAULT_INFO_FMT "%c%c%c %s%s"
#define ACTIVATION_FAULT_INFO_PRINTF(info)		\
  ((info).access & L4_FPAGE_READABLE ? 'r' : '~'),	\
  ((info).access & L4_FPAGE_WRITABLE ? 'w' : '~'),	\
  ((info).access & L4_FPAGE_EXECUTABLE ? 'x' : '~'),	\
  cap_type_string ((info).type),			\
  (info.discarded) ? " discarded" : ""

/* Raise a fault at address FAULT_ADDRESS.  If IP is not 0, then IP is
   the value of the IP of the faulting thread at the time of the fault
   and SP the value of the stack pointer at the time of the fault.  */
RPC (fault, 4, 0, 0,
     addr_t, fault_address, uintptr_t, sp, uintptr_t, ip,
     struct activation_fault_info, activation_fault_info)

#undef RPC_STUB_PREFIX
#undef RPC_ID_PREFIX

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
extern error_t hurd_activation_state_alloc (addr_t thread,
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

#endif /* !ASM */

#endif
