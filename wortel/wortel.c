/* Main function for root server.
   Copyright (C) 2003 Free Software Foundation, Inc.
   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU Hurd is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <alloca.h>

#include "wortel.h"


/* The program name.  */
char *program_name = "wortel";

rootserver_t physmem;


/* Return the number of memory descriptors.  */
l4_word_t
loader_get_num_memory_desc (void)
{
  return l4_num_memory_desc ();
}


/* Return the NRth memory descriptor.  The first memory descriptor is
   indexed by 0.  */
l4_memory_desc_t
loader_get_memory_desc (l4_word_t nr)
{
  return l4_memory_desc (nr);
}


static void
load_components (void)
{
  if (!physmem.low)
    panic ("No physical memory server found");
  loader_add_region ("physmem-mod", physmem.low, physmem.high);

  loader_elf_load ("physmem-server", physmem.low, physmem.high,
		   &physmem.low, &physmem.high, &physmem.ip);
  loader_remove_region ("physmem-mod");
}


static void
start_components (void)
{
  l4_word_t min_page_size = getpagesize ();
  l4_word_t ret;
  l4_word_t control;
  l4_msg_t msg;
  l4_msg_tag_t msg_tag;
  
  if (physmem.low & (min_page_size - 1))
    panic ("physmem is not page aligned on this architecture");
  if (physmem.low > physmem.high)
    panic ("physmem has invalid memory range");
  if (physmem.ip < physmem.low || physmem.ip > physmem.high)
    panic ("physmem has invalid IP");

  /* Thread nr is next available after rootserver thread nr,
     version part is 2 (rootserver is 1).  */
  l4_thread_id_t physmem_server
    = l4_global_id (l4_thread_no (l4_myself ()) + 2,
		    2);
  /* The UTCB location below is only a hack.  We also need a way to
     specify the maximum number of threads (ie the size of the UTCB
     area), for example via ELF symbols, or via the command line.
     This can also be used to actually create these threads up
     front.  */
  ret = l4_thread_control (physmem_server, physmem_server, l4_myself (),
			   l4_nilthread, (void *) 0x2000000);
  if (!ret)
    panic ("Creation of initial physmem thread failed");

  /* The UTCB area must be controllable in some way, see above.  Same
     for KIP area.  */
  ret = l4_space_control (physmem_server, 0,
			  l4_fpage_log2 (0x2400000, l4_kip_area_size_log2 ()),
			  l4_fpage_log2 (0x2000000, 14),
			  l4_anythread, &control);
  if (!ret)
    panic ("Creation of physmem address space failed");

  ret = l4_thread_control (physmem_server, physmem_server, l4_nilthread,
			   l4_myself (), (void *) -1);
  if (!ret)
    panic ("Activation of initial physmem thread failed");

  l4_msg_clear (&msg);
  l4_set_msg_label (&msg, 0);
  l4_msg_append_word (&msg, physmem.ip);
  l4_msg_append_word (&msg, 0);
  l4_msg_load (&msg);
  msg_tag = l4_send (physmem_server);
  if (l4_ipc_failed (msg_tag))
    panic ("Sending startup message to physmem thread failed: %u",
	   l4_error_code ());

  {
    l4_fpage_t *fpages;
    unsigned int nr_fpages = 0;
    l4_word_t start = physmem.low;
    l4_word_t end = (physmem.high + min_page_size) & ~(min_page_size - 1);
    l4_word_t region;

    /* We want to grant all the memory for the physmem binary image
       with the first page fault, but we might have to send several
       fpages.  So we first create a list of all fpages we need, then
       we serve one after another, providing the one containing the
       fault address last.  */

    /* A page-aligned region of size up to 2^k * min_page_size can be
       covered by k fpages at most (proof by induction).  At this
       point, END is at least one MIN_PAGE_SIZE larger than START.  */
    region = (end - start) / min_page_size;
    while (region > 0)
      {
	nr_fpages++;
	region >>= 1;
      }
    fpages = alloca (sizeof (l4_fpage_t) * nr_fpages);

    nr_fpages = 0;
    while (start < end)
      {
	fpages[nr_fpages] = l4_fpage (start, end - start);
	start += l4_size (fpages[nr_fpages]);
	nr_fpages++;
      }

    /* Now serve page requests.  */
    while (nr_fpages)
      {
	l4_grant_item_t grant_item;
	l4_fpage_t fpage;
	l4_word_t addr;
	unsigned int i;

	msg_tag = l4_receive (physmem_server);
	if (l4_ipc_failed (msg_tag))
	  panic ("Receiving messages from physmem thread failed: %u",
		 (l4_error_code () >> 1) & 0x7);
	if ((l4_label (msg_tag) >> 4) != 0xffe)
	  panic ("Message from physmem thread is not a page fault");
	l4_msg_store (msg_tag, &msg);
	if (l4_untyped_words (msg_tag) != 2 || l4_typed_words (msg_tag) != 0)
	  panic ("Invalid format of page fault message");
	addr = l4_msg_word (&msg, 0);
	if (addr != physmem.ip)
	  panic ("Page fault at unexpected address 0x%x (expected 0x%x)",
		 addr, physmem.ip);

	if (nr_fpages == 1)
	  i = 0;
	else
	  for (i = 0; i < nr_fpages; i++)
	    if (addr < l4_address (fpages[i])
		|| addr >= l4_address (fpages[i]) + l4_size (fpages[i]))
	      break;
	if (i == nr_fpages)
	  panic ("Could not find suitable fpage");

	fpage = l4_fpage_add_rights (fpages[i], l4_fully_accessible);
	debug ("Granting Fpage: 0x%x - 0x%x\n", l4_address (fpage),
	       l4_address (fpage) + l4_size (fpage));

	if (i != 0)
	  fpages[i] = fpages[nr_fpages - 1];
	nr_fpages--;

	/* First we have to request the fpage from sigma0.  */
	l4_accept (l4_map_grant_items (l4_complete_address_space));
	l4_msg_clear (&msg);
	l4_set_msg_label (&msg, 0xffa0);
	l4_msg_append_word (&msg, fpage.raw);
	l4_msg_append_word (&msg, L4_DEFAULT_MEMORY);
	l4_msg_load (&msg);
	msg_tag = l4_call (l4_global_id (l4_thread_user_base (), 1));
	if (l4_ipc_failed (msg_tag))
	  panic ("sigma0 request failed during %s: %u",
		 l4_error_code () & 1 ? "receive" : "send",
		 (l4_error_code () >> 1) & 0x7);
	if (l4_untyped_words (msg_tag) != 0
	    || l4_typed_words (msg_tag) != 2)
	  panic ("Invalid format of sigma0 reply");
	l4_msg_store (msg_tag, &msg);
	if (l4_msg_word (&msg, 1) == l4_nilpage.raw)
	  panic ("sigma0 rejected mapping");

	/* Now we can grant the mapping to the physmem server.
	   FIXME: Should use the fpage returned by sigma0.  */
	l4_msg_clear (&msg);
	l4_set_msg_label (&msg, 0);
	/* FIXME: Keep track of mappings already provided.  Possibly
	   map text section rx and data rw.  */
	grant_item = l4_grant_item (fpage, l4_address (fpage));
	l4_msg_append_grant_item (&msg, grant_item);
	l4_msg_load (&msg);
	l4_reply (physmem_server);
      }
  }

  do
    {
      l4_word_t label;

      msg_tag = l4_receive (physmem_server);
      if (l4_ipc_failed (msg_tag))
	panic ("Receiving messages from physmemserver thread failed: %u",
	       (l4_error_code () >> 1) & 0x7);

      label = l4_label (msg_tag);
      l4_msg_store (msg_tag, &msg);

#define WORTEL_MSG_PUTCHAR 1
      if (label == WORTEL_MSG_PUTCHAR)
	{
	  int chr;

	  /* This is a putchar() message.  */
	  if (l4_untyped_words (msg_tag) != 1
	      || l4_typed_words (msg_tag) != 0)
	    panic ("Invalid format of putchar msg");

	  chr = (int) l4_msg_word (&msg, 0);
	  putchar (chr);
	  /* No reply needed.  */
	  continue;
	}
      else
	panic ("Invalid message with tag 0x%x", msg_tag);
    }
  while (1);
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
		  "Boot the Hurd system and wrap the L4 privileged system "
		  "calls.\n"
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



#define STACK_SIZE 4096
char exception_handler_stack[STACK_SIZE];

void
exception_handler (void)
{
  int count = 0;

  while (count < 10)
    {
      l4_msg_tag_t tag;
      l4_thread_id_t from;
      l4_word_t mr[12];

      tag = l4_wait (&from);
      debug ("EXCEPTION HANDLER: Received message from: ");
      debug ("0x%x", from);
      debug ("\n");
      debug ("Tag: 0x%x", tag.raw);
      debug ("Label: %u  Untyped: %u  Typed: %u\n",
	      l4_label (tag), l4_untyped_words (tag), l4_typed_words (tag));

      l4_store_mrs (1, 12, mr);
      debug ("Succeeded: %u  Propagated: %u  Redirected: %u  Xcpu: %u\n",
	      l4_ipc_succeeded (tag), l4_ipc_propagated (tag),
	      l4_ipc_redirected (tag), l4_ipc_xcpu (tag));
      debug ("Error Code: 0x%x\n", l4_error_code ());
      debug ("EIP: 0x%x  EFLAGS: 0x%x  Exception: %u  ErrCode: %u\n",
	      mr[0], mr[1], mr[2], mr[3]);
      debug ("EDI: 0x%x  ESI: 0x%x  EBP: 0x%x  ESP: 0x%x\n",
	      mr[4], mr[5], mr[6], mr[7]);
      debug ("EAX: 0x%x  EBX: 0x%x  ECX: 0x%x  EDX: 0x%x\n",
	      mr[11], mr[8], mr[10], mr[9]);
    }
  
  debug ("EXCEPTION HANDLER: Too many exceptions.\n");

  while (1)
    l4_yield ();
}


static void
setup (void)
{
  l4_thread_id_t thread;
  l4_word_t result;
  void *utcb;

  /* FIXME: This is not specified by the standard.  We don't know
     where and how much space we have for other thread's UTCB
     areas in the root server.  */
  utcb = (void *) ((l4_my_local_id ().raw & ~(l4_utcb_size () - 1))
		   + l4_utcb_size ());
  thread = l4_global_id (l4_thread_no (l4_myself ()) + 1, 1);
  debug ("Creating exception handler thread at utcb 0x%x: ",
	  (l4_word_t) utcb);
  result = l4_thread_control (thread, l4_myself (), l4_myself (),
			      l4_global_id (l4_thread_no (l4_myself()) - 2, 1),
			      utcb);
  debug ("%s\n", result ? "successful" : "failed");
  /* Set the priority of the other thread to our priority.  Otherwise
     it is 100 and will never be scheduled as long as we are not in a
     blocking receive.  */
  l4_set_priority (thread, 255);
  l4_start_sp_ip (thread, ((l4_word_t) exception_handler_stack)
		  + sizeof (exception_handler_stack),
		  (l4_word_t) exception_handler);
  l4_set_exception_handler (thread);
}


int
main (int argc, char *argv[])
{
  parse_args (argc, argv);

  debug ("%s " PACKAGE_VERSION "\n", program_name);

  setup ();

  find_components ();

  load_components ();

  start_components ();

  while (1)
    l4_yield ();

  return 0;
}
