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
#include <l4/stubs.h>
#include <l4/stubs-init.h>

#include "wortel.h"
#include "multiboot.h"


/* Check if the bit BIT in FLAGS is set.  */
#define CHECK_FLAG(flags,bit)	((flags) & (1 << (bit)))


/* Initialize libl4, setup the argument vector, and pass control over
   to the main function.  */
void
cmain (void)
{
  multiboot_info_t *mbi;
  int argc = 0;
  char **argv = 0;

  l4_init ();
  l4_init_stubs ();

  mbi = (multiboot_info_t *) l4_boot_info ();
  debug ("Multiboot Info: 0x%x\n", mbi);

#if 0
  if (CHECK_FLAG (mbi->flags, 3))
    {
      module_t *mod = (module_t *) mbi->mods_addr;
      int nr;

      /* FIXME: Should add all modules that we need to start up to the
	 global, architecture independent module list.  */
      for (nr = 0; nr < mbi->mods_count; nr++)
	debug ("Module %i: Start 0x%x, End 0x%x, Cmd %s\n",
	       nr + 1, mod[nr].mod_start, mod[nr].mod_end, mod[nr].string);
    }
#endif

  if (CHECK_FLAG (mbi->flags, 3) && mbi->mods_count > 0)
    {
      /* A command line is available.  */
      module_t *mod = (module_t *) mbi->mods_addr;
      char *str = (char *) mod[0].string;
      int nr = 0;

      /* First time around we count the number of arguments.  */
      argc = 1;
      while (*str && *str == ' ')
	str++;

      while (*str)
	if (*(str++) == ' ')
	  {
	    while (*str && *str == ' ')
	      str++;
	    if (*str)
	      argc++;
	  }
      argv = alloca (sizeof (char *) * (argc + 1));

      /* Second time around we fill in the argv.  */
      str = (char *) mod[0].string;

      while (*str && *str == ' ')
	str++;
      argv[nr++] = str;

      while (*str)
	{
	  if (*str == ' ')
	    {
	      *(str++) = '\0';
	      while (*str && *str == ' ')
		str++;
	      if (*str)
		argv[nr++] = str;
	    }
	  else
	    str++;
	}
      argv[nr] = 0;
    }
  else
    {
      argc = 1;

      argv = alloca (sizeof (char *) * 2);
      argv[0] = PROGRAM_NAME;
      argv[1] = 0;
    }

  /* Now invoke the main function.  */
  main (argc, argv);

  /* Never reached.  */
}
