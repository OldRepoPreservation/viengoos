/* object.c - Object store management.
   Copyright (C) 2007 Free Software Foundation, Inc.
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
#include <bit-array.h>

#include "object.h"
#include "activity.h"


struct object_desc *object_descs;

/* XXX: The number of in memory folios.  (Recall: one folio => 512kb
   storage.)  */
#define FOLIOS_CORE 256
static unsigned char folios[FOLIOS_CORE / 8];

/* Given an OID, we need a way to find 1) whether the object is
   memory, and 2) if so, where.  We achieve this using a hash.  The
   hash maps object OIDs to union object *s.  */
/* XXX: Although the current implementation of the has function
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

/* The object OBJECT was just brought into memory.  Set it up.  */
static void
memory_object_setup (struct object *object)
{
  struct object_desc *odesc = object_to_object_desc (object);

  debug (5, "Setting up 0x%llx (object %d)", odesc->oid,
	 ((uintptr_t) odesc - (uintptr_t) object_descs)
	 / sizeof (*odesc));

  bool had_value;
  hurd_ihash_value_t old_value;
  error_t err = hurd_ihash_replace (&objects, odesc->oid, odesc,
				    &had_value, &old_value);
  assert (err == 0);
  /* If there was an old value, it better have the same value as what
     we just added.  */
  assert (! had_value || old_value == odesc);
}

/* Release the object.  */
static void
memory_object_destroy (struct activity *activity, struct object *object)
{
  struct object_desc *odesc = object_to_object_desc (object);

  debug (5, "Destroy 0x%llx (object %d)", odesc->oid,
	 ((uintptr_t) odesc - (uintptr_t) object_descs)
	 / sizeof (*odesc));

  struct cap cap = object_desc_to_cap (odesc);
  cap_shootdown (activity, &cap);

  hurd_ihash_locp_remove (&objects, odesc->locp);
  assert (! hurd_ihash_find (&objects, odesc->oid));

  /* XXX: Remove from linked lists!  */


#ifdef NDEBUG
  memset (odesc, 0xde, sizeof (struct object_desc));
#endif

  /* Return the frame to the free pool.  */
  memory_frame_free ((l4_word_t) object);
}

void
object_init (void)
{
  assert (sizeof (struct folio) <= PAGESIZE);

  hurd_ihash_init (&objects, (int) (&((struct object_desc *)0)->locp));

  /* Allocate enough object descriptors for the number of pages.  */
  object_descs = calloc (sizeof (struct object_desc),
			 ((last_frame - first_frame) / PAGESIZE + 1));
  if (! object_descs)
    panic ("Failed to allocate object descriptor array!\n");
}

struct object *
object_find_soft (struct activity *activity, oid_t oid)
{
  struct object_desc *odesc = hurd_ihash_find (&objects, oid);
  if (! odesc)
    return NULL;
  else
    {
      struct object *object = object_desc_to_object (odesc);
      if (oid != odesc->oid)
	{
	  debug (1, "oid (%llx) != desc oid (%llx)",
		 oid, odesc->oid);
	}
      assert (oid == odesc->oid);
      return object;
    }
}


struct object *
object_find (struct activity *activity, oid_t oid)
{
  struct object *obj = object_find_soft (activity, oid);
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

	  obj = (struct object *) memory_frame_allocate ();
	  folio = (struct folio *) obj;
	  if (! folio)
	    {
	      /* XXX: Out of memory.  Do some garbage collection.  */
	      return NULL;
	    }

	  goto setup_desc;
	}

      /* It's not an in-memory folio.  We read it from disk below.  */
    }
  else
    {
      /* Find the folio corresponding to the object.  */
      folio = (struct folio *) object_find (activity, oid - page - 1);
      assert (folio);

      if (! folio->objects[page].content)
	/* The object is a zero page.  No need to read anything from
	   backing store: just allocate a page and zero it.  */
	{
	  obj = (struct object *) memory_frame_allocate ();
	  if (! obj)
	    {
	      /* XXX: Out of memory.  Do some garbage collection.  */
	      return NULL;
	    }

	  goto setup_desc;
	}
    }
  
  /* Read the object from backing store.  */

  /* XXX: Do it.  */
  return NULL;

 setup_desc:;
  /* OBJ points to the in-memory copy of the object.  Set up its
     corresponding descriptor.  */
  struct object_desc *odesc = object_to_object_desc (obj);

  if (page == -1)
    /* It's a folio.  */
    {
      odesc->type = cap_folio;
      odesc->version = folio->folio_version;
    }
  else
    {
      odesc->type = folio->objects[page].type;
      odesc->version = folio->objects[page].version;
    }
  odesc->oid = oid;
  memory_object_setup (obj);

  return obj;
}

void
folio_reparent (struct activity *principal, struct folio *folio,
		struct activity *new_parent)
{
  /* XXX: Implement this for the real "reparent" case (and not just
     the parent case).  */

  /* Record the owner.  */
  struct object_desc *pdesc
    = object_to_object_desc ((struct object *) new_parent);
  assert (pdesc->type == cap_activity);
  folio->activity.oid = pdesc->oid;
  folio->activity.version = pdesc->version;
  folio->activity.type = cap_activity;

  /* Add FOLIO to ACTIVITY's list of allocated folios.  */

  /* Set FOLIO->NEXT to the current head.  */
  folio->next = new_parent->folios;
  folio->prev.type = cap_void;

  oid_t foid = object_to_object_desc ((struct object *) folio)->oid;

  struct object *head = cap_to_object (principal, &new_parent->folios);
  if (head)
    /* Update the old head's previous pointer to point to FOLIO.  */
    {
      struct object_desc *odesc = object_to_object_desc (head);
      assert (odesc->type == cap_folio);

      struct folio *h = (struct folio *) head;

      struct object *head_prev = cap_to_object (principal, &h->prev);
      assert (! head_prev);

      h->prev.oid = foid;
      h->prev.type = cap_folio;
      h->prev.version = folio->folio_version;
    }

  /* Make FOLIO the head.  */
  new_parent->folios.oid = foid;
  new_parent->folios.type = cap_folio;
  new_parent->folios.version = folio->folio_version;
}

struct folio *
folio_alloc (struct activity *activity)
{
  if (activity)
    {
      /* Check that the activity does not exceed its quota.  */
      /* XXX: Charge not only the activity but also its ancestors.  */
      if (activity->storage_quota
	  && activity->folio_count < activity->storage_quota)
	activity->folio_count ++;
    }
#ifndef NDEBUG
  else
    {
      static int once;
      assert (! once);
      once = 1;
    }
#endif

  /* XXX: We only do in-memory folios right now.  */
  int f = bit_alloc (folios, sizeof (folios), 0);
  if (f < 0)
    panic ("Out of folios");
  oid_t foid = f * (FOLIO_OBJECTS + 1);

  /* We can't just allocate a fresh page as we need to preserve the
     version information for the folio as well as the objects.  */
  struct folio *folio = (struct folio *) object_find (activity, foid);

  if (activity)
    folio_reparent (activity, folio, activity);

  return folio;
}

void
folio_free (struct activity *activity, struct folio *folio)
{
  /* NB: The activity freeing FOLIO may not be the one who paid for
     the storage for it.  Nevertheless, the paging activity, etc., is
     paid for by the caller.  */

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

  /* Update the allocation information.  Namely, remove folio from the
     activity's linked list.  */
  struct activity *storage_activity
    = (struct activity *) cap_to_object (activity, &folio->activity);
  assert (storage_activity);
  
  struct folio *next = (struct folio *) cap_to_object (activity, &folio->next);
  struct folio *prev = (struct folio *) cap_to_object (activity, &folio->prev);

  if (prev)
    prev->next = folio->next;
  else
    /* If there is no previous pointer, then FOLIO is the start of the
       list and we need to update the head.  */
    storage_activity->folios = folio->next;

  if (next)
    next->prev = folio->prev;

  /* XXX: Update accounting data.  */


  /* And free the folio.  */
  bit_dealloc (folios, fdesc->oid / (FOLIO_OBJECTS + 1));

  fdesc->version ++;
}

void
folio_object_alloc (struct activity *activity,
		    struct folio *folio,
		    int idx,
		    enum cap_type type,
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

  if (folio->objects[idx].type == cap_activity
      || folio->objects[idx].type == cap_thread)
    /* These object types have state that needs to be explicitly
       destroyed.  */
    {
      object = object_find (activity, oid);

      /* See if we need to destroy the object.  */
      switch (folio->objects[idx].type)
	{
	case cap_activity:
	  debug (4, "Destroying activity at %llx", oid);
	  activity_destroy (activity, NULL, (struct activity *) object);
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
    object = object_find_soft (activity, oid);
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
	  cap_shootdown (activity, &cap);
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

  if (objectp)
    /* Caller wants to use the object.  */
    {
      assert (type != cap_void);

      if (! object)
	object = object_find (activity, oid);
      *objectp = object;
    }
}
