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


rootserver_t kernel;
rootserver_t sigma0;
rootserver_t sigma1;
rootserver_t rootserver;

/* The boot info to be inserted into the L4 KIP.  */
l4_word_t boot_info;

struct l4_memory_desc memory_map[MEMORY_MAP_MAX];
int memory_map_size;


/* True if debug mode is enabled.  */
int debug;


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
	  printf ("Try `" PROGRAM_NAME " --help' for more information\n");
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
	  printf (PROGRAM_NAME " " PACKAGE_VERSION "\n");
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
	  debug = 1;
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

  debug (PROGRAM_NAME " " PACKAGE_VERSION "\n");

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
