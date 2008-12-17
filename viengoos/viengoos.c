/* viengoos.c - Main file for viengoos.
   Copyright (C) 2007, 2008 Free Software Foundation, Inc.
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

#include "mutex.h"

#ifndef NDEBUG
struct ss_lock_trace ss_lock_trace[SS_LOCK_TRACE_COUNT];
int ss_lock_trace_count;
#endif

#include <assert.h>
#include <unistd.h>
#include <alloca.h>
#include <string.h>

#include <l4/thread-start.h>
#include <l4/pagefault.h>

#include <hurd/startup.h>
#include <hurd/stddef.h>
#include <viengoos/thread.h>
#include <hurd/as.h>

#include <process-spawn.h>

#include "viengoos.h"
#include "sigma0.h"
#include "memory.h"
#include "boot-modules.h"
#include "cap.h"
#include "object.h"
#include "activity.h"
#include "thread.h"
#include "server.h"
#include "shutdown.h"
#include "output.h"
#include "zalloc.h"
#include "ager.h"


#define BUG_ADDRESS	"<bug-hurd@gnu.org>"

/* The program name.  */
char *program_name = "viengoos";

/* The following must be defined and are used to calculate the extents
   of the laden binary itself.  _END is one more than the address of
   the last byte.  */
extern char _start;
extern char _end;

static struct output_driver output_device;

l4_thread_id_t viengoos_tid;
l4_thread_id_t ager_tid;

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
		  "  -D, --debug LEVEL enable debug output (1-5)"
#ifdef DEBUG_ELIDE
		  " (disabled)"
#endif
		  "\n"
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
	  if (!output_init (&output_device, argv[i], true))
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
#ifndef DEBUG_ELIDE
	  output_debug = strtol (argv[i], (char **) NULL, 10);
#endif
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
  do_debug (3)
    memory_reserve_dump ();
  memory_grab ();

  printf ("memory: %x-%x\n", first_frame, last_frame);

  /* We need to ensure that the whole binary is faulted in.  sigma0 is
     only willing to page the first thread.  Since memory_configure
     only grabs otherwise unreserved memory and the binary is
     reserved, we either have to implement a pager for additional
     threads (e.g., the ager) or we just fault the binary in now.  The
     latter is the easiest solution.  */
  l4_word_t p;
  for (p = (l4_word_t) &_start; p < (l4_word_t) &_end; p += PAGESIZE)
    * (volatile l4_word_t *) p = *(l4_word_t *)p;

  /* After this point, we should never fault.  */
  l4_set_pager (l4_nilthread);
}

struct thread *
system_task_load (void)
{
  const char *const argv[] = { boot_modules[0].command_line, NULL };
  struct thread *thread;
  thread = process_spawn (VG_ADDR_VOID,
			  (void *) boot_modules[0].start,
			  (void *) boot_modules[0].end,
			  argv, NULL,
			  true);

  /* Free the associated memory.  */
  memory_reservation_clear (memory_reservation_system_executable);

  return thread;
}

void
ager_start (void)
{
  const int stack_size = PAGESIZE * 32;
  void *stack = (void *) zalloc (stack_size);
  /* XXX: We assume the stack grows down.  */
  void *sp = stack + stack_size;

  /* Push the return address onto the stack.  */
  /* XXX: We assume the stack grows down and we assume that the stack
     has the normal x86 layout.  */
  sp -= sizeof (l4_word_t);
  * (l4_word_t *) sp = 0;

  l4_thread_id_t tid = l4_global_id (l4_thread_no (l4_myself ()) + 1,
				     l4_version (l4_myself ()));

  int ret = l4_thread_control (tid, l4_myself (),
			       l4_myself (),
			       l4_myself (),
			       (void *) _L4_utcb_base () + l4_utcb_size ());
  if (! ret)
    panic ("Could not create ager thread (id=%x.%x): %s",
	   l4_thread_no (tid), l4_version (tid),
	   l4_strerror (l4_error_code ()));

  
  debug (1, "Created ager: %x", tid);

  ager_tid = tid;

  l4_thread_id_t targ = tid;
  l4_word_t control = _L4_XCHG_REGS_CANCEL_IPC
    | _L4_XCHG_REGS_SET_SP | _L4_XCHG_REGS_SET_IP;
  l4_word_t dummy = 0;
  l4_word_t sp_arg = (l4_word_t) sp;
  l4_word_t ip = (l4_word_t) ager_loop;
  _L4_exchange_registers (&targ, &control, &sp_arg, &ip,
			  &dummy, &dummy, &dummy);
  if (targ == l4_nilthread)
    panic ("Failed to start ager thread (id=%x.%x): %s",
	   l4_thread_no (tid), l4_version (tid),
	   l4_strerror (l4_error_code ()));
}

static void bootstrap (void) __attribute__ ((noinline));

static void
bootstrap (void)
{
  viengoos_tid = l4_myself ();

  /* Reserve the rm binary.  */
  if (! memory_reserve ((l4_word_t) &_start, (l4_word_t) &_end,
			memory_reservation_self))
    panic ("Failed to reserve memory for self.");

  /* Find the modules.  */
  find_components ();

  memory_configure ();

  object_init ();

  ager_start ();

  /* Load the system task.  */
  struct thread *thread = system_task_load ();

#if 0
  /* Discard every second page to try and catch out of bounds errors.
     After this point, there will be no unallocated frames that are
     consecutive in memory.  */
  int discarded = 0;

  void discard (void *f)
  {
    l4_flush (l4_fpage ((l4_word_t) f, PAGESIZE));
    discarded ++;
  }


  uintptr_t size;
  for (size = 1 << (sizeof (uintptr_t) * 8 - 1); size > PAGESIZE; size >>= 1)
    {
      void *chunk;
      while ((chunk = zalloc (size)))
	{
	  void *f = chunk;
	  void *end = chunk + size;

	  /* Discard the first page.  */
	  discard (f);
	  f += PAGESIZE;

	  while (end - f >= 2 * PAGESIZE)
	    {
	      /* Release the second.  */
	      zfree (f, PAGESIZE);
	      f += PAGESIZE;

	      discard (f);
	      f += PAGESIZE;
	    }

	  if (f < end)
	    {
	      assert (f + PAGESIZE == end);
	      discard (f);
	    }
	}
    }

  debug (0, DEBUG_BOLD ("Discarded %d pages"), discarded);
#endif

  /* MEMORY_TOTAL is the total number of frames in the system.  We
     need to know the number of frames that are available to user
     tasks.  This is the sum of the free memory plus that which is
     already allocated to activities.  So far, there is only the root
     activity.  */
  memory_total = zalloc_memory + root_activity->frames_total;
}

int
main (int argc, char *argv[])
{
  parse_args (argc, argv);

  s_printf ("If the following fails, you failed to patch L4 or libl4.  "
	    "Reread the README.\n");
  debug (1, "%s " PACKAGE_VERSION " (%x)", program_name, l4_my_global_id ());

  /* Bootstrap the system.  */
  bootstrap ();

  /* And, start serving requests.  */
  server_loop ();
}
