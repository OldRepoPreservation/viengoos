/* l4/gnu/sigma0.h - Public GNU interface for sigma0 protocol.
   Copyright (C) 2004 Free Software Foundation, Inc.
   Written by Marco Gerards <metgerards@student.han.nl>.

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

#ifndef _L4_SIGMA0_H
# error "Never use <l4/gnu/sigma0.h> directly; include <l4/sigma0.h> instead."
#endif


/* The thread ID of sigma0.  */
#define L4_SIGMA0_TID	(l4_global_id (l4_thread_user_base (), 1))

/* The message label for the "get page receive window" operation.
   This is -6 in the upper 24 bits.  */
#define L4_SIGMA0_MSG_GET_PAGE_RCV_WINDOW _L4_SIGMA0_MSG_GET_PAGE_RCV_WINDOW

/* The message label for undocumented sigma0 operations.  This is
   -1001 in the upper 24 bits.  */
#define L4_SIGMA0_MSG_EXT	(0xc170)

/* For undocumented operations, this is the meaning of the first
   untyped word in the message (MR1).  */
#define L4_SIGMA0_MSG_EXT_SET_VERBOSITY	1
#define L4_SIGMA0_MSG_EXT_DUMP_MEMORY	2


static inline _L4_fpage_t
_L4_attribute_always_inline
l4_sigma0_get_page_rcv_window (l4_fpage_t fpage, l4_fpage_t rcv_window)
{
  return _L4_sigma0_get_page_rcv_window (L4_SIGMA0_TID, fpage, rcv_window);
}


static inline l4_fpage_t
_L4_attribute_always_inline
l4_sigma0_get_page (l4_fpage_t fpage)
{
  return _L4_sigma0_get_page (L4_SIGMA0_TID, fpage);
}


static inline l4_fpage_t
_L4_attribute_always_inline
l4_sigma0_get_any (l4_word_t size, l4_fpage_t rcv_window)
{
  return _L4_sigma0_get_any (L4_SIGMA0_TID, size, rcv_window);
}


static inline void
_L4_attribute_always_inline
l4_sigma0_set_verbosity (l4_word_t level)
{
  __L4_msg_tag_t _tag;

  _tag.raw = _L4_niltag;
  _tag.label = L4_SIGMA0_MSG_EXT;
  _tag.untyped = 2;

  _L4_set_msg_tag (_tag.raw);
  _L4_load_mr (1, L4_SIGMA0_MSG_EXT_SET_VERBOSITY);
  _L4_load_mr (2, level);
  tag = _L4_call (L4_SIGMA0_TID);
}


static inline void
_L4_attribute_always_inline
l4_sigma0_dump_memory (l4_word_t wait)
{
  __L4_msg_tag_t _tag;
  
  _tag.raw = _L4_niltag;
  _tag.label = L4_MSG_SIGMA0_EXT;
  _tag.untyped = 2;

  _L4_set_msg_tag (_tag.raw);
  l4_load_mr (1, L4_SIGMA0_EXT_DUMP_MEMORY);
  l4_load_mr (2, wait);

  if (wait)
    tag = l4_call (L4_SIGMA0_TID);
  else
    tag = l4_send (L4_SIGMA0_TID);
}
