/* pager.c - Generic pager implementation.
   Copyright (C) 2007 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2, or (at
   your option) any later version.
   
   The GNU Hurd is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with the GNU Hurd; see the file COPYING.  If not, write to
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139,
   USA.  */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "pager.h"

/* Protects PAGERS and all pager's NODE.  This lock may not be taken
   if a pager's LOCK is held by the caller.  */
ss_mutex_t pagers_lock;
hurd_btree_pager_t pagers;

bool
pager_install (struct pager *pager)
{
  assert (! ss_mutex_trylock (&pagers_lock));

  assert (pager->fault);
  assert (pager->region.count);

  debug (3, "Installing pager at " ADDR_FMT "+%d",
	 ADDR_PRINTF (pager->region.start), pager->region.count);

  struct pager *conflict = hurd_btree_pager_insert (&pagers, pager);
  if (conflict)
    {
      debug (1, "Can't install pager at " ADDR_FMT "+%d; conflicts "
	     "with pager at " ADDR_FMT "+%d",
	     ADDR_PRINTF (pager->region.start), pager->region.count,
	     ADDR_PRINTF (conflict->region.start), conflict->region.count);
      return false;
    }

  return true;
}

bool
pager_relocate (struct pager *pager, struct pager_region region)
{
  assert (! ss_mutex_trylock (&pagers_lock));

  /* XXX: Grows could be a bit smarter.  We could check the next and
     previous pointers, for instance.  */

  /* Detach the pager.  */
  hurd_btree_pager_detach (&pagers, pager);

  /* Save the old region in case there is a conflict.  */
  struct pager_region old = pager->region;

  pager->region = region;
  struct pager *conflict = hurd_btree_pager_insert (&pagers, pager);
  if (conflict)
    /* There is a conflict.  Insert the pager back where it was.  */
    {
      pager->region = old;
      conflict = hurd_btree_pager_insert (&pagers, pager);
      assert (! conflict);

      return false;
    }
  return true;
}

void
pager_deinstall (struct pager *pager)
{
  assert (! ss_mutex_trylock (&pagers_lock));

  hurd_btree_pager_detach (&pagers, pager);
}

static void __attribute__ ((noinline))
ensure_stack(int i)
{
  /* XXX: If we fault on the stack while we have PAGERS_LOCK, we
     deadlock.  Ensure that we have some stack space and hope it is
     enough.  (This can't be too much as we may be running on the
     exception handler's stack.)  */
  volatile char space[1024 + 512];
  space[0] = 0;
  space[sizeof (space) - 1] = 0;
}

bool
pager_fault (addr_t addr, uintptr_t ip, struct exception_info info)
{
  /* Find the pager.  */
  struct pager_region region;
  region.start = addr;
  region.count = 1;

  ensure_stack (1);

  ss_mutex_lock (&pagers_lock);

  struct pager *pager = hurd_btree_pager_find (&pagers, &region);
  if (! pager)
    {
      debug (3, "No pager covers " ADDR_FMT, ADDR_PRINTF (addr));
      ss_mutex_unlock (&pagers_lock);
      return false;
    }

  ss_mutex_lock (&pager->lock);
  ss_mutex_unlock (&pagers_lock);

  /* Propagate the fault.  */
  bool r = pager->fault (pager, addr, ip, info);
  if (! r)
    debug (3, "Pager did not fault " ADDR_FMT, ADDR_PRINTF (addr));

  ss_mutex_unlock (&pager->lock);

  return r;
}
