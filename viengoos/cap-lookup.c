/* cap-lookup.c - Address space walker.
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

#include <hurd/cap.h>
#include <hurd/folio.h>
#include <hurd/stddef.h>
#include <assert.h>

#include "bits.h"

#ifdef RM_INTERN
#include "object.h"
#endif

union rt
{
  struct cap cap;
  struct cap *capp;
};

enum lookup_mode
  {
    want_cap,
    want_slot,
    want_object
  };

static bool
lookup (activity_t activity,
	struct cap *root, addr_t address,
	enum cap_type type, bool *writable,
	enum lookup_mode mode, union rt *rt)
{
  struct cap *start = root;

  l4_uint64_t addr = addr_prefix (address);
  l4_word_t remaining = addr_depth (address);
  /* The code below assumes that the REMAINING significant bits are in the
     lower bits, not upper.  */
  addr >>= (ADDR_BITS - remaining);

  struct cap fake_slot;

  /* Assume the object is writable until proven otherwise.  */
  int w = true;

  while (remaining > 0)
    {
      assert (CAP_TYPE_MIN <= root->type && root->type <= CAP_TYPE_MAX);

      if (cap_is_a (root, cap_rcappage))
	/* The page directory is read-only.  Note the weakened access
	   appropriately.  */
	{
	  if (type != -1 && type != cap_rpage && type != cap_rcappage)
	    {
	      debug (1, "Read-only cappage at %llx/%d but %s requires "
		     "write access",
		     addr_prefix (addr_chop (address, remaining)),
		     addr_depth (address) - remaining,
		     cap_type_string (type));
		     
	      /* Translating this capability does not provide write
		 access.  The only objects that are useful without write
		 access are read-only pages and read-only capability
		 pages.  If the user is not looking for one of those,
		 then bail.  */
	      return false;
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
	      return false;
	    }

	  int guard = extract_bits64_inv (addr, remaining - 1, gdepth);
	  if (CAP_GUARD (root) != guard)
	    {
	      debug (1, "Guard 0x%llx/%d does not match 0x%llx's "
		     "bits %d-%d => 0x%x",
		     CAP_GUARD (root), CAP_GUARD_BITS (root), addr,
		     remaining - gdepth, remaining - 1, guard);
	      return false;
	    }

	  remaining -= gdepth;
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
		return false;
	      }

	    struct object *object = cap_to_object (activity, root);
	    if (! object)
	      {
#ifdef RM_INTERN
		debug (1, "Failed to get object with OID %llx",
		       root->oid);
#endif
		return false;
	      }

	    int offset = CAP_SUBPAGE_OFFSET (root)
	      + extract_bits64_inv (addr, remaining - 1, bits);
	    assert (0 <= offset && offset < CAPPAGE_SLOTS);
	    remaining -= bits;

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
	      return false;
	    }

	  struct object *object = cap_to_object (activity, root);
#ifdef RM_INTERN
	  if (! object)
	    {
	      debug (1, "Failed to get object with OID %llx",
		     root->oid);
	      return false;
	    }
#else
	  assert (object);
#endif

	  struct folio *folio = (struct folio *) object;

	  int i = extract_bits64_inv (addr, remaining - 1, FOLIO_OBJECTS_LOG2);
#ifdef RM_INTERN
	  root = &fake_slot;
	  if (folio->objects[i].type == cap_void)
	    {
	      memset (root, 0, sizeof (*root));
	      root->type = cap_void;
	    }
	  else
	    {
	      struct object_desc *fdesc;
	      fdesc = object_to_object_desc (object);

	      object = object_find (activity, fdesc->oid + i + 1);
	      assert (object);
	      *root = object_to_cap (object);
	    }
#else
	  root = &folio->objects[i];
#endif

	  remaining -= FOLIO_OBJECTS_LOG2;
	  break;

	default:
	  /* We designate a non-address bit translating object but we
	     have no bits left to translate.  */
	  debug (1, "Encountered a %s at %llx/%d, expected a cappage",
		 cap_type_string (root->type),
		 addr_prefix (addr_chop (address, remaining)),
		 addr_depth (address) - remaining);
	  return false;
	}

      if (remaining == 0)
	/* We've indexed the object and have no bits remaining to
	   translate.  */
	{
	  if (CAP_GUARD_BITS (root) && mode == want_object)
	    /* The caller wants an object but we haven't translated
	       the slot's guard.  */
	    {
	      debug (1, "Found slot at %llx/%d but referenced object "
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

  if (! cap_is_a (root, type))
    /* Types don't match.  They may, however, be compatible.  */
    {
      if (((cap_is_a (root, cap_rpage) || cap_is_a (root, cap_page))
	   && (type == cap_rpage || type == cap_page))
	  || ((cap_is_a (root, cap_rcappage) || cap_is_a (root, cap_cappage))
	      && (type == cap_rcappage || type == cap_cappage)))
	/* Type are compatible.  We just need to downgrade the
	   rights.  */
	w = false;
      else if (type != -1)
	/* Incompatible types.  */
	{
	  do_debug (4)
	    as_dump_from (activity, start, __func__);
	  debug (4, "Requested type %s but cap at 0x%llx/%d designates a %s",
		 cap_type_string (type),
		 addr_prefix (address), addr_depth (address),
		 cap_type_string (root->type));
	  return false;
	}
    }

  if (cap_is_a (root, cap_rpage) || cap_is_a (root, cap_rcappage))
    w = false;

  if (writable)
    *writable = w;

  if (mode == want_slot)
    {
      if (root == &fake_slot)
	{
	  debug (1, "%llx/%d resolves to a folio object but want a slot",
		 addr_prefix (address), addr_depth (address));
	  return false;
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

struct cap
cap_lookup_rel (activity_t activity,
		struct cap *root, addr_t address,
		enum cap_type type, bool *writable)
{
  union rt rt;

  if (! lookup (activity, root, address, type, writable, want_cap, &rt))
    return (struct cap) { .type = cap_void };
  return rt.cap;
}

struct cap
object_lookup_rel (activity_t activity,
		   struct cap *root, addr_t address,
		   enum cap_type type, bool *writable)
{
  union rt rt;

  if (! lookup (activity, root, address, type, writable, want_object, &rt))
    return (struct cap) { .type = cap_void };
  return rt.cap;
}

struct cap *
slot_lookup_rel (activity_t activity,
		 struct cap *root, addr_t address,
		 enum cap_type type, bool *writable)
{
  union rt rt;

  if (! lookup (activity, root, address, type, writable, want_slot, &rt))
    return NULL;
  return rt.capp;
}

