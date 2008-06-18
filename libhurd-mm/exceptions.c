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
#include <hurd/exceptions.h>
#include <hurd/storage.h>
#include <hurd/slab.h>
#include <hurd/thread.h>
#include <l4/thread.h>

#include <signal.h>
#include <string.h>

#include "map.h"
#include "as.h"

extern struct hurd_startup_data *__hurd_startup_data;

static void
utcb_state_save (struct exception_frame *exception_frame)
{
  l4_word_t *utcb = _L4_utcb ();

  exception_frame->saved_sender = utcb[_L4_UTCB_SENDER];
  exception_frame->saved_receiver = utcb[_L4_UTCB_RECEIVER];
  exception_frame->saved_timeout = utcb[_L4_UTCB_TIMEOUT];
  exception_frame->saved_error_code = utcb[_L4_UTCB_ERROR_CODE];
  exception_frame->saved_flags = utcb[_L4_UTCB_FLAGS];
  exception_frame->saved_br0 = utcb[_L4_UTCB_BR0];
  memcpy (&exception_frame->saved_message,
	  utcb, L4_NUM_MRS * sizeof (l4_word_t));
}

static void
utcb_state_restore (struct exception_frame *exception_frame)
{
  l4_word_t *utcb = _L4_utcb ();

  utcb[_L4_UTCB_SENDER] = exception_frame->saved_sender;
  utcb[_L4_UTCB_RECEIVER] = exception_frame->saved_receiver;
  utcb[_L4_UTCB_TIMEOUT] = exception_frame->saved_timeout;
  utcb[_L4_UTCB_ERROR_CODE] = exception_frame->saved_error_code;
  utcb[_L4_UTCB_FLAGS] = exception_frame->saved_flags;
  utcb[_L4_UTCB_BR0] = exception_frame->saved_br0;
  memcpy (utcb, &exception_frame->saved_message,
	  L4_NUM_MRS * sizeof (l4_word_t));
}

static struct hurd_slab_space exception_frame_slab;

static error_t
exception_frame_slab_alloc (void *hook, size_t size, void **ptr)
{
  assert (size == PAGESIZE);

  struct exception_frame frame;
  utcb_state_save (&frame);

  struct storage storage = storage_alloc (meta_data_activity,
					  cap_page, STORAGE_EPHEMERAL,
					  OBJECT_POLICY_DEFAULT, ADDR_VOID);
  *ptr = ADDR_TO_PTR (addr_extend (storage.addr, 0, PAGESIZE_LOG2));

  utcb_state_restore (&frame);

  return 0;
}

static error_t
exception_frame_slab_dealloc (void *hook, void *buffer, size_t size)
{
  assert (size == PAGESIZE);

  addr_t addr = addr_chop (PTR_TO_ADDR (buffer), PAGESIZE_LOG2);
  storage_free (addr, false);

  return 0;
}

static struct exception_frame *
exception_frame_alloc (struct exception_page *exception_page)
{
  struct exception_frame *exception_frame;

  if (! exception_page->exception_stack
      && exception_page->exception_stack_bottom)
    /* The stack is empty but we have an available frame.  */
    {
      exception_frame = exception_page->exception_stack_bottom;
      exception_page->exception_stack = exception_frame;
    }
  else if (exception_page->exception_stack
	   && exception_page->exception_stack->prev)
    /* The stack is not empty and we have an available frame.  */
    {
      exception_frame = exception_page->exception_stack->prev;
      exception_page->exception_stack = exception_frame;
    }
  else
    /* We do not have an available frame.  */
    {
      void *buffer;
      error_t err = hurd_slab_alloc (&exception_frame_slab, &buffer);
      if (err)
	panic ("Out of memory!");

      exception_frame = buffer;

      exception_frame->prev = NULL;
      exception_frame->next = exception_page->exception_stack;
      if (exception_frame->next)
	exception_frame->next->prev = exception_frame;

      exception_page->exception_stack = exception_frame;

      if (! exception_page->exception_stack_bottom)
	/* This is the first frame we've allocated.  */
	exception_page->exception_stack_bottom = exception_frame;
    }

  return exception_frame;
}

/* Fetch an exception.  */
void
exception_fetch_exception (void)
{
  l4_msg_t msg;
  rm_exception_collect_send_marshal (&msg, ADDR_VOID);
  l4_msg_load (msg);

  l4_thread_id_t from;
  l4_msg_tag_t msg_tag = l4_reply_wait (__hurd_startup_data->rm, &from);
  if (l4_ipc_failed (msg_tag))
    panic ("Receiving message failed: %u", (l4_error_code () >> 1) & 0x7);
}

/* XXX: Before returning from either exception_handler_normal or
   exception_handler_activated, we need to examine the thread's
   control state and if the IPC was interrupt, set the error code
   appropriately.  This also requires changing all invocations of IPCs
   to loop on interrupt.  Currently, this is not a problem as the only
   exception that we get is a page fault, which can only occur when
   the thread is not in an IPC.  (Sure, there are string buffers, but
   we don't use them.)  */

void
exception_handler_normal (struct exception_frame *exception_frame)
{
  debug (5, "Exception handler called (0x%x.%x, exception_frame: %p, "
	 "next: %p)",
	 l4_thread_no (l4_myself ()), l4_version (l4_myself ()),
	 exception_frame, exception_frame->next);

  l4_msg_t *msg = &exception_frame->exception;

  l4_msg_tag_t msg_tag = l4_msg_msg_tag (*msg);
  l4_word_t label;
  label = l4_label (msg_tag);

  switch (label)
    {
    case EXCEPTION_fault:
      {
	addr_t fault;
	uintptr_t ip;
	uintptr_t sp;
	struct exception_info info;

	error_t err;
	err = exception_fault_send_unmarshal (msg, &fault, &sp, &ip, &info);
	if (err)
	  panic ("Failed to unmarshal exception: %d", err);

	bool r = map_fault (fault, ip, info);
	if (! r)
	  {
	    debug (0, "SIGSEGV at " ADDR_FMT " (ip: %p, sp: %p, eax: %p, "
		   "ebx: %p, ecx: %p, edx: %p, edi: %p, esi: %p, "
		   "eflags: %p)",
		   ADDR_PRINTF (fault), ip, sp,
		   exception_frame->regs[0],
		   exception_frame->regs[5],
		   exception_frame->regs[1],
		   exception_frame->regs[2],
		   exception_frame->regs[6],
		   exception_frame->regs[7],
		   exception_frame->regs[3]);

	    extern int backtrace (void **array, int size);

	    void *a[20];
	    int count = backtrace (a, sizeof (a) / sizeof (a[0]));
	    int i;
	    s_printf ("Backtrace: ");
	    for (i = 0; i < count; i ++)
	      s_printf ("%p ", a[i]);
	    s_printf ("\n");

	    siginfo_t si;
	    memset (&si, 0, sizeof (si));
	    si.si_signo = SIGSEGV;
	    si.si_addr = ADDR_TO_PTR (fault);

	    /* XXX: Should set si.si_code to SEGV_MAPERR or
	       SEGV_ACCERR.  */

	    pthread_kill_siginfo_np (pthread_self (), si);
	  }

	break;
      }

    default:
      panic ("Unknown message id: %d", label);
    }

  utcb_state_restore (exception_frame);
}

#ifndef NDEBUG
static l4_word_t
crc (struct exception_page *exception_page)
{
  l4_word_t crc = 0;
  l4_word_t *p;
  for (p = (l4_word_t *) exception_page; p < &exception_page->crc; p ++)
    crc += *p;

  return crc;
}
#endif

struct exception_frame *
exception_handler_activated (struct exception_page *exception_page)
{
  /* We expect EXCEPTION_PAGE to be page aligned.  */
  assert (((uintptr_t) exception_page & (PAGESIZE - 1)) == 0);
  assert (exception_page->activated_mode);

  /* Allocate an exception frame.  */
  struct exception_frame *exception_frame
    = exception_frame_alloc (exception_page);
  utcb_state_save (exception_frame);

  debug (5, "Exception handler called (exception_page: %p)",
	 exception_page);

#ifndef NDEBUG
  exception_page->crc = crc (exception_page);
#endif

  l4_msg_t *msg = &exception_page->exception;

  l4_msg_tag_t msg_tag = l4_msg_msg_tag (*msg);
  l4_word_t label;
  label = l4_label (msg_tag);

  switch (label)
    {
    case EXCEPTION_fault:
      {
	addr_t fault;
	uintptr_t ip;
	uintptr_t sp;
	struct exception_info info;

	error_t err;
	err = exception_fault_send_unmarshal (msg, &fault, &sp, &ip, &info);
	if (err)
	  panic ("Failed to unmarshal exception: %d", err);

	/* XXX: We assume that the stack grows down here.  */
	uintptr_t f = (uintptr_t) ADDR_TO_PTR (fault);
	if (sp - PAGESIZE <= f && f <= sp + PAGESIZE * 4)
	  /* The fault occurs within four pages of the stack pointer.
	     It has got to be a stack fault.  Handle it here.  */
	  {
	    debug (5, "Handling fault at " ADDR_FMT " in activated mode "
		   "(ip: %x, sp: %x).",
		   ADDR_PRINTF (fault), ip, sp);

	    bool r = map_fault (fault, ip, info);
	    if (! r)
	      {
		debug (0, "SIGSEGV at " ADDR_FMT " (ip: %p, sp: %p, eax: %p, "
		       "ebx: %p, ecx: %p, edx: %p, edi: %p, esi: %p, "
		       "eflags: %p)",
		       ADDR_PRINTF (fault), ip, sp,
		       exception_frame->regs[0],
		       exception_frame->regs[5],
		       exception_frame->regs[1],
		       exception_frame->regs[2],
		       exception_frame->regs[6],
		       exception_frame->regs[7],
		       exception_frame->regs[3]);

		siginfo_t si;
		memset (&si, 0, sizeof (si));
		si.si_signo = SIGSEGV;
		si.si_addr = ADDR_TO_PTR (fault);

		/* XXX: Should set si.si_code to SEGV_MAPERR or
		   SEGV_ACCERR.  */

		pthread_kill_siginfo_np (pthread_self (), si);
	      }
	    assert (exception_page->crc == crc (exception_page));

	    utcb_state_restore (exception_frame);

	    assert (exception_page->crc == crc (exception_page));
	    assertx (exception_page->exception_stack == exception_frame,
		     "%p != %p",
		     exception_page->exception_stack, exception_frame);

	    exception_page->exception_stack
	      = exception_page->exception_stack->next;
	    return NULL;
	  }

	debug (5, "Handling fault at " ADDR_FMT " in normal mode "
	       "(ip: %x, sp: %x).",
	       ADDR_PRINTF (fault), ip, sp);

	break;
      }

    default:
      panic ("Unknown message id: %d", label);
    }

  /* Handle the fault in normal mode.  */

  /* Copy the relevant bits.  */
  memcpy (&exception_frame->exception, msg,
	  (1 + l4_untyped_words (msg_tag)) * sizeof (l4_word_t));

  assert (exception_page->crc == crc (exception_page));
  return exception_frame;
}

void
exception_handler_init (void)
{
  error_t err = hurd_slab_init (&exception_frame_slab,
				sizeof (struct exception_frame), 0,
				exception_frame_slab_alloc,
				exception_frame_slab_dealloc,
				NULL, NULL, NULL);
  assert (! err);

  extern struct hurd_startup_data *__hurd_startup_data;

  /* We use the start of the area (lowest address) as the exception page.  */
  addr_t stack_area = as_alloc (EXCEPTION_STACK_SIZE_LOG2, 1, true);
  void *stack_area_base
    = ADDR_TO_PTR (addr_extend (stack_area, 0, EXCEPTION_STACK_SIZE_LOG2));

  debug (5, "Exception area: %x-%x",
	 stack_area_base, stack_area_base + EXCEPTION_STACK_SIZE - 1);

  void *page;
  for (page = stack_area_base;
       page < stack_area_base + EXCEPTION_STACK_SIZE;
       page += PAGESIZE)
    {
      addr_t slot = addr_chop (PTR_TO_ADDR (page), PAGESIZE_LOG2);

      as_ensure (slot);

      struct storage storage;
      storage = storage_alloc (ADDR_VOID, cap_page,
			       STORAGE_LONG_LIVED,
			       OBJECT_POLICY_DEFAULT,
			       slot);

      if (ADDR_IS_VOID (storage.addr))
	panic ("Failed to allocate page for exception state");
    }

  struct exception_page *exception_page = stack_area_base;

  /* XXX: We assume the stack grows down!  SP is set to the end of the
     exception page.  */
  exception_page->exception_handler_sp
    = (uintptr_t) stack_area_base + EXCEPTION_STACK_SIZE;

  /* The word beyond the base of the stack is a pointer to the
     exception page.  */
  exception_page->exception_handler_sp -= sizeof (void *);
  * (void **) exception_page->exception_handler_sp = exception_page;

  exception_page->exception_handler_ip = (l4_word_t) &exception_handler_entry;
  exception_page->exception_handler_end = (l4_word_t) &exception_handler_end;

  struct hurd_thread_exregs_in in;
  in.exception_page = addr_chop (PTR_TO_ADDR (exception_page), PAGESIZE_LOG2);

  struct hurd_thread_exregs_out out;
  err = rm_thread_exregs (ADDR_VOID, __hurd_startup_data->thread,
			  HURD_EXREGS_SET_EXCEPTION_PAGE,
			  in, &out);
  if (err)
    panic ("Failed to install exception page");
}

void
exception_page_cleanup (struct exception_page *exception_page)
{
  struct exception_frame *f;
  struct exception_frame *prev = exception_page->exception_stack_bottom;

  while ((f = prev))
    {
      prev = f->prev;
      hurd_slab_dealloc (&exception_frame_slab, f);
    }
}

