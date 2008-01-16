/* storage.c - Storage allocation functions.
   Copyright (C) 2007, 2008 Free Software Foundation, Inc.
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
#include <hurd/mutex.h>

#ifndef NDEBUG
struct ss_lock_trace ss_lock_trace[SS_LOCK_TRACE_COUNT];
int ss_lock_trace_count;
#endif

#include <bit-array.h>

#include <stddef.h>
#include <string.h>
#include <atomic.h>

#include "as.h"

extern struct hurd_startup_data *__hurd_startup_data;

/* Objects are allocated from folios.  As a folio is the unit of
   storage allocation, we can only free a folio when it is completely
   empty.  For this reason, we try to group long lived objects
   together and short lived objects together.  */

/* Total number of free objects across all folios.  */
static uatomic32_t free_count;

struct storage_desc
{
  /* The address of the folio.  */
  addr_t folio;
  /* The location of the shadow cap designating this folio.  */
  struct cap *cap;

  /* Which objects are allocated.  */
  unsigned char alloced[FOLIO_OBJECTS / 8];

  /* The number of free objects.  */
  unsigned char free;

  /* The storage object's mode of allocation.  */
  char mode;

  /* Protects all members above here.  This lock may be taken if
     STORAGE_DESCS_LOCK is held.  */
  ss_mutex_t lock;


  /* Each storage area is stored in a btree keyed by the address of
     the folio.  Protected by STORAGE_DESCS_LOCK.  */
  hurd_btree_node_t node;

  struct storage_desc *next;
  struct storage_desc **prevp;
};

/* Protects the node, next and prevp field in each storage descriptor.
   Do not take this lock if the thread holds any DESC->LOCK!  */
static ss_mutex_t storage_descs_lock;

static void
link (struct storage_desc **list, struct storage_desc *e)
{
  /* Better be locked.  */
  assert (! ss_mutex_trylock (&storage_descs_lock));

  e->next = *list;
  if (e->next)
    e->next->prevp = &e->next;
  e->prevp = list;
  *list = e;
}

#define LINK(l, e) \
  ({ \
    debug (1, "link(%p, %p)", (l), (e)); \
    LINK ((l), (e)); \
  })

static void
unlink (struct storage_desc *e)
{
  /* Better be locked.  */
  assert (! ss_mutex_trylock (&storage_descs_lock));

  assert (e->prevp);

  *e->prevp = e->next;
  if (e->next)
    e->next->prevp = e->prevp;

#ifndef NDEBUG
  /* Try to detect multiple unlink.  */
  e->next = NULL;
  e->prevp = NULL;
#endif
}

#define UNLINK(e) \
  ({ \
    debug (1, "unlink(%p)", (e)); \
    UNLINK ((e)); \
  })

static int
addr_compare (const addr_t *a, const addr_t *b)
{
  if (a->raw < b->raw)
    return -1;
  return a->raw != b->raw;
}

BTREE_CLASS (storage_desc, struct storage_desc,
	     addr_t, folio, node, addr_compare, false)

static hurd_btree_storage_desc_t storage_descs;

/* Storage descriptors are alloced from a slab.  */
static struct hurd_slab_space storage_desc_slab;

/* Before we are initialized, allocating data instruction addressable
   storage is at best awkward primarily because we may also need to
   create page tables.  We avoid this by preallocating some slab
   space.  */
static char slab_space[PAGESIZE] __attribute__ ((aligned(PAGESIZE)));
static atomicptr_t slab_space_reserve = (atomicptr_t) slab_space;

/* After allocating a descriptor, we check to see whether there is
   still a reserve page.  By keeping a page of storage available, we
   avoid the problematic situation of trying to allocate storage and
   insert it into the addresss space but requiring storage and a
   storage descriptor (of which there are none!) to do so.  */
static void
check_slab_space_reserve (void)
{
  bool r = !!slab_space_reserve;
  if (likely (r))
    return;

  /* We don't have a reserve.  Allocate one now.  */
  struct storage storage = storage_alloc (meta_data_activity, cap_page,
					  STORAGE_LONG_LIVED, ADDR_VOID);
  void *buffer = ADDR_TO_PTR (addr_extend (storage.addr, 0, PAGESIZE_LOG2));

  buffer = (void *) atomic_exchange_acq (&slab_space_reserve, buffer);
  if (buffer)
    /* Someone else allocated a buffer.  We don't need two, so
       deallocate it.  */
    storage_free (addr_chop (PTR_TO_ADDR (buffer), PAGESIZE_LOG2), false);
}

static error_t
storage_desc_slab_alloc (void *hook, size_t size, void **ptr)
{
  assert (size == PAGESIZE);

  void *buffer = (void *) atomic_exchange_acq (&slab_space_reserve, NULL);
  assert (buffer);

  *ptr = buffer;

  return 0;
}

static error_t
storage_desc_slab_dealloc (void *hook, void *buffer, size_t size)
{
  /* There is no need to special case SLAB_SPACE; we can free it in
     the normal way and the right thing will happen.  */

  assert (size == PAGESIZE);

  addr_t addr = addr_chop (PTR_TO_ADDR (buffer), PAGESIZE_LOG2);
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
  assert (! ss_mutex_trylock (&storage->lock));

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
  /* We do not need to hold STORAGE->LOCK here as either we are in the
     init phase and thus single threaded or we are initializing a new
     storage descriptor, which is still unreachable from any other
     thread.  */

  struct object *shadow;

  int idx = bit_alloc (storage->alloced, sizeof (storage->alloced), 0);
  if (likely (idx != -1))
    {
      storage->free --;
      atomic_decrement (&free_count);

      error_t err = rm_folio_object_alloc (meta_data_activity,
					   storage->folio, idx, cap_page,
					   OBJECT_POLICY_DEFAULT, 0,
					   ADDR_VOID, ADDR_VOID);
      assert (err == 0);
      shadow = ADDR_TO_PTR (addr_extend (addr_extend (storage->folio,
						      idx, FOLIO_OBJECTS_LOG2),
					 0, PAGESIZE_LOG2));

      if (storage->free == 0)
	/* This can happen when starting up.  */
	{
	  assert (! as_init_done);

	  ss_mutex_lock (&storage_descs_lock);
	  ss_mutex_lock (&storage->lock);

	  /* STORAGE->FREE may be zero if someone came along and deallocated
	     a page between our dropping and retaking the lock.  */
	  if (storage->free == 0)
	    unlink (storage);

	  ss_mutex_unlock (&storage->lock);
	  ss_mutex_unlock (&storage_descs_lock);
	}
    }
  else
    /* This only happens during startup.  Otherwise, we always
       allocate a shadow page before we use the storage.  */
    {
      assert (! as_init_done);

      struct storage storage = storage_alloc (meta_data_activity, cap_page,
					      STORAGE_LONG_LIVED, ADDR_VOID);
      if (ADDR_IS_VOID (storage.addr))
	panic ("Out of storage.");
      shadow = ADDR_TO_PTR (addr_extend (storage.addr, 0, PAGESIZE_LOG2));
    }

  storage->cap = cap;
  cap->type = cap_folio;

  cap_set_shadow (cap, shadow);

  shadow->caps[idx].type = cap_page;
  CAP_PROPERTIES_SET (&shadow->caps[idx],
		      CAP_PROPERTIES (OBJECT_POLICY_DEFAULT,
				      CAP_ADDR_TRANS_VOID));
}

void
storage_shadow_setup (struct cap *cap, addr_t folio)
{
  /* This code is only called from the initialization code.  When this
     code runs, there is exactly one thread.  Thus, there is no need
     to get the STORAGE_DESCS_LOCK.  */

  struct storage_desc *sdesc;
  sdesc = hurd_btree_storage_desc_find (&storage_descs, &folio);
  assert (sdesc);

  shadow_setup (cap, sdesc);
}

struct storage
storage_alloc_ (addr_t activity,
		enum cap_type type, enum storage_expectancy expectancy,
		addr_t addr)
{
  atomic_read_barrier ();
  if (unlikely (free_count <= MIN_FREE_PAGES))
    /* Insufficient storage reserve.  Allocate a new storage area.  */
    {
    do_allocate:
      debug (3, "Allocating additional folio, free count: %d", free_count);

      /* Although we have not yet allocated the objects, allocating
	 support structures for the folio may require memory causing
	 us to recurse.  Thus, we add them first.  */
      atomic_add (&free_count, FOLIO_OBJECTS);

      /* Here is the big recursive dependency!  Using the address that
	 as_alloc returns might require allocating one (or more) page
	 tables to make a slot available.  Moreover, each of those
	 page tables requires not only a cappage but also a shadow
	 page table.  */
      addr_t addr;
      struct cap *cap = NULL;
      if (likely (as_init_done))
	{
	  addr = as_alloc (FOLIO_OBJECTS_LOG2 + PAGESIZE_LOG2, 1, true);
	  if (ADDR_IS_VOID (addr))
	    panic ("Failed to allocate address space!");

	  cap = as_slot_ensure (addr);
	  assert (cap);
	}
      else
	{
	  struct hurd_object_desc *desc;
	  desc = as_alloc_slow (FOLIO_OBJECTS_LOG2 + PAGESIZE_LOG2);
	  if (! desc || ADDR_IS_VOID (desc->object))
	    panic ("Failed to allocate address space!");

	  addr = desc->object;
	  desc->storage = addr;
	  desc->type = cap_folio;
	}

      /* And then the folio.  */
      error_t err = rm_folio_alloc (activity, addr, FOLIO_POLICY_DEFAULT);
      assert (! err);

      /* Allocate and fill a descriptor.  */
      struct storage_desc *s = storage_desc_alloc ();

      s->lock = (ss_mutex_t) 0;
      s->folio = addr;
      memset (&s->alloced, 0, sizeof (s->alloced));
      s->free = FOLIO_OBJECTS;
      s->cap = cap;

      if (cap)
	shadow_setup (cap, s);

      /* S is setup.  Make it available.  */
      ss_mutex_lock (&storage_descs_lock);

      if (expectancy == STORAGE_EPHEMERAL)
	{
	  s->mode = SHORT_LIVED;
	  link (&short_lived, s);
	}
      else
	{
	  s->mode = LONG_LIVED_ALLOCING;
	  link (&long_lived_allocing, s);
	}

      hurd_btree_storage_desc_insert (&storage_descs, s);

      ss_mutex_unlock (&storage_descs_lock);

      /* Having added the storage, we now check if we need to allocate
	 a new reserve slab buffer.  */
      check_slab_space_reserve ();
    }


  /* Find an appropriate storage area.  */
  struct storage_desc *pluck (struct storage_desc *list)
  {
    while (list)
      {
	/* We could just wait on the lock, however, we can just as
	   well allocate from another storage descriptor.  This may
	   lead to allocating additional storage areas, however, this
	   should be proportional to the contention.  */
	if (ss_mutex_trylock (&list->lock))
	  return list;

	list = list->next;
      }

    return NULL;
  }

  ss_mutex_lock (&storage_descs_lock);

  struct storage_desc *desc;
  if (expectancy == STORAGE_EPHEMERAL)
    {
      desc = pluck (short_lived);
      if (! desc)
	desc = pluck (long_lived_allocing);
      if (! desc)
	desc = pluck (long_lived_freeing);
    }
  else
    {
      desc = pluck (long_lived_allocing);
      if (! desc)
	{
	  desc = pluck (long_lived_freeing);
	  if (! desc)
	    desc = pluck (short_lived);

	  if (desc)
	    {
	      unlink (desc);
	      link (&long_lived_allocing, desc);
	    }
	}
    }

  ss_mutex_unlock (&storage_descs_lock);

  if (! desc)
    /* There are no unlock storage areas available.  Allocate one.  */
    goto do_allocate;

  /* DESC desigantes a storage area from which we can allocate a page.
     DESC->LOCK is held.  */

  int idx = bit_alloc (desc->alloced, sizeof (desc->alloced), 0);
  assertx (idx != -1,
	   "Folio (" ADDR_FMT ") full (free: %d) but on a list!",
	   ADDR_PRINTF (desc->folio), desc->free);

  addr_t folio = desc->folio;
  addr_t object = addr_extend (folio, idx, FOLIO_OBJECTS_LOG2);

  debug (5, "Allocating object %d from " ADDR_FMT " (" ADDR_FMT ") "
	 "(%d left), copying to " ADDR_FMT,
	 idx, ADDR_PRINTF (folio), ADDR_PRINTF (object),
	 desc->free, ADDR_PRINTF (addr));

  atomic_decrement (&free_count);
  desc->free --;

  bool need_unlink = false;
  if (desc->free == 0)
    /* The folio is now full.  We can't take the STORAGE_DESCS_LOCK
       lock as we have DESC->LOCK.  Finish what we are doing and only
       then actually remove it.  */
    {
      assert (bit_alloc (desc->alloced, sizeof (desc->alloced), 0) == -1);

      debug (1, "Folio at " ADDR_FMT " full", ADDR_PRINTF (folio));

      need_unlink = true;
      if (desc->mode == LONG_LIVED_ALLOCING)
	/* Change the folio from the allocating state to the stable
	   state.  */
	desc->mode = LONG_LIVED_STABLE;
    }

  struct object *shadow = desc->cap ? cap_get_shadow (desc->cap) : NULL;
  struct cap *cap = NULL;
  if (likely (!! shadow))
    {
      cap = &shadow->caps[idx];
      CAP_PROPERTIES_SET (cap, CAP_PROPERTIES (OBJECT_POLICY_DEFAULT,
					       CAP_ADDR_TRANS_VOID));
      cap->type = type;
    }
  else
    assert (! as_init_done);

  error_t err = rm_folio_object_alloc (meta_data_activity,
				       folio, idx, type,
				       OBJECT_POLICY_DEFAULT, 0,
				       addr, ADDR_VOID);
  assert (! err);

  /* We drop DESC->LOCK.  */
  ss_mutex_unlock (&desc->lock);

  if (need_unlink)
    /* We noted that the folio was full.  Unlink it now.  */
    {
      ss_mutex_lock (&storage_descs_lock);
      ss_mutex_lock (&desc->lock);

      /* DESC->FREE may be zero if someone came along and deallocated
	 a page between our dropping and retaking the lock.  */
      if (desc->free == 0)
	unlink (desc);

      ss_mutex_unlock (&desc->lock);
      ss_mutex_unlock (&storage_descs_lock);
    }

  if (! ADDR_IS_VOID (addr))
    /* We also have to update the shadow for ADDR.  Unfortunately, we
       don't have the cap although the caller might.  */
    {
      struct cap *cap = slot_lookup (meta_data_activity, addr, -1, NULL);
      if (! cap)
	as_dump (NULL);
      assert (cap);
      cap->type = type;
    }

  struct storage storage;
  storage.cap = cap;
  storage.addr = object;

  return storage;
}

void
storage_free_ (addr_t object, bool unmap_now)
{
  addr_t folio = addr_chop (object, FOLIO_OBJECTS_LOG2);

  atomic_increment (&free_count);

  ss_mutex_lock (&storage_descs_lock);

  /* Find the storage descriptor.  */
  struct storage_desc *storage;
  storage = hurd_btree_storage_desc_find (&storage_descs, &folio);
  assert (storage);

  ss_mutex_lock (&storage->lock);

  storage->free ++;

  struct object *shadow = storage->cap ? cap_get_shadow (storage->cap) : NULL;

  if (storage->free == FOLIO_OBJECTS
      || ((storage->free == FOLIO_OBJECTS - 1)
	  && shadow
	  && ADDR_EQ (folio, addr_chop (PTR_TO_ADDR (shadow),
					FOLIO_OBJECTS_LOG2 + PAGESIZE_LOG2))))
    /* The folio is now empty.  */
    {
      debug (1, "Folio at " ADDR_FMT " now empty", ADDR_PRINTF (folio));

      if (free_count - FOLIO_OBJECTS > MIN_FREE_PAGES)
	/* There are sufficient reserve pages not including this
	   folio.  Thus, we free STORAGE.  */
	{
	  atomic_add (&free_count, - FOLIO_OBJECTS);

	  unlink (storage);
	  hurd_btree_storage_desc_detach (&storage_descs, storage);

	  ss_mutex_unlock (&storage_descs_lock);


	  cap_set_shadow (storage->cap, NULL);
	  storage->cap->type = cap_void;

	  storage_desc_free (storage);

	  error_t err = rm_folio_free (meta_data_activity, folio);
	  assert (err == 0);

	  if (shadow)
	    {
	      addr_t shadow_addr = addr_chop (PTR_TO_ADDR (shadow),
					      PAGESIZE_LOG2);

	      if (ADDR_EQ (addr_chop (shadow_addr, FOLIO_OBJECTS_LOG2), folio))
		{
		  /* The shadow was allocate from ourself, which we
		     already freed.  */
		}
	      else
		storage_free (shadow_addr, false);
	    }

	  return;
	}
    }

  if (storage->free == 1)
    {
      /* The folio is no longer completely full.  Return it to a
	 list.  */
      if (storage->mode == STORAGE_EPHEMERAL)
	link (&short_lived, storage);
      else
	link (&long_lived_allocing, storage);
    }

  else if (storage->mode == LONG_LIVED_STABLE
	   && storage->free > FREEING_THRESHOLD)
    /* The folio is stable and is now less than half allocated.
       Change the folio's state to freeing.  */
    {
      unlink (storage);
      storage->mode = LONG_LIVED_FREEING;
      link (&long_lived_freeing, storage);
    }

  ss_mutex_unlock (&storage_descs_lock);

  int idx = addr_extract (object, FOLIO_OBJECTS_LOG2);
  bit_dealloc (storage->alloced, idx);

  error_t err = rm_folio_object_alloc (meta_data_activity,
				       folio, idx, cap_void,
				       OBJECT_POLICY_DEFAULT, 0,
				       ADDR_VOID, ADDR_VOID);
  assert (err == 0);

  if (likely (!! shadow))
    {
      shadow->caps[idx].type = cap_void;
      CAP_PROPERTIES_SET (&shadow->caps[idx],
			  CAP_PROPERTIES (OBJECT_POLICY_DEFAULT,
					  CAP_ADDR_TRANS_VOID));
    }
  else
    assert (! as_init_done);

  ss_mutex_unlock (&storage->lock);
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

  /* We are single threaded, however, several functions assert that
     this lock is held.  */
  ss_mutex_lock (&storage_descs_lock);

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
	  sdesc->lock = (ss_mutex_t) 0;
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

	  if (sdesc->free == 0)
	    unlink (sdesc);
	}
    }

  ss_mutex_unlock (&storage_descs_lock);

  debug (1, "Have %d initial free objects", free_count);
  /* XXX: We can only call this after initialization is complete.
     This presents a problem: we might require additional storage
     descriptors than a single pre-allocated page provides.  This is
     quite unlikely but something we really need to deal with.  */
  check_slab_space_reserve ();
}
