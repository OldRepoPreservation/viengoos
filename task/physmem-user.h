/* physmem-user.h - User stub interfaces for physmem RPCs.
   Copyright (C) 2004 Free Software Foundation, Inc.
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

#ifndef HURD_PHYSMEM_USER_H
#define HURD_PHYSMEM_USER_H	1

#include <l4.h>

#include <sys/types.h>

#include <hurd/types.h>


/* Map the memory at offset OFFSET with size SIZE at address VADDR
   from the container CONT in the physical memory server PHYSMEM.  */
error_t physmem_map (l4_thread_id_t physmem, hurd_cap_handle_t cont,
		     l4_word_t offset, size_t size, void *vaddr);

#endif	/* HURD_PHYSMEM_USER_H */
