/* mmap.c - A simple mmap for anonymous memory allocations in task.
   Copyright (C) 2003, 2004, 2005 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

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
   along with the GNU Hurd; see the file COPYING.  If not, write to
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139,
   USA.  */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/mman.h>
#include <errno.h>
#include <hurd/anonymous.h>
#include <hurd/vm.h>

#include "output.h"

void *
mmap (void *address, size_t length, int protect, int flags,
      int filedes, off_t offset)
{
  error_t err;
  uintptr_t a = (uintptr_t) address;

  if (address)
    panic ("mmap called with non-zero ADDRESS");
  if (flags != (MAP_PRIVATE | MAP_ANONYMOUS))
    panic ("mmap called with invalid flags");
  if (protect != (PROT_READ | PROT_WRITE))
    panic ("mmap called with invalid protection");

  err = hurd_anonymous_allocate (&a, length, HURD_ANONYMOUS_ZEROFILL, 0);
  if (err)
    {
      errno = err;
      return MAP_FAILED;
    }

  return (void *) a;
}

int
munmap (void *addr, size_t length)
{
  error_t err;

  /* POSIX says we must round LENGTH up to an even number of pages.
     If ADDR is unaligned, that is an error (which hurd_vm_deallocate
     will catch).  */
  err = hurd_vm_release ((uintptr_t) addr, ((length + getpagesize () - 1)
					    & ~(getpagesize () - 1)));
  if (err)
    {
      errno = err;
      return -1;
    }
  return 0;
}
