/* l4/bits/compat/space.h - L4 space features for ia32.
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

#ifndef _L4_SPACE_H
# error "Never use <l4/bits/compat/space.h> directly; include <l4/space.h> instead."
#endif


/* IO Fpages.  */

static inline L4_Fpage_t
_L4_attribute_always_inline
L4_IoFpage (L4_Word_t base_address, int size)
{
  L4_Fpage_t f;

  f.raw = _L4_io_fpage (base_address, size);
  return f;
}


static inline L4_Fpage_t
_L4_attribute_always_inline
L4_IoFpageLog2 (L4_Word_t base_address, int size_log2)
{
  L4_Fpage_t f;

  f.raw = _L4_io_fpage_log2 (base_address, size_log2);
  return f;
}


/* L4_SpaceControl control argument.  */

#define L4_LargeSpace	_L4_LARGE_SPACE

static inline L4_Word_t
_L4_attribute_always_inline
L4_SmallSpace (L4_Word_t loc, L4_Word_t size)
{
  return _L4_small_space (loc, size);
}
