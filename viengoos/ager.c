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
#include <hurd/mutex.h>

#include <assert.h>

#include "ager.h"
#include "object.h"
#include "activity.h"
#include "zalloc.h"

/* When frames are shared amoung multiple activities, the first
   activity to access the frame is charged.  This is unfair; the cost
   needs to be distributed among all users.  We approximate this by
   changing the owner of the frame on a hard fault.  The problem is
   how to observe these faults: l4_unmap does not tell us who faulted
   (indeed, which would be insufficient for us anyways--we need an
   activity).

   When a new user comes along, this is not a problem: the new user
   faults and the ownership changes.

   When there are multiple users and the main user becomes inactive,
   no minor faults will be observed.

   There are a couple of ways to see these faults: we unmap the frame
   when it becomes inactive.  This will eventually happen unless the
   page is really hammered.  This is the strategy we use when
   UNMAP_INACTIVE is non-zero.

   We can periodically unmap all pages.  This is nice as ownership
   will regularly change hands for all frames, even those that don't
   become inactive.  This helps to distribute the costs.  This is the
   strategy we use when UNMAP_PERIODICALLY is non-zero.  */
#define UNMAP_INACTIVE 0
#define UNMAP_PERIODICALLY 1

void
ager_loop (l4_thread_id_t main_thread)
{
  debug (3, "Ager loop running");

  /* 125 ms (=> ~8Hz).  */
  l4_time_t timeout = l4_time_period (1 << 17);

#if UNMAP_PERIODICALLY
  int iterations = 0;
#endif

  int frames = (last_frame - first_frame + PAGESIZE) / PAGESIZE;

  for (;;)
    {
      int frame = 0;

#define BATCH_SIZE (L4_NUM_MRS / 2)
      struct object_desc *descs[BATCH_SIZE];
      struct object *objects[BATCH_SIZE];
      l4_fpage_t fpages[BATCH_SIZE];

      /* We try to batch calls to l4_unmap, hence the acrobatics.  */

      /* Grab a batch of live objects starting with object I.  */
      int grab (void)
      {
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

	    if (! ss_mutex_trylock (&desc->lock))
	      /* We failed to get the lock.  This means that
		 someone is using this object and thus it makes no
		 sense to age it; just continue.  */
	      continue;

	    if (! desc->live || desc->eviction_candidate)
	      /* State changed between check and lock acquisition,
		 unlock and continue.  */
	      {
		ss_mutex_unlock (&desc->lock);
		continue;
	      }

	    assertx (desc->activity,
		     "OID: " OID_FMT " (%s), age: %d",
		     OID_PRINTF (desc->oid), cap_type_string (desc->type),
		     desc->age);

	    descs[count] = desc;
	    objects[count] = object_desc_to_object (desc);
	    fpages[count] = l4_fpage ((l4_word_t) objects[count], PAGESIZE);

	    count ++;
	  }

	return count;
      }

      int retired = 0;
      int revived = 0;

      while (frame < frames)
	{
	  int count = grab ();
	  if (count == 0)
	    break;

	  /* Get the status bits for the COUNT objects.  (We flush
	     rather than unmap as we are also interested in whether we
	     have accessed the object--this occurs as we access some
	     objects, e.g., cappages, on behalf activities or we have
	     flushed a page of data to disk.) */
	  l4_flush_fpages (count, fpages);

	  ss_mutex_lock (&lru_lock);

	  int i;
	  for (i = 0; i < count; i ++)
	    {
	      struct object_desc *desc = descs[i];
	      l4_fpage_t fpage = fpages[i];

	      l4_word_t rights = l4_rights (fpage);
	      int dirty = (rights & L4_FPAGE_WRITABLE);
	      int referenced = (rights & L4_FPAGE_READABLE);

	      if (dirty)
		/* Dirty implies referenced.  */
		assert (referenced);

	      if (object_active (desc))
		/* The object was active.  */
		{
		  assert (desc->activity);

		  desc->dirty |= dirty;

		  object_age (desc, referenced);

		  if (! object_active (desc)
		      && desc->policy.priority == OBJECT_PRIORITY_LRU)
		    /* The object has become inactive and needs to be
		       moved.  */
		    {
		      retired ++;

		      /* Detach from active list.  */
		      activity_lru_list_unlink (&desc->activity->active,
						desc);

		      /* Attach to appropriate inactive list.  */
		      if (desc->dirty && ! desc->policy.discardable)
			activity_lru_list_push
			  (&desc->activity->inactive_dirty, desc);
		      else
			activity_lru_list_push
			  (&desc->activity->inactive_clean, desc);

#if UNMAP_INACTIVE
		      l4_fpage_t f;
		      f = l4_fpage_add_rights (fpage,
					       L4_FPAGE_FULLY_ACCESSIBLE);
		      l4_unmap_fpage (f);
#endif
		    }
		}
	      else
		/* The object was inactive.  */
		{
		  object_age (desc, referenced);

		  if (referenced)
		    /* The object has become active.  */
		    {
		      revived ++;

		      if (desc->policy.priority == OBJECT_PRIORITY_LRU)
			{
			  /* Detach from inactive list.  */
			  if (desc->dirty && ! desc->policy.discardable)
			    activity_lru_list_unlink
			      (&desc->activity->inactive_dirty, desc);
			  else
			    activity_lru_list_unlink
			      (&desc->activity->inactive_clean, desc);

			  /* Attach to active list.  */
			  activity_lru_list_push (&desc->activity->active,
						  desc);
			}

		      desc->dirty |= dirty;
		    }
		}

	      ss_mutex_unlock (&desc->lock);
	    }

	  ss_mutex_unlock (&lru_lock);
	}

#if UNMAP_PERIODICALLY
      if (iterations == 8 * 5)
	{
	  debug (1, "Unmapping all (%d of %d free). "
		 "last interation retired: %d, revived: %d",
		 zalloc_memory, memory_total, retired, revived);

	  l4_unmap_fpage (l4_fpage_add_rights (L4_COMPLETE_ADDRESS_SPACE,
					       L4_FPAGE_FULLY_ACCESSIBLE));
	  iterations = 0;
	}
      else
	iterations ++;
#endif

      /* Wait TIMEOUT or until we are interrupted by the main
	 thread.  */
      l4_receive_timeout (main_thread, timeout);
    }
}
