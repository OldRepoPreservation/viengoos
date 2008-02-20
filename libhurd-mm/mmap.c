/* mmap.c - mmap implementation.
   Copyright (C) 2007 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU Hurd is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#include <hurd/stddef.h>
#include <hurd/addr.h>
#include <hurd/as.h>
#include <hurd/storage.h>
#include <hurd/anonymous.h>

#include <sys/mman.h>
#include <stdint.h>

void *
mmap (void *addr, size_t length, int protect, int flags,
      int filedes, off_t offset)
{
  if (length == 0)
    return MAP_FAILED;

  if ((flags & ~(MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED)))
    panic ("mmap called with invalid flags");
  if (protect != (PROT_READ | PROT_WRITE))
    panic ("mmap called with unsupported protection");

  if ((flags & MAP_FIXED))
    /* Interpret ADDR exactly.  */
    {
      if (((uintptr_t) addr & (PAGESIZE - 1)))
	return MAP_FAILED;
    }
  else
    {
      /* Round the address down to a multiple of the pagesize.  */
      addr = (void *) ((uintptr_t) addr & ~(PAGESIZE - 1));
    }

  /* Round length up.  */
  length = (length + PAGESIZE - 1) & ~(PAGESIZE - 1);

  struct anonymous_pager *pager;
  pager = anonymous_pager_alloc (ADDR_VOID, addr, length,
				 OBJECT_POLICY_DEFAULT,
				 (flags & MAP_FIXED) ? ANONYMOUS_FIXED: 0,
				 NULL, &addr);
  if (! pager)
    panic ("Failed to create pager!");

  return addr;
}

int
munmap (void *addr, size_t length)
{
  l4_word_t start = (l4_word_t) addr;
  l4_word_t end = start + length - 1;

  struct pager_region region;
  region.start = PTR_TO_ADDR (addr);
  region.count = length;

  debug (5, "(%p, %x (%p))", addr, length, addr + length - 1);

  /* There is a race. We can't hold PAGERS_LOCK when we call
     PAGER->DESTROY.  In particular, the destroy function may also
     want to unmap some memory.  We can't grab one, drop the lock,
     destroy and repeat until all the pagers in the region are gone:
     another call to mmap could reuse a region we unmaped.  Then
     we'd unmap that new region.

     To make unmap atomic, we grab the lock, disconnect all pagers in
     the region, adding each to a list, and then destroy each pager on
     that list.  */
  struct pager *list = NULL;

  ss_mutex_lock (&pagers_lock);

  struct pager *pager = hurd_btree_pager_find (&pagers, &region);
  if (! pager)
    {
      ss_mutex_unlock (&pagers_lock);
      return 0;
    }

  struct pager *prev = hurd_btree_pager_prev (pager);

  while (pager)
    {
      struct pager *next = hurd_btree_pager_next (pager);

      l4_uint64_t pager_start = addr_prefix (pager->region.start);
      l4_uint64_t pager_end = pager_start
	+ (pager->region.count
	   << (ADDR_BITS - addr_depth (pager->region.start))) - 1;

      if (pager_start > end)
	break;

      if (pager_start < start || pager_end > end)
	{
	  debug (0, "munmap (%x-%x), pager at %llx-%llx",
		 start, end, pager_start, pager_end);
	  panic ("Attempt to partially unmap pager unsupported");
	}

      pager_deinstall (pager);
      pager->next = list;
      list = pager;

      pager = next;
    }

  pager = prev;
  while (pager)
    {
      struct pager *prev = hurd_btree_pager_next (pager);

      l4_uint64_t pager_start = addr_prefix (pager->region.start);
      l4_uint64_t pager_end = pager_start
	+ (pager->region.count
	   << (ADDR_BITS - addr_depth (pager->region.start))) - 1;

      if (pager_end < start)
	break;

      if (pager_start < start)
	panic ("Attempt to partially unmap pager unsupported");

      pager_deinstall (pager);
      pager->next = list;
      list = pager;

      pager = prev;
    }

  ss_mutex_unlock (&pagers_lock);

  pager = list;
  while (pager)
    {
      struct pager *next = pager->next;

      l4_uint64_t pager_start = addr_prefix (pager->region.start);
      l4_uint64_t pager_end = pager_start
	+ (pager->region.count
	   << (ADDR_BITS - addr_depth (pager->region.start))) - 1;
      debug (5, "Detroying pager covering %llx-%llx (%d pages)",
	     pager_start, pager_end,
	     (int) (pager_end - pager_start + 1) / PAGESIZE);

      pager->destroy (pager);

      pager = next;
    }

  return 0;
}
