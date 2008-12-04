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
#include <hurd/bits/math.h>

/* Return 0 if DATA is 0, or the bit number of the most significant
   bit set in DATA.  The least significant bit is 1, the most
   significant bit 32 resp. 64.  */
static inline uintptr_t
vg_msb (uintptr_t data)
{
  if (__builtin_constant_p (data))
    {
#define __VG_MSB_TRY(b) else if (data < (1 << (b))) return (b)
#define __VG_MSB_IS(b) else return (b)

      if (!data)
	return 0;
      __VG_MSB_TRY(1); __VG_MSB_TRY(2); __VG_MSB_TRY(3); __VG_MSB_TRY(4);
      __VG_MSB_TRY(5); __VG_MSB_TRY(6); __VG_MSB_TRY(7); __VG_MSB_TRY(8);
      __VG_MSB_TRY(9); __VG_MSB_TRY(10); __VG_MSB_TRY(11); __VG_MSB_TRY(12);
      __VG_MSB_TRY(13); __VG_MSB_TRY(14); __VG_MSB_TRY(15); __VG_MSB_TRY(16);
      __VG_MSB_TRY(17); __VG_MSB_TRY(18); __VG_MSB_TRY(19); __VG_MSB_TRY(20);
      __VG_MSB_TRY(21); __VG_MSB_TRY(22); __VG_MSB_TRY(23); __VG_MSB_TRY(24);
      __VG_MSB_TRY(25); __VG_MSB_TRY(26); __VG_MSB_TRY(27); __VG_MSB_TRY(28);
      __VG_MSB_TRY(29); __VG_MSB_TRY(30); __VG_MSB_TRY(31);
#if __WORDSIZE == 32
      __VG_MSB_IS(32);
#else
      __VG_MSB_TRY(32); __VG_MSB_TRY(33); __VG_MSB_TRY(34); __VG_MSB_TRY(35);
      __VG_MSB_TRY(36); __VG_MSB_TRY(37); __VG_MSB_TRY(38); __VG_MSB_TRY(39);
      __VG_MSB_TRY(40); __VG_MSB_TRY(41); __VG_MSB_TRY(42); __VG_MSB_TRY(43);
      __VG_MSB_TRY(44); __VG_MSB_TRY(45); __VG_MSB_TRY(46); __VG_MSB_TRY(47);
      __VG_MSB_TRY(48); __VG_MSB_TRY(49); __VG_MSB_TRY(50); __VG_MSB_TRY(51);
      __VG_MSB_TRY(52); __VG_MSB_TRY(53); __VG_MSB_TRY(54); __VG_MSB_TRY(55);
      __VG_MSB_TRY(56); __VG_MSB_TRY(57); __VG_MSB_TRY(58); __VG_MSB_TRY(59);
      __VG_MSB_TRY(60); __VG_MSB_TRY(61); __VG_MSB_TRY(62); __VG_MSB_TRY(63);
      __VG_MSB_IS(64);
#endif
    }

  if (__builtin_expect (data != 0, 1))
    return __VG_msb (data);
  else
    return 0;
}

static inline uintptr_t
vg_msb64 (uint64_t data)
{
#if __WORDSIZE == 64
  return vg_msb (data);
#else
  int d = vg_msb (data >> 32);
  if (d)
    return d + 32;
  return vg_msb (data);
#endif
}

/* Return 0 if DATA is 0, or the bit number of the least significant
   bit set in DATA.  The least significant bit is 1, the most
   significant bit 32 resp. 64.  */
static inline uintptr_t
vg_lsb (uintptr_t data)
{
  if (__builtin_constant_p (data))
    {
#define __VG_LSB_TRY(b) else if (data >= (1 << (b - 1))) return (b)
#define __VG_LSB_IS(b) else return (b)

      if (!data)
	return 0;
#if __WORDSIZE == 64
      __VG_LSB_TRY(64); __VG_LSB_TRY(63); __VG_LSB_TRY(62); __VG_LSB_TRY(61);
      __VG_LSB_TRY(60); __VG_LSB_TRY(59); __VG_LSB_TRY(58); __VG_LSB_TRY(57);
      __VG_LSB_TRY(56); __VG_LSB_TRY(55); __VG_LSB_TRY(54); __VG_LSB_TRY(53);
      __VG_LSB_TRY(52); __VG_LSB_TRY(51); __VG_LSB_TRY(50); __VG_LSB_TRY(49);
      __VG_LSB_TRY(48); __VG_LSB_TRY(47); __VG_LSB_TRY(46); __VG_LSB_TRY(45);
      __VG_LSB_TRY(44); __VG_LSB_TRY(43); __VG_LSB_TRY(42); __VG_LSB_TRY(41);
      __VG_LSB_TRY(40); __VG_LSB_TRY(39); __VG_LSB_TRY(38); __VG_LSB_TRY(37);
      __VG_LSB_TRY(36); __VG_LSB_TRY(35); __VG_LSB_TRY(34); __VG_LSB_TRY(33);
#endif
      __VG_LSB_TRY(32); __VG_LSB_TRY(31); __VG_LSB_TRY(30); __VG_LSB_TRY(29);
      __VG_LSB_TRY(28); __VG_LSB_TRY(27); __VG_LSB_TRY(26); __VG_LSB_TRY(25);
      __VG_LSB_TRY(24); __VG_LSB_TRY(23); __VG_LSB_TRY(22); __VG_LSB_TRY(21);
      __VG_LSB_TRY(20); __VG_LSB_TRY(19); __VG_LSB_TRY(18); __VG_LSB_TRY(17);
      __VG_LSB_TRY(16); __VG_LSB_TRY(15); __VG_LSB_TRY(14); __VG_LSB_TRY(13);
      __VG_LSB_TRY(12); __VG_LSB_TRY(11); __VG_LSB_TRY(10); __VG_LSB_TRY(9);
      __VG_LSB_TRY(8); __VG_LSB_TRY(7); __VG_LSB_TRY(6); __VG_LSB_TRY(5);
      __VG_LSB_TRY(4); __VG_LSB_TRY(3); __VG_LSB_TRY(2); __VG_LSB_IS(1);
    }

  if (__builtin_expect (data != 0, 1))
    return __VG_lsb (data);
  else
    return 0;
}

static inline uintptr_t
vg_lsb64 (uint64_t data)
{
#if __WORDSIZE == 64
  return vg_lsb (data);
#else
  int d = vg_lsb (data);
  if (d)
    return d;
  d = vg_lsb (data >> 32);
  if (d)
    return d + 32;
  return 0;
#endif
}

#endif
