/* physmem-user.h - physmem client stubs.
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

#ifndef HURD_PHYSMEM_USER_H
#define HURD_PHYSMEM_USER_H	1

#include <stdint.h>
#include <sys/types.h>
#include <l4.h>
#include <hurd/types.h>


/* Memory control object.  */
typedef hurd_cap_handle_t hurd_pm_control_t;

/* Container.  */
typedef hurd_cap_handle_t hurd_pm_container_t;

/* A limited access capability for a container.  */
typedef hurd_cap_handle_t hurd_pm_container_access_t;

/* If an error occurs allocating a frame, return what has succeeded so
   far and return the error code for the failing index.  */
#define HURD_PM_CONT_ALLOC_PARTIAL	(1 << 0)
/* Do not fail if the specified identifier is already in use.
   Instead, deallocate the current frame and allocate a new one in its
   place.  This is useful only to shortcut explicit deallocation
   requests; using it to avoid EEXIST error messages will lead to
   problems as it suggests that the client is not keep track of
   frames.  */
#define HURD_PM_CONT_ALLOC_SQUASH	(1 << 1)
/* Allocate extra frames if needed.  */
#define HURD_PM_CONT_ALLOC_EXTRA	(1 << 2)
/* Only allocate frames suitable for DMA.  */
#define HURD_PM_CONT_ALLOC_DMA		(1 << 3)

/* Create a container in *CONTAINER managed by the physical memory
   server from the contol memory capability CONTROL.  */
extern error_t hurd_pm_container_create (hurd_pm_control_t control,
					 hurd_pm_container_t *container);

/* Create a limited access capability for container CONTAINER, return
   in *ACCESS.  */
extern error_t hurd_pm_container_share (hurd_pm_container_t container,
					hurd_pm_container_access_t *access);

/* Allocate SIZE bytes of physical memory (which must be a multiple of
   the page size) according to the flags FLAGS into container
   CONTAINER and associate the first byte with index START (and byte X
   with START+X where X < SIZE).  On return *AMOUNT will contain the
   number of bytes successfully allocated.  The returned error code
   will indicate the error trying to allocate byte *AMOUNT.

   physmem will use the least number of frames possible to cover the
   region (recall: a frame is any power of 2 greater than the minimum
   page size) without allocating too much memory.  That is, if you
   allocate 12k and the page size is 4k, physmem will allocate two
   frames, an 8k frame and a 4k frame.  If for some reason you need
   only 4k frames, then you must call this function multiple
   times.  */
extern error_t hurd_pm_container_allocate (hurd_pm_container_t container,
					   l4_word_t start, l4_word_t size,
					   l4_word_t flags, l4_word_t *amount);

/* Deallocate SIZE bytes of physical memory starting at offset START
   in container CONTAINER.  This implicitly unmaps any extant mappings
   of the physical memory.  If is not START page aligned, EINVAL is
   returned.  If SIZE is not a multiple of the page size, EINVAL is
   returned.

   Multiple allocations may be deallocated in a single RPC.  Parts of
   an allocation may also be deallocated.  Any part of the region
   without a valid mapping will be ignored (and no error
   returned).  */
extern error_t hurd_pm_container_deallocate (hurd_pm_container_t container,
					     uintptr_t start, uintptr_t size);
					     
/* Map the frames in container CONTAINER starting at offset OFFSET
   with size SIZE at virtual memory address VADDR of the calling task
   with access rights RIGHTS.  */
extern error_t hurd_pm_container_map (hurd_pm_container_t container,
				      l4_word_t offset, size_t size,
				      uintptr_t vaddr, l4_word_t rights);

/* Logically copy COUNT bytes from container SRC_CONTAINER starting at
   byte SRC_START to container DEST_CONTAINER starting at byte
   DEST_START.  On return, *AMOUNT contains the number of bytes copied
   when an error occurred (if any).

   If copying would overwrite frames in DEST_CONTAINER and FLAGS
   contains HURD_PM_CONT_ALLOC_SQUASH, the frames are implicitly
   deallocated, otherwise EEXIST is returned.

   If an address to copy has no memory associated with it, ESRCH is
   returned.

   SRC_START and DEST_START must correspond to the start of a base
   page.  COUNT must be a multiple of the base page.  Failing this,
   EINVAL is returned.

   *AMOUNT will always contain a number of bytes which is a multiple
   of the base page size.  */
extern error_t hurd_pm_container_copy (hurd_pm_container_t src_container,
				       uintptr_t src_start,
				       hurd_pm_container_t dest_container,
				       uintptr_t dest_start,
				       size_t count,
				       uintptr_t flags,
				       size_t *amount);
#endif	/* HURD_PHYSMEM_USER_H */
