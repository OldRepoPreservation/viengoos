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
#include "thread.h"
#include "profile.h"

int pager_min_alloc_before_next_collect;

static void
is_clean (struct object_desc *desc)
{
#ifndef NDEBUG
  struct object *object = object_desc_to_object (desc);
  l4_fpage_t result = l4_unmap_fpage (l4_fpage ((l4_word_t) object,
						PAGESIZE));
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
#endif
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

  int i;
#ifndef NDEBUG
  int active = 0;
  int inactive = 0;

  for (i = OBJECT_PRIORITY_MIN; i <= OBJECT_PRIORITY_MAX; i ++)
    {
      active += activity_list_count (&victim->frames[i].active);
      inactive += activity_list_count (&victim->frames[i].inactive);
    }

  assertx (active + inactive + eviction_list_count (&victim->eviction_dirty)
	   == victim->frames_local,
	   "%d + %d + %d != %d!",
	   active, inactive, eviction_list_count (&victim->eviction_dirty),
	   victim->frames_local);
#endif

  debug (5, "Reclaiming %d from " OBJECT_NAME_FMT ", %d frames "
	 "(global: avail: %d, laundry: %d)",
	 goal, OBJECT_NAME_PRINTF ((struct object *) victim),
	 victim->frames_local,
	 available_list_count (&available), laundry_list_count (&laundry));

  for (i = OBJECT_PRIORITY_MIN; i <= OBJECT_PRIORITY_MAX; i ++)
    {
      int s = count;

      struct object_desc *desc;
      while (count < goal
	     && (desc = activity_list_head (&victim->frames[i].inactive)))
	{
	  assert (! desc->eviction_candidate);
	  assert (! list_node_attached (&desc->available_node));
	  assertx (i == desc->policy.priority,
		   "%d != %d",
		   i, desc->policy.priority);

	  activity_list_unlink (&victim->frames[i].inactive, desc);

	  object_desc_flush (desc, false);
	  if (desc->dirty && ! desc->policy.discardable)
	    {
	      eviction_list_queue (&victim->eviction_dirty, desc);

	      laundry_list_queue (&laundry, desc);
	      laundry_count ++;
	    }
	  else
	    {
	      is_clean (desc);

	      eviction_list_queue (&victim->eviction_clean, desc);

	      available_list_queue (&available, desc);

	      if (desc->policy.discardable)
		discarded ++;
	    }

	  desc->eviction_candidate = true;

	  count ++;
	}

      if (count - s > 0)
	debug (5, "Reclaimed %d inactive, priority level %d",
	       count - s, i);
      s = count;

      /* Currently we evict in LIFO order.  We should do a semi-sort and
	 then evict accordingly.  */
      while (count < goal
	     && (desc = activity_list_head (&victim->frames[i].active)))
	{
	  assert (! desc->eviction_candidate);
	  assertx (i == desc->policy.priority,
		   "%d != %d",
		   i, desc->policy.priority);

	  object_desc_flush (desc, false);

	  desc->eviction_candidate = true;

	  activity_list_unlink (&victim->frames[i].active, desc);

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

      if (count - s > 0)
	debug (5, "Reclaimed %d active, priority level %d",
	       count - s, i);
    }

  victim->frames_local -= count - laundry_count;
  ACTIVITY_STATS (victim)->evicted += count;

  struct activity *ancestor = victim;
  activity_for_each_ancestor
    (ancestor,
     ({
       ancestor->frames_total -= count - laundry_count;
       ancestor->frames_pending_eviction += laundry_count;
     }));

  debug (5, "Reclaimed from " OBJECT_NAME_FMT ": goal: %d; %d frames; "
	 DEBUG_BOLD ("%d in laundry, %d made available (%d discarded)") " "
	 "(now: free: %d, avail: %d, laundry: %d)",
	 OBJECT_NAME_PRINTF ((struct object *) victim), goal,
	 victim->frames_local,
	 laundry_count, count - laundry_count, discarded,
	 zalloc_memory, available_list_count (&available),
	 laundry_list_count (&laundry));

#ifndef NDEBUG
  active = 0;
  inactive = 0;

  for (i = OBJECT_PRIORITY_MIN; i <= OBJECT_PRIORITY_MAX; i ++)
    {
      active += activity_list_count (&victim->frames[i].active);
      inactive += activity_list_count (&victim->frames[i].inactive);
    }

  assertx (active + inactive + eviction_list_count (&victim->eviction_dirty)
	   == victim->frames_local,
	   "%d + %d + %d != %d!",
	   active, inactive, eviction_list_count (&victim->eviction_dirty),
	   victim->frames_local);
#endif

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

  debug (5, "Used: %d of %d, available: %d (%d%%), pending page out: %d, "
	 "low water: %d, goal: %d",
	 memory_total - available_memory, memory_total,
	 available_memory, (available_memory * 100) / memory_total,
	 laundry_list_count (&laundry),
	 PAGER_LOW_WATER_MARK, goal);

  profile_start ((uintptr_t) &pager_collect, __FUNCTION__);

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
	debug (5, "Considering " OBJECT_NAME_FMT
	       ": policy: %d/%d; effective frames: %d (total: %d/%d; "
	       "pending eviction: %d/%d, active: %d/%d, available: %d/%d)",
	       OBJECT_NAME_PRINTF ((struct object *) activity),
	       activity_policy.priority, activity_policy.weight,
	       activity_frames,
	       activity->frames_total, activity->frames_local,
	       eviction_list_count (&activity->eviction_dirty),
	       activity->frames_pending_eviction,
	       ACTIVITY_STATS_LAST (activity)->active,
	       ACTIVITY_STATS_LAST (activity)->active_local,
	       ACTIVITY_STATS_LAST (activity)->available,
	       ACTIVITY_STATS_LAST (activity)->available_local);

	if (activity_frames <= goal / 1000)
	  /* ACTIVITY has no frames to yield; don't consider it.  */
	  {
	    debug (5, "Not choosing " OBJECT_NAME_FMT,
		   OBJECT_NAME_PRINTF ((struct object *) activity));
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

      profile_region ("pager_collect(find victim)");

      victim = root_activity;
      do
	{
	  debug (5, "Current victim: " OBJECT_NAME_FMT,
		 OBJECT_NAME_PRINTF ((struct object *) victim));

	  parent = victim;
	  victim = NULL;

	  /* Each time through, we let the number of active frames play a
	     less significant role.  */
	  unsigned int factor;
	  for (factor = 1; ! victim && factor < 16; factor += 2)
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

		      /* If PARENT->FREE_ALLOCATIONS is non-zero, we
			 have already selected this activity for
			 eviction and have sent it a message asking it
			 to free memory.  It may make
			 PARENT->FREE_ALLOCATIONS before it is again
			 eligible.  */
		      if (! parent->free_allocations)
			{
			  int frames = (int) parent->frames_local
			    - eviction_list_count (&parent->eviction_dirty)
			    - ((int) ACTIVITY_STATS_LAST (parent)->active_local
			       >> factor);
			  if (frames > 0)
			    done = process (parent, parent->policy.child_rel,
					    frames);
			  if (done)
			    break;
			}
		      else
			debug (5, "Excluding " OBJECT_NAME_FMT
			       ": %d free frames, %d excluded",
			       OBJECT_NAME_PRINTF ((struct object *) parent),
			       parent->free_allocations,
			       parent->frames_excluded);
		    }

		  int frames = (int) child->frames_total
		    - child->frames_excluded
		    - child->frames_pending_eviction
		    - ((int) ACTIVITY_STATS_LAST (child)->active >> factor);
		  if (frames > 0)
		    done = process (child, child->policy.sibling_rel, frames);
		  if (done)
		    break;
		}

	      if (! done && ! have_self)
		{
		  int frames = (int) parent->frames_local
		    - parent->frames_excluded
		    - eviction_list_count (&parent->eviction_dirty)
		    - ((int) ACTIVITY_STATS_LAST (parent)->active_local
		       >> factor);
		  if (frames > 0)
		    process (parent, parent->policy.child_rel, frames);
		}

	      if (victim)
		break;

	      debug (5, "nothing; raising factor to %d", 1 << (factor + 1));
	    }

	  if (! victim)
	    break;

	  ACTIVITY_STATS (victim)->pressure ++;
	}
      while (victim != parent);

      profile_region_end ();

      if (! victim)
	break;

      /* We steal from VICTIM.  */

      ACTIVITY_STATS (victim)->pressure_local += 4;

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

      debug (5, "%d of %d pages available; "
	     DEBUG_BOLD ("Revoking from activity " OBJECT_NAME_FMT ", ")
	     "%d/%d frames (pending eviction: %d/%d), share: %d, goal: %d",
	     zalloc_memory + available_list_count (&available), memory_total,
	     OBJECT_NAME_PRINTF ((struct object *) victim),
	     victim->frames_local, victim->frames_total,
	     eviction_list_count (&victim->eviction_dirty),
	     victim->frames_pending_eviction,
	     share, goal);

      int reclaim = victim_frames - share;
      if (reclaim > goal)
	reclaim = goal;

      bool need_reclaim = true;

      struct thread *thread;
      object_wait_queue_for_each (victim, (struct object *) victim,
				  thread)
	if (thread->wait_reason == THREAD_WAIT_ACTIVITY_INFO
	    && (thread->wait_reason_arg & activity_info_pressure))
	  break;

      if (thread)
	{
	  debug (5, DEBUG_BOLD ("Requesting that " OBJECT_NAME_FMT " free "
				"%d pages.")
		 " Karma: %d, available: %d, frames: %d",
		 OBJECT_NAME_PRINTF ((struct object *) victim),
		 goal, victim->free_bad_karma,
		 ACTIVITY_STATS_LAST (victim)->available_local,
		 victim->frames_local);

	  if (! victim->free_goal
	      && victim->free_bad_karma == 0)
	    /* If the activity manages half the goal, we'll be
	       happy.  */
	    {
	      need_reclaim = false;

	      victim->free_goal = goal / 2;
	      victim->free_allocations = 1000;
	      victim->free_initial_allocation = victim->frames_local;

	      struct activity *ancestor = victim;
	      activity_for_each_ancestor
		(ancestor,
		 ({
		   ancestor->frames_excluded += victim->frames_local;
		 }));
	    }

	  total_freed += goal;

	  struct activity_info info;
	  info.event = activity_info_pressure;
	  info.pressure.amount = - goal;

	  object_wait_queue_for_each (victim,
				      (struct object *) victim,
				      thread)
	    if (thread->wait_reason == THREAD_WAIT_ACTIVITY_INFO
		&& (thread->wait_reason_arg & activity_info_pressure))
	      {
		object_wait_queue_dequeue (victim, thread);
		rm_activity_info_reply (thread->tid, info);
	      }
	}

      if (victim->free_bad_karma)
	victim->free_bad_karma --;

      if (need_reclaim)
	total_freed += reclaim_from (victim, reclaim);
    }

  if (zalloc_memory + available_list_count (&available)
      >= PAGER_HIGH_WATER_MARK)
    /* We collected enough.  */
    pager_min_alloc_before_next_collect = 0;
  else
    /* Don't collect again until there have been at least 1/3 as many
       allocations as there are currently remaining pages.  */
    pager_min_alloc_before_next_collect
      = (zalloc_memory + available_list_count (&available)) / 3;

  profile_end ((uintptr_t) &pager_collect);

  return total_freed;
}
