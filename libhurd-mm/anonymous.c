/* anonymous.h - Anonymous memory pager interface.
   Copyright (C) 2007 Free Software Foundation, Inc.
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

#include <hurd/stddef.h>
#include <hurd/btree.h>
#include <hurd/slab.h>

#include "anonymous.h"
#include "pager.h"
#include "storage.h"
#include "as.h"

#define DISCARDABLE(a) ((a)->flags & ANONYMOUS_DISCARDABLE)

struct storage_desc
{
  hurd_btree_node_t node;
  
  /* The virtual address.  */
  addr_t addr;
  /* The allocated storage.  */
  addr_t storage;
};

static int
addr_compare (const addr_t *a, const addr_t *b)
{
  if (a->raw < b->raw)
    return -1;
  return a->raw != b->raw;
}

BTREE_CLASS (storage_desc, struct storage_desc,
	     addr_t, addr, node, addr_compare)

static error_t
slab_alloc (void *hook, size_t size, void **ptr)
{
  addr_t storage = storage_alloc (meta_data_activity,
				  cap_page, STORAGE_LONG_LIVED, ADDR_VOID);

  *ptr = ADDR_TO_PTR (addr_extend (storage, 0, PAGESIZE_LOG2));

  return 0;
}

static error_t
slab_dealloc (void *hook, void *buffer, size_t size)
{
  assert (size == PAGESIZE);

  addr_t addr = addr_chop (PTR_TO_ADDR (buffer), PAGESIZE_LOG2);
  storage_free (addr, false);

  return 0;
}

/* Storage descriptors are alloced from a slab.  */
static struct hurd_slab_space storage_desc_slab
 = HURD_SLAB_SPACE_INITIALIZER (struct storage_desc,
				slab_alloc, slab_dealloc, NULL, NULL, NULL);

static struct storage_desc *
storage_desc_alloc (void)
{
  void *buffer;
  error_t err = hurd_slab_alloc (&storage_desc_slab, &buffer);
  if (err)
    panic ("Out of memory!");

  return buffer;
}

static void
storage_desc_free (struct storage_desc *storage)
{
  hurd_slab_dealloc (&storage_desc_slab, storage);
}

/* Anonymous pagers are allocated from a slab.  */
static struct hurd_slab_space anonymous_pager_slab
 = HURD_SLAB_SPACE_INITIALIZER (struct anonymous_pager,
				slab_alloc, slab_dealloc, NULL, NULL, NULL);

#if 0
void
anonymous_pager_ensure (struct anonymous_pager *anon,
			uintptr_t addr, size_t count)
{
  assert ((addr & (PAGESIZE - 1)) == 0);
  assert ((count & (PAGESIZE - 1)) == 0);

  assert (addr_prefix (anon->pager.region.start) <= addr);
  // assert (addr + count * PAGESIZE < addr_prefix (anon->pager.region.start) <= addr);

  addr_t page = addr_chop (PTR_TO_ADDR (addr), PAGESIZE_LOG2);

  struct storage_desc *storage_desc = NULL;
  storage_desc = hurd_btree_pager_find (&anon->storage, &page);

  while (count > 0)
    {
    }
}
#endif

static bool
fault (struct pager *pager,
       addr_t addr, uintptr_t ip, struct exception_info info)
{
  struct anonymous_pager *anon = (struct anonymous_pager *) pager;

  addr_t page;
  if (addr_depth (addr) == ADDR_BITS)
    page = addr_chop (addr, PAGESIZE_LOG2);
  else
    {
      assert (addr_depth (addr) == ADDR_BITS - PAGESIZE_LOG2);
      page = addr;
    }

  hurd_btree_storage_desc_t *storage_descs;
  storage_descs = (hurd_btree_storage_desc_t *) &anon->storage;

  struct storage_desc *storage_desc = NULL;
  if (DISCARDABLE (anon))
    /* We can only fault on a page that we already have a descriptor
       for if the page was discardable.  Thus, if the pager is not an
       anonymous pager, we know that there is no descriptor for the
       page.  */
    storage_desc = hurd_btree_storage_desc_find (storage_descs, &page);

  if (! storage_desc)
    {
      storage_desc = storage_desc_alloc ();
      storage_desc->addr = page;

      bool r = as_slot_ensure (page);
      if (! r)
	panic ("Failed to ensure slot at " ADDR_FMT, ADDR_PRINTF (page));

      storage_desc->storage = storage_alloc (anon->activity,
					     cap_page, STORAGE_UNKNOWN, page);
      if (ADDR_IS_VOID (storage_desc->storage))
	panic ("Out of memory.");

      struct storage_desc *conflict;
      conflict = hurd_btree_storage_desc_insert (storage_descs, storage_desc);
      assert (! conflict);
    }

  if (anon->fill)
    /* XXX: There is a slight problem here.  */
    anon->fill (anon, anon->cookie,
		(uintptr_t) ADDR_TO_PTR (addr_extend (page, 0, PAGESIZE_LOG2)),
		1);

  return true;
}

struct anonymous_pager *
anonymous_pager_alloc (addr_t activity,
		       uintptr_t addr, size_t size,
		       uintptr_t flags, int map_now)
{
  assert ((addr & (PAGESIZE - 1)) == 0);
  assert ((size & (PAGESIZE - 1)) == 0);

  void *buffer;
  error_t err = hurd_slab_alloc (&anonymous_pager_slab, &buffer);
  if (err)
    panic ("Out of memory!");
  struct anonymous_pager *anon = buffer;

  memset (anon, 0, sizeof (*anon));
  anon->pager.fault = fault;

  anon->activity = activity;
  anon->flags = flags;
  if (map_now)
    /* Allocate the required storage right now.  */
    ;

  /* Install the pager.  */
  anon->pager.region.start = addr_chop (PTR_TO_ADDR (addr), PAGESIZE_LOG2);
  anon->pager.region.count = size >> PAGESIZE_LOG2;

  bool r = pager_install (&anon->pager);
  if (! r)
    /* Ooops!  There is a region conflict.  */
    {
      hurd_slab_dealloc (&anonymous_pager_slab, anon);
      return NULL;
    }

  return anon;
}

void
anonymous_pager_destroy (struct anonymous_pager *anon)
{
  /* Deinstall the pager.  */
  pager_deinstall (&anon->pager);

  /* Free the allocated storage.  */
  hurd_btree_storage_desc_t *storage_descs;
  storage_descs = (hurd_btree_storage_desc_t *) &anon->storage;

  struct storage_desc *node, *next;
  for (node = hurd_btree_storage_desc_first (storage_descs); node; node = next)
    {
      next = hurd_btree_storage_desc_next (node);

      storage_free (node->storage, false);
      storage_desc_free (node);
    }

  /* And free its storage.  */
  hurd_slab_dealloc (&anonymous_pager_slab, anon);
}
