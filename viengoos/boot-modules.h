/* boot-modules.h - Boot module interface.
   Copyright (C) 2007 Free Software Foundation, Inc.
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

#ifndef RM_BOOT_MODULES_H
#define RM_BOOT_MODULES_H

#include <l4.h>

struct boot_module
{
  l4_word_t start;
  l4_word_t end;
  char *command_line;
};

#define BOOT_MODULES_MAX 32

/* These must be filled in by the architecture specific code.  */
extern struct boot_module boot_modules[BOOT_MODULES_MAX];
extern int boot_module_count;

#endif
