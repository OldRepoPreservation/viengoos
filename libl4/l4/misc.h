/* l4/misc.h - Public interface to L4 miscellaneous functions.
   Copyright (C) 2003, 2004 Free Software Foundation, Inc.
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

#ifndef _L4_MISC_H
#define _L4_MISC_H	1

#include <l4/types.h>
#include <l4/bits/misc.h>
#include <l4/vregs.h>
#include <l4/syscall.h>


/* l4_memory_control convenience interface.  */

#define _L4_DEFAULT_MEMORY	(_L4_WORD_C(0))

static inline _L4_word_t
_L4_attribute_always_inline
_L4_set_page_attribute (_L4_fpage_t fpage, _L4_word_t attribute)
{
  _L4_word_t attributes[4];

  attributes[0] = attribute;
  _L4_set_rights (&fpage, 0);
  _L4_load_mr (0, fpage);
  return _L4_memory_control (0, &attribute); 
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_set_pages_attributes (_L4_word_t nr, _L4_fpage_t *fpages,
			  _L4_word_t *attributes)
{
  _L4_load_mrs (0, nr, (_L4_word_t *) fpages);
  return _L4_memory_control (nr - 1, attributes);
}


/* Now incorporate the public interfaces the user has selected.  */
#ifdef _L4_INTERFACE_L4
#include <l4/compat/misc.h>
#endif
#ifdef _L4_INTERFACE_GNU
#include <l4/gnu/misc.h>
#endif

#endif	/* misc.h */
