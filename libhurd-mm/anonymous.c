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
  
  /* Offset from start of pager.  */
  uintptr_t offset;
  /* The allocated storage.  */
  addr_t storage;
};

static int
offset_compare (const uintptr_t *a, const uintptr_t *b)
{
  bool have_range = (*a & 1) || (*b & 1);
  if (unlikely (have_range))
    /* If the least significant bit is set, then we the following word
       is the length.  In this case, we are interested in overlap.  */
    {
      uintptr_t a_start = *a & ~1;
      uintptr_t a_end = a_start + ((*a & 1) ? a[1] : 0);
      uintptr_t b_start = *b & ~1;
      uintptr_t b_end = b_start + ((*b & 1) ? b[1] : 0);

      if (a_end < b_start)
	return -1;
      if (a_start > b_end)
	return 1;
      /* Overlap.  */
      return 0;
    }

  if (*a < *b)
    return -1;
  return *a != *b;
}

BTREE_CLASS (storage_desc, struct storage_desc,
	     uintptr_t, offset, node, offset_compare, false)

static error_t
slab_alloc (void *hook, size_t size, void **ptr)
{
  assert (size == PAGESIZE);

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

/* Storage descriptors are alloced from a slab.  */
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

  assert (addr_prefix (anon->pager->region.start) <= addr);
  // assert (addr + count * PAGESIZE < addr_prefix (anon->pager->region.start) <= addr);

  addr_t page = addr_chop (PTR_TO_ADDR (addr), PAGESIZE_LOG2);

  struct storage_desc *storage_desc = NULL;
  storage_desc = hurd_btree_pager_find (&anon->storage, &page);

  while (count > 0)
    {
    }
}
#endif

static bool
fault (struct pager *pager, uintptr_t offset, int count, bool read_only,
       uintptr_t fault_addr, uintptr_t ip, struct exception_info info)
{
  debug (5, "Fault at %x", fault_addr);

  struct anonymous_pager *anon = (struct anonymous_pager *) pager;

  ss_mutex_lock (&anon->lock);

  bool recursive = false;
  if (anon->fill)
    {
      if (! ss_mutex_trylock (&anon->fill_lock))
	{
	  if (anon->fill_thread == l4_myself ())
	    recursive = true;

	  if (! recursive)
	    /* Wait for the fill lock.  */
	    ss_mutex_lock (&anon->fill_lock);
	}

      if (! recursive)
	assert (anon->fill_thread == l4_nilthread);
      anon->fill_thread = l4_myself ();

      if (! recursive && (anon->flags & ANONYMOUS_THREAD_SAFE))
	/* Revoke access to the visible region.  */
	{
	  assert (anon->map_area_count == 1);
	  as_ensure_use
	    (anon->map_area,
	     ({
	       /* XXX: Do it.  Where to get a void capability?  */
	     }));
	}
    }

  assert (offset < pager->length);

  offset &= ~(PAGESIZE - 1);
  fault_addr &= ~(PAGESIZE - 1);

  struct storage_desc *storage_desc = NULL;
  if (! (anon->flags & ANONYMOUS_NO_ALLOC))
    {
      hurd_btree_storage_desc_t *storage_descs;
      storage_descs = (hurd_btree_storage_desc_t *) &anon->storage;

      storage_desc = hurd_btree_storage_desc_find (storage_descs, &offset);

      if (info.discarded)
	{
	  /* We can only fault on a page that we already have a descriptor
	     for if the page was discardable.  Thus, if the pager is not
	     an anonymous pager, we know that there is no descriptor for
	     the page.  */
	  assert (storage_desc);
	  assert (anon->policy.discardable);
	  assert (anon->fill);

	  error_t err;
	  err = rm_object_discarded_clear (ADDR_VOID, storage_desc->storage);
	  assertx (err == 0, "%d", err);

	  debug (5, "Clearing discarded bit for %p / " ADDR_FMT,
		 fault_addr, ADDR_PRINTF (storage_desc->storage));
	}
      else
	{
	  if (! storage_desc)
	    /* Seems we have not yet allocated a page.  */
	    {
	      storage_desc = storage_desc_alloc ();
	      storage_desc->offset = offset;

	      struct storage storage
		= storage_alloc (anon->activity,
				 cap_page, STORAGE_UNKNOWN, anon->policy,
				 ADDR_VOID);
	      if (ADDR_IS_VOID (storage.addr))
		panic ("Out of memory.");
	      storage_desc->storage = storage.addr;

	      struct storage_desc *conflict;
	      conflict = hurd_btree_storage_desc_insert (storage_descs,
							 storage_desc);
	      assertx (! conflict,
		       "Fault address: %x, offset: %x",
		       fault_addr, offset);

	      debug (5, "Allocating storage for %p at " ADDR_FMT,
		     fault_addr, ADDR_PRINTF (storage_desc->storage));
	    }
	  else
	    debug (5, "Copying storage " ADDR_FMT " to %p",
		   ADDR_PRINTF (storage_desc->storage), fault_addr);

	  /* We generate a fake shadow cap for the storage as we know
	     its contents (It is a page that is in a folio with the
	     policy ANON->POLICY.)  */
	  struct cap page;
	  memset (&page, 0, sizeof (page));
	  page.type = cap_page;
	  CAP_POLICY_SET (&page, anon->policy);

	  addr_t addr = addr_chop (PTR_TO_ADDR (fault_addr), PAGESIZE_LOG2);
	  as_ensure_use
	    (addr,
	     ({
	       bool ret;
	       ret = cap_copy_x (meta_data_activity,
				 ADDR_VOID, slot, addr,
				 ADDR_VOID, page, storage_desc->storage,
				 read_only ? CAP_COPY_WEAKEN : 0,
				 CAP_PROPERTIES_VOID);
	       assert (ret);
	     }));
	}
    }

  bool r = true;

  if (anon->fill)
    {
      if (! recursive || ! (anon->flags & ANONYMOUS_NO_RECURSIVE))
	{
	  debug (5, "Fault at %x, storage: " ADDR_FMT,
		 fault_addr,
		 ADDR_PRINTF (storage_desc
			      ? storage_desc->storage : ADDR_VOID));
	  void *pages[1]
	    = { storage_desc
		? ADDR_TO_PTR (addr_extend (storage_desc->storage,
					    0, PAGESIZE_LOG2))
		: (void *) fault_addr };
	  r = anon->fill (anon, offset, 1, pages, info);
	}

      if (! recursive)
	{
	  if ((anon->flags & ANONYMOUS_THREAD_SAFE))
	    /* Restore access to the visible region.  */
	    {
	      as_ensure_use
		(anon->map_area,
		 ({
		   /* XXX: Do it.  */
		 }));
	    }

	  anon->fill_thread = l4_nilthread;
	  ss_mutex_unlock (&anon->fill_lock);
	}
    }

  ss_mutex_unlock (&anon->lock);

  return r;
}

static void
mdestroy (struct map *map)
{
  struct anonymous_pager *anon = (struct anonymous_pager *) map->pager;

#ifndef NDEBUG
  /* Void the area.  */
  addr_t addr;
  for (addr = ADDR (map->region.start, ADDR_BITS - PAGESIZE_LOG2);
       addr_prefix (addr) < map->region.start + map->region.length;
       addr = addr_add (addr, 1))
    {
      /* This may fail if the page has not yet been faulted in.  */
      as_slot_lookup_use
	(addr,
	 ({
	   error_t err;
	   err = rm_cap_rubout (meta_data_activity, ADDR_VOID, addr);
	   assert (! err);
	   slot->type = cap_void;
	 }));
    }
#endif

  /* XXX: We assume that every byte is mapped by at most one mapping.
     We may have to reexamine this assumption if we allow multiple
     mappings onto the same part of a pager (e.g., via mremap).  */

  /* Free the storage in this region.  */

  uintptr_t offset[2];
  offset[0] = map->offset | 1;
  offset[1] = map->region.length;

  hurd_btree_storage_desc_t *storage_descs;
  storage_descs = (hurd_btree_storage_desc_t *) &anon->storage;

  struct storage_desc *next
    = hurd_btree_storage_desc_find (storage_descs, &offset[0]);
  if (next)
    {
      /* We destory STORAGE_DESC.  Grab its pervious pointer
	 first.  */
      struct storage_desc *prev = hurd_btree_storage_desc_prev (next);

      int dir;
      struct storage_desc *storage_desc;
      for (dir = 0; dir < 2; dir ++, next = prev)
	while ((storage_desc = next))
	  {
	    next = (dir == 0 ? hurd_btree_storage_desc_next (storage_desc)
		    : hurd_btree_storage_desc_prev (storage_desc));

	    if (storage_desc->offset < map->region.start
		|| (storage_desc->offset
		    > map->region.start + map->region.length - 1))
	      break;

	    storage_free (storage_desc->storage, false);

#ifndef NDEBUG
	    /* When reallocating, we expect that the node field is 0.
	       libhurd-btree asserts this, so make it so.  */
	    memset (storage_desc, 0, sizeof (struct storage_desc));
#endif
	    storage_desc_free (storage_desc);
	  }
    }

  /* Free the map area.  Should we also free the staging area?  */
  as_free (PTR_TO_ADDR (map->region.start), map->region.length);
}

static void
destroy (struct pager *pager)
{
  assert (! ss_mutex_trylock (&pager->lock));
  assert (! pager->maps);

  struct anonymous_pager *anon = (struct anonymous_pager *) pager;

  if (anon->staging_area)
    /* Free the staging area.  */
    {
      assert ((anon->flags & ANONYMOUS_STAGING_AREA));
      as_free (addr_chop (PTR_TO_ADDR (anon->staging_area), PAGESIZE_LOG2),
	       anon->pager.length / PAGESIZE);
    }
  else
    assert (! (anon->flags & ANONYMOUS_STAGING_AREA));

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
		       void *hint, uintptr_t length, enum map_access access,
		       struct object_policy policy,
		       uintptr_t flags, anonymous_pager_fill_t fill,
		       void **addr_out)
{
  assert (addr_out);
  debug (5, "(%p, %d pages, %x)", hint, length / PAGESIZE, flags);
  assert (((uintptr_t) hint & (PAGESIZE - 1)) == 0);
  assert ((length & (PAGESIZE - 1)) == 0);
  assert (length > 0);

  if ((flags & ANONYMOUS_NO_ALLOC))
    assert (fill);

  void *buffer;
  error_t err = hurd_slab_alloc (&anonymous_pager_slab, &buffer);
  if (err)
    panic ("Out of memory!");

  struct anonymous_pager *anon = buffer;
  memset (anon, 0, sizeof (*anon));

  anon->pager.length = length;
  anon->pager.fault = fault;
  anon->pager.no_refs = destroy;

  anon->activity = activity;
  anon->flags = flags;

  anon->fill = fill;
  anon->policy = policy;

  if (! pager_init (&anon->pager))
    goto error_with_buffer;

  int width = PAGESIZE_LOG2;
  int count = length >> width;

  if ((flags & ANONYMOUS_THREAD_SAFE))
    /* We want to be able to disable access to the data via the mapped
       window.  We round size up to the smallest power of two greater
       than or equal to LENGTH.  In this way, when a fault comes in,
       disabling access is relatively easy: we just replace the single
       capability that dominates the mapped with a void capability and
       restore it on the way out.  */
    {
      /* This flag makes no sense if a fill function is not also
	 specified.  */
      assert (fill);

      count = 1;
      /* e.g., l4_msb (4k * 2 - 1) - 1 = 12.  */
      width = l4_msb (length * 2 - 1) - 1;

      if (hint)
	/* We will allocate a region whose size is 1 << WIDTH.  This
	   may not cover all of the requested region if the starting
	   address is not aligned on a 1 << WIDTH boundary.  Consider
	   a requested address of 12k and a size of 8k.  In this case,
	   WIDTH is 13 and addr_chop (hint, WIDTH) => 8k thus yielding
	   the region 8-16k, yet, the requested region is 12k-20k!  In
	   such cases, we just need to double the width to cover the
	   whole region.  */
	{
	  if (((uintptr_t) hint & ((1 << width) - 1)) + (1 << width)
	      < (uintptr_t) hint + length)
	    width ++;
	}
    }

  /* Allocate the map area.  */

  bool alloced = false;
  if (hint)
    /* Caller wants a specific address range.  */
    {
      /* NB: this may round HINT down if we need a power-of-2 staging
	 area!  */
      anon->map_area = addr_chop (PTR_TO_ADDR (hint), width);

      bool r = as_alloc_at (anon->map_area, count);
      if (! r)
	/* No room for this region.  */
	{
	  if ((flags & ANONYMOUS_FIXED))
	    {
	      debug (0, "(%x, %x (%x)): Specified range " ADDR_FMT "+%d "
		     "in use and ANONYMOUS_FIXED specified",
		     hint, length, hint + length - 1,
		     ADDR_PRINTF (anon->map_area), count);
	      goto error_with_buffer;
	    }
	}
      else
	{
	  alloced = true;
	  *addr_out = hint;
	}
    }
  if (! alloced)
    {
      anon->map_area = as_alloc (width, count, true);
      if (ADDR_IS_VOID (anon->map_area))
	{
	  debug (0, "(%x, %x (%x)): No VA available",
		 hint, length, hint + length - 1);
	  goto error_with_buffer;
	}

      *addr_out = ADDR_TO_PTR (addr_extend (anon->map_area, 0, width));
    }

  anon->map_area_count = count;


  if ((flags & ANONYMOUS_STAGING_AREA))
    /* We need a staging area.  */
    {
      addr_t staging_area = as_alloc (PAGESIZE_LOG2, length / PAGESIZE, true);
      if (ADDR_IS_VOID (staging_area))
	goto error_with_map_area;

      anon->staging_area = ADDR_TO_PTR (addr_extend (staging_area,
						     0, PAGESIZE_LOG2));
    }


  /* Install the map.  */
  struct region region = { (uintptr_t) *addr_out, length };
  struct map *map = map_create (region, access, &anon->pager, 0, mdestroy);
  /* There is no way that we get a region conflict.  */
  if (! map)
    panic ("Memory exhausted.");


  debug (5, "Installed pager at %x spanning %d pages",
	 *addr_out, length / PAGESIZE);

  return anon;

 error_with_map_area:
  as_free (anon->map_area, anon->map_area_count);
 error_with_buffer:
  hurd_slab_dealloc (&anonymous_pager_slab, anon);

  return NULL;
}

void
anonymous_pager_destroy (struct anonymous_pager *anon)
{
  ss_mutex_lock (&anon->lock);

  pager_deinit (&anon->pager);
}
