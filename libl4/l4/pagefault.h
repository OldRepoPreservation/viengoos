/* l4/pagefault.h - Public interface to the pagefault protocol.
   Copyright (C) 2004 Free Software Foundation, Inc.
   Written by Marcus Brinkmann <marcus@gnu.org>.

   This file is part of the GNU L4 library.
 
   The GNU L4 library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License
   as published by the Free Software Foundation; either version 2.1 of
   the License, or (at your option) any later version.
 
   The GNU L4 library is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.
 
   You should have received a copy of the GNU Lesser General Public
   License along with the GNU L4 library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

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


/* Reply to a previous pagefault request message by thread TO with the
   provided map or grant item.  Returns 1 on success and 0 if the Ipc
   system call failed (then _L4_error_code provides more information
   about the failure).  */
static inline _L4_word_t
_L4_attribute_always_inline
_L4_pagefault_reply (_L4_thread_id_t to, void *item)
{
  __L4_msg_tag_t _tag;
  _L4_msg_tag_t tag;
  _L4_word_t msg[2];

  *((_L4_dword_t *) msg) = *((_L4_dword_t *) item);

  _tag.raw = _L4_niltag;
  _tag.typed = 2;
  tag = _tag.raw;

  _L4_set_msg_tag (tag);
  _L4_load_mr (1, msg[0]);
  _L4_load_mr (2, msg[1]);
  tag = _L4_reply (to);
  return _L4_ipc_succeeded (tag);
}


/* Now incorporate the public interfaces the user has selected.  */
#ifdef _L4_INTERFACE_GNU
#include <l4/gnu/pagefault.h>
#endif

#endif	/* l4/pagefault.h */
