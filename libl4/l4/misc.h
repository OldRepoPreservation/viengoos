/* misc.h - Public interface to L4 miscellaneous functions.
   Copyright (C) 2003 Free Software Foundation, Inc.
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

#define L4_DEFAULT_MEMORY	0x0

static inline void
__attribute__((__always_inline__))
l4_set_page_attribute (l4_fpage_t fpage, l4_word_t attribute)
{
  l4_set_rights (&fpage, 0);
  l4_load_mr (0, fpage.raw);
  l4_memory_control (0, &attribute); 
}


static inline void
__attribute__((__always_inline__))
l4_set_pages_attributes (l4_word_t nr, l4_fpage_t *fpages,
			 l4_word_t *attributes)
{
  l4_load_mrs (0, nr, (l4_word_t *) fpages);
  l4_memory_control (nr - 1, attributes);
}

#endif	/* misc.h */
