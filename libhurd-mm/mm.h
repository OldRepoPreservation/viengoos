/* mm.h - Memory management interface.
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

#ifndef HURD_MM_MM_H
#define HURD_MM_MM_H

#include <hurd/addr.h>

/* Set to one by mm_init just before returning.  */
extern int mm_init_done;

/* Initialize the memory management sub-system.  ACTIVITY is the
   activity to use to account meta-data resources.  */
extern void mm_init (addr_t activity);

#endif /* HURD_MM_MM_H */
