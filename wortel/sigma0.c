/* Client code for sigma0.
   Copyright (C) 2003 Free Software Foundation, Inc.
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <l4.h>

#include "shutdown.h"

/* The thread ID of sigma0.  */
#define SIGMA0_TID	(l4_global_id (l4_thread_user_base (), 1))

/* The message label for the sigma0 request page operation.  This is
   -6 in the upper 24 bits.  */
#define SIGMA0_RPC	(0xffa0)

/* The message label for undocumented sigma0 operations.  This is
 -1001 in the upper 24 bits.  */
#define SIGMA0_EXT	(0xc170)

/* For undocumented operations, this is the meaning of the first
   untyped word in the message (MR1).  */
#define SIGMA0_EXT_SET_VERBOSITY	1
#define SIGMA0_EXT_DUMP_MEMORY		2

/* Set the verbosity level in sigma0.  The only levels used currently
   are 1 to 3.  Returns 0 on success, otherwise an IPC error code.  */
void
sigma0_set_verbosity (l4_word_t level)
{
  l4_msg_t msg;
  l4_msg_tag_t tag;

  l4_msg_clear (&msg);
  l4_set_msg_label (&msg, SIGMA0_EXT);
  l4_msg_append_word (&msg, SIGMA0_EXT_SET_VERBOSITY);
  l4_msg_append_word (&msg, level);
  l4_msg_load (&msg);
  tag = l4_send (SIGMA0_TID);
  if (l4_ipc_failed (tag))
    panic ("%s: request failed during %s: %u", __func__,
	   l4_error_code () & 1 ? "receive" : "send",
	   (l4_error_code () >> 1) & 0x7);
}


/* Request a memory dump from sigma0.  If WAIT is true, wait until the
   dump is completed before continuing.  */
void
sigma0_dump_memory (int wait)
{
  l4_msg_t msg;
  l4_msg_tag_t tag;

  l4_msg_clear (&msg);
  l4_set_msg_label (&msg, SIGMA0_EXT);
  l4_msg_append_word (&msg, SIGMA0_EXT_DUMP_MEMORY);
  l4_msg_append_word (&msg, wait);
  l4_msg_load (&msg);
  if (wait)
    tag = l4_call (SIGMA0_TID);
  else
    tag = l4_send (SIGMA0_TID);
  if (l4_ipc_failed (tag))
    panic ("%s: request failed during %s: %u", __func__,
	   l4_error_code () & 1 ? "receive" : "send",
	   (l4_error_code () >> 1) & 0x7);
}


/* Request the fpage FPAGE from sigma0.  */
void
sigma0_get_fpage (l4_fpage_t fpage)
{
  l4_msg_t msg;
  l4_msg_tag_t tag;
  l4_map_item_t map_item;

  l4_accept (l4_map_grant_items (l4_complete_address_space));
  l4_msg_clear (&msg);
  l4_set_msg_label (&msg, SIGMA0_RPC);
  l4_msg_append_word (&msg, fpage.raw);
  l4_msg_append_word (&msg, L4_DEFAULT_MEMORY);
  l4_msg_load (&msg);
  tag = l4_call (SIGMA0_TID);
  if (l4_ipc_failed (tag))
    panic ("%s: request failed during %s: %u", __func__,
	   l4_error_code () & 1 ? "receive" : "send",
	   (l4_error_code () >> 1) & 0x7);
  if (l4_untyped_words (tag) != 0 || l4_typed_words (tag) != 2)
    panic ("%s: invalid format of sigma0 reply", __func__);
  l4_msg_store (tag, &msg);
  l4_msg_get_map_item (&msg, 0, &map_item);
  if (l4_is_nil_fpage (map_item.send_fpage))
    panic ("%s: sigma0 rejected mapping", __func__);
}


/* Request an fpage of the size 2^SIZE from sigma0.  The fpage will be
   fullly accessible.  */
l4_fpage_t
sigma0_get_any (unsigned int size)
{
  l4_msg_t msg;
  l4_msg_tag_t tag;
  l4_map_item_t map_item;
  l4_fpage_t fpage = l4_fpage_log2 (-1, size);

  l4_accept (l4_map_grant_items (l4_complete_address_space));
  l4_msg_clear (&msg);
  l4_set_msg_label (&msg, SIGMA0_RPC);
  l4_msg_append_word (&msg, fpage.raw);
  l4_msg_append_word (&msg, L4_DEFAULT_MEMORY);
  l4_msg_load (&msg);
  tag = l4_call (SIGMA0_TID);
  if (l4_ipc_failed (tag))
    panic ("%s: request failed during %s: %u", __func__,
	   l4_error_code () & 1 ? "receive" : "send",
	   (l4_error_code () >> 1) & 0x7);
  if (l4_untyped_words (tag) != 0
      || l4_typed_words (tag) != 2)
    panic ("%s: invalid format of sigma0 reply", __func__);
  l4_msg_store (tag, &msg);
  l4_msg_get_map_item (&msg, 0, &map_item);
  return map_item.send_fpage;
}
