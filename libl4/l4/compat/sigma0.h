/* l4/compat/sigma0.h - Public compat interface to the sigma0 protocol.
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
# error "Never use <l4/compat/sigma0.h> directly; include <l4/sigma0.h> instead."
#endif


static inline L4_Fpage_t
_L4_attribute_always_inline
L4_Sigma0_GetPage_RcvWindow (L4_ThreadId_t sigma0, L4_Fpage_t fpage,
			     L4_Fpage_t rcv_window)
{
  fpage.raw = _L4_sigma0_get_page_rcv_window (sigma0.raw, fpage.raw,
					      rcv_window.raw);
  return fpage;
}


static inline L4_Fpage_t
_L4_attribute_always_inline
L4_Sigma0_GetPage (L4_ThreadId_t sigma0, L4_Fpage_t fpage)
{
  fpage.raw = _L4_sigma0_get_page (sigma0.raw, fpage.raw);
  return fpage;
}

static inline L4_Fpage_t
_L4_attribute_always_inline
L4_Sigma0_GetAny (L4_ThreadId_t sigma0, L4_Word_t size, L4_Fpage_t rcv_window)
{
  L4_Fpage_t fpage;
  fpage.raw = _L4_sigma0_get_any (sigma0.raw, size, rcv_window.raw);
  return fpage;
}
