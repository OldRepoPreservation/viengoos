/* wortel.h - Generic definitions.
   Copyright (C) 2003 Free Software Foundation, Inc.
   Written by Marcus Brinkmann.

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
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA. */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <l4.h>

#include "output.h"
#include "shutdown.h"
#include "loader.h"


/* The program name.  */
extern char *program_name;

#define BUG_ADDRESS	"<bug-hurd@gnu.org>"


typedef __l4_rootserver_t rootserver_t;

/* For the boot components, find_components() must fill in the start
   and end address of the ELF images in memory.  The end address is
   one more than the last byte in the image.  */
extern rootserver_t physmem;

/* Find the kernel, the initial servers and the other information
   required for booting.  */
void find_components (void);

int main (int argc, char *argv[]);
