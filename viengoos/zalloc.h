/* Zone allocator for physical memory server.
   Copyright (C) 2003, 2007 Free Software Foundation, Inc.
   Written by Neal H Walfield.

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

#ifndef __ZALLOC_H__
#define __ZALLOC_H__

#include <stdint.h>

/* Add to the pool the block BLOCK of size SIZE.  BLOCK must be
   aligned to the system's minimum page size.  SIZE must be a multiple
   of the system's minimum page size.  */
void zfree (uintptr_t block, uintptr_t size);

/* Allocate a block of memory of size SIZE.  SIZE must be a multiple
   of the system's minimum page size. */
uintptr_t zalloc (uintptr_t size);

/* Dump some internal data structures.  Only defined if zalloc was
   compiled without NDEBUG defined.  */
void zalloc_dump_zones (const char *prefix);

#endif	/* __ZALLOC_H__ */
