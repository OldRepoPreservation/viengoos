/* viengoos.h - Viengoos declarations.
   Copyright (C) 2007, 2008 Free Software Foundation, Inc.
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

#ifndef PRIV_H
#define PRIV_H

#include <l4/types.h>

/* The program name, set statically.  */
extern char *program_name;

/* A pointer to the root activity.  */
extern struct activity *root_activity;

/* The arch-independent main function called by the arch dependent
   code.  */
extern int main (int, char *[]);

/* Find the kernel, the initial servers and the other information
   required for booting.  */
extern void find_components (void);

/* Viengoos's tid.  */
l4_thread_id_t viengoos_tid;
/* Ager's tid.  */
l4_thread_id_t ager_tid;

#endif
