/* physmem-user.c - physmem client stubs.
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdint.h>
#include <l4.h>

#include <compiler.h>
#include <hurd/startup.h>

#include "physmem-user.h"

/* Initialized by the machine-specific startup-code.  */
extern struct hurd_startup_data *__hurd_startup_data;

#define physmem (__hurd_startup_data->image.server)

enum container_ops
  {
    container_create_id = 130,
    container_share_id,
    container_allocate_id,
    container_deallocate_id,
    container_map_id,
    container_copy_id
  };

/* Create a container managed by the physical memory server.  On
   success, returned in *CONTAINER.  */
error_t
hurd_pm_container_create (hurd_pm_container_t control,
			  hurd_pm_container_t *container)

{
  l4_msg_t msg;
  l4_msg_tag_t tag;

  l4_msg_clear (msg);
  l4_set_msg_label (msg, container_create_id);
  l4_msg_append_word (msg, control);
  l4_msg_load (msg);

  tag = l4_call (physmem);
  l4_msg_store (tag, msg);

  *container = l4_msg_word (msg, 0);

  return l4_msg_label (msg);
}

/* Create a limited access capability for container CONTAINER, return
   in *ACCESS.  */
error_t
hurd_pm_container_share (hurd_pm_control_t container,
			 hurd_pm_container_access_t *access)
{
  l4_msg_t msg;
  l4_msg_tag_t tag;

  l4_msg_clear (msg);
  l4_set_msg_label (msg, container_share_id);
  l4_msg_append_word (msg, container);
  l4_msg_load (msg);

  tag = l4_call (physmem);
  l4_msg_store (tag, msg);

  *access = l4_msg_word (msg, 0);

  return l4_msg_label (msg);
}

/* Allocate SIZE bytes according to FLAGS into container CONTAINER
   starting with index START.  *AMOUNT contains the number of bytes
   successfully allocated.  */
error_t
hurd_pm_container_allocate (hurd_pm_container_t container,
			    l4_word_t start, l4_word_t size,
			    l4_word_t flags, l4_word_t *amount)
{
  l4_msg_t msg;
  l4_msg_tag_t tag;

  l4_msg_clear (msg);
  l4_set_msg_label (msg, container_allocate_id);
  l4_msg_append_word (msg, container);
  l4_msg_append_word (msg, flags);
  l4_msg_append_word (msg, start);
  l4_msg_append_word (msg, size);

  l4_msg_load (msg);

  tag = l4_call (physmem);
  l4_msg_store (tag, msg);

  *amount = l4_msg_word (msg, 0);

  return l4_msg_label (msg);
}

error_t
hurd_pm_container_deallocate (hurd_pm_container_t container,
			      uintptr_t start, uintptr_t size)
{
  l4_msg_t msg;
  l4_msg_tag_t tag;

  l4_msg_clear (msg);
  l4_set_msg_label (msg, container_deallocate_id);
  l4_msg_append_word (msg, container);
  l4_msg_append_word (msg, start);
  l4_msg_append_word (msg, size);

  l4_msg_load (msg);

  tag = l4_call (physmem);
  l4_msg_store (tag, msg);

  return l4_msg_label (msg);
}
					     

/* Map the memory at offset OFFSET with size SIZE at address VADDR
   from the container CONT in the physical memory server PHYSMEM.  */
error_t
hurd_pm_container_map (hurd_pm_container_t container,
		       l4_word_t offset, size_t size,
		       uintptr_t vaddr, l4_word_t rights)
{
  l4_msg_t msg;
  l4_msg_tag_t tag;

  /* Let physmem take over the address space completely.  */
  l4_accept (l4_map_grant_items (L4_COMPLETE_ADDRESS_SPACE));

  l4_msg_clear (msg);
  l4_set_msg_label (msg, container_map_id);
  l4_msg_append_word (msg, container);
  l4_msg_append_word (msg, offset | rights);
  l4_msg_append_word (msg, size);
  l4_msg_append_word (msg, vaddr);
  l4_msg_load (msg);

  tag = l4_call (physmem);
  l4_msg_store (tag, msg);

  for (int i = 0;
       i < l4_typed_words (tag);
       i += sizeof (l4_map_item_t) / sizeof (l4_word_t))
    {
      l4_map_item_t mi;
      l4_msg_get_map_item (msg, i, &mi);
      assert (l4_is_map_item (mi));
      printf ("fpage(%x,%x)@%x ", l4_address (l4_map_item_snd_fpage (mi)),
	      l4_size (l4_map_item_snd_fpage (mi)),
	      l4_map_item_snd_base (mi));
    }
  printf ("\n");

  return l4_msg_label (msg);
}

error_t
hurd_pm_container_copy (hurd_pm_container_t src_container,
			uintptr_t src_start,
			hurd_pm_container_t dest_container,
			uintptr_t dest_start,
			size_t count,
			uintptr_t flags,
			size_t *amount)
{
  l4_msg_t msg;
  l4_msg_tag_t tag;

  l4_msg_clear (msg);
  l4_set_msg_label (msg, container_copy_id);
  l4_msg_append_word (msg, src_container);
  l4_msg_append_word (msg, src_start);
  l4_msg_append_word (msg, dest_container);
  l4_msg_append_word (msg, dest_start);
  l4_msg_append_word (msg, count);
  l4_msg_append_word (msg, flags);

  l4_msg_load (msg);

  tag = l4_call (physmem);
  l4_msg_store (tag, msg);

  *amount = l4_msg_word (msg, 0);

  return l4_msg_label (msg);
}
