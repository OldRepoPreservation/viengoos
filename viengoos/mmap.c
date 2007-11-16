/* mmap.c - A simple mmap for anonymous memory allocations in physmem.
   Copyright (C) 2003, 2007 Free Software Foundation, Inc.
   Written by Marcus Brinkmann.

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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <hurd/stddef.h>
#include <sys/mman.h>

#include "zalloc.h"


void *
mmap (void *address, size_t length, int protect, int flags,
      int filedes, off_t offset)
{
  debug (4, "Allocation request for %d bytes", length);

  if (address)
    panic ("mmap called with non-zero ADDRESS");
  if (flags != (MAP_PRIVATE | MAP_ANONYMOUS))
    panic ("mmap called with invalid flags");
  if (protect != (PROT_READ | PROT_WRITE))
    panic ("mmap called with invalid protection");

  /* At this point, we can safely ignore FILEDES and OFFSET.  */
  void *r = ((void *) zalloc (length)) ?: (void *) -1;
  debug (4, "=> %p", r);
  return r;
}


int
munmap (void *addr, size_t length)
{
  zfree ((l4_word_t) addr, length);
  return 0;
}
