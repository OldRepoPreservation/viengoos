/* vm.h - Virtual memory interface.
   Copyright (C) 2004, 2005 Free Software Foundation, Inc.
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

#ifndef HURD_MM_VM_H
#define HURD_MM_VM_H

#include <errno.h>
#include <stdint.h>

/* Release any bindings between virtual memory and backing stores
   starting at virtual memory starting at ADDRESS and extending SIZE
   bytes.

   If *ADDRESS is an invalid address or SIZE is not a multiple of the
   page size, EINVAL is returned.

   Mapped regions need not be deallocated in their entirety, i.e. it
   is safe to deallocate only part of an existing region.

   If the region contains multiple existing regions each of those
   regions is deallocated in turn.

   If any part of the specified region is not actually mapped, that
   part of the region is ignored.

   Zero is returned on success; an error code is returned on
   failure.  */
extern error_t hurd_vm_release (uintptr_t address, size_t size);

#endif /* HURD_MM_VM_H */
