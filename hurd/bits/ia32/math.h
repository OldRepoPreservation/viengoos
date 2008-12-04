/* hurd/math.h - Public interface to Hurd mathematical support functions.
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

#ifndef _HURD_BITS_MATH_H
#define _HURD_BITS_MATH_H

#include <stdint.h>

/* Calculate the MSB set in DATA.  DATA is not 0.  */
static inline uintptr_t
__VG_msb (uintptr_t data)
{
  uintptr_t msb;

  __asm__ ("bsr %[data], %[msb]"
	   : [msb] "=r" (msb)
	   : [data] "rm" (data));

  return msb + 1;
}


/* Calculate the LSB set in DATA.  DATA is not 0.  */
static inline uintptr_t
__VG_lsb (uintptr_t data)
{
  uintptr_t lsb;

  __asm__ ("bsf %[data], %[lsb]"
	   : [lsb] "=r" (lsb)
	   : [data] "rm" (data));

  return lsb + 1;
}

#endif
