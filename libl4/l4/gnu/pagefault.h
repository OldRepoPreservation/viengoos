/* l4/gnu/pagefault.h - Public GNU interface to the pagefault protocol.
   Copyright (C) 2004 Free Software Foundation, Inc.
   Written by Marcus Brinkmann <marcus@gnu.org>.

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
# error "Never use <l4/gnu/pagefault.h> directly; include <l4/pagefault.h> instead."
#endif


/* Return true if the provided message tag matches a pagefault IPC.  */
static inline l4_word_t
_L4_attribute_always_inline
l4_is_pagefault (l4_msg_tag_t tag)
{
  return _L4_is_pagefault (tag);
}


/* Return the fault reason, address, and faulting user-level IP for
   the pagefault message described by TAG.  MR1 and MR2 must still
   contain the pagefault message.  The function returns the fault
   address.  */
static inline l4_word_t
_L4_attribute_always_inline
l4_pagefault (l4_msg_tag_t tag, l4_word_t *access, l4_word_t *ip)
{
  return _L4_pagefault (tag, access, ip);
}


/* Formulate a reply message (in the thread's virtual registers) to a
   previous pagefault request message with the provided map or grant
   item.  */
static inline void
_L4_attribute_always_inline
l4_pagefault_reply_formulate (void *item)
{
  _L4_pagefault_reply_formulate (item);
}


/* Reply to a previous pagefault request message by thread TO with the
   provided map or grant item.  Returns 1 on success and 0 if the Ipc
   system call failed (then l4_error_code provides more information
   about the failure).  */
static inline l4_word_t
_L4_attribute_always_inline
l4_pagefault_reply (l4_thread_id_t to, void *item)
{
  return _L4_pagefault_reply (to, item);
}
