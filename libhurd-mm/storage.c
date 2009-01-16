/* storage.c - Storage allocation functions.
   Copyright (C) 2007, 2008 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 3 of the
   License, or (at your option) any later version.

   The GNU Hurd is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see
   <http://www.gnu.org/licenses/>.  */

#include "storage.h"

#include <hurd/stddef.h>
#include <hurd/btree.h>
#include <hurd/slab.h>
#include <viengoos/addr.h>
#include <viengoos/folio.h>
#include <hurd/startup.h>
#include <viengoos/misc.h>
#include <hurd/mutex.h>
#include <backtrace.h>

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
  vg_addr_t folio;
  /* The location of the shadow vg_cap designating this folio.  */
  struct vg_object *shadow;

  /* Which objects are allocated.  */
  unsigned char alloced[VG_FOLIO_OBJECTS / 8];

  /* The number of free objects.  */
  unsigned char free;

  /* The storage object's mode of allocation.  */
  char mode;

  /* Protects all members above here.  This lock may be taken if
     STORAGE_DESCS_LOCK is held.  */
  ss_mutex_t lock;
  vg_thread_id_t owner;


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
list_link (struct storage_desc **list, struct storage_desc *e)
{
  /* Better be locked.  */
  assert (! ss_mutex_trylock (&storage_descs_lock));

  e->next = *list;
  if (e->next)
    e->next->prevp = &e->next;
  e->prevp = list;
  *list = e;
}

static void
list_unlink (struct storage_desc *e)
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

static int
addr_compare (const vg_addr_t *a, const vg_addr_t *b)
{
  if (a->raw < b->raw)
    return -1;
  return a->raw != b->raw;
}

BTREE_CLASS (storage_desc, struct storage_desc,
	     vg_addr_t, folio, node, addr_compare, false)

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
  struct storage storage = storage_alloc (meta_data_activity, vg_cap_page,
					  STORAGE_LONG_LIVED,
					  VG_OBJECT_POLICY_DEFAULT,
					  VG_ADDR_VOID);
  void *buffer = VG_ADDR_TO_PTR (vg_addr_extend (storage.addr,
						 0, PAGESIZE_LOG2));

  buffer = (void *) atomic_exchange_acq (&slab_space_reserve, buffer);
  if (buffer)
    /* Someone else allocated a buffer.  We don't need two, so
       deallocate it.  */
    storage_free (vg_addr_chop (VG_PTR_TO_ADDR (buffer), PAGESIZE_LOG2),
		  false);
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

  vg_addr_t addr = vg_addr_chop (VG_PTR_TO_ADDR (buffer), PAGESIZE_LOG2);
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
#define FREEING_THRESHOLD (VG_FOLIO_OBJECTS / 2)

static void
shadow_setup (struct vg_cap *cap, struct storage_desc *desc)
{
  /* We do not need to hold DESC->LOCK here as either we are in the
     init phase and thus single threaded or we are initializing a new
     storage descriptor, which is still unreachable from any other
     thread.  */

  struct vg_object *shadow;

  int idx = bit_alloc (desc->alloced, sizeof (desc->alloced), 0);
  if (likely (idx != -1))
    {
      desc->free --;
      atomic_decrement (&free_count);

      error_t err = vg_folio_object_alloc (meta_data_activity,
					   desc->folio, idx, vg_cap_page,
					   VG_OBJECT_POLICY_DEFAULT, 0,
					   NULL, NULL);
      assert (err == 0);
      shadow = VG_ADDR_TO_PTR (vg_addr_extend
			       (vg_addr_extend (desc->folio,
						idx, VG_FOLIO_OBJECTS_LOG2),
				0, PAGESIZE_LOG2));

      if (desc->free == 0)
	/* This can happen when starting up.  */
	{
	  assert (! as_init_done);

	  ss_mutex_lock (&storage_descs_lock);
	  ss_mutex_lock (&desc->lock);
	  assert (desc->owner == vg_niltid);
	  desc->owner = hurd_myself ();

	  /* DESC->FREE may be zero if someone came along and deallocated
	     a page between our dropping and retaking the lock.  */
	  if (desc->free == 0)
	    list_unlink (desc);

	  assert (desc->owner == hurd_myself ());
	  desc->owner = vg_niltid;
	  ss_mutex_unlock (&desc->lock);
	  ss_mutex_unlock (&storage_descs_lock);
	}
    }
  else
    /* This only happens during startup.  Otherwise, we always
       allocate a shadow page before we use the storage.  */
    {
      assert (! as_init_done);

      struct storage storage = storage_alloc (meta_data_activity, vg_cap_page,
					      STORAGE_LONG_LIVED,
					      VG_OBJECT_POLICY_DEFAULT,
					      VG_ADDR_VOID);
      if (VG_ADDR_IS_VOID (storage.addr))
	panic ("Out of storage.");
      shadow = VG_ADDR_TO_PTR (vg_addr_extend (storage.addr,
					       0, PAGESIZE_LOG2));
    }

  desc->shadow = shadow;

  cap->type = vg_cap_folio;
  VG_CAP_SET_SUBPAGE (cap, 0, 1);
  vg_cap_set_shadow (cap, shadow);

  if (idx != -1)
    {
      shadow->caps[idx].type = vg_cap_page;
      VG_CAP_PROPERTIES_SET (&shadow->caps[idx],
			     VG_CAP_PROPERTIES (VG_OBJECT_POLICY_DEFAULT,
						VG_CAP_ADDR_TRANS_VOID));
    }
}

void
storage_shadow_setup (struct vg_cap *cap, vg_addr_t folio)
{
  /* This code is only called from the initialization code.  When this
     code runs, there is exactly one thread.  Thus, there is no need
     to get the STORAGE_DESCS_LOCK.  */

  struct storage_desc *sdesc;
  sdesc = hurd_btree_storage_desc_find (&storage_descs, &folio);
  assert (sdesc);

  shadow_setup (cap, sdesc);
}

static bool storage_init_done;

static int
num_threads (void)
{
  extern int __pthread_num_threads __attribute__ ((weak));

  if (&__pthread_num_threads)
    return __pthread_num_threads;
  else
    return 1;
}

/* The minimum number of pages that should be available.  */
#define FREE_PAGES_LOW_WATER (32 + 16 * num_threads ())
/* If the number of free pages drops below this amount, the we might
   soon have a problem.  In this case, we serialize access to the pool
   of available pages to allow some thread that is able to allocate
   more pages the chance to do so.  */
#define FREE_PAGES_SERIALIZE (16 + 8 * num_threads ())

static pthread_mutex_t storage_low_mutex
  = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

bool
storage_have_reserve (void)
{
  return likely (free_count > FREE_PAGES_LOW_WATER);
}

static bool do_serialize;

static void
storage_check_reserve_internal (bool force_allocate,
				vg_addr_t activity,
				enum storage_expectancy expectancy,
				bool i_may_have_lock)
{
 top:
  if (! force_allocate && storage_have_reserve ())
    return;

  /* Insufficient storage reserve.  Allocate a new storage area.  */

  if (i_may_have_lock && free_count > 0)
    /* XXX: as_insert calls allocate_object, which calls us.  When
       we allocate a new folio, we need to insert it into the
       address space.  This requires calling as_insert, which
       results in a deadlock.  Here we try to take the lock.  If
       we succeed, then we don't hold the lock and it is okay to
       call as_insert.  If we fail, then either there is a lot of
       contention or we would deadlock.  As long as the page
       reserve is large enough, this is not a problem.  A solution
       would be to check in as_insert whether there are any free
       pages and if not to call some as-yet unwritten function
       which forces the reserve to grow.  */
    {
      extern vg_thread_id_t as_rwlock_owner;
      if (as_rwlock_owner)
	{
	  if (as_rwlock_owner == hurd_myself ())
	    /* This thread has the lock.  */
	    return;
	}
      else
	/* AS_RWLOCK_OWNER is 0.  Either the lock is not held or its
	   held read-only.  See if we can get a write lock.  If so,
	   its the former.  */
	{
	  extern pthread_rwlock_t as_rwlock;
	  if (pthread_rwlock_trywrlock (&as_rwlock) == EBUSY)
	    /* Conservatively assume that we have the lock.  */
	    return;

	  pthread_rwlock_unlock (&as_rwlock);
	}

      i_may_have_lock = false;
    }

  bool have_lock = false;
  bool did_serialize = false;
  if (do_serialize || free_count < FREE_PAGES_SERIALIZE)
    {
      if (pthread_mutex_trylock (&storage_low_mutex) == EBUSY)
	/* Someone else is in.  */
	{
	  /* Wait.  */
	  pthread_mutex_lock (&storage_low_mutex);
	  pthread_mutex_unlock (&storage_low_mutex);
	  /* Retry from the beginning.  */
	  goto top;
	}

      if (! do_serialize)
	{
	  do_serialize = true;
	  did_serialize = true;
	}

      have_lock = true;
    }

  if (free_count == 0)
    debug (0, "free_count is zero.");

  debug (3, "Allocating additional folio, free count: %d",
	 free_count);

  /* Although we have not yet allocated the objects, allocating
     support structures for the folio may require memory causing
     us to recurse.  Thus, we add them first.  */
  atomic_add (&free_count, VG_FOLIO_OBJECTS);

  /* Here is the big recursive dependency!  Using the address that
     as_alloc returns might require allocating one (or more) page
     tables to make a slot available.  Moreover, each of those
     page tables requires not only a cappage but also a shadow
     page table.  */
  vg_addr_t addr;
  if (likely (as_init_done))
    {
      addr = as_alloc (VG_FOLIO_OBJECTS_LOG2 + PAGESIZE_LOG2, 1, true);
      if (VG_ADDR_IS_VOID (addr))
	panic ("Failed to allocate address space!");

      as_ensure (addr);
    }
  else
    {
      struct hurd_object_desc *desc;
      desc = as_alloc_slow (VG_FOLIO_OBJECTS_LOG2 + PAGESIZE_LOG2);
      if (! desc || VG_ADDR_IS_VOID (desc->object))
	panic ("Failed to allocate address space!");

      addr = desc->object;
      desc->storage = addr;
      desc->type = vg_cap_folio;
    }

  /* And then the folio.  */
  vg_addr_t a = addr;
  error_t err = vg_folio_alloc (activity, activity, VG_FOLIO_POLICY_DEFAULT,
				&a);
  assert (! err);
  assert (VG_ADDR_EQ (addr, a));

  /* Allocate and fill a descriptor.  */
  struct storage_desc *s = storage_desc_alloc ();

  s->lock = (ss_mutex_t) 0;
  s->owner = vg_niltid;
  s->folio = addr;
  memset (&s->alloced, 0, sizeof (s->alloced));
  s->free = VG_FOLIO_OBJECTS;

  if (likely (as_init_done))
    {
      bool ret = as_slot_lookup_use (addr,
				     ({
				       shadow_setup (slot, s);
				     }));
      assert (ret);
    }

  /* S is setup.  Make it available.  */
  ss_mutex_lock (&storage_descs_lock);

  if (expectancy == STORAGE_EPHEMERAL)
    {
      s->mode = SHORT_LIVED;
      list_link (&short_lived, s);
    }
  else
    {
      s->mode = LONG_LIVED_ALLOCING;
      list_link (&long_lived_allocing, s);
    }

  hurd_btree_storage_desc_insert (&storage_descs, s);

  ss_mutex_unlock (&storage_descs_lock);

  /* Having added the storage, we now check if we need to allocate
     a new reserve slab buffer.  */
  check_slab_space_reserve ();

  if (have_lock)
    {
      if (did_serialize)
	{
	  assert (do_serialize);
	  do_serialize = false;
	}
      pthread_mutex_unlock (&storage_low_mutex);
    }
}

void
storage_check_reserve (bool i_may_have_lock)
{
  storage_check_reserve_internal (false, meta_data_activity, STORAGE_UNKNOWN,
				  i_may_have_lock);
}

#undef storage_alloc
struct storage
storage_alloc (vg_addr_t activity,
	       enum vg_cap_type type, enum storage_expectancy expectancy,
	       struct vg_object_policy policy,
	       vg_addr_t addr)
{
  assert (storage_init_done);

  struct storage_desc *desc;
  bool do_allocate = false;
  int tries = 0;
  do
    {
      if (++ tries == 5)
	{
	  backtrace_print ();
	  debug (0, "Failing to get storage (free count: %d).  Live lock?",
		 free_count);
	}

      storage_check_reserve_internal (do_allocate, meta_data_activity,
				      expectancy, true);

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
	      {
		assert (list->owner == vg_niltid);
		list->owner = hurd_myself ();
		return list;
	      }

	    if (tries == 5)
	      debug (0, "Folio at " VG_ADDR_FMT " %d free (locked by %x)",
		     VG_ADDR_PRINTF (list->folio), list->free,
		     list->owner);

	    list = list->next;
	  }

	return NULL;
      }

      ss_mutex_lock (&storage_descs_lock);

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
		  list_unlink (desc);
		  list_link (&long_lived_allocing, desc);
		}
	    }
	}

      if (! desc || desc->free != 1)
	/* Only drop this lock if we are not about to allocate the last
	   page.  Otherwise, we still need the lock.  */
	ss_mutex_unlock (&storage_descs_lock);

      do_allocate = true;
    }
  while (! desc);

  /* DESC desigantes a storage area from which we can allocate a page.
     DESC->LOCK is held.  */

  int idx = bit_alloc (desc->alloced, sizeof (desc->alloced), 0);
  assertx (idx != -1,
	   "Folio (" VG_ADDR_FMT ") full (free: %d) but on a list!",
	   VG_ADDR_PRINTF (desc->folio), desc->free);

  vg_addr_t folio = desc->folio;
  vg_addr_t object = vg_addr_extend (folio, idx, VG_FOLIO_OBJECTS_LOG2);

  debug (5, "Allocating object %d as %s from " VG_ADDR_FMT " (" VG_ADDR_FMT ") "
	 "(%d left), installing at " VG_ADDR_FMT,
	 idx, vg_cap_type_string (type),
	 VG_ADDR_PRINTF (folio), VG_ADDR_PRINTF (object),
	 desc->free, VG_ADDR_PRINTF (addr));

  atomic_decrement (&free_count);
  desc->free --;

  if (desc->free == 0)
    /* The folio is now full.  We can't take the STORAGE_DESCS_LOCK
       lock as we have DESC->LOCK.  Finish what we are doing and only
       then actually remove it.  */
    {
      assert (bit_alloc (desc->alloced, sizeof (desc->alloced), 0) == -1);

      debug (3, "Folio at " VG_ADDR_FMT " full", VG_ADDR_PRINTF (folio));

      list_unlink (desc);

      if (desc->mode == LONG_LIVED_ALLOCING)
	/* Change the folio from the allocating state to the stable
	   state.  */
	desc->mode = LONG_LIVED_STABLE;

      ss_mutex_unlock (&storage_descs_lock);
    }

  struct vg_object *shadow = desc->shadow;
  struct vg_cap *cap = NULL;
  if (likely (!! shadow))
    {
      cap = &shadow->caps[idx];
      VG_CAP_PROPERTIES_SET (cap, VG_CAP_PROPERTIES (policy,
						     VG_CAP_ADDR_TRANS_VOID));
      cap->type = type;
    }
  else
    assert (! as_init_done);

  /* We drop DESC->LOCK.  */
  assert (desc->owner == hurd_myself ());
  desc->owner = vg_niltid;
  ss_mutex_unlock (&desc->lock);

  vg_addr_t a = addr;
  error_t err = vg_folio_object_alloc (activity, folio, idx, type, policy, 0,
				       &a, NULL);
  assertx (! err,
	   "Allocating object %d from " VG_ADDR_FMT " at " VG_ADDR_FMT ": %d!",
	   idx, VG_ADDR_PRINTF (folio), VG_ADDR_PRINTF (addr), err);
  assert (VG_ADDR_EQ (a, addr));

  if (! VG_ADDR_IS_VOID (addr))
    /* We also have to update the shadow for VG_ADDR.  Unfortunately, we
       don't have the cap although the caller might.  */
    {
      bool ret = as_slot_lookup_use
	(addr,
	 ({
	   slot->type = type;
	   vg_cap_set_shadow (slot, NULL);
	   VG_CAP_POLICY_SET (slot, policy);
	 }));
      if (! ret)
	{
	  as_dump (NULL);
	  assert (ret);
	}
    }

  struct storage storage;
  storage.cap = cap;
  storage.addr = object;

#ifndef NDEBUG
  if (type == vg_cap_page)
    {
      unsigned int *p = VG_ADDR_TO_PTR (vg_addr_extend (storage.addr,
						  0, PAGESIZE_LOG2));
      int c;
      for (c = 0; c < PAGESIZE / sizeof (int); c ++)
	assertx (p[c] == 0,
		 VG_ADDR_FMT "(%p)[%d] = %x",
		 VG_ADDR_PRINTF (storage.addr), p, c * sizeof (int), p[c]);
    }
#endif
  debug (5, "Allocated " VG_ADDR_FMT "; " VG_ADDR_FMT,
	 VG_ADDR_PRINTF (storage.addr), VG_ADDR_PRINTF (addr));

  return storage;
}

void
storage_free_ (vg_addr_t object, bool unmap_now)
{
  debug (5, DEBUG_BOLD ("Freeing " VG_ADDR_FMT), VG_ADDR_PRINTF (object));

  vg_addr_t folio = vg_addr_chop (object, VG_FOLIO_OBJECTS_LOG2);

  ss_mutex_lock (&storage_descs_lock);

  /* Find the storage descriptor.  */
  struct storage_desc *storage;
  storage = hurd_btree_storage_desc_find (&storage_descs, &folio);
  assertx (storage,
	   "No storage associated with " VG_ADDR_FMT " "
	   "(did you pass the storage address?)",
	   VG_ADDR_PRINTF (object));

  ss_mutex_lock (&storage->lock);
  assert (storage->owner == vg_niltid);
  storage->owner = hurd_myself ();

  atomic_increment (&free_count);
  storage->free ++;

  struct vg_object *shadow = storage->shadow;

  if (storage->free == VG_FOLIO_OBJECTS
      || ((storage->free == VG_FOLIO_OBJECTS - 1)
	  && shadow
	  && VG_ADDR_EQ (folio,
			 vg_addr_chop (VG_PTR_TO_ADDR (shadow),
				       VG_FOLIO_OBJECTS_LOG2 + PAGESIZE_LOG2))))
    /* The folio is now empty.  */
    {
      debug (1, "Folio at " VG_ADDR_FMT " now empty", VG_ADDR_PRINTF (folio));

      if (free_count - VG_FOLIO_OBJECTS > FREE_PAGES_LOW_WATER)
	/* There are sufficient reserve pages not including this
	   folio.  Thus, we free STORAGE.  */
	{
	  atomic_add (&free_count, - VG_FOLIO_OBJECTS);

	  list_unlink (storage);
	  hurd_btree_storage_desc_detach (&storage_descs, storage);

	  ss_mutex_unlock (&storage_descs_lock);


	  error_t err = vg_folio_free (meta_data_activity, folio);
	  assert (err == 0);

	  as_slot_lookup_use (folio,
			      ({
				vg_cap_set_shadow (slot, NULL);
				slot->type = vg_cap_void;
			      }));

	  storage_desc_free (storage);

	  if (shadow)
	    {
	      vg_addr_t shadow_addr = vg_addr_chop (VG_PTR_TO_ADDR (shadow),
					      PAGESIZE_LOG2);

	      if (VG_ADDR_EQ (vg_addr_chop (shadow_addr,
					    VG_FOLIO_OBJECTS_LOG2), folio))
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
	list_link (&short_lived, storage);
      else
	list_link (&long_lived_allocing, storage);
    }

  else if (storage->mode == LONG_LIVED_STABLE
	   && storage->free > FREEING_THRESHOLD)
    /* The folio is stable and is now less than half allocated.
       Change the folio's state to freeing.  */
    {
      list_unlink (storage);
      storage->mode = LONG_LIVED_FREEING;
      list_link (&long_lived_freeing, storage);
    }

  ss_mutex_unlock (&storage_descs_lock);

  int idx = vg_addr_extract (object, VG_FOLIO_OBJECTS_LOG2);
  bit_dealloc (storage->alloced, idx);

  if (unmap_now
#ifndef NDEBUG
      || 1
#endif
      )
    {
      error_t err = vg_folio_object_alloc (meta_data_activity,
					   folio, idx, vg_cap_void,
					   VG_OBJECT_POLICY_DEFAULT, 0,
					   NULL, NULL);
      assert (err == 0);
    }

  if (likely (!! shadow))
    {
      shadow->caps[idx].type = vg_cap_void;
      vg_cap_set_shadow (&shadow->caps[idx], NULL);
      VG_CAP_PROPERTIES_SET (&shadow->caps[idx],
			  VG_CAP_PROPERTIES (VG_OBJECT_POLICY_DEFAULT,
					  VG_CAP_ADDR_TRANS_VOID));
    }
  else
    assert (! as_init_done);

  assert (storage->owner == hurd_myself ());
  storage->owner = vg_niltid;
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

  int folio_count = 0;

  for (i = 0, odesc = &__hurd_startup_data->descs[0];
       i < __hurd_startup_data->desc_count;
       i ++, odesc ++)
    {
      if (VG_ADDR_IS_VOID (odesc->storage))
	continue;

      vg_addr_t folio;
      if (odesc->type == vg_cap_folio)
	folio = odesc->object;
      else
	folio = vg_addr_chop (odesc->storage, VG_FOLIO_OBJECTS_LOG2);

      struct storage_desc *sdesc;
      sdesc = hurd_btree_storage_desc_find (&storage_descs, &folio);
      if (! sdesc)
	/* Haven't seen this folio yet.  */
	{
	  debug (5, "Adding folio " VG_ADDR_FMT, VG_ADDR_PRINTF (folio));

	  folio_count ++;

	  sdesc = storage_desc_alloc ();
	  sdesc->lock = (ss_mutex_t) 0;
	  sdesc->owner = vg_niltid;
	  sdesc->folio = folio;
	  sdesc->free = VG_FOLIO_OBJECTS;
	  sdesc->mode = LONG_LIVED_ALLOCING;

	  list_link (&long_lived_allocing, sdesc);

	  hurd_btree_storage_desc_insert (&storage_descs, sdesc);

	  /* Assume that the folio is free.  As we encounter objects,
	     we will mark them as allocated.  */
	  free_count += VG_FOLIO_OBJECTS;
	}

      if (odesc->type != vg_cap_folio)
	{
	  int idx = vg_addr_extract (odesc->storage, VG_FOLIO_OBJECTS_LOG2);

	  debug (5, "%llx/%d, %d -> %llx/%d (%s)",
		 vg_addr_prefix (folio),
		 vg_addr_depth (folio),
		 idx,
		 vg_addr_prefix (odesc->storage),
		 vg_addr_depth (odesc->storage),
		 vg_cap_type_string (odesc->type));

	  bit_set (sdesc->alloced, sizeof (sdesc->alloced), idx);

	  sdesc->free --;
	  free_count --;

	  if (sdesc->free == 0)
	    list_unlink (sdesc);
	}
    }

  ss_mutex_unlock (&storage_descs_lock);

  debug (1, "%d folios, %d objects used, %d free objects",
	 folio_count, __hurd_startup_data->desc_count, (int) free_count);

  storage_init_done = true;

  /* XXX: We can only call this after initialization is complete.
     This presents a problem: we might require additional storage
     descriptors than a single pre-allocated page provides.  This is
     quite unlikely but something we really need to deal with.  */
  check_slab_space_reserve ();
}
