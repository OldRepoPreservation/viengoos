/* loader.h - Load ELF binary images, interfaces.
   Copyright (C) 2003, 2007 Free Software Foundation, Inc.
   Written by Marcus Brinkmann.

   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 3, or (at
   your option) any later version.

   The GNU Hurd is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA. */

#ifndef _LOADER_H
#define _LOADER_H	1

#include <l4/types.h>
#include <l4/kip.h>


/* The user must provide the following functions.  */

/* Return the number of memory descriptors.  */
l4_word_t loader_get_num_memory_desc (void);

/* Return the NRth memory descriptor.  The first memory descriptor is
   indexed by 0.  */
l4_memory_desc_t *loader_get_memory_desc (l4_word_t nr);


/* Callback passed to loader_add_region.  The region named NAME
   starting at START and continuing until END (exclusive) has been
   relocated to NEW_START.  Update any data structures
   appropriately.  */
typedef void (*relocate_region) (const char *name,
				 l4_word_t start, l4_word_t end,
				 l4_word_t new_start,
				 void *cookie);

/* Add the region with the name NAME from covering memory starting at
   byte START and continuing and including byte END to the table of
   protected region.  Check for overlaps with existing regions.  If at
   some time an overlap is detected and RR is not NULL, the region may
   be relocated.  In this case RR is invoked (and passed COOKIE).  RR
   can then update any pointers referring to the memory.  If DESC_TYPE
   is not -1, then this region will be added as a memory descriptor by
   loader_regions_reserve of type DESC_TYPE.  */
void loader_add_region (const char *name, l4_word_t start, l4_word_t end,
			relocate_region rr, void *cookie,
			int desc_type);

/* Remove any regions with the name NAME from the region descriptor
   table.  */
void loader_remove_region (const char *name);

/* Setup memory descriptors corresponding to the current set of
   reserved descriptors.  Should be called once, immediately before
   jumping into the kernel.  */
void loader_regions_reserve (void);

/* Load the ELF image from START to END into memory under the name
   NAME (also used as the name for the region of the resulting ELF
   program).  Return the lowest and highest address used by the
   program in NEW_START_P and NEW_END_P, and the entry point in ENTRY.
   The used regions are automatically added with the type DESC_TYPE
   (cf., loader_add_region).  */
void loader_elf_load (const char *name, l4_word_t start, l4_word_t end,
		      l4_word_t *new_start_p, l4_word_t *new_end_p,
		      l4_word_t *entry, int desc_type);

#endif	/* _LOADER_H */
