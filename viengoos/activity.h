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

/* Forward.  */
struct object_desc;
struct thread;

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
  /* List of in-memory children.  Children that are not in memory are
     not on this list.  When an activity is paged in, activity_prepare
     attaches it to its parent's children list.  On page-out,
     activity_deprepare detaches it.  */
  struct activity *children;
  struct activity *sibling_next;
  struct activity *sibling_prev;

  /* Objects owned by this activity.  */
  struct object_activity_lru_list active;
  struct object_activity_lru_list inactive_clean;
  struct object_activity_lru_list inactive_dirty;

  /* All objects owned by this activity whose priority is not
     OBJECT_PRIORITY_LRU, keyed by priority.  */
  hurd_btree_priorities_t priorities;

  uint32_t frames_local;
  /* Number of frames allocated to this activity (including children).
     This is the sum of the number of objects on ACTIVE,
     INACTIVE_CLEAN and INACTIVE_DIRTY plus the number of frames
     allocated to each child.  */
  uint32_t frames_total;

  int dying;
};

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
  activity->frames_local += objects;
  activity_for_each_ancestor (activity,
			      ({
				assert (activity->frames_total >= 0);
				activity->frames_total += objects;
				assert (activity->frames_total >= 0);
			      }));
}

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

/* Iterate over ACTIVITY's children which are in memory.  */
#define activity_for_each_inmemory_child(__feic_activity, __feic_child,	\
					 __feic_code)			\
  do									\
    {									\
      for (__feic_child = __feic_activity->children; __feic_child;	\
	   __feic_child = __feic_child->sibling_next)			\
	{								\
	  assert (__feic_child->parent == __feic_activity);		\
	  assert (object_type ((struct object *) __feic_child)		\
		  == cap_activity_control);				\
									\
	  __feic_code;							\
	}								\
    }									\
  while (0)


/* Dump the activity ACTIVITY and its children to the screen.  */
extern void activity_dump (struct activity *activity);

#endif
