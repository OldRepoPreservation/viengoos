/* activity.c - Activity object implementation.
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

#include <errno.h>
#include <assert.h>
#include <hurd/cap.h>

#include "activity.h"
#include "object.h"

struct activity *root_activity;

void
activity_create (struct activity *parent,
		 struct activity *child)
{
  struct object *old_parent = cap_to_object (parent, &child->parent_cap);
  if (old_parent)
    /* CHILD is live.  Destroy it first.  */
    {
      assert (object_type (old_parent) == cap_activity_control);
      activity_destroy (parent, child);
    }

  if (! parent)
    {
      assert (! root_activity);
      return;
    }

  /* Set child's parent pointer.  */
  child->parent_cap = object_to_cap ((struct object *) parent);

  /* Connect to PARENT's activity list.  */
  child->sibling_next_cap = parent->children_cap;
  child->sibling_prev_cap.type = cap_void;
  parent->children_cap = object_to_cap ((struct object *) child);

  struct object *old_head = cap_to_object (parent, &child->sibling_next_cap);
  if (old_head)
    {
      assert (object_type (old_head) == cap_activity_control);
      /* The old head's previous pointer should be NULL.  */
      assert (! cap_to_object
	      (parent, &((struct activity *) old_head)->sibling_prev_cap));

      ((struct activity *) old_head)->sibling_prev_cap
	= object_to_cap ((struct object *) child);
    }

  activity_prepare (parent, child);
}

void
activity_destroy (struct activity *activity, struct activity *victim)
{
  assert (object_type ((struct object *) activity) == cap_activity_control);
  assert (object_type ((struct object *) victim) == cap_activity_control);

  /* We should never destroy the root activity.  */
  if (! victim->parent)
    {
      assert (victim == root_activity);
      panic ("Request to destroy root activity");
    }

  assert (! victim->dying);
  victim->dying = 1;

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
  while ((o = cap_to_object (activity, &victim->children_cap)))
    {
      /* If O was destroyed, it should have been removed from its
	 respective activity's allocation list.  */
      assert (o);

      struct object_desc *desc = object_to_object_desc (o);
      assert (desc->type == cap_activity_control);

      object_free (activity, o);
    }

  /* Disown all allocated memory objects.  */
  ss_mutex_lock (&lru_lock);
  struct object_desc *desc;
  int count = 0;
  while ((desc = object_activity_lru_list_head (&victim->active)))
    {
      object_desc_disown_simple (desc);
      count ++;
    }
  while ((desc = object_activity_lru_list_head (&victim->inactive_clean)))
    {
      object_desc_disown_simple (desc);
      count ++;
    }
  while ((desc = object_activity_lru_list_head (&victim->inactive_dirty)))
    {
      object_desc_disown_simple (desc);
      count ++;
    }
  ss_mutex_unlock (&lru_lock);

  activity_charge (victim, -count);

  do_debug (1)
    if (victim->frames_total != 0)
      {
	debug (0, "activity (%llx)->frames_total = %d",
	       object_to_object_desc ((struct object *) victim)->oid,
	       victim->frames_total);
	activity_dump (root_activity);

	struct object_desc *desc;
	ss_mutex_lock (&lru_lock);
	for (desc = object_activity_lru_list_head (&victim->active);
	     desc; desc = object_activity_lru_list_next (desc))
	  debug (0, " %llx: %s", desc->oid, cap_type_string (desc->type));
	ss_mutex_unlock (&lru_lock);
      }
  assert (victim->frames_total == 0);
  assert (victim->frames_local == 0);
  assert (victim->folio_count == 0);

  activity_deprepare (activity, victim);

  /* Remove from parent's activity list.  */
  struct activity *parent = victim->parent;
  assert ((struct object *) parent
	  == cap_to_object (activity, &victim->parent_cap));

  struct object *prev_object = cap_to_object (activity,
					      &victim->sibling_prev_cap);
  assert (! prev_object
	  || object_to_object_desc (prev_object)->type == cap_activity_control);
  struct activity *prev = (struct activity *) prev_object;

  struct object *next_object = cap_to_object (activity,
					      &victim->sibling_next_cap);
  assert (! next_object
	  || object_to_object_desc (next_object)->type == cap_activity_control);
  struct activity *next = (struct activity *) next_object;

  if (prev)
    prev->sibling_next_cap = victim->sibling_next_cap;
  else
    /* VICTIM is the head of PARENT's child list.  */
    {
      assert (cap_to_object (activity, &parent->children_cap)
	      == (struct object *) victim);
      parent->children_cap = victim->sibling_next_cap;
    }

  if (next)
    next->sibling_prev_cap = victim->sibling_prev_cap;

  victim->sibling_next_cap.type = cap_void;
  victim->sibling_prev_cap.type = cap_void;
}

void
activity_prepare (struct activity *principal, struct activity *activity)
{
  /* Lookup parent.  */
  activity->parent = (struct activity *) cap_to_object (principal,
							&activity->parent_cap);
  assert (activity->parent);

  /* Link to parent's children list.  */
  assert (! activity->parent->children
	  || ! activity->parent->children->sibling_prev);

  activity->sibling_next = activity->parent->children;
  if (activity->parent->children)
    activity->parent->children->sibling_prev = activity;
  activity->parent->children = activity;

  /* We have no in-memory children.  */
  activity->children = NULL;
}

void
activity_deprepare (struct activity *principal, struct activity *victim)
{
  /* If we have any in-memory children, then we can't be paged
     out.  */
  assert (! victim->children);

  /* Unlink from parent's children list.  */
  assert (victim->parent);

  if (victim->sibling_prev)
    victim->sibling_prev->sibling_next = victim->sibling_next;
  else
    {
      assert (victim->parent->children == victim);
      victim->parent->children = victim->sibling_next;
    }

  if (victim->sibling_next)
    victim->sibling_next->sibling_prev = victim->sibling_prev;
}

static void
do_activity_dump (struct activity *activity, int indent)
{
  char indent_string[indent + 1];
  memset (indent_string, ' ', indent);
  indent_string[indent] = 0;

  int active = object_activity_lru_list_count (&activity->active);
  int dirty = object_activity_lru_list_count (&activity->inactive_dirty);
  int clean = object_activity_lru_list_count (&activity->inactive_clean);

  printf ("%s %llx: %d frames (active: %d, dirty: %d, clean: %d) "
	  "(total frames: %d)\n",
	  indent_string,
	  object_to_object_desc ((struct object *) activity)->oid,
	  activity->frames_local, active, dirty, clean,
	  activity->frames_total);

  assert (active + dirty + clean == activity->frames_local);

  struct activity *child;
  activity_for_each_child (activity, child,
			   ({ do_activity_dump (child, indent + 1); }));
}

void
activity_dump (struct activity *activity)
{
  ss_mutex_lock (&lru_lock);

  do_activity_dump (activity, 0);

  ss_mutex_unlock (&lru_lock);
}
