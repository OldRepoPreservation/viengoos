/* physmem.h - Interfaces exported by physmem.
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

#ifndef HURD_PHYSMEM_H
#define HURD_PHYSMEM_H

/* Execute permission.  */
#define HURD_PM_CONT_EXECUTE		(1 << 0)
/* Write permission.  */
#define HURD_PM_CONT_WRITE		(1 << 1)
/* Read permission.  */
#define HURD_PM_CONT_READ		(1 << 2)
/* Read and write permission.  */
#define HURD_PM_CONT_RW			(HURD_PM_CONT_READ|HURD_PM_CONT_WRITE)
/* Read, write and execute.  */
#define HURD_PM_CONT_RWX		(HURD_PM_CONT_RW|HURD_PM_CONT_EXECUTE)

/* Don't copy on write (COW), simply share (a la SYSV SHM).  */
#define HURD_PM_CONT_COPY_SHARED	(1 << 3)
/* Don't copy the region, move it.  */
#define HURD_PM_CONT_COPY_MOVE		(1 << 4)

/* Either completely fail or completely succeed: don't partially
   succeed.  */
#define HURD_PM_CONT_ALL_OR_NONE	(1 << 8)

/* Do not fail if the specified identifier is already in use.
   Instead, deallocate the current frame and allocate a new one in its
   place.  This is useful only to shortcut explicit deallocation
   requests; using it to avoid EEXIST error messages will lead to
   problems as it suggests that the client is not keep track of
   frames.  */
#define HURD_PM_CONT_ALLOC_SQUASH	(1 << 9)
/* Allocate extra frames if needed.  */
#define HURD_PM_CONT_ALLOC_EXTRA	(1 << 10)
/* Only allocate frames suitable for DMA.  */
#define HURD_PM_CONT_ALLOC_DMA		(1 << 11)

/* RPC Identifiers.  */
enum
  {
    hurd_pm_container_create_id = 130,
    hurd_pm_container_share_id,
    hurd_pm_container_allocate_id,
    hurd_pm_container_deallocate_id,
    hurd_pm_container_map_id,
    hurd_pm_container_copy_id
  };

#include <hurd/types.h>

/* Memory control object.  */
typedef hurd_cap_handle_t hurd_pm_control_t;

/* Container.  */
typedef hurd_cap_handle_t hurd_pm_container_t;

#endif /* HURD_PHYSMEM_H */
