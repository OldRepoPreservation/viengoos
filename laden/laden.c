/* laden.c - Main function for laden.
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

#include "laden.h"


/* The program name.  */
char *program_name = "laden";

rootserver_t kernel;
rootserver_t sigma0;
rootserver_t sigma1;
rootserver_t rootserver;

/* The boot info to be inserted into the L4 KIP.  */
l4_word_t boot_info;


struct l4_memory_desc memory_map[MEMORY_MAP_MAX];

l4_word_t memory_map_size;


/* Return the number of memory descriptors.  */
l4_word_t
loader_get_num_memory_desc (void)
{
  return memory_map_size;
}


/* Return the NRth memory descriptor.  The first memory descriptor is
   indexed by 0.  */
l4_memory_desc_t
loader_get_memory_desc (l4_word_t nr)
{
  return &memory_map[nr];
}


/* Load the ELF images of the kernel and the initial servers into
   memory, checking for overlaps.  Update the start and end
   information with the information from the ELF program, and fill in
   the entry points.  */
static void
load_components (void)
{
  if (!kernel.low)
    panic ("No L4 kernel found");
  loader_add_region ("kernel-mod", kernel.low, kernel.high);

  if (!sigma0.low)
    panic ("No sigma0 server found");
  loader_add_region ("sigma0-mod", sigma0.low, sigma0.high);

  if (sigma1.low)
    loader_add_region ("sigma1-mod", sigma1.low, sigma1.high);

  if (!rootserver.low)
    panic ("No rootserver server found");
  loader_add_region ("rootserver-mod", rootserver.low, rootserver.high);

  loader_elf_load ("kernel", kernel.low, kernel.high,
		   &kernel.low, &kernel.high, &kernel.ip);
  loader_remove_region ("kernel-mod");

  loader_elf_load ("sigma0", sigma0.low, sigma0.high,
		   &sigma0.low, &sigma0.high, &sigma0.ip);
  loader_remove_region ("sigma0-mod");

  if (sigma1.low)
    {
      loader_elf_load ("sigma1", sigma1.low, sigma1.high,
		       &sigma1.low, &sigma1.high, &sigma1.ip);
      loader_remove_region ("sigma1-mod");
    }

  loader_elf_load ("rootserver", rootserver.low, rootserver.high,
		   &rootserver.low, &rootserver.high, &rootserver.ip);
  loader_remove_region ("rootserver-mod");
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

	  printf ("Report bugs to " BUG_ADDRESS ".\n", argv[0]);
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
      else if (!strcmp (argv[i], "-r") || !strcmp (argv[i], "--reset"))
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

  debug ("%s " PACKAGE_VERSION "\n", program_name);

  find_components ();

  load_components ();

  kip_fixup ();

  debug ("Entering kernel at address 0x%x...\n", kernel.ip);

  output_deinit ();

  /* FIXME.  Flush D-cache?  */

  (*(void (*) (void)) kernel.ip) ();

  /* Should not be reached.  */
  shutdown ();

  /* Never reached.  */
  return 0;
}
