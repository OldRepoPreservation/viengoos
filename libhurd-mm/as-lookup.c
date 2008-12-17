/* as-lookup.c - Address space walker.
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

#include <viengoos/cap.h>
#include <viengoos/folio.h>
#include <hurd/as.h>
#include <hurd/stddef.h>
#include <assert.h>

#include "bits.h"

#ifdef RM_INTERN
#include <profile.h>
#endif

#ifdef RM_INTERN
#include "../viengoos/object.h"
#endif

#ifndef NDEBUG
#define DUMP_OR_RET(ret)			\
  do						\
    {						\
      if (dump_path)				\
	{					\
	  debug (0, "Bye.");			\
	  return ret;				\
	}					\
      else					\
	{					\
	  dump_path = true;			\
	  goto dump_path;			\
	}					\
    }						\
  while (0)
#else
#define DUMP_OR_RET(ret)			\
  return ret;
#endif

static bool
as_lookup_rel_internal (activity_t activity,
			struct vg_cap *root, vg_addr_t address,
			enum vg_cap_type type, bool *writable,
			enum as_lookup_mode mode, union as_lookup_ret *rt,
			bool dump)
{
  assert (root);

  struct vg_cap *start = root;

#ifndef NDEBUG
  bool dump_path = dump;
 dump_path:
#else
# define dump_path false
#endif
  root = start;

  uint64_t addr = vg_addr_prefix (address);
  uintptr_t remaining = vg_addr_depth (address);
  /* The code below assumes that the REMAINING significant bits are in the
     lower bits, not upper.  */
  addr >>= (VG_ADDR_BITS - remaining);

  struct vg_cap fake_slot;

  /* Assume the object is writable until proven otherwise.  */
  int w = true;

  if (dump_path)
    debug (0, "Looking up %s at " VG_ADDR_FMT,
	   mode == as_lookup_want_cap ? "vg_cap"
	   : (mode == as_lookup_want_slot ? "slot" : "object"),
	   VG_ADDR_PRINTF (address));

  while (remaining > 0)
    {
      if (dump_path)
	debug (0, "Cap at " VG_ADDR_FMT ": " VG_CAP_FMT " -> " VG_ADDR_FMT " (%d)",
	       VG_ADDR_PRINTF (vg_addr_chop (address, remaining)),
	       VG_CAP_PRINTF (root),
	       VG_ADDR_PRINTF (vg_addr_chop (address,
				       remaining - VG_CAP_GUARD_BITS (root))),
	       remaining);

      assertx (VG_CAP_TYPE_MIN <= root->type && root->type <= VG_CAP_TYPE_MAX,
	       "Cap at " VG_ADDR_FMT " has type %d?! (" VG_ADDR_FMT ")",
	       VG_ADDR_PRINTF (vg_addr_chop (address, remaining)), root->type,
	       VG_ADDR_PRINTF (address));

      if (root->type == vg_cap_rcappage)
	/* The page directory is read-only.  Note the weakened access
	   appropriately.  */
	{
	  if (type != -1 && ! vg_cap_type_weak_p (type))
	    {
	      debug (1, "Read-only cappage at %llx/%d but %s requires "
		     "write access",
		     vg_addr_prefix (vg_addr_chop (address, remaining)),
		     vg_addr_depth (address) - remaining,
		     vg_cap_type_string (type));
		     
	      /* Translating this capability does not provide write
		 access.  The requested type is strong, bail.  */
	      DUMP_OR_RET (false);
	    }

	  w = false;
	}

      if (VG_CAP_GUARD_BITS (root))
	/* Check that VG_ADDR contains the guard.  */
	{
	  int gdepth = VG_CAP_GUARD_BITS (root);

	  if (gdepth > remaining)
	    {
	      debug (1, "Translating %llx/%d; not enough bits (%d) to "
		     "translate %d-bit guard at /%d",
		     vg_addr_prefix (address), vg_addr_depth (address),
		     remaining, gdepth, VG_ADDR_BITS - remaining);

	      DUMP_OR_RET (false);
	    }

	  int guard = extract_bits64_inv (addr, remaining - 1, gdepth);
	  if (VG_CAP_GUARD (root) != guard)
	    {
	      debug (dump_path ? 0 : 5,
		     "Translating " VG_ADDR_FMT ": guard 0x%llx/%d does "
		     "not match 0x%llx's bits %d-%d => 0x%x",
		     VG_ADDR_PRINTF (address),
		     VG_CAP_GUARD (root), VG_CAP_GUARD_BITS (root), addr,
		     remaining - gdepth, remaining - 1, guard);
	      return false;
	    }

	  remaining -= gdepth;

	  if (dump_path)
	    debug (0, "Translating guard: %d/%d (%d)",
		   guard, gdepth, remaining);
	}

      if (remaining == 0)
	/* We've translate the guard bits and there are no bits left
	   to translate.  We now designate the object and not the
	   slot, however, if we designate an object, we always return
	   the slot pointing to it.  */
	break;

      switch (root->type)
	{
	case vg_cap_cappage:
	case vg_cap_rcappage:
	  {
	    /* Index the page table.  */
	    int bits = VG_CAP_SUBPAGE_SIZE_LOG2 (root);
	    if (remaining < bits)
	      {
		debug (1, "Translating " VG_ADDR_FMT "; not enough bits (%d) "
		       "to index %d-bit cappage at " VG_ADDR_FMT,
		       VG_ADDR_PRINTF (address), remaining, bits,
		       VG_ADDR_PRINTF (vg_addr_chop (address, remaining)));
		DUMP_OR_RET (false);
	      }

	    struct object *object = vg_cap_to_object (activity, root);
	    if (! object)
	      {
#ifdef RM_INTERN
		debug (1, "Failed to get object with OID " VG_OID_FMT,
		       VG_OID_PRINTF (root->oid));
		DUMP_OR_RET (false);
#endif
		return false;
	      }

	    int offset = VG_CAP_SUBPAGE_OFFSET (root)
	      + extract_bits64_inv (addr, remaining - 1, bits);
	    assert (0 <= offset && offset < VG_CAPPAGE_SLOTS);
	    remaining -= bits;

	    if (dump_path)
	      debug (0, "Indexing cappage: %d/%d (%d)",
		     offset, bits, remaining);

	    root = &object->caps[offset];
	    break;
	  }

	case vg_cap_folio:
	  if (remaining < VG_FOLIO_OBJECTS_LOG2)
	    {
	      debug (1, "Translating " VG_ADDR_FMT "; not enough bits (%d) "
		     "to index folio at " VG_ADDR_FMT,
		     VG_ADDR_PRINTF (address), remaining,
		     VG_ADDR_PRINTF (vg_addr_chop (address, remaining)));
	      DUMP_OR_RET (false);
	    }

	  struct object *object = vg_cap_to_object (activity, root);
	  if (! object)
	      {
#ifdef RM_INTERN
		debug (1, "Failed to get object with OID " VG_OID_FMT,
		       VG_OID_PRINTF (root->oid));
#endif
		DUMP_OR_RET (false);
	      }

	  struct folio *folio = (struct folio *) object;

	  int i = extract_bits64_inv (addr, remaining - 1, VG_FOLIO_OBJECTS_LOG2);
#ifdef RM_INTERN
	  root = &fake_slot;
	  *root = vg_folio_object_cap (folio, i);
#else
	  root = &folio->objects[i];
#endif

	  remaining -= VG_FOLIO_OBJECTS_LOG2;

	  if (dump_path)
	    debug (0, "Indexing folio: %d/%d (%d)",
		   i, VG_FOLIO_OBJECTS_LOG2, remaining);

	  break;

	case vg_cap_thread:
	case vg_cap_messenger:
	  /* Note: rmessengers don't expose their capability slots.  */
	  {
	    /* Index the object.  */
	    int bits;
	    switch (root->type)
	      {
	      case vg_cap_thread:
		bits = VG_THREAD_SLOTS_LOG2;
		break;

	      case vg_cap_messenger:
		bits = VG_MESSENGER_SLOTS_LOG2;
		break;
	      }

	    if (remaining < bits)
	      {
		debug (1, "Translating " VG_ADDR_FMT "; not enough bits (%d) "
		       "to index %d-bit %s at " VG_ADDR_FMT,
		       VG_ADDR_PRINTF (address), remaining, bits,
		       vg_cap_type_string (root->type),
		       VG_ADDR_PRINTF (vg_addr_chop (address, remaining)));
		DUMP_OR_RET (false);
	      }

	    struct object *object = vg_cap_to_object (activity, root);
	    if (! object)
	      {
#ifdef RM_INTERN
		debug (1, "Failed to get object with OID " VG_OID_FMT,
		       VG_OID_PRINTF (root->oid));
		DUMP_OR_RET (false);
#endif
		return false;
	      }
#ifdef RM_INTERN
	    assert (object_type (object) == root->type);
#endif

	    int offset = extract_bits64_inv (addr, remaining - 1, bits);
	    assert (0 <= offset && offset < (1 << bits));
	    remaining -= bits;

	    if (dump_path)
	      debug (0, "Indexing %s: %d/%d (%d)",
		     vg_cap_type_string (root->type), offset, bits, remaining);

	    root = &object->caps[offset];
	    break;
	  }

	default:
	  /* We designate a non-address bit translating object but we
	     have no bits left to translate.  This is not an unusual
	     error: it will occur when the application faults in an
	     area for which it has a pager.  */
	  do_debug (4)
	    as_dump_from (activity, start, NULL);
	  debug (dump_path ? 0 : 5,
		 "Translating " VG_ADDR_FMT ", encountered a %s at "
		 VG_ADDR_FMT " but expected a cappage or a folio",
		 VG_ADDR_PRINTF (address),
		 vg_cap_type_string (root->type),
		 VG_ADDR_PRINTF (vg_addr_chop (address, remaining)));
	  return false;
	}

      if (remaining == 0)
	/* We've indexed the object and have no bits remaining to
	   translate.  */
	{
	  if (VG_CAP_GUARD_BITS (root) && mode == as_lookup_want_object)
	    /* The caller wants an object but we haven't translated
	       the slot's guard.  */
	    {
	      debug (dump_path ? 0 : 4,
		     "Found slot at %llx/%d but referenced object "
		     "(%s) has an untranslated guard of %lld/%d!",
		     vg_addr_prefix (address), vg_addr_depth (address),
		     vg_cap_type_string (root->type), VG_CAP_GUARD (root),
		     VG_CAP_GUARD_BITS (root));
	      return false;
	    }

	  break;
	}
    }
  assert (remaining == 0);

  if (dump_path)
    debug (0, "Cap at " VG_ADDR_FMT ": " VG_CAP_FMT " -> " VG_ADDR_FMT " (%d)",
	   VG_ADDR_PRINTF (vg_addr_chop (address, remaining)),
	   VG_CAP_PRINTF (root),
	   VG_ADDR_PRINTF (vg_addr_chop (address,
				   remaining - VG_CAP_GUARD_BITS (root))),
	   remaining);

  if (type != -1 && type != root->type)
    /* Types don't match.  */
    {
      if (vg_cap_type_strengthen (type) == root->type)
	/* The capability just provides more strength than
	   requested.  That's fine.  */
	;
      else
	/* Incompatible types.  */
	{
	  do_debug (4)
	    as_dump_from (activity, start, __func__);
	  debug (dump_path ? 0 : 4,
		 "vg_cap at " VG_ADDR_FMT " designates a %s but want a %s",
		 VG_ADDR_PRINTF (address), vg_cap_type_string (root->type),
		 vg_cap_type_string (type));
	  return false;
	}
    }

  if (mode == as_lookup_want_object && vg_cap_type_weak_p (root->type))
    w = false;

  if (writable)
    *writable = w;

  if (mode == as_lookup_want_slot)
    {
      if (root == &fake_slot)
	{
	  debug (1, "%llx/%d resolves to a folio object but want a slot",
		 vg_addr_prefix (address), vg_addr_depth (address));
	  DUMP_OR_RET (false);
	}
      rt->capp = root;
      return true;
    }
  else
    {
      rt->cap = *root;
      return true;
    }
}

bool
as_lookup_rel (activity_t activity,
	       struct vg_cap *root, vg_addr_t address,
	       enum vg_cap_type type, bool *writable,
	       enum as_lookup_mode mode, union as_lookup_ret *rt)
{
  bool r;

#ifdef RM_INTERN
  profile_region (NULL);
#endif
  r = as_lookup_rel_internal (activity,
			      root, address, type, writable, mode, rt,
			      false);
#ifdef RM_INTERN
  profile_region_end ();
#endif

  return r;
}

void
as_dump_path_rel (activity_t activity, struct vg_cap *root, vg_addr_t addr)
{
  union as_lookup_ret rt;

  as_lookup_rel_internal (activity,
			  root, addr, -1,
			  NULL, as_lookup_want_cap, &rt,
			  true);
}
