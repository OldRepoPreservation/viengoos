/* cap.c - Basic capability framework.
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

#include <assert.h>
#include <hurd/stddef.h>
#include <viengoos/messenger.h>

#include "cap.h"
#include "object.h"
#include "activity.h"
#include "thread.h"

const int cap_type_num_slots[] = { [vg_cap_void] = 0,
				   [vg_cap_page] = 0,
				   [vg_cap_rpage] = 0,
				   [vg_cap_cappage] = VG_CAPPAGE_SLOTS,
				   [vg_cap_rcappage] = VG_CAPPAGE_SLOTS,
				   [vg_cap_folio] = 0,
				   [vg_cap_activity] = 0,
				   [vg_cap_activity_control] = 0,
				   [vg_cap_thread] = VG_THREAD_SLOTS };

static struct object *
cap_to_object_internal (struct activity *activity, struct vg_cap *cap,
			bool hard)
{
  if (cap->type == vg_cap_void)
    return NULL;

  /* XXX: If CAP does not grant write access, then we need to flatten
     the discardable bit.  */
  struct object *object;
  if (hard)
    {
      object = object_find (activity, cap->oid, VG_CAP_POLICY_GET (*cap));
    }
  else
    object = object_find_soft (activity, cap->oid, VG_CAP_POLICY_GET (*cap));

  if (! object)
    return NULL;

  struct object_desc *desc = object_to_object_desc (object);
  if (desc->version != cap->version)
    {
      /* Clear the capability to save the effort of looking up the
	 object in the future.  */
      cap->type = vg_cap_void;
      return NULL;
    }

  /* If the capability is valid, then the cap type and the object type
     must be compatible.  */
  assert (vg_cap_types_compatible (cap->type, desc->type));

  return object;
}

struct object *
vg_cap_to_object (struct activity *activity, struct vg_cap *cap)
{
  return cap_to_object_internal (activity, cap, true);
}

struct object *
cap_to_object_soft (struct activity *activity, struct vg_cap *cap)
{
  return cap_to_object_internal (activity, cap, false);
}

void
cap_shootdown (struct activity *activity, struct vg_cap *root)
{
  assert (activity);

  /* XXX: A recursive function may not be the best idea here.  We are
     guaranteed, however, at most 63 nested calls.  */
  void doit (struct vg_cap *cap, int remaining)
    {
      int i;
      struct object *object;

      remaining -= VG_CAP_GUARD_BITS (cap);

      switch (cap->type)
	{
	case vg_cap_page:
	case vg_cap_rpage:
	  if (remaining < PAGESIZE_LOG2)
	    return;

	  /* If the object is not in memory, then it can't be
	     mapped.  */
	  object = object_find_soft (activity, cap->oid,
				     VG_OBJECT_POLICY (cap->discardable,
						    cap->priority));
	  if (! object)
	    return;

	  struct object_desc *desc = object_to_object_desc (object);
	  if (desc->version != cap->version)
	    {
	      /* Clear the capability to save the effort of looking up the
		 object in the future.  */
	      cap->type = vg_cap_void;
	      return;
	    }

	  object_desc_unmap (desc);
	  return;

	case vg_cap_cappage:
	case vg_cap_rcappage:
	  if (remaining < VG_CAP_SUBPAGE_SIZE_LOG2 (cap) + PAGESIZE_LOG2)
	    return;

	  object = vg_cap_to_object (activity, cap);
	  if (! object)
	    return;

	  remaining -= VG_CAP_SUBPAGE_SIZE_LOG2 (cap);

	  for (i = 0; i < VG_CAP_SUBPAGE_SIZE (cap); i ++)
	    if (root->oid != object->caps[i].oid)
	      doit (&object->caps[i], remaining);

	  return;

	case vg_cap_messenger:
	case vg_cap_rmessenger:
	  if (remaining < VG_MESSENGER_SLOTS_LOG2 + PAGESIZE_LOG2)
	    return;

	  object = vg_cap_to_object (activity, cap);
	  if (! object)
	    return;

	  remaining -= VG_MESSENGER_SLOTS_LOG2;

	  for (i = 0; i < VG_MESSENGER_SLOTS_LOG2; i ++)
	    if (root->oid != object->caps[i].oid)
	      doit (&object->caps[i], remaining);

	  return;

	case vg_cap_thread:
	  if (remaining < VG_THREAD_SLOTS_LOG2 + PAGESIZE_LOG2)
	    return;

	  object = vg_cap_to_object (activity, cap);
	  if (! object)
	    return;

	  remaining -= VG_THREAD_SLOTS_LOG2;

	  for (i = 0; i < VG_THREAD_SLOTS_LOG2; i ++)
	    if (root->oid != object->caps[i].oid)
	      doit (&object->caps[i],
		    remaining
		    + (i == VG_THREAD_ASPACE_SLOT ? VG_THREAD_SLOTS_LOG2 : 0));

	  return;


	case vg_cap_folio:
	  if (remaining < VG_FOLIO_OBJECTS_LOG2 + PAGESIZE_LOG2)
	    return;

	  object = vg_cap_to_object (activity, cap);
	  if (! object)
	    return;

	  struct folio *folio = (struct folio *) object;
	  struct object_desc *fdesc = object_to_object_desc (object);
	  vg_oid_t foid = fdesc->oid;

	  remaining -= VG_FOLIO_OBJECTS_LOG2;

	  for (i = 0; i < VG_FOLIO_OBJECTS; i ++)
	    if (vg_folio_object_type (folio, i) == vg_cap_page
		|| vg_folio_object_type (folio, i) == vg_cap_rpage
		|| vg_folio_object_type (folio, i) == vg_cap_cappage
		|| vg_folio_object_type (folio, i) == vg_cap_rcappage)
	      {
		struct vg_cap cap;

		cap.version = folio_object_version (folio, i);
		cap.type = vg_folio_object_type (folio, i);
		cap.addr_trans = VG_CAP_ADDR_TRANS_VOID;
		cap.oid = foid + 1 + i;

		if (root->oid != cap.oid)
		  doit (&cap, remaining);
	      }

	  return;

	default:
	  return;
	}
    }

  doit (root, VG_ADDR_BITS);
}
