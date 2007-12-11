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

struct activity *root_activity;

void
activity_create (struct activity *parent,
		 struct activity *child)
{
  struct object *old_parent = cap_to_object (parent, &child->parent);
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
  child->parent = object_to_cap ((struct object *) parent);
  child->parent_ptr = parent;

  /* Connect to PARENT's activity list.  */
  child->sibling_next = parent->children;
  child->sibling_prev.type = cap_void;
  parent->children = object_to_cap ((struct object *) child);

  struct object *old_head = cap_to_object (parent, &child->sibling_next);
  if (old_head)
    {
      assert (object_type (old_head) == cap_activity_control);
      /* The old head's previous pointer should be NULL.  */
      assert (! cap_to_object (parent,
			       &((struct activity *) old_head)->sibling_prev));

      ((struct activity *) old_head)->sibling_prev
	= object_to_cap ((struct object *) child);
    }
}

void
activity_destroy (struct activity *activity, struct activity *victim)
{
  assert (object_type ((struct object *) activity) == cap_activity_control);
  assert (object_type ((struct object *) victim) == cap_activity_control);

  /* We should never destroy the root activity.  */
  if (! victim->parent_ptr)
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
  while ((o = cap_to_object (activity, &victim->children)))
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
  while ((desc = victim->active))
    {
      object_desc_disown_simple (desc);
      count ++;
    }
  while ((desc = victim->inactive_clean))
    {
      object_desc_disown_simple (desc);
      count ++;
    }
  while ((desc = victim->inactive_dirty))
    {
      object_desc_disown_simple (desc);
      count ++;
    }
  ss_mutex_unlock (&lru_lock);

  activity_charge (victim, -count);

  do_debug (1)
    if (victim->frames != 0)
      {
	debug (0, "activity (%llx)->frame = %d",
	       object_to_object_desc ((struct object *) victim)->oid,
	       victim->frames);
	activity_dump (root_activity);

	struct object_desc *desc;
	ss_mutex_lock (&lru_lock);
	for (desc = victim->active; desc; desc = desc->activity_lru.next)
	  debug (0, " %llx: %s", desc->oid, cap_type_string (desc->type));
	ss_mutex_unlock (&lru_lock);
      }
  assert (victim->frames == 0);
  assert (victim->folio_count == 0);

  /* Remove from parent's activity list.  */
  struct activity *parent = victim->parent_ptr;
  assert ((struct object *) parent
	  == cap_to_object (activity, &victim->parent));

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
    /* VICTIM is the head of PARENT's child list.  */
    {
      assert (cap_to_object (activity, &parent->children)
	      == (struct object *) victim);
      parent->children = victim->sibling_next;
    }

  if (next)
    next->sibling_prev = victim->sibling_prev;

  victim->sibling_next.type = cap_void;
  victim->sibling_prev.type = cap_void;
}

static void
do_activity_dump (struct activity *activity, int indent)
{
  char indent_string[indent + 1];
  memset (indent_string, ' ', indent);
  indent_string[indent] = 0;

  int active = 0;
  struct object_desc *desc;
  for (desc = activity->active; desc; desc = desc->activity_lru.next)
    active ++;

  int dirty = 0;
  for (desc = activity->inactive_dirty; desc; desc = desc->activity_lru.next)
    dirty ++;

  int clean = 0;
  for (desc = activity->inactive_clean; desc; desc = desc->activity_lru.next)
    clean ++;

  printf ("%s %llx: %d frames (active: %d, dirty: %d, clean: %d)\n",
	  indent_string,
	  object_to_object_desc ((struct object *) activity)->oid,
	  activity->frames, active, dirty, clean);

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


void
activity_consistency_check_ (const char *func, int line,
			     struct activity *activity)
{
  /* The number of objects on the active and inactive lists plus the
     objects owned by the descendents must equal activity->frames.  */

  assert (! ss_mutex_trylock (&lru_lock));

  int active = 0;
  struct object_desc *d;
  for (d = activity->active; d; d = d->activity_lru.next)
    active ++;

  int dirty = 0;
  for (d = activity->inactive_dirty; d; d = d->activity_lru.next)
    dirty ++;

  int clean = 0;
  for (d = activity->inactive_clean; d; d = d->activity_lru.next)
    clean ++;

  int children = 0;
  struct activity *child;
  activity_for_each_child (activity, child,
			   ({ children += child->frames; }));

  if (active + dirty + clean + children != activity->frames)
    debug (0, "at %s:%d: frames (%d) "
	   "!= active (%d) + dirty (%d) + clean (%d) + children (%d)",
	   func, line,
	   activity->frames, active, dirty, clean, children);
  assert (active + dirty + clean + children == activity->frames);
}
