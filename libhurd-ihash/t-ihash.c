/* t-ihash.c - Integer-keyed hash table function unit-tests.
   Copyright (C) 2007 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.
   
   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   The GNU Hurd is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with the GNU Hurd; see the file COPYING.  If not, write to
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#define _GNU_SOURCE

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <stdio.h>

#include "ihash.h"

#define F(format, args...) \
  ({ \
    printf ("fail: line %d ", __LINE__); \
    printf (format "... ", ##args); \
    return 1; \
  })

int
main (int argc, char *argv[])
{
  struct hurd_ihash hash;

  hurd_ihash_init (&hash, HURD_IHASH_NO_LOCP);

  printf ("Testing hurd_ihash_add... ");

  int k;
  hurd_ihash_value_t v;
  for (k = 1; k < 100; k ++)
    if (hurd_ihash_add (&hash, (hurd_ihash_key_t) k,
			(hurd_ihash_value_t) k) != 0)
      F ("failed to insert %d\n", k);
  for (k = 1; k < 100; k ++)
    {
      v = hurd_ihash_find (&hash, (hurd_ihash_key_t) k);
      if (v != (hurd_ihash_value_t) k)
	F ("unexpected value %d for key %d\n", (int) v, k);
    }

  printf ("ok\n");

  printf ("Testing hurd_ihash_replace... ");

  bool had_value;
  for (k = 1; k < 100; k ++)
    if (hurd_ihash_replace (&hash,
			    (hurd_ihash_key_t) k,
			    (hurd_ihash_value_t) k + 1,
			    &had_value, &v) != 0)
      F ("failed to replace key %d\n", k);
    else if (! had_value)
      F ("No value for key %d\n", k);
    else if ((int) v != k)
      F ("Value for key %d is %d, not %d\n", k, (int) v, k);

  for (k = 100; k < 200; k ++)
    if (hurd_ihash_replace (&hash,
			    (hurd_ihash_key_t) k,
			    (hurd_ihash_value_t) (k + 1),
			    &had_value, &v) != 0)
      F ("failed to replace value of key %d\n", k);
    else if (had_value)
      F ("key %d had the unexpected value %d!\n", k, (int) v);

  for (k = 1; k < 100; k ++)
    if (hurd_ihash_replace (&hash, (hurd_ihash_key_t) k,
			    (hurd_ihash_value_t) k,
			    &had_value, &v) != 0)
      F ("failed to replace the value of key %d\n", k);
    else if (! had_value)
      F ("No value for key %d\n", k);
    else if ((int) v != k + 1)
      F ("Value for key %d is %d, not %d\n", k, (int) v, k + 1);

  printf ("ok\n");

  return 0;
}
