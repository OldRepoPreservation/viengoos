/* physmem-user.c - User stubs for physmem RPCs.
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <l4.h>

#include "physmem-user.h"


/* Map the memory at offset OFFSET with size SIZE at address VADDR
   from the container CONT in the physical memory server PHYSMEM.  */
error_t
physmem_map (l4_thread_id_t physmem, hurd_cap_handle_t cont,
	     l4_word_t offset, size_t size, void *vaddr)
{
  l4_msg_t msg;
  l4_msg_tag_t tag;

  /* Let physmem take over the address space completely.  */
  l4_accept (l4_map_grant_items (L4_COMPLETE_ADDRESS_SPACE));

  l4_msg_clear (msg);
  l4_set_msg_label (msg, 128 /* PHYSMEM_MAP */);
  l4_msg_append_word (msg, cont);
  l4_msg_append_word (msg, offset);
  l4_msg_append_word (msg, size);
  l4_msg_append_word (msg, (l4_word_t) vaddr);
  l4_msg_load (msg);
  tag = l4_call (physmem);

  /* FIXME: Check return.  */
  return 0;
}
