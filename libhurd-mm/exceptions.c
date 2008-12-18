/* exceptions.c - Exception handler implementation.
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

#include <hurd/startup.h>
#include <hurd/stddef.h>
#include <hurd/storage.h>
#include <hurd/thread.h>
#include <hurd/mm.h>
#include <viengoos/misc.h>
#include <hurd/slab.h>
#include <l4/thread.h>

#include <signal.h>
#include <string.h>
#include <backtrace.h>

#include "map.h"
#include "as.h"

void
hurd_fault_catcher_register (struct hurd_fault_catcher *catcher)
{
  struct hurd_utcb *utcb = hurd_utcb ();
  assert (utcb);
  assert (catcher);

  catcher->magic = HURD_FAULT_CATCHER_MAGIC;

  catcher->next = utcb->catchers;
  catcher->prevp = &utcb->catchers;

  utcb->catchers = catcher;
  if (catcher->next)
    catcher->next->prevp = &catcher->next;
}

void
hurd_fault_catcher_unregister (struct hurd_fault_catcher *catcher)
{
  assertx (catcher->magic == HURD_FAULT_CATCHER_MAGIC,
	   "%p", (void *) catcher->magic);
  catcher->magic = ~HURD_FAULT_CATCHER_MAGIC;

  *catcher->prevp = catcher->next;
  if (catcher->next)
    catcher->next->prevp = catcher->prevp;
}

extern struct hurd_startup_data *__hurd_startup_data;

void
hurd_activation_frame_longjmp (struct activation_frame *activation_frame,
			       jmp_buf buf, bool set_ret, int ret)
{
#ifdef i386
  /* XXX: Hack! Hack! This is customized for the newlib version!!!

     From newlib/newlib/libc/machine/i386/setjmp.S

     jmp_buf:
      eax ebx ecx edx esi edi ebp esp eip
      0   4   8   12  16  20  24  28  32
  */
  /* A cheap check to try and ensure we are using a newlib data
     structure.  */
  assert (sizeof (jmp_buf) == sizeof (uintptr_t) * 9);

  uintptr_t *regs = (uintptr_t *) buf;
  activation_frame->eax = *(regs ++);
  activation_frame->ebx = *(regs ++);
  activation_frame->ecx = *(regs ++);
  activation_frame->edx = *(regs ++);
  activation_frame->esi = *(regs ++);
  activation_frame->edi = *(regs ++);
  activation_frame->ebp = *(regs ++);
  activation_frame->esp = *(regs ++);
  activation_frame->eip = *(regs ++);

  /* The return value is stored in eax.  */
  if (set_ret)
    activation_frame->eax = ret;

#else
# warning Not ported to this architecture
#endif
}

static void
l4_utcb_state_save (struct activation_frame *activation_frame)
{
  uintptr_t *utcb = _L4_utcb ();

  activation_frame->saved_sender = utcb[_L4_UTCB_SENDER];
  activation_frame->saved_receiver = utcb[_L4_UTCB_RECEIVER];
  activation_frame->saved_timeout = utcb[_L4_UTCB_TIMEOUT];
  activation_frame->saved_error_code = utcb[_L4_UTCB_ERROR_CODE];
  activation_frame->saved_flags = utcb[_L4_UTCB_FLAGS];
  activation_frame->saved_br0 = utcb[_L4_UTCB_BR0];
  memcpy (&activation_frame->saved_message,
	  &utcb[_L4_UTCB_MR0], L4_NUM_MRS * sizeof (uintptr_t));
}

static void
l4_utcb_state_restore (struct activation_frame *activation_frame)
{
  uintptr_t *utcb = _L4_utcb ();

  utcb[_L4_UTCB_SENDER] = activation_frame->saved_sender;
  utcb[_L4_UTCB_RECEIVER] = activation_frame->saved_receiver;
  utcb[_L4_UTCB_TIMEOUT] = activation_frame->saved_timeout;
  utcb[_L4_UTCB_ERROR_CODE] = activation_frame->saved_error_code;
  utcb[_L4_UTCB_FLAGS] = activation_frame->saved_flags;
  utcb[_L4_UTCB_BR0] = activation_frame->saved_br0;
  memcpy (&utcb[_L4_UTCB_MR0], &activation_frame->saved_message,
	  L4_NUM_MRS * sizeof (uintptr_t));
}

/* Fetch any pending activation.  */
void
hurd_activation_fetch (void)
{
  debug (0, DEBUG_BOLD ("XXX"));

  /* Any reply will come in the form of a pending activation being
     delivered.  This RPC does not generate a response.  */
  error_t err = vg_thread_activation_collect_send (VG_ADDR_VOID, VG_ADDR_VOID,
						   VG_ADDR_VOID);
  if (err)
    panic ("Sending thread_activation_collect failed: %d", err);
}

void
hurd_activation_message_register (struct hurd_message_buffer *message_buffer)
{
  if (unlikely (! mm_init_done))
    return;

  struct hurd_utcb *utcb = hurd_utcb ();
  assert (utcb);
  assert (message_buffer);

  debug (5, "Registering %p (utcb: %p)", message_buffer, utcb);

  if (utcb->extant_message)
    panic ("Already have an extant message buffer!");

  utcb->extant_message = message_buffer;
  message_buffer->just_free = false;
  message_buffer->closure = NULL;
}

void
hurd_activation_message_unregister (struct hurd_message_buffer *message_buffer)
{
  if (unlikely (! mm_init_done))
    return;

  struct hurd_utcb *utcb = hurd_utcb ();
  assert (utcb);
  assert (message_buffer);
  assert (utcb->extant_message == message_buffer);
  utcb->extant_message = NULL;
}

/* Message buffers contain an activation frame.  Exceptions reuse
   message buffers and can be nested.  To avoid squashing the
   activation frame, we need to allocate  */

static error_t activation_frame_slab_alloc (void *, size_t, void **);
static error_t activation_frame_slab_dealloc (void *, void *, size_t);

static struct hurd_slab_space activation_frame_slab
  = HURD_SLAB_SPACE_INITIALIZER (struct activation_frame,
				 activation_frame_slab_alloc,
				 activation_frame_slab_dealloc,
				 NULL, NULL, NULL);

static error_t
activation_frame_slab_alloc (void *hook, size_t size, void **ptr)
{
  assert (size == PAGESIZE);

  struct storage storage = storage_alloc (meta_data_activity,
					  vg_cap_page, STORAGE_EPHEMERAL,
					  VG_OBJECT_POLICY_DEFAULT, VG_ADDR_VOID);
  *ptr = VG_ADDR_TO_PTR (vg_addr_extend (storage.addr, 0, PAGESIZE_LOG2));

  return 0;
}

static error_t
activation_frame_slab_dealloc (void *hook, void *buffer, size_t size)
{
  assert (size == PAGESIZE);

  vg_addr_t addr = vg_addr_chop (VG_PTR_TO_ADDR (buffer), PAGESIZE_LOG2);
  storage_free (addr, false);

  return 0;
}

static void
check_activation_frame_reserve (struct hurd_utcb *utcb)
{
  if (unlikely (! utcb->activation_stack
		|| ! utcb->activation_stack->prev))
    /* There are no activation frames in reserve.  Allocate one.  */
    {
      void *buffer;
      error_t err = hurd_slab_alloc (&activation_frame_slab, &buffer);
      if (err)
	panic ("Out of memory!");

      struct activation_frame *activation_frame = buffer;
      activation_frame->canary = ACTIVATION_FRAME_CANARY;

      activation_frame->prev = NULL;
      activation_frame->next = utcb->activation_stack;
      if (activation_frame->next)
	activation_frame->next->prev = activation_frame;

      if (! utcb->activation_stack_bottom)
	/* This is the first frame we've allocated.  */
	utcb->activation_stack_bottom = activation_frame;
    }
}

static struct activation_frame *
activation_frame_alloc (struct hurd_utcb *utcb)
{
  struct activation_frame *activation_frame;

  if (! utcb->activation_stack
      && utcb->activation_stack_bottom)
    /* The stack is empty but we have an available frame.  */
    {
      activation_frame = utcb->activation_stack_bottom;
      utcb->activation_stack = activation_frame;
    }
  else if (utcb->activation_stack
	   && utcb->activation_stack->prev)
    /* The stack is not empty and we have an available frame.  */
    {
      activation_frame = utcb->activation_stack->prev;
      utcb->activation_stack = activation_frame;
    }
  else
    /* We do not have an available frame.  */
    panic ("Activation frame reserve is empty.");

  return activation_frame;
}

void
hurd_activation_stack_dump (void)
{
  struct hurd_utcb *utcb = hurd_utcb ();

  int depth = 0;
  struct activation_frame *activation_frame;
  for (activation_frame = utcb->activation_stack;
       activation_frame;
       activation_frame = activation_frame->next)
    {
      depth ++;
      debug (0, "%d (%p): ip: %p, sp: %p, eax: %p, ebx: %p, ecx: %p, "
	     "edx: %p, edi: %p, esi: %p, ebp: %p, eflags: %p",
	     depth, activation_frame,
	     (void *) activation_frame->eip,
	     (void *) activation_frame->esp,
	     (void *) activation_frame->eax,
	     (void *) activation_frame->ebx,
	     (void *) activation_frame->ecx,
	     (void *) activation_frame->edx,
	     (void *) activation_frame->edi,
	     (void *) activation_frame->esi,
	     (void *) activation_frame->ebp,
	     (void *) activation_frame->eflags);

    }
}

void
hurd_activation_handler_normal (struct activation_frame *activation_frame,
				struct hurd_utcb *utcb)
{
  assert (utcb == hurd_utcb ());
  assert (activation_frame->canary == ACTIVATION_FRAME_CANARY);
  assert (utcb->activation_stack == activation_frame);

  do_debug (4)
    {
      static int calls;
      int call = ++ calls;

      int depth = 0;
      struct activation_frame *af;
      for (af = utcb->activation_stack; af; af = af->next)
	depth ++;

      debug (0, "Activation (%d; %d nested) (frame: %p, next: %p)",
	     call, depth, activation_frame, activation_frame->next);
      hurd_activation_stack_dump ();
    }

  struct hurd_message_buffer *mb = activation_frame->message_buffer;
  assert (mb->magic == HURD_MESSAGE_BUFFER_MAGIC);

  check_activation_frame_reserve (utcb);

  if (mb->closure)
    {
      debug (5, "Executing closure %p", mb->closure);
      mb->closure (mb);
    }
  else
    {
      debug (5, "Exception");

      assert (mb == utcb->exception_buffer);

      uintptr_t label = vg_message_word (mb->reply, 0);
      switch (label)
	{
	case VG_ACTIVATION_fault:
	  {
	    vg_addr_t fault;
	    uintptr_t ip;
	    uintptr_t sp;
	    struct vg_activation_fault_info info;

	    error_t err;
	    err = vg_activation_fault_send_unmarshal (mb->reply,
						      &fault, &sp, &ip, &info,
						      NULL);
	    if (err)
	      panic ("Failed to unmarshal exception: %d", err);

	    debug (5, "Fault at " VG_ADDR_FMT " (ip: %p, sp: %p, eax: %p, "
		   "ebx: %p, ecx: %p, edx: %p, edi: %p, esi: %p, ebp: %p, "
		   "eflags: %p)",
		   VG_ADDR_PRINTF (fault),
		   (void *) ip, (void *) sp,
		   (void *) activation_frame->eax,
		   (void *) activation_frame->ebx,
		   (void *) activation_frame->ecx,
		   (void *) activation_frame->edx,
		   (void *) activation_frame->edi,
		   (void *) activation_frame->esi,
		   (void *) activation_frame->ebp,
		   (void *) activation_frame->eflags);

	    extern l4_thread_id_t as_rwlock_owner;

	    bool r = false;
	    if (likely (as_rwlock_owner != l4_myself ()))
	      r = map_fault (fault, ip, info);
	    if (! r)
	      {
		uintptr_t f = (uintptr_t) VG_ADDR_TO_PTR (fault);
		struct hurd_fault_catcher *catcher;
		for (catcher = utcb->catchers; catcher; catcher = catcher->next)
		  {
		    assertx (catcher->magic == HURD_FAULT_CATCHER_MAGIC,
			     "Catcher %p has bad magic: %p",
			     catcher, (void *) catcher->magic);

		    if (catcher->start <= f
			&& f <= catcher->start + catcher->len - 1)
		      {
			debug (5, "Catcher caught fault at %p! (callback: %p)",
			       (void *) f, catcher->callback);
			if (catcher->callback (activation_frame, f))
			  /* The callback claims that we can continue.  */
			  break;
		      }
		    else
		      debug (5, "Catcher %p-%p does not cover fault %p",
			     (void *) catcher->start,
			     (void *) catcher->start + catcher->len - 1,
			     (void *) f);
		  }

		if (! catcher)
		  {
		    if (as_rwlock_owner == l4_myself ())
		      debug (0, "I hold as_rwlock!");

		    debug (0, "SIGSEGV at " VG_ADDR_FMT " "
			   "(ip: %p, sp: %p, eax: %p, ebx: %p, ecx: %p, "
			   "edx: %p, edi: %p, esi: %p, ebp: %p, eflags: %p)",
			   VG_ADDR_PRINTF (fault),
			   (void *) ip, (void *) sp,
			   (void *) activation_frame->eax,
			   (void *) activation_frame->ebx,
			   (void *) activation_frame->ecx,
			   (void *) activation_frame->edx,
			   (void *) activation_frame->edi,
			   (void *) activation_frame->esi,
			   (void *) activation_frame->ebp,
			   (void *) activation_frame->eflags);

		    backtrace_print ();

		    siginfo_t si;
		    memset (&si, 0, sizeof (si));
		    si.si_signo = SIGSEGV;
		    si.si_addr = VG_ADDR_TO_PTR (fault);

		    /* XXX: Should set si.si_code to SEGV_MAPERR or
		       SEGV_ACCERR.  */

		    pthread_kill_siginfo_np (pthread_self (), si);
		  }
	      }

	    break;
	  }

	default:
	  panic ("Unknown message id: %d", label);
	}
    }

  if (activation_frame->normal_mode_stack == utcb->alternate_stack)
    utcb->alternate_stack_inuse = false;

  assert (utcb->canary0 == UTCB_CANARY0);
  assert (utcb->canary1 == UTCB_CANARY1);

  l4_utcb_state_restore (activation_frame);
}

#ifndef NDEBUG
static uintptr_t
crc (struct hurd_utcb *utcb)
{
  uintptr_t crc = 0;
  uintptr_t *p;
  for (p = (uintptr_t *) utcb; p < &utcb->crc; p ++)
    crc += *p;

  return crc;
}
#endif

struct activation_frame *
hurd_activation_handler_activated (struct hurd_utcb *utcb)
{
  assert (((uintptr_t) utcb & (PAGESIZE - 1)) == 0);
  assert (utcb->canary0 == UTCB_CANARY0);
  assert (utcb->canary1 == UTCB_CANARY1);
  assert (utcb->vg.activated_mode);
  /* XXX: Assumption that stack grows down...  */
  assert (utcb->vg.activation_handler_sp - PAGESIZE <= (uintptr_t) &utcb);
  assert ((uintptr_t) &utcb <= utcb->vg.activation_handler_sp);

  if (unlikely (! mm_init_done))
    /* Just returns: during initialization, we don't except any faults or
       asynchronous IPC.  We do expect that IPC will be made but it will
       always be made with VG_IPC_RETURN and as such just returning will
       do the right thing.  */
    return NULL;

  /* This comes after the mm_init_done check as when switching utcbs,
     this may not be true.  */
  assertx (utcb == hurd_utcb (),
	   "%p != %p (func: %p; ip: %p, sp: %p)",
	   utcb, hurd_utcb (), hurd_utcb,
	   (void *) utcb->vg.saved_ip, (void *) utcb->vg.saved_sp);

  debug (5, "Activation handler called (utcb: %p)", utcb);

  struct hurd_message_buffer *mb
    = (struct hurd_message_buffer *) (uintptr_t) utcb->vg.messenger_id;

  debug (5, "Got message %llx (utcb: %p)", utcb->vg.messenger_id, utcb);

  assert (mb->magic == HURD_MESSAGE_BUFFER_MAGIC);

  struct activation_frame *activation_frame = activation_frame_alloc (utcb);
  assert (activation_frame->canary == ACTIVATION_FRAME_CANARY);

  l4_utcb_state_save (activation_frame);

  activation_frame->message_buffer = mb;

#ifndef NDEBUG
  utcb->crc = crc (utcb);
#endif

  /* Whether we need to process the activation in normal mode.  */
  bool trampoline = true;

  if (mb == utcb->extant_message)
    /* The extant IPC reply.  Just return, everything is in place.  */
    {
#ifndef NDEBUG
      do_debug (0)
	{
	  int label = 0;
	  if (vg_message_data_count (mb->request) >= sizeof (uintptr_t))
	    label = vg_message_word (mb->request, 0);
	  error_t err = -1;
	  if (vg_message_data_count (mb->reply) >= sizeof (uintptr_t))
	    err = vg_message_word (mb->reply, 0);

	  debug (5, "Extant RPC: %s (%d) -> %d",
		 vg_method_id_string (label), label, err);
	}
#endif

      utcb->extant_message = NULL;
      trampoline = false;
    }
  else if (mb->closure)
    {
      debug (5, "Closure");
    }
  else if (mb == utcb->exception_buffer)
    /* It's an exception.  Process it.  */
    {
      debug (5, "Exception");

      uintptr_t label = vg_message_word (mb->reply, 0);
      switch (label)
	{
	case VG_ACTIVATION_fault:
	  {
	    vg_addr_t fault;
	    uintptr_t ip;
	    uintptr_t sp;
	    struct vg_activation_fault_info info;

	    error_t err;
	    err = vg_activation_fault_send_unmarshal (mb->reply,
						      &fault, &sp, &ip, &info,
						      NULL);
	    if (err)
	      panic ("Failed to unmarshal exception: %d", err);

	    debug (4, "Fault at " VG_ADDR_FMT "(ip: %x, sp: %x).",
		   VG_ADDR_PRINTF (fault), ip, sp);

	    uintptr_t f = (uintptr_t) VG_ADDR_TO_PTR (fault);
	    uintptr_t stack_page = (sp & ~(PAGESIZE - 1));
	    uintptr_t fault_page = (f & ~(PAGESIZE - 1));
	    if (stack_page == fault_page
		|| stack_page - PAGESIZE == fault_page)
	      /* The fault on the same page as the stack pointer or
		 the following page.  It is likely a stack fault.
		 Handle it using the alternate stack.  */
	      {
		debug (5, "Stack fault at " VG_ADDR_FMT "(ip: %x, sp: %x).",
		       VG_ADDR_PRINTF (fault), ip, sp);

		assert (! utcb->alternate_stack_inuse);
		utcb->alternate_stack_inuse = true;

		assert (utcb->alternate_stack);

		activation_frame->normal_mode_stack = utcb->alternate_stack;
	      }

	    debug (5, "Handling fault at " VG_ADDR_FMT " in normal mode "
		   "(ip: %x, sp: %x).",
		   VG_ADDR_PRINTF (fault), ip, sp);

	    break;
	  }

	default:
	  panic ("Unknown message id: %d", label);
	}

      /* Unblock the exception handler messenger.  */
      error_t err = vg_ipc (VG_IPC_RECEIVE | VG_IPC_RECEIVE_ACTIVATE
			    | VG_IPC_RETURN,
			    VG_ADDR_VOID, utcb->exception_buffer->receiver,
			    VG_ADDR_VOID,
			    VG_ADDR_VOID, VG_ADDR_VOID, VG_ADDR_VOID, VG_ADDR_VOID);
      assert (! err);
    }
  else if (mb->just_free)
    {
      debug (5, "Just freeing");
      hurd_message_buffer_free (mb);
      trampoline = false;
    }
  else
    {
      panic ("Unknown messenger %llx (extant: %p; exception: %p) (label: %d)",
	     utcb->vg.messenger_id,
	     utcb->extant_message, utcb->exception_buffer,
	     vg_message_word (mb->reply, 0));
    }

  /* Assert that the utcb has not been modified.  */
  assert (utcb->crc == crc (utcb));

  if (! trampoline)
    {
      debug (5, "Direct return");

      assert (utcb->activation_stack == activation_frame);
      utcb->activation_stack = utcb->activation_stack->next;

      l4_utcb_state_restore (activation_frame);

      activation_frame = NULL;
    }
  else
    {
      debug (5, "Continuing in normal mode");
      l4_utcb_state_restore (activation_frame);
    }

  assert (utcb->canary0 == UTCB_CANARY0);
  assert (utcb->canary1 == UTCB_CANARY1);

  return activation_frame;
}

static char activation_handler_area0[PAGESIZE]
  __attribute__ ((aligned (PAGESIZE)));
static char activation_handler_msg[PAGESIZE]
  __attribute__ ((aligned (PAGESIZE)));
static struct hurd_utcb *initial_utcb = (void *) &activation_handler_area0[0];

static struct hurd_utcb *
simple_utcb_fetcher (void)
{
  assert (initial_utcb->canary0 == UTCB_CANARY0);
  assert (initial_utcb->canary1 == UTCB_CANARY1);

  return initial_utcb;
}

struct hurd_utcb *(*hurd_utcb) (void);

void
hurd_activation_handler_init_early (void)
{
  initial_utcb->canary0 = UTCB_CANARY0;
  initial_utcb->canary1 = UTCB_CANARY1;

  hurd_utcb = simple_utcb_fetcher;

  struct hurd_utcb *utcb = hurd_utcb ();
  assert (utcb == initial_utcb);

  /* XXX: We assume the stack grows down!  SP is set to the end of the
     exception page.  */
  utcb->vg.activation_handler_sp
    = (uintptr_t) activation_handler_area0 + sizeof (activation_handler_area0);

  /* The word beyond the base of the stack is interpreted as a pointer
     to the exception page.  Make it so.  */
  utcb->vg.activation_handler_sp -= sizeof (void *);
  * (void **) utcb->vg.activation_handler_sp = utcb;

  utcb->vg.activation_handler_ip = (uintptr_t) &hurd_activation_handler_entry;
  utcb->vg.activation_handler_end = (uintptr_t) &hurd_activation_handler_end;

  struct vg_thread_exregs_in in;
  memset (&in, 0, sizeof (in));

  struct vg_message *msg = (void *) &activation_handler_msg[0];
  vg_thread_exregs_send_marshal (msg, VG_EXREGS_SET_UTCB, in,
				 VG_ADDR_VOID, VG_ADDR_VOID,
				 VG_PTR_TO_PAGE (utcb), VG_ADDR_VOID,
				 __hurd_startup_data->messengers[1]);

  error_t err;
  err = vg_ipc_full (VG_IPC_RECEIVE | VG_IPC_SEND | VG_IPC_RECEIVE_ACTIVATE
		     | VG_IPC_RECEIVE_SET_THREAD_TO_CALLER
		     | VG_IPC_RECEIVE_SET_ASROOT_TO_CALLERS
		     | VG_IPC_RECEIVE_INLINE
		     | VG_IPC_SEND_SET_THREAD_TO_CALLER
		     | VG_IPC_SEND_SET_ASROOT_TO_CALLERS,
		     VG_ADDR_VOID,
		     __hurd_startup_data->messengers[1], VG_ADDR_VOID, VG_ADDR_VOID,
		     VG_ADDR_VOID, __hurd_startup_data->thread,
		     __hurd_startup_data->messengers[0], VG_PTR_TO_PAGE (msg),
		     0, 0, VG_ADDR_VOID);
  if (err)
    panic ("Failed to send IPC: %d", err);
  if (utcb->vg.inline_words[0])
    panic ("Failed to install utcb page: %d", utcb->vg.inline_words[0]);
}

void
hurd_activation_handler_init (void)
{
  struct hurd_utcb *utcb;
  error_t err = hurd_activation_state_alloc (__hurd_startup_data->thread,
					     &utcb);
  if (err)
    panic ("Failed to allocate activation state: %d", err);

  assert (! initial_utcb->activation_stack);

  initial_utcb = utcb;

  debug (4, "initial_utcb (%p) is now: %p", &initial_utcb, initial_utcb);
}

/* The activation area is 16 pages large.  It consists of the utch,
   the activation stack and an alternate stack (which is needed to
   handle stack faults).  */
#define ACTIVATION_AREA_SIZE_LOG2 (PAGESIZE_LOG2 + 4)
#define ACTIVATION_AREA_SIZE (1 << ACTIVATION_AREA_SIZE_LOG2)

error_t
hurd_activation_state_alloc (vg_addr_t thread, struct hurd_utcb **utcbp)
{
  debug (5, DEBUG_BOLD ("allocating activation state for " VG_ADDR_FMT),
	 VG_ADDR_PRINTF (thread));

  vg_addr_t activation_area = as_alloc (ACTIVATION_AREA_SIZE_LOG2, 1, true);
  void *activation_area_base
    = VG_ADDR_TO_PTR (vg_addr_extend (activation_area,
				0, ACTIVATION_AREA_SIZE_LOG2));

  debug (0, "Activation area: %p-%p",
	 activation_area_base, activation_area_base + ACTIVATION_AREA_SIZE);

  int page_count = 0;
  /* Be careful!  We assume that pages is properly set up after at
     most 2 allocations!  */
  vg_addr_t pages_[2];
  vg_addr_t *pages = pages_;

  void alloc (void *addr)
  {
    vg_addr_t slot = vg_addr_chop (VG_PTR_TO_ADDR (addr), PAGESIZE_LOG2);

    as_ensure (slot);

    struct storage storage;
    storage = storage_alloc (VG_ADDR_VOID, vg_cap_page,
			     STORAGE_LONG_LIVED,
			     VG_OBJECT_POLICY_DEFAULT,
			     slot);

    if (VG_ADDR_IS_VOID (storage.addr))
      panic ("Failed to allocate page for exception state");

    if (pages == pages_)
      assert (page_count < sizeof (pages_) / sizeof (pages_[0]));
    pages[page_count ++] = storage.addr;
  }

  /* When NDEBUG is true, we leave some pages empty so that should
     something overrun, we'll fault.  */
#ifndef NDEBUG
#define SKIP 1
#else
#define SKIP 0
#endif

  int page = SKIP;

  /* Allocate the utcb.  */
  struct hurd_utcb *utcb = activation_area_base + page * PAGESIZE;
  alloc (utcb);
  page += 1 + SKIP;

  /* And set up the small activation stack.
     UTCB->ACTIVATION_HANDLER_SP is the base of the stack.

     XXX: We assume the stack grows down!  */
#ifndef NDEBUG
  /* Use a dedicated page.  */
  utcb->vg.activation_handler_sp
    = (uintptr_t) activation_area_base + page * PAGESIZE;
  alloc ((void *) utcb->vg.activation_handler_sp);

  utcb->vg.activation_handler_sp += PAGESIZE;
  page += 1 + SKIP;
#else
  /* Use the end of the UTCB.  */
  utcb->vg.activation_handler_sp = utcb + PAGESIZE;
#endif

  /* At the top of the stack page, we use some space to remember the
     storage we allocate so that we can free it later.  */
  utcb->vg.activation_handler_sp
    -= sizeof (vg_addr_t) * ACTIVATION_AREA_SIZE / PAGESIZE;
  memset ((void *) utcb->vg.activation_handler_sp, 0,
	  sizeof (vg_addr_t) * ACTIVATION_AREA_SIZE / PAGESIZE);
  memcpy ((void *) utcb->vg.activation_handler_sp, pages,
	  sizeof (vg_addr_t) * page_count);
  pages = (vg_addr_t *) utcb->vg.activation_handler_sp;

  /* The word beyond the base of the stack is a pointer to the
     exception page.  */
  utcb->vg.activation_handler_sp -= sizeof (void *);
  * (void **) utcb->vg.activation_handler_sp = utcb;


  /* And a medium-sized alternate stack.  */
  void *a;
  for (a = activation_area_base + page * PAGESIZE;
       a < activation_area_base + ACTIVATION_AREA_SIZE - SKIP * PAGESIZE;
       a += PAGESIZE)
    alloc (a);

  assert (a - activation_area_base + page * PAGESIZE >= AS_STACK_SPACE);

  /* XXX: We again assume that the stack grows down.  */
  utcb->alternate_stack = a;


  utcb->vg.activation_handler_ip = (uintptr_t) &hurd_activation_handler_entry;
  utcb->vg.activation_handler_end = (uintptr_t) &hurd_activation_handler_end;

  utcb->exception_buffer = hurd_message_buffer_alloc_long ();
  utcb->extant_message = NULL;

  utcb->canary0 = UTCB_CANARY0;
  utcb->canary1 = UTCB_CANARY1;

  debug (5, "Activation area: %p-%p; utcb: %p; stack: %p; alt stack: %p",
	 (void *) activation_area_base,
	 (void *) activation_area_base + ACTIVATION_AREA_SIZE - 1,
	 utcb, (void *) utcb->vg.activation_handler_sp, utcb->alternate_stack);


  /* Unblock the exception handler messenger.  */
  error_t err = vg_ipc (VG_IPC_RECEIVE | VG_IPC_RECEIVE_ACTIVATE
			| VG_IPC_RETURN,
			VG_ADDR_VOID, utcb->exception_buffer->receiver, VG_ADDR_VOID,
			VG_ADDR_VOID, VG_ADDR_VOID, VG_ADDR_VOID, VG_ADDR_VOID);
  assert (! err);


  *utcbp = utcb;

  struct vg_thread_exregs_in in;
  struct vg_thread_exregs_out out;

  err = vg_thread_exregs (VG_ADDR_VOID, thread,
			  VG_EXREGS_SET_UTCB
			  | VG_EXREGS_SET_EXCEPTION_MESSENGER,
			  in, VG_ADDR_VOID, VG_ADDR_VOID,
			  VG_PTR_TO_PAGE (utcb),
			  utcb->exception_buffer->receiver,
			  &out, NULL, NULL, NULL, NULL);
  if (err)
    panic ("Failed to install utcb");

  err = vg_cap_copy (VG_ADDR_VOID,
		     utcb->exception_buffer->receiver,
		     VG_ADDR (VG_MESSENGER_THREAD_SLOT, VG_MESSENGER_SLOTS_LOG2),
		     VG_ADDR_VOID, thread,
		     0, VG_CAP_PROPERTIES_DEFAULT);
  if (err)
    panic ("Failed to set messenger's thread");

  err = vg_thread_id (VG_ADDR_VOID, thread, &utcb->tid);
  if (err)
    panic ("Failed to get thread id");

  check_activation_frame_reserve (utcb);

  return 0;
}

void
hurd_activation_state_free (struct hurd_utcb *utcb)
{
  assert (utcb->canary0 == UTCB_CANARY0);
  assert (utcb->canary1 == UTCB_CANARY1);
  assert (! utcb->activation_stack);

  /* Free any activation frames.  */
  struct activation_frame *f;
  struct activation_frame *prev = utcb->activation_stack_bottom;
  while ((f = prev))
    {
      prev = f->prev;
      hurd_slab_dealloc (&activation_frame_slab, f);
    }

  hurd_message_buffer_free (utcb->exception_buffer);

  /* Free the allocated storage.  */
  /* Copy the array as we're going to free the storage that it is
     in.  */
  vg_addr_t pages[ACTIVATION_AREA_SIZE / PAGESIZE];
  memcpy (pages,
	  (void *) utcb->vg.activation_handler_sp + sizeof (uintptr_t),
	  sizeof (vg_addr_t) * ACTIVATION_AREA_SIZE / PAGESIZE);

  int i;
  for (i = 0; i < sizeof (pages) / sizeof (pages[0]); i ++)
    if (! VG_ADDR_IS_VOID (pages[i]))
      storage_free (pages[i], false);

  /* Finally, free the address space.  */
  int page = SKIP;
  void *activation_area_base = (void *) utcb - page * PAGESIZE;
  as_free (vg_addr_chop (VG_PTR_TO_ADDR (activation_area_base),
		      ACTIVATION_AREA_SIZE_LOG2),
	   false);
}
