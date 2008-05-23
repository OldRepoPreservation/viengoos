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
#include "object.h"
#include "activity.h"
#include "zalloc.h"

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

void
ager_loop (l4_thread_id_t main_thread)
{
  debug (3, "Ager loop running");

  /* 125 ms (=> ~8Hz).  */
  l4_time_t timeout = l4_time_period (1 << 17);

  int iterations = 0;

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

	    if (iterations % FREQ == 0 && desc->shared)
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

	  int count = grab ();
	  if (count == 0)
	    {
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
	  l4_flush_fpages (count, fpages);
	  if (also_unmap)
	    {
	      /* XXX: This is a bit more aggressive than required.
		 Instead, we should only unmap the shared pages.  */
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
		      ACTIVITY_STAT_UPDATE (desc->activity, became_inactive, 1);

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
		      ACTIVITY_STAT_UPDATE (desc->activity, became_active, 1);

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

	  ss_mutex_unlock (&kernel_lock);
	}

      /* Update statistics every two seconds.  */
      if (iterations % FREQ == 0)
	{
	  ss_mutex_lock (&kernel_lock);

	  /* XXX: Update the statistics.  We need to average some of
	     the fields including the number of active, inactive,
	     clean and dirty pages.  Also, we need to calculate each
	     activity's allocation, a damping factor and the
	     pressure.  */
	  void doit (struct activity *activity, uint32_t frames)
	  {
	    ACTIVITY_STATS (activity)->period = iterations / FREQ;

	    ACTIVITY_STATS (activity)->available = frames;

	    bool have_self = false;

	    bool have_one = false;
	    int priority;
	    uint32_t remaining_frames = frames;

	    struct activity *child;
	    for (child = activity_children_list_head (&activity->children);
		 child;
		 child = activity_children_list_next (child))
	      {
		if (! have_self
		    && (activity->policy.child_rel.priority
			<= child->policy.sibling_rel.priority))
		  {
		    have_self = true;

		    if (! have_one
			|| priority > activity->policy.child_rel.priority)
		      {
			priority = activity->policy.child_rel.priority;
			frames = remaining_frames;
		      }

		    remaining_frames -= activity->frames_local;
		  }

		if (! have_one
		    || priority > child->policy.sibling_rel.priority)
		  {
		    priority = child->policy.sibling_rel.priority;
		    frames = remaining_frames;
		  }

		remaining_frames -= child->frames_total;

		doit (child, frames);
	      }

	    ACTIVITY_STATS (activity)->clean /= FREQ;
	    ACTIVITY_STATS (activity)->dirty /= FREQ;
	    ACTIVITY_STATS (activity)->active /= FREQ;
	    ACTIVITY_STATS (activity)->active_local /= FREQ;

	    activity->current_period ++;
	    if (activity->current_period == ACTIVITY_STATS_PERIODS + 1)
	      activity->current_period = 0;

	    memset (ACTIVITY_STATS (activity),
		    0, sizeof (*ACTIVITY_STATS (activity)));
	  }

	  doit (root_activity, memory_total);

	  do_debug (1)
	    {
	      int a = zalloc_memory + available_list_count (&available);
	      debug (0, "%d of %d (%d%%) free; "
		     "since last interation: %d became inactive, %d active",
		     a, memory_total, (a * 100) / memory_total,
		     became_inactive, became_active);
	    }

	  ss_mutex_unlock (&kernel_lock);
	}

      iterations ++;

      /* Wait TIMEOUT or until we are interrupted by the main
	 thread.  */
      l4_receive_timeout (main_thread, timeout);
    }
}
