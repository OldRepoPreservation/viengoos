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
#include <l4/thread.h>

#include "pager.h"

extern struct hurd_startup_data *__hurd_startup_data;

/* Return the next word.  */
#define ARG() \
  ({ \
    assert (args_read < expected_words); \
    l4_msg_word (msg, args_read ++); \
  })

  /* Return word WORD_.  */
#if L4_WORDSIZE == 32
#define ARG64() \
  ({ \
    union { l4_uint64_t raw; struct { l4_uint32_t word[2]; }; } value_; \
    value_.word[0] = ARG (); \
    value_.word[1] = ARG (); \
    value_.raw; \
  })
#define ARG64_WORDS 2
#else
#define ARG64(word_) ARG(word_)
#define ARG64_WORDS 1
#endif

#define ARG_ADDR() ((addr_t) { ARG64() })

/* Check that the received message contains WORDS untyped words.  */
#define CHECK(words, words64) \
  ({ \
    expected_words = (words) + (words64) * ARG64_WORDS; \
    if (l4_untyped_words (msg_tag) != expected_words \
        || l4_typed_words (msg_tag) != 0) \
      { \
        debug (1, "Invalid format for %s: expected %d words, got %d", \
	       exception_method_id_string (label), \
               expected_words, l4_untyped_words (msg_tag)); \
        continue; \
      } \
  })

static void
exception_thread (void)
{
  debug (5, "Exception thread launched (0x%x.%x)",
	 l4_thread_no (l4_myself ()), l4_version (l4_myself ()));

  for (;;)
    {
      l4_msg_t msg;
      rm_exception_collect_marshal (&msg);
      l4_msg_load (msg);

      l4_thread_id_t from;
      l4_msg_tag_t msg_tag = l4_reply_wait (__hurd_startup_data->rm, &from);
      if (l4_ipc_failed (msg_tag))
	panic ("Receiving message failed: %u", (l4_error_code () >> 1) & 0x7);

      l4_msg_store (msg_tag, msg);
      l4_word_t label;
      label = l4_label (msg_tag);

      int args_read = 0;
      int expected_words;
      switch (label)
	{
	case EXCEPTION_fault:
	  CHECK (2, 1);

	  addr_t fault = ARG_ADDR ();
	  uintptr_t ip = ARG ();
	  struct exception_info info;
	  info.raw = ARG ();

	  bool r = pager_fault (fault, ip, info);
	  if (r)
	    /* The appears to have been resolved.  Resume the
	       thread.  */
	    l4_start (hurd_main_thread (l4_myself ()));
	  else
	    {
	      debug (1, "Failed to handle fault at " ADDR_FMT,
		     ADDR_PRINTF (fault));
	      /* XXX: Should raise SIGSEGV.  */
	    }

	  /* Resume the main thread.  */
	  
	  break;

	default:
	  debug (1, "Unknown message id: %d", label);
	}
    }
}

static char stack[2 * PAGESIZE] __attribute__ ((aligned(PAGESIZE)));

void
exception_handler_init (void)
{
  /* XXX: This only works for architectures on which the stack grows
     downward.  */
  void *sp = stack + sizeof (stack);

  l4_thread_id_t tid = hurd_exception_thread (l4_myself ());
  l4_start_sp_ip (tid, (l4_word_t) sp, (l4_word_t) &exception_thread);
}
