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

#include <unistd.h>
#include <alloca.h>

#include "wortel.h"
#include "sigma0.h"


/* The program name.  */
const char program_name[] = "wortel";

/* The region of wortel itself.  */
l4_word_t wortel_start;
l4_word_t wortel_end;


/* Unused memory.  These fpages mark memory which we needed at some
   time, but don't need anymore.  It can be granted to the physical
   memory server at startup.  This includes architecture dependent
   boot data as well as the physical memory server module.  */
l4_fpage_t wortel_unused_fpages[MAX_UNUSED_FPAGES];
unsigned int wortel_unused_fpages_count;


/* Room for the arguments.  1 KB is a cramped half-screen full, which
   should be more than enough.  */
char mods_args[1024];

/* The number of bytes in mods_args already consumed.  */
unsigned mods_args_len;

const char *mod_names[] = { "physmem-mod", "task-mod", "root-fs-mod" };

/* For the boot components, find_components() must fill in the start
   and end address of the ELF images in memory.  The end address is
   one more than the last byte in the image.  */
struct wortel_module mods[MOD_NUMBER];

unsigned int mods_count;

/* The physical memory server master control capability for the root
   filesystem.  */
hurd_cap_scid_t physmem_master;


/* The maximum number of tasks allowed to use the rootserver.  */
#define MAX_USERS 16

/* FIXME: Needs to be somewhere else.  */
typedef l4_word_t hurd_task_id_t;
#define HURD_TASK_ID_NULL 0

/* The allowed user tasks.  */
static hurd_task_id_t cap_list[MAX_USERS];

/* Register TASK as allowed user and return the capability ID, or -1
   if there is no space.  */
static int
wortel_add_user (hurd_task_id_t task)
{
  unsigned int i;

  for (i = 0; i < MAX_USERS; i++)
    if (cap_list[i] == HURD_TASK_ID_NULL)
      break;

  if (i == MAX_USERS)
    return -1;

  cap_list[i] = task;
  return i;
}

#define WORTEL_CAP_VALID(capnr, task) \
  (capnr >= 0 && capnr < MAX_USERS && cap_list[capnr] == task)


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


/* The maximum number of fpages required to cover a page aligned range
   of memory.  This is k if the maximum memory range size to cover is
   2^(k + min_page_size_log2), which can be easily proved by
   induction.  The minimum page size in L4 is at least
   L4_MIN_PAGE_SIZE.  We also need to have each fpage aligned to a
   multiple of its own size.  This makes the proof by induction a bit
   more convoluted, but does not change the result.  */
#define MAX_FPAGES (sizeof (l4_word_t) * 8 - L4_MIN_PAGE_SIZE_LOG2)


/* Determine the fpages required to cover the bytes from START to END
   (exclusive).  START must be aligned to the minimal page size
   supported by the system.  Returns the number of fpages required to
   cover the range, and returns that many fpages (with maximum
   accessibility) in FPAGES.  At most MAX_FPAGES fpages will be
   returned.  Each fpage will also be aligned to a multiple of its own
   size.  */
static unsigned int
make_fpages (l4_word_t start, l4_word_t end, l4_fpage_t *fpages)
{
  l4_word_t min_page_size = l4_min_page_size ();
  unsigned int nr_fpages = 0;

  if (start >= end)
    return 0;

  if (start & (min_page_size - 1))
    panic ("make_fpages: START is not aligned to minimum page size");

  end = (end + min_page_size - 1) & ~(min_page_size - 1);
  /* END is now at least one MIN_PAGE_SIZE larger than START.  */
  nr_fpages = 0;
  while (start < end)
    {
      unsigned int addr_align;
      unsigned int size_align;

      /* Each fpage must be self-aligned.  */
      addr_align = l4_lsb (start) - 1;
      size_align = l4_msb (end - start) - 1;
      if (addr_align < size_align)
	size_align = addr_align;

      fpages[nr_fpages]
	= l4_fpage_add_rights (l4_fpage_log2 (start, size_align),
			       l4_fully_accessible);
      start += l4_size (fpages[nr_fpages]);
      nr_fpages++;
    }
  return nr_fpages;
}



static void
load_components (void)
{
  l4_fpage_t fpages[MAX_FPAGES];
  unsigned int nr_fpages;
  l4_word_t min_page_size = l4_min_page_size ();
  unsigned int i;
  l4_word_t addr;
  l4_word_t end_addr;

  /* One issue we have to solve is to make sure that when the physical
     memory server requests all the available physical fpages, we know
     which fpages we can give to it, and which we can't.  This can be
     left to sigma0, as long as we request all fpages from sigma0 we
     can't give to the physical memory server up-front.

     We do this in several steps: First, we copy all of the startup
     information to memory that is part of the wortel binary image
     itself.  This is done by the architecture dependent
     find_components function.  Then we request all of our own memory,
     all the memory for our modules, and finally all the memory needed
     for the physical memory server image (at its destination
     address).  */

  if (wortel_start & (min_page_size - 1))
    panic ("%s does not start on a page boundary", program_name);
  loader_add_region (program_name, wortel_start, wortel_end);

  /* We must request our own memory using the smallest fpages
     possible, because we already have some memory mapped due to page
     faults, and fpages are specified as inseparable objects.  */
  for (addr = wortel_start; addr < wortel_end; addr += min_page_size)
    sigma0_get_fpage (l4_fpage (addr, min_page_size));

  /* First protect all pages covered by the modules.  This will also
     show if each module starts (and ends) on its own page.  Request
     all memory for all modules.  Although we can release the memory
     for the physmem module later on, we have to touch it anyway to
     load it to its destination address, and requesting the fpages now
     allows us to choose how we want to split up the module in
     (inseparable) fpages.  */
  for (i = 0; i < mods_count; i++)
    {
      if (mods[i].start & (min_page_size - 1))
	panic ("Module %s does not start on a page boundary", mods[i].name);
      loader_add_region (mods[i].name, mods[i].start, mods[i].end);

      nr_fpages = make_fpages (mods[i].start, mods[i].end, fpages);
      if (i == MOD_PHYSMEM)
	{
	  /* The physical memory server module memory can be recycled
	     later.  */
	  if (nr_fpages + wortel_unused_fpages_count > MAX_UNUSED_FPAGES)
	    panic ("not enough room in unused fpages list for physmem-mod");
	  memcpy (&wortel_unused_fpages[wortel_unused_fpages_count],
		  fpages, sizeof (l4_fpage_t) * nr_fpages);
	  wortel_unused_fpages_count += nr_fpages;
	}

      while (nr_fpages--)
	sigma0_get_fpage (fpages[nr_fpages]);
    }

  /* Because loading the physical memory server to its destination
     address will touch the destination memory, which we later want to
     grant to the physical memory server using (inseparable) fpages,
     we request the desired fpages up-front.  */
  loader_elf_dest ("physmem-server", mods[MOD_PHYSMEM].start,
		   mods[MOD_PHYSMEM].end, &addr, &end_addr);
  nr_fpages = make_fpages (addr, end_addr, fpages);
  while (nr_fpages--)
    sigma0_get_fpage (fpages[nr_fpages]);

  /* Now load the physical memory server to its destination
     address.  */
  addr = mods[MOD_PHYSMEM].start;
  end_addr = mods[MOD_PHYSMEM].end;
  if (!addr)
    panic ("No physical memory server found");
  loader_elf_load ("physmem-server", addr, end_addr,
		   &mods[MOD_PHYSMEM].start, &mods[MOD_PHYSMEM].end,
		   &mods[MOD_PHYSMEM].ip);
  loader_remove_region ("physmem-mod");
}


static void
start_components (void)
{
  l4_msg_t msg;
  l4_msg_tag_t tag;
  l4_word_t ret;
  l4_word_t control;
  unsigned int cap_id;
  unsigned int i;
  l4_word_t thread_no = l4_thread_no (l4_myself ()) + 1;
  hurd_task_id_t task_id = 2;
  l4_thread_id_t server;

  for (i = 0; i < mods_count; i++)
    {
      /* FIXME: Should only be done for modules turned into tasks.  */
      mods[i].main_thread = l4_global_id (thread_no++, task_id);
      mods[i].server_thread = l4_global_id (thread_no++, task_id);
      task_id++;
    }
  
  if (mods[MOD_PHYSMEM].start > mods[MOD_PHYSMEM].end)
    panic ("physmem has invalid memory range");
  if (mods[MOD_PHYSMEM].ip < mods[MOD_PHYSMEM].start
      || mods[MOD_PHYSMEM].ip > mods[MOD_PHYSMEM].end)
    panic ("physmem has invalid IP");

  server = mods[MOD_PHYSMEM].main_thread;
  /* FIXME: Pass cap_id to physmem.  */
  cap_id = wortel_add_user (l4_version (server));
  /* The UTCB location below is only a hack.  We also need a way to
     specify the maximum number of threads (ie the size of the UTCB
     area), for example via ELF symbols, or via the command line.
     This can also be used to actually create these threads up
     front.  */
  ret = l4_thread_control (server, server, l4_myself (), l4_nilthread,
			   (void *) -1);
  if (!ret)
    panic ("Creation of initial physmem thread failed");

  /* The UTCB area must be controllable in some way, see above.  Same
     for KIP area.  */
  ret = l4_space_control (server, 0,
			  l4_fpage_log2 (wortel_start,
					 l4_kip_area_size_log2 ()),
			  l4_fpage_log2 (wortel_start + l4_kip_area_size (),
					 l4_utcb_area_size_log2 ()),
			  l4_anythread, &control);
  if (!ret)
    panic ("Creation of physmem address space failed");

  ret = l4_thread_control (server, server, l4_nilthread, l4_myself (),
			   (void *) (wortel_start + l4_kip_area_size ()));
  if (!ret) 
    panic ("Activation of initial physmem thread failed");

  l4_msg_clear (&msg);
  l4_set_msg_label (&msg, 0);
  l4_msg_append_word (&msg, mods[MOD_PHYSMEM].ip);
  l4_msg_append_word (&msg, 0);
  l4_msg_load (&msg);
  tag = l4_send (server);
  if (l4_ipc_failed (tag))
    panic ("Sending startup message to physmem thread failed: %u",
	   l4_error_code ());

  {
    l4_fpage_t fpages[MAX_FPAGES];
    unsigned int nr_fpages;

    /* We want to grant all the memory for the physmem binary image
       with the first page fault, but we might have to send several
       fpages.  So we first create a list of all fpages we need, then
       we serve one after another, providing the one containing the
       fault address last.  */
    nr_fpages = make_fpages (mods[MOD_PHYSMEM].start, mods[MOD_PHYSMEM].end,
			     fpages);

    /* Now serve page requests.  */
    while (nr_fpages)
      {
	l4_grant_item_t grant_item;
	l4_fpage_t fpage;
	l4_word_t addr;
	unsigned int i;

	tag = l4_receive (server);
	if (l4_ipc_failed (tag))
	  panic ("Receiving messages from physmem thread failed: %u",
		 (l4_error_code () >> 1) & 0x7);
	if ((l4_label (tag) >> 4) != 0xffe)
	  panic ("Message from physmem thread is not a page fault");
	l4_msg_store (tag, &msg);
	if (l4_untyped_words (tag) != 2 || l4_typed_words (tag) != 0)
	  panic ("Invalid format of page fault message");
	addr = l4_msg_word (&msg, 0);
	if (addr != mods[MOD_PHYSMEM].ip)
	  panic ("Page fault at unexpected address 0x%x (expected 0x%x)",
		 addr, mods[MOD_PHYSMEM].ip);

	if (nr_fpages == 1)
	  i = 0;
	else
	  for (i = 0; i < nr_fpages; i++)
	    if (addr < l4_address (fpages[i])
		|| addr >= l4_address (fpages[i]) + l4_size (fpages[i]))
	      break;
	if (i == nr_fpages)
	  panic ("Could not find suitable fpage");

	fpage = fpages[i];
	if (i != 0)
	  fpages[i] = fpages[nr_fpages - 1];
	nr_fpages--;

	/* The memory was already requested from sigma0 by
	   load_components, so grant it right away.  */
	debug ("Granting fpage: 0x%x/%u\n", l4_address (fpage),
	       l4_size_log2 (fpage));
	l4_msg_clear (&msg);
	l4_set_msg_label (&msg, 0);
	/* FIXME: Keep track of mappings already provided.  Possibly
	   map text section rx and data rw.  */
	grant_item = l4_grant_item (fpage, l4_address (fpage));
	l4_msg_append_grant_item (&msg, grant_item);
	l4_msg_load (&msg);
	l4_reply (server);
      }
  }

  /* Now we have kicked off the boot process.  The rest will be done
     in the normal server loop.  */
}


/* Serve rootserver bootstrap requests.  We do everything within this
   loop, because this allows the other servers to print messages and
   panic while we are performing the bootstrap.  If we were to make
   calls to the other servers, we would have to poll for the reply in
   a special server loop anyway to allow debug output and other
   interleaved communication.  */
static void
serve_bootstrap_requests (void)
{
  /* The size of the region that we are currently trying to allocate
     for GET_MEM requests.  If this is smaller than L4_MIN_PAGE_SIZE,
     no more memory is available.  */
  unsigned int get_mem_size = sizeof (l4_word_t) * 8 - 1;

  /* The current module for which we want to create the memory
     container.  We skip the physical memory container itself.
     SERVER_TASK must be the task ID of the owner of that module
     data.  */
  unsigned int mod_idx = 1;
  hurd_task_id_t server_task = (mod_idx < mods_count)
    ? l4_version (mods[mod_idx].main_thread) : 0;

  /* Allocate a single page at address 0, because we don't want to
     bother anybody with that silly page.  */
  sigma0_get_fpage (l4_fpage (0, l4_min_page_size ()));

  do
    {
      l4_thread_id_t from;
      l4_word_t label;
      l4_msg_t msg;
      l4_msg_tag_t tag;

      tag = l4_wait (&from);
      if (l4_ipc_failed (tag))
	panic ("Receiving message failed: %u", (l4_error_code () >> 1) & 0x7);

      label = l4_label (tag);
      /* FIXME: Shouldn't store the whole msg before checking access
	 rights.  */
      l4_msg_store (tag, &msg);
      if (!WORTEL_CAP_VALID (l4_msg_word (&msg, 0), l4_version (from)))
	/* FIXME: Shouldn't be a panic of course.  */
	panic ("Unprivileged user attemps to access wortel rootserver");

#define WORTEL_MSG_PUTCHAR		1
#define WORTEL_MSG_SHUTDOWN		2
#define WORTEL_MSG_GET_MEM		3
#define WORTEL_MSG_GET_CAP_REQUEST	4
#define WORTEL_MSG_GET_CAP_REPLY	5
      if (label == WORTEL_MSG_PUTCHAR)
	{
	  int chr;

	  /* This is a putchar() message.  */
	  if (l4_untyped_words (tag) != 2
	      || l4_typed_words (tag) != 0)
	    panic ("Invalid format of putchar msg");

	  chr = (int) l4_msg_word (&msg, 1);
	  putchar (chr);
	  /* No reply needed.  */
	  continue;
	}
      else if (label == WORTEL_MSG_SHUTDOWN)
	panic ("Bootstrap failed");
      else if (label == WORTEL_MSG_GET_MEM)
	{
	  l4_fpage_t fpage;
	  l4_grant_item_t grant_item;

	  if (l4_untyped_words (tag) != 1
	      || l4_typed_words (tag) != 0)
	    panic ("Invalid format of get_mem msg");

	  if (get_mem_size < L4_MIN_PAGE_SIZE_LOG2)
	    panic ("physmem server does not stop requesting memory");

	  /* Give out the memory.  First our unused fpages, then the
	     fpages we can get from sigma0.  */
	  if (wortel_unused_fpages_count)
	    fpage = wortel_unused_fpages[--wortel_unused_fpages_count];
	  else
	    do
	      {
		fpage = sigma0_get_any (get_mem_size);
		if (fpage.raw == l4_nilpage.raw)
		  get_mem_size--;
	      }
	    while (fpage.raw == l4_nilpage.raw
		   && get_mem_size >= L4_MIN_PAGE_SIZE_LOG2);

	  /* When we get the nilpage, then this is an indication that
	     we grabbed all possible conventional memory.  We still
	     own our own memory we run on, and whatever memory the
	     output driver is using (for example VGA mapped
	     memory).  */
	  grant_item = l4_grant_item (fpage, l4_address (fpage));
	  l4_msg_clear (&msg);
	  l4_msg_append_grant_item (&msg, grant_item);
	  l4_msg_load (&msg);
	  l4_reply (from);
	}
      else if (label == WORTEL_MSG_GET_CAP_REQUEST)
	{
	  if (l4_untyped_words (tag) != 1
	      || l4_typed_words (tag) != 0)
	    panic ("Invalid format of get cap request msg");

	  if (mod_idx == mods_count)
	    {
	      /* Request the global control capability now.  */
	      l4_msg_clear (&msg);
	      l4_set_msg_label (&msg, 0);

	      l4_msg_append_word (&msg,
				  l4_version (mods[MOD_ROOT_FS].main_thread));
	      l4_msg_load (&msg);
	      l4_reply (from);
	    }
	  else if (mod_idx > mods_count)
	    panic ("physmem does not stop requesting capabilities");
	  else
	    {
	      /* We are allowed to make a capability request now.  */
	      l4_fpage_t fpages[MAX_FPAGES];
	      unsigned int nr_fpages;

	      nr_fpages = make_fpages (mods[mod_idx].start,
				       mods[mod_idx].end, fpages);

	      /* We can not pass more than 30 grant items in our
		 message, because there are only 64 message registers,
		 we need 4 for the label, the task ID, the start and
		 end address, and each grant item takes up two message
		 registers.  */
	      if (nr_fpages > 30)
		panic ("%s: Module %s is too large and has an "
		       "unfortunate alignment", __func__, mods[mod_idx].name);

	      l4_msg_clear (&msg);
	      l4_set_msg_label (&msg, 0);
	      l4_msg_append_word (&msg, server_task);
	      l4_msg_append_word (&msg, mods[mod_idx].start);
	      l4_msg_append_word (&msg, mods[mod_idx].end);
	      while (nr_fpages--)
		{
		  l4_grant_item_t grant_item;
		  l4_fpage_t fpage = fpages[nr_fpages];

		  grant_item = l4_grant_item (fpage, l4_address (fpage));
		  l4_msg_append_grant_item (&msg, grant_item);
		}
	      l4_msg_load (&msg);
	      l4_reply (from);
	    }
	}
      else if (label == WORTEL_MSG_GET_CAP_REPLY)
	{
	  if (l4_untyped_words (tag) != 2
	      || l4_typed_words (tag) != 0)
	    panic ("Invalid format of get cap reply msg");

	  if (mod_idx > mods_count)
	    panic ("Invalid get cap reply message");
	  else if (mod_idx == mods_count)
	    physmem_master = l4_msg_word (&msg, 1);
	  else
	    mods[mod_idx].mem_cont = l4_msg_word (&msg, 1);

	  /* Does not require a reply.  */
	  mod_idx++;
	  /* FIXME: Only do this if the next module in the list is
	     a new task.  */
	  if (mod_idx < mods_count)
	    server_task = l4_version (mods[mod_idx].main_thread);
	}
      else if ((label >> 4) == 0xffe)
	{
	  if (l4_untyped_words (tag) != 2 || l4_typed_words (tag) != 0)
	    panic ("Invalid format of page fault message");
	  panic ("Unexpected page fault from 0x%x at address 0x%x (IP 0x%x)",
		 from.raw, l4_msg_word (&msg, 0), l4_msg_word (&msg, 1));
	}
      else
	panic ("Invalid message with tag 0x%x", tag.raw);
    }
  while (1);
}


/* Serve rootserver requests.  */
static void
serve_requests (void)
{
  do
    {
      l4_thread_id_t from;
      l4_word_t label;
      l4_msg_t msg;
      l4_msg_tag_t msg_tag;

      msg_tag = l4_wait (&from);
      if (l4_ipc_failed (msg_tag))
	panic ("Receiving message failed: %u", (l4_error_code () >> 1) & 0x7);

      label = l4_label (msg_tag);
      /* FIXME: Shouldn't store the whole msg before checking access
	 rights.  */
      l4_msg_store (msg_tag, &msg);
      if (!WORTEL_CAP_VALID (l4_msg_word (&msg, 0), l4_version (from)))
	/* FIXME: Shouldn't be a panic of course.  */
	panic ("Unprivileged user attemps to access wortel rootserver");

#define WORTEL_MSG_PUTCHAR 1
      if (label == WORTEL_MSG_PUTCHAR)
	{
	  int chr;

	  /* This is a putchar() message.  */
	  if (l4_untyped_words (msg_tag) != 2
	      || l4_typed_words (msg_tag) != 0)
	    panic ("Invalid format of putchar msg");

	  chr = (int) l4_msg_word (&msg, 1);
	  putchar (chr);
	  /* No reply needed.  */
	  continue;
	}
      else if (label == WORTEL_MSG_SHUTDOWN)
	panic ("Bootstrap failed");
      else
	panic ("Invalid message with tag 0x%x", msg_tag.raw);
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

  if (mods_count < MOD_NUMBER)
    panic ("Some modules are missing");

  load_components ();

  start_components ();

  serve_bootstrap_requests ();

  serve_requests ();

  return 0;
}
