/* Copyright (C) 1992, 1993, 1994, 1997, 2005 Free Software Foundation, Inc.
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

#include <signal.h>
#include <time.h>
#include <unistd.h>


/* Make the process sleep for SECONDS seconds, or until a signal arrives
   and is not ignored.  The function returns the number of seconds less
   than SECONDS which it actually slept (zero if it slept the full time).
   There is no return value to indicate error, but if `sleep' returns
   SECONDS, it probably didn't work.  */
unsigned int
__sleep (unsigned int seconds)
{
  time_t before, after;
  unsigned int elapsed;
  l4_uint64_t usecs;
  l4_time_t period;

  usecs = seconds;
  usecs = usecs * 1000 * 1000;
  period = l4_time_period (usecs);

  before = time ((time_t *) NULL);
  /* FIXME: This can return before the time elapsed even if we are not
     interrupted, in case the cpu clock is fast.  */
  l4_sleep (period);
  after = time ((time_t *) NULL);

  elapsed = after - before;

  return elapsed > seconds ? 0 : seconds - elapsed;
}
weak_alias (__sleep, sleep)
