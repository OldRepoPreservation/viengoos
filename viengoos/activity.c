/* activity.c - Activity object implementation.
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

#include <errno.h>
#include <assert.h>
#include <hurd/cap.h>

#include "activity.h"
#include "object.h"

error_t
activity_allocate (struct activity *parent,
		   struct thread *caller,
		   addr_t faddr, l4_word_t index,
		   addr_t aaddr, addr_t caddr,
		   l4_word_t priority, l4_word_t weight,
		   l4_word_t storage_quota)
{
  if (! (0 <= index && index < FOLIO_OBJECTS))
    return EINVAL;

  struct cap folio_cap = object_lookup_rel (parent, &caller->aspace,
					    faddr, cap_folio, NULL);
  if (folio_cap.type == cap_void)
    return ENOENT;
  struct object *folio = cap_to_object (parent, &folio_cap);
  if (! folio)
    return ENOENT;

  struct cap *acap = slot_lookup_rel (parent, &caller->aspace, aaddr,
				      -1, NULL);
  if (! acap)
    return ENOENT;
  struct cap *ccap = slot_lookup_rel (parent, &caller->aspace, caddr,
				      -1, NULL);
  if (! ccap)
    return ENOENT;

  struct object *o;
  folio_object_alloc (parent, (struct folio *) folio, index,
		      cap_activity, &o);
  struct activity *activity = (struct activity *) o;
  *ccap = *acap = object_to_cap (o);
  ccap->type = cap_activity_control;

  activity->priority = priority;
  activity->weight = weight;
  activity->storage_quota = storage_quota;

  return 0;
}

void
activity_destroy (struct activity *activity,
		  struct cap *cap, struct activity *target)
{
  /* XXX: If we implement storage reparenting, we need to be careful
     to avoid a recursive loop as an activity's storage may be stored
     in a folio allocated to itself.  */
  assert (! cap || cap->type == cap_activity_control);

  /* We should never destroy the root activity.  */
  if (target->parent.type == cap_void)
    panic ("Request to destroy root activity");

  /* XXX: Rewrite this to avoid recusion!!!  */

  /* Destroy all folios allocated to this activity.  */
  while (target->folios.type != cap_void)
    {
      struct object *f = cap_to_object (activity, &target->folios);
      /* If F was destroyed, it should have been removed from its
	 respective activity's allocation list.  */
      assert (f);
      folio_free (activity, (struct folio *) f);
    }

  /* Activity's that are sub-activity's of ACTIVITY are not
     necessarily allocated out of storage allocated to ACTIVITY.  */
  while (target->children.type != cap_void)
    {
      struct object *a = cap_to_object (activity, &activity->children);
      /* If A was destroyed, it should have been removed from its
	 respective activity's allocation list.  */
      assert (a);
      activity_destroy (target, NULL, (struct activity *) a);
    }

  /* Remove from parent's activity list.  */
  struct activity *prev = NULL;
  if (target->sibling_prev.type != cap_void)
    prev = (struct activity *) cap_to_object (activity, &target->sibling_prev);

  struct activity *next = NULL;
  if (target->sibling_next.type != cap_void)
    next = (struct activity *) cap_to_object (activity, &target->sibling_next);

  struct activity *p
    = (struct activity *) cap_to_object (activity, &target->parent);
  assert (p);
  struct object_desc *pdesc = object_to_object_desc ((struct object *) p);

  if (prev)
    prev->sibling_next = target->sibling_next;
  else
    {
      assert (p->children.oid == pdesc->oid);
      assert (p->children.version == pdesc->version);
    }

  if (next)
    {
      next->sibling_prev = target->sibling_prev;
      if (! prev)
	/* NEXT is new head.  */
	p->children = activity->sibling_next;
    }

  object_free (activity, (struct object *) target);
}
