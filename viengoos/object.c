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
#include <viengoos/folio.h>
#include <viengoos/thread.h>
#include <viengoos/messenger.h>
#include <bit-array.h>
#include <assert.h>

#include "object.h"
#include "activity.h"
#include "thread.h"
#include "zalloc.h"
#include "messenger.h"

/* For lack of a better place.  */
ss_mutex_t kernel_lock;

struct object_desc *object_descs;

struct laundry_list laundry;
struct available_list available;

/* XXX: The number of in memory folios.  (Recall: one folio => 512kb
   storage.)  */
#define FOLIOS_CORE (4096 * 8)
static unsigned char folios[FOLIOS_CORE / 8];

/* Given an OID, we need a way to find 1) whether the object is
   memory, and 2) if so, where.  We achieve this using a hash.  The
   hash maps object OIDs to struct object_desc *s.  */
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
  build_assert (sizeof (struct vg_folio) <= PAGESIZE);
  build_assert (sizeof (struct activity) <= PAGESIZE);
  build_assert (sizeof (struct vg_object) <= PAGESIZE);
  build_assert (sizeof (struct thread) <= PAGESIZE);
  /* Assert that the size of a vg_cap is a power of 2.  */
  build_assert ((sizeof (struct vg_cap) & (sizeof (struct vg_cap) - 1)) == 0);


  /* Allocate object hash.  */
  int count = (last_frame - first_frame) / PAGESIZE + 1;

  /* XXX: Use a load factory of just 30% until we get a better hash
     implementation.  The default of 80% can result in very long
     chains.  */
  size_t size = hurd_ihash_buffer_size (count, true, 30);
  /* Round up to a multiple of the page size.  */
  size = (size + PAGESIZE - 1) & ~(PAGESIZE - 1);

  void *buffer = (void *) zalloc (size);
  if (! buffer)
    panic ("Failed to allocate memory for object hash!\n");

  hurd_ihash_init_with_buffer (&objects, true,
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
}

/* Allocate and set up a memory object.  TYPE, OID and VERSION must
   correspond to the values storage on disk.  */
static struct vg_object *
memory_object_alloc (struct activity *activity,
		     enum vg_cap_type type,
		     vg_oid_t oid, l4_word_t version,
		     struct vg_object_policy policy)
{
  debug (5, "Allocating %llx(%d), %s", oid, version, vg_cap_type_string (type));

  assert (activity || ! root_activity);
  assert (type != vg_cap_void);
  assert ((type == vg_cap_folio) == ((oid % (VG_FOLIO_OBJECTS + 1)) == 0));

  struct vg_object *object = (struct vg_object *) memory_frame_allocate (activity);
  if (! object)
    {
      /* XXX: Do some garbage collection.  */

      return NULL;
    }

  /* Fill in the object descriptor.  */

  struct object_desc *odesc = object_to_object_desc (object);
  assert (! odesc->live);
  memset (odesc, 0, sizeof (*odesc));

  /* Clear the status bits.  */
#ifndef _L4_TEST_ENVIRONMENT
  l4_flush (l4_fpage ((l4_word_t) object, PAGESIZE));
#endif

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
    /* Account the memory to the activity ACTIVITY.  */
    object_desc_claim (activity, odesc, policy, true);

  return object;
}

void
memory_object_destroy (struct activity *activity, struct vg_object *object)
{
  assert (activity);

  struct object_desc *desc = object_to_object_desc (object);

  assert (desc->live);

  assertx (vg_folio_object_type (objects_folio (activity, object),
				 objects_folio_offset (object)) == desc->type,
	   "(" VG_OID_FMT ") %s != %s",
	   VG_OID_PRINTF (desc->oid),
	   vg_cap_type_string
	   (vg_folio_object_type (objects_folio (activity, object),
				  objects_folio_offset (object))),
	   vg_cap_type_string (desc->type));

  debug (5, "Destroy %s at 0x%llx (object %d)",
	 vg_cap_type_string (desc->type), desc->oid,
	 ((uintptr_t) desc - (uintptr_t) object_descs) / sizeof (*desc));

  struct vg_cap vg_cap = object_desc_to_cap (desc);
  cap_shootdown (activity, &vg_cap);

  object_desc_claim (NULL, desc, desc->policy, true);

  if (desc->type == vg_cap_activity_control)
    {
      struct activity *a = (struct activity *) object;
      if (a->frames_total)
	panic ("Attempt to page-out activity with allocated frames");
    }

  desc->live = 0;

  hurd_ihash_locp_remove (&objects, desc->locp);
  assert (! hurd_ihash_find (&objects, desc->oid));

#ifndef NDEBUG
  memset (desc, 0xde, offsetof (struct object_desc, live));
  memset ((void *) desc + offsetof (struct object_desc, live) + 4, 0xde,
	  sizeof (struct object_desc)
	  - offsetof (struct object_desc, live) - 4);
#endif
}

struct vg_object *
object_find_soft (struct activity *activity, vg_oid_t oid,
		  struct vg_object_policy policy)
{
  struct object_desc *odesc = hurd_ihash_find (&objects, oid);
  if (! odesc)
    return NULL;

  struct vg_object *object = object_desc_to_object (odesc);
  assert (oid == odesc->oid);

  if (oid % (VG_FOLIO_OBJECTS + 1) != 0)
    {
#ifndef NDEBUG
      struct vg_folio *folio = objects_folio (activity, object);
      int i = objects_folio_offset (object);

      assertx (vg_folio_object_type (folio, i) == odesc->type,
	       "(" VG_OID_FMT ") %s != %s",
	       VG_OID_PRINTF (oid),
	       vg_cap_type_string (vg_folio_object_type (folio, i)),
	       vg_cap_type_string (odesc->type));
      assertx (! folio_object_discarded (folio, i),
	       VG_OID_FMT ": %s",
	       VG_OID_PRINTF (oid),
	       vg_cap_type_string (odesc->type));
#endif
    }

  if (! activity)
    {
      assert (! root_activity);
      return object;
    }

  if (! odesc->activity || ! object_active (odesc)
      || (odesc->activity != activity && odesc->floating)
      || odesc->eviction_candidate)
    /* Either the object is unclaimed, it is inactive, it is floating
       (claimed but looking for a new owner), or it is tagged for
       eviction.  Claim ownership.  */
    {
      object_desc_claim (activity, odesc, policy, true);
      odesc->floating = false;
    }
  else if (odesc->activity != activity)
    /* It's claimed by someone else, mark it as shared.  */
    odesc->shared = true;

  return object;
}

struct vg_object *
object_find (struct activity *activity, vg_oid_t oid,
	     struct vg_object_policy policy)
{
  struct vg_object *obj = object_find_soft (activity, oid, policy);
  if (obj)
    return obj;

  struct vg_folio *folio;

  int page = (oid % (VG_FOLIO_OBJECTS + 1)) - 1;
  if (page == -1)
    /* The object to find is a folio.  */
    {
      if (oid / (VG_FOLIO_OBJECTS + 1) < FOLIOS_CORE)
	/* It's an in-core folio.  */
	{
	  assert (bit_test (folios, oid / (VG_FOLIO_OBJECTS + 1)));

	  return memory_object_alloc (activity, vg_cap_folio, oid, 0,
				      policy);
	}

      /* It's not an in-memory folio.  We read it from disk below.  */
    }
  else
    {
      /* Find the folio corresponding to the object.  */
      folio = (struct vg_folio *) object_find (activity, oid - page - 1,
					    VG_OBJECT_POLICY_DEFAULT);
      assertx (folio,
	       "Didn't find folio " VG_OID_FMT,
	       VG_OID_PRINTF (oid - page - 1));

      if (vg_folio_object_type (folio, page) == vg_cap_void)
	return NULL;

      if (folio_object_discarded (folio, page))
	/* Don't return a discarded object until the discarded flag is
	   explicitly cleared.  */
	return NULL;

      if (! folio_object_content (folio, page))
	/* The object is a zero page.  No need to read anything from
	   backing store: just allocate a page and zero it.  */
	return memory_object_alloc (activity, vg_folio_object_type (folio, page),
				    oid, folio_object_version (folio, page),
				    policy);
    }
  
  /* Read the object from backing store.  */

  /* XXX: Do it.  */
  return NULL;
}

void
folio_parent (struct activity *activity, struct vg_folio *folio)
{
  /* Some sanity checks.  */
  assert (({
	struct object_desc *desc;
	desc = object_to_object_desc ((struct vg_object *) folio);
	assert (desc->oid % (VG_FOLIO_OBJECTS + 1) == 0);
	true;
      }));
  assert (! vg_cap_to_object (activity, &folio->activity));
  assert (! vg_cap_to_object (activity, &folio->next));
  assert (! vg_cap_to_object (activity, &folio->prev));
  assert (({
	struct object_desc *desc;
	desc = object_to_object_desc ((struct vg_object *) folio);
	if (desc->oid != 0)
	  /* Only the very first folio may have objects allocated out
	     of it before it is parented.  */
	  {
	    int i;
	    for (i = 0; i < VG_FOLIO_OBJECTS; i ++)
	      assert (! object_find_soft (activity, desc->oid + 1 + i,
					  VG_OBJECT_POLICY_DEFAULT));
	  }
	true;
      }));

  /* Record the owner.  */
  folio->activity = object_to_cap ((struct vg_object *) activity);

  /* Add FOLIO to ACTIVITY's folio list.  */

  /* Update the old head's previous pointer.  */
  struct vg_object *head = vg_cap_to_object (activity, &activity->folios);
  if (head)
    {
      /* It shouldn't have a previous pointer.  */
      struct vg_object *prev = vg_cap_to_object (activity,
					   &((struct vg_folio *) head)->prev);
      assert (! prev);

      ((struct vg_folio *) head)->prev = object_to_cap ((struct vg_object *) folio);
    }

  /* Point FOLIO->NEXT to the old head.  */
  folio->next = activity->folios;

  /* Ensure FOLIO's PREV pointer is void.  */
  folio->prev.type = vg_cap_void;

  /* Finally, set ACTIVITY->FOLIOS to the new head.  */
  activity->folios = object_to_cap ((struct vg_object *) folio);
  assert (vg_cap_to_object (activity, &activity->folios)
	  == (struct vg_object *) folio);
}

struct vg_folio *
folio_alloc (struct activity *activity, struct vg_folio_policy policy)
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
  vg_oid_t foid = f * (VG_FOLIO_OBJECTS + 1);

  /* We can't just allocate a fresh page: we need to preserve the
     version information for the folio as well as the objects.  */
  struct vg_folio *folio = (struct vg_folio *) object_find (activity, foid,
						      VG_OBJECT_POLICY_DEFAULT);

  if (activity)
    folio_parent (activity, folio);

  folio->policy = policy;

  return folio;
}

void
folio_free (struct activity *activity, struct vg_folio *folio)
{
  /* Make sure that FOLIO appears on its owner's folio list.  */
  assert (({
	struct activity *owner
	  = (struct activity *) vg_cap_to_object (activity, &folio->activity);
	assert (owner);
	assert (object_type ((struct vg_object *) owner) == vg_cap_activity_control);
	struct vg_folio *f;
	for (f = (struct vg_folio *) vg_cap_to_object (activity, &owner->folios);
	     f; f = (struct vg_folio *) vg_cap_to_object (activity, &f->next))
	  {
	    assert (object_type ((struct vg_object *) folio) == vg_cap_folio);
	    if (f == folio)
	      break;
	  }
	assert (f);
	true;
      }));

  /* NB: The activity freeing FOLIO may not be the one who paid for
     the storage for it.  We use it as the entity who should pay for
     the paging activity, etc.  */

  struct object_desc *fdesc = object_to_object_desc ((struct vg_object *) folio);
  assert (fdesc->type == vg_cap_folio);
  assert (fdesc->oid % (VG_FOLIO_OBJECTS + 1) == 0);

  /* Free the objects.  This bumps the version of any live objects.
     This is correct as although the folio is being destroyed, when we
     lookup an object via a capability, we only check that the
     capability's version matches the object's version (we do not
     check whether the folio is valid).  */
  /* As we free the objects, we also don't have to call cap_shootdown
     here.  */
  int i;
  for (i = 0; i < VG_FOLIO_OBJECTS; i ++)
    folio_object_free (activity, folio, i);

  struct activity *owner
    = (struct activity *) vg_cap_to_object (activity, &folio->activity);
  assert (owner);

  /* Update the allocation information.  */
  struct activity *a = owner;
  activity_for_each_ancestor (a, ({ a->folio_count --; }));

  /* Clear the owner.  */
  folio->activity.type = vg_cap_void;

  /* Remove FOLIO from its owner's folio list.  */
  struct vg_folio *next = (struct vg_folio *) vg_cap_to_object (activity, &folio->next);
  struct vg_folio *prev = (struct vg_folio *) vg_cap_to_object (activity, &folio->prev);

  if (prev)
    prev->next = folio->next;
  else
    /* If there is no previous pointer, then FOLIO is the start of the
       list and we need to update the head.  */
    owner->folios = folio->next;

  if (next)
    next->prev = folio->prev;

  folio->next.type = vg_cap_void;
  folio->prev.type = vg_cap_void;

  /* And free the folio.  */

  /* XXX: We need to now schedule the folio for page-out: it contains
     previous data including version information.  */
  fdesc->version = folio_object_version (folio, -1) + 1;
  folio_object_version_set (folio, -1, fdesc->version);
  bit_dealloc (folios, fdesc->oid / (VG_FOLIO_OBJECTS + 1));
}

struct vg_cap
folio_object_alloc (struct activity *activity,
		    struct vg_folio *folio,
		    int idx,
		    enum vg_cap_type type,
		    struct vg_object_policy policy,
		    uintptr_t return_code)
{
  assert (0 <= idx && idx < VG_FOLIO_OBJECTS);

  type = vg_cap_type_strengthen (type);

  struct object_desc *fdesc = object_to_object_desc ((struct vg_object *) folio);
  assert (fdesc->type == vg_cap_folio);
  assert (fdesc->oid % (1 + VG_FOLIO_OBJECTS) == 0);

  debug (5, VG_OID_FMT ":%d -> %s (%s/%d)",
	 VG_OID_PRINTF (fdesc->oid), idx, vg_cap_type_string (type),
	 policy.discardable ? "discardable" : "precious", policy.priority);

  vg_oid_t oid = fdesc->oid + 1 + idx;

  struct vg_object *object = NULL;

  /* Deallocate any existing object.  */

  if (vg_folio_object_type (folio, idx) == vg_cap_activity_control
      || vg_folio_object_type (folio, idx) == vg_cap_thread
      || vg_folio_object_type (folio, idx) == vg_cap_messenger)
    /* These object types have state that needs to be explicitly
       destroyed.  */
    {
      object = object_find (activity, oid, VG_OBJECT_POLICY_DEFAULT);

      assert (object_to_object_desc (object)->type
	      == vg_folio_object_type (folio, idx));

      /* See if we need to destroy the object.  */
      switch (vg_folio_object_type (folio, idx))
	{
	case vg_cap_activity_control:
	  debug (4, "Destroying activity at %llx", oid);
	  activity_destroy (activity, (struct activity *) object);
	  break;
	case vg_cap_thread:
	  debug (4, "Destroying thread object at %llx", oid);
	  thread_deinit (activity, (struct thread *) object);
	  break;
	case vg_cap_messenger:
	  debug (4, "Destroying messenger object at %llx", oid);
	  messenger_destroy (activity, (struct messenger *) object);
	  break;
	default:
	  assert (!"Object desc type does not match folio type.");
	  break;
	}
    }

  /* Wake any threads waiting on this object.  We wake them even if
     they are not waiting for this object's death.  */
  struct messenger *messenger;
  folio_object_wait_queue_for_each (activity, folio, idx, messenger)
    {
      object_wait_queue_unlink (activity, messenger);
      if (messenger->wait_reason == MESSENGER_WAIT_DESTROY)
	vg_object_reply_on_destruction_reply (activity,
					      messenger, return_code);
      else
	rpc_error_reply (activity, messenger, EFAULT);
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
      assert (odesc->version == folio_object_version (folio, idx));
      assertx (odesc->type == vg_folio_object_type (folio, idx),
	       VG_OID_FMT ": %s != %s",
	       VG_OID_PRINTF (odesc->oid), vg_cap_type_string (odesc->type),
	       vg_cap_type_string (vg_folio_object_type (folio, idx)));

      if (type == vg_cap_void)
	/* We are deallocating the object: free associated memory.  */
	{
	  memory_object_destroy (activity, object);

	  /* Return the frame to the free pool.  */
	  memory_frame_free ((l4_word_t) object);

	  object = NULL;
	}
      else
	{
	  struct vg_cap vg_cap = object_desc_to_cap (odesc);
	  assert (activity);
	  cap_shootdown (activity, &vg_cap);

	  memset ((void *) object, 0, PAGESIZE);
	  object_desc_flush (odesc, true);
	  odesc->dirty = false;
	  odesc->user_referenced = false;
	  odesc->user_dirty = false;

	  object_desc_claim (activity, odesc, policy, true);

	  odesc->type = type;
	  odesc->shared = false;
	}
    }

  if (vg_folio_object_type (folio, idx) != vg_cap_void)
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

  vg_folio_object_type_set (folio, idx, type);
  folio_object_content_set (folio, idx, false);
  folio_object_discarded_set (folio, idx, false);
  vg_folio_object_policy_set (folio, idx, policy);
  folio_object_referenced_set (folio, idx, false);
  folio_object_dirty_set (folio, idx, false);

  switch (type)
    {
    case vg_cap_activity_control:
      {
	if (! object)
	  object = object_find (activity, oid, policy);

	activity_create (activity, (struct activity *) object);
	break;
      }

    default:
      ;
    }

  struct vg_cap vg_cap;
  memset (&vg_cap, 0, sizeof (vg_cap));
  vg_cap.type = type;
  vg_cap.oid = oid;
  vg_cap.version = folio_object_version (folio, idx);
  VG_CAP_POLICY_SET (&vg_cap, policy);

  return vg_cap;
}

void
folio_policy (struct activity *activity,
	      struct vg_folio *folio,
	      uintptr_t flags, struct vg_folio_policy in,
	      struct vg_folio_policy *out)
{
  if ((flags & VG_FOLIO_POLICY_DELIVER) && out)
    {
      out->discardable = folio->policy.discardable;
      out->group = folio->policy.group;
      out->priority = folio->policy.priority;
    }

  if (! (flags & VG_FOLIO_POLICY_SET))
    return;

  if ((flags & VG_FOLIO_POLICY_GROUP_SET))
    folio->policy.group = in.group;

  if ((flags & VG_FOLIO_POLICY_DISCARDABLE_SET)
      && in.discardable != folio->policy.discardable)
    /* XXX: We need to move the folio from the discardable list to the
       precious list (or vice versa).  */
    folio->policy.discardable = in.discardable;

  if ((flags & VG_FOLIO_POLICY_PRIORITY_SET))
    folio->policy.priority = in.priority;
}

void
object_desc_claim (struct activity *activity, struct object_desc *desc,
		   struct vg_object_policy policy, bool update_accounting)
{
  assert (desc->activity || activity);

  struct vg_object_policy o = desc->policy;
  bool ec = desc->eviction_candidate;

#ifndef NDEBUG
  if (desc->activity && update_accounting)
    {
      int active = 0;
      int inactive = 0;

      int i;
      for (i = VG_OBJECT_PRIORITY_MIN; i <= VG_OBJECT_PRIORITY_MAX; i ++)
	{
	  active += activity_list_count (&desc->activity->frames[i].active);
	  inactive += activity_list_count (&desc->activity->frames[i].inactive);
	}

      assertx (active + inactive
	       + eviction_list_count (&desc->activity->eviction_dirty)
	       == desc->activity->frames_local,
	       "%d+%d+%d = %d != %d! (%p=>%p,%d,%d,%d=>%d,%d=>%d)",
	       active, inactive,
	       eviction_list_count (&desc->activity->eviction_dirty),
	       active + inactive
	       + eviction_list_count (&desc->activity->eviction_dirty),
	       desc->activity->frames_local,
	       desc->activity, activity,
	       desc->eviction_candidate, desc->dirty,
	       o.discardable, policy.discardable, 
	       o.priority, policy.priority);
    }
  if (activity)
    {
      int active = 0;
      int inactive = 0;

      int i;
      for (i = VG_OBJECT_PRIORITY_MIN; i <= VG_OBJECT_PRIORITY_MAX; i ++)
	{
	  active += activity_list_count (&activity->frames[i].active);
	  inactive += activity_list_count (&activity->frames[i].inactive);
	}

      assertx (active + inactive
	       + eviction_list_count (&activity->eviction_dirty)
	       == activity->frames_local,
	       "%d+%d+%d = %d != %d! (%p=>%p,%d,%d,%d=>%d,%d=>%d)",
	       active, inactive,
	       eviction_list_count (&activity->eviction_dirty),
	       active + inactive
	       + eviction_list_count (&activity->eviction_dirty),
	       activity->frames_local,
	       desc->activity, activity,
	       desc->eviction_candidate, desc->dirty,
	       o.discardable, policy.discardable, 
	       o.priority, policy.priority);
    }
#endif

  if (desc->activity == activity
      && ! desc->eviction_candidate
      && desc->policy.priority == policy.priority)
    /* The owner remains the same, the object is not an eviction
       candidate and the priority didn't change; don't do any
       unnecessary work.  */
    {
      desc->policy.discardable = policy.discardable;
      goto out;
    }


  /* We need to disconnect DESC from its old activity.  If DESC does
     not have an activity, it is being initialized.  */
  if (desc->activity)
    {
      debug (5, VG_OID_FMT " claims from " VG_OID_FMT,
	     VG_OID_PRINTF (object_to_object_desc ((struct vg_object *) desc
						->activity)->oid),
	     VG_OID_PRINTF (activity
			 ? object_to_object_desc ((struct vg_object *)
						  activity)->oid
			 : 0));

      assert (object_type ((struct vg_object *) desc->activity)
	      == vg_cap_activity_control);

      if (desc->eviction_candidate)
	/* DESC is an eviction candidate.  The act of claiming saves
	   it.  */
	{
	  if (desc->dirty && ! desc->policy.discardable)
	    {
	      laundry_list_unlink (&laundry, desc);
	      eviction_list_unlink (&desc->activity->eviction_dirty, desc);

	      if (update_accounting)
		{
		  if (activity != desc->activity)
		    desc->activity->frames_local --;

		  struct activity *ancestor = desc->activity;
		  activity_for_each_ancestor
		    (ancestor,
		     ({
		       if (activity != desc->activity)
			 ancestor->frames_total --;
		       ancestor->frames_pending_eviction --;
		     }));
		}
	    }
	  else
	    {
	      available_list_unlink (&available, desc);
	      eviction_list_unlink (&desc->activity->eviction_clean, desc);
	    }
	}
      else
	{
	  activity_list_unlink
	    (object_active (desc)
	     ? &desc->activity->frames[desc->policy.priority].active
	     : &desc->activity->frames[desc->policy.priority].inactive,
	     desc);

	  if (activity != desc->activity && update_accounting)
	    activity_charge (desc->activity, -1);
	}

      if ((activity != desc->activity
	   || (desc->eviction_candidate
	       && ! (desc->dirty && ! desc->policy.discardable)))
	  && update_accounting
	  && desc->activity->free_goal)
	{
	  desc->activity->free_goal --;
	  if (desc->activity->free_goal == 0)
	    /* The activity met the free goal!  */
	    {
	      debug (0, DEBUG_BOLD (OBJECT_NAME_FMT " met goal."),
		     OBJECT_NAME_PRINTF ((struct vg_object *) desc->activity));

	      struct activity *ancestor = desc->activity;
	      activity_for_each_ancestor
		(ancestor,
		 ({
		   ancestor->frames_excluded
		     -= desc->activity->free_initial_allocation;
		 }));

	      desc->activity->free_allocations = 0;
	    }
	}
    }

  if (! activity)
    goto out;

  desc->policy.priority = policy.priority;

  /* Assign to ACTIVITY.  */

  /* We make the object active.  The invariants require that DESC->AGE
     be non-zero.  */
  object_age (desc, true);
  activity_list_push (&activity->frames[desc->policy.priority].active, desc);

  if ((activity != desc->activity
       || (desc->eviction_candidate
	   && ! (desc->dirty && ! desc->policy.discardable)))
      && update_accounting)
    {
      activity_charge (activity, 1);

      if (activity->free_allocations)
	{
	  activity->free_allocations --;
	  if (activity->free_allocations == 0)
	    {
	      if (activity->free_goal)
		/* Bad boy!  */
		{
		  activity->free_bad_karma = 8;

		  debug (0, DEBUG_BOLD (OBJECT_NAME_FMT
					" failed to free %d pages."),
			 OBJECT_NAME_PRINTF ((struct vg_object *) activity),
			 activity->free_goal);
		}

	      struct activity *ancestor = activity;
	      activity_for_each_ancestor
		(ancestor,
		 ({
		   ancestor->frames_excluded
		     -= activity->free_initial_allocation;
		 }));

	      activity->free_goal = 0;
	    }
	}
    }

  desc->eviction_candidate = false;
  desc->activity = activity;
  desc->policy.discardable = policy.discardable;

  debug (5, VG_OID_FMT " claimed " VG_OID_FMT " (%s): %s",
	 VG_OID_PRINTF (object_to_object_desc ((struct vg_object *) activity)->oid),
	 VG_OID_PRINTF (desc->oid),
	 vg_cap_type_string (desc->type),
	 desc->policy.discardable ? "discardable" : "precious");

 out:;
#ifndef NDEBUG
  if (desc->activity && update_accounting)
    {
      int active = 0;
      int inactive = 0;

      int i;
      for (i = VG_OBJECT_PRIORITY_MIN; i <= VG_OBJECT_PRIORITY_MAX; i ++)
	{
	  active += activity_list_count (&desc->activity->frames[i].active);
	  inactive += activity_list_count (&desc->activity->frames[i].inactive);
	}

      assertx (active + inactive
	       + eviction_list_count (&desc->activity->eviction_dirty)
	       == desc->activity->frames_local,
	       "%d+%d+%d = %d != %d (%p=>%p,%d,%d,%d=>%d,%d=>%d)",
	       active, inactive,
	       eviction_list_count (&desc->activity->eviction_dirty),
	       active + inactive
	       + eviction_list_count (&desc->activity->eviction_dirty),
	       desc->activity->frames_local,
	       desc->activity, activity, ec, desc->dirty,
	       o.discardable, policy.discardable, 
	       o.priority, policy.priority);
    }
  if (activity && update_accounting)
    {
      int active = 0;
      int inactive = 0;

      int i;
      for (i = VG_OBJECT_PRIORITY_MIN; i <= VG_OBJECT_PRIORITY_MAX; i ++)
	{
	  active += activity_list_count (&activity->frames[i].active);
	  inactive += activity_list_count (&activity->frames[i].inactive);
	}

      assertx (active + inactive
	       + eviction_list_count (&activity->eviction_dirty)
	       == activity->frames_local,
	       "%d+%d+%d = %d != %d! (%p=>%p,%d,%d,%d=>%d,%d=>%d)",
	       active, inactive,
	       eviction_list_count (&activity->eviction_dirty),
	       active + inactive
	       + eviction_list_count (&activity->eviction_dirty),
	       activity->frames_local,
	       desc->activity, activity, ec, desc->dirty,
	       o.discardable, policy.discardable, 
	       o.priority, policy.priority);
    }
#endif
}

/* Return the first waiter queued on object OBJECT.  */
struct messenger *
object_wait_queue_head (struct activity *activity, struct vg_object *object)
{
  struct vg_folio *folio = objects_folio (activity, object);
  int i = objects_folio_offset (object);

  if (! folio_object_wait_queue_p (folio, i))
    return NULL;

  vg_oid_t h = folio_object_wait_queue (folio, i);
  struct vg_object *head = object_find (activity, h, VG_OBJECT_POLICY_DEFAULT);
  assert (head);
  assert (object_type (head) == vg_cap_messenger);
  assert (((struct messenger *) head)->wait_queue_p);
  assert (((struct messenger *) head)->wait_queue_head);

  return (struct messenger *) head;
}

/* Return the last waiter queued on object OBJECT.  */
struct messenger *
object_wait_queue_tail (struct activity *activity, struct vg_object *object)
{
  struct messenger *head = object_wait_queue_head (activity, object);
  if (! head)
    return NULL;

  if (head->wait_queue_tail)
    /* HEAD is also the list's tail.  */
    return head;

  struct messenger *tail;
  tail = (struct messenger *) object_find (activity, head->wait_queue.prev,
					   VG_OBJECT_POLICY_DEFAULT);
  assert (tail);
  assert (object_type ((struct vg_object *) tail) == vg_cap_messenger);
  assert (tail->wait_queue_p);
  assert (tail->wait_queue_tail);

  return tail;
}

/* Return the waiter following M.  */
struct messenger *
object_wait_queue_next (struct activity *activity, struct messenger *m)
{
  if (m->wait_queue_tail)
    return NULL;

  struct messenger *next;
  next = (struct messenger *) object_find (activity, m->wait_queue.next,
					   VG_OBJECT_POLICY_DEFAULT);
  assert (next);
  assert (object_type ((struct vg_object *) next) == vg_cap_messenger);
  assert (next->wait_queue_p);
  assert (! next->wait_queue_head);

  return next;
}

/* Return the waiter preceding M.  */
struct messenger *
object_wait_queue_prev (struct activity *activity, struct messenger *m)
{
  if (m->wait_queue_head)
    return NULL;

  struct messenger *prev;
  prev = (struct messenger *) object_find (activity, m->wait_queue.prev,
					   VG_OBJECT_POLICY_DEFAULT);
  assert (prev);
  assert (object_type ((struct vg_object *) prev) == vg_cap_messenger);
  assert (prev->wait_queue_p);
  assert (! prev->wait_queue_tail);

  return prev;
}

static void
object_wait_queue_check (struct activity *activity, struct messenger *messenger)
{
#ifndef NDEBUG
  if (! messenger->wait_queue_p)
    return;

  struct messenger *last = messenger;
  struct messenger *m;
  for (;;)
    {
      if (last->wait_queue_tail)
	break;

      m = (struct messenger *) object_find (activity, last->wait_queue.next,
					    VG_OBJECT_POLICY_DEFAULT);
      assert (m);
      assert (m->wait_queue_p);
      assert (! m->wait_queue_head);
      struct vg_object *p = object_find (activity, m->wait_queue.prev,
					VG_OBJECT_POLICY_DEFAULT);
      assert (p == (struct vg_object *) last);
      
      last = m;
    }

  assert (last->wait_queue_tail);

  struct vg_object *o = object_find (activity, last->wait_queue.next,
				  VG_OBJECT_POLICY_DEFAULT);
  assert (o);
  assert (folio_object_wait_queue_p (objects_folio (activity, o),
				     objects_folio_offset (o)));

  struct messenger *head = object_wait_queue_head (activity, o);
  if (! head)
    return;
  assert (head->wait_queue_head);

  struct messenger *tail;
  tail = (struct messenger *) object_find (activity, head->wait_queue.prev,
					   VG_OBJECT_POLICY_DEFAULT);
  assert (tail);
  assert (tail->wait_queue_tail);

  assert (last == tail);

  last = head;
  while (last != messenger)
    {
      assert (! last->wait_queue_tail);

      m = (struct messenger *) object_find (activity, last->wait_queue.next,
					    VG_OBJECT_POLICY_DEFAULT);
      assert (m);
      assert (m->wait_queue_p);
      assert (! m->wait_queue_head);

      struct vg_object *p = object_find (activity, m->wait_queue.prev,
				      VG_OBJECT_POLICY_DEFAULT);
      assert (p == (struct vg_object *) last);
      
      last = m;
    }
#endif /* !NDEBUG */
}

void
object_wait_queue_push (struct activity *activity,
			struct vg_object *object, struct messenger *messenger)
{
  debug (5, "Pushing " VG_OID_FMT " onto %p",
	 VG_OID_PRINTF (object_to_object_desc ((struct vg_object *) messenger)->oid),
	 object);

  object_wait_queue_check (activity, messenger);

  assert (! messenger->wait_queue_p);

  struct messenger *oldhead = object_wait_queue_head (activity, object);
  if (oldhead)
    {
      assert (oldhead->wait_queue_head);

      /* MESSENGER->PREV = TAIL.  */
      messenger->wait_queue.prev = oldhead->wait_queue.prev;

      /* OLDHEAD->PREV = MESSENGER.  */
      oldhead->wait_queue_head = 0;
      oldhead->wait_queue.prev = object_oid ((struct vg_object *) messenger);

      /* MESSENGER->NEXT = OLDHEAD.  */
      messenger->wait_queue.next = object_oid ((struct vg_object *) oldhead);

      messenger->wait_queue_tail = 0;
    }
  else
    /* Empty list.  */
    {
      folio_object_wait_queue_p_set (objects_folio (activity, object),
				     objects_folio_offset (object),
				     true);

      /* MESSENGER->PREV = MESSENGER.  */
      messenger->wait_queue.prev = object_oid ((struct vg_object *) messenger);

      /* MESSENGER->NEXT = OBJECT.  */
      messenger->wait_queue_tail = 1;
      messenger->wait_queue.next = object_oid (object);
    }

  messenger->wait_queue_p = true;

  /* WAIT_QUEUE = MESSENGER.  */
  messenger->wait_queue_head = 1;
  folio_object_wait_queue_set (objects_folio (activity, object),
			       objects_folio_offset (object),
			       object_oid ((struct vg_object *) messenger));

  object_wait_queue_check (activity, messenger);
}

void
object_wait_queue_enqueue (struct activity *activity,
			   struct vg_object *object, struct messenger *messenger)
{
  debug (5, "Enqueueing " VG_OID_FMT " on %p",
	 VG_OID_PRINTF (object_to_object_desc
			((struct vg_object *) messenger)->oid),
	 object);

  object_wait_queue_check (activity, messenger);

  assert (! messenger->wait_queue_p);

  struct messenger *oldtail = object_wait_queue_tail (activity, object);
  if (oldtail)
    {
      /* HEAD->PREV = MESSENGER.  */
      struct messenger *head = object_wait_queue_head (activity, object);
      head->wait_queue.prev = object_oid ((struct vg_object *) messenger);

      assert (oldtail->wait_queue_tail);

      /* MESSENGER->PREV = OLDTAIL.  */
      messenger->wait_queue.prev = object_oid ((struct vg_object *) oldtail);

      /* OLDTAIL->NEXT = MESSENGER.  */
      oldtail->wait_queue_tail = 0;
      oldtail->wait_queue.next = object_oid ((struct vg_object *) messenger);

      /* MESSENGER->NEXT = OBJECT.  */
      messenger->wait_queue.next = object_oid (object);

      messenger->wait_queue_head = 0;
      messenger->wait_queue_tail = 1;
    }
  else
    /* Empty list.  */
    {
      folio_object_wait_queue_p_set (objects_folio (activity, object),
				     objects_folio_offset (object),
				     true);

      /* MESSENGER->PREV = MESSENGER.  */
      messenger->wait_queue.prev = object_oid ((struct vg_object *) messenger);

      /* MESSENGER->NEXT = OBJECT.  */
      messenger->wait_queue_tail = 1;
      messenger->wait_queue.next = object_oid (object);

      /* WAIT_QUEUE = MESSENGER.  */
      messenger->wait_queue_head = 1;
      folio_object_wait_queue_set (objects_folio (activity, object),
				   objects_folio_offset (object),
				   object_oid ((struct vg_object *) messenger));
    }

  messenger->wait_queue_p = true;

  object_wait_queue_check (activity, messenger);
}

/* Unlink messenger MESSENGER from its wait queue.  */
void
object_wait_queue_unlink (struct activity *activity,
			  struct messenger *messenger)
{
  debug (5, "Removing " VG_OID_FMT,
	 VG_OID_PRINTF (object_to_object_desc
			((struct vg_object *) messenger)->oid));

  assert (messenger->wait_queue_p);

  object_wait_queue_check (activity, messenger);

  if (messenger->wait_queue_tail)
    /* MESSENGER is the tail.  MESSENGER->NEXT must be the object on which
       we are queued.  */
    {
      struct vg_object *object;
      object = object_find (activity, messenger->wait_queue.next,
			    VG_OBJECT_POLICY_DEFAULT);
      assert (object);
      assert (folio_object_wait_queue_p (objects_folio (activity, object),
					 objects_folio_offset (object)));
      assert (object_wait_queue_tail (activity, object) == messenger);

      if (messenger->wait_queue_head)      
	/* MESSENGER is also the head and thus the only item on the
	   list.  */
	{
	  assert (object_find (activity, messenger->wait_queue.prev,
			       VG_OBJECT_POLICY_DEFAULT)
		  == (struct vg_object *) messenger);

	  folio_object_wait_queue_p_set (objects_folio (activity, object),
					 objects_folio_offset (object),
					 false);
	}
      else
	/* MESSENGER is not also the head.  */
	{
	  struct messenger *head = object_wait_queue_head (activity, object);

	  /* HEAD->PREV == TAIL.  */
	  assert (object_find (activity, head->wait_queue.prev,
			       VG_OBJECT_POLICY_DEFAULT)
		  == (struct vg_object *) messenger);

	  /* HEAD->PREV = TAIL->PREV.  */
	  head->wait_queue.prev = messenger->wait_queue.prev;

	  /* TAIL->PREV->NEXT = OBJECT.  */
	  struct messenger *prev;
	  prev = (struct messenger *) object_find (activity,
						messenger->wait_queue.prev,
						VG_OBJECT_POLICY_DEFAULT);
	  assert (prev);
	  assert (object_type ((struct vg_object *) prev) == vg_cap_messenger);

	  prev->wait_queue_tail = 1;
	  prev->wait_queue.next = messenger->wait_queue.next;
	}
    }
  else
    /* MESSENGER is not the tail.  */
    {
      struct messenger *next = object_wait_queue_next (activity, messenger);
      assert (next);

      struct vg_object *p = object_find (activity, messenger->wait_queue.prev,
					 VG_OBJECT_POLICY_DEFAULT);
      assert (p);
      assert (object_type (p) == vg_cap_messenger);
      struct messenger *prev = (struct messenger *) p;

      if (messenger->wait_queue_head)
	/* MESSENGER is the head.  */
	{
	  /* MESSENGER->PREV is the tail, TAIL->NEXT the object.  */
	  struct messenger *tail = prev;

	  struct vg_object *object
	    = object_find (activity, tail->wait_queue.next,
			   VG_OBJECT_POLICY_DEFAULT);
	  assert (object);
	  assert (object_wait_queue_head (activity, object) == messenger);


	  /* OBJECT->WAIT_QUEUE = MESSENGER->NEXT.  */
	  next->wait_queue_head = 1;

	  folio_object_wait_queue_set (objects_folio (activity, object),
				       objects_folio_offset (object),
				       messenger->wait_queue.next);
	}
      else
	/* MESSENGER is neither the head nor the tail.  */
	{
	  /* MESSENGER->PREV->NEXT = MESSENGER->NEXT.  */
	  prev->wait_queue.next = messenger->wait_queue.next;
	}

      /* MESSENGER->NEXT->PREV = MESSENGER->PREV.  */
      next->wait_queue.prev = messenger->wait_queue.prev;
    }

  messenger->wait_queue_p = false;

#ifndef NDEBUG
  if (messenger->wait_reason == MESSENGER_WAIT_FUTEX)
    futex_waiter_list_unlink (&futex_waiters, messenger);
#endif

  object_wait_queue_check (activity, messenger);
}
