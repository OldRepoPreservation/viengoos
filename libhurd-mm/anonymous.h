/* anonymous.h - Standard anonymous memory.
   Copyright (C) 2005 Free Software Foundation, Inc.
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

#ifndef HURD_ANONYMOUS_H
#define HURD_ANONYMOUS_H

#include <stdint.h>

/* Require zero filled anonymous memory.  */
#define HURD_ANONYMOUS_ZEROFILL (1 << 16)

/* Initialize the anonymous memory subsystem.  */
extern void hurd_anonymous_init (void);

/* Allocate a region of virtual memory to be backed by anonymous
   storage.  (Memory is allocated on demand and multiplexed.)  The
   region initially has read and write access permissions set.  The
   region is initially set to be inherited COW by its children.

   If FLAGS contains HURD_VM_HERE, the region starting at *ADDRESS and
   continuing for SIZE bytes shall be used.  If *ADDRESS is an invalid
   address, EINVAL is returned.  If any part of the specified region
   is in use, the operation fails and EEXIST is returned.

   If FLAGS does not contain HURD_VM_HERE and there is insufficient
   space in the address space, ENOSPC is returned.

   If FLAGS contains VM_ZEROFILL, the backing memory is zerod,
   otherwise, the contents are undefined.

   If SIZE is not a multiple of the page size, EINVAL is returned.

   If MAP_NOW is true, then physical memory is allocated and a mapping
   established before the function returns.  This is useful when the
   pager is not up yet or the caller knows that the region will be
   immediately accessed thus eliding the overhead of the fault.

   If there is insufficient memory to perform the operation, ENOMEM is
   returned.

   On success, *ADDRESS contains the address of the start of the
   region and zero is returned.  */
/* Set of a region SIZE bytes long backed by anonymous memory.  If
   FLAGS contains MAP_FIXED then *ADDR is used as the virtual memory
   address to use.  If this is already in use, EEXIST is returned.
   The virtual memory address where the mapping is done is returned in
   *ADDR.  If the caller knows the region will be accessed
   immediately, it may pass the required access permissions in MAP_NOW
   the map will be installed immediately.  */
extern error_t hurd_anonymous_allocate (uintptr_t *addr, size_t size,
					uintptr_t flags, int map_now);

#endif /* HURD_ANONYMOUS_H */
