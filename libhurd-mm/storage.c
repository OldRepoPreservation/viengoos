/* storage.c - Storage allocation functions.
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

#include "storage.h"

#include <hurd/stddef.h>
#include <hurd/btree.h>
#include <hurd/slab.h>
#include <hurd/addr.h>
#include <hurd/folio.h>
#include <hurd/startup.h>
#include <hurd/rm.h>

#include <bit-array.h>

#include <stddef.h>
#include <string.h>

#include "as.h"

/* Objects are allocated from folios.  As a folio is the unit of
   storage allocation, we can only free a folio when it is completely
   empty.  For this reason, we try to group long lived objects
   together and short lived objects together.  */

/* Total number of free objects across all folios.  */
static int free_count;

struct storage_desc
{
  /* Each storage area is stored in a btree keyed by the address of
     the folio.  */
  hurd_btree_node_t node;

  /* The address of the folio.  */
  addr_t folio;
  /* The location of the folio's shadow object.  */
  struct object *shadow;

  /* Which objects are allocated.  */
  unsigned char alloced[FOLIO_OBJECTS / 8];
  /* The number of free objects.  */
  unsigned char free;

  /* The storage object's mode of allocation.  */
  char mode;

  struct storage_desc *next;
  struct storage_desc **prevp;
};

static void
link (struct storage_desc **list, struct storage_desc *e)
{
  e->next = *list;
  if (e->next)
    e->next->prevp = &e->next;
  e->prevp = list;
  *list = e;
}

static void
unlink (struct storage_desc *e)
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
  if (a->raw < b->raw)
    return -1;
  return a->raw != b->raw;
}

BTREE_CLASS (storage_desc, struct storage_desc,
	     addr_t, folio, node, addr_compare)

static hurd_btree_storage_desc_t storage_descs;

/* Storage descriptors are alloced from a slab.  */
static struct hurd_slab_space storage_desc_slab;

/* Before we are initialized, allocating data instruction addressable
   storage is at best awkward primarily because we may also need to
   create page tables.  We avoid this by preallocating some slab
   space.  */
static char slab_space[PAGESIZE] __attribute__ ((aligned(PAGESIZE)));
static void *slab_space_reserve = slab_space;

/* After allocating a descriptor, we check to see whether there is
   still a reserve page.  By keeping a page of storage available, we
   avoid the problematic situation of trying to allocate storage and
   insert it into the addresss space but requiring storage and a
   storage descriptor (of which there are none!) to do so.  */
static void
check_slab_space_reserve (void)
{
  if (likely (!!slab_space_reserve))
    return;
  
  addr_t addr = storage_alloc (meta_data_activity, cap_page,
			       STORAGE_LONG_LIVED, ADDR_VOID);
  slab_space_reserve = ADDR_TO_PTR (addr);
}

static error_t
storage_desc_slab_alloc (void *hook, size_t size, void **ptr)
{
  assert (size == PAGESIZE);

  assert (slab_space_reserve);
  *ptr = slab_space_reserve;
  slab_space_reserve = NULL;

  return 0;
}

static error_t
storage_desc_slab_dealloc (void *hook, void *buffer, size_t size)
{
  /* There is no need to special case SLAB_SPACE; we can free it in
     the normal way and the right thing will happen.  */

  assert (size == PAGESIZE);

  addr_t addr = PTR_TO_ADDR (buffer);
  storage_free (addr, false);

  return 0;
}

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

/* Each storage block with at least one unallocated object lives on
   one of three lists.  Where a storage area lives is determined by
   the type of storage: whether a storage block has mostly long-lived
   objects and it is being used as a source for allocations, mostly
   long-lived but we are trying to free it; or, mostly ephemeral
   objects.  */
static struct storage_desc *long_lived_allocing;
static struct storage_desc *long_lived_freeing;
static struct storage_desc *short_lived;

/* The storage area is a source of allocations.  */
#define LONG_LIVED_ALLOCING 1
/* The storage area has been full.  */
#define LONG_LIVED_STABLE 2
/* The storage area is trying to be freed.  */
#define LONG_LIVED_FREEING 3

/* The storage area is dedicated to short lived objects.  */
#define SHORT_LIVED 4

/* Once there are more free objects in a LONG_LIVED_STABLE folio than
   FREEING_THRESHOLD, we change the folios state from stable to
   freeing.  */
#define FREEING_THRESHOLD (FOLIO_OBJECTS / 2)

/* The minimum number of pages that should be available.  */
#define MIN_FREE_PAGES 16

static void
shadow_setup (struct cap *cap, struct storage_desc *storage)
{
  /* XXX: We need to remember that we freed the shadow page from
     ourself so that we can deteck when this storage area is
     actually free.  */
  int idx = bit_alloc (storage->alloced, sizeof (storage->alloced), 0);
  if (idx == -1)
    panic ("Folio full!  No space for a shadow object.");

  storage->free --;
  free_count --;

  error_t err = rm_folio_object_alloc (meta_data_activity,
				       storage->folio, idx, cap_page,
				       ADDR_VOID);
  assert (err == 0);
  storage->shadow = ADDR_TO_PTR (addr_extend (storage->folio, idx,
					      FOLIO_OBJECTS_LOG2));
  cap_set_shadow (cap, storage->shadow);
  cap->type = cap_folio;

  storage->shadow->caps[idx].type = cap_page;
  storage->shadow->caps[idx].addr_trans = CAP_ADDR_TRANS_VOID;
}

void
storage_shadow_setup (struct cap *cap, addr_t folio)
{
  struct storage_desc *sdesc;
  sdesc = hurd_btree_storage_desc_find (&storage_descs, &folio);
  assert (sdesc);

  shadow_setup (cap, sdesc);
}

/* Allocate an object of type TYPE.  If ADDR is not ADDR_VOID, inserts
   a capability to it the slot at address ADDR.  Returns the address
   of the object in the folio (which must be passed to storage_free to
   free the storage).  */
addr_t
storage_alloc (addr_t activity,
	       enum cap_type type, enum storage_expectancy expectancy,
	       addr_t addr)
{
  struct storage_desc *storage;

 restart:
  if (expectancy == STORAGE_EPHEMERAL)
    {
      storage = short_lived;
      if (! storage)
	storage = long_lived_allocing;
      if (! storage)
	storage = long_lived_freeing;
    }
  else
    {
      storage = long_lived_allocing;
      if (! storage)
	{
	  storage = long_lived_freeing;
	  if (! storage)
	    storage = short_lived;

	  if (storage)
	    {
	      unlink (storage);
	      link (&long_lived_allocing, storage);
	    }
	}
    }

  if (! storage || free_count <= MIN_FREE_PAGES)
    /* Insufficient free storage.  Allocate a new storage area.  */
    {
      /* Although we have not yet allocated the objects, allocating
	 structures may require memory causing us to recurse.  Thus,
	 we add them first.  */
      free_count += FOLIO_OBJECTS;

      /* Here is the big recursive dependency!  Using the address that
	 as_alloc returns might require allocating one (or more) page
	 tables to make a slot available.  Moreover, each of those
	 page tables requires not only a cappage but also a shadow
	 page table.  */
      addr_t addr = as_alloc (FOLIO_OBJECTS_LOG2 + PAGESIZE_LOG2,
			      1, true);
      if (ADDR_IS_VOID (addr))
	panic ("Failed to allocate address space!");

      struct cap *cap = as_slot_ensure (addr);
      assert (cap);

      /* And then the folio.  */
      error_t err = rm_folio_alloc (activity, addr);
      assert (! err);

      /* Allocate and fill a descriptor.  */
      struct storage_desc *s = storage_desc_alloc ();

      s->folio = addr;
      memset (&s->alloced, 0, sizeof (s->alloced));
      s->free = FOLIO_OBJECTS;

      if (! storage && expectancy == STORAGE_EPHEMERAL)
	{
	  s->mode = SHORT_LIVED;
	  link (&short_lived, s);
	}
      else
	{
	  s->mode = LONG_LIVED_ALLOCING;
	  link (&long_lived_allocing, s);
	}

      shadow_setup (cap, s);

      /* And finally, insert the new storage descriptor.  */
      hurd_btree_storage_desc_insert (&storage_descs, s);

      /* Having added the storage, we now check if we need to allocate
	 a new reserve slab buffer.  */
      check_slab_space_reserve ();

      if (! storage)
	storage = s;

      if (storage->free == 0)
	/* Some allocations (by as_slot_ensure) appear to have caused
	   us to empty this folio.  Restart.  */
	goto restart;
    }

  int idx = bit_alloc (storage->alloced, sizeof (storage->alloced), 0);
  assert (idx != -1);

  addr_t folio = storage->folio;
  addr_t object = addr_extend (folio, idx, FOLIO_OBJECTS_LOG2);

  debug (5, "Allocating object %d from " ADDR_FMT " (" ADDR_FMT ") "
	 "(%d left), copying to " ADDR_FMT,
	 idx, ADDR_PRINTF (folio), ADDR_PRINTF (object),
	 storage->free, ADDR_PRINTF (addr));

  storage->free --;
  free_count --;

  if (storage->free == 0)
    /* The folio is now full.  */
    {
      unlink (storage);
#ifndef NDEBUG
      storage->next = NULL;
      storage->prevp = NULL;
#endif
      if (storage->mode == LONG_LIVED_ALLOCING)
	/* Change the folio from the allocating state to the stable
	   state.  */
	storage->mode = LONG_LIVED_STABLE;
    }

  if (likely (!! storage->shadow))
    {
      storage->shadow->caps[idx].addr_trans = CAP_ADDR_TRANS_VOID;
      storage->shadow->caps[idx].type = type;
    }
  else
    assert (! as_init_done);

  error_t err = rm_folio_object_alloc (meta_data_activity,
				       folio, idx, type, addr);
  assert (! err);

  if (! ADDR_IS_VOID (addr))
    /* We also have to update the shadow for ADDR.  Unfortunately, we
       don't have the cap although the caller might.  */
    {
      struct cap *cap = slot_lookup (meta_data_activity, addr, -1, NULL);

      cap->type = type;
    }

  return object;
}

void
storage_free (addr_t object, bool unmap_now)
{
  bool freeing_shadow = false;

 restart:;
  addr_t folio = addr_chop (object, FOLIO_OBJECTS_LOG2);

  struct storage_desc *storage;
  storage = hurd_btree_storage_desc_find (&storage_descs, &folio);
  assert (storage);

  int idx = addr_extract (object, FOLIO_OBJECTS_LOG2);
  bit_dealloc (storage->alloced, idx);
  storage->free ++;
  free_count ++;

  error_t err = rm_folio_object_alloc (meta_data_activity,
				       folio, idx, cap_void, ADDR_VOID);
  assert (err == 0);

  /* Update the shadow object.  */
  if (likely (!! storage->shadow))
    {
      storage->shadow->caps[idx].type = cap_void;
      storage->shadow->caps[idx].addr_trans = CAP_ADDR_TRANS_VOID;
    }
  else
    assert (! storage->shadow);


  if (storage->free == FOLIO_OBJECTS - 1)
    {
      if (ADDR_EQ (addr_chop (PTR_TO_ADDR (storage->shadow),
			      FOLIO_OBJECTS_LOG2),
		   storage->folio))
	/* The last allocated page is our shadow page.  Free this
	   folio.  */
	{
	  object = PTR_TO_ADDR (storage->shadow);
	  storage->shadow = NULL;
	  freeing_shadow = true;
	  goto restart;
	}
    }

  else if (storage->free == 1 && storage->mode == LONG_LIVED_STABLE)
    /* The folio is no longer completely full.  Return it to the long
       lived allocating list.  */
    link (&long_lived_allocing, storage);

  else if (storage->mode == LONG_LIVED_STABLE
	   && storage->free > FREEING_THRESHOLD)
    /* The folio is stable and is now less than half allocated.
       Change the folio's state to freeing.  */
    {
      unlink (storage);
      storage->mode = LONG_LIVED_FREEING;
      link (&long_lived_freeing, storage);
    }

  else if (storage->free == FOLIO_OBJECTS)
    /* The folio is now empty.  */
    {
      if (free_count - FOLIO_OBJECTS > MIN_FREE_PAGES)
	/* There are sufficient reserve pages not including this
	   folio.  Thus, we free STORAGE.  */
	{
	  void *shadow = storage->shadow;

	  free_count -= FOLIO_OBJECTS;

	  unlink (storage);
	  hurd_btree_storage_desc_detach (&storage_descs, storage);
	  storage_desc_free (storage);

	  error_t err = rm_folio_free (meta_data_activity, folio);
	  assert (err == 0);

	  if (shadow)
	    {
	      assert (! freeing_shadow);
	      storage_free (PTR_TO_ADDR (shadow), false);
	    }
	  else
	    assert (freeing_shadow);
	}
    }
}

void
storage_init (void)
{
  /* Initialize the slab.  */
  error_t err = hurd_slab_init (&storage_desc_slab,
				sizeof (struct storage_desc), 0,
				storage_desc_slab_alloc,
				storage_desc_slab_dealloc,
				NULL, NULL, NULL);
  assert (! err);

  /* Identify the existing objects and folios.  */
  struct hurd_object_desc *odesc;
  int i;

  for (i = 0, odesc = &__hurd_startup_data->descs[0];
       i < __hurd_startup_data->desc_count;
       i ++, odesc ++)
    {
      addr_t folio;
      if (odesc->type == cap_folio)
	folio = odesc->object;
      else
	folio = addr_chop (odesc->storage, FOLIO_OBJECTS_LOG2);

      struct storage_desc *sdesc;
      sdesc = hurd_btree_storage_desc_find (&storage_descs, &folio);
      if (! sdesc)
	/* Haven't seen this folio yet.  */
	{
	  sdesc = storage_desc_alloc ();
	  sdesc->folio = folio;
	  sdesc->free = FOLIO_OBJECTS;
	  sdesc->mode = LONG_LIVED_ALLOCING;

	  link (&long_lived_allocing, sdesc);

	  hurd_btree_storage_desc_insert (&storage_descs, sdesc);

	  /* Assume that the folio is free.  As we encounter objects,
	     we will mark them as allocated.  */
	  free_count += FOLIO_OBJECTS;
	}

      if (odesc->type != cap_folio)
	{
	  int idx = addr_extract (odesc->storage, FOLIO_OBJECTS_LOG2);

	  debug (5, "%llx/%d, %d -> %llx/%d (%s)",
		 addr_prefix (folio),
		 addr_depth (folio),
		 idx,
		 addr_prefix (odesc->storage),
		 addr_depth (odesc->storage),
		 cap_type_string (odesc->type));

	  bit_set (sdesc->alloced, sizeof (sdesc->alloced), idx);

	  sdesc->free --;
	  free_count --;
	}

    }

  debug (1, "Have %d initial free objects", free_count);
  /* XXX: We can only call this after initialization is complete.
     This presents a problem: we might require additional storage
     descriptors than a single pre-allocated page provides.  This is
     quite unlikely but something we really need to deal with.  */
  check_slab_space_reserve ();
}
