/* malloc-wrap.c: Doug Lea's malloc for the physical memory server.
   Copyright (C) 2003 Free Software Foundation, Inc.
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

/* Configuration of Doug Lea's malloc.  */

#include <errno.h>

#include <l4.h>

#include "zalloc.h"

#define __STD_C 1
#define LACKS_UNISTD_H
#define LACKS_SYS_PARAM_H
#define LACKS_SYS_MMAN_H
#define LACKS_FCNTL_H

/* We want to use optimized versions of memset and memcpy.  */
#define HAVE_MEMCPY

/* We always use the supplied mmap emulation.  */
#define MORECORE(x) MORECORE_FAILURE
#define HAVE_MMAP 1
#define HAVE_MREMAP 0
#define MUNMAP_FAILURE  (-1)
#define MMAP_CLEARS 1
#define malloc_getpagesize l4_min_page_size ()
#define mmap(addr, size, p, f, fd, o) (zalloc (size) ?: MUNMAP_FAILURE)
#define munmap(addr, size) (zfree ((l4_word_t) addr, size), 0)
#define MMAP_AS_MORECORE_SIZE (16 * malloc_getpagesize)
#define DEFAULT_MMAP_THRESHOLD (4 * malloc_getpagesize)

/* These values don't really matter in mmap emulation */
#define MAP_PRIVATE 1
#define MAP_ANONYMOUS 2
#define PROT_READ 1
#define PROT_WRITE 2

/* Suppress debug output in mstats().  */
#define fprintf(...)

/* Now include Doug Lea's malloc.  */
#include "malloc.c"
