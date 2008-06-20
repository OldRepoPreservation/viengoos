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

#include <hurd/cap.h>
#include <hurd/folio.h>
#include <hurd/as.h>
#include <hurd/stddef.h>
#include <assert.h>

#include "bits.h"

#ifdef RM_INTERN
#include "../viengoos/profile.h"
#endif

#ifndef RM_INTERN
# include <pthread.h>
pthread_rwlock_t as_rwlock;
#endif

#ifdef RM_INTERN
#include "../viengoos/object.h"
#endif

#ifndef NDEBUG
#define DUMP_OR_RET(ret)			\
  {						\
    if (dump_path)				\
      {						\
	debug (0, "Bye.");			\
	return ret;				\
      }						\
    else					\
      {						\
	dump_path = true;			\
	goto dump_path;				\
      }						\
  }
#else
#define DUMP_OR_RET(ret)			\
  if (dump_path)				\
    return ret;
#endif

static bool
as_lookup_rel_internal (activity_t activity,
			struct cap *root, addr_t address,
			enum cap_type type, bool *writable,
			enum as_lookup_mode mode, union as_lookup_ret *rt,
			bool dump)
{
  bool dump_path = dump;

  struct cap *start = root;
 dump_path:;
  root = start;

  l4_uint64_t addr = addr_prefix (address);
  l4_word_t remaining = addr_depth (address);
  /* The code below assumes that the REMAINING significant bits are in the
     lower bits, not upper.  */
  addr >>= (ADDR_BITS - remaining);

  struct cap fake_slot;

  /* Assume the object is writable until proven otherwise.  */
  int w = true;

  if (dump_path)
    debug (0, "Looking up %s at " ADDR_FMT,
	   mode == as_lookup_want_cap ? "cap"
	   : (mode == as_lookup_want_slot ? "slot" : "object"),
	   ADDR_PRINTF (address));

  while (remaining > 0)
    {
      if (dump_path)
	debug (0, "Cap at " ADDR_FMT ": " CAP_FMT " -> " ADDR_FMT " (%d)",
	       ADDR_PRINTF (addr_chop (address, remaining)),
	       CAP_PRINTF (root),
	       ADDR_PRINTF (addr_chop (address,
				       remaining - CAP_GUARD_BITS (root))),
	       remaining);

      assert (CAP_TYPE_MIN <= root->type && root->type <= CAP_TYPE_MAX);

      if (root->type == cap_rcappage)
	/* The page directory is read-only.  Note the weakened access
	   appropriately.  */
	{
	  if (type != -1 && ! cap_type_weak_p (type))
	    {
	      debug (1, "Read-only cappage at %llx/%d but %s requires "
		     "write access",
		     addr_prefix (addr_chop (address, remaining)),
		     addr_depth (address) - remaining,
		     cap_type_string (type));
		     
	      /* Translating this capability does not provide write
		 access.  The requested type is strong, bail.  */
	      DUMP_OR_RET (false);
	    }

	  w = false;
	}

      if (CAP_GUARD_BITS (root))
	/* Check that ADDR contains the guard.  */
	{
	  int gdepth = CAP_GUARD_BITS (root);

	  if (gdepth > remaining)
	    {
	      debug (1, "Translating %llx/%d; not enough bits (%d) to "
		     "translate %d-bit guard at /%d",
		     addr_prefix (address), addr_depth (address),
		     remaining, gdepth, ADDR_BITS - remaining);

	      DUMP_OR_RET (false);
	    }

	  int guard = extract_bits64_inv (addr, remaining - 1, gdepth);
	  if (CAP_GUARD (root) != guard)
	    {
	      debug (dump_path ? 0 : 5,
		     "Translating " ADDR_FMT ": guard 0x%llx/%d does "
		     "not match 0x%llx's bits %d-%d => 0x%x",
		     ADDR_PRINTF (address),
		     CAP_GUARD (root), CAP_GUARD_BITS (root), addr,
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
	case cap_cappage:
	case cap_rcappage:
	  {
	    /* Index the page table.  */
	    int bits = CAP_SUBPAGE_SIZE_LOG2 (root);
	    if (remaining < bits)
	      {
		debug (1, "Translating " ADDR_FMT "; not enough bits (%d) "
		       "to index %d-bit cappage at " ADDR_FMT,
		       ADDR_PRINTF (address), remaining, bits,
		       ADDR_PRINTF (addr_chop (address, remaining)));
		DUMP_OR_RET (false);
	      }

	    struct object *object = cap_to_object (activity, root);
	    if (! object)
	      {
#ifdef RM_INTERN
		debug (1, "Failed to get object with OID " OID_FMT,
		       OID_PRINTF (root->oid));
		DUMP_OR_RET (false);
#endif
		return false;
	      }

	    int offset = CAP_SUBPAGE_OFFSET (root)
	      + extract_bits64_inv (addr, remaining - 1, bits);
	    assert (0 <= offset && offset < CAPPAGE_SLOTS);
	    remaining -= bits;

	    if (dump_path)
	      debug (0, "Indexing cappage: %d/%d (%d)",
		     offset, bits, remaining);

	    root = &object->caps[offset];
	    break;
	  }

	case cap_folio:
	  if (remaining < FOLIO_OBJECTS_LOG2)
	    {
	      debug (1, "Translating " ADDR_FMT "; not enough bits (%d) "
		     "to index folio at " ADDR_FMT,
		     ADDR_PRINTF (address), remaining,
		     ADDR_PRINTF (addr_chop (address, remaining)));
	      DUMP_OR_RET (false);
	    }

	  struct object *object = cap_to_object (activity, root);
	  if (! object)
	      {
#ifdef RM_INTERN
		debug (1, "Failed to get object with OID " OID_FMT,
		       OID_PRINTF (root->oid));
#endif
		DUMP_OR_RET (false);
	      }

	  struct folio *folio = (struct folio *) object;

	  int i = extract_bits64_inv (addr, remaining - 1, FOLIO_OBJECTS_LOG2);
#ifdef RM_INTERN
	  root = &fake_slot;
	  *root = folio_object_cap (folio, i);
#else
	  root = &folio->objects[i];
#endif

	  remaining -= FOLIO_OBJECTS_LOG2;

	  if (dump_path)
	    debug (0, "Indexing folio: %d/%d (%d)",
		   i, FOLIO_OBJECTS_LOG2, remaining);

	  break;

	default:
	  /* We designate a non-address bit translating object but we
	     have no bits left to translate.  This is not an unusual
	     error: it will occur when the application faults in an
	     area for which it has a pager.  */
	  do_debug (4)
	    as_dump_from (activity, start, NULL);
	  debug (dump_path ? 0 : 5,
		 "Translating " ADDR_FMT ", encountered a %s at "
		 ADDR_FMT " but expected a cappage or a folio",
		 ADDR_PRINTF (address),
		 cap_type_string (root->type),
		 ADDR_PRINTF (addr_chop (address, remaining)));
	  return false;
	}

      if (remaining == 0)
	/* We've indexed the object and have no bits remaining to
	   translate.  */
	{
	  if (CAP_GUARD_BITS (root) && mode == as_lookup_want_object)
	    /* The caller wants an object but we haven't translated
	       the slot's guard.  */
	    {
	      debug (dump_path ? 0 : 4,
		     "Found slot at %llx/%d but referenced object "
		     "(%s) has an untranslated guard of %lld/%d!",
		     addr_prefix (address), addr_depth (address),
		     cap_type_string (root->type), CAP_GUARD (root),
		     CAP_GUARD_BITS (root));
	      return false;
	    }

	  break;
	}
    }
  assert (remaining == 0);

  if (dump_path)
    debug (0, "Cap at " ADDR_FMT ": " CAP_FMT " -> " ADDR_FMT " (%d)",
	   ADDR_PRINTF (addr_chop (address, remaining)),
	   CAP_PRINTF (root),
	   ADDR_PRINTF (addr_chop (address,
				   remaining - CAP_GUARD_BITS (root))),
	   remaining);

  if (type != -1 && type != root->type)
    /* Types don't match.  */
    {
      if (cap_type_strengthen (type) == root->type)
	/* The capability just provides more strength than
	   requested.  That's fine.  */
	;
      else
	/* Incompatible types.  */
	{
	  do_debug (4)
	    as_dump_from (activity, start, __func__);
	  debug (dump_path ? 0 : 4,
		 "cap at " ADDR_FMT " designates a %s but want a %s",
		 ADDR_PRINTF (address), cap_type_string (root->type),
		 cap_type_string (type));
	  return false;
	}
    }

  if (mode == as_lookup_want_object && cap_type_weak_p (root->type))
    w = false;

  if (writable)
    *writable = w;

  if (mode == as_lookup_want_slot)
    {
      if (root == &fake_slot)
	{
	  debug (1, "%llx/%d resolves to a folio object but want a slot",
		 addr_prefix (address), addr_depth (address));
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
	       struct cap *root, addr_t address,
	       enum cap_type type, bool *writable,
	       enum as_lookup_mode mode, union as_lookup_ret *rt)
{
#ifdef RM_INTERN
  profile_start ((uintptr_t) &as_lookup_rel, "as_lookup");
#endif
  bool r = as_lookup_rel_internal (activity,
				   root, address, type, writable, mode, rt,
				   false);
#ifdef RM_INTERN
  profile_end ((uintptr_t) &as_lookup_rel);
#endif

  return r;
}

void
as_dump_path_rel (activity_t activity, struct cap *root, addr_t addr)
{
  union as_lookup_ret rt;

  return as_lookup_rel_internal (activity,
				 root, addr, -1,
				 NULL, as_lookup_want_cap, &rt,
				 true);
}
