/* l4/bits/space.h - L4 spaces definitions for ia32.
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

#ifndef _L4_SPACE_H
# error "Never use <l4/bits/space.h> directly; include <l4/space.h> instead."
#endif

#include <l4/math.h>


/* IO Fpages.  */

typedef _L4_word_t _L4_io_fpage_t;

typedef _L4_RAW
(_L4_io_fpage_t, _L4_STRUCT1
 ({
   _L4_BITFIELD4
     (_L4_word_t,
      _L4_BITFIELD (rights, 4),
      _L4_BITFIELD (_two, 2),
      _L4_BITFIELD (log2_size, 6),
      _L4_BITFIELD_32_64 (base, 16, 48));
 })) __L4_io_fpage_t;


static inline _L4_fpage_t
_L4_attribute_always_inline
_L4_io_fpage (_L4_word_t base_address, int size)
{
  __L4_io_fpage_t io_fpage;
  _L4_word_t msb = _L4_msb (size);

  io_fpage.rights = 0;
  io_fpage._two = 2;
  io_fpage.log2_size = (1 << msb) == size ? msb : msb + 1;
  io_fpage.base = base_address;
  return io_fpage.raw;
}


static inline _L4_fpage_t
_L4_attribute_always_inline
_L4_io_fpage_log2 (_L4_word_t base_address, int log2_size)
{
  __L4_io_fpage_t io_fpage;

  io_fpage.rights = 0;
  io_fpage._two = 2;
  io_fpage.log2_size = log2_size;
  io_fpage.base = base_address;
  return io_fpage.raw;
}


/* _L4_space_control control argument.  */

#define _L4_LARGE_SPACE		(_L4_WORD_C(0))
#define _L4_SMALL_SPACE		(_L4_WORD_C(1) << 31)


/* LOC and SIZE are in MB.  */
static inline _L4_word_t
_L4_attribute_always_inline
_L4_small_space (_L4_word_t loc, _L4_word_t size)
{
  _L4_word_t small_space = loc >> 1;	/* Divide by 2 (MB).  */
  _L4_word_t two_pow_p = size >> 2;	/* Divide by 4 (MB).  */

  /* Make P the LSB of small_space.  */
  small_space = (small_space & ~(two_pow_p - 1)) | two_pow_p;
  return small_space & 0xff;
}


/* Now incorporate the public interfaces the user has selected.  */
#ifdef _L4_INTERFACE_L4
#include <l4/bits/compat/space.h>
#endif
#ifdef _L4_INTERFACE_GNU
#include <l4/bits/gnu/space.h>
#endif
