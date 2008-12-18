/* thread.h - Thread definitions.
   Copyright (C) 2007, 2008 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

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

#ifndef __have_activation_frame
# define __have_activation_frame

# include <stdint.h>

# ifdef USE_L4
#  include <l4/ipc.h>
# endif

struct hurd_message_buffer;

struct activation_frame
{
  /* **** ia32-exception-entry.S silently depends on the layout of
     this structure up to and including the next field ****  */
#ifdef i386
  union
  {
    uintptr_t regs[10];
    struct
    {
      uintptr_t eax;
      uintptr_t ecx;
      uintptr_t edx;
      uintptr_t eflags;
      uintptr_t eip;
      uintptr_t ebx;
      uintptr_t edi;
      uintptr_t esi;
      uintptr_t ebp;
      uintptr_t esp;
    };
  };
#else
# error Not ported to this architecture!
#endif
  /* The base of the stack to use when running
     hurd_activation_handler_normal.  If NULL, then the interrupted
     stack is used.  */
  void *normal_mode_stack;

  struct activation_frame *next;
  struct activation_frame *prev;

  struct hurd_message_buffer *message_buffer;

#ifdef USE_L4
  /* We need to save parts of the UTCB.  */
  l4_word_t saved_sender;
  l4_word_t saved_receiver;
  l4_word_t saved_timeout;
  l4_word_t saved_error_code;
  l4_word_t saved_flags;
  l4_word_t saved_br0;
  l4_msg_t saved_message;
#endif

#define ACTIVATION_FRAME_CANARY 0x10ADAB1E
  uintptr_t canary;
};
#endif

#if defined (__need_activation_frame)
# undef __need_activation_frame
#else

#ifndef _HURD_THREAD_H
#define _HURD_THREAD_H 1

#include <viengoos/thread.h>

#include <stdint.h>
#include <hurd/types.h>
#include <viengoos/addr.h>
#include <viengoos/addr-trans.h>
#include <viengoos/cap.h>
#include <viengoos/messenger.h>
#include <setjmp.h>

/* The user thread control block.  */
struct hurd_utcb
{
  struct vg_utcb vg;

  /* Top of the activation frame stack (i.e., the active
     activation).  */
  struct activation_frame *activation_stack;
  /* The bottom of the activation stack.  */
  struct activation_frame *activation_stack_bottom;

  /* The CRC protects the above fields by checking for
     modification, which can happen if a call back function uses
     too much stack.  The fields following crc are not protected
     by the crc as they are expected to be changed by the
     activation handler.  */

  uintptr_t crc;

  /* The exception buffer.  */
  struct hurd_message_buffer *exception_buffer;
  /* The current extant IPC.  */
  struct hurd_message_buffer *extant_message;

  struct hurd_fault_catcher *catchers;

  /* The alternate activation stack.  */
  void *alternate_stack;
  bool alternate_stack_inuse;

  vg_thread_id_t tid;

#define UTCB_CANARY0 0xCA17A1
#define UTCB_CANARY1 0xDEADB15D
  uintptr_t canary0;
  uintptr_t canary1;
};

/* Return the calling thread's UTCB.  Threading libraries should set
   this to their own implementation once they are up and running.  */
extern struct hurd_utcb *(*hurd_utcb) (void);

/* Initializes the activation handler to allow receiving IPCs (but
   does not handle other faults).  This must be called exactly once
   before any IPCs are sent.  */
extern void hurd_activation_handler_init_early (void);

/* Initialize the activation handler.  This must be called after the
   storage sub-system has been initialized.  At this point, the
   activation handler is able to handle exceptions.  */
extern void hurd_activation_handler_init (void);


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


/* Cause the activation frame to assume the state of the long jump
   buffer BUF.  If SET_RET is true, the normal function return value
   is set to RET.  */
extern void hurd_activation_frame_longjmp (struct activation_frame *af,
					   jmp_buf buf,
					   bool set_ret, int ret);


/* Dump the activation stack to stdout.  */
extern void hurd_activation_stack_dump (void);

struct hurd_fault_catcher
{
#define HURD_FAULT_CATCHER_MAGIC 0xb01dface
  uintptr_t magic;

  /* Start of the region to watch.  */
  uintptr_t start;
  /* Length of the region in bytes.  */
  uintptr_t len;
  /* The callback.  Return true to continue execute.  False to throw a
     SIGSEGV.  */
  bool (*callback) (struct activation_frame *activation_frame,
		    uintptr_t fault);

  struct hurd_fault_catcher *next;
  struct hurd_fault_catcher **prevp;
};

/* Register a fatch catch handler.  */
extern void hurd_fault_catcher_register (struct hurd_fault_catcher *catcher);

/* Unregister a fault catch handler.  */
extern void hurd_fault_catcher_unregister (struct hurd_fault_catcher *catcher);

static inline vg_thread_id_t
hurd_myself (void)
{
  struct hurd_utcb *utcb = hurd_utcb ();

  return utcb->tid;
}

#endif /* _HURD_THREAD_H */
#endif /* __need_activation_frame */
