/* Copyright (C) 1991,1995,1996,1997, 2002, 2005 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#include <errno.h>
#include <unistd.h>


/* FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME */
#include <l4.h>

#include <hurd/types.h>
#include <hurd/startup.h>

/* Allocate SIZE bytes according to FLAGS into container CONTAINER
   starting with index START.  *AMOUNT contains the number of bytes
   successfully allocated.  */
static l4_word_t
allocate (l4_thread_id_t server, hurd_cap_handle_t container,
	  l4_fpage_t start, l4_word_t size,
	  l4_word_t flags, l4_word_t *amount)
{
  l4_msg_t msg;
  l4_msg_tag_t tag;

  l4_msg_clear (msg);
  l4_set_msg_label (msg, 132 /* Magic number for container_allocate_id.  */);
  l4_msg_append_word (msg, container);
  l4_msg_append_word (msg, flags);
  l4_msg_append_word (msg, start);
  l4_msg_append_word (msg, size);

  l4_msg_load (msg);

  tag = l4_call (server);
  l4_msg_store (tag, msg);

  *amount = l4_msg_word (msg, 0);

  return l4_msg_label (msg);
}

/* Map the memory at offset OFFSET with size SIZE at address VADDR
   from the container CONT in the physical memory server PHYSMEM.  */
static l4_word_t
map (l4_thread_id_t server, hurd_cap_handle_t container,
     l4_word_t offset, size_t size,
     void *vaddr, l4_word_t rights)
{
  l4_msg_t msg;
  l4_msg_tag_t tag;

  /* Magic!  If the offset is above 0x8000000 then we need to allocate
     anonymous memory.  */
  if (offset >= 0x8000000)
    {
      l4_word_t amount;
      allocate (server, container, offset, size, 0, &amount);
    }

  l4_msg_clear (msg);
  l4_set_msg_label (msg, 134 /* XXX: Magic number for container_map_id.  */);
  l4_msg_append_word (msg, container);
  l4_msg_append_word (msg, offset | rights);
  l4_msg_append_word (msg, size);
  l4_msg_append_word (msg, (l4_word_t) vaddr);
  l4_msg_load (msg);

  tag = l4_call (server);
  l4_msg_store (tag, msg);

  return l4_msg_label (msg);
}


/* sbrk.c expects this.  */
void *__curbrk;

extern int _end;

extern struct hurd_startup_data *_hurd_startup_data;

/* Set the end of the process's data space to ADDR.
   Return 0 if successful, -1 if not.  */
int
__brk (addr)
     void *addr;
{
  if (addr == 0)
    {
      /* Initialization.  */
      __curbrk = &_end;
      return 0;
    }

  /* FIXME: We only support growing the data section for now.  */
  if (addr < __curbrk)
    {
      __set_errno (ENOSYS);
      return -1;
    }
  else
    {
      l4_word_t pagend = l4_page_round ((l4_word_t) __curbrk);
      l4_word_t new_pagend = l4_page_round ((l4_word_t) addr);

      if (new_pagend > pagend)
	map (_hurd_startup_data->mapv[0].cont.server,
	     _hurd_startup_data->mapv[0].cont.cap_handle,
	     pagend, new_pagend - pagend, pagend,
	     L4_FPAGE_FULLY_ACCESSIBLE);
      __curbrk = addr;

      return 0;
    }
}
stub_warning (brk)

weak_alias (__brk, brk)
#include <stub-tag.h>
