/* as-custom.c - Address space composition helper functions.
   Copyright (C) 2008 Free Software Foundation, Inc.
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

#include <hurd/activity.h>
#include <hurd/addr.h>
#include <hurd/cap.h>

/* PT designates a cappage or a folio.  The cappage or folio is at
   address PT_ADDR.  Index the object designed by PTE returning the
   location of the idx'th capability slot.  If the capability is
   implicit (in the case of a folio), return a fabricated capability
   in *FAKE_SLOT and return FAKE_SLOT.  Return NULL on failure.  */
typedef struct cap *(*as_object_index_t) (activity_t activity,
					  struct cap *pt,
					  addr_t pt_addr, int idx,
					  struct cap *fake_slot);

/* Expose as_slot_ensure_full_custom and as_insert_custom.  */
#define ID_SUFFIX custom
#define OBJECT_INDEX_ARG_TYPE as_object_index_t

/* Require that the caller worry about mutual exclusion.  */
#define AS_LOCK
#define AS_UNLOCK

#include "as.c"
