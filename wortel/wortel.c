/* Main function for root server.
   Copyright (C) 2003, 2004 Free Software Foundation, Inc.
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

#include <assert.h>
#include <unistd.h>
#include <alloca.h>

#include <l4/thread-start.h>
#include <l4/pagefault.h>

#include <hurd/wortel.h>
#include <hurd/startup.h>

#include "wortel-intern.h"
#include "sigma0.h"


/* The program name.  */
const char program_name[] = "wortel";

/* The region of wortel itself.  */
l4_word_t wortel_start;
l4_word_t wortel_end;


/* Unused memory.  These fpages mark memory which we needed at some
   time, but don't need anymore.  It can be mapped to the physical
   memory server at startup.  This includes architecture dependent
   boot data as well as the physical memory server module.  */
l4_fpage_t wortel_unused_fpages[MAX_UNUSED_FPAGES];
unsigned int wortel_unused_fpages_count;


/* Room for the arguments.  1 KB is a cramped half-screen full, which
   should be more than enough.  */
char mods_args[1024];

/* The number of bytes in mods_args already consumed.  */
unsigned mods_args_len;

/* Printable names of the boot modules.  */
const char *mod_names[] = { "physmem-mod", "task-mod",
			    "deva-mod", "deva-store-mod",
			    "root-fs-mod" };

/* The boot modules.  */
struct wortel_module mods[MOD_NUMBER];

/* The number of modules present.  Currently, this is enforced to be
   MOD_NUMBER.  */
unsigned int mods_count;

/* The physical memory server master control capability for the root
   filesystem.  */
hurd_cap_handle_t physmem_master;


/* The maximum number of tasks allowed to use the rootserver.  */
#define MAX_USERS 16

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
l4_memory_desc_t *
loader_get_memory_desc (l4_word_t nr)
{
  return l4_memory_desc (nr);
}


/* Map in the memory used by the modules, and move the physmem module
   to its load address.  This keeps track of the inseparable fpages
   involved.  Also further initialize the module entries.  */
static void
load_components (void)
{
  l4_fpage_t fpages[L4_FPAGE_SPAN_MAX];
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

      nr_fpages = l4_fpage_span (mods[i].start, mods[i].end - 1, fpages);
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
     map to the physical memory server using (inseparable) fpages, we
     request the desired destination fpages up-front.  */
  loader_elf_dest ("physmem-server", mods[MOD_PHYSMEM].start,
		   mods[MOD_PHYSMEM].end, &addr, &end_addr);
  nr_fpages = l4_fpage_span (addr, end_addr - 1, fpages);
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


/* Finish initialization of the module entries.  */
static void
setup_components (void)
{
  unsigned int i;

  /* The first thread number we assign is the one that follows our own
     thread number numerically.  wortel only needs one thread.  */
#define WORTEL_THREADS	1
  l4_word_t thread_no = l4_thread_no (l4_myself ()) + WORTEL_THREADS;

  /* The first task ID we assign is 2.  0 is not a valid task ID
     (because it is not a valid version ID), and we reserve 1 for
     wortel itself (incidentially, our thread version is already 1).
     Because we associate all non-task modules with the preceding
     task, we bump up the task_id number whenever a new task is
     encountered.  So start with one less than 2 to compensate that
     the first time around.  */
  hurd_task_id_t task_id = 2 - 1;

  assert (MOD_IS_TASK (0));

  for (i = 0; i < mods_count; i++)
    {
      if (MOD_IS_TASK (i))
	{
	  task_id++;

	  /* The main thread we create for any task is also the
	     designated server thread for that task.  It is the thread
	     that other tasks will use to invoke RPCs on capabilities
	     provided by this task.  */
	  mods[i].server_thread = l4_global_id (thread_no++, task_id);

	  /* physmem needs three extra threads (one main thread that
	     is not the server thread and two alternating worker
	     threads), because it is started before the task server is
	     running, while the others need none.  */
	  mods[i].nr_extra_threads = (i == MOD_PHYSMEM ? 3 : 0);
	  thread_no += mods[i].nr_extra_threads;

	  /* Allocate some memory for the startup page.  We allocate
	     it here, create a container from it, and then map it into
	     the task's address space.  The only bad thing that can
	     happen is that the task destroys the container while the
	     memory is still mapped (and physmem can't revoke the
	     mapping).  However, only trusted tasks are involved, so
	     it is ok.  FIXME: Maybe use the idea of a designated
	     container in physmem for use of a last-resort page fault
	     service through physmem, see TODO.  */
	  mods[i].startup = sigma0_get_any (HURD_STARTUP_SIZE_LOG2);
	  if (mods[i].startup == L4_NILPAGE)
	    panic ("can not allocate startup code fpage for %s",
		   mod_names[i]);
	}

      mods[i].task_id = task_id;
    }
}


/* Start up the physical memory server.  */
static void
start_physmem (void)
{
  unsigned int i;
  l4_word_t ret;
  l4_word_t control;
  l4_thread_id_t physmem;
  wortel_cap_id_t cap_id;
  l4_fpage_t fpages[L4_FPAGE_SPAN_MAX];
  unsigned int nr_fpages;

  physmem = mods[MOD_PHYSMEM].server_thread;

  /* FIXME: Pass cap_id to physmem.  */
  cap_id = wortel_add_user (mods[MOD_PHYSMEM].task_id);

  /* Create the main thread (which is also the designated server
     thread).  For now, we will be the scheduler.  FIXME: Set the
     scheduler to the task server's scheduler eventually.  */
  ret = l4_thread_control (physmem, physmem, l4_myself (),
			   l4_nilthread, (void *) -1);
  if (!ret)
    panic ("could not create initial physmem thread: %s",
	   l4_strerror (l4_error_code ()));

  /* FIXME: The KIP area and UTCB location for physmem should be more
     configurable (for example via "boot script" parameters or ELF
     symbols).  Below we choose the location of wortel's own memory,
     because this is guaranteed not to be mapped into physmem's
     virtual addresses space.  However, it severly restricts the
     number of threads that can be created in physmem.  It is
     preferable to use some high area between the end of RAM and the
     start of the kernel area.  If there are 3GB RAM however, there is
     no such high address that is not also the location of RAM.  */
  /* FIXME: Irregardless of how we choose the UTCB and KIP area, the
     below code makes assumptions about page alignedness that are
     architecture specific.  And it does not assert that we do not
     accidently exceed the wortel end address.  */
  /* FIXME: In any case, we have to pass the UTCB area size (and
     location) to physmem.  */
  ret = l4_space_control (physmem, 0,
			  l4_fpage_log2 (wortel_start,
					 l4_kip_area_size_log2 ()),
			  l4_fpage_log2 (wortel_start + l4_kip_area_size (),
					 l4_utcb_area_size_log2 ()),
			  l4_anythread, &control);
  if (!ret)
    panic ("could not create physmem address space: %s",
	   l4_strerror (l4_error_code ()));

  /* Activate the physmem thread.  We will be the pager.  We are using
     the first UTCB entry.  */
  ret = l4_thread_control (physmem, physmem, l4_nilthread, l4_myself (),
			   (void *) (wortel_start + l4_kip_area_size ()));
  if (!ret)
    panic ("activation of physmem main thread failed: %s",
	   l4_strerror (l4_error_code ()));

  ret = l4_thread_start (physmem, 0, mods[MOD_PHYSMEM].ip);
  if (!ret)
    panic ("Sending startup message to physmem thread failed: %u",
	   l4_error_code ());

  /* Set up the extra threads for physmem.  FIXME: UTCB location has
     the same issues as described above.  */
  for (i = 1; i <= mods[MOD_PHYSMEM].nr_extra_threads; i++)
    {
      l4_thread_id_t extra_thread;
      
      extra_thread = l4_global_id (l4_thread_no (physmem) + i,
				   mods[MOD_PHYSMEM].task_id);

      /* We create the extra threads as active threads, because
	 inactive threads can not be activated with an exchange
	 register system call.  The scheduler is us.  FIXME:
	 Eventually set the scheduler to the task server's
	 scheduler.  */
      ret = l4_thread_control (extra_thread, physmem, l4_myself (),
			       extra_thread,
			       (void *) (wortel_start + l4_kip_area_size ()
					 + i * l4_utcb_size ()));
      if (!ret)
	panic ("could not create extra physmem thread: %s",
	       l4_strerror (l4_error_code ()));
    }


  /* We want to map all the memory for the physmem binary image with
     the first page fault, but we might have to send several fpages.
     So we first create a list of all fpages we need, then we serve
     one after another, providing the one containing the fault address
     last.  */
  nr_fpages = l4_fpage_span (mods[MOD_PHYSMEM].start,
			     mods[MOD_PHYSMEM].end - 1,
			     fpages);
  
  /* Now serve page requests.  */
  while (nr_fpages)
    {
      l4_msg_tag_t tag;
      l4_map_item_t map_item;
      l4_fpage_t fpage;
      l4_word_t addr;
      
      tag = l4_receive (physmem);
      if (l4_ipc_failed (tag))
	panic ("Receiving messages from physmem thread failed: %u",
	       (l4_error_code () >> 1) & 0x7);
      if (!l4_is_pagefault (tag))
	panic ("Message from physmem thread is not a page fault");
      addr = l4_pagefault (tag, NULL, NULL);
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
	 load_components, so map it right away.  */
      debug ("Mapping fpage: 0x%x/%u\n", l4_address (fpage),
	     l4_size_log2 (fpage));
      /* FIXME: Keep track of mappings already provided.  Possibly
	 map text section rx and data rw.  */
      map_item = l4_map_item (fpage, l4_address (fpage));
      ret = l4_pagefault_reply (physmem, (void *) &map_item);
      if (!ret)
	panic ("sending pagefault reply to physmem failed: %u\n",
	       l4_error_code ());
    }
}


/* These symbols mark the beginning and end of the startup code.  */
extern void startup_bin_start;
extern void startup_bin_end;

#define STARTUP_LOAD_ADDR	0x8000

#include "elf.h"

/* Start up the physical memory server.  */
static void
start_task (void)
{
  /* The virtual memory layout of the task server: startup code starts
     at HURD_STARTUP_START (32K), stack grows down from
     HURD_STARTUP_START + HURD_STARTUP_SIZE (64K).  64K: Kernel
     interface page, followed by the UTCB area.  */
  l4_word_t start = l4_address (mods[MOD_TASK].startup);
  l4_word_t size = l4_size (mods[MOD_TASK].startup);
  l4_word_t stack;
  l4_word_t startup_bin_size;
  l4_word_t entry_point;
  l4_thread_id_t task;
  l4_word_t ret;
  l4_word_t control;
  char *argz = NULL;
  size_t argz_len = 0;
  unsigned int mapc;
  l4_word_t mapv;
  struct hurd_startup_map *phys_mapv;
  struct hurd_startup_data *startup;
  struct hurd_startup_data *phys_startup;
  l4_word_t task_entry_point;

  /* First clear everything.  This ensures that the .bss section is
     initialized to zero.  */
  memset ((void *) start, 0, size);

  /* FIXME: This assumes that the stack grows downwards.  */
#define STACK_GROWS_DOWNWARDS	1

#define STARTUP_TO_PHYS(addr) ((char *) (start + ((l4_word_t) addr)))
#define STARTUP_TO_VIRT(addr) ((char *) (((l4_word_t) STARTUP_LOAD_ADDR) \
					 + ((l4_word_t) addr)))

  /* FIXME: ALLOCA could take an argument that specifies the
     alignment.  */
#if STACK_GROWS_DOWNWARDS
  entry_point = 0;
  stack = size;
#define PUSH(val)						\
  do								\
    {								\
      stack -= sizeof (l4_word_t);				\
      *((l4_word_t *) STARTUP_TO_PHYS (stack)) = val;		\
    }								\
  while (0)
#define ALLOCA(nr)						\
  do								\
    {								\
      unsigned int c;						\
      c = ((nr) + sizeof (l4_word_t) - 1)			\
          & ~(sizeof (l4_word_t) - 1);				\
      stack -= c;						\
    }								\
  while (0)
#else
  entry_point = STARTUP_STACK_TOTAL;
  stack = 0;
#define PUSH(val)						\
  do								\
    {								\
      stack += sizeof (l4_word_t);				\
      *((l4_word_t *) STARTUP_TO_PHYS (stack)) = val;		\
    }								\
  while (0)
#define ALLOCA(nr)						\
  do								\
    {								\
      unsigned int c;						\
      c = ((nr) + sizeof (l4_word_t) - 1)			\
          & ~(sizeof (l4_word_t) - 1);				\
      stack += c;						\
    }								\
  while (0)
#endif

  ALLOCA (sizeof (struct hurd_startup_data));
  startup = (struct hurd_startup_data *) stack;
  phys_startup = (struct hurd_startup_data *) STARTUP_TO_PHYS (startup);

  /* Prepare the argument vectors.  */
  if (mods[MOD_TASK].args)
    {
      char *dst;
      char *src;
      size_t len;

      /* Determine the argument line dimensions.  */
      src = mods[MOD_TASK].args;
      len = strlen (src) + 1;
      ALLOCA (len);
      argz = (char *) stack;
      dst = ((char *) STARTUP_TO_PHYS (argz));

      /* Now walk the argument line backwards, separating arguments
	 with binary zeroes.  */
      while (len)
	{
	  while (len && (*src == ' ' || *src == '\t'))
	    {
	      src++;
	      len--;
	    }

	  if (len)
	    {
	      do
		{
		  *(dst++) = *(src++);
		  len--;
		  argz_len++;
		}
	      while (len && !(*src == ' ' || *src == '\t'));

	      *(dst++) = '\0';
	      argz_len++;
	    }
	}
    }


  {
    /* Prepare the map items.  */

    /* We have to arrange the map items so that the receive windows
       are proper fpages.  We don't know the physical memory layout of
       the container, so there is nothing we can do about choosing
       appropriate offsets to go along with.  The physmem server will
       take care of that - which means that he may return a smaller
       mapping than requested.  */
    l4_word_t min_page_size = l4_min_page_size ();
    /* Get the memory range to which the ELF image from START to END
       (exclusive) will be loaded.  NAME is used for panic
       messages.  */
    const char *name = mods[MOD_TASK].name;
    Elf32_Ehdr *elf = (Elf32_Ehdr *) mods[MOD_TASK].start;
    int i;

    mapc = 0;

    if (elf->e_ident[EI_MAG0] != ELFMAG0
	|| elf->e_ident[EI_MAG1] != ELFMAG1
	|| elf->e_ident[EI_MAG2] != ELFMAG2
	|| elf->e_ident[EI_MAG3] != ELFMAG3)
      panic ("%s is not an ELF file", name);

    if (elf->e_type != ET_EXEC)
      panic ("%s is not an executable file", name);

    if (!elf->e_phoff)
      panic ("%s has no valid program header offset", name);

    /* FIXME: Some architectures support both word sizes.  */
    if (!((elf->e_ident[EI_CLASS] == ELFCLASS32
	   && L4_WORDSIZE == 32)
	  || (elf->e_ident[EI_CLASS] == ELFCLASS64
	      && L4_WORDSIZE == 64)))
      panic ("%s has invalid word size", name);
    if (!((elf->e_ident[EI_DATA] == ELFDATA2LSB
	   && L4_BYTE_ORDER == L4_LITTLE_ENDIAN)
	  || (elf->e_ident[EI_DATA] == ELFDATA2MSB
	      && L4_BYTE_ORDER == L4_BIG_ENDIAN)))
      panic ("%s has invalid byte order", name);

#if i386
# define elf_machine EM_386
#elif PPC
# define elf_machine EM_PPC
#else
# error Not ported to this architecture!
#endif

    if (elf->e_machine != elf_machine)
      panic ("%s is not for this architecture", name);

    task_entry_point = elf->e_entry;

    if (!elf->e_phnum)
      panic ("%s does not have any program headers", name);

    /* We walk the program headers backwards, as we are pushing the
       items onto the list backwards as well.  */
    i = elf->e_phnum;
    do
      {
	Elf32_Phdr *ph;

	i--;

	ph = (Elf32_Phdr *) (((char *) elf) + elf->e_phoff
			     + i * elf->e_phentsize);
	if (ph->p_type == PT_LOAD)
	  {
	    /* We add first the extra allocated memory map item
	       because the array is built up in the reversed
	       order.  */
	    if (ph->p_memsz > ph->p_filesz)
	      {
		/* The file may need some extra memory allocated.  */
		l4_word_t memsz = l4_page_round (ph->p_memsz);
		l4_word_t filesz = l4_page_round (ph->p_filesz);

		if (memsz > filesz)
		  {
		    ALLOCA (sizeof (struct hurd_startup_map));
		    mapv = stack;
		    phys_mapv = (struct hurd_startup_map *)
		      STARTUP_TO_PHYS (mapv);

		    /* FIXME When we have physical mem containers for
		       allocation, use them.  */
		    phys_mapv->cont.server = mods[MOD_PHYSMEM].server_thread;
		    phys_mapv->cont.cap_id = mods[MOD_TASK].mem_cont;
		    phys_mapv->size = memsz - filesz;
		    /* The dirty hack here causes physmem to allocate
		       anonymous memory.  */
		    phys_mapv->offset = l4_page_trunc (ph->p_vaddr) + filesz
		      | ((ph->p_flags & PF_X) ? L4_FPAGE_EXECUTABLE : 0)
		      | ((ph->p_flags & PF_W) ? L4_FPAGE_WRITABLE : 0)
		      | ((ph->p_flags & PF_R) ? L4_FPAGE_READABLE : 0);
		    phys_mapv->vaddr
		      = (void *) (l4_page_trunc (ph->p_vaddr) + filesz);
		    mapc++;
		  }
	      }
	    if (ph->p_filesz)
	      {
		ALLOCA (sizeof (struct hurd_startup_map));
		mapv = stack;
		phys_mapv = (struct hurd_startup_map *)
		  STARTUP_TO_PHYS (mapv);

		/* FIXME When we have physical mem containers for
		   allocation, use them.  */
		phys_mapv->cont.server = mods[MOD_PHYSMEM].server_thread;
		phys_mapv->cont.cap_id = mods[MOD_TASK].mem_cont;
		phys_mapv->size = l4_page_round (ph->p_filesz);;
		phys_mapv->offset = l4_page_trunc (ph->p_offset)
		  | ((ph->p_flags & PF_X) ? L4_FPAGE_EXECUTABLE : 0)
		  | ((ph->p_flags & PF_W) ? L4_FPAGE_WRITABLE : 0)
		  | ((ph->p_flags & PF_R) ? L4_FPAGE_READABLE : 0);
		phys_mapv->vaddr = (void *) l4_page_trunc (ph->p_vaddr);
		mapc++;
	      }
	  }
      }
    while (i > 0);
    mapv = stack;
  }

  /* Fill the startup structure.  */
  phys_startup->version_major = HURD_STARTUP_VERSION_MAJOR;
  phys_startup->version_minor = HURD_STARTUP_VERSION_MINOR;
  phys_startup->argz = argz_len ? STARTUP_TO_VIRT (argz) : NULL;
  phys_startup->argz_len = argz_len;
  phys_startup->wortel.server = l4_my_global_id ();
  phys_startup->wortel.cap_id = wortel_add_user (mods[MOD_TASK].task_id);
  phys_startup->image.server = mods[MOD_PHYSMEM].server_thread;
  phys_startup->image.cap_id = mods[MOD_TASK].mem_cont;
  phys_startup->mapc = mapc;
  phys_startup->mapv = (struct hurd_startup_map *) STARTUP_TO_VIRT (mapv);
  /* The program header is already a virtual address for the task.  */
  phys_startup->phdr = (void *) mods[MOD_TASK].header_loc;
  phys_startup->phdr_len = mods[MOD_TASK].header_size;
  phys_startup->entry_point = (void *) task_entry_point;
  phys_startup->startup.server = mods[MOD_PHYSMEM].server_thread;
  phys_startup->startup.cap_id = mods[MOD_TASK].startup_cont;
   
  /* The stack layout is in accordance to the following startup prototype:
     void start (struct hurd_startup_data *startup_data).  */
  PUSH ((l4_word_t) STARTUP_TO_VIRT (startup));

  /* FIXME: We can not check for .bss section overflow.  So we just
     reserve 2K for .bss which should be plenty.  Normally, .bss only
     includes the global variables used for libl4 (__l4_kip and the
     system call stubs).  */
#define STARTUP_BSS	2048
  /* We reserve 4KB for the stack.  */
#define STARTUP_STACK_TOTAL	4096
  startup_bin_size = ((char *) &startup_bin_end)
    - ((char *) &startup_bin_start);
  if (startup_bin_size + STARTUP_BSS > size - STARTUP_STACK_TOTAL)
    panic ("startup binary does not fit into startup area");

  memcpy ((void *) (start + entry_point),
	  &startup_bin_start, startup_bin_size);

  task = mods[MOD_TASK].server_thread;

  /* Create the main thread (which is also the designated server
     thread).  For now, we will be the scheduler.  FIXME: Set the
     scheduler to the task server's scheduler eventually.  */
  ret = l4_thread_control (task, task, l4_myself (),
			   l4_nilthread, (void *) -1);
  if (!ret)
    panic ("could not create initial task thread: %s",
	   l4_strerror (l4_error_code ()));

  /* FIXME: The KIP area and UTCB location for task should be more
     configurable (for example via "boot script" parameters or ELF
     symbols).  */
  /* FIXME: Irregardless of how we choose the UTCB and KIP area, the
     below code makes assumptions about page alignedness that are
     architecture specific.  And it does not assert that we do not
     accidently exceed the wortel end address.  */
  /* FIXME: In any case, we have to pass the UTCB area size (and
     location) to task.  */
  ret = l4_space_control (task, 0,
			  l4_fpage_log2 (((l4_word_t) HURD_STARTUP_ADDR)
					 + HURD_STARTUP_SIZE,
					 l4_kip_area_size_log2 ()),
			  l4_fpage_log2 (((l4_word_t) HURD_STARTUP_ADDR)
					 + HURD_STARTUP_SIZE
					 + l4_kip_area_size (),
					 l4_utcb_area_size_log2 ()),
			  l4_anythread, &control);
  if (!ret)
    panic ("could not create task address space: %s",
	   l4_strerror (l4_error_code ()));

  /* Activate the task thread.  We will be the pager.  We are using
     the first UTCB entry.  */
  ret = l4_thread_control (task, task, l4_nilthread, l4_myself (),
			   (void *) (HURD_STARTUP_ADDR + HURD_STARTUP_SIZE
				     + l4_kip_area_size ()));
  if (!ret)
    panic ("activation of task main thread failed: %s",
	   l4_strerror (l4_error_code ()));

  /* Calculate the new entry point and stack address
     (virtual addresses of the task).  */
  ret = l4_thread_start (task, (l4_word_t) STARTUP_TO_VIRT (stack),
			 (l4_word_t) STARTUP_TO_VIRT (entry_point));
  if (!ret)
    panic ("Sending startup message to task thread failed: %u",
	   l4_error_code ());

  assert (!mods[MOD_TASK].nr_extra_threads);

  /* Now serve the first page request.  */
  {
    l4_msg_tag_t tag;
    l4_map_item_t map_item;
    l4_fpage_t fpage = mods[MOD_TASK].startup;
    l4_word_t addr;
      
    tag = l4_receive (task);
    if (l4_ipc_failed (tag))
      panic ("Receiving messages from task thread failed: %u",
	     (l4_error_code () >> 1) & 0x7);
    if (!l4_is_pagefault (tag))
      panic ("Message from task thread is not a page fault");
    addr = l4_pagefault (tag, NULL, NULL);
    if (addr != (l4_word_t) STARTUP_TO_VIRT (entry_point))
      panic ("Page fault at unexpected address 0x%x (expected 0x%x)",
	     addr, STARTUP_TO_VIRT (entry_point));

    /* The memory was already requested from sigma0 by
       load_components, so map it right away.  The mapping will be
       destroyed when the startup code requests the very same mapping
       from physmem via the startup container.  */
    map_item = l4_map_item (fpage, STARTUP_LOAD_ADDR);
    ret = l4_pagefault_reply (task, (void *) &map_item);
    if (!ret)
      panic ("sending pagefault reply to task failed: %u\n",
	     l4_error_code ());
  }
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
  unsigned int i;

  /* The size of the region that we are currently trying to allocate
     for GET_MEM requests.  When this drops below L4_MIN_PAGE_SIZE, no
     more memory is available.  */
  unsigned int get_mem_size = sizeof (l4_word_t) * 8 - 1;

  /* True if we need to remap the page at address 0.  */
  int get_page_zero = 0;

  /* For each container we want to create, we have one item in this
     array.  */
  struct
  {
    unsigned int module;
    l4_word_t start;
    l4_word_t end;
    hurd_cap_handle_t *cont;
  } container[2 * mods_count + 1];
  unsigned int nr_cont = 0;
  unsigned int cur_cont = 0;

  /* This is the physmem thread that sent us the GET_TASK_CAP RPC.  */
  l4_thread_id_t physmem;

  /* Make a list of all the containers we want.  */
  for (i = 0; i < mods_count; i++)
    {
      /* We do not want to create containers for the physical memory
	 server.  It already has its own memory.  */
      if (i == MOD_PHYSMEM)
	continue;

      if (MOD_IS_TASK(i))
	{
	  container[nr_cont].module = i;
	  container[nr_cont].start = l4_address (mods[i].startup);
	  container[nr_cont].end = l4_address (mods[i].startup)
	    + l4_size (mods[i].startup) - 1;
	  container[nr_cont].cont = &mods[i].startup_cont;
	  nr_cont++;
	}

      container[nr_cont].module = i;
      container[nr_cont].start = mods[i].start;
      /* FIXME: Make damn sure somewhere that every module contains at
	 least one byte.  */
      container[nr_cont].end = mods[i].end - 1;
      container[nr_cont].cont = &mods[i].mem_cont;

      {
	/* We zero out the unused bytes on the last page, because they
	   may be part of a .bss section.  This could be somewhere
	   else, of course, but this place is just as good as any
	   other.  */
	l4_word_t start = container[nr_cont].end;
	l4_word_t end = start | (l4_min_page_size () - 1);

	memset ((char *) (start + 1), 0, end - start);
      }

      nr_cont++;

    }


  /* If a conventinal page with address 0 exists in the memory
     descriptors, allocate it because we don't want to bother anybody
     with that silly page.  FIXME: We should eventually remap it to a
     high address and provide it to physmem.  */
  for (i = 0; i < loader_get_num_memory_desc (); i++)
    {
      l4_memory_desc_t *memdesc = loader_get_memory_desc (i);
      if (memdesc->low == 0)
	get_page_zero = (memdesc->type == L4_MEMDESC_CONVENTIONAL);
    }
  if (get_page_zero)
    sigma0_get_fpage (l4_fpage (0, l4_min_page_size ()));


  do
    {
      l4_thread_id_t from = l4_anythread;
      l4_word_t label;
      wortel_cap_id_t cap_id;
      l4_msg_t msg;
      l4_msg_tag_t tag;

      tag = l4_wait (&from);
      if (l4_ipc_failed (tag))
	panic ("Receiving message failed: %u", (l4_error_code () >> 1) & 0x7);

      /* FIXME: Remove when not debugging.  */
      if ((l4_label (tag) >> 4) == 0xffe)
	{
	  if (l4_untyped_words (tag) != 2 || l4_typed_words (tag) != 0)
	    panic ("Invalid format of page fault message");
	  panic ("Unexpected page fault from 0x%x at address 0x%x (IP 0x%x)",
		 from, l4_msg_word (msg, 0), l4_msg_word (msg, 1));
	}

      label = l4_label (tag) >> WORTEL_MSG_CAP_ID_BITS;
      cap_id = l4_label (tag) & ((1 << WORTEL_MSG_CAP_ID_BITS) - 1);

#if 0
      if (!WORTEL_CAP_VALID (cap_id, l4_version (from)))
	/* FIXME: Shouldn't be a panic of course.  */
	panic ("Unprivileged user 0x%x attemps to access wortel rootserver 0x%x",
	       from, cap_id);
#endif

      l4_msg_store (tag, msg);

      if (label == WORTEL_MSG_PUTCHAR)
	{
	  int chr;

	  chr = (int) l4_msg_word (msg, 0);
	  putchar (chr);

	  l4_msg_clear (msg);
	  l4_msg_load (msg);
	  l4_reply (from);
	}
      else if (label == WORTEL_MSG_SHUTDOWN)
	panic ("Bootstrap failed");
      else if (label == WORTEL_MSG_GET_MEM)
	{
	  l4_fpage_t fpage;
	  l4_map_item_t map_item;

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
		if (fpage == L4_NILPAGE)
		  get_mem_size--;
	      }
	    while (fpage == L4_NILPAGE
		   && get_mem_size >= L4_MIN_PAGE_SIZE_LOG2);

	  /* When we get the nilpage, then this is an indication that
	     we grabbed all possible conventional memory.  We still
	     own our own memory we run on, and whatever memory the
	     output driver is using (for example VGA mapped
	     memory).  */
	  map_item = l4_map_item (fpage, l4_address (fpage));

	  l4_msg_clear (msg);
	  l4_msg_append_map_item (msg, map_item);
	  l4_msg_load (msg);
	  l4_reply (from);
	}
      else if (label == WORTEL_MSG_GET_THREADS)
	{
	  /* FIXME: Use the task ID to find out which server wants
	     this info?  */
	  l4_msg_clear (msg);
	  l4_msg_append_word (msg, mods[MOD_PHYSMEM].nr_extra_threads);
	  l4_msg_load (msg);
	  l4_reply (from);
	}
      else if (label == WORTEL_MSG_GET_CAP_REQUEST)
	{
	  if (cur_cont > nr_cont)
	    panic ("physmem does not stop requesting capabilities");
	  else if (cur_cont == nr_cont)
	    {
	      /* Request the global control capability now.  */
	      l4_msg_clear (msg);
	      l4_msg_append_word
		(msg, l4_version (mods[MOD_ROOT_FS].server_thread));
	      l4_msg_load (msg);
	      l4_reply (from);
	    }
	  else
	    {
	      /* We are allowed to make a capability request now.  */
	      l4_fpage_t fpages[L4_FPAGE_SPAN_MAX];
	      unsigned int nr_fpages;
	      unsigned int i;

	      nr_fpages = l4_fpage_span (container[cur_cont].start,
					 container[cur_cont].end, fpages);

	      /* We can not pass more than 31 map items in our
		 message, because there are only 64 message registers,
		 we need 1 for the task ID, and each map item takes up
		 two message registers.  */
	      if (nr_fpages > 31)
		panic ("%s: Module %s is too large and has an "
		       "unfortunate alignment", __func__,
		       mods[container[cur_cont].module].name);

	      l4_msg_clear (msg);
	      l4_set_msg_label (msg, 0);
	      l4_msg_append_word (msg,
				  mods[container[cur_cont].module].task_id);
	      for (i = 0; i < nr_fpages; i++)
		{
		  l4_map_item_t map_item;
		  l4_fpage_t fpage = fpages[i];

		  map_item = l4_map_item (fpage, l4_address (fpage));
		  l4_msg_append_map_item (msg, map_item);
		}
	      l4_msg_load (msg);
	      l4_reply (from);
	    }
	}
      else if (label == WORTEL_MSG_GET_CAP_REPLY)
	{
	  if (l4_untyped_words (tag) != 1 || l4_typed_words (tag) != 0)
	    panic ("Invalid format of get cap reply msg");

	  if (cur_cont > nr_cont)
	    panic ("Invalid get cap reply message");
	  else if (cur_cont == nr_cont)
	    physmem_master = l4_msg_word (msg, 0);
	  else
	    *container[cur_cont].cont = l4_msg_word (msg, 0);

	  cur_cont++;

	  l4_msg_clear (msg);
	  l4_msg_load (msg);
	  l4_reply (from);
	}
      else if (label == WORTEL_MSG_GET_TASK_CAP)
	{
	  /* This RPC is used by physmem after the memory container
	     caps have been successfully created to request the task
	     server cap of the physmem task.  */

	  /* FIXME: Use the task ID to find out which server wants
	     this info.  */

	  /* We have to reply later, when we started up the task
	     server and received the physmem task cap.  */
	  physmem = from;

	  /* Start up the task server and continue serving RPCs.  */
	  start_task ();
	}
      else
	panic ("Invalid message with tag 0x%x", tag);
    }
  while (1);
}


/* Serve rootserver requests.  */
static void
serve_requests (void)
{
  do
    {
      l4_thread_id_t from = l4_anythread;
      l4_word_t label;
      l4_msg_t msg;
      l4_msg_tag_t msg_tag;

      msg_tag = l4_wait (&from);
      if (l4_ipc_failed (msg_tag))
	panic ("Receiving message failed: %u", (l4_error_code () >> 1) & 0x7);

      label = l4_label (msg_tag);
      /* FIXME: Shouldn't store the whole msg before checking access
	 rights.  */
      l4_msg_store (msg_tag, msg);
      if (!WORTEL_CAP_VALID (l4_msg_word (msg, 0), l4_version (from)))
	/* FIXME: Shouldn't be a panic of course.  */
	panic ("Unprivileged user 0x%x attemps to access wortel rootserver",
	       from);

      if (label == WORTEL_MSG_PUTCHAR)
	{
	  int chr;

	  /* This is a putchar() message.  */
	  if (l4_untyped_words (msg_tag) != 2
	      || l4_typed_words (msg_tag) != 0)
	    panic ("Invalid format of putchar msg");

	  chr = (int) l4_msg_word (msg, 1);
	  putchar (chr);
	  /* No reply needed.  */
	  continue;
	}
      else if (label == WORTEL_MSG_SHUTDOWN)
	panic ("Bootstrap failed");
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

  setup_components ();

  start_physmem ();

  serve_bootstrap_requests ();

  serve_requests ();

  return 0;
}
