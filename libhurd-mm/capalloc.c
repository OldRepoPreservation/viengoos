/* capalloc.c - Capability allocation functions.
   Copyright (C) 2007 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

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

#include "capalloc.h"

#include "storage.h"
#include "as.h"

#include <hurd/addr.h>
#include <hurd/btree.h>
#include <hurd/slab.h>
#include <hurd/stddef.h>

#include <bit-array.h>

#include <string.h>

struct cappage_desc
{
  hurd_btree_node_t node;

  addr_t cappage;
  unsigned char alloced[CAPPAGE_SLOTS / 8];
  unsigned short free;

  struct cappage_desc *next;
  struct cappage_desc **prevp;
};

static void
link (struct cappage_desc **list, struct cappage_desc *e)
{
  e->next = *list;
  if (e->next)
    e->next->prevp = &e->next;
  e->prevp = list;
  *list = e;
}

static void
unlink (struct cappage_desc *e)
{
  assert (e->next);
  assert (e->prevp);

  *e->prevp = e->next;
  if (e->next)
    e->next->prevp = e->prevp;
}

static int
addr_compare (const addr_t *a, const addr_t *b)
{
  if (addr_prefix (*a) < addr_prefix (*b))
    return -1;
  return addr_prefix (*a) != addr_prefix (*b);
}

BTREE_CLASS (cappage_desc, struct cappage_desc,
	     addr_t, cappage, node, addr_compare)

static hurd_btree_cappage_desc_t cappage_descs;

/* Storage descriptors are alloced from a slab.  */
static error_t
cappage_desc_slab_alloc (void *hook, size_t size, void **ptr)
{
  assert (size == PAGESIZE);

  addr_t storage = storage_alloc (meta_data_activity,
				  cap_page, STORAGE_LONG_LIVED, ADDR_VOID);
  if (ADDR_IS_VOID (storage))
    panic ("Out of storage");
  *ptr = ADDR_TO_PTR (storage);

  return 0;
}

static error_t
cappage_desc_slab_dealloc (void *hook, void *buffer, size_t size)
{
  assert (size == PAGESIZE);

  addr_t addr = ADDR ((uintptr_t) buffer, ADDR_BITS - PAGESIZE_LOG2);
  storage_free (addr, false);

  return 0;
}

static struct hurd_slab_space cappage_desc_slab
  = HURD_SLAB_SPACE_INITIALIZER (struct cappage_desc,
				 cappage_desc_slab_alloc,
				 cappage_desc_slab_dealloc,
				 NULL, NULL, NULL);

static struct cappage_desc *
cappage_desc_alloc (void)
{
  void *buffer;
  error_t err = hurd_slab_alloc (&cappage_desc_slab, &buffer);
  if (err)
    panic ("Out of memory!");
  return buffer;
}

static void
cappage_desc_free (struct cappage_desc *storage)
{
  hurd_slab_dealloc (&cappage_desc_slab, storage);
}

struct cappage_desc *nonempty;

addr_t
capalloc (void)
{
  if (! nonempty)
    {
      nonempty = cappage_desc_alloc ();

      /* As there is such a large number of objects, we expect that
	 the page will be long lived.  */
      nonempty->cappage = storage_alloc (meta_data_activity,
					 cap_cappage, STORAGE_LONG_LIVED,
					 ADDR_VOID);

      memset (&nonempty->alloced, 0, sizeof (nonempty->alloced));
      nonempty->free = CAPPAGE_SLOTS;

      nonempty->next = NULL;
      nonempty->prevp = &nonempty;

      hurd_btree_cappage_desc_insert (&cappage_descs, nonempty);
    }

  int idx = bit_alloc (nonempty->alloced, sizeof (nonempty->alloced), 0);
  assert (idx != -1);

  nonempty->free --;

  if (nonempty->free == 0)
    unlink (nonempty);

  return addr_extend (nonempty->cappage, idx, CAPPAGE_SLOTS_LOG2);
}

void
capfree (addr_t cap)
{
  addr_t cappage = addr_chop (cap, CAPPAGE_SLOTS_LOG2);

  struct cappage_desc *desc;
  desc = hurd_btree_cappage_desc_find (&cappage_descs, &cappage);
  assert (desc);

  bit_dealloc (desc->alloced, addr_extract (cap, CAPPAGE_SLOTS_LOG2));
  desc->free ++;

  if (desc->free == 1)
    /* The cappage is no longer full.  Add it back to the list of
       nonempty cappages.  */
    link (&nonempty, desc);
  else if (desc->free == CAPPAGE_SLOTS)
    /* No slots in the cappage are allocated.  Free it if there is at
       least one cappage on NONEMPTY.  */
    {
      if (nonempty)
	{
	  hurd_btree_cappage_desc_detach (&cappage_descs, desc);
	  cappage_desc_free (desc);
	}
    }
}

