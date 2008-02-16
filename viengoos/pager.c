/* pager.c - Pager implementation.
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

#include "memory.h"
#include "zalloc.h"
#include "activity.h"
#include "object.h"
#include "pager.h"

static void
is_clean (struct object_desc *desc)
{
  if (! desc->dirty)
    {
      struct object *object = object_desc_to_object (desc);

      int *p = (int *) object;
      int i;
      for (i = 0; i < PAGESIZE / sizeof (int); i ++, p ++)
	assertx (*p == 0,
		 OID_FMT "/%p (%s) is dirty!",
		 OID_PRINTF (desc->oid), p, cap_type_string (desc->type));
    }
}

/* Reclaim GOAL pages from VICTIM.  (Reclaim means either schedule for
   page-out if dirty and not discardable or place on the clean
   list.)  */
static int
reclaim_from (struct activity *victim, int goal)
{
  int count = 0;
  int laundry_count = 0;

  /* XXX: Implement group dealloc.  */

  /* First try objects with a priority lower than LRU.  */

  struct object_desc *desc;
  struct object_desc *next = hurd_btree_priorities_first (&victim->priorities);
  while (((desc = next)) && count < goal)
    {
      assert (! desc->eviction_candidate);

      assert (desc->policy.priority != OBJECT_PRIORITY_LRU);
      if (desc->policy.priority > OBJECT_PRIORITY_LRU)
	/* DESC's priority is higher than OBJECT_PRIORITY_LRU, prefer
	   LRU ordered pages.  */
	break;

      next = hurd_btree_priorities_next (desc);

      object_desc_flush (desc);

      hurd_btree_priorities_detach (&victim->priorities, desc);

      desc->eviction_candidate = true;

      if (desc->dirty && ! desc->policy.discardable)
	{
	  if (! list_node_attached (&desc->laundry_node))
	    laundry_list_queue (&laundry, desc);

	  eviction_list_queue (&victim->eviction_dirty, desc);
	  laundry_count ++;
	}
      else
	{
	  assert (! list_node_attached (&desc->available_node));
	  is_clean (desc);

	  available_list_queue (&available, desc);
	  eviction_list_queue (&victim->eviction_clean, desc);
	}

      count ++;
    }

  if (count < goal)
    /* VICTIM still has to yield pages.  Start stealing from the
       inactive LRU lists.  */
    {
      struct object_desc *clean, *dirty;

      /* For every clean page we steal, we queue a dirty page for
	 writeout.  */

      clean = activity_lru_list_head (&victim->inactive_clean);
      dirty = activity_lru_list_head (&victim->inactive_dirty);

      struct object_desc *next;

      while ((clean || dirty) && count < goal)
	{
	  if (clean)
	    {
	      assert (! clean->eviction_candidate);
	      assert (! clean->dirty || clean->policy.discardable);
	      assert (! list_node_attached (&clean->available_node));

	      next = activity_lru_list_next (clean);

	      activity_lru_list_unlink (&victim->inactive_clean, clean);

	      object_desc_flush (clean);
	      if (clean->dirty)
		/* It is possible that the page was dirtied between
		   the last check and now.  */
		{
		  eviction_list_push (&victim->eviction_dirty, clean);

		  laundry_list_queue (&laundry, clean);
		}
	      else
		{
		  is_clean (desc);

		  eviction_list_push (&victim->eviction_clean, clean);

		  available_list_queue (&available, clean);
		}

	      clean->eviction_candidate = true;

	      count ++;

	      clean = next;
	    }

	  if (count == goal)
	    break;

	  if (dirty)
	    {
	      assert (! dirty->eviction_candidate);
	      assert (dirty->dirty && ! dirty->policy.discardable);
	      assert (! list_node_attached (&dirty->laundry_node));

	      next = activity_lru_list_next (dirty);

	      object_desc_flush (dirty);

	      dirty->eviction_candidate = true;

	      activity_lru_list_unlink (&victim->inactive_dirty, dirty);
	      eviction_list_push (&victim->eviction_dirty, dirty);

	      laundry_list_queue (&laundry, dirty);

	      laundry_count ++;
	      count ++;

	      dirty = next;
	    }
	}
    }

  if (count < goal)
    /* Still hasn't yielded enough.  Steal from the active list.  */
    {
      struct object_desc *desc;
      struct object_desc *next = activity_lru_list_head (&victim->active);

      while ((desc = next) && count < goal)
	{
	  assert (! desc->eviction_candidate);

	  next = activity_lru_list_next (desc);

	  object_desc_flush (desc);

	  desc->eviction_candidate = true;

	  activity_lru_list_unlink (&victim->active, desc);

	  if (desc->dirty && ! desc->policy.discardable)
	    {
	      if (! list_node_attached (&desc->laundry_node))
		laundry_list_queue (&laundry, desc);

	      eviction_list_queue (&victim->eviction_dirty, desc);
	      laundry_count ++;
	    }
	  else
	    {
	      assert (! list_node_attached (&desc->available_node));
	      is_clean (desc);

	      available_list_queue (&available, desc);
	      eviction_list_queue (&victim->eviction_clean, desc);
	    }

	  count ++;
	}
    }

  if (count < goal)
    /* We've cleared the low priority nodes, the inactive LRU lists
       and the active LRU list.  Steal from the high priority
       lists.  */
    {
      /* NEXT points to the first high priority object descriptor.  */

      while (count < goal && (desc = next))
	{
	  assert (! desc->eviction_candidate);
	  assert (desc->policy.priority > OBJECT_PRIORITY_LRU);

	  next = hurd_btree_priorities_next (desc);

	  object_desc_flush (desc);

	  hurd_btree_priorities_detach (&victim->priorities, desc);

	  desc->eviction_candidate = true;

	  if (desc->dirty && ! desc->policy.discardable)
	    {
	      if (! list_node_attached (&desc->laundry_node))
		laundry_list_queue (&laundry, desc);

	      eviction_list_queue (&victim->eviction_dirty, desc);
	      laundry_count ++;
	    }
	  else
	    {
	      assert (! list_node_attached (&desc->available_node));
	      is_clean (desc);

	      available_list_queue (&available, desc);
	      eviction_list_queue (&victim->eviction_clean, desc);
	    }

	  count ++;
	}
    }

  ACTIVITY_STAT_UPDATE (victim, evicted, count);

  victim->frames_local -= count;
  struct activity *ancestor = victim;
  activity_for_each_ancestor (ancestor,
			      ({ ancestor->frames_total -= count; }));

  debug (0, "Goal: %d, %d in laundry, %d made available",
	 goal, laundry_count, count - laundry_count);
  /* We should never have selected a task from which we can free
     nothing!  */
  assert (count > 0);

  return count;
}

/* Free memory from the activity or activities with the largest
   disproportionate share of memory such that the number of clean
   pages plus the number of pages in the laundry exceed the low-memory
   threshold.  */
int
pager_collect (int goal)
{
  activity_dump (root_activity);

  int available_memory = zalloc_memory + available_list_count (&available);

  debug (0, "Frames: %d, available: %d (%d%%), paging out: %d, "
	 "low water: %d, goal: %d",
	 memory_total,
	 available_memory, (available_memory * 100) / memory_total,
	 laundry_list_count (&laundry),
	 PAGER_LOW_WATER_MARK, goal);

  /* Find a victim.  */
  struct activity *victim = root_activity;
  struct activity *parent;

  struct activity_memory_policy victim_policy;
  int victim_frames;

  int total_freed = 0;

  while (total_freed < goal)
    {
      /* The total weight and the total number of frames of the lowest
	 priority group.  */
      int weight;
      int frames;

      /* FRAMES is the number of frames allocated to the activity
	 minus the number of active frames.  */
      bool process (struct activity *activity,
		    struct activity_memory_policy activity_policy,
		    int activity_frames)
      {
	debug (0, "Considering activity " OID_FMT
	       ": prio: %d; frames: %d/%d; active: %d/%d",
	       OID_PRINTF (object_oid ((struct object *) activity)),
	       activity_policy.priority,
	       activity->frames_total, activity->frames_local,
	       ACTIVITY_STATS_LAST (activity)->active,
	       ACTIVITY_STATS_LAST (activity)->active_local);

	if (activity_frames <= 0)
	  /* ACTIIVTY has no frames to yield; don't consider it.  */
	  {
	    debug (0, "Not choosing activity " OID_FMT,
		   OID_PRINTF (object_oid ((struct object *) activity)));
	    return false;
	  }

	if (! victim)
	  {
	    victim = activity;
	    victim_frames = activity_frames;
	    victim_policy = activity_policy;

	    /* Initialize the weight.  */
	    weight = activity_policy.weight;
	    frames = activity_frames;

	    return false;
	  }

	/* We should be processing the activities in reverse priority
	   order.  */
	assertx (activity_policy.priority >= victim_policy.priority,
		 "%d < %d", activity_policy.priority, victim_policy.priority);

	if (activity->policy.sibling_rel.priority > victim_policy.priority)
	  /* ACTIVITY has a higher absolute priority, we're done.  */
	  return true;

	/* ACTIVITY and VICTIM have equal priority.  Steal from the
	   one which has the most pages taking into account their
	   respective weights.  */
	assert (activity->policy.sibling_rel.priority
		== victim_policy.priority);

	weight += activity_policy.weight;
	frames += activity_frames;

	if (activity_policy.weight == victim_policy.weight)
	  /* ACTIVITY and VICTIM have the same weight.  Prefer the one
	     with more frames.  */
	  {
	    if (activity_frames > victim_frames)
	      {
		victim = activity;
		victim_frames = activity_frames;
		victim_policy = activity_policy;
	      }
	  }
	else
	  {
	    int f = activity_frames + victim_frames;
	    int w = activity_policy.weight + victim_policy.weight;

	    int activity_excess = activity_frames
	      - (activity_policy.weight * f) / w;
	    int victim_excess = victim_frames
	      - (victim_policy.weight * f) / w;

	    if (activity_excess > victim_excess)
	      /* ACTIVITY has more excess frames than VICTIM.  */
	      {
		victim = activity;
		victim_frames = activity_frames;
		victim_policy = activity_policy;
	      }
	  }

	return false;
      }

      parent = victim;
      victim = NULL;
      bool have_self = false;

      struct activity *child;
      bool done = false;
      for (child = activity_children_list_tail (&parent->children);
	   child;
	   child = activity_children_list_prev (child))
	{
	  if (! have_self && (parent->policy.child_rel.priority <=
			      child->policy.sibling_rel.priority))
	    {
	      have_self = true;
	      done = process (parent, parent->policy.child_rel,
			      parent->frames_local
			      - ACTIVITY_STATS_LAST (parent)->active_local);
	      if (done)
		break;
	    }

	  done = process (child, child->policy.sibling_rel,
			  child->frames_total
			  - ACTIVITY_STATS_LAST (child)->active);
	  if (done)
	    break;
	}

      if (! done && ! have_self)
	process (parent, parent->policy.child_rel,
		 parent->frames_local
		 - ACTIVITY_STATS_LAST (parent)->active_local);

      assert (victim);

      /* We steal from VICTIM.  */

      /* Calculate VICTIM's share of the frames allocated to all the
	 activity's at this priority level.  */
      int share = 0;
      if (weight > 0 && frames < goal)
	share = ((frames - goal) * victim_policy.weight) / weight;

      /* VICTIM's share must be less than or equal to the frames
	 allocated to this priority as we know that this activity has
	 an excess and VICTIM is the most excessive.  */
      assertx (share < victim->frames_total,
	       "%d <= %d", share, victim->frames_total);

      assertx (victim_frames >= share,
	       "%d > %d", victim_frames, share);

      debug (0, "Choosing activity " OID_FMT "%s, %d/%d frames, "
	     "share: %d, goal: %d",
	     OID_PRINTF (object_to_object_desc ((struct object *) victim)->oid),
	     victim->parent ? "" : " (root activity)",
	     victim->frames_total, victim->frames_local, share, goal);

      int reclaim = victim_frames - share;
      if (reclaim > goal)
	reclaim = goal;

      total_freed += reclaim_from (victim, reclaim);
    }

  return total_freed;
}
