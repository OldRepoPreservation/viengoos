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
activity_create (struct activity *parent,
		 struct activity *child,
		 l4_word_t priority, l4_word_t weight,
		 l4_word_t storage_quota)
{
  struct object_desc *desc = object_to_object_desc ((struct object *) parent);
  assert (desc->type == cap_activity_control);

  desc = object_to_object_desc ((struct object *) child);
  assert (desc->type == cap_activity_control);

  struct object *old_parent = cap_to_object (parent, &child->parent);
  if (old_parent)
    /* CHILD is live.  Destroy it first.  */
    {
      struct object_desc *desc = object_to_object_desc (old_parent);
      assert (desc->type == cap_activity_control);

      activity_destroy (parent, child);
    }

  child->parent = object_to_cap ((struct object *) parent);

  child->sibling_next = parent->children;
  child->sibling_prev.type = cap_void;
  parent->children = object_to_cap ((struct object *) child);

  struct object *next = cap_to_object (parent, &child->sibling_next);
  if (next)
    {
      desc = object_to_object_desc (next);
      assert (desc->type == cap_activity_control);

      struct activity *n = (struct activity *) next;

      struct object *prev = cap_to_object (parent, &n->sibling_prev);
      assert (! prev);

      ((struct activity *) n)->sibling_prev
	= object_to_cap ((struct object *) child);
    }

  child->priority = priority;
  child->weight = weight;
  child->storage_quota = storage_quota;

  return 0;
}

void
activity_destroy (struct activity *activity, struct activity *victim)
{
  struct object_desc *desc = object_to_object_desc ((struct object *) activity);
  assert (desc->type == cap_activity_control);

  desc = object_to_object_desc ((struct object *) victim);
  assert (desc->type == cap_activity_control);

  /* We should never destroy the root activity.  */
  if (victim->parent.type == cap_void)
    panic ("Request to destroy root activity");

  /* XXX: Rewrite this to avoid recusion!!!  */

  /* Destroy all folios allocated to this activity.  */
  struct object *o;
  while ((o = cap_to_object (activity, &victim->folios)))
    {
      /* If O was destroyed, it should have been removed from its
	 respective activity's allocation list.  */
      assert (o);

      struct object_desc *desc = object_to_object_desc (o);
      assert (desc->type == cap_folio);

      folio_free (activity, (struct folio *) o);
    }

  /* Activity's that are sub-activity's of ACTIVITY are not
     necessarily allocated out of storage allocated to ACTIVITY.  */
  while ((o = cap_to_object (activity, &victim->children)))
    {
      /* If O was destroyed, it should have been removed from its
	 respective activity's allocation list.  */
      assert (o);

      struct object_desc *desc = object_to_object_desc (o);
      assert (desc->type == cap_activity_control);

      activity_destroy (activity, (struct activity *) o);
    }

  /* Remove from parent's activity list.  */
  struct object *parent_object = cap_to_object (activity, &victim->parent);
  assert (parent_object);
  struct object_desc *pdesc = object_to_object_desc (parent_object);
  assert (pdesc->type == cap_activity_control);
  struct activity *parent = (struct activity *) parent_object;

  struct object *prev_object = cap_to_object (activity, &victim->sibling_prev);
  assert (! prev_object
	  || object_to_object_desc (prev_object)->type == cap_activity_control);
  struct activity *prev = (struct activity *) prev_object;

  struct object *next_object = cap_to_object (activity, &victim->sibling_next);
  assert (! next_object
	  || object_to_object_desc (next_object)->type == cap_activity_control);
  struct activity *next = (struct activity *) next_object;

  if (prev)
    prev->sibling_next = victim->sibling_next;
  else
    /* VICTIM better be the head of PARENT's child list.  */
    {
      struct object_desc *desc
	= object_to_object_desc ((struct object *) victim);

      assert (parent->children.oid == desc->oid);
      assert (parent->children.version == desc->version);
    }

  if (next)
    {
      next->sibling_prev = victim->sibling_prev;
      if (! prev)
	/* NEXT is new head.  */
	parent->children = victim->sibling_next;
    }
}
