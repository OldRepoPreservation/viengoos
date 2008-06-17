/* map.c - Generic map implementation.
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "pager.h"

extern ss_mutex_t maps_lock;

bool
pager_init (struct pager *pager)
{
  assert (pager->fault);
  assert (pager->length);
  assert ((pager->length & (PAGESIZE - 1)) == 0);

  debug (3, "Creating pager with size %x (%d pages)",
	 pager->length, pager->length / PAGESIZE);

  pager->maps = 0;
  pager->lock = 0;

  return true;
}

void
pager_deinit (struct pager *pager)
{
  maps_lock_lock ();

  /* Destroy all but one references with MAPS_LOCK held.  When we
     destroy the last reference, map_destroy will call the pager's no
     ref call back, which may destroy PAGER.  */

  ss_mutex_lock (&pager->lock);
  assert (pager->maps);
  for (;;)
    {
      struct map *map = pager->maps;
      map_disconnect (map);

      if (! map->map_list_next)
	/* The last reference.  */
	{
	  maps_lock_unlock ();

	  map_destroy (map);

	  return;
	}

      /* This drops PAGER->LOCK.  */
      map_destroy (map);
      ss_mutex_lock (&pager->lock);
    }
}
