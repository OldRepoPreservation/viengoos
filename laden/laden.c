/* laden.c - Main function for laden.
   Copyright (C) 2003, 2005, 2007 Free Software Foundation, Inc.
   Written by Marcus Brinkmann.

   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 3, or (at
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

#include "laden.h"


/* The program name.  */
char *program_name = "laden";

l4_rootserver_t kernel;
l4_rootserver_t sigma0;
l4_rootserver_t sigma1;
l4_rootserver_t rootserver;

/* The boot info to be inserted into the L4 KIP.  */
l4_word_t boot_info;


static void
rootserver_relocate (const char *name,
		     l4_word_t start, l4_word_t end, l4_word_t new_start,
		     void *cookie)
{
  l4_rootserver_t *s = cookie;
  s->low = new_start;
  s->high = new_start + (end - start);
}

/* Load the ELF images of the kernel and the initial servers into
   memory, checking for overlaps.  Update the start and end
   information with the information from the ELF program, and fill in
   the entry points.  */
static void
load_components (void)
{
  /* Make sure that the required components are available and mark the
     memory their packed images occupy as used.  */
  if (!kernel.low)
    panic ("No L4 kernel found");
  loader_add_region ("kernel-mod", kernel.low, kernel.high,
		     rootserver_relocate, &kernel, -1);

  if (!sigma0.low)
    panic ("No sigma0 server found");
  loader_add_region ("sigma0-mod", sigma0.low, sigma0.high,
		     rootserver_relocate, &sigma0, -1);

 if (sigma1.low)
    loader_add_region ("sigma1-mod", sigma1.low, sigma1.high,
		       rootserver_relocate, &sigma1, -1);

  if (!rootserver.low)
    panic ("No rootserver server found");
  loader_add_region ("rootserver-mod", rootserver.low, rootserver.high,
		     rootserver_relocate, &rootserver, -1);

  /* Since we did not panic, there are no conflicts and we can now
     unpack the images.  */
  loader_elf_load ("kernel", kernel.low, kernel.high,
		   &kernel.low, &kernel.high, &kernel.ip, -1);
  loader_remove_region ("kernel-mod");

  loader_elf_load ("sigma0", sigma0.low, sigma0.high,
		   &sigma0.low, &sigma0.high, &sigma0.ip, -1);
  loader_remove_region ("sigma0-mod");
#ifdef _L4_V2
  /* Use the page following the extracted image as the stack.  */
  /* XXX: Should reserve this?  */
  sigma0.sp = ((sigma0.high + 0xfff) & ~0xfff) + 0x1000;
#endif

  if (sigma1.low)
    {
      loader_elf_load ("sigma1", sigma1.low, sigma1.high,
		       &sigma1.low, &sigma1.high, &sigma1.ip, -1);
      loader_remove_region ("sigma1-mod");
    }

  loader_elf_load ("rootserver", rootserver.low, rootserver.high,
		   &rootserver.low, &rootserver.high, &rootserver.ip, -1);
  loader_remove_region ("rootserver-mod");
#ifdef _L4_V2
  /* Use the page following the extracted image as the stack.  */
  /* XXX: Should reserve this?  */
  rootserver.sp = ((rootserver.high + 0xfff) & ~0xfff) + 0x1000;
#endif
}


static void
parse_args (int argc, char *argv[])
{
  int i = 1;

  while (i < argc)
    {
      if (!strcmp (argv[i], "--usage"))
	{
	  i++;
	  printf ("Usage %s [OPTION...]\n", argv[0]);
	  printf ("Try `%s --help' for more information\n", program_name);
	  shutdown ();	  
	}
      else if (!strcmp (argv[i], "--help"))
	{
	  struct output_driver **drv = output_drivers;

	  i++;
	  printf ("Usage: %s [OPTION...]\n"
		  "\n"
		  "Load the L4 kernel, sigma0 and the rootserver, then "
		  "start the system.\n"
		  "\n"
		  "  -o, --output DRV  use output driver DRV\n"
		  "  -D, --debug       enable debug output\n"
		  "  -h, --halt        halt the system at error (default)\n"
		  "  -r, --reboot      reboot the system at error\n"
		  "\n"
		  "      --usage       print out some usage information and "
		  "exit\n"
		  "      --help        display this help and exit\n"
		  "      --version     output version information and exit\n"
		  "\n%s\n", argv[0], help_arch ());

	  printf ("Valid output drivers are: ");
	  while (*drv)
	    {
	      printf ("%s", (*drv)->name);
	      if (drv == output_drivers)
		printf (" (default)");
	      drv++;
	      if (*drv && (*drv)->name)
		printf (", ");
	      else
		printf (".\n\n");
	    }

	  printf ("Report bugs to " BUG_ADDRESS ".\n");
	  shutdown ();	  
	}
      else if (!strcmp (argv[i], "--version"))
	{
	  i++;
	  printf ("%s " PACKAGE_VERSION "\n", program_name);
	  shutdown ();	  
	}
      else if (!strcmp (argv[i], "-o") || !strcmp (argv[i], "--output"))
	{
	  i++;
	  if (!output_init (argv[i]))
	    panic ("Unknown output driver %s", argv[i]);
	  i++;
	}
      else if (!strcmp (argv[i], "-h") || !strcmp (argv[i], "--halt"))
	{
	  i++;
	  shutdown_reset = 0;
	}
      else if (!strcmp (argv[i], "-r") || !strcmp (argv[i], "--reboot"))
	{
	  i++;
	  shutdown_reset = 1;
	}
      else if (!strcmp (argv[i], "-D") || !strcmp (argv[i], "--debug"))
	{
	  i++;
	  output_debug = 1;
	}
      else if (argv[i][0] == '-')
	panic ("Unsupported option %s", argv[i]);
      else
	panic ("Invalid non-option argument %s", argv[i]);
    }
}


int
main (int argc, char *argv[])
{
  parse_args (argc, argv);

  debug (1, "%s " PACKAGE_VERSION, program_name);

  find_components ();

  load_components ();

  loader_regions_reserve ();

  kip_fixup ();

  debug (1, "Entering kernel at address 0x%x...", kernel.ip);

  output_deinit ();

  start_kernel (kernel.ip);

  /* Should not be reached.  */
  panic ("kernel returned to bootloader");

  /* Never reached.  */
  return 0;
}
