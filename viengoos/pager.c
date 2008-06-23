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
  struct object *object = object_desc_to_object (desc);
  l4_fpage_t result = l4_unmap_fpage (l4_fpage ((l4_word_t) object, PAGESIZE));
  assertx (! l4_was_written (result) && ! l4_was_referenced (result),
	   "The %s " OID_FMT "(at %p) has status bits set (%s %s)",
	   cap_type_string (desc->type), OID_PRINTF (desc->oid), object,
	   l4_was_written (result) ? "dirty" : "",
	   l4_was_referenced (result) ? "refed" : "");

  if (! desc->dirty)
    {
      uint64_t *p = (uint64_t *) object;
      uint64_t i;
      bool clean = true;
      for (i = 0; i < PAGESIZE / sizeof (i); i ++, p ++)
	if (*p != 0)
	  {
	    debug (0, "%p[%lld*%d]==%llx",
		   object, i, sizeof (i), *p);
	    clean = false;
	  }
      assertx (clean,
	       "The %s " OID_FMT "(at %p) is dirty!",
	       cap_type_string (desc->type), OID_PRINTF (desc->oid),
	       object);
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
  int discarded = 0;

  /* XXX: Implement group dealloc.  */

  /* First try objects with a priority lower than LRU.  */

  assertx (victim->priorities_count
	   + activity_lru_list_count (&victim->active)
	   + activity_lru_list_count (&victim->inactive)
	   + eviction_list_count (&victim->eviction_dirty)
	   == victim->frames_local,
	   "%d + %d + %d + %d != %d!",
	   victim->priorities_count,
	   activity_lru_list_count (&victim->active),
	   activity_lru_list_count (&victim->inactive),
	   eviction_list_count (&victim->eviction_dirty),
	   victim->frames_local);

  debug (5, "Reclaiming %d from " OID_FMT ", %d frames "
	 "(global: avail: %d, laundry: %d)",
	 goal, OID_PRINTF (object_oid ((struct object *) victim)),
	 victim->frames_local,
	 available_list_count (&available), laundry_list_count (&laundry));

  struct object_desc *desc;
  struct object_desc *next
    = hurd_btree_priorities_first (&victim->priorities);
  while (((desc = next)) && count < goal)
    {
      assert (! desc->eviction_candidate);

      assert (desc->policy.priority != OBJECT_PRIORITY_LRU);
      if (desc->policy.priority > OBJECT_PRIORITY_LRU)
	/* DESC's priority is higher than OBJECT_PRIORITY_LRU, prefer
	   LRU ordered pages.  */
	break;

      next = hurd_btree_priorities_next (desc);

      object_desc_flush (desc, false);

      hurd_btree_priorities_detach (&victim->priorities, desc);
      victim->priorities_count --;

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

	  if (desc->policy.discardable)
	    discarded ++;
	}

      count ++;
    }

  if (count < goal)
    /* VICTIM still has to yield pages.  Start stealing from the
       inactive LRU lists.  */
    {
      struct object_desc *inactive;

      /* For every clean page we steal, we queue a dirty page for
	 writeout.  */

      inactive = activity_lru_list_head (&victim->inactive);

      struct object_desc *next;

      while (inactive && count < goal)
	{
	  assert (! inactive->eviction_candidate);
	  assert (! list_node_attached (&inactive->available_node));

	  next = activity_lru_list_next (inactive);

	  activity_lru_list_unlink (&victim->inactive, inactive);

	  object_desc_flush (inactive, false);
	  if (inactive->dirty && ! inactive->policy.discardable)
	    {
	      eviction_list_push (&victim->eviction_dirty, inactive);

	      laundry_list_queue (&laundry, inactive);
	      laundry_count ++;
	    }
	  else
	    {
	      is_clean (inactive);

	      eviction_list_push (&victim->eviction_clean, inactive);

	      available_list_queue (&available, inactive);

	      if (desc->policy.discardable)
		discarded ++;
	    }

	  inactive->eviction_candidate = true;

	  count ++;

	  inactive = next;
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

	  object_desc_flush (desc, false);

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

	      if (desc->policy.discardable)
		discarded ++;
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

	  object_desc_flush (desc, false);

	  hurd_btree_priorities_detach (&victim->priorities, desc);
	  victim->priorities_count --;

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

	      if (desc->policy.discardable)
		discarded ++;
	    }

	  count ++;
	}
    }

  victim->frames_local -= count - laundry_count;
  struct activity *ancestor = victim;
  activity_for_each_ancestor
    (ancestor,
     ({
       ancestor->frames_total -= count - laundry_count;
       ancestor->frames_pending_eviction += laundry_count;

       ACTIVITY_STATS (ancestor)->evicted += count;
     }));

  debug (5, "Reclaimed from " OID_FMT ": goal: %d; %d frames; "
	 DEBUG_BOLD ("%d in laundry, %d made available (%d discarded)") " "
	 "(now: avail: %d, laundry: %d)",
	 OID_PRINTF (object_oid ((struct object *) victim)), goal,
	 victim->frames_local,
	 laundry_count, count - laundry_count, discarded,
	 available_list_count (&available), laundry_list_count (&laundry));

  assertx (victim->priorities_count
	   + activity_lru_list_count (&victim->active)
	   + activity_lru_list_count (&victim->inactive)
	   + eviction_list_count (&victim->eviction_dirty)
	   == victim->frames_local,
	   "%d + %d + %d + %d != %d!",
	   victim->priorities_count,
	   activity_lru_list_count (&victim->active),
	   activity_lru_list_count (&victim->inactive),
	   eviction_list_count (&victim->eviction_dirty),
	   victim->frames_local);

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
  struct activity *victim;
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
	debug (5, "Considering " OID_FMT
	       ": policy: %d/%d; frames: %d (total: %d/%d; "
	       "pending eviction: %d/%d, active: %d/%d)",
	       OID_PRINTF (object_oid ((struct object *) activity)),
	       activity_policy.priority, activity_policy.weight,
	       activity_frames,
	       activity->frames_total, activity->frames_local,
	       eviction_list_count (&activity->eviction_dirty),
	       activity->frames_pending_eviction,
	       ACTIVITY_STATS_LAST (activity)->active,
	       ACTIVITY_STATS_LAST (activity)->active_local);

	if (activity_frames <= goal / 1000)
	  /* ACTIVITY has no frames to yield; don't consider it.  */
	  {
	    debug (5, "Not choosing activity " OID_FMT,
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

      victim = root_activity;
      do
	{
	  debug (5, "Current victim: " OID_FMT,
		 OID_PRINTF (object_to_object_desc ((struct object *) victim)
			     ->oid));

	  parent = victim;
	  victim = NULL;

	  /* Each time through, we let the number of active frames play a
	     less significant role.  */
	  unsigned int factor;
	  for (factor = 0; ! victim && factor < 16; factor += 2)
	    {
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
		      int frames = parent->frames_local
			- eviction_list_count (&parent->eviction_dirty)
			- (ACTIVITY_STATS_LAST (parent)->active_local
			   >> factor);
		      if (frames > 0)
			done = process (parent, parent->policy.child_rel,
					frames);
		      if (done)
			break;
		    }

		  int frames = child->frames_total
		    - child->frames_pending_eviction
		    - (ACTIVITY_STATS_LAST (child)->active >> factor);
		  if (frames > 0)
		    done = process (child, child->policy.sibling_rel, frames);
		  if (done)
		    break;
		}

	      if (! done && ! have_self)
		{
		  int frames = parent->frames_local
		    - eviction_list_count (&parent->eviction_dirty)
		    - (ACTIVITY_STATS_LAST (parent)->active_local >> factor);
		  if (frames > 0)
		    process (parent, parent->policy.child_rel, frames);
		}

	      if (victim)
		break;

	      debug (5, "nothing; raising factor to %d", 1 << (factor + 1));
	    }

	  if (! victim)
	    break;
	  assert (victim);
	}
      while (victim != parent);

      if (! victim)
	break;

      /* We steal from VICTIM.  */

      /* Calculate VICTIM's share of the frames allocated to all the
	 activity's at this priority level.  */
      int share = 0;
      if (weight > 0 && frames > goal)
	share = ((uint64_t) (frames - goal) * victim_policy.weight) / weight;
      assert (share >= 0);

      /* VICTIM's share must be less than or equal to the frames
	 allocated to this priority as we know that this activity has
	 an excess and VICTIM is the most excessive.  */
      assertx (share <= victim->frames_total,
	       "%d > %d", share, victim->frames_total);

      assertx (victim_frames >= share,
	       "%d < %d", victim_frames, share);

      debug (0, DEBUG_BOLD ("Revoking from activity " OID_FMT "%s, ")
	     "%d/%d frames (pending eviction: %d/%d), share: %d, goal: %d",
	     OID_PRINTF (object_to_object_desc ((struct object *) victim)->oid),
	     victim->parent ? "" : " (root activity)",
	     victim->frames_local, victim->frames_total,
	     eviction_list_count (&victim->eviction_dirty),
	     victim->frames_pending_eviction,
	     share, goal);

      int reclaim = victim_frames - share;
      if (reclaim > goal)
	reclaim = goal;

      total_freed += reclaim_from (victim, reclaim);
    }

  return total_freed;
}
