/* anonymous.h - Anonymous memory pager interface.
   Copyright (C) 2007, 2008 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

   This file is part of the GNU Hurd.

   GNU Hurd is free software: you can redistribute it and/or modify it
   under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   GNU Hurd is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with GNU Hurd.  If not, see
   <http://www.gnu.org/licenses/>.  */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <hurd/stddef.h>
#include <hurd/btree.h>
#include <hurd/slab.h>

#include <hurd/rm.h>

#include "anonymous.h"
#include "pager.h"
#include "storage.h"
#include "as.h"

/* All fields are protected by the pager's lock.  */
struct storage_desc
{
  hurd_btree_node_t node;
  
  /* Offset from start of mapping.  */
  uintptr_t offset;
  /* The allocated storage.  */
  addr_t storage;
};

static int
offset_compare (const uintptr_t *a, const uintptr_t *b)
{
  if (a < b)
    return -1;
  return a != b;
}

BTREE_CLASS (storage_desc, struct storage_desc,
	     uintptr_t, offset, node, offset_compare, false)

static error_t
slab_alloc (void *hook, size_t size, void **ptr)
{
  struct storage storage = storage_alloc (meta_data_activity, cap_page,
					  STORAGE_LONG_LIVED,
					  OBJECT_POLICY_DEFAULT, ADDR_VOID);
  if (ADDR_IS_VOID (storage.addr))
    panic ("Out of space.");
  *ptr = ADDR_TO_PTR (addr_extend (storage.addr, 0, PAGESIZE_LOG2));

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
  assert (! ss_mutex_trylock (&pager->lock));

  debug (5, "Fault at " ADDR_FMT, ADDR_PRINTF (addr));

  struct anonymous_pager *anon = (struct anonymous_pager *) pager;

  bool recursive = false;
  if (anon->fill)
    {
      if (! ss_mutex_trylock (&anon->fill_lock))
	{
	  if (anon->fill_thread == l4_myself ())
	    recursive = true;

	  ss_mutex_unlock (&anon->pager.lock);

	  if (! recursive)
	    /* Wait for the fill lock.  */
	    ss_mutex_lock (&anon->fill_lock);
	}
      else
	ss_mutex_unlock (&anon->pager.lock);

      if (! recursive)
	assert (anon->fill_thread == l4_nilthread);
      anon->fill_thread = l4_myself ();

      if (! recursive && (anon->flags & ANONYMOUS_THREAD_SAFE))
	/* Revoke access to the visible region.  */
	{
	  /* XXX: Do it.  */
	}
    }

  uintptr_t page;
  if (addr_depth (addr) == ADDR_BITS)
    page = (uintptr_t) ADDR_TO_PTR (addr) & ~(PAGESIZE - 1);
  else
    {
      assert (addr_depth (addr) == ADDR_BITS - PAGESIZE_LOG2);
      page = (uintptr_t) addr_prefix (addr);
    }

  assert (page >= addr_prefix (pager->region.start));
  uintptr_t offset = page - addr_prefix (pager->region.start);
  assert (offset < PAGER_REGION_LENGTH (pager->region));

  if (! (anon->flags & ANONYMOUS_NO_ALLOC))
    {
      hurd_btree_storage_desc_t *storage_descs;
      storage_descs = (hurd_btree_storage_desc_t *) &anon->storage;

      struct storage_desc *storage_desc = NULL;
      if (info.discarded)
	{
	  /* We can only fault on a page that we already have a descriptor
	     for if the page was discardable.  Thus, if the pager is not
	     an anonymous pager, we know that there is no descriptor for
	     the page.  */
	  assert (anon->policy.discardable);
	  assert (anon->fill);

	  storage_desc = hurd_btree_storage_desc_find (storage_descs, &page);
	}

      if (! storage_desc)
	{
	  storage_desc = storage_desc_alloc ();
	  storage_desc->offset = offset;

	  addr_t addr = addr_chop (PTR_TO_ADDR (page), PAGESIZE_LOG2);
	  as_ensure (addr);

	  struct storage storage
	    = storage_alloc (anon->activity,
			     cap_page, STORAGE_UNKNOWN, anon->policy, addr);
	  if (ADDR_IS_VOID (storage.addr))
	    panic ("Out of memory.");
	  storage_desc->storage = storage.addr;

	  struct storage_desc *conflict;
	  conflict = hurd_btree_storage_desc_insert (storage_descs,
						     storage_desc);
	  assert (! conflict);
	}
      else if (info.discarded)
	rm_object_discarded_clear (ADDR_VOID, storage_desc->storage);
      else
	panic ("Fault invoked for no reason?!");
    }

  bool r = true;

  if (anon->fill)
    {
      if (! recursive || ! (anon->flags & ANONYMOUS_NO_RECURSIVE))
	{
	  void *base = anon->staging_area
	    ?: (void *) (uintptr_t) addr_prefix (pager->region.start);

	  r = anon->fill (anon, base, offset, 1, info);
	}

      if (! recursive)
	{
	  if ((anon->flags & ANONYMOUS_THREAD_SAFE))
	    /* Restore access to the visible region.  */
	    {
	      /* XXX: Do it.  */
	    }

	  anon->fill_thread = l4_nilthread;
	  ss_mutex_unlock (&anon->fill_lock);
	}
    }
  else
    ss_mutex_unlock (&anon->pager.lock);

  return r;
}

static void
destroy (struct pager *pager)
{
  struct anonymous_pager *anon = (struct anonymous_pager *) pager;

  as_free (anon->alloced_region.start, anon->alloced_region.count);

  if ((anon->flags & ANONYMOUS_THREAD_SAFE))
    as_free (PTR_TO_ADDR (anon->staging_area),
	     PAGER_REGION_LENGTH (anon->pager.region));

  /* Free the allocated storage.  */
  hurd_btree_storage_desc_t *storage_descs;
  storage_descs = (hurd_btree_storage_desc_t *) &anon->storage;

  struct storage_desc *node, *next;
  for (node = hurd_btree_storage_desc_first (storage_descs); node; node = next)
    {
      next = hurd_btree_storage_desc_next (node);

      storage_free (node->storage, false);

#ifndef NDEBUG
      /* When reallocating, we expect that the node field is 0.
	 libhurd-btree asserts this, so make it so.  */
      memset (node, 0, sizeof (struct storage_desc));
#endif
      storage_desc_free (node);
    }

  /* There is no need to unlock &anon->pager.lock: we free it.  */

  /* And free its storage.  */
  hurd_slab_dealloc (&anonymous_pager_slab, anon);
}

struct anonymous_pager *
anonymous_pager_alloc (addr_t activity,
		       void *hint, size_t size,
		       struct object_policy policy,
		       uintptr_t flags, anonymous_pager_fill_t fill,
		       void **addr_out)
{
  assert (addr_out);
  debug (5, "(%p, %d pages, %x)", hint, size / PAGESIZE, flags);
  assert (((uintptr_t) hint & (PAGESIZE - 1)) == 0);
  assert ((size & (PAGESIZE - 1)) == 0);
  assert (size > 0);

  if ((flags & ANONYMOUS_NO_ALLOC))
    assert (fill);

  void *buffer;
  error_t err = hurd_slab_alloc (&anonymous_pager_slab, &buffer);
  if (err)
    panic ("Out of memory!");

  struct anonymous_pager *anon = buffer;
  memset (anon, 0, sizeof (*anon));


  addr_t addr;
  int width = PAGESIZE_LOG2;
  int count = size >> width;

  if (hint)
    addr = addr_chop (PTR_TO_ADDR (hint), width);
  else
    addr = ADDR_VOID;

  if ((flags & ANONYMOUS_THREAD_SAFE))
    /* Round size up to the smallest power of two greater than or
       equal to SIZE.  In this way, when a fault comes in, protecting
       the visible region is relatively easy: we replace the single
       capability that dominates the visible region with a void
       capability and restore it on the way out.  */
    {
      /* This flag makes no sense if a fill function is not also
	 specified.  */
      assert (fill);

      count = 1;
      /* l4_msb (4k * 2 - 1) - 1 = 12.  */
      width = l4_msb (size * 2 - 1) - 1;

      if (hint)
	/* We will allocate a region whose size 1 << WIDTH.  This may
	   not cover all of the requested region if the starting
	   address is not aligned on a 1 << WIDTH boundary.  Consider
	   a requested address of 12k and a size of 8k.  In this case,
	   WIDTH is 13 and addr_chop (hint, WIDTH) => 8k thus
	   yielding the region 8-16k, yet, the requested region is
	   12k-20k!  In such cases, we just need to double the width
	   to cover the whole region.  */
	{
	  int extra = 0;
	  if (((uintptr_t) hint & ((1 << width) - 1)) + (1 << width)
	      < (uintptr_t) hint + size)
	    extra = 1;

	  addr = addr_chop (PTR_TO_ADDR (hint), width + extra);
	}
    }

  bool alloced = false;
  if (! ADDR_IS_VOID (addr))
    /* Caller wants a specific address range.  */
    {
      bool r = as_alloc_at (addr, count);
      if (! r)
	/* No room for this region.  */
	{
	  if ((flags & ANONYMOUS_FIXED))
	    goto error_with_buffer;
	}

      alloced = true;

      /* The region that we are actually paging.  */
      anon->pager.region.start = PTR_TO_ADDR (hint);
      anon->pager.region.count = size;

      /* The region that we allocated.  */
      anon->alloced_region.start = addr;
      anon->alloced_region.count = count;

      *addr_out = hint;
    }

  if (! alloced)
    {
      addr_t region = as_alloc (width, count, true);
      if (ADDR_IS_VOID (region))
	goto error_with_buffer;

      anon->pager.region.start = anon->alloced_region.start = region;
      anon->pager.region.count = anon->alloced_region.count = count;

      *addr_out = ADDR_TO_PTR (addr_extend (region, 0, width));
    }

  assertx (PAGER_REGION_LENGTH (anon->pager.region) == size,
	   "%llx != %x",
	   PAGER_REGION_LENGTH (anon->pager.region), size);

  if ((flags & ANONYMOUS_THREAD_SAFE))
    /* We need a staging area.  */
    {
      addr_t staging = as_alloc (width, count, true);
      if (ADDR_IS_VOID (staging))
	goto error_with_area;

      anon->staging_area = ADDR_TO_PTR (addr_extend (staging, 0, width));
    }
  else
    anon->staging_area
      = ADDR_TO_PTR (addr_extend (anon->pager.region.start, 0,
				  ADDR_BITS
				  - addr_depth (anon->pager.region.start)));

  anon->pager.fault = fault;
  anon->pager.destroy = destroy;

  anon->activity = activity;
  anon->flags = flags;

  anon->fill = fill;
  anon->policy = policy;

  /* Install the pager.  */
  ss_mutex_lock (&pagers_lock);
  bool r = pager_install (&anon->pager);
  ss_mutex_unlock (&pagers_lock);
  if (! r)
    /* Ooops!  There is a region conflict.  */
    goto error_with_staging;

  debug (5, "Installed pager at " ADDR_FMT " spanning %d pages",
	 ADDR_PRINTF (anon->pager.region.start),
	 (int) PAGER_REGION_LENGTH (anon->pager.region) / PAGESIZE);

  return anon;

 error_with_staging:
  if ((flags & ANONYMOUS_THREAD_SAFE))
    as_free (PTR_TO_ADDR (anon->staging_area),
	     PAGER_REGION_LENGTH (anon->pager.region));
 error_with_area:
  as_free (anon->alloced_region.start, anon->alloced_region.count);
 error_with_buffer:
  hurd_slab_dealloc (&anonymous_pager_slab, anon);

  return NULL;
}

void
anonymous_pager_destroy (struct anonymous_pager *anon)
{
  ss_mutex_lock (&pagers_lock);
  pager_deinstall (&anon->pager);
  ss_mutex_unlock (&pagers_lock);

  ss_mutex_lock (&anon->pager.lock);
  destroy (&anon->pager);
}
