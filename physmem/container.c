/* container.c - container class for physical memory server.
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

#include <stdlib.h>

#include <l4.h>
#include <hurd/cap-server.h>

#include "physmem.h"
#include "zalloc.h"



struct container
{
  /* The capability object must be the first member of this
     struct.  */
  struct hurd_cap_obj obj;

  /* For now, a container is nothing more than a contiguous,
     page-aligned range of memory.  This is the reason why
     L4_FPAGE_SPAN_MAX fpages are sufficient.  */
  l4_fpage_t fpages[L4_FPAGE_SPAN_MAX];

  /* The number of entries in FPAGES.  */
  l4_word_t nr_fpages;
};
typedef struct container *container_t;


static void
container_reinit (hurd_cap_class_t cap_class, hurd_cap_obj_t obj)
{
  container_t container = (container_t) obj;
  l4_word_t nr_fpages;

  nr_fpages = container->nr_fpages;

  l4_unmap_fpages (nr_fpages, container->fpages);

  while (nr_fpages > 0)
    {
      l4_fpage_t fpage = container->fpages[--nr_fpages];
      zfree (l4_address (fpage), l4_size (fpage));
    }
}


error_t
container_map (hurd_cap_rpc_context_t ctx)
{
  container_t container = (container_t) ctx->obj;
  l4_word_t offset = l4_page_trunc (l4_msg_word (ctx->msg, 1));
  l4_word_t rights = l4_msg_word (ctx->msg, 1) & 0x7;
  l4_word_t size = l4_page_round (l4_msg_word (ctx->msg, 2));
  l4_word_t vaddr = l4_page_trunc (l4_msg_word (ctx->msg, 3));
  l4_word_t start;
  l4_word_t end;
  l4_word_t nr_fpages;
#define MAX_MAP_ITEMS ((L4_NUM_MRS - 1) / 2)
  l4_fpage_t fpages[MAX_MAP_ITEMS];
  l4_word_t i;

  /* FIXME FIXME FIXME */
  if (offset > 0x8000000)
    {
      /* Allocation.  */
      start = zalloc (size);
      end = start + size - 1;
    }
  else
    {
      /* FIXME: Currently, we just trust that everything is all-right.  */
      start = l4_address (container->fpages[0]) + offset;
      end = start + size - 1;
    }

  l4_msg_clear (ctx->msg);
  nr_fpages = l4_fpage_xspan (start, end, vaddr, fpages, MAX_MAP_ITEMS);

  for (i = 0; i < nr_fpages; i++)
    {
      l4_map_item_t map_item;
      l4_fpage_t fpage;

      fpage = fpages[i];
      l4_set_rights (&fpage, rights);
      map_item = l4_map_item (fpage, vaddr);
      l4_msg_append_map_item (ctx->msg, map_item);
      vaddr += l4_size (fpage);
    }
  return 0;
}


error_t
container_demuxer (hurd_cap_rpc_context_t ctx)
{
  error_t err = 0;

  switch (l4_msg_label (ctx->msg))
    {
      /* PHYSMEM_MAP */
    case 128:
      err = container_map (ctx);
      break;

    default:
      err = EOPNOTSUPP;
    }

  return err;
}



static struct hurd_cap_class container_class;

/* Initialize the container class subsystem.  */
error_t
container_class_init ()
{
  return hurd_cap_class_init (&container_class, sizeof (struct container),
			      __alignof__ (struct container),
			      NULL, NULL, container_reinit, NULL,
			      container_demuxer);
}


/* Allocate a new container object covering the NR_FPAGES fpages
   listed in FPAGES.  The object returned is locked and has one
   reference.  */
error_t
container_alloc (l4_word_t nr_fpages, l4_word_t *fpages,
		 hurd_cap_obj_t *r_obj)
{
  error_t err;
  container_t container;

  err = hurd_cap_class_alloc (&container_class, (hurd_cap_obj_t *) &container);
  if (err)
    return err;

  assert (nr_fpages <= L4_FPAGE_SPAN_MAX);
  container->nr_fpages = nr_fpages;
  memcpy (container->fpages, fpages, sizeof (l4_fpage_t) * nr_fpages);

  *r_obj = &container->obj;
  return 0;
}
