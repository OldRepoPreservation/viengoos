/* activity.h - Activity object implementation.
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

#ifndef RM_ACTIVITY_H
#define RM_ACTIVITY_H

#include <l4.h>
#include <errno.h>
#include <hurd/btree.h>

#include "cap.h"
#include "object.h"

/* Forward.  */
struct object_desc;
struct thread;

struct activity
{
  /* On-disk data.  */

  /* Parent activity.  */
  struct cap parent;

  /* List of child activities (if any).  Threaded via
     SIBLING_NEXT.  */
  struct cap children;

  /* This activity's siblings.  */
  struct cap sibling_next;
  struct cap sibling_prev;

  /* Head of the linked list of folios allocated to this activity.  */
  struct cap folios;

  /* Parent assigned values.  */
  /* Memory.  */
  l4_word_t priority;
  l4_word_t weight;
  /* Maximum number of folios this activity may allocate.  0 means no
     limit.  */
  l4_word_t storage_quota;

  /* Number of folios allocated to this activity (including
     children).  */
  l4_word_t folio_count;

  /* Cached location of the in-memory parent activity.  This must be
     set on page-in.  It is only NULL for the root activity.  */
  struct activity *parent_ptr;

  /* Objects owned by this activity.  */
  struct object_desc *active;
  struct object_desc *inactive_clean;
  struct object_desc *inactive_dirty;

  /* All objects owned by this activity whose priority is not
     OBJECT_PRIORITY_LRU, keyed by priority.  */
  hurd_btree_priorities_t priorities;

  /* Number of frames allocated to this activity (including children).
     This is the sum of the number of objects on ACTIVE,
     INACTIVE_CLEAN and INACTIVE_DIRTY plus the number of frames
     allocated to each child.  */
  uint32_t frames;

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

/* Starting with ACTIVITY and for each direct ancestor execute CODE.
   Modifies ACTIVITY.  */
#define activity_for_each_ancestor(__fea_activity, __fea_code)		\
  do {									\
    assert (__fea_activity);						\
    do									\
      {									\
	__fea_code;							\
									\
	if (! __fea_activity->parent_ptr)				\
	  assert (__fea_activity == root_activity);			\
									\
	__fea_activity = __fea_activity->parent_ptr;			\
      }									\
    while (__fea_activity);						\
  } while (0)

/* Charge activity ACTIVITY for OBJECTS objects.  OBJECTS may be
   negative, in which case the charge is a credit.  */
static inline void
activity_charge (struct activity *activity, int objects)
{
  assert (activity);
  activity_for_each_ancestor (activity,
			      ({
				assert (activity->frames >= 0);
				activity->frames += objects;
				assert (activity->frames >= 0);
			      }));
}

/* For each child of ACTIVITY, set to CHILD and execute code.  The
   caller may destroy CHILD, however, it may not destroy any
   siblings.  */
#define activity_for_each_child(__fec_activity, __fec_child, __fec_code) \
  do {									\
    __fec_child								\
      = (struct activity *) cap_to_object ((__fec_activity),		\
					   &(__fec_activity)->children); \
    while (__fec_child)							\
      {									\
	/* Grab the next child incase this child is destroyed.  */	\
	struct cap __fec_next = __fec_child->sibling_next;		\
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


/* Perform a consistency checl on the activity ACTIVITY.  Will take
   LRU_LOCK.  */
extern void activity_consistency_check_ (const char *func, int line,
					 struct activity *activity);
#define activity_consistency_check(a)		\
  activity_consistency_check_ (__func__, __LINE__, (a))

#endif
