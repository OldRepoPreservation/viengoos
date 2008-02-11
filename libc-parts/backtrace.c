/* backtrace.c - Gather a backtrace.
   Copyright (C) 2008 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU Hurd is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#define RA(level)							\
  if (level < size && __builtin_frame_address ((level) + 1))		\
    {									\
      array[level] = __builtin_return_address ((level) + 1);		\
      if (array[level] == 0)						\
	return (level) + 1;						\
    }									\
  else									\
    return level;

int
backtrace (void **array, int size)
{
  RA(0);
  RA(1);
  RA(2);
  RA(3);
  RA(4);
  RA(5);
  RA(6);
  RA(7);
  RA(9);
  RA(10);
  RA(11);
  RA(12);
  RA(13);
  RA(14);
  RA(15);
  RA(16);
  RA(17);
  RA(18);
  RA(19);
  RA(20);
  return 21;
}
