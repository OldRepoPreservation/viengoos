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

#include "cap.h"

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

  /* The remainder of the elements are in-memory only.  */

  /* Head of list of objects owned by this activity.  */
  struct object_desc *objects;

  /* Number of frames allocated to this activity (including
     children).  */
  int frames;
};

/* Allocate a new activity.  Charge to activity PARENT, which is the
   parent.  FOLIO specifies the capability slot in CALLER's address
   space that contains the folio to use to allocate the storage and
   INDEX specifies which in the folio to use.  ACTIVITY and CONTROL
   specify where to store the capabilities designating the new
   activity and the activity's control capability, respectively.
   PRIORITY, WEIGHT and STORAGE_QUOTA are the initial priority and
   weight of the activity.  */
extern error_t activity_allocate (struct activity *parent,
				  struct thread *caller,
				  addr_t folio, l4_word_t index,
				  addr_t activity, addr_t control,
				  l4_word_t priority, l4_word_t weight,
				  l4_word_t storage_quota);

extern void activity_destroy (struct activity *activity,
			      struct cap *cap, struct activity *target);

#endif
