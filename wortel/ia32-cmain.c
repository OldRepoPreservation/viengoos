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
      argv[0] = program_name;
      argv[1] = 0;
    }

  /* Now invoke the main function.  */
  main (argc, argv);

  /* Never reached.  */
}


/* The following must be defined and are used to calculate the extents
   of the laden binary itself.  */
extern char _start;
extern char _end;


/* Find the kernel, the initial servers and the other information
   required for booting.  */
void
find_components (void)
{
  multiboot_info_t *mbi = (multiboot_info_t *) l4_boot_info ();
  l4_word_t start;
  l4_word_t end;

  /* Load the module information.  */
  if (CHECK_FLAG (mbi->flags, 3))
    {
      module_t *mod = (module_t *) mbi->mods_addr;
      unsigned int nr_mods;
      unsigned int i;

      mods_count = mbi->mods_count - 1;
      if (mods_count > MOD_NUMBER)
	mods_count = MOD_NUMBER;
      /* Skip the entry for the rootserver.  */
      mod++;

      for (i = 0; i < nr_mods; i++)
	{
	  mods[i].name = mod_names[i];
	  mods[i].start = mod[i].mod_start;
	  mods[i].end = mod[i].mod_end;
	  mods[i].args = (char *) mod[i].string;
	  mod++;
	}
    }

  /* Now protect ourselves and the multiboot info (at least the module
     configuration).  */
  loader_add_region (program_name, (l4_word_t) &_start, (l4_word_t) &_end);

  start = (l4_word_t) mbi;
  end = start + sizeof (*mbi) - 1;
  loader_add_region ("grub-mbi", start, end);
  
  if (CHECK_FLAG (mbi->flags, 3) && mbi->mods_count)
    {
      module_t *mod = (module_t *) mbi->mods_addr;
      int nr;

      start = (l4_word_t) mod;
      end = ((l4_word_t) mod) + mbi->mods_count * sizeof (*mod);
      loader_add_region ("grub-mods", start, end);

      start = (l4_word_t) mod[0].string;
      end = start;
      for (nr = 0; nr < mbi->mods_count; nr++)
	{
	  char *str = (char *) mod[nr].string;

	  if (str)
	    {
	      if (((l4_word_t) str) < start)
		start = (l4_word_t) str;
	      while (*str)
		str++;
	      if (((l4_word_t) str) > end)
		end = (l4_word_t) str;
	    }
	}
      loader_add_region ("grub-mods-cmdlines", start, end);
    }
}
