/* Copyright (C) 1994, 1995, 1997, 2005 Free Software Foundation, Inc.
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

#include <errno.h>
#include <unistd.h>

/* Return 1 if FD is a terminal, 0 if not.  */
int
__isatty (fd)
     int fd;
{
  error_t err;

  /* FIXME: This is just to make line buffering for our cheap
     stdin/stdout/stderr work.  */
  if (fd == 0 || fd == 1 || fd == 2)
    return 1;

  return 0;
}

weak_alias (__isatty, isatty)
