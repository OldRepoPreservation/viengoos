/* mmap.c - A simple mmap for anonymous memory allocations in task.
   Copyright (C) 2003, 2004 Free Software Foundation, Inc.
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

#include <sys/mman.h>
#include <pthread.h>

#include <hurd/startup.h>

#include "output.h"
#include "physmem-user.h"


/* Initialized by the machine-specific startup-code.  */
extern struct hurd_startup_data *__hurd_startup_data;


/* FIXME: All of this is totally lame.  We just know that physmem
   always allocates new memory for addresses above 0x8000000.  We also
   know that there is free virtual address space at 0x9000000.  This
   must be replaced by some real memory manager of course.  */
static void *free_vaddr = ((void *) 0x9000000);

static pthread_mutex_t vaddr_lock = PTHREAD_MUTEX_INITIALIZER;


void *
mmap (void *address, size_t length, int protect, int flags,
      int filedes, off_t offset)
{
  error_t err;

  if (address)
    panic ("mmap called with non-zero ADDRESS");
  if (flags != (MAP_PRIVATE | MAP_ANONYMOUS))
    panic ("mmap called with invalid flags");
  if (protect != (PROT_READ | PROT_WRITE))
    panic ("mmap called with invalid protection");

  /* At this point, we can safely ignore FILEDES and OFFSET.  */

  pthread_mutex_lock (&vaddr_lock);
  address = free_vaddr;
  err = physmem_map (__hurd_startup_data->image.server,
		     __hurd_startup_data->image.cap_handle,
		     ((l4_word_t) address) | L4_FPAGE_FULLY_ACCESSIBLE,
		      length, address);
  if (!err)
    free_vaddr += length;
  pthread_mutex_unlock (&vaddr_lock);

  return err ? MAP_FAILED : address;
}


int
munmap (void *addr, size_t length)
{
  panic ("munmap() not implemented");
  return 0;
}
