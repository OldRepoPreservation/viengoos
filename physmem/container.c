/* Main function for physical memory server.
   Copyright (C) 2003 Free Software Foundation, Inc.
   Written by Marcus Brinkmann.

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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>

#include <hurd/cap-server.h>

#include "physmem.h"




/* The maximum number of fpages required to cover a page aligned range
   of memory.  This is k if the maximum memory range size to cover is
   2^(k + min_page_size_log2), which can be easily proved by
   induction.  The minimum page size in L4 is at least
   L4_MIN_PAGE_SIZE.  We also need to have each fpage aligned to a
   multiple of its own size.  This makes the proof by induction a bit
   more convoluted, but does not change the result.  */
/* FIXME: Could be made more architecture specific wrt the minimum
   page size.  */
#define MAX_FPAGES (sizeof (l4_word_t) * 8 - L4_MIN_PAGE_SIZE_LOG2)


struct container
{
  /* The capability object must be the first member of this
     struct.  */
  struct hurd_cap_obj obj;

  /* For now, a container is nothing more than a contiguous,
     page-aligned range of memory.  This is the reason why MAX_FPAGES
     are sufficient.  */
  l4_fpage_t fpages[MAX_FPAGES];

  /* The number of entries in FPAGES.  */
  l4_word_t nr_fpages;

  /* True if mapped.  */
  bool mapped;
};
typedef struct container *container_t;


error_t
container_demuxer (hurd_cap_rpc_context_t ctx)
{
  return 0;
}



static struct hurd_cap_class container_class;

/* Initialize the container class subsystem.  */
error_t
container_class_init ()
{
  return hurd_cap_class_init (&container_class, sizeof (struct container),
			      __alignof__ (struct container),
			      NULL, NULL, NULL, NULL, container_demuxer);
}


/* Allocate a new container object covering the NR_FPAGES fpages
   listed in FPAGES.  The object is locked and has one reference.  */
error_t
container_alloc (l4_word_t nr_fpages, l4_word_t *fpages, bool mapped,
		 hurd_cap_obj_t *r_obj)
{
  error_t err;
  container_t container;

  err = hurd_cap_class_alloc (&container_class, (hurd_cap_obj_t *) &container);
  if (err)
    return err;

  assert (nr_fpages <= MAX_FPAGES);
  container->nr_fpages = nr_fpages;
  memcpy (container->fpages, fpages, sizeof (l4_fpage_t) * nr_fpages);
  container->mapped = mapped;

  *r_obj = &container->obj;
  return 0;
}
