/* Copyright (C) 1992,93,94,97,98,99, 2001, 2005 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#include <l4.h>

#include <errno.h>
#include <sys/time.h>
#include <unistd.h>

/* Sleep USECONDS microseconds, or until a previously set timer goes off.  */
int
usleep (useconds_t useconds)
{
  l4_sleep (useconds);

  return 0;
}
