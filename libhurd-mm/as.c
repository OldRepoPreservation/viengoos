/* as.c - Address space management.
   Copyright (C) 2004, 2005 Free Software Foundation, Inc.
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

#include "priv.h"

struct as as;

/* Find a free region of the virtual address space for a region of
   size SIZE with alignment ALIGN.  Must be called with the map lock.
   The lock msut not be dropped until the virtual address is entered
   into the mapping database (and this function should not be called
   again until that has happened).  */
uintptr_t
as_find_free (size_t size, size_t align)
{
  /* Start the probe at the lowest address aligned address after
     VIRTUAL_MEMORY_START.  FIXME: We should use a hint.  But then, we
     shouldn't use linked lists either.  */
  l4_word_t start = (VIRTUAL_MEMORY_START + align - 1) & ~(align - 1);
  bool ok;
  struct map *map;

  do
    {
      /* The proposed start is free unless proven not to be.  */
      ok = true;

      /* Iterate over all of the maps and see if any intersect.  If
	 none do, then we have a good address.  */
      for (map = hurd_btree_map_first (&as.mappings); map;
	   map = hurd_btree_map_next (map))
	if (overlap (start, size, map->vm.start, map->vm.size))
	  {
	    ok = false;
	    /* Use the next aligned virtual address after MAP.  */
	    /* FIXME: We need to check that we don't overflow.  */
	    start = (map->vm.start + map->vm.size + align - 1) & ~(align - 1);
	    break;
	  }
    }
  while (! ok);

  return start;
}
