/* l4/bits/gnu/space.h - GNU L4 space features for ia32.
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
# error "Never use <l4/bits/gnu/space.h> directly; include <l4/space.h> instead."
#endif


/* IO Fpages.  */

static inline l4_fpage_t
l4_io_fpage (l4_word_t base_address, int size)
{
  return _L4_io_fpage (base_address, size);
}

static inline l4_fpage_t
l4_io_fpage_log2 (l4_word_t base_address, int log2_size)
{
  return _L4_io_fpage_log2 (base_address, log2_size);
}


/* l4_space_control control argument.  */

#define L4_LARGE_SPACE	_L4_LARGE_SPACE
#define L4_SMALL_SPACE	_L4_SMALL_SPACE

static inline l4_word_t
l4_small_space (l4_word_t loc, l4_word_t size)
{
  return _L4_small_space (loc, size);
}
