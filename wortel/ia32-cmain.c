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

#include "wortel-intern.h"
#include "multiboot.h"
#include "sigma0.h"


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
  debug ("Multiboot Info: %p\n", mbi);

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
      argv[0] = (char *) program_name;
      argv[1] = 0;
    }

  /* Now invoke the main function.  */
  main (argc, argv);

  /* Never reached.  */
}


/* The following must be defined and are used to calculate the extents
   of the laden binary itself.  _END is one more than the address of
   the last byte.  */
extern char _start;
extern char _end;


/* Add the area from START to END (inclusive) to the list of unused
   pages.  */
static void
add_unused_area (l4_word_t start, l4_word_t size)
{
  l4_word_t min_page_size = l4_min_page_size ();
  l4_word_t end = start + size - 1;

  debug ("%s: add region 0x%" L4_PRIxWORD " with size 0x%" L4_PRIxWORD "\n",
	 __func__, start, size);

  /* Round down START and END.  */
  start = start & ~(min_page_size - 1);
  end = end & ~(min_page_size - 1);

  for (; start <= end; start += min_page_size)
    {
      unsigned int i;

      /* Only add the page if it was not added by us before.  We only
	 check the address, and not the size, because we only have to
	 check against the pages we add ourselves at this point, and
	 we only add pages of the smallest possible size.  */
      for (i = 0; i < wortel_unused_fpages_count; i++)
	if (l4_address (wortel_unused_fpages[i]) == start)
	  break;

      if (i == wortel_unused_fpages_count)
	{
	  l4_fpage_t fpage = l4_fpage (start, min_page_size);
	  fpage = l4_fpage_add_rights (fpage, L4_FPAGE_FULLY_ACCESSIBLE);
	  sigma0_get_fpage (fpage);
	  wortel_unused_fpages[i] = fpage;
	  wortel_unused_fpages_count++;
	}
    }
}


/* Find the kernel, the initial servers and the other information
   required for booting.  */
void
find_components (void)
{
  multiboot_info_t *mbi = (multiboot_info_t *) l4_boot_info ();

  /* Load the module information.  */
  if (CHECK_FLAG (mbi->flags, 3))
    {
      module_t *mod = (module_t *) mbi->mods_addr;
      unsigned int i;

      /* Add the argument string of the first module to the list of
	 unused pages.  */
      add_unused_area ((l4_word_t) mod[0].string,
		       strlen ((char *) mod[0].string) + 1);

      mods_count = mbi->mods_count - 1;
      if (mods_count > MOD_NUMBER)
	mods_count = MOD_NUMBER;

      /* Skip the entry for the rootserver.  */
      mod++;

      for (i = 0; i < mods_count; i++)
	{
	  char *args;
	  unsigned int old_mods_args_len;

	  mods[i].name = mod_names[i];
	  mods[i].start = mod[i].mod_start;
	  mods[i].end = mod[i].mod_end;

	  /* We copy over the argument lines, so that we don't depend
	     on the multiboot info structure anymore, and can reuse
	     that memory.  */
	  mods[i].args = &mods_args[mods_args_len];
	  args = (char *) mod[i].string;
	  old_mods_args_len = mods_args_len;
	  while (*args && mods_args_len < sizeof (mods_args))
	    mods_args[mods_args_len++] = *(args++);
	  if (mods_args_len == sizeof (mods_args))
	    panic ("No space to store the argument lines");
	  mods_args[mods_args_len++] = '\0';

	  /* Now we have to add the source string's area to the list
	     of unused pages, as we touched that memory.  */
	  add_unused_area ((l4_word_t) mod[i].string,
			   mods_args_len - old_mods_args_len);
	}

      /* Add the module info itself to the list of unused pages.  */
      add_unused_area ((l4_word_t) mbi->mods_addr,
		       mbi->mods_count * sizeof (module_t));
    }

  /* Add the multiboot info to the list of unused pages.  */
  add_unused_area ((l4_word_t) mbi, sizeof (*mbi));

  /* Finally initialize the wortel area variables.  */
  wortel_start = (l4_word_t) &_start;
  wortel_end = (l4_word_t) &_end;
}
