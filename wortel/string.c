/* string.c - Some minimal string routines.
   Copyright (C) 2003 Free Software Foundation, Inc.
   Written by Marcus Brinkmann.

   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2, or (at
   your option) any later version.

   The GNU Hurd is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA. */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "string.h"


int
strcmp (const char *s1, const char *s2)
{
  while (*s1 && *s2 && *s1 == *s2)
    {
      s1++;
      s2++;
    }
  return (*s1) - (*s2);
}


void *
memcpy (void *dest, const void *src, int nr)
{
  char *from = (char *) src;
  char *to = (char *) dest;

  while (nr--)
    *(to++) = *(from++);

  return dest;
}


void *
memset (void *str, int chr, int nr)
{
  char *addr = (char *) str;

  while (nr--)
    *(addr++) = (char) (chr & 0xff);

  return str;
}
