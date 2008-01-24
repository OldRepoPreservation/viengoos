/* as.h - Address space composition helper functions interface.
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

#ifndef RM_AS_H
#define RM_AS_H

#include <l4.h>
#include <hurd/cap.h>
#include <hurd/folio.h>

struct as_guard_cappage
{
  int gbits;
  int cappage_width;
};

/* Given UNTRANSLATED bits and a maximum guard length of GBITS, return
   the optimal guard length.  */
static inline struct as_guard_cappage
as_compute_gbits_cappage (int untranslated_bits, int to_translate,
			  int gbits)
{
  /* Our strategy is as follows: we want to avoid 1) having to move
     page tables around, and 2) small cappages.  We know that folios
     will be mapped such that their data pages are visible in the data
     address space of the process, i.e., at /ADDR_BITS-7-12.  Thus, we
     try to ensure that we have 7-bit cappages at /ADDR_BITS-7-12 and
     then 8-bit cappage at /ADDR_BITS-7-12-i*8, i > 0, i.e., /44, /36,
     etc.  */

  assert (untranslated_bits > 0);
  assert (to_translate > 0);
  assert (gbits >= 0);

  assert (untranslated_bits >= to_translate);
  assert (untranslated_bits >= gbits);
  assert (to_translate >= gbits);

  if (untranslated_bits <= PAGESIZE_LOG2)
    ;

  /* There could be less than PAGESIZE_LOG2 untranslated bits.  Place
     a cappage at /ADDR_BITS-PAGESIZE_LOG2.

      UNTRANSLATED_BITS
     |--------------------|
     |-------|
      GBITS
           |--------------|
            PAGESIZE_LOG2

           ^
  */
  else if (untranslated_bits - gbits <= PAGESIZE_LOG2)
    gbits = untranslated_bits - PAGESIZE_LOG2;

  /* There could be less than FOLIO_OBJECTS_LOG2 + PAGESIZE_LOG2
     untranslated bits.  Place a cappage at
     /ADDR_BITS-FOLIO_OBJECTS_LOG2-PAGESIZE_LOG2.

      UNTRANSLATED_BITS
     |--------------------|
     |-------|
      GBITS
           |------|-------|
           |       PAGESIZE_LOG2
           `FOLIO_OBJECTS_LOG2

	   ^ 
  */
  else if (untranslated_bits - gbits <= FOLIO_OBJECTS_LOG2 + PAGESIZE_LOG2)
    gbits = untranslated_bits - FOLIO_OBJECTS_LOG2 - PAGESIZE_LOG2;

  /* 
           UNTRANSLATED_BITS
     |-----------------------------|

     |----------|-------|----|-----|
     |          |       |     PAGESIZE_LOG2
     |          |       `FOLIO_OBJECTS_LOG2
     `GBITS     `REMAINDER

     Shrink GBITS such that REMAINDER becomes a multiple of
     CAPPAGE_SLOTS_LOG2.
   */
  else
    {
      int remainder = untranslated_bits - gbits
	- FOLIO_OBJECTS_LOG2 - PAGESIZE_LOG2;

      /* Amount to remove from GBITS such that REMAINDER + TO_REMOVE is a
	 multiple of CAPPAGE_SLOTS_LOG2.  */
      int to_remove = CAPPAGE_SLOTS_LOG2 - (remainder % CAPPAGE_SLOTS_LOG2);

      if (to_remove < gbits)
	gbits -= to_remove;
      else
	gbits = 0;
    }

  assert (gbits >= 0);

  struct as_guard_cappage gc;
  if (untranslated_bits - gbits == FOLIO_OBJECTS_LOG2 + PAGESIZE_LOG2)
    gc.cappage_width = FOLIO_OBJECTS_LOG2;
  else
    gc.cappage_width = CAPPAGE_SLOTS_LOG2;

  if (gbits + gc.cappage_width > to_translate)
    gc.cappage_width = to_translate - gbits;

  gc.gbits = gbits;

  return gc;
}

struct as_insert_rt
{
  struct cap cap;
  addr_t storage;
};

/* Callback used by the following function.  When called must return a
   cap designating an object of type TYPE.  */
typedef struct as_insert_rt allocate_object_callback_t (enum cap_type type,
							addr_t addr);

/* Copy the capability located at SOURCE_ADDR in the address space
   rooted at SOURCE_AS to address ADDR in the address space rooted at
   TARGET_AS.  Allocates any necessary page-tables in the target
   address space.  ALLOC_OBJECT is a callback to allocate an object of
   type TYPE at address ADDR.  The callback should NOT insert the
   allocated object into the addresss space.  */
extern void as_insert (activity_t activity,
		       addr_t target_as, struct cap *t_as_cap, addr_t target,
		       addr_t source_as, struct cap c_cap, addr_t source,
		       allocate_object_callback_t alloc);

/* Ensure that the slot designated by ADDR in the address space root
   at AS is accessible by allocating any required page tables.  Return
   the cap associated with ADDR.  */
extern struct cap *as_slot_ensure_full
  (activity_t activity,
   addr_t as, struct cap *root, addr_t addr,
   struct as_insert_rt
     (*allocate_object) (enum cap_type type, addr_t addr));

/* If debugging is enabled dump the address space described by ROOT.
   PREFIX is prefixed to each line of output.  */
extern void as_dump_from (activity_t activity, struct cap *root,
			  const char *prefix);

#endif
