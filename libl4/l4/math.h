/* l4/math.h - Public interface to L4 mathematical support functions.
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

#ifndef _L4_MATH_H
#define _L4_MATH_H	1

#include <l4/features.h>
#include <l4/types.h>

/* The architecture specific file defines __L4_msb() and __L4_lsb().  */
#include <l4/bits/math.h>


/* Return 0 if DATA is 0, or the bit number of the most significant
   bit set in DATA.  The least significant bit is 1, the most
   significant bit 32 resp. 64.  */
static inline _L4_word_t
_L4_attribute_always_inline
_L4_msb (_L4_word_t data)
{
  if (__builtin_constant_p (data))
    {
#define __L4_MSB_TRY(b) else if (data < (1 << (b))) return (b)
#define __L4_MSB_IS(b) else return (b)

      if (!data)
	return 0;
      __L4_MSB_TRY(1); __L4_MSB_TRY(2); __L4_MSB_TRY(3); __L4_MSB_TRY(4);
      __L4_MSB_TRY(5); __L4_MSB_TRY(6); __L4_MSB_TRY(7); __L4_MSB_TRY(8);
      __L4_MSB_TRY(9); __L4_MSB_TRY(10); __L4_MSB_TRY(11); __L4_MSB_TRY(12);
      __L4_MSB_TRY(13); __L4_MSB_TRY(14); __L4_MSB_TRY(15); __L4_MSB_TRY(16);
      __L4_MSB_TRY(17); __L4_MSB_TRY(18); __L4_MSB_TRY(19); __L4_MSB_TRY(20);
      __L4_MSB_TRY(21); __L4_MSB_TRY(22); __L4_MSB_TRY(23); __L4_MSB_TRY(24);
      __L4_MSB_TRY(25); __L4_MSB_TRY(26); __L4_MSB_TRY(27); __L4_MSB_TRY(28);
      __L4_MSB_TRY(29); __L4_MSB_TRY(30); __L4_MSB_TRY(31);
#if _L4_WORDSIZE == _L4_WORDSIZE_32
      __L4_MSB_IS(32);
#else
      __L4_MSB_TRY(32); __L4_MSB_TRY(33); __L4_MSB_TRY(34); __L4_MSB_TRY(35);
      __L4_MSB_TRY(36); __L4_MSB_TRY(37); __L4_MSB_TRY(38); __L4_MSB_TRY(39);
      __L4_MSB_TRY(40); __L4_MSB_TRY(41); __L4_MSB_TRY(42); __L4_MSB_TRY(43);
      __L4_MSB_TRY(44); __L4_MSB_TRY(45); __L4_MSB_TRY(46); __L4_MSB_TRY(47);
      __L4_MSB_TRY(48); __L4_MSB_TRY(49); __L4_MSB_TRY(50); __L4_MSB_TRY(51);
      __L4_MSB_TRY(52); __L4_MSB_TRY(53); __L4_MSB_TRY(54); __L4_MSB_TRY(55);
      __L4_MSB_TRY(56); __L4_MSB_TRY(57); __L4_MSB_TRY(58); __L4_MSB_TRY(59);
      __L4_MSB_TRY(60); __L4_MSB_TRY(61); __L4_MSB_TRY(62); __L4_MSB_TRY(63);
      __L4_MSB_IS(64)
#endif
    }

  if (__builtin_expect (data != 0, 1))
    return __L4_msb (data);
  else
    return 0;
}


/* Return 0 if DATA is 0, or the bit number of the least significant
   bit set in DATA.  The least significant bit is 1, the most
   significant bit 32 resp. 64.  */
static inline _L4_word_t
_L4_attribute_always_inline
_L4_lsb (_L4_word_t data)
{
  if (__builtin_constant_p (data))
    {
#define __L4_LSB_TRY(b) else if (data >= (1 << (b - 1))) return (b)
#define __L4_LSB_IS(b) else return (b)

      if (!data)
	return 0;
#if _L4_WORDSIZE == _L4_WORDSIZE_64
      __L4_LSB_TRY(64); __L4_LSB_TRY(63); __L4_LSB_TRY(62); __L4_LSB_TRY(61);
      __L4_LSB_TRY(60); __L4_LSB_TRY(59); __L4_LSB_TRY(58); __L4_LSB_TRY(57);
      __L4_LSB_TRY(56); __L4_LSB_TRY(55); __L4_LSB_TRY(54); __L4_LSB_TRY(53);
      __L4_LSB_TRY(52); __L4_LSB_TRY(51); __L4_LSB_TRY(50); __L4_LSB_TRY(49);
      __L4_LSB_TRY(48); __L4_LSB_TRY(47); __L4_LSB_TRY(46); __L4_LSB_TRY(45);
      __L4_LSB_TRY(44); __L4_LSB_TRY(43); __L4_LSB_TRY(42); __L4_LSB_TRY(41);
      __L4_LSB_TRY(40); __L4_LSB_TRY(39); __L4_LSB_TRY(38); __L4_LSB_TRY(37);
      __L4_LSB_TRY(36); __L4_LSB_TRY(35); __L4_LSB_TRY(34); __L4_LSB_TRY(33);
#endif
      __L4_LSB_TRY(32); __L4_LSB_TRY(31); __L4_LSB_TRY(30); __L4_LSB_TRY(29);
      __L4_LSB_TRY(28); __L4_LSB_TRY(27); __L4_LSB_TRY(26); __L4_LSB_TRY(25);
      __L4_LSB_TRY(24); __L4_LSB_TRY(23); __L4_LSB_TRY(22); __L4_LSB_TRY(11);
      __L4_LSB_TRY(20); __L4_LSB_TRY(19); __L4_LSB_TRY(18); __L4_LSB_TRY(17);
      __L4_LSB_TRY(16); __L4_LSB_TRY(15); __L4_LSB_TRY(14); __L4_LSB_TRY(13);
      __L4_LSB_TRY(12); __L4_LSB_TRY(11); __L4_LSB_TRY(10); __L4_LSB_TRY(9);
      __L4_LSB_TRY(8); __L4_LSB_TRY(7); __L4_LSB_TRY(6); __L4_LSB_TRY(5);
      __L4_LSB_TRY(4); __L4_LSB_TRY(3); __L4_LSB_TRY(2); __L4_LSB_IS(1);
    }

  if (__builtin_expect (data != 0, 1))
    return __L4_lsb (data);
  else
    return 0;
}


/* Now incorporate the public interfaces the user has selected.  */
#ifdef _L4_INTERFACE_GNU
#include <l4/gnu/math.h>
#endif

#endif	/* l4/math.h */
