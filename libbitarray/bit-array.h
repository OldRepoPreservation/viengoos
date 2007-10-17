/* bit-array.h - Bit array manipulation functions.
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

#ifndef _BIT_ARRAY_H_
#define _BIT_ARRAY_H_

#include <stdbool.h>
#include <assert.h>

/* Set bit BIT in array ARRAY (which is SIZE bytes long).  Returns
   true if bit was set, false otherwise.  */
static inline bool
bit_set (unsigned char *array, int size, int bit)
{
  assert (bit >= 0);
  assert (bit < size * 8);

  if ((array[bit / 8] & (1 << (bit & 0x7))))
    /* Already set!  */
    return false;

  array[bit / 8] |= 1 << (bit & 0x7);
  return true;  
}

/* Allocate the first free (zero) bit starting at bit START_BIT.  SIZE
   is the size of ARRAY (in bytes).  Returns -1 on failure, otherwise
   the bit allocated.  */
static inline int
bit_alloc (unsigned char *array, int size, int start_bit)
{
  int first_free_bit[]
    = { /* 0000 */ 0, /* 0001 */ 1, /* 0010 */ 0, /* 0011 */ 2,
        /* 0100 */ 0, /* 0101 */ 1, /* 0110 */ 0, /* 0111 */ 3,
        /* 1000 */ 0, /* 1001 */ 1, /* 1010 */ 0, /* 1011 */ 2,
        /* 1100 */ 0, /* 1101 */ 1, /* 1110 */ 0, /* 1111 */ -1 };

  int check_byte (unsigned char byte)
    {
      /* Check the lower four bits.  */
      int b = first_free_bit[byte & 0xf];
      if (b != -1)
	return b;

      /* Check the uppoer four bits.  */
      b = first_free_bit[byte >> 4];
      if (b != -1)
	return 4 + b;

      return -1;
    }

  assert (0 <= start_bit);
  assert (start_bit < size * 8);

  int start_byte = start_bit / 8;
  int byte = start_byte;
  unsigned char b;

  if ((start_bit & 0x7) != 0)
    /* We don't start on a byte boundary.  */
    {
      b = array[byte] | ((1 << (start_bit & 0x7)) - 1);
      goto check;
    }

  do
    {
      int bit;
      b = array[byte];
    check:
      bit = check_byte (b);
      if (bit != -1)
	/* Got one!  */
	{
	  array[byte] |= 1 << bit;
	  return byte * 8 + bit;
	}

      byte ++;
      if (byte == size)
	byte = 0;
    }
  while (byte != start_byte);

  if ((start_bit & 0x7) != 0)
    {
      b = array[byte] | ~((1 << (start_bit & 0x7)) - 1);
      int bit = check_byte (b);
      if (bit != -1)
	/* Got one!  */
	{
	  array[byte] |= 1 << bit;
	  return byte * 8 + bit;
	}
    }

  return -1;
}

static inline void
bit_dealloc (unsigned char *array, int bit)
{
  assert ((array[bit / 8] & (1 << (bit & 0x7))));

  /* Clear bit.  */
  array[bit / 8] &= ~(1 << (bit & 0x7));
}

static inline bool
bit_test (unsigned char *array, int bit)
{
  return !! (array[bit / 8] & (1 << (bit & 0x7)));
}

#endif
