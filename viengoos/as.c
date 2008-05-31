/* as.c - Address space composition helper functions.
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

#include <l4.h>
#include <stddef.h>
#include <assert.h>

#include <hurd/cap.h>
#include <hurd/stddef.h>
#include <hurd/folio.h>
#include <hurd/exceptions.h>

#ifndef RM_INTERN
# include <hurd/storage.h>
#endif

#include "as.h"
#include "bits.h"
#include "rm.h"

#ifdef RM_INTERN
#include "object.h"
#endif

#ifndef RM_INTERN
static void __attribute__ ((noinline))
ensure_stack(int i)
{
  /* XXX: If we fault on the stack while we have the address space
     lock, we deadlock.  Ensure that we have some stack space and hope
     it is enough.  (This can't be too much as we may be running on
     the exception handler's stack.)  */
  volatile char space[EXCEPTION_STACK_SIZE - PAGESIZE];
  int j;
  for (j = sizeof (space) - 1; j >= 0; j -= PAGESIZE)
    space[j] = i;
}

# ifndef AS_LOCK
#  define AS_LOCK						\
  do								\
     {								\
       extern pthread_rwlock_t as_lock;				\
								\
       ensure_stack (1);					\
       pthread_rwlock_wrlock (&as_lock);			\
     }								\
   while (0)
# endif

# ifndef AS_UNLOCK
#  define AS_UNLOCK				\
  do						\
    {						\
      extern pthread_rwlock_t as_lock;		\
						\
      pthread_rwlock_unlock (&as_lock);		\
    }						\
  while (0)
# endif

# define AS_DUMP rm_as_dump (ADDR_VOID, as_root_addr)

#else

# define AS_LOCK do { } while (0)
# define AS_UNLOCK do { } while (0)
# define AS_DUMP as_dump_from (activity, as_root, __func__);

#endif

/* The following macros allow providing specialized address-space
   construction functions.  */

/* The suffix to append to as_slot_ensure_full_ and as_insert_.  */
#ifdef ID_SUFFIX
# define ID__(a, b) a ## _ ## b
# define ID_(a, b) ID__(a, b)
# define ID(a) ID_(a, ID_SUFFIX)
#else
# define ID(a) a
#endif

/* The callback signature.  For instance:

    #define OBJECT_INDEX_ARG_TYPE index_callback_t
 */
#ifdef OBJECT_INDEX_ARG_TYPE
# define OBJECT_INDEX_PARAM , OBJECT_INDEX_ARG_TYPE do_index
# define OBJECT_INDEX_ARG do_index,
#else

/* When there is no user-supplied callback, we default to traversing
   kernel objects/shadow objects.  */

# define OBJECT_INDEX_PARAM
# define OBJECT_INDEX_ARG

/* PT designates a cappage or a folio.  The cappage or folio is at
   address PT_ADDR.  Index the object designed by PTE returning the
   location of the idx'th capability slot.  If the capability is
   implicit (in the case of a folio), return a fabricated capability
   in *FAKE_SLOT and return FAKE_SLOT.  Return NULL on failure.  */
static inline struct cap *
do_index (activity_t activity, struct cap *pte, addr_t pt_addr, int idx,
	  struct cap *fake_slot)
{
  assert (pte->type == cap_cappage || pte->type == cap_rcappage
	  || pte->type == cap_folio);

  /* Load the referenced object.  */
  struct object *pt = cap_to_object (activity, pte);
  if (! pt)
    /* PTE's type was not void but its designation was invalid.  This
       can only happen if we inserted an object and subsequently
       destroyed it.  */
    {
      /* The type should now have been set to cap_void.  */
      assert (pte->type == cap_void);
      panic ("No object at " ADDR_FMT, ADDR_PRINTF (pt_addr));
    }

  switch (pte->type)
    {
    case cap_cappage:
    case cap_rcappage:
      return &pt->caps[CAP_SUBPAGE_OFFSET (pte) + idx];

    case cap_folio:;
      struct folio *folio = (struct folio *) pt;

      if (folio_object_type (folio, idx) == cap_void)
	panic ("Can't use void object at " ADDR_FMT " for address translation",
	       ADDR_PRINTF (pt_addr));

      *fake_slot = folio_object_cap (folio, idx);

      return fake_slot;

    default:
      return NULL;
    }
}
#endif

/* Build up the address space at AS_ROOT_ADDR such that there is a
   capability slot at address ADDR.  Returns the address of the
   capability slot.

   ALLOCATE_OBJECT is a callback function that is expected to allocate
   a cappage to be used as a page table at address ADDR.  The callback
   function should allocate any necessary shadow page-tables.  The
   callback function may not call address space manipulation
   functions.

   If MAY_OVERWRITE is true, the function may overwrite an existing
   capability.  Otherwise, only capability slots containing a void
   capability are used.  */
static struct cap *
ID (as_build_internal) (activity_t activity,
			addr_t as_root_addr, struct cap *as_root, addr_t addr,
			struct as_insert_rt (*allocate_object) (enum cap_type
								type,
								addr_t addr)
			OBJECT_INDEX_PARAM,
			bool may_overwrite)
{
  struct cap *pte = as_root;

  debug (4, "Ensuring slot at " ADDR_FMT, ADDR_PRINTF (addr));
  assert (! ADDR_IS_VOID (addr));

  /* The number of bits to translate.  */
  int remaining = addr_depth (addr);
  /* The REMAINING bits to translates are in the REMAINING most significant
     bits of PREFIX.  Here it is more convenient to have them in the
     lower bits.  */
  uint64_t prefix = addr_prefix (addr) >> (ADDR_BITS - remaining);

  /* Folios are not made up of capability slots and cannot be written
     to.  When traversing a folio, we manufacture a capability to used
     object in FAKE_SLOT.  If ADDR ends up designating such a
     capability, we fail.  */
  struct cap fake_slot;

  do
    {
      uint64_t pte_guard = CAP_GUARD (pte);
      int pte_gbits = CAP_GUARD_BITS (pte);

      /* If PTE's guard matches, the designated page table translates
	 our address.  Otherwise, we need to insert a page table and
	 indirect access to the object designated by PTE via it.  */

      if (pte->type != cap_void
	  && remaining >= pte_gbits
	  && pte_guard == extract_bits64_inv (prefix, remaining - 1, pte_gbits))
	/* PTE's (possibly zero-width) guard matches and the
	   designated object translates ADDR.  */
	{
	  if (remaining == pte_gbits && may_overwrite)
	    {
	      debug (4, "Overwriting " ADDR_FMT " with " ADDR_FMT
		     " (at " ADDR_FMT ")",
		     ADDR_PRINTF (addr_extend (addr_chop (addr,
							  remaining),
					       pte_guard,
					       pte_gbits)),
		     ADDR_PRINTF (addr),
		     ADDR_PRINTF (addr_chop (addr, remaining)));
	      /* XXX: Free any data associated with the capability
		 (e.g., shadow pages).  */
	      break;
	    }

	  /* Subtract the number of bits the guard translates.  */
	  remaining -= pte_gbits;
	  assert (remaining >= 0);

	  if (remaining == 0)
	    /* PTE is not a void capability yet the guard translates
	       all of the bits and we may not overwrite the
	       capability.  This means that PTE references an object
	       at PREFIX.  This is a problem: we want to insert a
	       capability at PREFIX.  */
	    {
	      AS_DUMP;
	      panic ("There is already a %s object at %llx/%d!",
		     cap_type_string (pte->type),
		     addr_prefix (addr), addr_depth (addr));
	    }

	  /* We index the object designated by PTE below.  */
	}
      else
	/* There are two scenarios that lead us here: (1) the pte is
	   void or (2) the addresses at which we want to insert the
	   object does not match the guard at PTE.  Perhaps in the
	   former (as we only have 22 guard bits) and definately in
	   the latter, we need to introduce a new page table.

	   Consider the second scenario:

	     E - PTE
	     T - PTE target
	     * - New PTE

	   Scenario:

	   [ |E| | | ]         [ |E| | | ]
              | \ pte's           |            <- (1) common guard
              | / guard      [ | | |*| ]       <- (2) new page table
              T                 |   |
                                T  ...         <- (3) pivot T
	*/
	{
	  /* For convenience, we prefer that page tables occur at /44,
	     /36, /28, etc.  This is useful as when we insert another
	     page that conflicts with the guard, we can trivially make
	     use of either 7- or 8-bit cappages rather than smaller
	     subppages.  Moreover, it ensures that as paths are
	     decompressed, the tree remains relatively shallow.  The
	     reason we don't choose /43 is that folios are 19-bits
	     wide while cappages are 8-bits and data pages 12
	     (= 20-bits).

	     Consider an AS with a single page, the pte (*)
	     designating the object has a 20-bit guard:

		[ | | |*| | | ]   <- page table
		       |          <- 20 bit guard
		       o          <- page

	     If we insert another page and there is a common guard of
	     1-bit, we could reuse this bit:

		[ | | | | | | ]   <- page table
		       |          <--- 1 bit guard
		[ | | | | | | ]   <- page table
		     |   |        <-- 11-bit guards
		     o   o        <- pages

	     The problem with this is that if we want to avoid
	     shuffling (which we do), then when we insert a third page
	     that does not share the guard, we end up with small page
	     tables:

	         [ | | | | | | ]   <- page table
		      |
		    [ | ]         <- 1-bit subpage
		    /   \
		   o     o <- 8-bit cappage 
		  / \    | <- 11-bit guards
		 o   o   o

	     In this scenario, a larger guard (4 bits wide) would have
	     been better.

	     An additional reason to prefer larger guards at specific
	     depths is that it makes removing entries from the tree
	     easier.  */

	  /* The number of bits until the next object.  */
	  int tilobject;

	  /* GBITS is the amount of guard that we use to point to the
	     cappage we will allocate.

	     REMAINER - GBITS - log2 (sizeof (cappage)) is the guard
	     length of each entry in the new page.  */
	  int gbits;
	  if (pte->type == cap_void)
	    {
	      int space = l4_msb64 (extract_bits64 (prefix, 0, remaining));
	      if (space <= CAP_ADDR_TRANS_GUARD_SUBPAGE_BITS)
		/* The slot is available and the remaining bits to
		   translate fit in the guard.  */
		break;

	      gbits = tilobject = remaining;
	    }
	  else
	    /* Find the size of the common prefix.  */
	    {
	      uint64_t a = pte_guard;
	      int max = pte_gbits > remaining ? remaining : pte_gbits;
	      uint64_t b = extract_bits64_inv (prefix, remaining - 1, max);
	      if (remaining < pte_gbits)
		a >>= pte_gbits - remaining;

	      gbits = max - l4_msb64 (a ^ b);

	      tilobject = pte_gbits;
	    }

	  /* Make sure that the guard to use fits in the guard
	     area.  */
	  int firstset = l4_msb64 (extract_bits64_inv (prefix,
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
	  int untranslated_bits = remaining + ADDR_BITS - addr_depth (addr);

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

	  /* Allocate a new page table.  */
	  /* XXX: If we use a subpage, we just ignore the rest of the
	     page.  This is a bit of a waste but makes the code
	     simpler.  */
	  /* ALLOCATE_OBJECT wants the number of significant bits
	     translated to this object; REMAINING is number of bits
	     remaining to translate.  */
	  addr_t pt_addr = addr_chop (addr, remaining);
	  struct as_insert_rt rt = allocate_object (cap_cappage, pt_addr);
	  if (rt.cap.type == cap_void)
	    /* No memory.  */
	    return NULL;

	  /* We've now allocated a new page table.  

	     * - PTE
	     & - pivot
	     $ - new PTE

	     Before:                      After:

                   [ |*| | | | ]            [ |*| | | | ]
                      |                        |   <- shortened guard
                      |  <- orig. guard        v
                      v                        [ |&| | |$| ] <- new page table
	              [ | | | | | ]               |
	                                          v
	                                          [ | | | | | ]

	    Algorithm:

	      1) Copy contents of PTE to pivot.
	      2) Set PTE to point to new page table.
	      3) Index new page table to continue address translation
	         (note: the new PTE may be the same as the pivot).
	  */

	  /* 1.a) Get the pivot PTE.  */
	  int pivot_idx = extract_bits_inv (pte_guard,
					    pte_gbits - gbits - 1,
					    subpage_bits);

	  /* do_index requires that the subpage specification be
	     correct.  */
	  struct cap pt_cap = rt.cap;
	  CAP_SET_SUBPAGE (&pt_cap,
			   0, 1 << (CAPPAGE_SLOTS_LOG2 - subpage_bits));

	  struct cap *pivot_cap = do_index (activity,
					    &pt_cap, pt_addr,
					    pivot_idx, &fake_slot);
	  assert (pivot_cap != &fake_slot);

	  addr_t pivot_addr = addr_extend (rt.storage,
					   pivot_idx,
					   CAPPAGE_SLOTS_LOG2);

	  /* 1.b) Make the pivot designate the object the PTE
	     currently designates.  */
	  addr_t pte_addr = addr_chop (pt_addr, gbits);

	  struct cap_addr_trans addr_trans = pte->addr_trans;
	  int d = tilobject - gbits - subpage_bits;
	  CAP_ADDR_TRANS_SET_GUARD (&addr_trans,
				    extract_bits64 (pte_guard, 0, d), d);

	  bool r = cap_copy_x (activity,
			       ADDR_VOID, pivot_cap, pivot_addr,
			       as_root_addr, *pte, pte_addr,
			       CAP_COPY_COPY_ADDR_TRANS_GUARD,
			       CAP_PROPERTIES (OBJECT_POLICY_DEFAULT,
					       addr_trans));
	  assert (r);

	  /* 2) Set PTE to point to PT.  */
	  pte_guard = extract_bits64_inv (pte_guard,
					  pte_gbits - 1, gbits);
	  r = CAP_ADDR_TRANS_SET_GUARD_SUBPAGE (&addr_trans,
						pte_guard, gbits,
						0 /* We always use the
						     first subpage in
						     a page.  */,
						1 << (CAPPAGE_SLOTS_LOG2
						      - subpage_bits));
	  assert (r);

	  r = cap_copy_x (activity, as_root_addr, pte, pte_addr,
			  ADDR_VOID, pt_cap, rt.storage,
			  CAP_COPY_COPY_ADDR_TRANS_SUBPAGE
			  | CAP_COPY_COPY_ADDR_TRANS_GUARD,
			  CAP_PROPERTIES (OBJECT_POLICY_DEFAULT, addr_trans));
	  assert (r);
	}

      /* Index the object designated by PTE to find the next PTE.  The
	 guard has already been translated.  */
      int width;
      switch (pte->type)
	{
	case cap_cappage:
	case cap_rcappage:
	  width = CAP_SUBPAGE_SIZE_LOG2 (pte);
	  break;

	case cap_folio:
	  width = FOLIO_OBJECTS_LOG2;
	  break;

	default:
	  AS_DUMP;
	  panic ("Can't insert object at " ADDR_FMT ": "
		 "%s at " ADDR_FMT " does not translate address bits "
		 "(remaining: %d, gbits: %d, pte guard: %d, my guard: %d)",
		 ADDR_PRINTF (addr), cap_type_string (pte->type),
		 ADDR_PRINTF (addr_chop (addr, remaining)),
		 remaining, pte_gbits, pte_guard,
		 extract_bits64_inv (prefix, remaining - 1, pte_gbits));
	}

      /* That should not be more than we have left to translate.  */
      if (width > remaining)
	{
	  AS_DUMP;
	  panic ("Translating " ADDR_FMT ": can't index %d-bit %s at "
		 ADDR_FMT "; not enough bits (%d)",
		 ADDR_PRINTF (addr), width, cap_type_string (pte->type),
		 ADDR_PRINTF (addr_chop (addr, remaining)), remaining);
	}

      int idx = extract_bits64_inv (prefix, remaining - 1, width);

      enum cap_type type = pte->type;
      pte = do_index (activity, pte, addr_chop (addr, remaining), idx,
		      &fake_slot);
      if (! pte)
	panic ("Failed to index object at " ADDR_FMT,
	       ADDR_PRINTF (addr_chop (addr, remaining)));

      if (type == cap_folio)
	assert (pte == &fake_slot);
      else
	assert (pte != &fake_slot);

      remaining -= width;
    }
  while (remaining > 0);

  if (! may_overwrite)
    assertx (pte->type == cap_void,
	     ADDR_FMT " contains a %s but may not overwrite",
	     ADDR_PRINTF (addr), cap_type_string (pte->type));

  int gbits = remaining;
  /* It is safe to use an int as a guard has a most 22 bits.  */
  int guard = extract_bits64 (prefix, 0, gbits);
  if (gbits != CAP_GUARD_BITS (pte) || guard != CAP_GUARD (pte))
    {
      struct cap_addr_trans addr_trans = CAP_ADDR_TRANS_VOID;
      bool r = CAP_ADDR_TRANS_SET_GUARD_SUBPAGE (&addr_trans, guard, gbits,
						 0, 1);
      assert (r);
      r = cap_copy_x (activity, as_root_addr, pte, addr_chop (addr, gbits),
		      as_root_addr, *pte, addr_chop (addr, gbits),
		      CAP_COPY_COPY_ADDR_TRANS_GUARD,
		      CAP_PROPERTIES (OBJECT_POLICY_DEFAULT, addr_trans));
      assert (r);
    }

  return pte;
}

/* Ensure that the slot designated by A is accessible.  */
struct cap *
ID (as_slot_ensure_full) (activity_t activity,
			  addr_t as_root_addr, struct cap *root, addr_t a,
			  struct as_insert_rt
			  (*allocate_object) (enum cap_type type, addr_t addr)
			  OBJECT_INDEX_PARAM)
{
  AS_LOCK;

  struct cap *cap = ID (as_build_internal) (activity, as_root_addr, root, a,
					    allocate_object, OBJECT_INDEX_ARG
					    true);

  AS_UNLOCK;

#ifndef RM_INTERN
  storage_check_reserve (false);
#endif

  return cap;
}

struct cap *
ID (as_insert) (activity_t activity,
		addr_t as_root_addr, struct cap *root, addr_t addr,
		addr_t entry_as, struct cap entry, addr_t entry_addr,
		struct as_insert_rt (*allocate_object) (enum cap_type type,
							addr_t addr)
		OBJECT_INDEX_PARAM)
{
  AS_LOCK;

  struct cap *slot = ID (as_build_internal) (activity, as_root_addr,
					     root, addr, allocate_object,
					     OBJECT_INDEX_ARG false);
  assert (slot);
  cap_copy (activity, as_root_addr, slot, addr, entry_as, entry, entry_addr);

  AS_UNLOCK;

#ifndef RM_INTERN
  storage_check_reserve (false);
#endif

  return slot;
}
