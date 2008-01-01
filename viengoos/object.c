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
#include <bit-array.h>

#include "object.h"
#include "activity.h"
#include "thread.h"

struct object_desc *object_descs;

ss_mutex_t lru_lock;

struct object_global_lru_list global_active;
struct object_global_lru_list global_inactive_dirty;
struct object_global_lru_list global_inactive_clean;
struct object_activity_lru_list disowned;

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

  hurd_ihash_init (&objects, (int) (&((struct object_desc *)0)->locp));

  /* Allocate enough object descriptors for the number of pages.  */
  object_descs = calloc (sizeof (struct object_desc),
			 ((last_frame - first_frame) / PAGESIZE + 1));
  if (! object_descs)
    panic ("Failed to allocate object descriptor array!\n");
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

  assert (type != cap_void);
  assert ((type == cap_folio) == ((oid % (FOLIO_OBJECTS + 1)) == 0));

  struct object *object = (struct object *) memory_frame_allocate ();
  if (! object)
    {
      /* XXX: Do some garbage collection.  */

      return NULL;
    }

  /* Fill in the object descriptor.  */

  struct object_desc *odesc = object_to_object_desc (object);
  odesc->type = type;
  odesc->version = version;
  odesc->oid = oid;

  odesc->dirty = 0;
  odesc->lock = l4_nilthread;

  assert (! odesc->activity);

  /* Add to OBJECTS.  */
  bool had_value;
  hurd_ihash_value_t old_value;
  error_t err = hurd_ihash_replace (&objects, odesc->oid, odesc,
				    &had_value, &old_value);
  assert (err == 0);
  assert (! had_value);

  /* Connect to object lists.  */

  /* Give it a nominal age so that it is not immediately paged out.
     Normally, the page will be immediately referenced.  */
  odesc->age = 1;

  ss_mutex_lock (&lru_lock);

  odesc->global_lru.next = odesc->global_lru.prev = NULL;
  odesc->activity_lru.next = odesc->activity_lru.prev = NULL;

  object_global_lru_list_push (&global_active, odesc);
  /* object_desc_claim wants to first unlink the descriptor.  To make
     it happy, we initially connect the descriptor to the disowned
     list.  */
  object_activity_lru_list_push (&disowned, odesc);

  if (! activity)
    /* This may only happen if we are initializing.  */
    assert (! root_activity);
  else
    /* Account the memory to the activity ACTIVITY.  */
    object_desc_claim (activity, odesc, policy);

  ss_mutex_unlock (&lru_lock);

  return object;
}

/* Release the object.  */
static void
memory_object_destroy (struct activity *activity, struct object *object)
{
  assert (activity);

  struct object_desc *desc = object_to_object_desc (object);

  debug (5, "Destroy %s at 0x%llx (object %d)",
	 cap_type_string (desc->type), desc->oid,
	 ((uintptr_t) desc - (uintptr_t) object_descs)
	 / sizeof (*desc));

  struct cap cap = object_desc_to_cap (desc);
  cap_shootdown (activity, &cap);

  ss_mutex_lock (&lru_lock);
  object_desc_disown (desc);

  struct object_global_lru_list *global;
  if (desc->age)
    /* DESC is active.  */
    global = &global_active;
  else
    /* DESC is inactive.  */
    if (desc->dirty && ! desc->policy.discardable)
      /* And dirty.  */
      global = &global_inactive_dirty;
    else
      /* And clean.  */
      global = &global_inactive_clean;

  object_activity_lru_list_unlink (&disowned, desc);
  object_global_lru_list_unlink (global, desc);

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

  /* Return the frame to the free pool.  */
  memory_frame_free ((l4_word_t) object);
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

  if (! odesc->activity || odesc->age == 0)
    /* Either the object is unowned or it is inactive.  Claim
       ownership.  */
    {
      ss_mutex_lock (&lru_lock);
      object_desc_claim (activity, odesc, policy);
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

      if (folio->objects[page].type == cap_void)
	return NULL;

      if (! folio->objects[page].content)
	/* The object is a zero page.  No need to read anything from
	   backing store: just allocate a page and zero it.  */
	return memory_object_alloc (activity, folio->objects[page].type,
				    oid, folio->objects[page].version,
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
      activity_for_each_ancestor (a,
				  ({
				    if (a->storage_quota
					&& a->folio_count >= a->storage_quota)
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
					b->folio_count ++;
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

  /* Disown the frame.  */
  owner = fdesc->activity;

  ss_mutex_lock (&lru_lock);
  object_disown ((struct object *) folio);
  ss_mutex_unlock (&lru_lock);

  /* And free the folio.  */
  folio->folio_version = fdesc->version ++;
  bit_dealloc (folios, fdesc->oid / (FOLIO_OBJECTS + 1));
}

void
folio_object_alloc (struct activity *activity,
		    struct folio *folio,
		    int idx,
		    enum cap_type type,
		    struct object_policy policy,
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

  if (folio->objects[idx].type == cap_activity_control
      || folio->objects[idx].type == cap_thread)
    /* These object types have state that needs to be explicitly
       destroyed.  */
    {
      object = object_find (activity, oid, OBJECT_POLICY_DEFAULT);

      /* See if we need to destroy the object.  */
      switch (folio->objects[idx].type)
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

  if (! object)
    object = object_find_soft (activity, oid, policy);
  if (object)
    /* The object is in memory.  Update its descriptor and revoke any
       references to the old object.  */
    {
      struct object_desc *odesc = object_to_object_desc (object);
      assert (odesc->oid == oid);
      assert (odesc->type == folio->objects[idx].type);

      if (type == cap_void)
	/* We are deallocating the object: free associated memory.  */
	{
	  memory_object_destroy (activity, object);
	  object = NULL;
	}
      else
	{
	  struct cap cap = object_desc_to_cap (odesc);
	  assert (activity);
	  cap_shootdown (activity, &cap);

	  ss_mutex_lock (&lru_lock);
	  object_desc_claim (activity, odesc, policy);
	  ss_mutex_unlock (&lru_lock);
	}

      odesc->type = type;
      odesc->version = folio->objects[idx].version;
    }

  if (folio->objects[idx].type != cap_void)
    /* We know that if an object's type is void then there are no
       extant pointers to it.  If there are only pointers in memory,
       then we need to bump the memory version.  Otherwise, we need to
       bump the disk version.  */
    {
      /* XXX: Check if we can just bump the in-memory version.  */

      /* Bump the disk version.  */
      folio->objects[idx].version ++;
    }

  /* Set the object's new type.  */
  folio->objects[idx].type = type;
  /* Mark it as being empty.  */
  folio->objects[idx].content = 0;

  folio->objects[idx].policy = policy;

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
object_desc_disown_simple (struct object_desc *desc)
{
  assert (! ss_mutex_trylock (&lru_lock));
  assert (desc->activity);

  struct object_activity_lru_list *list;
  if (desc->age)
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

  object_activity_lru_list_unlink (list, desc);
  object_activity_lru_list_push (&disowned, desc);

  if (desc->policy.priority != OBJECT_PRIORITY_LRU)
    hurd_btree_priorities_detach (&desc->activity->priorities, desc);

  desc->activity = NULL;
}

void
object_desc_disown_ (struct object_desc *desc)
{
  activity_charge (desc->activity, -1);
  object_desc_disown_simple (desc);
}

void
object_desc_claim_ (struct activity *activity, struct object_desc *desc,
		    struct object_policy policy)
{
  assert (activity);

  if (desc->activity == activity)
    /* Same owner: update the policy.  */
    {
      desc->policy.discardable = policy.discardable;

      if (desc->policy.priority == policy.priority)
	/* The priority didn't change; don't do any unnecessary work.  */
	return;

      if (desc->policy.priority != OBJECT_PRIORITY_LRU)
	hurd_btree_priorities_detach (&desc->activity->priorities, desc);

      desc->policy.priority = policy.priority;

      if (desc->policy.priority != OBJECT_PRIORITY_LRU)
	hurd_btree_priorities_insert (&desc->activity->priorities, desc);

      return;
    }

  if (desc->activity)
    /* Already claimed by another activity; first disown it.  */
    object_desc_disown (desc);

  /* DESC->ACTIVITY is NULL so DESC must be on DISOWNED.  */
  object_activity_lru_list_unlink (&disowned, desc);
  object_activity_lru_list_push (&activity->active, desc);
  desc->activity = activity;
  activity_charge (activity, 1);

  desc->policy.discardable = policy.discardable;
  desc->policy.priority = policy.priority;
  if (policy.priority != OBJECT_PRIORITY_LRU)
    /* Add to ACTIVITY's priority queue.  */
    {
      void *ret = hurd_btree_priorities_insert (&activity->priorities, desc);
      assert (! ret);
    }
}
