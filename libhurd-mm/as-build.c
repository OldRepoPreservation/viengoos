/* as-build.c - Address space composition helper functions.
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

#include <stddef.h>
#include <assert.h>

#include <viengoos/cap.h>
#include <hurd/stddef.h>
#include <viengoos/folio.h>
#include <viengoos/thread.h>
#include <viengoos/messenger.h>
#include <hurd/as.h>
#include <viengoos/misc.h>

#ifndef RM_INTERN
# include <hurd/storage.h>
#endif

#include "as-compute-gbits.h"
#include "bits.h"

#ifdef RM_INTERN
#include "../viengoos/object.h"
#endif

#ifdef ID_SUFFIX
# define CUSTOM
#endif

#ifndef RM_INTERN
# include <hurd/trace.h>

# ifdef CUSTOM
extern struct trace_buffer as_trace;
# else
/* The buffer is protected by the as_lock lock.  */
struct trace_buffer as_trace = TRACE_BUFFER_INIT ("as_trace", 0,
						  true, true, false);
# endif

# define DEBUG(level, fmt, ...)					\
  do								\
    {								\
      debug (level, fmt, ##__VA_ARGS__);			\
      trace_buffer_add (&as_trace, fmt, ##__VA_ARGS__);		\
    }								\
  while (0)

# define PANIC(fmt, ...)					\
  do								\
    {								\
      trace_buffer_dump (&as_trace, 0);				\
      panic (fmt, ##__VA_ARGS__);				\
    }								\
  while (0)

#else

# define DEBUG(level, fmt, ...)					\
  debug (level, fmt, ##__VA_ARGS__)

# define PANIC(fmt, ...)					\
  panic (fmt, ##__VA_ARGS__)

#endif


#ifdef RM_INTERN
# define AS_DUMP as_dump_from (activity, as_root, __func__)
#else
# define AS_DUMP vg_as_dump (VG_ADDR_VOID, as_root_addr)
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
static inline struct vg_cap *
do_index (vg_activity_t activity, struct vg_cap *pte, vg_addr_t pt_addr, int idx,
	  struct vg_cap *fake_slot)
{
  assert (pte->type == vg_cap_cappage || pte->type == vg_cap_rcappage
	  || pte->type == vg_cap_folio
	  || pte->type == vg_cap_thread
	  || pte->type == vg_cap_messenger || pte->type == vg_cap_rmessenger);

  /* Load the referenced object.  */
  struct vg_object *pt = vg_cap_to_object (activity, pte);
  if (! pt)
    /* PTE's type was not void but its designation was invalid.  This
       can only happen if we inserted an object and subsequently
       destroyed it.  */
    {
      /* The type should now have been set to vg_cap_void.  */
      assert (pte->type == vg_cap_void);
      PANIC ("No object at " VG_ADDR_FMT, VG_ADDR_PRINTF (pt_addr));
    }

  switch (pte->type)
    {
    case vg_cap_cappage:
    case vg_cap_rcappage:
      return &pt->caps[VG_CAP_SUBPAGE_OFFSET (pte) + idx];

    case vg_cap_folio:;
      struct vg_folio *folio = (struct vg_folio *) pt;

      if (vg_folio_object_type (folio, idx) == vg_cap_void)
	PANIC ("Can't use void object at " VG_ADDR_FMT " for address translation",
	       VG_ADDR_PRINTF (pt_addr));

      *fake_slot = vg_folio_object_cap (folio, idx);

      return fake_slot;

    case vg_cap_thread:
      assert (idx < VG_THREAD_SLOTS);
      return &pt->caps[idx];
      
    case vg_cap_messenger:
      /* Note: rmessengers don't expose their capability slots.  */
      assert (idx < VG_MESSENGER_SLOTS);
      return &pt->caps[idx];
      
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
struct vg_cap *
ID (as_build) (vg_activity_t activity,
	       vg_addr_t as_root_addr, struct vg_cap *as_root, vg_addr_t addr,
	       as_allocate_page_table_t allocate_page_table
	       OBJECT_INDEX_PARAM,
	       bool may_overwrite)
{
  struct vg_cap *pte = as_root;

  DEBUG (5, DEBUG_BOLD ("Ensuring slot at " VG_ADDR_FMT) " may overwrite: %d",
	 VG_ADDR_PRINTF (addr), may_overwrite);
  assert (! VG_ADDR_IS_VOID (addr));

  /* The number of bits to translate.  */
  int remaining = vg_addr_depth (addr);
  /* The REMAINING bits to translates are in the REMAINING most significant
     bits of PREFIX.  Here it is more convenient to have them in the
     lower bits.  */
  uint64_t prefix = vg_addr_prefix (addr) >> (VG_ADDR_BITS - remaining);

  /* Folios are not made up of capability slots and cannot be written
     to.  When traversing a folio, we manufacture a capability to used
     object in FAKE_SLOT.  If ADDR ends up designating such a
     capability, we fail.  */
  struct vg_cap fake_slot;

  do
    {
      vg_addr_t pte_addr = vg_addr_chop (addr, remaining);

      DEBUG (5, "Cap at " VG_ADDR_FMT ": " VG_CAP_FMT " -> " VG_ADDR_FMT " (%p); "
	     "remaining: %d",
	     VG_ADDR_PRINTF (pte_addr),
	     VG_CAP_PRINTF (pte),
	     VG_ADDR_PRINTF (vg_addr_chop (addr,
				     remaining - VG_CAP_GUARD_BITS (pte))),
#ifdef RM_INTERN
	     NULL,
#else
	     vg_cap_get_shadow (pte),
#endif
	     remaining);

      AS_CHECK_SHADOW (as_root_addr, pte_addr, pte, {});

      uint64_t pte_guard = VG_CAP_GUARD (pte);
      int pte_gbits = VG_CAP_GUARD_BITS (pte);

      uint64_t addr_guard;
      if (remaining >= pte_gbits)
	addr_guard = extract_bits64_inv (prefix,
					 remaining - 1, pte_gbits);
      else
	addr_guard = -1;

      /* If PTE's guard matches, the designated page table translates
	 our address.  Otherwise, we need to insert a page table and
	 indirect access to the object designated by PTE via it.  */

      if (pte_gbits == remaining && pte_guard == addr_guard)
	/* Use the current pte, perhaps overwriting an existing
	   object.  We only reuse a PTE if it has no guard bits.  If
	   the slot is void and has guard, we assume that it has been
	   ensured and is about to be used.  If the slot is being used
	   in this way and we are here, the guard must be non-zero as
	   the other context may only use a slot if it owns the
	   area.  */
	break;
      else if ((pte->type == vg_cap_cappage || pte->type == vg_cap_rcappage
		|| pte->type == vg_cap_folio
		|| pte->type == vg_cap_thread
		|| pte->type == vg_cap_messenger)
	       && remaining >= pte_gbits
	       && pte_guard == addr_guard)
	/* PTE's (possibly zero-width) guard matches and the
	   designated object translates VG_ADDR.  We index the object
	   below.  */
	{
	  remaining -= pte_gbits;
	  DEBUG (5, "Matched guard %lld/%d, remaining: %d",
		 pte_guard, pte_gbits, remaining);

	  /* If REMAINING is 0, we should have executed the preceding
	     if clause.  */
	  assert (remaining > 0);
	}
      else
	/* There are two scenarios that lead us here: (1) the pte
	   designates an object that does not translate bits or (2)
	   the addresses at which we want to insert the object does
	   not match the guard at PTE.  Perhaps in the former (as we
	   only have 22 guard bits) and definately in the latter, we
	   need to introduce a new page table.

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

	  /* The minimum number of bits until either the object or the
	     object that is in the way.  */
	  int tilobject;

	  /* GBITS is the amount of guard that we use to point to the
	     cappage we will allocate.

	     REMAINER - GBITS - log2 (sizeof (cappage)) is the guard
	     length of the pte in the new cappage.  */
	  int gbits;

	  bool need_pivot = ! (pte->type == vg_cap_void && pte_gbits == 0);
	  if (! need_pivot)
	    /* The slot is available.  */
	    {
	      int space = vg_msb64 (extract_bits64 (prefix, 0, remaining));
	      if (space <= VG_CAP_ADDR_TRANS_GUARD_SUBPAGE_BITS)
		/* The remaining bits to translate fit in the
		   guard, we are done.  */
		break;

	      /* The guard value requires more than
		 VG_CAP_ADDR_TRANS_GUARD_SUBPAGE_BITS bits.  We need to
		 insert a page table.  */
	      gbits = tilobject = remaining;
	    }
	  else
	    /* PTE designates a live object.  Find the size of the
	       common prefix.  */
	    {
	      uint64_t a = pte_guard;
	      int max = pte_gbits > remaining ? remaining : pte_gbits;
	      uint64_t b = extract_bits64_inv (prefix, remaining - 1, max);
	      if (remaining < pte_gbits)
		a >>= pte_gbits - remaining;

	      gbits = max - vg_msb64 (a ^ b);

	      tilobject = pte_gbits;
	    }

	  /* Make sure that the guard to use fits in the guard
	     area.  */
	  int firstset = vg_msb64 (extract_bits64_inv (prefix,
						       remaining - 1, gbits));
	  if (firstset > VG_CAP_ADDR_TRANS_GUARD_SUBPAGE_BITS)
	    /* FIRSTSET is the first (most significant) non-zero guard
	       bit.  GBITS - FIRSTSET are the number of zero bits
	       before the most significant non-zero bit.  We can
	       include all of the initial zero bits plus up to the
	       next VG_CAP_ADDR_TRANS_GUARD_SUBPAGE_BITS bits.  */
	    gbits -= firstset - VG_CAP_ADDR_TRANS_GUARD_SUBPAGE_BITS;

	  /* We want to choose the guard length such that the cappage
	     that we insert occurs at certain positions so as minimize
	     small partial cappages and painful rearrangements of the
	     tree.  In particular, we want the total remaining bits to
	     translate after accounting the guard to be equal to
	     VG_FOLIO_OBJECTS_LOG2 + i * VG_CAPPAGE_SLOTS_LOG2 where i >= 0.
	     As GBITS is maximal, we may have to remove guard bits to
	     achieve this.  */
	  int untranslated_bits = remaining + VG_ADDR_BITS - vg_addr_depth (addr);

	  if (! (untranslated_bits > 0 && tilobject > 0 && gbits >= 0
		 && untranslated_bits >= tilobject
		 && untranslated_bits >= gbits
		 && tilobject >= gbits))
	    PANIC ("untranslated_bits: %d, tilobject: %d, gbits: %d",
		   untranslated_bits, tilobject, gbits);

	  struct as_guard_cappage gc
	    = as_compute_gbits_cappage (untranslated_bits,
					tilobject, gbits);
	  assert (gc.gbits <= gbits);
	  assert (gc.gbits + gc.cappage_width <= tilobject);
	  gbits = gc.gbits;

	  /* Account the bits translated by the guard.  */
	  remaining -= gbits;

	  int pt_width = gc.cappage_width;
	  if (! (pt_width > 0 && pt_width <= VG_CAPPAGE_SLOTS_LOG2))
	    PANIC ("pt_width: %d", pt_width);

	  /* Allocate a new page table.  */
	  /* XXX: If we use a subpage, we just ignore the rest of the
	     page.  This is a bit of a waste but makes the code
	     simpler.  */
	  vg_addr_t pt_addr = vg_addr_chop (addr, remaining);
	  struct as_allocate_pt_ret rt = allocate_page_table (pt_addr);
	  if (rt.cap.type == vg_cap_void)
	    /* No memory.  */
	    return NULL;

	  struct vg_cap pt_cap = rt.cap;
	  vg_addr_t pt_phys_addr = rt.storage;
	  /* do_index requires that the subpage specification be
	     correct.  */
	  VG_CAP_SET_SUBPAGE (&pt_cap,
			   0, 1 << (VG_CAPPAGE_SLOTS_LOG2 - pt_width));



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

	  int pivot_idx = extract_bits_inv (pte_guard,
					    pte_gbits - gbits - 1,
					    pt_width);
	  vg_addr_t pivot_addr = vg_addr_extend (pt_addr,
					   pivot_idx, pt_width);
	  vg_addr_t pivot_phys_addr = vg_addr_extend (pt_phys_addr,
						pivot_idx,
						VG_CAPPAGE_SLOTS_LOG2);

	  int pivot_gbits = pte_gbits - gbits - pt_width;
	  int pivot_guard = extract_bits64 (pte_guard, 0, pivot_gbits);

	  if (! VG_ADDR_EQ (vg_addr_extend (pivot_addr, pivot_guard, pivot_gbits),
			 vg_addr_extend (pte_addr, pte_guard, pte_gbits)))
	    {
	      PANIC ("old pte target: " VG_ADDR_FMT " != pivot target: " VG_ADDR_FMT,
		     VG_ADDR_PRINTF (vg_addr_extend (pte_addr,
					       pte_guard, pte_gbits)),
		     VG_ADDR_PRINTF (vg_addr_extend (pivot_addr,
					       pivot_guard, pivot_gbits)));
	    }

	  DEBUG (5, VG_ADDR_FMT ": indirecting pte at " VG_ADDR_FMT
		 " -> " VG_ADDR_FMT " " VG_CAP_FMT " with page table/%d at "
		 VG_ADDR_FMT "(%p) " "common guard: %d, remaining: %d;  "
		 "old target (need pivot: %d) now via pt[%d] "
		 "(" VG_ADDR_FMT "-> " DEBUG_BOLD (VG_ADDR_FMT) ")",
		 VG_ADDR_PRINTF (addr),
		 VG_ADDR_PRINTF (pte_addr),
		 VG_ADDR_PRINTF (vg_addr_extend (pte_addr, VG_CAP_GUARD (pte),
					   VG_CAP_GUARD_BITS (pte))),
		 VG_CAP_PRINTF (pte),
		 pt_width, VG_ADDR_PRINTF (pt_addr),
#ifdef RM_INTERN
		 NULL,
#else
		 vg_cap_get_shadow (&pt_cap),
#endif
		 gbits, remaining,
		 need_pivot, pivot_idx, VG_ADDR_PRINTF (pivot_addr),
		 VG_ADDR_PRINTF (vg_addr_extend (pivot_addr,
					   pivot_guard, pivot_gbits)));

	  /* 1.) Copy the PTE into the new page table.  Adjust the
	     guard in the process.  This is only necessary if PTE
	     actually designates something.  */
	  struct vg_cap *pivot_cap = NULL;
	  if (need_pivot)
	    {
	      /* 1.a) Get the pivot PTE.  */

	      pivot_cap = do_index (activity,
				    &pt_cap, pt_addr, pivot_idx,
				    &fake_slot);
	      assert (pivot_cap != &fake_slot);

	      /* 1.b) Make the pivot designate the object the PTE
		 currently designates.  */
	      struct vg_cap_addr_trans addr_trans = VG_CAP_ADDR_TRANS_VOID;

	      bool r;
	      r = VG_CAP_ADDR_TRANS_SET_GUARD (&addr_trans,
					    pivot_guard, pivot_gbits);
	      assert (r);

	      r = vg_cap_copy_x (activity,
			      VG_ADDR_VOID, pivot_cap, pivot_phys_addr,
			      as_root_addr, *pte, pte_addr,
			      VG_CAP_COPY_COPY_ADDR_TRANS_GUARD,
			      VG_CAP_PROPERTIES (VG_OBJECT_POLICY_DEFAULT,
					      addr_trans));
	      assert (r);
	    }

	  /* 2) Set PTE to point to the new page table (PT).  */
	  pte_guard = extract_bits64_inv (pte_guard,
					  pte_gbits - 1, gbits);
	  pte_gbits = gbits;

	  struct vg_cap_addr_trans addr_trans = VG_CAP_ADDR_TRANS_VOID;
	  bool r;
	  r = VG_CAP_ADDR_TRANS_SET_GUARD_SUBPAGE (&addr_trans,
						pte_guard, pte_gbits,
						0 /* We always use the
						     first subpage in
						     a page.  */,
						1 << (VG_CAPPAGE_SLOTS_LOG2
						      - pt_width));
	  assert (r);

	  r = vg_cap_copy_x (activity, as_root_addr, pte, pte_addr,
			  VG_ADDR_VOID, pt_cap, rt.storage,
			  VG_CAP_COPY_COPY_ADDR_TRANS_SUBPAGE
			  | VG_CAP_COPY_COPY_ADDR_TRANS_GUARD,
			  VG_CAP_PROPERTIES (VG_OBJECT_POLICY_DEFAULT, addr_trans));
	  assert (r);

#ifndef NDEBUG
# ifndef CUSTOM
	  /* We can't use this check with a custom function as
	     as_lookup does not (yet) support a custom indexer.  */
	  if (need_pivot)
	    {
	      union as_lookup_ret rt;
	      bool ret = as_lookup_rel (activity,
					as_root, pivot_addr, -1,
					NULL, as_lookup_want_slot,
					&rt);
	      if (! (ret && rt.capp == pivot_cap))
		as_dump_from (activity, as_root, "");
	      assertx (ret && rt.capp == pivot_cap,
		       VG_ADDR_FMT ": %sfound, got %p, expected %p",
		       VG_ADDR_PRINTF (pivot_addr),
		       ret ? "" : "not ", ret ? rt.capp : 0, pivot_cap);

	      AS_CHECK_SHADOW (as_root_addr, pivot_addr, pivot_cap, { });
	      AS_CHECK_SHADOW (as_root_addr, pte_addr, pte, { });
	    }
# endif
#endif
	}

      /* Index the object designated by PTE to find the next PTE.  The
	 guard has already been translated.  */
      int width;
      switch (pte->type)
	{
	case vg_cap_cappage:
	case vg_cap_rcappage:
	  width = VG_CAP_SUBPAGE_SIZE_LOG2 (pte);
	  break;

	case vg_cap_folio:
	  width = VG_FOLIO_OBJECTS_LOG2;
	  break;

	case vg_cap_thread:
	  width = VG_THREAD_SLOTS_LOG2;
	  break;

	case vg_cap_messenger:
	  /* Note: rmessengers don't expose their capability slots.  */
	  width = VG_MESSENGER_SLOTS_LOG2;
	  break;

	default:
	  AS_DUMP;
	  PANIC ("Can't insert object at " VG_ADDR_FMT ": "
		 VG_CAP_FMT " does translate address bits",
		 VG_ADDR_PRINTF (addr),
		 VG_CAP_PRINTF (pte));
	}

      /* That should not be more than we have left to translate.  */
      if (width > remaining)
	{
	  AS_DUMP;
	  PANIC ("Translating " VG_ADDR_FMT ": can't index %d-bit %s at "
		 VG_ADDR_FMT "; not enough bits (%d)",
		 VG_ADDR_PRINTF (addr), width, vg_cap_type_string (pte->type),
		 VG_ADDR_PRINTF (vg_addr_chop (addr, remaining)), remaining);
	}

      int idx = extract_bits64_inv (prefix, remaining - 1, width);

      enum vg_cap_type type = pte->type;
      pte = do_index (activity, pte, vg_addr_chop (addr, remaining), idx,
		      &fake_slot);
      if (! pte)
	PANIC ("Failed to index object at " VG_ADDR_FMT,
	       VG_ADDR_PRINTF (vg_addr_chop (addr, remaining)));

      if (type == vg_cap_folio)
	assert (pte == &fake_slot);
      else
	assert (pte != &fake_slot);

      remaining -= width;

      DEBUG (5, "Indexing %s/%d[%d]; remaining: %d",
	     vg_cap_type_string (type), width, idx, remaining);

      if (remaining == 0)
	AS_CHECK_SHADOW (as_root_addr, addr, pte, {});
    }
  while (remaining > 0);

  if (! (pte->type == vg_cap_void && VG_CAP_GUARD_BITS (pte) == 0))
    /* PTE in use.  */
    {
      if (remaining != VG_CAP_GUARD_BITS (pte)
	  && extract_bits64 (prefix, 0, remaining) != VG_CAP_GUARD (pte))
	DEBUG (0, "Overwriting " VG_CAP_FMT " at " VG_ADDR_FMT " -> " VG_ADDR_FMT,
	       VG_CAP_PRINTF (pte),
	       VG_ADDR_PRINTF (addr),
	       VG_ADDR_PRINTF (vg_addr_extend (addr, VG_CAP_GUARD (pte),
					 VG_CAP_GUARD_BITS (pte))));

      if (may_overwrite)
	{
	  DEBUG (5, "Overwriting " VG_CAP_FMT " at " VG_ADDR_FMT " -> " VG_ADDR_FMT,
		 VG_CAP_PRINTF (pte),
		 VG_ADDR_PRINTF (addr),
		 VG_ADDR_PRINTF (vg_addr_extend (addr, VG_CAP_GUARD (pte),
					   VG_CAP_GUARD_BITS (pte))));
	  /* XXX: Free any data associated with the capability
	     (e.g., shadow pages).  */
	}
      else
	{
	  AS_DUMP;
	  PANIC ("There is already an object at " VG_ADDR_FMT
		 " (" VG_CAP_FMT ") but may not overwrite.",
		 VG_ADDR_PRINTF (addr),
		 VG_CAP_PRINTF (pte));
	}
    }


  int gbits = remaining;
  /* It is safe to use an int as a guard has a most 22 significant
     bits.  */
  int guard = extract_bits64 (prefix, 0, gbits);
  if (gbits != VG_CAP_GUARD_BITS (pte) || guard != VG_CAP_GUARD (pte))
    {
      struct vg_cap_addr_trans addr_trans = VG_CAP_ADDR_TRANS_VOID;
      bool r = VG_CAP_ADDR_TRANS_SET_GUARD_SUBPAGE (&addr_trans, guard, gbits,
						 0, 1);
      assert (r);
      r = vg_cap_copy_x (activity, as_root_addr, pte, vg_addr_chop (addr, gbits),
		      as_root_addr, *pte, vg_addr_chop (addr, gbits),
		      VG_CAP_COPY_COPY_ADDR_TRANS_GUARD,
		      VG_CAP_PROPERTIES (VG_OBJECT_POLICY_DEFAULT, addr_trans));
      assert (r);

      AS_CHECK_SHADOW (as_root_addr, vg_addr_chop (addr, gbits), pte, { });
    }

#ifndef NDEBUG
# ifndef CUSTOM
  /* We can't use this check with a custom function as as_lookup does
     not (yet) support a custom indexer.  */
  {
    union as_lookup_ret rt;
    bool ret = as_lookup_rel (activity,
			      as_root, addr, -1,
			      NULL, as_lookup_want_slot,
			      &rt);
    if (! (ret && rt.capp == pte))
      as_dump_from (activity, as_root, "");
    assertx (ret && rt.capp == pte,
	     VG_ADDR_FMT ": %sfound, got %p, expected %p",
	     VG_ADDR_PRINTF (addr),
	     ret ? "" : "not ", ret ? rt.capp : 0, pte);
  }
# endif
#endif

  return pte;
}
