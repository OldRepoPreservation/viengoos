/* l4/math.h - Public interface to Hurd mathematical support functions.
   Copyright (C) 2003, 2004, 2007, 2008 Free Software Foundation, Inc.
   Written by Marcus Brinkmann <marcus@gnu.org>.

   This file is part of the GNU Hurd.
 
   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License
   as published by the Free Software Foundation; either version 2.1 of
   the License, or (at your option) any later version.
 
   The GNU Hurd is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.
 
   You should have received a copy of the GNU Lesser General Public
   License along with the GNU L4 library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#ifndef _HURD_MATH_H
#define _HURD_MATH_H

#include <stdint.h>
#include <bits/wordsize.h>

/* Return 0 if DATA is 0, or the bit number of the most significant
   bit set in DATA.  The least significant bit is 1, the most
   significant bit 32 resp. 64.  */
static inline uintptr_t
vg_msb (uintptr_t data)
{
  if (! data)
    return 0;

#if __WORDSIZE == 64
  return 8 * sizeof (data)  - __builtin_clzll (data);
#else
  return 8 * sizeof (data)  - __builtin_clz (data);
#endif
}

static inline uintptr_t
vg_msb64 (uint64_t data)
{
  if (! data)
    return 0;

  return 8 * sizeof (data)  - __builtin_clzll (data);
}

/* Return 0 if DATA is 0, or the bit number of the least significant
   bit set in DATA.  The least significant bit is 1, the most
   significant bit 32 resp. 64.  */
static inline uintptr_t
vg_lsb (uintptr_t data)
{
#if __WORDSIZE == 64
  return __builtin_ffsll (data);
#else
  return __builtin_ffs (data);
#endif
}

static inline uintptr_t
vg_lsb64 (uint64_t data)
{
  return __builtin_ffsll (data);
}

#endif
