/* cap.c - Basic capability framework.
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

#include <assert.h>
#include <hurd/stddef.h>

#include "cap.h"
#include "object.h"
#include "activity.h"
#include "thread.h"

const int cap_type_num_slots[] = { [cap_void] = 0,
				   [cap_page] = 0,
				   [cap_rpage] = 0,
				   [cap_cappage] = CAPPAGE_SLOTS,
				   [cap_rcappage] = CAPPAGE_SLOTS,
				   [cap_folio] = 0,
				   [cap_activity] = 0,
				   [cap_activity_control] = 0,
				   [cap_thread] = THREAD_SLOTS };

struct object *
cap_to_object (struct activity *activity, struct cap *cap)
{
  if (cap->type == cap_void)
    return NULL;

  /* XXX: If CAP does not grant write access, then we need to flatten
     the discardable bit.  */
  struct object *object = object_find (activity, cap->oid,
				       OBJECT_POLICY (cap->discardable,
						      cap->priority));
  if (! object)
    {
      /* Clear the capability to save the effort of looking up the
	 object in the future.  */
      cap->type = cap_void;
      return NULL;
    }

  struct object_desc *desc = object_to_object_desc (object);
  if (desc->version != cap->version)
    {
      /* Clear the capability to save the effort of looking up the
	 object in the future.  */
      cap->type = cap_void;
      return NULL;
    }

  /* If the capability is valid, then the cap type and the object type
     must be compatible.  */
  assert (cap_types_compatible (cap->type, desc->type));

  return object;
}

void
cap_shootdown (struct activity *activity, struct cap *cap)
{
  assert (activity);

  /* XXX: A recursive function may not be the best idea here.  We are
     guaranteed, however, at most 63 nested calls.  */
  void doit (struct cap *cap, int remaining)
    {
      int i;
      struct object *object;

      remaining -= CAP_GUARD_BITS (cap);

      switch (cap->type)
	{
	case cap_page:
	case cap_rpage:
	  if (remaining < PAGESIZE_LOG2)
	    return;

	  /* If the object is not in memory, then it can't be
	     mapped.  */
	  object = object_find_soft (activity, cap->oid,
				     OBJECT_POLICY (cap->discardable,
						    cap->priority));
	  if (! object)
	    return;

	  struct object_desc *desc = object_to_object_desc (object);
	  if (desc->version != cap->version)
	    {
	      /* Clear the capability to save the effort of looking up the
		 object in the future.  */
	      cap->type = cap_void;
	      return;
	    }

#ifndef _L4_TEST_ENVIRONMENT
	  l4_fpage_t fpage = l4_fpage ((l4_word_t) object, PAGESIZE);
	  fpage = l4_fpage_add_rights (fpage, L4_FPAGE_FULLY_ACCESSIBLE);
	  l4_unmap_fpage (fpage);
#endif
	  return;

	case cap_cappage:
	case cap_rcappage:
	  if (remaining < CAP_SUBPAGE_SIZE_LOG2 (cap) + PAGESIZE_LOG2)
	    return;

	  object = cap_to_object (activity, cap);
	  if (! object)
	    return;

	  remaining -= CAP_SUBPAGE_SIZE_LOG2 (cap);

	  for (i = 0; i < CAP_SUBPAGE_SIZE_LOG2 (cap); i ++)
	    doit (&object->caps[i], remaining);

	  return;

	case cap_folio:
	  if (remaining < FOLIO_OBJECTS_LOG2 + PAGESIZE_LOG2)
	    return;

	  object = cap_to_object (activity, cap);
	  if (! object)
	    return;

	  struct folio *folio = (struct folio *) object;
	  struct object_desc *fdesc = object_to_object_desc (object);
	  oid_t foid = fdesc->oid;

	  remaining -= FOLIO_OBJECTS_LOG2;

	  for (i = 0; i < FOLIO_OBJECTS_LOG2; i ++)
	    if (folio->objects[i].type == cap_page
		|| folio->objects[i].type == cap_rpage
		|| folio->objects[i].type == cap_cappage
		|| folio->objects[i].type == cap_rcappage)
	      {
		struct cap cap;

		cap.version = folio->objects[i].version;
		cap.type = folio->objects[i].type;
		cap.addr_trans = CAP_ADDR_TRANS_VOID;
		cap.oid = foid + 1 + i;

		doit (&cap, remaining);
	      }

	  return;

	default:
	  return;
	}
    }

  doit (cap, ADDR_BITS);
}
