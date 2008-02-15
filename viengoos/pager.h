/* pager.h - Pager interface.
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

#ifndef VIENGOOS_PAGER_H
#define VIENGOOS_PAGER_H

#include "memory.h"
#include "zalloc.h"
#include "object.h"

/* Try to make an additional GOAL frames available.  Returns the
   number of frames scheduled for page-out or placed on the eviction
   list.  */
extern int pager_collect (int goal);

/* We try to keep at least 1/8 (12.5%) of memory available for
   immediate allocation.  */
#define PAGER_LOW_WATER_MARK (memory_total / 8)
/* When we start freeing, we try to get at least 3/16 (~19%) of memory
   available for immediate allocation.  */
#define PAGER_HIGH_WATER_MARK				\
  (PAGER_LOW_WATER_MARK + PAGER_LOW_WATER_MARK / 2)

/* Returns the number of frames that need to be collected.  Returns 0
   if there is no memory pressure.  If non-zero the caller should
   follow up with a call to pager_collect.  */
static inline int
pager_collect_needed (void)
{
  int available_pages = zalloc_memory
    + available_list_count (&available)
    /* We only count the pages on the laundry half as they won't be
       available immediately.  */
    + laundry_list_count (&laundry) / 2;

  if (available_pages > PAGER_LOW_WATER_MARK)
    return 0;

  return PAGER_HIGH_WATER_MARK - available_pages;
}

static inline void
pager_query (void)
{
  int goal = pager_collect_needed ();
  if (unlikely (goal > 0))
    pager_collect (goal);
}

#endif
