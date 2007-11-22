/* viengoos.c - Main file for viengoos.
   Copyright (C) 2007 Free Software Foundation, Inc.
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>
#include <unistd.h>
#include <alloca.h>
#include <string.h>

#include <l4/thread-start.h>
#include <l4/pagefault.h>

#include <hurd/startup.h>
#include <hurd/stddef.h>

#include "viengoos.h"
#include "sigma0.h"
#include "memory.h"
#include "boot-modules.h"
#include "loader.h"
#include "cap.h"
#include "object.h"
#include "activity.h"
#include "thread.h"
#include "as.h"
#include "server.h"
#include "shutdown.h"
#include "output.h"


#define BUG_ADDRESS	"<bug-hurd@gnu.org>"

struct activity *root_activity;

/* The program name.  */
const char program_name[] = "viengoos";

/* The following must be defined and are used to calculate the extents
   of the laden binary itself.  _END is one more than the address of
   the last byte.  */
extern char _start;
extern char _end;


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
	  shutdown_machine ();	  
	}
      else if (!strcmp (argv[i], "--help"))
	{
	  struct output_driver **drv = output_drivers;

	  i++;
	  printf ("Usage: %s [OPTION...]\n"
		  "\n"
		  "Boot the Hurd system and wrap the L4 privileged system "
		  "calls.\n"
		  "\n"
		  "  -o, --output DRV  use output driver DRV\n"
		  "  -D, --debug LEVEL enable debug output (1-5)\n"
		  "  -h, --halt        halt the system at error (default)\n"
		  "  -r, --reboot      reboot the system at error\n"
		  "\n"
		  "      --usage       print out some usage information and "
		  "exit\n"
		  "      --help        display this help and exit\n"
		  "      --version     output version information and exit\n"
		  "\n", argv[0]);

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
	  shutdown_machine ();	  
	}
      else if (!strcmp (argv[i], "--version"))
	{
	  i++;
	  printf ("%s " PACKAGE_VERSION "\n", program_name);
	  shutdown_machine ();	  
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
	  if (! ('0' <= argv[i][0] && argv[i][0] <= '9'))
	    panic ("Option -D expects an integer argument");
	  output_debug = atoi (argv[i]);
	  i++;
	}
      else if (argv[i][0] == '-')
	panic ("Unsupported option %s", argv[i]);
      else
	panic ("Invalid non-option argument %s", argv[i]);
    }
}

static void
memory_configure (void)
{
  /* Reserve their memory.  */
  int i;
  for (i = 0; i < boot_module_count; i ++)
    {
      if (! memory_reserve (boot_modules[i].start, boot_modules[i].end,
			    i == 0 ? memory_reservation_system_executable
			    : memory_reservation_modules))
	panic ("Failed to reserve memory for boot module %d (%s).",
	       i, boot_modules[i].command_line);
      if (boot_modules[i].command_line
	  && ! memory_reserve ((l4_word_t) boot_modules[i].command_line,
			       (l4_word_t) boot_modules[i].command_line
			       + strlen (boot_modules[i].command_line),
			       i == 0 ? memory_reservation_system_executable
			       : memory_reservation_modules))
	panic ("Failed to reserve memory for boot module %d's "
	       "command line (%s).",
	       i, boot_modules[i].command_line);
    }

  /* Grab all available physical memory.  */
  if (3 < output_debug)
    memory_reserve_dump ();
  memory_grab ();

  printf ("memory: %x-%x\n", first_frame, last_frame);
}

void
system_task_load (void)
{
  struct hurd_startup_data *startup_data = (void *) memory_frame_allocate ();

  bool boot_strapped = false;

  struct thread *thread;

  /* The area where we will store the hurd object descriptors won't be
     ready until after we have already allocated some objects.  We
     allocate a few descriptors, which should be more than enough.  */
  struct hurd_object_desc *descs = (void *) &startup_data[1];
  int desc_max = ((PAGESIZE - sizeof (struct hurd_startup_data))
		  / sizeof (struct hurd_object_desc));
  struct object *objects[desc_max];
  int desc_count = 0;

  struct folio *folio = NULL;
  int folio_index;
  addr_t folio_addr;

  struct as_insert_rt allocate_object (enum cap_type type, addr_t addr)
    {
      debug (4, "(%s, 0x%llx/%d)",
	     cap_type_string (type), addr_prefix (addr), addr_depth (addr));

      assert (type != cap_void);
      assert (type != cap_folio);

      if (! folio || folio_index == FOLIO_OBJECTS)
	/* Allocate additional storage.  */
	{
	  static int f = 1;

	  folio = folio_alloc (root_activity);
	  folio_index = 0;

	  /* XXX: Allocate more space.  */
	  if (desc_count == desc_max)
	    panic ("Out of object descriptors (binary too big)");
	  int i = desc_count ++;
	  struct hurd_object_desc *desc = &descs[i];
	  /* We allocate a folio such that pages allocated are
	     mappable in the data address space.  */
	  folio_addr = desc->object = ADDR (f << (FOLIO_OBJECTS_LOG2
						  + PAGESIZE_LOG2),
					    ADDR_BITS - FOLIO_OBJECTS_LOG2
					    - PAGESIZE_LOG2);
	  f ++;
	  desc->type = cap_folio;

	  objects[i] = (struct object *) folio;

	  if (boot_strapped)
	    as_insert (root_activity, &thread->aspace, folio_addr,
		       object_to_cap ((struct object *) folio), ADDR_VOID,
		       allocate_object);
	}

      struct object *object;
      int index = folio_index ++;
      folio_object_alloc (root_activity, folio, index, type, &object);

      if (! (desc_count < desc_max))
	panic ("Initial task too large.");

      int i = desc_count ++;
      objects[i] = object;
      struct hurd_object_desc *desc = &descs[i];

      desc->object = addr;
      desc->storage = addr_extend (folio_addr, index, FOLIO_OBJECTS_LOG2);
      desc->type = type;

      struct as_insert_rt rt;
      rt.cap = object_to_cap (object);
      rt.storage = desc->storage;
      return rt;
    }

  /* When allocating objects, we allocate them above 4GB.  */
  l4_uint64_t next_free_page = 1ULL << 32;
  addr_t capability_cappage = ADDR_VOID;
  struct object *capability_cappage_object;
  int capability_cappage_count;

  /* Allocate a capability location.  */
  addr_t csalloc (void)
    {
      if (ADDR_IS_VOID (capability_cappage)
	  || capability_cappage_count == CAPPAGE_SLOTS)
	{
	  capability_cappage = ADDR (next_free_page,
				     ADDR_BITS - PAGESIZE_LOG2);
	  next_free_page += PAGESIZE;

	  capability_cappage_count = 0;

	  struct cap cap
	    = allocate_object (cap_cappage, capability_cappage).cap;
	  struct object *object = cap_to_object (root_activity, &cap);
	  if (boot_strapped)
	    as_insert (root_activity, &thread->aspace, capability_cappage,
		       object_to_cap (object), ADDR_VOID, allocate_object);
	  else
	    capability_cappage_object = object;
	}

      return addr_extend (capability_cappage, capability_cappage_count ++,
			  CAPPAGE_SLOTS_LOG2);
    }

  /* XXX: Boostrap problem.  To allocate a folio we need to assign it
     to a principle, however, the representation of a principle
     requires storage.  Our solution is to allow a folio to be created
     without specifying a resource principal, allocating a resource
     principal and then assigning the folio to that resource
     principal.

     This isn't really a good solution as once we really go the
     persistent route, there may be references to the data structures
     in the persistent image.  Moreover, the root activity data needs
     to be saved.

     A way around this problem would be the approach that EROS takes:
     start with a hand-created system image.  */
  startup_data->activity = csalloc ();
  struct cap cap = allocate_object (cap_activity, startup_data->activity).cap;
  root_activity = (struct activity *) cap_to_object (root_activity, &cap);
  folio_reparent (root_activity, folio, root_activity);

  startup_data->thread = csalloc ();
  cap = allocate_object (cap_thread, startup_data->thread).cap;
  thread = (struct thread *) cap_to_object (root_activity, &cap);
  thread->activity = object_to_cap (root_activity);
  thread_init (thread);

  /* Insert the objects we've allocated so far into TASK's address
     space.  */
  boot_strapped = true;

  as_insert (root_activity, &thread->aspace, capability_cappage,
	     object_to_cap (capability_cappage_object), ADDR_VOID,
	     allocate_object);
  as_insert (root_activity, &thread->aspace, folio_addr,
	     object_to_cap ((struct object *) folio), ADDR_VOID,
	     allocate_object);

  /* We insert the thread and activity under two difference names: one
     reference for the hurd object descriptor and one for
     STARTUP_DATA->NAME. */
  as_insert (root_activity, &thread->aspace, startup_data->activity,
	     object_to_cap ((struct object *) root_activity), ADDR_VOID,
	     allocate_object);
  startup_data->activity = csalloc ();
  as_insert (root_activity, &thread->aspace, startup_data->activity,
	     object_to_cap ((struct object *) root_activity), ADDR_VOID,
	     allocate_object);

  as_insert (root_activity, &thread->aspace, startup_data->thread,
	     object_to_cap ((struct object *) thread), ADDR_VOID,
	     allocate_object);
  startup_data->thread = csalloc ();
  as_insert (root_activity, &thread->aspace, startup_data->thread,
	     object_to_cap ((struct object *) thread), ADDR_VOID,
	     allocate_object);

  /* Allocate the startup data object and copy the data from the
     temporary page, updating any necessary pointers.  */
#define STARTUP_DATA_ADDR 0x1000
  addr_t startup_data_addr = ADDR (STARTUP_DATA_ADDR,
				   ADDR_BITS - PAGESIZE_LOG2);
  cap = allocate_object (cap_page, startup_data_addr).cap;
  struct object *startup_data_page = cap_to_object (root_activity, &cap);
  as_insert (root_activity, &thread->aspace, startup_data_addr,
	     object_to_cap (startup_data_page), ADDR_VOID, allocate_object);
  memcpy (startup_data_page, startup_data, PAGESIZE);
  /* Free the staging area.  */
  memory_frame_free ((l4_word_t) startup_data);
  startup_data = (void *) startup_data_page;
  descs = (void *) &startup_data[1];  

  startup_data = (struct hurd_startup_data *) startup_data_page;
  startup_data->version_major = HURD_STARTUP_VERSION_MAJOR;
  startup_data->version_minor = HURD_STARTUP_VERSION_MINOR;
  startup_data->utcb_area = UTCB_AREA_BASE;
  startup_data->rm = l4_myself ();
  startup_data->descs
    = (void *) STARTUP_DATA_ADDR + (sizeof (struct hurd_startup_data));

  thread->sp = STARTUP_DATA_ADDR;

  /* Load the binary.  */
  loader_elf_load (allocate_object, root_activity, thread,
		   "system", boot_modules[0].start, boot_modules[0].end,
		   &thread->ip);


  /* Add the argument vector.  If it would overflow the page, we
     truncate it.  */
  startup_data->argz_len = strlen (boot_modules[0].command_line) + 1;

  int offset = sizeof (struct hurd_startup_data)
    + desc_count * sizeof (struct hurd_object_desc);
  int space = PAGESIZE - offset;
  if (space < startup_data->argz_len)
    {
      printf ("Truncating command line from %d to %d characters\n",
	      startup_data->argz_len, space);
      startup_data->argz_len = space;
    }
  memcpy ((void *) startup_data + offset, boot_modules[0].command_line,
	  startup_data->argz_len - 1);
  startup_data->argz = (void *) STARTUP_DATA_ADDR + offset;

  startup_data->desc_count = desc_count;

  /* Release the memory used by the binary.  */
  memory_reservation_clear (memory_reservation_system_executable);

  if (2 < output_debug)
    /* Dump the system task's address space before we start it
       running.  */
    {
      printf ("System task's AS\n");
      as_dump_from (root_activity, &thread->aspace, "");
    }

  thread_commission (thread);
  debug (1, "System task started (tid: %x.%x; ip=0x%x).",
	 l4_thread_no (thread->tid), l4_version (thread->tid), thread->ip);
}

int
main (int argc, char *argv[])
{
  parse_args (argc, argv);

  debug (1, "%s " PACKAGE_VERSION " (%x)", program_name, l4_my_global_id ());

  /* Assert that the size of a cap is a power of 2.  */
  assert ((sizeof (struct cap) & (sizeof (struct cap) - 1)) == 0);

  /* Reserve the rm binary.  */
  if (! memory_reserve ((l4_word_t) &_start, (l4_word_t) &_end,
			memory_reservation_self))
    panic ("Failed to reserve memory for self.");

  /* Find the modules.  */
  find_components ();

  memory_configure ();

  object_init ();

  /* Load the system task.  */
  system_task_load ();

  /* And, start serving requests.  */
  server_loop ();

  /* Should never return.  */
  panic ("server_loop returned!");
  return 0;
}
