/* cap.h - Basic capability framework interface.
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

#ifndef RM_CAP_H
#define RM_CAP_H

#include <l4.h>
#include <hurd/cap.h>

/* The number of slots in a capability object of the given type.  */
extern const int cap_type_num_slots[];

/* Set's the capability TARGET to point to the same object as the
   capability SOURCE, however, preserves the guard in TARGET.  */
static inline bool
cap_set (struct activity *activity, struct cap *target, struct cap source)
{
  /* This is kosher as we know the implementation of CAP_COPY.  */
  return cap_copy (activity,
		   ADDR_VOID, target, ADDR_VOID,
		   ADDR_VOID, source, ADDR_VOID);
}

/* Invalidate all mappings that may depend on this object.  */
extern void cap_shootdown (struct activity *activity, struct cap *cap);

#endif
