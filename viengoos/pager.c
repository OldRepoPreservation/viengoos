/* pager.c - Pager implementation.
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

#include "memory.h"
#include "zalloc.h"
#include "activity.h"
#include "object.h"

/* We try to keep at least 1/8 (12.5%) of memory available for
   immediate allocation.  */
#define LOW_WATER_MARK (memory_total / 8)
/* When we start freeing, we try to get at least 3/16 (~19%) of memory
   available for immediate allocation.  */
#define HIGH_WATER_MARK (LOW_WATER_MARK + LOW_WATER_MARK / 2)

/* Free memory from the activity or activities with the largest
   disproportionate share of memory such that the number of clean
   pages plus the number of pages in the laundry exceed the low-memory
   threshold.  */
void
pager_collect (void)
{
  int available_pages = zalloc_memory
    + available_list_count (&available)
    /* We only count the pages on the laundry half as they won't be
       available immediately.  */
    + laundry_list_count (&laundry) / 2;

  if (available_pages > LOW_WATER_MARK)
    return;

  /* We've dipped below the low mark mark.  Free enough memory such
     that we are above the high water mark.  */

  int goal = HIGH_WATER_MARK - available_pages;

  debug (0, "Frames: %d, available: %d (%d%%) (dirty: %d), "
	 "low water: %d, goal: %d",
	 memory_total,
	 available_pages, (available_pages * 100) / memory_total,
	 laundry_list_count (&laundry),
	 LOW_WATER_MARK, goal);

  /* Find a victim.  */
  struct activity *victim = root_activity;
  struct activity *parent;

  struct activity_memory_policy victim_policy;
  int victim_frames;

  do
    {
      /* The total weight and the total number of frames of the lowest
	 priority group.  */
      int weight;
      int frames;

      parent = victim;
      victim_frames = victim->frames_local;
      victim_policy = victim->policy.child_rel;
      weight = victim_policy.weight;
      frames = victim_frames;

      struct activity *child;
      activity_for_each_inmemory_child
	(parent, child,
	 ({
	   if (child->policy.sibling_rel.priority < victim_policy.priority)
	     /* CHILD has a lower absolute priority.  */
	     {
	       victim = child;
	       victim_frames = victim->frames_total;
	       victim_policy = victim->policy.sibling_rel;

	       /* Reset the weight.  */
	       weight = victim_policy.weight;
	       frames = victim_frames;
	     }
	   else if (child->policy.sibling_rel.priority
		    == victim_policy.priority)
	     /* CHILD and VICTIM have equal priority.  Steal from the one
		which has the most pages taking into their respective
		weights.  */
	     {
	       weight += child->policy.sibling_rel.weight;
	       frames += child->frames_total;

	       if (child->policy.sibling_rel.weight == victim_policy.weight)
		 /* CHILD and VICTIM have the same weight.  Prefer the
		    one with more frames.  */
		 {
		   if (child->frames_total > frames)
		     {
		       victim = child;
		       victim_frames = victim->frames_total;
		       victim_policy = victim->policy.sibling_rel;
		     }
		 }
	       else
		 {
		   int f = child->frames_total + victim_frames;
		   int w = child->policy.sibling_rel.weight
		     + victim_policy.weight;

		   int child_excess = child->frames_total
		     - (child->policy.sibling_rel.weight * f) / w;
		   int victim_excess = victim_frames
		     - (victim_policy.weight * f) / w;

		   if (child_excess > victim_excess)
		     /* CHILD has more excess frames than VICTIM.  */
		     {
		       victim = child;
		       victim_frames = victim->frames_total;
		       victim_policy = victim->policy.sibling_rel;
		     }
		 }
	     }
	 }));

      if (frames >= goal)
	/* The number of frames at this priority level exceed the
	   goal.  Page all activity's at this priority level
	   out.  */
	{
	  /* XXX: Do it.  */
	}

      /* VICTIM's share of the frames allocated to all the activity's
	 at this priority level.  */
      int share;
      if (weight == 0)
	share = 0;
      else
	share = (frames * victim_policy.weight) / weight;

      /* VICTIM's share must be less than or equal to the frames
	 allocated to this priority as we know that this activity has
	 an excess and VICTIM is the most excessive.  */
      assertx (share < victim->frames_total,
	       "%d <= %d", share, victim->frames_total);

      if (victim->frames_total - share < goal)
	/* VICTIM's excess is less than the amount we want to reclaim.
	   Reclaim from the second worst offender after we finish
	   here.  */
	{
	  /* XXX: Do it.  */
	}

      if (victim->frames_total - share < goal)
	goal = victim->frames_total - share;

      debug (0, "Choosing activity " OID_FMT "%s, %d/%d frames, "
	     "share: %d, goal: %d",
	     OID_PRINTF (object_to_object_desc ((struct object *) victim)->oid),
	     victim->parent ? "" : " (root activity)",
	     victim->frames_total, victim->frames_local, share, goal);
    }
  while (victim != parent);
  assert (victim);

  /* We want to steal GOAL pages from VICTIM.  */

  int count = 0;
  int laundry_count = 0;

  /* XXX: Implement group dealloc.  */

  ss_mutex_lock (&lru_lock);

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

      object_desc_unmap (desc);

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

	      if (object_desc_unmap (clean))
		/* It is possible that the page was dirtied between
		   the last check and now.  */
		{
		  eviction_list_push (&victim->eviction_dirty, clean);

		  laundry_list_queue (&laundry, clean);
		}
	      else
		{
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

	      object_desc_unmap (dirty);

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

	  object_desc_unmap (desc);

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

	  object_desc_unmap (desc);

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

	      available_list_queue (&available, desc);
	      eviction_list_queue (&victim->eviction_clean, desc);
	    }

	  count ++;
	}
    }

  ACTIVITY_STAT_UPDATE (victim, evicted, count);

  debug (0, "Goal: %d, %d in laundry, %d made available",
	 goal, laundry_count, count - laundry_count);

  ss_mutex_unlock (&lru_lock);
}
