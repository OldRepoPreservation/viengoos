/* nanosleep.c - Sleep for a period specified with a struct timespec.
   Copyright (C) 2002, 2005 Free Software Foundation, Inc.
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


/* Pause execution for a number of nanoseconds.  */
int
__nanosleep (const struct timespec *requested_time,
	     struct timespec *remaining_time)
{
  struct timeval before;
  struct timeval after;
  l4_uint64_t usecs;

  usecs = requested_time->tv_sec;
  usecs = usecs * 1000 * 1000;
  usecs = usecs + (((l4_uint64_t) requested_time->tv_nsec) + 999) / 1000;

  if (remaining_time && __gettimeofday (&before, NULL) < 0)
    return -1;
  l4_sleep (usecs);
  if (remaining_time && __gettimeofday (&after, NULL) < 0)
    return -1;

  if (remaining_time)
    {
      struct timeval requested;
      struct timeval elapsed;
      struct timeval remaining;

      TIMESPEC_TO_TIMEVAL (&requested, requested_time);

      timersub (&after, &before, &elapsed);
      if (timercmp (&requested, &elapsed, <))
	timerclear (&remaining);
      else
	timersub (&requested, &elapsed, &remaining);

      TIMEVAL_TO_TIMESPEC (&remaining, remaining_time);
    }

  return 0;
}
libc_hidden_def (__nanosleep)
weak_alias (__nanosleep, nanosleep)
