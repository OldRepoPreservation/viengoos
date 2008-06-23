/* ager.c - Ager loop implementation.
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

#include <l4/space.h>
#include <l4/schedule.h>
#include "mutex.h"

#include <assert.h>

#include "ager.h"
#include "viengoos.h"
#include "object.h"
#include "activity.h"
#include "zalloc.h"
#include "thread.h"
#include "pager.h"
#include "profile.h"

#define MIN(x,y) ((x) < (y) ? (x) : (y))
#define MAX(x,y) ((x) > (y) ? (x) : (y))


/* A frames has a single claimant.  When a frame is shared among
   multiple activities, the first activity to access claims it (that
   is, that activity is accounted the frame).  To distribute the cost
   among all users, we charge a user proportional to the frequency of
   access.  This is achieved by periodically revoking access to the
   frame and transferring the claim to the next activity to access the
   frame.
   
   The unmapping is required as when there are multiple users and the
   main user no longer users the frame, the frame may remain active as
   other users continue to access it.  The main user will remain the
   claimant, however, as no minor faults will be observed (the frame
   is active).

   XXX: Currently, we unmap shared, mapped frames every every few
   seconds.  Unfortunately, this can lead to an attack whereby a
   malicious activity is able to freeload by carefully timing access
   to frames.  Instead, we should unmap based on a random
   distribution.  */

/* The frequency with which we assemble statistics.  */
#define FREQ (sizeof (((struct object_desc *)0)->age) * 8)

static int period;

static void
update_stats (void)
{
  ss_mutex_lock (&kernel_lock);
  profile_start ((uintptr_t) &update_stats, "update_stats");

  /* XXX: Update the statistics.  We need to average some of the
     fields including the number of active, inactive, clean and dirty
     pages.  Also, we need to calculate each activity's allocation, a
     damping factor and the pressure.  */
  void stats (struct activity *activity, uint32_t frames)
  {
    ACTIVITY_STATS (activity)->period = period / FREQ;

    ACTIVITY_STATS (activity)->clean /= FREQ;
    ACTIVITY_STATS (activity)->dirty /= FREQ;
    ACTIVITY_STATS (activity)->pending_eviction
      = activity->frames_pending_eviction;
    ACTIVITY_STATS (activity)->active /= FREQ;
    ACTIVITY_STATS (activity)->active_local /= FREQ;

    ACTIVITY_STATS (activity)->pressure +=
      ACTIVITY_STATS_LAST (activity)->pressure >> 1;

    if (activity->frames_total > frames
	|| ACTIVITY_STATS (activity)->pressure)
      /* The number of allocated frames exceeds the activity's
	 entitlement.  Encourage it to deallocate.  */
      {
	/* The amount by which we decrease is proportional to
	   pressure.  */
	int dec = frames / 8;
	dec /= 5 - MIN (ACTIVITY_STATS (activity)->pressure, 4);

	debug (0, "Due to pressure (%d), decreasing frames available "
	       "to " OID_FMT " from %d to %d",
	       ACTIVITY_STATS (activity)->pressure,
	       OID_PRINTF (object_to_object_desc ((struct object *)
						  activity)->oid),
	       frames, frames - dec);

	frames -= dec;

	ACTIVITY_STATS (activity)->damping_factor
	  = -1024 * 1024 / PAGESIZE;
      }
    else if (activity->frames_total > 7 * (frames / 8))
      /* The allocated amount is close to the available frames.
	 Encourage it not to allocate.  */
      ACTIVITY_STATS (activity)->damping_factor = 0;
    else
      /* It can allocate.  */
      ACTIVITY_STATS (activity)->damping_factor
	= 1024 * 1024 / PAGESIZE;

    ACTIVITY_STATS (activity)->available = frames;

    bool have_self = false;

    /* The list is sort lowest priority towards the head.  */
    struct activity *child
      = activity_children_list_tail (&activity->children);

    if (! child)
      /* No children.  There is no need to execute the below loop.  */
      ACTIVITY_STATS (activity)->available_local = frames;

    struct activity *p;
    for (;
	 child;
	 /* Only advance to the next child if we actually processed
	    CHILD.  (This can happen when we process ACTIVITY and that
	    has a lower priority than ACTIVITY).  Also do a round for
	    ACTIVITY if it happens to have the highest priority.  */
	 child = p ?: (have_self ? NULL : activity))
      {
	int priority
	  = child ? child->policy.sibling_rel.priority : 0;
	if (! have_self
	    && (activity->policy.child_rel.priority >= priority
		|| child == activity))
	  {
	    have_self = true;

	    /* ACTIVITY could be lower than CHILD.  */
	    priority = activity->policy.child_rel.priority;

	    if (child == activity)
	      child = NULL;
	  }


	/* Find the total allocated frames and weight for this
	   priority level.  */
	uint64_t weight = 0;
	uintptr_t alloced = 0;
	uintptr_t claimed = 0;
	uintptr_t disowned = 0;

	int activity_count = 0;

	void add (int w, int a)
	{
	  weight += w;
	  alloced += a;

	  activity_count ++;
	}

	if (activity->policy.child_rel.priority == priority)
	  add (activity->policy.child_rel.weight,
	       activity->frames_local);

	for (p = child;
	     p && p->policy.sibling_rel.priority == priority;
	     p = activity_children_list_prev (p))
	  {
	    claimed += ACTIVITY_STATS (p)->claimed;
	    disowned += ACTIVITY_STATS (p)->disowned;

	    add (p->policy.sibling_rel.weight,
		 p->frames_total);
	  }


	/* The amount by which the activities exceed their share.  */
	uintptr_t excess = 0;
	/* The amount that they are entitled to use but don't.  */
	uintptr_t unused = 0;

	void params (int my_weight, int my_alloc)
	{
	  uintptr_t share;
	  if (weight == 0)
	    share = 0;
	  else
	    share = ((uint64_t) frames * my_weight) / weight;

	  if (my_alloc > share)
	    excess += my_alloc - share;
	  else
	    unused += share - my_alloc;
	}

	if (activity->policy.child_rel.priority == priority)
	  params (activity->policy.child_rel.weight,
		  activity->frames_local);

	for (p = child;
	     p && p->policy.sibling_rel.priority == priority;
	     p = activity_children_list_prev (p))
	  params (p->policy.sibling_rel.weight, p->frames_total);


	/* Determine the amount of memory available to each
	   activity.  */

	uintptr_t comp_avail (struct activity *activity,
			      int my_weight, int my_alloced,
			      int my_claimed, int my_disowned)
	{
	  uintptr_t share;
	  if (weight == 0)
	    share = 0;
	  else
	    share = ((uint64_t) frames * my_weight) / weight;

	  uintptr_t avail = share;

	  /* The max of our share and our current allocation.  */
	  if (my_alloced > avail)
	    avail = my_alloced;


	  /* The number of frames this activity can steal from other
	     activities in this priority group (i.e., the frames other
	     activities have allocated in excess of their share).  */
	  uintptr_t could_steal = excess;
	  if (weight == 0)
	    could_steal = 0;
	  else
	    {
	      /* We can't steal from ourselves.  */
	      if (my_alloced > share)
		could_steal -= my_alloced - share;
	    }

	  /* If we steal, we steal relative to our current allocation.
	     Not relative to our current share.  Intuition: we are not
	     using anything, other takes are using our share.  What we
	     steal is our share.  If we added the amount to steal to
	     our share, we'd come up with twice our share.  */
	  if (my_alloced + could_steal > share)
	    avail = my_alloced + could_steal;


	  /* The number of frames that this activity can use, which
	     other activities in its priority group have (so far)
	     shown no interest in.  */
	  uintptr_t could_use = unused;
	  if (share >= my_alloced)
	    /* What everone else does not want minus what this
	       activity does not want, i.e., don't count its
	       contribution to unused twice.  */
	    could_use -= share - my_alloced;
	  else
	    {
	      /* The activity contributes nothing to the unused pool.
		 In fact, it is already using some of it.  Subtract
		 that.  */
	      if (unused > my_alloced - share)
		could_use -= my_alloced - share;
	      else
		could_use = 0;
	    }

	  avail += could_use;


	  /* Adjust for allocation trends.  */
	  int delta = (claimed - my_claimed) - (disowned - my_disowned);
	  if (my_alloced >= share && delta > 0)
	    /* This activity's allocation exceeds its share and other
	       activities have claimed more frames than they have
	       disowned.  Assume the same difference in the next
	       period and pay for it, proportionally.  */
	    {
	      if (weight > 0)
		avail -= ((uint64_t) delta * my_weight) / weight;
	      else if (avail > delta)
		avail -= delta;
	      else
		avail = 0;
	    }


	  uintptr_t free;
	  if (frames > alloced)
	    free = frames - alloced;
	  else
	    free = 0;

	  debug (5, OID_FMT ": alloced: %d of %d, "
		 "priority: %d, weight: %d/%lld, prio group frames: %d, "
		 "claimed: %d, disowned: %d, "
		 "share: %d, excess: %d, unused: %d, "
		 "could steal: %d, could use: %d, free: %d, avail: %d",
		 OID_PRINTF (object_to_object_desc ((struct object *)
						    activity)->oid),
		 my_alloced, alloced, priority, my_weight, weight, frames,
		 my_claimed, my_disowned, share, excess, unused,
		 could_steal, could_use, free, avail);
	  return avail;
	}

	if (activity->policy.child_rel.priority == priority)
	  ACTIVITY_STATS (activity)->available_local
	    = comp_avail (activity,
			  activity->policy.child_rel.weight,
			  activity->frames_local,
			  0, 0);

	debug (5, "%d frames for %d activities with priority %d, "
	       "total weight: %lld, alloced: %d, excess: %d",
	       frames, activity_count, priority, weight,
	       alloced, excess);

	for (p = child;
	     p && p->policy.sibling_rel.priority == priority;
	     p = activity_children_list_prev (p))
	  stats (p, comp_avail (p, p->policy.sibling_rel.weight,
				p->frames_total,
				ACTIVITY_STATS (p)->claimed,
				ACTIVITY_STATS (p)->disowned));

	if (frames > alloced)
	  frames -= alloced;
	else
	  frames = 0;
      }

    debug (5, OID_FMT " (s: %d/%d; c: %d/%d): "
	   "%d/%d frames, %d/%d avail (" OID_FMT ")",
	   OID_PRINTF (object_to_object_desc ((struct object *)
					      activity)->oid),
	   activity->policy.sibling_rel.priority,
	   activity->policy.sibling_rel.weight,
	   activity->policy.child_rel.priority,
	   activity->policy.child_rel.weight,
	   activity->frames_local,
	   activity->frames_total,
	   ACTIVITY_STATS (activity)->available_local,
	   ACTIVITY_STATS (activity)->available,
	   OID_PRINTF (activity->parent
		       ? object_to_object_desc ((struct object *)
						activity->parent)->oid
		       : 0));

    activity->current_period ++;
    if (activity->current_period == ACTIVITY_STATS_PERIODS + 1)
      activity->current_period = 0;

    memset (ACTIVITY_STATS (activity),
	    0, sizeof (*ACTIVITY_STATS (activity)));

    /* Wake anyone waiting for this statistic.  */
    struct thread *thread;
    object_wait_queue_for_each (activity, (struct object *) activity,
				thread)
      if (thread->wait_reason == THREAD_WAIT_STATS
	  && thread->wait_reason_arg <= period / FREQ)
	{
	  object_wait_queue_dequeue (activity, thread);

	  /* XXX: Only return valid stat buffers.  */
	  struct activity_stats_buffer buffer;
	  int i;
	  for (i = 0; i < ACTIVITY_STATS_PERIODS; i ++)
	    {
	      int period = activity->current_period - 1 - i;
	      if (period < 0)
		period = (ACTIVITY_STATS_PERIODS + 1) + period;

	      buffer.stats[i] = activity->stats[period];
	    }

	  l4_msg_t msg;
	  rm_activity_stats_reply_marshal (&msg,
					   buffer,
					   ACTIVITY_STATS_PERIODS);
	  l4_msg_tag_t msg_tag = l4_msg_msg_tag (msg);
	  l4_set_propagation (&msg_tag);
	  l4_msg_set_msg_tag (msg, msg_tag);
	  l4_set_virtual_sender (viengoos_tid);
	  l4_msg_load (msg);
	  msg_tag = l4_reply (thread->tid);

	  if (l4_ipc_failed (msg_tag))
	    debug (0, "%s %x failed: %u", 
		   l4_error_code () & 1 ? "Receiving from" : "Sending to",
		   l4_error_code () & 1 ? l4_myself () : thread->tid,
		   (l4_error_code () >> 1) & 0x7);
	}
  }

  stats (root_activity, memory_total - PAGER_LOW_WATER_MARK);

  profile_end ((uintptr_t) &update_stats);
  ss_mutex_unlock (&kernel_lock);
}


void
ager_loop (void)
{
  debug (3, "Ager loop running");

  /* 250 ms (=> ~4Hz).  */
  l4_time_t timeout = l4_time_period (1 << 18);

  int frames = (last_frame - first_frame + PAGESIZE) / PAGESIZE;

  for (;;)
    {
      int frame = 0;

#define BATCH_SIZE (L4_NUM_MRS / 2)
      struct object_desc *descs[BATCH_SIZE];
      struct object *objects[BATCH_SIZE];
      l4_fpage_t fpages[BATCH_SIZE];

      bool also_unmap;

      /* We try to batch calls to l4_unmap, hence the acrobatics.  */

      /* Grab a batch of live objects starting with object I.  */
      int grab (void)
      {
	also_unmap = false;

	int count;
	for (count = 0; frame < frames && count < BATCH_SIZE; frame ++)
	  {
	    struct object_desc *desc = &object_descs[frame];

	    if (! desc->live)
	      /* The object is not live.  */
	      continue;
	    if (desc->eviction_candidate)
	      /* Eviction candidates are unmapped.  Don't waste our
		 time.  */
	      continue;

	    assertx (desc->activity,
		     "OID: " OID_FMT " (%s), age: %d",
		     OID_PRINTF (desc->oid), cap_type_string (desc->type),
		     desc->age);

	    descs[count] = desc;
	    objects[count] = object_desc_to_object (desc);

	    fpages[count] = l4_fpage ((l4_word_t) objects[count], PAGESIZE);

	    if (period % FREQ == 0 && desc->shared)
	      /* We periodically unmap shared frames and mark them as
		 floating.  See above for details.  */
	      {
		if (desc->type == cap_page)
		  /* We only unmap the object if it is a page.  No
		     other objects are actually mapped to users.  */
		  also_unmap = true;
		desc->mapped = false;
		desc->floating = true;
	      }

	    count ++;
	  }

	return count;
      }

      int became_inactive = 0;
      int became_active = 0;

      while (frame < frames)
	{
	  ss_mutex_lock (&kernel_lock);
	  profile_start ((uintptr_t) &ager_loop, "ager");

	  int count = grab ();
	  if (count == 0)
	    {
	      profile_end ((uintptr_t) &ager_loop);
	      ss_mutex_unlock (&kernel_lock);
	      break;
	    }

	  /* Get the status bits for the COUNT objects.  First, we do
	     a flush.  The fpages all have no access bits set so this
	     does not change any mappings and we get the status bits
	     for users as well as ourselves.  Then, if needed, do an
	     unmap.  This is used to unmap any shared mappings but
	     only unmaps from users, not from us.

	     If we were to do this at the same time, we would also
	     change our own mappings.  This is a pain.  Although
	     sigma0 would map them back for the root task's first
	     thread, it does not for subsequent threads.  Moreover, we
	     would have to access the pages at fault time to ensure
	     that they are mapped, which is just ugly.  */
	  profile_start ((uintptr_t) &ager_loop + 1, "l4_unmap");
	  l4_flush_fpages (count, fpages);
	  if (also_unmap)
	    {
	      int i;
	      int j = 0;
	      l4_fpage_t unmap[count];
	      for (i = 0; i < count; i ++)
		if (descs[i]->shared && descs[i]->type == cap_page)
		  unmap[j ++]
		    = l4_fpage_add_rights (fpages[i],
					   L4_FPAGE_FULLY_ACCESSIBLE);
	      assert (j > 0);

	      l4_unmap_fpages (j, unmap);

	      /* Bitwise or the status bits.  */
	      j = 0;
	      for (i = 0; i < count; i ++)
		if (descs[i]->shared && descs[i]->type == cap_page)
		  fpages[i] = l4_fpage_add_rights (fpages[i],
						   l4_rights (unmap[j ++]));
	    }
	  profile_end ((uintptr_t) &ager_loop + 1);

	  int i;
	  for (i = 0; i < count; i ++)
	    {
	      struct object_desc *desc = descs[i];
	      l4_fpage_t fpage = fpages[i];

	      assert (l4_address (fpage)
		      == (uintptr_t) object_desc_to_object (desc));

	      l4_word_t rights = l4_rights (fpage);
	      int dirty = !!(rights & L4_FPAGE_WRITABLE);
	      int referenced = !!(rights & L4_FPAGE_READABLE);
	      if (dirty)
		debug (5, "%p is dirty", object_desc_to_object (desc));

	      desc->user_dirty |= dirty;
	      desc->user_referenced |= referenced;

	      if (dirty)
		/* Dirty implies referenced.  */
		assert (referenced);

	      if (object_active (desc))
		/* The object was active.  */
		{
		  assert (desc->activity);
		  assert (desc->age);

		  desc->dirty |= dirty;

		  object_age (desc, referenced);

		  if (! object_active (desc)
		      && desc->policy.priority == OBJECT_PRIORITY_LRU)
		    /* The object has become inactive and needs to be
		       moved.  */
		    {
		      ACTIVITY_STAT_UPDATE (desc->activity,
					    became_inactive, 1);

		      became_inactive ++;

		      /* Detach from active list.  */
		      activity_lru_list_unlink (&desc->activity->active,
						desc);

		      activity_lru_list_push (&desc->activity->inactive, desc);
		    }
		  else
		    {
		      ACTIVITY_STATS (desc->activity)->active_local ++;
		      ACTIVITY_STAT_UPDATE (desc->activity, active, 1);
		    }
		}
	      else
		/* The object was inactive.  */
		{
		  object_age (desc, referenced);

		  if (referenced)
		    /* The object has become active.  */
		    {
		      ACTIVITY_STAT_UPDATE (desc->activity,
					    became_active, 1);

		      became_active ++;

		      if (desc->policy.priority == OBJECT_PRIORITY_LRU)
			{
			  /* Detach from inactive list.  */
			  activity_lru_list_unlink
			    (&desc->activity->inactive, desc);

			  /* Attach to active list.  */
			  activity_lru_list_push (&desc->activity->active,
						  desc);
			}

		      desc->dirty |= dirty;
		    }
		}

	      if (desc->dirty && ! desc->policy.discardable)
		ACTIVITY_STAT_UPDATE (desc->activity, dirty, 1);
	      else
		ACTIVITY_STAT_UPDATE (desc->activity, clean, 1);
	    }

	  profile_end ((uintptr_t) &ager_loop);
	  ss_mutex_unlock (&kernel_lock);
	}

      /* Update statistics every two seconds.  */
      if (period % FREQ == 0)
	{
	  update_stats ();

	  do_debug (1)
	    {
	      int a = zalloc_memory + available_list_count (&available);
	      debug (0, "%d: %d of %d (%d%%) free; laundry: %d; "
		     "%d became inactive, %d became active",
		     period / FREQ,
		     a, memory_total, (a * 100) / memory_total,
		     laundry_list_count (&laundry),
		     became_inactive, became_active);
	    }
	}
      period ++;

      /* Wait TIMEOUT or until we are interrupted by the main
	 thread.  */
      l4_receive_timeout (viengoos_tid, timeout);
    }
}
