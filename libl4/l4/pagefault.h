/* l4/pagefault.h - Public interface to the pagefault protocol.
   Copyright (C) 2004, 2005, 2007 Free Software Foundation, Inc.
   Written by Marcus Brinkmann <marcus@gnu.org>.

   This file is part of the GNU L4 library.
 
   The GNU L4 library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License
   as published by the Free Software Foundation; either version 3 of
   the License, or (at your option) any later version.
   
   The GNU L4 library is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.
   
   You should have received a copy of the GNU Lesser General Public
   License along with this program.  If not, see
   <http://www.gnu.org/licenses/>.  */

#ifndef _L4_PAGEFAULT_H
#define _L4_PAGEFAULT_H	1

#include <l4/ipc.h>


/* Return true if the provided message tag matches a pagefault IPC.  */
static inline _L4_word_t
_L4_attribute_always_inline
_L4_is_pagefault (_L4_msg_tag_t tag)
{
  __L4_msg_tag_t _tag_pf;
  __L4_msg_tag_t _tag_mask;

  _tag_pf.raw = _L4_niltag;
  _tag_pf.untyped = 2;
  _tag_pf.label = 0xffe0;

  _tag_mask.raw = _L4_niltag;
  _tag_mask.label = 0x7;

  return (tag & ~_tag_mask.raw) == _tag_pf.raw;
}


/* Return the fault reason, address, and faulting user-level IP for
   the pagefault message described by TAG.  MR1 and MR2 must still
   contain the pagefault message.  The function returns the fault
   address.  */
static inline _L4_word_t
_L4_attribute_always_inline
_L4_pagefault (_L4_msg_tag_t tag, _L4_word_t *access, _L4_word_t *ip)
{
  _L4_word_t addr;

  if (access)
    *access = _L4_label (tag) & 0x7;

  if (ip)
    l4_store_mr (2, ip);

  l4_store_mr (1, &addr);
  return addr;
}


/* Formulate a reply message in MSG to a previous pagefault request
   message with the provided map or grant item.  */
static inline void
_L4_attribute_always_inline
_L4_pagefault_reply_formulate_in (_L4_msg_t msg, void *item)
{
  _L4_msg_put (msg, 0, 0, 0, 2, item);
}


/* Formulate a reply message (in the thread's virtual registers) to a
   previous pagefault request message with the provided map or grant
   item.  */
static inline void
_L4_attribute_always_inline
_L4_pagefault_reply_formulate (void *item)
{
  _L4_msg_t msg;
  _L4_pagefault_reply_formulate_in (msg, item);
  _L4_msg_load (msg);
}


/* Reply to a previous pagefault request message by thread TO with the
   provided map or grant item.  Returns 1 on success and 0 if the Ipc
   system call failed (then _L4_error_code provides more information
   about the failure).  */
static inline _L4_word_t
_L4_attribute_always_inline
_L4_pagefault_reply (_L4_thread_id_t to, void *item)
{
  _L4_msg_tag_t tag;

  _L4_pagefault_reply_formulate (item);

  tag = _L4_reply (to);
  return _L4_ipc_succeeded (tag);
}


/* Now incorporate the public interfaces the user has selected.  */
#ifdef _L4_INTERFACE_GNU
#include <l4/gnu/pagefault.h>
#endif

#endif	/* l4/pagefault.h */
