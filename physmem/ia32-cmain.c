/* ia32-cmain.c - Startup code for the ia32.
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

#include <alloca.h>
#include <stdint.h>

#include <l4/globals.h>
#include <l4/init.h>
#include <l4/stubs.h>
#include <l4/stubs-init.h>

#include "physmem.h"


/* Initialize libl4, setup the argument vector, and pass control over
   to the main function.  */
void
cmain (void)
{
  int argc = 0;
  char **argv = 0;

  l4_init ();
  l4_init_stubs ();

  argc = 1;
  argv = alloca (sizeof (char *) * 2);
  argv[0] = program_name;
  argv[1] = 0;

  /* Now invoke the main function.  */
  main (argc, argv);

  /* Never reached.  */
}
