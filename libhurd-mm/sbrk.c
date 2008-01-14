/* sbrk.c - sbrk implementation.
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

#include <stdint.h>
#include <errno.h>
#include <sys/mman.h>
#include <assert.h>
#include <hurd/mutex.h>

/* We need an implementation for sbrk for glibc, which uses it before
   malloc is available.  In a real sbrk implementation, the initial
   break value is set to the end of the data segment.  We don't do
   that; we just allocate a block of memory using mmap and maintain a
   pointer into it.  */

static ss_mutex_t lock;
static void *endds;
static void *brk;

void *
sbrk (intptr_t inc)
{
  ss_mutex_lock (&lock);

  if (! endds)
    {
      /* sbrk isn't used that much so 16MB of virtual address space
	 should be enough.  */
#define SIZE 16 * 1024 * 1024
      endds = mmap (0, SIZE, PROT_READ | PROT_WRITE,
		    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
      if (endds == MAP_FAILED)
	{
	  ss_mutex_unlock (&lock);
	  errno = ENOMEM;
	  return (void *) -1;
	}

      brk = endds;
    }

  void *p = brk;
  brk += inc;

  assert (brk >= endds);

  if (brk > endds + SIZE)
    {
      brk -= inc;
      errno = ENOMEM;
      p = (void *) -1;
    }

  ss_mutex_unlock (&lock);

  return p;
}
