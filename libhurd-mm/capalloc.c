/* capalloc.c - Capability allocation functions.
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

#include "capalloc.h"

#include "storage.h"
#include "as.h"

#include <viengoos/addr.h>
#include <hurd/btree.h>
#include <hurd/slab.h>
#include <hurd/stddef.h>

#include <bit-array.h>

#include <string.h>
#include <pthread.h>

struct cappage_desc
{
  addr_t cappage;
  struct cap *cap;

  unsigned char alloced[CAPPAGE_SLOTS / 8];
  unsigned short free;

  pthread_mutex_t lock;

  hurd_btree_node_t node;

  struct cappage_desc *next;
  struct cappage_desc **prevp;
};

static void
list_link (struct cappage_desc **list, struct cappage_desc *e)
{
  e->next = *list;
  if (e->next)
    e->next->prevp = &e->next;
  e->prevp = list;
  *list = e;
}

static void
list_unlink (struct cappage_desc *e)
{
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
	     addr_t, cappage, node, addr_compare, false)

static pthread_mutex_t cappage_descs_lock = PTHREAD_MUTEX_INITIALIZER;
static hurd_btree_cappage_desc_t cappage_descs;

/* Storage descriptors are alloced from a slab.  */
static error_t
cappage_desc_slab_alloc (void *hook, size_t size, void **ptr)
{
  assert (size == PAGESIZE);

  struct storage storage = storage_alloc (meta_data_activity,
					  cap_page, STORAGE_LONG_LIVED,
					  OBJECT_POLICY_DEFAULT, ADDR_VOID);
  if (ADDR_IS_VOID (storage.addr))
    panic ("Out of storage");
  *ptr = ADDR_TO_PTR (addr_extend (storage.addr, 0, PAGESIZE_LOG2));

  return 0;
}

static error_t
cappage_desc_slab_dealloc (void *hook, void *buffer, size_t size)
{
  assert (size == PAGESIZE);

  addr_t addr = addr_chop (PTR_TO_ADDR (buffer), PAGESIZE_LOG2);
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
  /* Find an appropriate storage area.  */
  struct cappage_desc *pluck (struct cappage_desc *list)
  {
    while (list)
      {
	/* We could just wait on the lock, however, we can just as
	   well allocate from another storage descriptor.  This may
	   lead to allocating additional storage areas, however, this
	   should be proportional to the contention.  */
	if (pthread_mutex_trylock (&list->lock) == 0)
	  return list;

	list = list->next;
      }

    return NULL;
  }

  pthread_mutex_lock (&cappage_descs_lock);
  struct cappage_desc *area = pluck (nonempty);
  pthread_mutex_unlock (&cappage_descs_lock);

  bool did_alloc = false;
  if (! area)
    {
      did_alloc = true;

      area = cappage_desc_alloc ();

      /* As there is such a large number of caps per cappage, we
	 expect that the page will be long lived.  */
      struct storage storage = storage_alloc (meta_data_activity,
					      cap_cappage, STORAGE_LONG_LIVED,
					      OBJECT_POLICY_DEFAULT, ADDR_VOID);
      if (ADDR_IS_VOID (storage.addr))
	{
	  cappage_desc_free (area);
	  return ADDR_VOID;
	}

      area->lock = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
      pthread_mutex_lock (&area->lock);

      area->cappage = storage.addr;
      area->cap = storage.cap;

      /* Then, allocate the shadow object.  */
      struct storage shadow_storage
	= storage_alloc (meta_data_activity, cap_page,
			 STORAGE_LONG_LIVED, OBJECT_POLICY_DEFAULT, ADDR_VOID);
      if (ADDR_IS_VOID (shadow_storage.addr))
	{
	  /* No memory.  */
	  storage_free (area->cappage, false);
	  cappage_desc_free (area);
	  return ADDR_VOID;
	}

      struct object *shadow = ADDR_TO_PTR (addr_extend (shadow_storage.addr,
							0, PAGESIZE_LOG2));
      memset (shadow, 0, PAGESIZE);
      cap_set_shadow (area->cap, shadow);

      memset (&area->alloced, 0, sizeof (area->alloced));
      area->free = CAPPAGE_SLOTS;
    }

  int idx = bit_alloc (area->alloced, sizeof (area->alloced), 0);
  assert (idx != -1);

  addr_t addr = addr_extend (area->cappage, idx, CAPPAGE_SLOTS_LOG2);

  area->free --;
  if (area->free == 0)
    {
      pthread_mutex_unlock (&area->lock);

      pthread_mutex_lock (&cappage_descs_lock);
      pthread_mutex_lock (&area->lock);
      if (area->free == 0)
	list_unlink (area);
      pthread_mutex_unlock (&area->lock);
      pthread_mutex_unlock (&cappage_descs_lock);
    }
  else
    pthread_mutex_unlock (&area->lock);

  if (did_alloc)
    /* Only add the cappage now.  This way, we only make it available
       once AREA->LOCK is no longer held.  */
    {
      pthread_mutex_lock (&cappage_descs_lock);

      list_link (&nonempty, area);
      hurd_btree_cappage_desc_insert (&cappage_descs, area);

      pthread_mutex_unlock (&cappage_descs_lock);
    }

  return addr;
}

void
capfree (addr_t cap)
{
  addr_t cappage = addr_chop (cap, CAPPAGE_SLOTS_LOG2);

  struct cappage_desc *desc;

  pthread_mutex_lock (&cappage_descs_lock);
  desc = hurd_btree_cappage_desc_find (&cappage_descs, &cappage);
  assert (desc);
  pthread_mutex_lock (&desc->lock);

  bit_dealloc (desc->alloced, addr_extract (cap, CAPPAGE_SLOTS_LOG2));
  desc->free ++;

  if (desc->free == 1)
    /* The cappage is no longer full.  Add it back to the list of
       nonempty cappages.  */
    list_link (&nonempty, desc);
  else if (desc->free == CAPPAGE_SLOTS)
    /* No slots in the cappage are allocated.  Free it if there is at
       least one cappage on NONEMPTY.  */
    {
      assert (nonempty);
      if (nonempty->next)
	{
	  hurd_btree_cappage_desc_detach (&cappage_descs, desc);
	  list_unlink (desc);
	  pthread_mutex_unlock (&cappage_descs_lock);

	  struct object *shadow = cap_get_shadow (desc->cap);
	  storage_free (addr_chop (PTR_TO_ADDR (shadow), PAGESIZE_LOG2),
			false);
	  cap_set_shadow (desc->cap, NULL);

	  desc->cap->type = cap_void;

	  cappage_desc_free (desc);

	  pthread_mutex_unlock (&cappage_descs_lock);

	  storage_free (cappage, false);

	  /* Already dropped the locks.  */
	  return;
	}
    }

  pthread_mutex_unlock (&desc->lock);
  pthread_mutex_unlock (&cappage_descs_lock);
}

