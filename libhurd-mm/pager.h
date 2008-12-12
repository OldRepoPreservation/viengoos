/* pager.h - Generic pager interface.
   Copyright (C) 2007, 2008 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

   This file is part of the GNU Hurd.

   GNU Hurd is free software: you can redistribute it and/or modify it
   under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   GNU Hurd is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with GNU Hurd.  If not, see
   <http://www.gnu.org/licenses/>.  */

#ifndef _HURD_PAGER_H
#define _HURD_PAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <hurd/map.h>

/* Forward.  */
struct pager;

/* Install the COUNT pages starting at offset OFFSET into the address
   space starting at address ADDR.  If RO is true, then only install
   them read-only.  If an access occurred, it is described by IP and
   INFO.  Return true if the fault was handler (and an appropriate
   mapping installed), otherwise, false.  */
typedef bool (*pager_fault_t) (struct pager *pager,
			       uintptr_t offset, int count, bool ro,
			       uintptr_t fault_addr, uintptr_t ip,
			       struct activation_fault_info info);

/* The count sub-trees starting at ADDR are no longer referenced and
   their associated storage may be reclaimed.  */
typedef void (*pager_reclaim_t) (struct pager *pager,
				 addr_t addr, int count);

/* Called when the last map to a pager has been destroyed.  (This
   function should not call pager_deinit!)  Called with PAGER->LOCK
   held.  This function should unlock PAGER->LOCK, if required.  */
typedef void (*pager_no_refs_t) (struct pager *pager);

enum
  {
    pager_advice_normal,
    pager_advice_random,
    pager_advice_sequential,
    pager_advice_willneed,
    pager_advice_dontneed,
  };

/* Called to suggest some action on a range of pages.  This function
   is called with MAPS_LOCK held.  It should not be released.  */
typedef void (*pager_advise_t) (struct pager *pager,
				uintptr_t start, uintptr_t length,
				uintptr_t advice);

struct pager
{
  /* List of maps that reference this pager.  This is protected by
     LOCK.  */
  struct map *maps;
  ss_mutex_t lock;

  /* The length of the object, in bytes.  */
  uintptr_t length;


  pager_fault_t fault;

  pager_no_refs_t no_refs;

  pager_advise_t advise;
};

#define PAGER_VOID { NULL, 0, 0, NULL, NULL, NULL }

/* Initialize the pager.  All fields must be set appropriately.  After
   calling this function, LENGTH and FAULT may no longer be
   changed.  */
extern bool pager_init (struct pager *pager);

/* Deinitialize the pager PAGER, destroying all the mappings in the
   process.  Takes MAPS_LOCKS.  */
extern void pager_deinit (struct pager *pager);

#endif
