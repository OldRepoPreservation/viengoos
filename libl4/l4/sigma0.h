/* l4/sigma0.h - Public interface to the sigma0 protocol.
   Copyright (C) 2004 Free Software Foundation, Inc.
   Written by Marcus Brinkmann <marcus@gnu.org>.
   Modified by Marco Gerards <metgerards@student.han.nl>.

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
#define _L4_SIGMA0_H	1

#include <l4/ipc.h>


/* The message label for the "get page receive window" operation.
   This is -6 in the upper 24 bits.  */
#define _L4_SIGMA0_MSG_GET_PAGE_RCV_WINDOW	(0xffa0)


static inline _L4_fpage_t
_L4_attribute_always_inline
_L4_sigma0_get_page_rcv_window (_L4_thread_id_t sigma0, _L4_fpage_t fpage,
				_L4_fpage_t rcv_window)
{
  __L4_msg_tag_t _tag;
  _L4_word_t mrs[2];

  _L4_accept (_L4_map_grant_items (rcv_window));

  _tag.raw = _L4_niltag;
  _tag.label = _L4_SIGMA0_MSG_GET_PAGE_RCV_WINDOW;
  _tag.untyped = 2;

  _L4_set_msg_tag (_tag.raw);
  _L4_load_mr (1, fpage);
  _L4_load_mr (2, _L4_DEFAULT_MEMORY);
  
  tag = _L4_call (sigma0);
  if (_L4_ipc_failed (tag))
    return _L4_nilpage;

  _L4_store_mr (1, &mrs[0]);
  _L4_store_mr (2, &mrs[1]);

  return _L4_map_item_snd_fpage (*((_L4_map_item_t *) mrs));
}


static inline _L4_fpage_t
_L4_attribute_always_inline
_L4_sigma0_get_page (_L4_thread_id_t sigma0, _L4_fpage_t fpage)
{
  return _L4_sigma0_get_page_rcv_window (sigma0, fpage,
					 _L4_complete_address_space);
}


static inline _L4_fpage_t
_L4_attribute_always_inline
_L4_sigma0_get_any (_L4_thread_id_t sigma0, _L4_word_t size,
		    _L4_fpage_t rcv_window)
{
  l4_fpage_t fpage = _L4_fpage_log2 (~0, size);

  return _L4_sigma0_get_page_rcv_window (sigma0, fpage, rcv_window);
}



/* Now incorporate the public interfaces the user has selected.  */
#ifdef _L4_INTERFACE_L4
#include <l4/compat/sigma0.h>
#endif
#ifdef _L4_INTERFACE_GNU
#include <l4/gnu/sigma0.h>
#endif

#endif	/* l4/sigma0.h */
