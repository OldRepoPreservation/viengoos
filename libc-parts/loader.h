/* loader.h - Load ELF binary images, interfaces.
   Copyright (C) 2003, 2007, 2008 Free Software Foundation, Inc.
   Written by Marcus Brinkmann.

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

#ifndef _LOADER_H
#define _LOADER_H	1

#include <stdint.h>
#include <stdbool.h>

/* Allocate a page at address ADDR in the new task's address space.
   Returns the address of the page.  If RO is true, the reference
   inserted into the task's address space should be made
   read-only.  */
typedef void *loader_allocate_object_callback_t (uintptr_t addr, bool ro);

/* If there is a page at ADDR in the new task's address space, return
   return the page, otherwise, NULL.  */
typedef void *loader_lookup_object_callback_t (uintptr_t addr);

/* Load the ELF image from START to END into memory under the name
   NAME (also used as the name for the region of the resulting ELF
   program).  On success, returns true the entry point in *ENTRY.
   Otherwise, false.  */
extern bool loader_elf_load (loader_allocate_object_callback_t alloc,
			     loader_lookup_object_callback_t lookup,
			     void *start, void *end, uintptr_t *entry);

#endif	/* _LOADER_H */
