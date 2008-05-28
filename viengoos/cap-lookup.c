/* cap-lookup.c - Address space walker.
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

#ifndef RM_INTERN

pthread_rwlock_t as_lock = __PTHREAD_RWLOCK_INITIALIZER;

static void __attribute__ ((noinline))
ensure_stack(void)
{
  /* XXX: If we fault on the stack while we have the address space
     lock, we deadlock.  Ensure that we have some stack space and hope
     it is enough.  (This can't be too much as we may be running on
     the exception handler's stack.)  */
  volatile char space[1024 + 512];
  space[0] = 0;
  space[sizeof (space) - 1] = 0;
}

# define AS_LOCK					\
  do							\
    {							\
      ensure_stack ();					\
      pthread_rwlock_rdlock (&as_lock);			\
    }							\
  while (0)

# define AS_UNLOCK pthread_rwlock_unlock (&as_lock)

#else
# define AS_LOCK do { } while (0)
# define AS_UNLOCK do { } while (0)
#endif


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

      if (root->type == cap_rcappage)
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
	      debug (5, "Translating " ADDR_FMT ": guard 0x%llx/%d does "
		     "not match 0x%llx's bits %d-%d => 0x%x",
		     ADDR_PRINTF (address),
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
		debug (1, "Failed to get object with OID " OID_FMT,
		       OID_PRINTF (root->oid));
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
	  if (! object)
	      {
#ifdef RM_INTERN
		debug (1, "Failed to get object with OID " OID_FMT,
		       OID_PRINTF (root->oid));
#endif
		return false;
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
	  break;

	default:
	  /* We designate a non-address bit translating object but we
	     have no bits left to translate.  This is not an unusual
	     error: it will occur when the application faults in an
	     area for which it has a pager.  */
	  do_debug (4)
	    as_dump_from (activity, start, NULL);
	  debug (4, "Translating " ADDR_FMT ", encountered a %s at "
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
	  if (CAP_GUARD_BITS (root) && mode == want_object)
	    /* The caller wants an object but we haven't translated
	       the slot's guard.  */
	    {
	      debug (4, "Found slot at %llx/%d but referenced object "
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
	  debug (4, "cap at " ADDR_FMT " designates a %s but want a %s",
		 ADDR_PRINTF (address), cap_type_string (root->type),
		 cap_type_string (type));
	  return false;
	}
    }

  if (mode == want_object
      && (root->type == cap_rpage || root->type == cap_rcappage))
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

  AS_LOCK;

  bool ret = lookup (activity, root, address, type, writable, want_cap, &rt);

  AS_UNLOCK;

  if (! ret)
    return (struct cap) { .type = cap_void };
  return rt.cap;
}

struct cap
object_lookup_rel (activity_t activity,
		   struct cap *root, addr_t address,
		   enum cap_type type, bool *writable)
{
  union rt rt;

  AS_LOCK;

  bool ret = lookup (activity, root, address, type, writable, want_object, &rt);

  AS_UNLOCK;

  if (! ret)
    return (struct cap) { .type = cap_void };
  return rt.cap;
}

struct cap *
slot_lookup_rel (activity_t activity,
		 struct cap *root, addr_t address,
		 enum cap_type type, bool *writable)
{
  union rt rt;

  AS_LOCK;

  bool ret = lookup (activity, root, address, type, writable, want_slot, &rt);

  AS_UNLOCK;

  if (! ret)
    return NULL;
  return rt.capp;
}

extern int s_printf (const char *fmt, ...);
extern int s_putchar (int chr);

static void
print_nr (int width, l4_int64_t nr, bool hex)
{
  extern int putchar (int chr);

  int base = 10;
  if (hex)
    base = 16;

  l4_int64_t v = nr;
  int w = 0;
  if (v < 0)
    {
      v = -v;
      w ++;
    }
  do
    {
      w ++;
      v /= base;
    }
  while (v > 0);

  int i;
  for (i = w; i < width; i ++)
    s_putchar (' ');

  if (hex)
    s_printf ("0x%llx", nr);
  else
    s_printf ("%lld", nr);
}

static void
do_walk (activity_t activity, int index,
	 struct cap *root, addr_t addr,
	 int indent, bool descend, const char *output_prefix)
{
  int i;

  struct cap cap = cap_lookup_rel (activity, root, addr, -1, NULL);
  if (cap.type == cap_void)
    return;

  if (! cap_to_object (activity, &cap))
    /* Cap is there but the object has been deallocated.  */
    return;

  if (output_prefix)
    s_printf ("%s: ", output_prefix);
  for (i = 0; i < indent; i ++)
    s_printf (".");

  s_printf ("[ ");
  if (index != -1)
    print_nr (3, index, false);
  else
    s_printf ("root");
  s_printf (" ] ");

  print_nr (12, addr_prefix (addr), true);
  s_printf ("/%d ", addr_depth (addr));
  if (CAP_GUARD_BITS (&cap))
    s_printf ("| 0x%llx/%d ", CAP_GUARD (&cap), CAP_GUARD_BITS (&cap));
  if (CAP_SUBPAGES (&cap) != 1)
    s_printf ("(%d/%d) ", CAP_SUBPAGE (&cap), CAP_SUBPAGES (&cap));

  if (CAP_GUARD_BITS (&cap)
      && ADDR_BITS - addr_depth (addr) >= CAP_GUARD_BITS (&cap))
    s_printf ("=> 0x%llx/%d ",
	    addr_prefix (addr_extend (addr,
				      CAP_GUARD (&cap),
				      CAP_GUARD_BITS (&cap))),
	    addr_depth (addr) + CAP_GUARD_BITS (&cap));

#ifdef RM_INTERN
  s_printf ("@" OID_FMT " ", OID_PRINTF (cap.oid));
#endif
  s_printf ("%s", cap_type_string (cap.type));

  if (! descend)
    s_printf ("...");

  s_printf ("\n");

  if (! descend)
    return;

  if (addr_depth (addr) + CAP_GUARD_BITS (&cap) > ADDR_BITS)
    return;

  addr = addr_extend (addr, CAP_GUARD (&cap), CAP_GUARD_BITS (&cap));

  switch (cap.type)
    {
    case cap_cappage:
    case cap_rcappage:
      if (addr_depth (addr) + CAP_SUBPAGE_SIZE_LOG2 (&cap) > ADDR_BITS)
	return;

      for (i = 0; i < CAP_SUBPAGE_SIZE (&cap); i ++)
	do_walk (activity, i, root,
		 addr_extend (addr, i, CAP_SUBPAGE_SIZE_LOG2 (&cap)),
		 indent + 1, true, output_prefix);

      return;

    case cap_folio:
      if (addr_depth (addr) + FOLIO_OBJECTS_LOG2 > ADDR_BITS)
	return;

      for (i = 0; i < FOLIO_OBJECTS; i ++)
	do_walk (activity, i, root,
		 addr_extend (addr, i, FOLIO_OBJECTS_LOG2),
		 indent + 1, false, output_prefix);

      return;

    default:
      return;
    }
}

/* AS_LOCK must not be held.  */
void
as_dump_from (activity_t activity, struct cap *root, const char *prefix)
{
  do_walk (activity, -1, root, ADDR (0, 0), 0, true, prefix);
}
