/* cap.h - Basic capability framework interface.
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

#ifndef RM_CAP_H
#define RM_CAP_H

#include <l4.h>
#include <viengoos/cap.h>

/* The number of slots in a capability object of the given type.  */
extern const int cap_type_num_slots[];

/* Set's the capability TARGET to point to the same object as the
   capability SOURCE, however, preserves the guard in TARGET.  */
static inline bool
cap_set (struct activity *activity, struct vg_cap *target, struct vg_cap source)
{
  /* This is kosher as we know the implementation of CAP_COPY.  */
  return vg_cap_copy_simple (activity,
			     VG_ADDR_VOID, target, VG_ADDR_VOID,
			     VG_ADDR_VOID, source, VG_ADDR_VOID);
}

/* Invalidate all mappings that may depend on this object.  */
extern void cap_shootdown (struct activity *activity, struct vg_cap *cap);

/* Return the object designated by CAP, if any.  */
struct vg_object *vg_cap_to_object (struct activity *activity,
				    struct vg_cap *cap);

/* Like vg_cap_to_object but only returns the object if it is in
   memory.  */
struct vg_object *cap_to_object_soft (struct activity *activity,
				      struct vg_cap *cap);



#endif
