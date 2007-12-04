/* exceptions.c - Exception handler implementation.
   Copyright (C) 2007 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU Hurd is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#include <hurd/startup.h>
#include <hurd/stddef.h>
#include <hurd/exceptions.h>
#include <hurd/storage.h>
#include <hurd/thread.h>
#include <l4/thread.h>

#include "pager.h"

extern struct hurd_startup_data *__hurd_startup_data;

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

/* This function is invoked by the monitor when the thread raises an
   exception.  The exception is saved in the exception page.  */
void
exception_handler (struct exception_page *exception_page)
{
  debug (5, "Exception handler called (0x%x.%x, exception_page: %p)",
	 l4_thread_no (l4_myself ()), l4_version (l4_myself ()),
	 exception_page);

  l4_msg_tag_t msg_tag = l4_msg_msg_tag (exception_page->exception);
  l4_word_t label;
  label = l4_label (msg_tag);

  int args_read = 0;
  int expected_words;
  switch (label)
    {
    case EXCEPTION_fault:
      {
	addr_t fault;
	uintptr_t ip;
	struct exception_info info;

	error_t err;
	err = exception_fault_send_unmarshal (exception_page->exception,
					      &fault, &ip, &info);
	if (err)
	  panic ("Failed to unmarshalling exception: %d", err);

	bool r = pager_fault (fault, ip, info);
	if (! r)
	  {
	    debug (1, "Failed to handle fault at " ADDR_FMT " (ip=%x)",
		   ADDR_PRINTF (fault), ip);
	    /* XXX: Should raise SIGSEGV.  */
	    for (;;)
	      l4_yield ();
	  }
      }
      break;

    default:
      debug (1, "Unknown message id: %d", label);
    }
}

void
exception_handler_init (void)
{
  extern struct hurd_startup_data *__hurd_startup_data;

  struct storage storage = storage_alloc (ADDR_VOID, cap_page,
					  STORAGE_LONG_LIVED, ADDR_VOID);

  if (ADDR_IS_VOID (storage.addr))
    panic ("Failed to allocate page for exception state");

  struct exception_page *exception_page 
    = ADDR_TO_PTR (addr_extend (storage.addr, 0, PAGESIZE_LOG2));

  /* XXX: We assume the stack grows down!  SP is set to the end of the
     exception page.  */
  exception_page->exception_handler_sp = (l4_word_t) exception_page + PAGESIZE;

  exception_page->exception_handler_ip = (l4_word_t) &exception_handler_entry;
  exception_page->exception_handler_end = (l4_word_t) &exception_handler_end;

  struct hurd_thread_exregs_in in;
  in.exception_page = storage.addr;

  struct hurd_thread_exregs_out out;
  error_t err = rm_thread_exregs (ADDR_VOID, __hurd_startup_data->thread,
				  HURD_EXREGS_SET_EXCEPTION_PAGE,
				  in, &out);
  if (err)
    panic ("Failed to install exception page");
}
