/* bits.h - Bit manipulation functions.
   Copyright (C) 2007 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 3 of the
   License, or (at your option) any later version.

   The GNU Hurd is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see
   <http://www.gnu.org/licenses/>.  */

#ifndef RM_BITS_H
#define RM_BITS_H

#include <stdint.h>
#include <assert.h>

/* Return the C bits of word W starting a bit S.  (The LSB is 0 and
   the MSB is L4_WORDSIZE.)  */
static inline unsigned int
extract_bits (unsigned int w, int s, int c)
{
  assert (0 <= s && s < (sizeof (unsigned int) * 8));
  assert (0 <= c && s + c <= (sizeof (unsigned int) * 8));

  if (c == (sizeof (unsigned int) * 8))
    /* 1U << (sizeof (unsigned int) * 8) is problematic: "If the value of
       the right operand is negative or is greater than or equal to
       the width of the promoted left operand, the behavior is
       undefined."  */
    {
      assert (s == 0);
      return w;
    }
  else
    return (w >> s) & ((1ULL << c) - 1);
}

static inline uint64_t
extract_bits64 (uint64_t w, int s, int c)
{
  assert (0 <= s && s < (sizeof (uint64_t) * 8));
  assert (0 <= c && s + c <= (sizeof (uint64_t) * 8));

  if (c == (sizeof (uint64_t) * 8))
    {
      assert (s == 0);
      return w;
    }
  else
    return (w >> s) & ((1ULL << c) - 1);
}

/* Return the C bits of word W ending at bit E.  (The LSB is 0 and the
   MSB is (sizeof (unsigned int) * 8).)  */
static inline unsigned int
extract_bits_inv (unsigned int w, int e, int c)
{
  /* We special case this check here to allow extract_bits_inv (w, 31,
     0).  */
  if (c == 0)
    return 0;

  return extract_bits (w, e - c + 1, c);
}

static inline uint64_t
extract_bits64_inv (uint64_t w, int e, int c)
{
  /* We special case this check here to allow extract_bits64_inv (w, 63,
     0).  */
  if (c == 0)
    return 0;

  return extract_bits64 (w, e - c + 1, c);
}

#endif
