/* object.c - Object store management.
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

#include <l4.h>
#include <string.h>
#include <hurd/stddef.h>
#include <hurd/ihash.h>
#include <hurd/folio.h>
#include <hurd/thread.h>
#include <bit-array.h>

#include "object.h"
#include "activity.h"
#include "thread.h"
#include "zalloc.h"

struct object_desc *object_descs;

ss_mutex_t lru_lock;

struct laundry_list laundry;
struct available_list available;

/* XXX: The number of in memory folios.  (Recall: one folio => 512kb
   storage.)  */
#define FOLIOS_CORE 256
static unsigned char folios[FOLIOS_CORE / 8];

/* Given an OID, we need a way to find 1) whether the object is
   memory, and 2) if so, where.  We achieve this using a hash.  The
   hash maps object OIDs to struct object_desc *s.  */
/* XXX: Although the current implementation of the hash function
   dynamically allocates memory according to demand, the maximum
   amount of required memory can be calculated at startup.  */
/* XXX: A hash is key'd by a machine word, however, an oid is
   64-bits.  */
/* XXX: When dereferencing a capability slot, we look up the object
   using the hash and then check that the version number stored in the
   capability slot matchs that in the object.  This likely incurs a
   cache-line miss to read the version from the object descriptor.  We
   can elide this by hashing from the concatenation of the OID and the
   version number but see the last point for why this is
   problematic.  */
static struct hurd_ihash objects;

void
object_init (void)
{
  assertx (sizeof (struct folio) <= PAGESIZE, "%d", sizeof (struct folio));
  assertx (sizeof (struct activity) <= PAGESIZE,
	   "%d", sizeof (struct activity));
  assertx (sizeof (struct object) <= PAGESIZE, "%d", sizeof (struct object));
  assertx (sizeof (struct thread) <= PAGESIZE, "%d", sizeof (struct thread));


  /* Allocate object hash.  */
  int count = (last_frame - first_frame) / PAGESIZE + 1;

  size_t size = hurd_ihash_buffer_size (count, 0);
  /* Round up to a multiple of the page size.  */
  size = (size + PAGESIZE - 1) & ~(PAGESIZE - 1);

  void *buffer = (void *) zalloc (size);
  if (! buffer)
    panic ("Failed to allocate memory for object hash!\n");

  memset (buffer, 0, size);

  hurd_ihash_init_with_buffer (&objects,
			       (int) (&((struct object_desc *)0)->locp),
			       buffer, size);


  /* Allocate object desc array: enough object descriptors for the
     number of pages.  */
  size = ((last_frame - first_frame) / PAGESIZE + 1)
    * sizeof (struct object_desc);
  /* Round up.  */
  size = (size + PAGESIZE - 1) & ~(PAGESIZE - 1);

  object_descs = (void *) zalloc (size);
  if (! object_descs)
    panic ("Failed to allocate memory for object descriptor array!\n");

  memset (object_descs, 0, size);
}

/* Allocate and set up a memory object.  TYPE, OID and VERSION must
   correspond to the values storage on disk.  */
static struct object *
memory_object_alloc (struct activity *activity,
		     enum cap_type type,
		     oid_t oid, l4_word_t version,
		     struct object_policy policy)
{
  debug (5, "Allocating %llx(%d), %s", oid, version, cap_type_string (type));

  assert (activity || ! root_activity);
  assert (type != cap_void);
  assert ((type == cap_folio) == ((oid % (FOLIO_OBJECTS + 1)) == 0));

  struct object *object = (struct object *) memory_frame_allocate (activity);
  if (! object)
    {
      /* XXX: Do some garbage collection.  */

      return NULL;
    }

  /* Fill in the object descriptor.  */

  struct object_desc *odesc = object_to_object_desc (object);
  assert (! odesc->live);
  memset (odesc, 0, sizeof (*odesc));

  odesc->type = type;
  odesc->version = version;
  odesc->oid = oid;

  /* Add to OBJECTS.  */
  bool had_value;
  hurd_ihash_value_t old_value;
  error_t err = hurd_ihash_replace (&objects, odesc->oid, odesc,
				    &had_value, &old_value);
  assert (err == 0);
  assert (! had_value);

  /* Mark the object as live.  */
  odesc->live = 1;

  if (! activity)
    /* This may only happen if we are initializing.  */
    assert (! root_activity);
  else
    {
      ss_mutex_lock (&lru_lock);

      /* Account the memory to the activity ACTIVITY.  */
      object_desc_claim (activity, odesc, policy, true);

      ss_mutex_unlock (&lru_lock);
    }

  return object;
}

void
memory_object_destroy (struct activity *activity, struct object *object)
{
  assert (activity);
  assert (! ss_mutex_trylock (&lru_lock));

  struct object_desc *desc = object_to_object_desc (object);

  assert (desc->live);

  debug (5, "Destroy %s at 0x%llx (object %d)",
	 cap_type_string (desc->type), desc->oid,
	 ((uintptr_t) desc - (uintptr_t) object_descs)
	 / sizeof (*desc));

  if (desc->dirty && desc->policy.discardable)
    /* Note that the page was discarded.  */
    /* XXX: This doesn't really belong here.  */
    {
      struct folio *folio = objects_folio (activity, object);
      folio_object_content_set (folio, objects_folio_offset (object), false);
    }

  struct cap cap = object_desc_to_cap (desc);
  cap_shootdown (activity, &cap);

  object_desc_claim (NULL, desc, desc->policy, true);

  if (desc->type == cap_activity_control)
    {
      struct activity *a = (struct activity *) object;
      if (a->frames_total)
	panic ("Attempt to page-out activity with allocated frames");
    }

  hurd_ihash_locp_remove (&objects, desc->locp);
  assert (! hurd_ihash_find (&objects, desc->oid));

#ifdef NDEBUG
  memset (desc, 0xde, sizeof (struct object_desc));
#endif

  desc->live = 0;
}

struct object *
object_find_soft (struct activity *activity, oid_t oid,
		  struct object_policy policy)
{
  struct object_desc *odesc = hurd_ihash_find (&objects, oid);
  if (! odesc)
    return NULL;

  struct object *object = object_desc_to_object (odesc);
  assert (oid == odesc->oid);

  if (! activity)
    {
      assert (! root_activity);
      return object;
    }

  if (! odesc->activity || ! object_active (odesc))
    /* Either the object is unowned or it is inactive.  Claim
       ownership.  */
    {
      ss_mutex_lock (&lru_lock);
      object_desc_claim (activity, odesc, policy, true);
      ss_mutex_unlock (&lru_lock);
    }

  return object;
}

struct object *
object_find (struct activity *activity, oid_t oid,
	     struct object_policy policy)
{
  struct object *obj = object_find_soft (activity, oid, policy);
  if (obj)
    return obj;

  struct folio *folio;

  int page = (oid % (FOLIO_OBJECTS + 1)) - 1;
  if (page == -1)
    /* The object to find is a folio.  */
    {
      if (oid / (FOLIO_OBJECTS + 1) < FOLIOS_CORE)
	/* It's an in-core folio.  */
	{
	  assert (bit_test (folios, oid / (FOLIO_OBJECTS + 1)));

	  return memory_object_alloc (activity, cap_folio, oid, 0,
				      policy);
	}

      /* It's not an in-memory folio.  We read it from disk below.  */
    }
  else
    {
      /* Find the folio corresponding to the object.  */
      folio = (struct folio *) object_find (activity, oid - page - 1,
					    OBJECT_POLICY_DEFAULT);
      assert (folio);

      if (folio_object_type (folio, page) == cap_void)
	return NULL;

      if (! folio_object_content (folio, page))
	/* The object is a zero page.  No need to read anything from
	   backing store: just allocate a page and zero it.  */
	return memory_object_alloc (activity, folio_object_type (folio, page),
				    oid, folio_object_version (folio, page),
				    policy);
    }
  
  /* Read the object from backing store.  */

  /* XXX: Do it.  */
  return NULL;
}

void
folio_parent (struct activity *activity, struct folio *folio)
{
  /* Some sanity checks.  */
  assert (({
	struct object_desc *desc;
	desc = object_to_object_desc ((struct object *) folio);
	assert (desc->oid % (FOLIO_OBJECTS + 1) == 0);
	true;
      }));
  assert (! cap_to_object (activity, &folio->activity));
  assert (! cap_to_object (activity, &folio->next));
  assert (! cap_to_object (activity, &folio->prev));
  assert (({
	struct object_desc *desc;
	desc = object_to_object_desc ((struct object *) folio);
	if (desc->oid != 0)
	  /* Only the very first folio may have objects allocated out
	     of it before it is parented.  */
	  {
	    int i;
	    for (i = 0; i < FOLIO_OBJECTS; i ++)
	      assert (! object_find_soft (activity, desc->oid + 1 + i,
					  OBJECT_POLICY_DEFAULT));
	  }
	true;
      }));

  /* Record the owner.  */
  folio->activity = object_to_cap ((struct object *) activity);

  /* Add FOLIO to ACTIVITY's folio list.  */

  /* Update the old head's previous pointer.  */
  struct object *head = cap_to_object (activity, &activity->folios);
  if (head)
    {
      /* It shouldn't have a previous pointer.  */
      struct object *prev = cap_to_object (activity,
					   &((struct folio *) head)->prev);
      assert (! prev);

      ((struct folio *) head)->prev = object_to_cap ((struct object *) folio);
    }

  /* Point FOLIO->NEXT to the old head.  */
  folio->next = activity->folios;

  /* Ensure FOLIO's PREV pointer is void.  */
  folio->prev.type = cap_void;

  /* Finally, set ACTIVITY->FOLIOS to the new head.  */
  activity->folios = object_to_cap ((struct object *) folio);
  assert (cap_to_object (activity, &activity->folios)
	  == (struct object *) folio);
}

struct folio *
folio_alloc (struct activity *activity, struct folio_policy policy)
{
  if (! activity)
    assert (! root_activity);
  else
    /* Check that ACTIVITY won't exceed its quota.  */
    {
      struct activity *a = activity;
      activity_for_each_ancestor
	(a,
	 ({
	   if (a->policy.folios
	       && a->folio_count >= a->policy.folios)
	     break;

	   a->folio_count ++;
	 }));

      if (a)
	/* Exceeded A's quota.  Readjust the folio count of the
	   activities from ACTIVITY to A and bail.  */
	{
	  struct activity *b = activity;
	  activity_for_each_ancestor (b,
				      ({
					if (b == a)
					  break;
					b->folio_count --;
				      }));

	  return NULL;
	}
    }

  /* XXX: We only do in-memory folios right now.  */
  int f = bit_alloc (folios, sizeof (folios), 0);
  if (f < 0)
    panic ("Out of folios");
  oid_t foid = f * (FOLIO_OBJECTS + 1);

  /* We can't just allocate a fresh page as we need to preserve the
     version information for the folio as well as the objects.  */
  struct folio *folio = (struct folio *) object_find (activity, foid,
						      OBJECT_POLICY_DEFAULT);

  if (activity)
    folio_parent (activity, folio);

  folio->policy = policy;

  return folio;
}

void
folio_free (struct activity *activity, struct folio *folio)
{
  /* Make sure that FOLIO appears on its owner's folio list.  */
  assert (({
	struct activity *owner
	  = (struct activity *) cap_to_object (activity, &folio->activity);
	assert (owner);
	assert (object_type ((struct object *) owner) == cap_activity_control);
	struct folio *f;
	for (f = (struct folio *) cap_to_object (activity, &owner->folios);
	     f; f = (struct folio *) cap_to_object (activity, &f->next))
	  {
	    assert (object_type ((struct object *) folio) == cap_folio);
	    if (f == folio)
	      break;
	  }
	assert (f);
	true;
      }));

  /* NB: The activity freeing FOLIO may not be the one who paid for
     the storage for it.  We use it as the entity who should pay for
     the paging activity, etc.  */

  struct object_desc *fdesc = object_to_object_desc ((struct object *) folio);
  assert (fdesc->type == cap_folio);
  assert (fdesc->oid % (FOLIO_OBJECTS + 1) == 0);

  /* Free the objects.  This bumps the version of any live objects.
     This is correct as although the folio is being destroyed, when we
     lookup an object via a capability, we only check that the
     capability's version matches the object's version (we do not
     check whether the folio is valid).  */
  /* As we free the objects, we also don't have to call cap_shootdown
     here.  */
  int i;
  for (i = 0; i < FOLIO_OBJECTS; i ++)
    folio_object_free (activity, folio, i);

  struct activity *owner
    = (struct activity *) cap_to_object (activity, &folio->activity);
  assert (owner);

  /* Update the allocation information.  */
  struct activity *a = owner;
  activity_for_each_ancestor (a, ({ a->folio_count --; }));

  /* Clear the owner.  */
  folio->activity.type = cap_void;

  /* Remove FOLIO from its owner's folio list.  */
  struct folio *next = (struct folio *) cap_to_object (activity, &folio->next);
  struct folio *prev = (struct folio *) cap_to_object (activity, &folio->prev);

  if (prev)
    prev->next = folio->next;
  else
    /* If there is no previous pointer, then FOLIO is the start of the
       list and we need to update the head.  */
    owner->folios = folio->next;

  if (next)
    next->prev = folio->prev;

  folio->next.type = cap_void;
  folio->prev.type = cap_void;

  /* And free the folio.  */

  /* XXX: We need to now schedule the folio for page-out: it contains
     previous data including version information.  */
  fdesc->version = folio_object_version (folio, -1) + 1;
  folio_object_version_set (folio, -1, fdesc->version);
  bit_dealloc (folios, fdesc->oid / (FOLIO_OBJECTS + 1));
}

void
folio_object_alloc (struct activity *activity,
		    struct folio *folio,
		    int idx,
		    enum cap_type type,
		    struct object_policy policy,
		    uintptr_t return_code,
		    struct object **objectp)
{
  debug (4, "allocating %s at %d", cap_type_string (type), idx);

  assert (0 <= idx && idx < FOLIO_OBJECTS);

  struct object_desc *fdesc = object_to_object_desc ((struct object *) folio);
  assert (fdesc->type == cap_folio);
  assert (fdesc->oid % (1 + FOLIO_OBJECTS) == 0);

  oid_t oid = fdesc->oid + 1 + idx;

  struct object *object = NULL;

  /* Deallocate any existing object.  */

  if (folio_object_type (folio, idx) == cap_activity_control
      || folio_object_type (folio, idx) == cap_thread)
    /* These object types have state that needs to be explicitly
       destroyed.  */
    {
      object = object_find (activity, oid, OBJECT_POLICY_DEFAULT);

      /* See if we need to destroy the object.  */
      switch (folio_object_type (folio, idx))
	{
	case cap_activity_control:
	  debug (4, "Destroying activity at %llx", oid);
	  activity_destroy (activity, (struct activity *) object);
	  break;
	case cap_thread:
	  debug (4, "Destroying thread object at %llx", oid);
	  thread_deinit (activity, (struct thread *) object);
	  break;
	default:
	  assert (!"Object desc type does not match folio type.");
	  break;
	}
    }

  /* Wake any thread's waiting on this object.  We wake them even if
     they are not waiting for this object's death.  */
  struct thread *thread;
  folio_object_wait_queue_for_each (activity, folio, idx, thread)
    {
      object_wait_queue_dequeue (activity, thread);
      if (thread->wait_reason == THREAD_WAIT_DESTROY)
	rm_thread_wait_object_destroyed_reply (thread->tid, return_code);
      else
	rpc_error_reply (thread->tid, EFAULT);
    }

  struct object_desc *odesc;

  if (! object)
    object = object_find_soft (activity, oid, policy);
  if (object)
    /* The object is in memory.  Update its descriptor and revoke any
       references to the old object.  */
    {
      odesc = object_to_object_desc (object);
      assert (odesc->oid == oid);
      assert (odesc->type == folio_object_type (folio, idx));

      if (type == cap_void)
	/* We are deallocating the object: free associated memory.  */
	{
	  ss_mutex_lock (&lru_lock);
	  memory_object_destroy (activity, object);
	  ss_mutex_unlock (&lru_lock);

	  /* Return the frame to the free pool.  */
	  memory_frame_free ((l4_word_t) object);

	  object = NULL;
	}
      else
	{
	  struct cap cap = object_desc_to_cap (odesc);
	  assert (activity);
	  cap_shootdown (activity, &cap);

	  ss_mutex_lock (&lru_lock);
	  object_desc_claim (activity, odesc, policy, true);
	  ss_mutex_unlock (&lru_lock);

	  odesc->type = type;
	}
    }

  if (folio_object_type (folio, idx) != cap_void)
    /* We know that if an object's type is void then there are no
       extant pointers to it.  If there are only pointers in memory,
       then we need to bump the memory version.  Otherwise, we need to
       bump the disk version.  */
    {
      /* XXX: Check if we can just bump the in-memory version.  */

      /* Bump the disk version.  */
      folio_object_version_set (folio, idx,
				folio_object_version (folio, idx) + 1);
      if (object)
	odesc->version = folio_object_version (folio, idx);
    }

  folio_object_type_set (folio, idx, type);
  folio_object_content_set (folio, idx, false);
  folio_object_policy_set (folio, idx, policy);

  switch (type)
    {
    case cap_activity_control:
      {
	if (! object)
	  object = object_find (activity, oid, policy);

	activity_create (activity, (struct activity *) object);
	break;
      }

    default:
      ;
    }

  if (objectp)
    /* Caller wants to use the object.  */
    {
      assert (type != cap_void);

      if (! object)
	object = object_find (activity, oid, policy);
      *objectp = object;
    }
}

void
folio_policy (struct activity *activity,
	      struct folio *folio,
	      uintptr_t flags, struct folio_policy in,
	      struct folio_policy *out)
{
  if ((flags & FOLIO_POLICY_DELIVER) && out)
    {
      out->discardable = folio->policy.discardable;
      out->group = folio->policy.group;
      out->priority = folio->policy.priority;
    }

  if (! (flags & FOLIO_POLICY_SET))
    return;

  if ((flags & FOLIO_POLICY_GROUP_SET))
    folio->policy.group = in.group;

  if ((flags & FOLIO_POLICY_DISCARDABLE_SET)
      && in.discardable != folio->policy.discardable)
    /* XXX: We need to move the folio from the discardable list to the
       precious list (or vice versa).  */
    folio->policy.discardable = in.discardable;

  if ((flags & FOLIO_POLICY_PRIORITY_SET))
    folio->policy.priority = in.priority;
}

void
object_desc_claim (struct activity *activity, struct object_desc *desc,
		   struct object_policy policy, bool update_accounting)
{
  assert (desc->activity || activity);

  if (desc->activity == activity
      && ! desc->eviction_candidate
      && desc->policy.priority == policy.priority)
    /* The owner remains the same, the object is not an eviction
       candidate and the priority didn't change; don't do any
       unnecessary work.  */
    {
      desc->policy.discardable = policy.discardable;
      return;
    }


  /* We need to disconnect DESC from its old activity.  If DESC does
     not have an activity, it being initialized.  */
  if (desc->activity)
    {
      assert (object_type ((struct object *) desc->activity)
	      == cap_activity_control);

      if (desc->eviction_candidate)
	/* DESC is an eviction candidate.  The act of claiming saves
	   it.  */
	{
	  if (desc->dirty && ! desc->policy.discardable)
	    {
	      laundry_list_unlink (&laundry, desc);
	      eviction_list_unlink (&desc->activity->eviction_dirty, desc);
	    }
	  else
	    {
	      available_list_unlink (&available, desc);
	      eviction_list_unlink (&desc->activity->eviction_clean, desc);
	    }
	}
      else
	{
	  if (desc->policy.priority != OBJECT_PRIORITY_LRU)
	    hurd_btree_priorities_detach (&desc->activity->priorities, desc);
	  else
	    {
	      struct activity_lru_list *list;
	      if (object_active (desc))
		/* DESC is active.  */
		list = &desc->activity->active;
	      else
		/* DESC is inactive.  */
		if (desc->dirty && ! desc->policy.discardable)
		  /* And dirty.  */
		  list = &desc->activity->inactive_dirty;
		else
		  /* And clean.  */
		  list = &desc->activity->inactive_clean;

	      activity_lru_list_unlink (list, desc);
	    }

	  if (activity != desc->activity && update_accounting)
	    activity_charge (desc->activity, -1);
	}
    }

  if (! activity)
    return;

  desc->policy.priority = policy.priority;

  /* Assign to ACTIVITY.  */

  /* We make the object active.  The invariants require that DESC->AGE
     be non-zero.  */
  object_age (desc, true);
  if (desc->policy.priority != OBJECT_PRIORITY_LRU)
    {
      void *ret = hurd_btree_priorities_insert (&activity->priorities,
						desc);
      assert (! ret);
    }
  else
    activity_lru_list_push (&activity->active, desc);

  if ((desc->eviction_candidate || activity != desc->activity)
      && update_accounting)
    activity_charge (activity, 1);
	
  desc->eviction_candidate = false;
  desc->activity = activity;
  desc->policy.discardable = policy.discardable;
}

/* Return the first waiter queued on object OBJECT.  */
struct thread *
object_wait_queue_head (struct activity *activity, struct object *object)
{
  struct folio *folio = objects_folio (activity, object);
  int i = objects_folio_offset (object);

  if (! folio_object_wait_queue_p (folio, i))
    return NULL;

  oid_t h = folio_object_wait_queue (folio, i);
  struct object *head = object_find (activity, h, OBJECT_POLICY_DEFAULT);
  assert (head);
  assert (object_type (head) == cap_thread);
  assert (((struct thread *) head)->wait_queue_p);
  assert (((struct thread *) head)->wait_queue_head);

  return (struct thread *) head;
}

/* Return the last waiter queued on object OBJECT.  */
struct thread *
object_wait_queue_tail (struct activity *activity, struct object *object)
{
  struct thread *head = object_wait_queue_head (activity, object);
  if (! head)
    return NULL;

  if (head->wait_queue_tail)
    /* HEAD is also the list's tail.  */
    return head;

  struct thread *tail;
  tail = (struct thread *) object_find (activity, head->wait_queue.prev,
					OBJECT_POLICY_DEFAULT);
  assert (tail);
  assert (object_type ((struct object *) tail) == cap_thread);
  assert (tail->wait_queue_p);
  assert (tail->wait_queue_tail);

  return tail;
}

/* Return the waiter following THREAD.  */
struct thread *
object_wait_queue_next (struct activity *activity, struct thread *t)
{
  if (t->wait_queue_tail)
    return NULL;

  struct thread *next;
  next = (struct thread *) object_find (activity, t->wait_queue.next,
					OBJECT_POLICY_DEFAULT);
  assert (next);
  assert (object_type ((struct object *) next) == cap_thread);
  assert (next->wait_queue_p);
  assert (! next->wait_queue_head);

  return next;
}

/* Return the waiter preceding THREAD.  */
struct thread *
object_wait_queue_prev (struct activity *activity, struct thread *t)
{
  if (t->wait_queue_head)
    return NULL;

  struct thread *prev;
  prev = (struct thread *) object_find (activity, t->wait_queue.prev,
					OBJECT_POLICY_DEFAULT);
  assert (prev);
  assert (object_type ((struct object *) prev) == cap_thread);
  assert (prev->wait_queue_p);
  assert (! prev->wait_queue_tail);

  return prev;
}

static void
object_wait_queue_check (struct activity *activity, struct thread *thread)
{
#ifndef NDEBUG
  if (! thread->wait_queue_p)
    return;

  struct thread *last = thread;
  struct thread *t;
  for (;;)
    {
      if (last->wait_queue_tail)
	break;

      t = (struct thread *) object_find (activity, last->wait_queue.next,
					 OBJECT_POLICY_DEFAULT);
      assert (t);
      assert (t->wait_queue_p);
      assert (! t->wait_queue_head);
      struct object *p = object_find (activity, t->wait_queue.prev,
					OBJECT_POLICY_DEFAULT);
      assert (p == (struct object *) last);
      
      last = t;
    }

  assert (last->wait_queue_tail);

  struct object *o = object_find (activity, last->wait_queue.next,
				  OBJECT_POLICY_DEFAULT);
  assert (o);
  assert (folio_object_wait_queue_p (objects_folio (activity, o),
				     objects_folio_offset (o)));

  struct thread *head = object_wait_queue_head (activity, o);
  if (! head)
    return;
  assert (head->wait_queue_head);

  struct thread *tail;
  tail = (struct thread *) object_find (activity, head->wait_queue.prev,
					OBJECT_POLICY_DEFAULT);
  assert (tail);
  assert (tail->wait_queue_tail);

  assert (last == tail);

  last = head;
  while (last != thread)
    {
      assert (! last->wait_queue_tail);

      t = (struct thread *) object_find (activity, last->wait_queue.next,
					 OBJECT_POLICY_DEFAULT);
      assert (t);
      assert (t->wait_queue_p);
      assert (! t->wait_queue_head);

      struct object *p = object_find (activity, t->wait_queue.prev,
				      OBJECT_POLICY_DEFAULT);
      assert (p == (struct object *) last);
      
      last = t;
    }
#endif /* !NDEBUG */
}

/* Enqueue the thread THREAD on object OBJECT's wait queue.  */
void
object_wait_queue_enqueue (struct activity *activity,
			   struct object *object, struct thread *thread)
{
  debug (5, "Adding " OID_FMT " to %p",
	 OID_PRINTF (object_to_object_desc ((struct object *) thread)->oid),
	 object);

  object_wait_queue_check (activity, thread);

  assert (! thread->wait_queue_p);

  struct thread *oldhead = object_wait_queue_head (activity, object);
  if (oldhead)
    {
      assert (oldhead->wait_queue_head);

      /* THREAD->PREV = TAIL.  */
      thread->wait_queue.prev = oldhead->wait_queue.prev;

      /* OLDHEAD->PREV = THREAD.  */
      oldhead->wait_queue_head = 0;
      oldhead->wait_queue.prev = object_oid ((struct object *) thread);

      /* THREAD->NEXT = OLDHEAD.  */
      thread->wait_queue.next = object_oid ((struct object *) oldhead);

      thread->wait_queue_tail = 0;
    }
  else
    /* Empty list.  */
    {
      folio_object_wait_queue_p_set (objects_folio (activity, object),
				     objects_folio_offset (object),
				     true);

      /* THREAD->PREV = THREAD.  */
      thread->wait_queue.prev = object_oid ((struct object *) thread);

      /* THREAD->NEXT = OBJECT.  */
      thread->wait_queue_tail = 1;
      thread->wait_queue.next = object_oid (object);
    }

  thread->wait_queue_p = true;

  /* WAIT_QUEUE = THREAD.  */
  thread->wait_queue_head = 1;
  folio_object_wait_queue_set (objects_folio (activity, object),
			       objects_folio_offset (object),
			       object_oid ((struct object *) thread));

  object_wait_queue_check (activity, thread);
}

/* Dequeue thread THREAD from its wait queue.  */
void
object_wait_queue_dequeue (struct activity *activity, struct thread *thread)
{
  debug (5, "Removing " OID_FMT,
	 OID_PRINTF (object_to_object_desc ((struct object *) thread)->oid));

  assert (thread->wait_queue_p);

  object_wait_queue_check (activity, thread);

  if (thread->wait_queue_tail)
    /* THREAD is the tail.  THREAD->NEXT must be the object on which
       we are queued.  */
    {
      struct object *object;
      object = object_find (activity, thread->wait_queue.next,
			    OBJECT_POLICY_DEFAULT);
      assert (object);
      assert (folio_object_wait_queue_p (objects_folio (activity, object),
					 objects_folio_offset (object)));
      assert (object_wait_queue_tail (activity, object) == thread);

      if (thread->wait_queue_head)      
	/* THREAD is also the head and thus the only item on the
	   list.  */
	{
	  assert (object_find (activity, thread->wait_queue.prev,
			       OBJECT_POLICY_DEFAULT)
		  == (struct object *) thread);

	  folio_object_wait_queue_p_set (objects_folio (activity, object),
					 objects_folio_offset (object),
					 false);
	}
      else
	/* THREAD is not also the head.  */
	{
	  struct thread *head = object_wait_queue_head (activity, object);

	  /* HEAD->PREV == TAIL.  */
	  assert (object_find (activity, head->wait_queue.prev,
			       OBJECT_POLICY_DEFAULT)
		  == (struct object *) thread);

	  /* HEAD->PREV = TAIL->PREV.  */
	  head->wait_queue.prev = thread->wait_queue.prev;

	  /* TAIL->PREV->NEXT = OBJECT.  */
	  struct thread *prev;
	  prev = (struct thread *) object_find (activity,
						thread->wait_queue.prev,
						OBJECT_POLICY_DEFAULT);
	  assert (prev);
	  assert (object_type ((struct object *) prev) == cap_thread);

	  prev->wait_queue_tail = 1;
	  prev->wait_queue.next = thread->wait_queue.next;
	}
    }
  else
    /* THREAD is not the tail.  */
    {
      struct thread *next = object_wait_queue_next (activity, thread);
      assert (next);

      struct object *p = object_find (activity, thread->wait_queue.prev,
				      OBJECT_POLICY_DEFAULT);
      assert (p);
      assert (object_type (p) == cap_thread);
      struct thread *prev = (struct thread *) p;

      if (thread->wait_queue_head)
	/* THREAD is the head.  */
	{
	  /* THREAD->PREV is the tail, TAIL->NEXT the object.  */
	  struct thread *tail = prev;

	  struct object *object = object_find (activity, tail->wait_queue.next,
					       OBJECT_POLICY_DEFAULT);
	  assert (object);
	  assert (object_wait_queue_head (activity, object) == thread);


	  /* OBJECT->WAIT_QUEUE = THREAD->NEXT.  */
	  next->wait_queue_head = 1;

	  folio_object_wait_queue_set (objects_folio (activity, object),
				       objects_folio_offset (object),
				       thread->wait_queue.next);
	}
      else
	/* THREAD is neither the head nor the tail.  */
	{
	  /* THREAD->PREV->NEXT = THREAD->NEXT.  */
	  prev->wait_queue.next = thread->wait_queue.next;
	}

      /* THREAD->NEXT->PREV = THREAD->PREV.  */
      next->wait_queue.prev = thread->wait_queue.prev;
    }

  thread->wait_queue_p = false;

  object_wait_queue_check (activity, thread);
}
