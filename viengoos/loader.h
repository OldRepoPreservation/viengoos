/* loader.h - Load ELF binary images, interfaces.
   Copyright (C) 2003, 2007 Free Software Foundation, Inc.
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

#ifndef RM_LOADER_H
#define RM_LOADER_H	1

#include <l4/types.h>

#include "cap.h"
#include "as.h"

/* Forwards.  */
struct activity;
struct thread;

/* Load the ELF image from START to END into memory under the name
   NAME (also used as the name for the region of the resulting ELF
   program).  Return the entry point in ENTRY.  */
extern void loader_elf_load (allocate_object_callback_t alloc,
			     struct activity *activity, struct thread *thread,
			     const char *name, l4_word_t start, l4_word_t end,
			     l4_word_t *entry);

#endif	/* RM_LOADER_H */
