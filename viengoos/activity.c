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
#include <viengoos/cap.h>

#include "activity.h"
#include "thread.h"
#include "object.h"
#include "profile.h"
#include "zalloc.h"
#include "memory.h"

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
  debug (0, "Destroying activity " OBJECT_NAME_FMT,
	 OBJECT_NAME_PRINTF ((struct object *) victim));

  assert (object_type ((struct object *) activity) == cap_activity_control);
  assert (object_type ((struct object *) victim) == cap_activity_control);

  profile_stats_dump ();

  /* We should never destroy the root activity.  */
  if (! victim->parent)
    {
      assert (victim == root_activity);

      debug (0, ""
#ifdef NCHECK
	     "NCHECK "
#endif
#ifdef NDEBUG
	     "NDEBUG "
#endif
#ifdef DEBUG_ELIDE
	     "DEBUG_ELIDE=%d", DEBUG_ELIDE + 0
#endif
	     );
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

  /* Activities that are sub-activities of ACTIVITY are not
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

  /* VICTIM->PARENT inherits all of VICTIM's objects.  */
  {
    struct object_desc *desc;
    int count = 0;

    /* Make ACTIVE objects inactive.  */
    int i;
    for (i = OBJECT_PRIORITY_MIN; i <= OBJECT_PRIORITY_MAX; i ++)
      {
	for (desc = activity_list_head (&victim->frames[i].active);
	     desc; desc = activity_list_next (desc))
	  {
	    assert (! desc->eviction_candidate);
	    assert (desc->activity == victim);
	    assert (! list_node_attached (&desc->laundry_node));
	    assert (desc->age);
	    assert (desc->policy.priority == i);

	    desc->age = 0;
	    desc->policy.priority = OBJECT_PRIORITY_MIN;
	    desc->activity = victim->parent;
	    count ++;
	  }
	activity_list_join
	  (&victim->parent->frames[OBJECT_PRIORITY_MIN].inactive,
	   &victim->frames[i].active);
      }

    /* Move inactive objects to the head of VICTIM->PARENT's appropriate
       inactive list (thereby making them the first eviction
       candidates).  */
    for (i = OBJECT_PRIORITY_MIN; i <= OBJECT_PRIORITY_MAX; i ++)
      {
	for (desc = activity_list_head (&victim->frames[i].inactive);
	     desc; desc = activity_list_next (desc))
	  {
	    assert (! desc->eviction_candidate);
	    assert (desc->activity == victim);
	    assert (! list_node_attached (&desc->laundry_node));
	    assert (! desc->age);
	    assert (desc->policy.priority == i);

	    desc->activity = victim->parent;
	    desc->policy.priority = OBJECT_PRIORITY_MIN;
	    count ++;
	  }
	activity_list_join
	  (&victim->parent->frames[OBJECT_PRIORITY_MIN].inactive,
	   &victim->frames[i].inactive);
      }


    /* And move all of VICTIM's eviction candidates to VICTIM->PARENT's
       eviction lists.  */
    for (desc = eviction_list_head (&victim->eviction_clean);
	 desc; desc = eviction_list_next (desc))
      {
	assert (desc->eviction_candidate);
	assert (desc->activity == victim);
	assert (! list_node_attached (&desc->laundry_node));
	assert (! desc->dirty || desc->policy.discardable);

	desc->activity = victim->parent;
      }
    eviction_list_join (&victim->parent->eviction_clean,
			&victim->eviction_clean);

    for (desc = eviction_list_head (&victim->eviction_dirty);
	 desc; desc = eviction_list_next (desc))
      {
	assert (desc->eviction_candidate);
#ifndef NDEBUG
	struct object_desc *adesc, *vdesc;
	adesc = object_to_object_desc ((struct object *) desc->activity);
	vdesc = object_to_object_desc ((struct object *) victim);

	assertx (desc->activity == victim,
		 OID_FMT " != " OID_FMT,
		 OID_PRINTF (adesc->oid), OID_PRINTF (vdesc->oid));
#endif
	assert (list_node_attached (&desc->laundry_node));
	assert (desc->dirty && !desc->policy.discardable);

	desc->activity = victim->parent;
      }
    eviction_list_join (&victim->parent->eviction_dirty,
			&victim->eviction_dirty);

    /* Adjust the counting information.  */
    do_debug (1)
      if (victim->frames_total != count || victim->frames_local != count)
	{
	  debug (0, "activity (%llx), total = %d, local: %d, count: %d",
		 object_to_object_desc ((struct object *) victim)->oid,
		 victim->frames_total, victim->frames_local, count);
	  activity_dump (root_activity);
	}
    assert (count == victim->frames_local);
    assert (count == victim->frames_total);
    victim->frames_local = victim->frames_total = 0;
    victim->parent->frames_local += count;
  }

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
  struct activity *head;
  head = activity_children_list_head (&activity->parent->children);
  if (! head || (activity->policy.sibling_rel.priority
		 >= head->policy.sibling_rel.priority))
    activity_children_list_insert_after (&activity->parent->children,
					 activity, NULL);
  else
    {
      struct activity *last;
      struct activity *next;

      for (last = head;
	   (next = activity_children_list_next (last))
	     && (next->policy.sibling_rel.priority
		 > activity->policy.sibling_rel.priority);
	   last = next)
	;

      assert (last);
      activity_children_list_insert_after (&activity->parent->children,
					   activity, last);
    }

#ifndef NDEBUG
  activity_children_list_init (&activity->children, "activity->children");

  static const char *names[OBJECT_PRIORITY_LEVELS]
    = { "inactive-64", "inactive-63", "inactive-62", "inactive-61",
	"inactive-60", "inactive-59", "inactive-58", "inactive-57",
	"inactive-56", "inactive-55", "inactive-54", "inactive-53",
	"inactive-52", "inactive-51", "inactive-50", "inactive-49",
	"inactive-48", "inactive-47", "inactive-46", "inactive-45",
	"inactive-44", "inactive-43", "inactive-42", "inactive-41",
	"inactive-40", "inactive-39", "inactive-38", "inactive-37",
	"inactive-36", "inactive-35", "inactive-34", "inactive-33",
	"inactive-32", "inactive-31", "inactive-30", "inactive-29",
	"inactive-28", "inactive-27", "inactive-26", "inactive-25",
	"inactive-24", "inactive-23", "inactive-22", "inactive-21",
	"inactive-20", "inactive-19", "inactive-18", "inactive-17",
	"inactive-16", "inactive-15", "inactive-14", "inactive-13",
	"inactive-12", "inactive-11", "inactive-10", "inactive-9",
	"inactive-8", "inactive-7", "inactive-6", "inactive-5",
	"inactive-4", "inactive-3", "inactive-2", "inactive-1",
	"inactive0", "inactive1", "inactive2", "inactive3",
	"inactive4", "inactive5", "inactive6", "inactive7",
	"inactive8", "inactive9", "inactive10", "inactive11",
	"inactive12", "inactive13", "inactive14", "inactive15",
	"inactive16", "inactive17", "inactive18", "inactive19",
	"inactive20", "inactive21", "inactive22", "inactive23",
	"inactive24", "inactive25", "inactive26", "inactive27",
	"inactive28", "inactive29", "inactive30", "inactive31",
	"inactive32", "inactive33", "inactive34", "inactive35",
	"inactive36", "inactive37", "inactive38", "inactive39",
	"inactive40", "inactive41", "inactive42", "inactive43",
	"inactive44", "inactive45", "inactive46", "inactive47",
	"inactive48", "inactive49", "inactive50", "inactive51",
	"inactive52", "inactive53", "inactive54", "inactive55",
	"inactive56", "inactive57", "inactive58", "inactive59",
	"inactive60", "inactive61", "inactive62", "inactive63" };
 
  int i;
  for (i = OBJECT_PRIORITY_MIN; i <= OBJECT_PRIORITY_MAX; i ++)
    {
      activity_list_init (&activity->frames[i].active,
			  &names[i - OBJECT_PRIORITY_MIN][2]);
      activity_list_init (&activity->frames[i].inactive,
			  names[i - OBJECT_PRIORITY_MIN]);
    }
  eviction_list_init (&activity->eviction_clean, "evict clean");
  eviction_list_init (&activity->eviction_dirty, "evict dirty");
#endif
}

void
activity_deprepare (struct activity *principal, struct activity *victim)
{
  /* If we have any in-memory children or frames, then we can't be
     paged out.  */
  assert (! activity_children_list_head (&victim->children));
  int i;
  for (i = OBJECT_PRIORITY_MIN; i <= OBJECT_PRIORITY_MAX; i ++)
    {
      assert (! activity_list_count (&victim->frames[i].active));
      assert (! activity_list_count (&victim->frames[i].inactive));
    }
  assert (! eviction_list_count (&victim->eviction_clean));
  assert (! eviction_list_count (&victim->eviction_dirty));

  /* Unlink from parent's children list.  */
  assert (victim->parent);

  activity_children_list_unlink (&victim->parent->children, victim);
}

void
activity_policy_update (struct activity *activity,
			struct activity_policy policy)
{
  int priority = policy.sibling_rel.priority;

  if (priority == activity->policy.sibling_rel.priority)
    /* Same priority: noop.  */
    ;
  else if (priority > activity->policy.sibling_rel.priority)
    /* Increased priority.  Search backwards and find the first
       activity that has a priority that is greater than or equal to
       PRIORITY.  Move ACTIVITY to just after that activity.  */
    {
      activity->policy.sibling_rel.priority = priority;

      struct activity *prev;
      for (prev = activity_children_list_prev (activity);
	   prev && prev->policy.sibling_rel.priority < priority;
	   prev = activity_children_list_prev (prev))
	;

      if (prev != activity_children_list_prev (activity))
	{
	  activity_children_list_unlink (&activity->parent->children, activity);
	  activity_children_list_insert_after (&activity->parent->children,
					       activity, prev);
	}
    }
  else
    /* Decreased priority.  */
    {
      activity->policy.sibling_rel.priority = priority;

      struct activity *next;
      struct activity *next_next;
      for (next = activity;
	   (next_next = activity_children_list_next (next))
	     && next_next->policy.sibling_rel.priority > priority;
	   next = next_next)
	;

      if (next != activity)
	{
	  activity_children_list_unlink (&activity->parent->children, activity);
	  activity_children_list_insert_after (&activity->parent->children,
					       activity, next);
	}
    }

  activity->policy = policy;
}

static void
do_activity_dump (struct activity *activity, int indent)
{
  char indent_string[indent + 1];
  memset (indent_string, ' ', indent);
  indent_string[indent] = 0;

  int active = 0;
  int inactive = 0;

  int i;
  for (i = OBJECT_PRIORITY_MIN; i <= OBJECT_PRIORITY_MAX; i ++)
    {
      active += activity_list_count (&activity->frames[i].active);
      inactive += activity_list_count (&activity->frames[i].inactive);
    }
  int clean = eviction_list_count (&activity->eviction_clean);
  int dirty = eviction_list_count (&activity->eviction_dirty);

  printf ("%s" OBJECT_NAME_FMT ": %d frames (active: %d, inactive: %d, "
	  "pending eviction: %d/%d); total: %d; "
	  "%d free allocs, %d karma, %d excluded, s:%d/%d; c:%d/%d\n",
	  indent_string,
	  OBJECT_NAME_PRINTF ((struct object *) activity),
	  activity->frames_local, active, inactive, clean, dirty,
	  activity->frames_total,
	  activity->free_allocations,
	  activity->free_bad_karma,
	  activity->frames_excluded,
	  activity->policy.sibling_rel.priority,
	  activity->policy.sibling_rel.weight,
	  activity->policy.child_rel.priority,
	  activity->policy.child_rel.weight);

  struct activity *child;
  activity_for_each_child (activity, child,
			   ({ do_activity_dump (child, indent + 1); }));
}

void
activity_dump (struct activity *activity)
{
  do_debug (5)
    {
      printf ("Available memory: %d (%d%%); laundry: %d\n",
	      zalloc_memory + available_list_count (&available),
	      (100 * (zalloc_memory + available_list_count (&available))) /
	      memory_total,
	      laundry_list_count (&laundry));
      do_activity_dump (activity, 0);
    }
}
