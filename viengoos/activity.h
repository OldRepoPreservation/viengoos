/* activity.h - Activity object implementation.
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

#ifndef RM_ACTIVITY_H
#define RM_ACTIVITY_H

#include <hurd/activity.h>

#include "cap.h"
#include "object.h"
#include "list.h"

/* Forward.  */
struct object_desc;
struct thread;

LIST_CLASS_TYPE(activity_children)

struct activity
{
  /* On-disk data.  */

  /* Parent activity.  */
  struct cap parent_cap;

  /* List of child activities (if any).  Threaded via
     SIBLING_NEXT.  */
  struct cap children_cap;

  /* This activity's siblings.  */
  struct cap sibling_next_cap;
  struct cap sibling_prev_cap;

  /* Head of the linked list of folios allocated to this activity.  */
  struct cap folios;

  /* Policy.  */
  struct activity_policy policy;

  /* Number of folios allocated to this activity (including
     children).  */
  uint32_t folio_count;

  /* Location of the in-memory parent activity.  An activity may only
     be in memory if its parent is in memory.  It is only NULL for the
     root activity.  This pointer is setup by activity_prepare.  */
  struct activity *parent;
  /* List of in-memory children, sorted highest priority first.
     Children that are not in memory are not on this list.  When an
     activity is paged in, activity_prepare attaches it to its
     parent's children list.  On page-out, activity_deprepare detaches
     it.  */
  struct activity_children_list children;
  struct list_node sibling;

  /* Objects owned by this activity whose priority is
     OBJECT_PRIORITY_LRU and for which DESC->EVICTION_CANDIDATE is
     false.  */
  struct activity_lru_list active;
  struct activity_lru_list inactive_clean;
  struct activity_lru_list inactive_dirty;

  /* Objects owned by this activity whose priority is not
     OBJECT_PRIORITY_LRU and for which DESC->EVICTION_CANDIDATE is
     false.  Keyed by priority.  */
  hurd_btree_priorities_t priorities;

  /* Objects that are owned by this activity and have been selected
     for eviction (DESC->EVICTION_CANDIDATE is true).  These objects
     do not appear on the active or inactive lists and do not
     contribute to frames_local or frames_total.  */
  struct eviction_list eviction_clean;
  struct eviction_list eviction_dirty;

  /* Number of frames allocated to this activity not counting
     children.  */
  uint32_t frames_local;
  /* Number of frames allocated to this activity (including children).
     This is the sum of the number of objects on ACTIVE,
     INACTIVE_CLEAN and INACTIVE_DIRTY plus the number of frames
     allocated to each child.  This does not include the number of
     frames on the eviction_clean and eviction_dirty lists.  */
  uint32_t frames_total;

  /* Whether the activity has been marked as dead (and thus will be
     shortly deallocated).  */
  unsigned char dying;

  /* Statistics.  */
  /* The current period.  */
  unsigned char current_period;
  struct activity_stats stats[ACTIVITY_STATS_PERIODS + 1];
};

LIST_CLASS(activity_children, struct activity, sibling, false)

/* Return the current activity stat structure.  */
#define ACTIVITY_STATS(__as_activity)					\
  ({									\
    assert ((__as_activity)->current_period				\
	    < (sizeof ((__as_activity)->stats)				\
	       / sizeof ((__as_activity)->stats[0])));			\
									\
    &(__as_activity)->stats[(__as_activity)->current_period];		\
  })

/* Return the last valid stat structure.  */
#define ACTIVITY_STATS_LAST(__asl_activity)				\
  ({									\
    int __asl_period = (__asl_activity)->current_period - 1;		\
    if (__asl_period == -1)						\
      __asl_period = (sizeof ((__asl_activity)->stats)			\
		      / sizeof ((__asl_activity)->stats[0])) - 1;	\
									\
    assert (__asl_period < (sizeof ((__asl_activity)->stats)		\
			    / sizeof ((__asl_activity)->stats[0])));	\
									\
    &(__asl_activity)->stats[__asl_period];				\
  })

/* The root activity.  */
extern struct activity *root_activity;

/* Initialize an activity.  Charge to activity PARENT, which is the
   parent.  FOLIO specifies the capability slot in CALLER's address
   space that contains the folio to use to allocate the storage and
   INDEX specifies which in the folio to use.  ACTIVITY and CONTROL
   specify where to store the capabilities designating the new
   activity and the activity's control capability, respectively.
   PRIORITY, WEIGHT and STORAGE_QUOTA are the initial priority and
   weight of the activity.  */
extern void activity_create (struct activity *parent,
			     struct activity *child);

/* The ACTIVITY activity destroys the activity VICTIM.  */
extern void activity_destroy (struct activity *activity,
			      struct activity *victim);

/* Call when bringing an activity into memory.  */
extern void activity_prepare (struct activity *principal,
			      struct activity *activity);

/* Call just before paging activity ACTIVITY out.  */
extern void activity_deprepare (struct activity *principal,
				struct activity *victim);

/* Set ACTIVITY's policy to POLICY.  */
extern void activity_policy_update (struct activity *activity,
				    struct activity_policy policy);


/* Starting with ACTIVITY and for each direct ancestor execute CODE.
   Modifies ACTIVITY.  */
#define activity_for_each_ancestor(__fea_activity, __fea_code)		\
  do {									\
    assert (__fea_activity);						\
    do									\
      {									\
	__fea_code;							\
									\
	if (! __fea_activity->parent)					\
	  assert (__fea_activity == root_activity);			\
									\
	__fea_activity = __fea_activity->parent;			\
      }									\
    while (__fea_activity);						\
  } while (0)

/* Charge activity ACTIVITY for OBJECTS objects.  OBJECTS may be
   negative, in which case the charge is a credit.  */
static inline void
activity_charge (struct activity *activity, int objects)
{
  assert (activity);
  if (objects < 0)
    assertx (-objects <= activity->frames_local,
	     "%d <= %d", -objects, activity->frames_local);

  activity->frames_local += objects;
  activity_for_each_ancestor (activity,
			      ({
				if (objects < 0)
				  assertx (-objects <= activity->frames_total,
					   "%d <= %d",
					   -objects, activity->frames_total);
				activity->frames_total += objects;
			      }));
}

#define ACTIVITY_STAT_UPDATE(__asu_activity, __asu_field, __asu_delta)	\
  do									\
    {									\
      struct activity *__asu_a = (__asu_activity);			\
      activity_for_each_ancestor					\
	(__asu_a,							\
	 ({								\
	   ACTIVITY_STATS (__asu_a)->__asu_field += __asu_delta;	\
	 }));								\
    }									\
  while (0)

/* For each child of ACTIVITY, set to CHILD and execute code.  The
   caller may destroy CHILD, however, it may not destroy any siblings.
   Be careful of deadlock: this function calls cap_to_object, which
   calls object_find, which may take LRU_LOCK.  */
#define activity_for_each_child(__fec_activity, __fec_child, __fec_code) \
  do {									\
    __fec_child								\
      = (struct activity *) cap_to_object ((__fec_activity),		\
					   &(__fec_activity)->children_cap); \
    while (__fec_child)							\
      {									\
	/* Grab the next child incase this child is destroyed.  */	\
	struct cap __fec_next = __fec_child->sibling_next_cap;		\
									\
	__fec_code;							\
									\
	/* Fetch the next child.  */					\
	__fec_child = (struct activity *) cap_to_object ((__fec_activity), \
							 &__fec_next);	\
	if (! __fec_child)						\
	  break;							\
      }									\
  } while (0)


/* Dump the activity ACTIVITY and its children to the screen.  */
extern void activity_dump (struct activity *activity);

#endif
