/* ager.c - Ager loop implementation.
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

#include <l4/space.h>
#include <l4/schedule.h>
#include <hurd/mutex.h>

#include <assert.h>

#include "ager.h"
#include "object.h"
#include "activity.h"

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

/* The amount to age a frame when it is found to be active.  */
#define AGE_DELTA (1 << 8)

void
ager_loop (l4_thread_id_t main_thread)
{
  debug (3, "Ager loop running");

  /* 125 ms (=> ~8Hz).  */
  l4_time_t timeout = l4_time_period (1 << 17);

#if UNMAP_PERIODICALLY
  int iterations = 0;
#endif

  for (;;)
    {
      int active = 0;
      int inactive = 0;
      int retired = 0;
      int revived = 0;

      /* XXX: This lock could be held for a while.  It would be much
	 better to grab it and release it as required.  This
	 complicates the code a bit and requires some thought.  */
      ss_mutex_lock (&lru_lock);

#if !UNMAP_INACTIVE
      /* We are going to move inactive items on the global active to
	 the appropriate inactive list.  Then we scan the inactive
	 lists for referenced objects.  We want to avoid querying
	 pages twice, hence we move the inactive lists.  */
      struct object_desc *old_inactive_clean;
      struct object_desc *old_inactive_dirty;

      object_global_lru_move (&old_inactive_clean, &global_inactive_clean);
      object_global_lru_move (&old_inactive_dirty, &global_inactive_dirty);
#endif

#define BATCH_SIZE (L4_NUM_MRS / 2)
      struct object_desc *descs[BATCH_SIZE];
      struct object *objects[BATCH_SIZE];
      l4_fpage_t fpages[BATCH_SIZE];

      /* We try to batch calls to l4_unmap, hence the acrobatics.  */

      /* Grab a batch of records starting with *PTR.  Changes *PTR to
	 point to the next unprocessed record.  */
      int grab (struct object_desc **ptr)
      {
	int count = 0;
	for (; *ptr && count < BATCH_SIZE; *ptr = (*ptr)->global_lru.next)
	  {
	    if (! ss_mutex_trylock (&(*ptr)->lock))
	      /* We failed to get the lock.  This means that someone is
		 using this object and thus it makes no sense to
		 age it; just continue.  */
	      continue;

	    descs[count] = (*ptr);
	    objects[count] = object_desc_to_object (descs[count]);
	    fpages[count] = l4_fpage ((l4_word_t) objects[count], PAGESIZE);

	    count ++;
	  }

	return count;
      }

      struct object_desc *ptr = global_active;
      for (;;)
	{
	  int count = grab (&ptr);
	  if (count == 0)
	    break;

	  active += count;

	  /* Get the status bits for the COUNT objects.  (We flush
	     rather than unmap as we are also interested in whether we
	     have accessed the object--this occurs as we access some
	     objects, e.g., cappages, on behalf activities or we have
	     flushed a page of data to disk.) */
	  l4_flush_fpages (count, fpages);

	  int i;
	  for (i = 0; i < count; i ++)
	    {
	      struct object_desc *desc = descs[i];
	      l4_fpage_t fpage = fpages[i];

	      l4_word_t rights = l4_rights (fpage);
	      int dirty = (rights & L4_FPAGE_WRITABLE);
	      int referenced = (rights & L4_FPAGE_READABLE);

	      desc->dirty = desc->dirty || dirty;

	      assert (desc->age);

	      if (! referenced)
		{
#if UNMAP_INACTIVE
		  l4_fpage_t f;
		  f = l4_fpage_add_rights (fpage, L4_FPAGE_FULLY_ACCESSIBLE);
		  l4_unmap_fpage (f);
#endif

		  desc->age >>= 1;

		  if (desc->age == 0)
		    {
		      /* Move to the appropriate inactive lists.  */
		      object_global_lru_unlink (desc);
		      if (desc->dirty)
			object_global_lru_link (&global_inactive_dirty, desc);
		      else
			object_global_lru_link (&global_inactive_clean, desc);

		      /* If the object is owned, then move it to its
			 activity's inactive list.  Otherwise, the
			 page is on the disowned list in which case we
			 needn't do anything.  */
		      if (desc->activity)
			{
			  object_activity_lru_unlink (desc);
			  if (desc->dirty)
			    object_activity_lru_link
			      (&desc->activity->inactive_dirty, desc);
			  else
			    object_activity_lru_link
			      (&desc->activity->inactive_clean, desc);
			}
		    }

		  retired ++;
		}
	      else
		{
		  /* Be careful with wrap around.  */
		  if ((typeof (desc->age)) (desc->age + AGE_DELTA) > desc->age)
		    desc->age += AGE_DELTA;
		}

	      ss_mutex_unlock (&desc->lock);
	    }
	}

#if !UNMAP_INACTIVE
      struct object_desc *lists[] = { old_inactive_clean,
				      old_inactive_dirty };
      int j;
      for (j = 0; j < 2; j ++)
	{
	  ptr = lists[j];

	  for (;;)
	    {
	      int count = grab (&ptr);
	      if (count == 0)
		break;

	      inactive += count;

	      /* Get the status bits for the COUNT objects.  (We flush
		 rather than unmap as we are also interested in whether we
		 have accessed the object--this occurs as we access some
		 objects, e.g., cappages, on behalf activities or we have
		 flushed a page of data to disk.) */
	      l4_flush_fpages (count, fpages);

	      int i;
	      for (i = 0; i < count; i ++)
		{
		  struct object_desc *desc = descs[i];
		  l4_fpage_t fpage = fpages[i];

		  l4_word_t rights = l4_rights (fpage);

		  int dirty = (rights & L4_FPAGE_WRITABLE);
		  int referenced = (rights & L4_FPAGE_READABLE);
		  if (dirty)
		    assert (referenced);

		  desc->dirty = desc->dirty ? : dirty;

		  if (referenced)
		    {
		      revived ++;

		      desc->age = AGE_DELTA;

		      /* Move to the active list.  */
		      object_global_lru_unlink (desc);
		      object_global_lru_link (&global_active, desc);

		      /* If the object is not owned, then it is on the
			 disowned list.  It's unusual that we should
			 get a reference on an unowned object, but,
			 that can happen.  */
		      if (desc->activity)
			{
			  object_activity_lru_unlink (desc);
			  object_activity_lru_link (&desc->activity->active,
						    desc);
			}
		    }

		  ss_mutex_unlock (&desc->lock);
		}
	    }
	}

      object_global_lru_join (&global_inactive_clean, &old_inactive_clean);
      object_global_lru_join (&global_inactive_dirty, &old_inactive_dirty);
#endif

      ss_mutex_unlock (&lru_lock);

#if UNMAP_PERIODICALLY
      if (iterations == 8 * 5)
	{
	  debug (1, "Unmapping all. %d active, %d inactive, last interation "
		 "retired: %d, revived: %d",
		 active, inactive, retired, revived);

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
