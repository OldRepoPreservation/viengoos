/* as.c - Address space composition helper functions.
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

#include <l4.h>
#include <stddef.h>
#include <assert.h>

#include <hurd/cap.h>
#include <hurd/stddef.h>
#include <hurd/folio.h>

#include "as.h"
#include "bits.h"

#ifdef RM_INTERN
#include "object.h"
#endif

#ifndef RM_INTERN
pthread_rwlock_t as_lock = __PTHREAD_RWLOCK_INITIALIZER;
# define AS_LOCK pthread_rwlock_wrlock (&as_lock)
# define AS_UNLOCK pthread_rwlock_unlock (&as_lock)
# define AS_DUMP rm_as_dump (ADDR_VOID, ADDR_VOID)

#else

# define AS_LOCK do { } while (0)
# define AS_UNLOCK do { } while (0)
# define AS_DUMP as_dump_from (activity, start, __func__);

#endif

/* Build the address space such that A designates a capability slot.
   If MAY_OVERWRITE is true, may overwrite an existing capability.
   Otherwise, the capability slot is expected to contain a void
   capability.  */
static struct cap *
as_build_internal (activity_t activity,
		   struct cap *root, addr_t a,
		   struct as_insert_rt (*allocate_object) (enum cap_type type,
							   addr_t addr),
		   bool may_overwrite)
{
  struct cap *start = root;

  assert (! ADDR_IS_VOID (a));

  l4_uint64_t addr = addr_prefix (a);
  l4_word_t remaining = addr_depth (a);

  debug (4, "Ensuring slot at 0x%llx/%d", addr, remaining);

  /* The REMAINING bits to translates are in the REMAINING most significant
     bits of ADDR.  Here it is more convenient to have them in the
     lower bits.  */
  addr >>= (ADDR_BITS - remaining);

  struct cap fake_slot;

  do
    {
      struct object *cappage = NULL;

      l4_uint64_t root_guard = CAP_GUARD (root);
      int root_gbits = CAP_GUARD_BITS (root);

      if (root->type != cap_void
	  && remaining >= root_gbits
	  && root_guard == extract_bits64_inv (addr,
					       remaining - 1, root_gbits))
	/* ROOT's (possibly zero-width) guard matches and thus
	   translates part of the address.  */
	{
	  if (remaining == root_gbits && may_overwrite)
	    {
	      debug (0, "Overwriting " ADDR_FMT " with " ADDR_FMT
		     " (at " ADDR_FMT ")",
		     ADDR_PRINTF (addr_extend (addr_chop (a, remaining),
					       root_guard, root_gbits)),
		     ADDR_PRINTF (a),
		     ADDR_PRINTF (addr_chop (a, remaining)));
	      /* XXX: Free any data associated with the capability
		 (e.g., shadow pages).  */
	      break;
	    }

	  /* Subtract the number of bits the guard translates.  */
	  remaining -= root_gbits;
	  assert (remaining >= 0);

	  if (remaining == 0)
	    /* ROOT is not a void capability yet the guard translates
	       all of the bits and we may not overwrite the
	       capability.  This means that ROOT references an object
	       at ADDR.  This is a problem: we want to insert a
	       capability at ADDR.  */
	    {
	      AS_DUMP;
	      panic ("There is already a %s object at %llx/%d!",
		     cap_type_string (root->type),
		     addr_prefix (a), addr_depth (a));
	    }

	  switch (root->type)
	    {
	    case cap_cappage:
	    case cap_rcappage:
	      /* Load the referenced object.  */
	      cappage = cap_to_object (activity, root);
	      if (! cappage)
		/* ROOT's type was not void but its designation was
		   invalid.  This can only happen if we inserted an object
		   and subsequently destroyed it.  */
		{
		  /* The type should now have been set to cap_void.  */
		  assert (root->type == cap_void);
		  AS_DUMP;
		  panic ("Lost object at %llx/%d",
			 addr_prefix (a), addr_depth (a) - remaining);
		}

	      /* We index CAPPAGE below.  */
	      break;

	    case cap_folio:
	      {
		if (remaining < FOLIO_OBJECTS_LOG2)
		  panic ("Translating " ADDR_FMT "; not enough bits (%d) "
			 "to index folio at " ADDR_FMT,
			 ADDR_PRINTF (a), remaining,
			 ADDR_PRINTF (addr_chop (a, remaining)));

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

		int i = extract_bits64_inv (addr,
					    remaining - 1, FOLIO_OBJECTS_LOG2);
		if (folio->objects[i].type == cap_void)
		  panic ("Translating %llx/%d; indexed folio /%d object void",
			 addr_prefix (a), addr_depth (a),
			 ADDR_BITS - remaining);

		root = &fake_slot;

#ifdef RM_INTERN
		struct object_desc *fdesc;
		fdesc = object_to_object_desc (object);

		object = object_find (activity, fdesc->oid + i + 1);
		assert (object);
		*root = object_to_cap (object);
#else
		/* We don't use cap_copy as we just need a byte
		   copy.  */
		*root = folio->objects[i];
#endif

		remaining -= FOLIO_OBJECTS_LOG2;
		continue;
	      }

	    default:
	      AS_DUMP;
	      panic ("Can't insert object at %llx/%d: "
		     "%s at 0x%llx/%d does not translate address bits",
		     addr_prefix (a), addr_depth (a),
		     cap_type_string (root->type),
		     addr_prefix (a), addr_depth (a) - remaining);
	    }
	}
      else
	/* We can get here due to two scenarios: ROOT is void or the
	   the addresses at which we want to insert the object does
	   not match the guard at ROOT.  Perhaps in the former and
	   definately in the latter, we need to introduce a level of
	   indirection.

	   R - ROOT
	   E - ENTRY
	   C - new cappage

              | <-root_depth-> |   mismatch -> |    <- gbits
              |                | <- match      C    <- new page table
              R                R             /   \  <- common guard,
              |                |            R     \    index and
              o                o            |      \   remaining guard
            /   \           /  |  \         o       E
           o     o         o   E   o      /   \
                               ^         o     o
                          just insert */
	{
	  /* For convenience, we prefer that cappages occur at /44,
	     /36, /28, etc.  This is useful as when we insert another
	     page that conflicts with the guard, we can trivially make
	     use of either 7- or 8-bit cappages rather than smaller
	     subppages.  Moreover, it ensures that as paths are
	     decompressed, the tree remains relatively shallow.  The
	     reason we don't choose /43 is that folios are 19-bits
	     wide, while cappages are 8-bits and data pages 12.

	     Consider an AS with a single page, the root having a
	     20-bit guard:

		       o
		       | <- 20 bit guard
		       o <- page

	     If we insert another page and there is a common guard of
	     1-bit, we could reuse this bit:

		      o
		      | <--- 1 bit guard
		      o <--- 8-bit cappage
		     / \ <-- 11-bit guards
		    o   o <- pages

	     The problem with this is when we insert a third page that
	     does not share the guard:

		      o
		      |
		      o   <- 1-bit subpage
		     / \
		    o   o <- 8-bit cappage 
		   / \  | <- 11-bit guards
		  o   o o

	     In this case, we would prefer a guard of 4 at the top.

	     Managing the tree would also become a pain when removing
	     entries.  */

	  /* The number of bits until the next object.  */
	  int tilobject;

	  /* GBITS is the amount of guard that we use to point to the
	     cappage we will allocate.

	     REMAINER - GBITS - log2 (sizeof (cappage)) is the guard
	     length of each entry in the new page.  */
	  int gbits;
	  if (root->type == cap_void)
	    {
	      int space = l4_msb64 (extract_bits64 (addr, 0, remaining));
	      if (space <= CAP_ADDR_TRANS_GUARD_SUBPAGE_BITS)
		/* The slot is available and the remaining bits to
		   translate fit in the guard.  */
		break;

	      gbits = tilobject = remaining;
	    }
	  else
	    /* Find the size of the common prefix.  */
	    {
	      l4_uint64_t a = root_guard;
	      int max = root_gbits > remaining ? remaining : root_gbits;
	      l4_uint64_t b = extract_bits64_inv (addr, remaining - 1, max);
	      if (remaining < root_gbits)
		a >>= root_gbits - remaining;

	      gbits = max - l4_msb64 (a ^ b);

	      tilobject = root_gbits;
	    }

	  /* Make sure that the guard to use fits in the guard
	     area.  */
	  int firstset = l4_msb64 (extract_bits64_inv (addr,
						       remaining - 1, gbits));
	  if (firstset > CAP_ADDR_TRANS_GUARD_SUBPAGE_BITS)
	    /* FIRSTSET is the first (most significant) non-zero guard
	       bit.  GBITS - FIRSTSET are the number of zero bits
	       before the most significant non-zero bit.  We can
	       include all of the initial zero bits plus up to the
	       next CAP_ADDR_TRANS_GUARD_SUBPAGE_BITS bits.  */
	    gbits = (gbits - firstset) + CAP_ADDR_TRANS_GUARD_SUBPAGE_BITS;

	  /* We want to choose the guard length such that the cappage
	     that we insert occurs at certain positions so as minimize
	     small partial cappages and painful rearrangements of the
	     tree.  In particular, we want the total remaining bits to
	     translate after accounting the guard to be equal to
	     FOLIO_OBJECTS_LOG2 + i * CAPPAGE_SLOTS_LOG2 where i >= 0.
	     As GBITS is maximal, we may have to remove guard bits to
	     achieve this.  */
	  int untranslated_bits = remaining + ADDR_BITS - addr_depth (a);

	  struct as_guard_cappage gc
	    = as_compute_gbits_cappage (untranslated_bits,
					tilobject, gbits);
	  assert (gc.gbits <= gbits);
	  assert (gc.gbits + gc.cappage_width <= tilobject);
	  gbits = gc.gbits;

	  /* Account the bits translated by the guard.  */
	  remaining -= gbits;

	  int subpage_bits = gc.cappage_width;
	  assert (subpage_bits >= 0);
	  assert (subpage_bits <= CAPPAGE_SLOTS_LOG2);

	  /* Allocate new cappage and rearrange the tree.  */
	  /* XXX: If we use a subpage, we just ignore the rest of the
	     page.  This is a bit of a waste but makes the code
	     simpler.  */
	  /* ALLOCATE_OBJECT wants the number of significant bits
	     translated to this object; REMAINING is number of bits
	     remaining to translate.  */
	  addr_t cappage_addr = addr_chop (a, remaining);
	  struct as_insert_rt rt = allocate_object (cap_cappage, cappage_addr);
	  if (rt.cap.type == cap_void)
	    /* No memory.  */
	    return NULL;

	  cappage = cap_to_object (activity, &rt.cap);

	  /* Indirect access to the object designated by ROOT via the
	     appropriate slot in new cappage (the pivot).  */
	  int pivot_idx = extract_bits_inv (root_guard,
					    root_gbits - gbits - 1,
					    subpage_bits);

	  addr_t pivot_addr = addr_extend (rt.storage,
					   pivot_idx,
					   CAPPAGE_SLOTS_LOG2);
	  addr_t root_addr = addr_chop (cappage_addr, gbits);

	  struct cap_addr_trans addr_trans = root->addr_trans;
	  int d = tilobject - gbits - subpage_bits;
	  CAP_ADDR_TRANS_SET_GUARD (&addr_trans,
				    extract_bits64 (root_guard, 0, d), d);

	  bool r = cap_copy_x (activity,
			       &cappage->caps[pivot_idx], pivot_addr,
			       *root, root_addr,
			       CAP_COPY_COPY_ADDR_TRANS_GUARD, addr_trans);
	  assert (r);

	  /* Finally, set the slot at ROOT to point to CAPPAGE.  */
	  root_guard = extract_bits64_inv (root_guard,
					   root_gbits - 1, gbits);
	  r = CAP_ADDR_TRANS_SET_GUARD_SUBPAGE (&addr_trans,
						root_guard, gbits,
						0 /* We always use the
						     first subpage in
						     a page.  */,
						1 << (CAPPAGE_SLOTS_LOG2
						      - subpage_bits));
	  assert (r);

	  r = cap_copy_x (activity, root, root_addr, rt.cap, rt.storage,
			  CAP_COPY_COPY_ADDR_TRANS_SUBPAGE
			  | CAP_COPY_COPY_ADDR_TRANS_GUARD,
			  addr_trans);
	  assert (r);
	}

      /* Index CAPPAGE finding the next PTE.  */

      /* The cappage referenced by ROOT translates WIDTH bits.  */
      int width = CAP_SUBPAGE_SIZE_LOG2 (root);
      /* That should not be more than we have left to translate.  */
      if (width > remaining)
	{
	  AS_DUMP;
	  panic ("Translating " ADDR_FMT ": can't index %d-bit cappage; "
		 "not enough bits (%d)",
		 ADDR_PRINTF (a), width, remaining);
	}
      int idx = extract_bits64_inv (addr, remaining - 1, width);
      root = &cappage->caps[CAP_SUBPAGE_OFFSET (root) + idx];

      remaining -= width;
    }
  while (remaining > 0);

  if (! may_overwrite)
    assert (root->type == cap_void);

  int gbits = remaining;
  l4_word_t guard = extract_bits64 (addr, 0, gbits);
  if (gbits != CAP_GUARD_BITS (root) || guard != CAP_GUARD (root))
    {
      struct cap_addr_trans addr_trans = CAP_ADDR_TRANS_VOID;
      bool r = CAP_ADDR_TRANS_SET_GUARD_SUBPAGE (&addr_trans, guard, gbits,
						 0, 1);
      assert (r);

      r = cap_copy_x (activity, root, addr_chop (a, gbits),
		      *root, addr_chop (a, gbits),
		      CAP_COPY_COPY_ADDR_TRANS_GUARD, addr_trans);
      assert (r);
    }

  return root;
}

/* Ensure that the slot designated by A is accessible.  */
struct cap *
as_slot_ensure_full (activity_t activity,
		     struct cap *root, addr_t a,
		     struct as_insert_rt
		     (*allocate_object) (enum cap_type type, addr_t addr))
{
  AS_LOCK;

  struct cap *cap = as_build_internal (activity, root, a,
				       allocate_object, true);

  AS_UNLOCK;

  return cap;
}

void
as_insert (activity_t activity,
	   struct cap *root, addr_t addr, struct cap entry, addr_t entry_addr,
	   struct as_insert_rt (*allocate_object) (enum cap_type type,
						   addr_t addr))
{
#ifndef RM_INTERN
  pthread_rwlock_wrlock (&as_lock);
#endif

  struct cap *slot = as_build_internal (activity, root, addr, allocate_object,
					false);
  cap_copy (activity, slot, addr, entry, entry_addr);

#ifndef RM_INTERN
  pthread_rwlock_unlock (&as_lock);
#endif
}

static void
print_nr (int width, l4_int64_t nr, bool hex)
{
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
    putchar (' ');

  if (hex)
    printf ("0x%llx", nr);
  else
    printf ("%lld", nr);
}

static void
do_walk (activity_t activity, int index, struct cap *root, addr_t addr,
	 int indent, const char *output_prefix)
{
  int i;

  struct cap cap = cap_lookup_rel (activity, root, addr, -1, NULL);
  if (cap.type == cap_void)
    return;

  if (! cap_to_object (activity, &cap))
    /* Cap is there but the object has been deallocated.  */
    return;

  if (output_prefix)
    printf ("%s: ", output_prefix);
  for (i = 0; i < indent; i ++)
    printf (".");

  printf ("[ ");
  if (index != -1)
    print_nr (3, index, false);
  else
    printf ("root");
  printf (" ] ");

  print_nr (12, addr_prefix (addr), true);
  printf ("/%d ", addr_depth (addr));
  if (CAP_GUARD_BITS (&cap))
    printf ("| 0x%llx/%d ", CAP_GUARD (&cap), CAP_GUARD_BITS (&cap));
  if (CAP_SUBPAGES (&cap) != 1)
    printf ("(%d/%d) ", CAP_SUBPAGE (&cap), CAP_SUBPAGES (&cap));

  if (CAP_GUARD_BITS (&cap)
      && ADDR_BITS - addr_depth (addr) >= CAP_GUARD_BITS (&cap))
    printf ("=> 0x%llx/%d ",
	    addr_prefix (addr_extend (addr,
				      CAP_GUARD (&cap),
				      CAP_GUARD_BITS (&cap))),
	    addr_depth (addr) + CAP_GUARD_BITS (&cap));

#ifdef RM_INTERN
  printf ("@%llx ", cap.oid);
#endif
  printf ("%s", cap_type_string (cap.type));

  printf ("\n");

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
		 indent + 1, output_prefix);

      return;

    case cap_folio:
      if (addr_depth (addr) + FOLIO_OBJECTS_LOG2 > ADDR_BITS)
	return;

      for (i = 0; i < FOLIO_OBJECTS; i ++)
	do_walk (activity, i, root,
		 addr_extend (addr, i, FOLIO_OBJECTS_LOG2),
		 indent + 1, output_prefix);

      return;

    default:
      return;
    }
}

/* AS_LOCK must not be held.  */
void
as_dump_from (activity_t activity, struct cap *root, const char *prefix)
{
  do_walk (activity, -1, root, ADDR (0, 0), 0, prefix);
}
