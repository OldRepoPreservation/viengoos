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

#include <alloca.h>
#include <stdint.h>

#include "laden.h"

#include "multiboot.h"


/* Return a help text for this architecture.  */
const char *
help_arch (void)
{
  return "The first module must be the L4 kernel, the second module "
    "sigma0 and the third\n"
    "module the rootserver.  Subsequent modules are passed "
    "through to the rootserver\n"
    "and handled by it.\n";
}


/* Check if the bit BIT in FLAGS is set.  */
#define CHECK_FLAG(flags,bit)	((flags) & (1 << (bit)))


/* Setup the argument vector and pass control over to the main
   function.  */
void
cmain (uint32_t magic, multiboot_info_t *mbi)
{
  int argc = 0;
  char **argv = 0;

  /* Verify that we are booted by a Multiboot-compliant boot loader.  */
  if (magic != MULTIBOOT_BOOTLOADER_MAGIC)
    panic ("Invalid magic number: 0x%x", magic);

  if (!CHECK_FLAG (mbi->flags, 0) && !CHECK_FLAG (mbi->flags, 6))
    panic ("Bootloader did not provide a memory map");

  if (CHECK_FLAG (mbi->flags, 2))
    {
      /* A command line was passed.  */
      char *str = (char *) mbi->cmdline;
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
      str = (char *) mbi->cmdline;

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
      argv[0] = PROGRAM_NAME;
      argv[1] = 0;
    }

  /* The boot info is set to the multiboot info on ia32.  We use this
     also to get at the multiboot info from other functions called at
     a later time.  */
  boot_info = (uint32_t) mbi;

  /* Now invoke the main function.  */
  main (argc, argv);

  /* Never reached.  */
}    


static void
debug_dump (void)
{
  multiboot_info_t *mbi = (multiboot_info_t *) boot_info;

  if (CHECK_FLAG (mbi->flags, 9))
    debug ("Booted by %s\n", (char *) mbi->boot_loader_name);

  if (CHECK_FLAG (mbi->flags, 0))
    debug ("Memory: Lower %u KB, Upper %u KB\n",
	   mbi->mem_lower, mbi->mem_upper);

  if (CHECK_FLAG (mbi->flags, 3))
    {
      module_t *mod = (module_t *) mbi->mods_addr;
      int nr;

      for (nr = 0; nr < mbi->mods_count; nr++)
	debug ("Module %i: Start 0x%x, End 0x%x, Cmd %s\n",
	       nr + 1, mod[nr].mod_start, mod[nr].mod_end, mod[nr].string);
    }

  if (CHECK_FLAG (mbi->flags, 6))
    {
      memory_map_t *mmap;
      int nr = 1;

      for (mmap = (memory_map_t *) mbi->mmap_addr;
	   (uint32_t) mmap < mbi->mmap_addr + mbi->mmap_length;
	   mmap = (memory_map_t *) ((uint32_t) mmap
				    + mmap->size + sizeof (mmap->size)))
	debug ("Memory Map %i: Type %i, Base 0x%x%x, Length 0x%x%x\n",
	       nr++, mmap->type, mmap->base_addr >> 32,
	       mmap->base_addr & ((1ULL << 32) - 1),
	       mmap->length >> 32, mmap->length & ((1ULL << 32) - 1));
    }
}


/* Find the kernel, the initial servers and the other information
   required for booting.  */
void
find_components (void)
{
  multiboot_info_t *mbi = (multiboot_info_t *) boot_info;

  debug_dump ();

  /* Load the module information.  */
  if (CHECK_FLAG (mbi->flags, 3))
    {
      module_t *mod = (module_t *) mbi->mods_addr;

      if (mbi->mods_count > 0)
	{
	  kernel.low = mod[0].mod_start;
	  kernel.high = mod[0].mod_end;
	}
      if (mbi->mods_count > 1)
	{
	  sigma0.low = mod[1].mod_start;
	  sigma0.high = mod[1].mod_end;
	}
      if (mbi->mods_count > 1)
	{
	  rootserver.low = mod[2].mod_start;
	  rootserver.high = mod[2].mod_end;
	}
    }

  /* Now create the memory map.  */
  if (CHECK_FLAG (mbi->flags, 6))
    {
      /* mmap_* are valid.  */
      memory_map_t *mmap;

      for (mmap = (memory_map_t *) mbi->mmap_addr;
	   (uint32_t) mmap < mbi->mmap_addr + mbi->mmap_length;
	   mmap = (memory_map_t *) ((uint32_t) mmap
				    + mmap->size + sizeof (mmap->size)))
	{
	  uint64_t end;

	  if (mmap->base_addr >> 32)
	    panic ("L4 does not support more than 4 GB on ia32");

	  end = mmap->base_addr + mmap->length;

	  if (end == (1ULL << 32))
	    {
#if 0
	    panic ("L4 does not support exactly 4 GB on ia32");
#elif 1
	      /* The L4 specification does not seem to allow this
		 configuration.  Truncate the region by dropping the
		 last page.  FIXME: kickstart overflows and sets the
		 high address to 0.  This is unambiguous, but needs to
		 be supported by sigma0 and the operating system.
		 Clarification of the specification is required.  */
	      end = (1ULL << 32) - (1 << 10);
#else
	      /* This is effectively what kickstart does.  */
	      end = 0;
#endif
	    }
	  else if (end >> 32)
	    panic ("L4 does not support more than 4 GB on ia32");

	  if (mmap->base_addr & ((1 << 10) - 1)
	      || mmap->length & ((1 << 10) - 1))
	    panic ("Memory region (0x%x - 0x%x) is unaligned",
		   (uint32_t) mmap->base_addr, (uint32_t) end);

	  add_memory_map ((uint32_t) mmap->base_addr, (uint32_t) end,
			  mmap->type == 1
			  ? L4_MEMDESC_CONVENTIONAL : L4_MEMDESC_ARCH,
			  mmap->type == 1 ? 0 : mmap->type);
	}
    }
  else if (CHECK_FLAG (mbi->flags, 0))
    {
      /* mem_* are valid.  */
      if (mbi->mem_lower & 0x2ff)
	panic ("Lower memory end address 0x%x is unaligned",
	       mbi->mem_lower);
      if (mbi->mem_upper & 0x2ff)
	panic ("Upper memory end address 0x%x is unaligned",
	       mbi->mem_upper);

      add_memory_map (0, mbi->mem_lower << 10, L4_MEMDESC_CONVENTIONAL, 0);
      add_memory_map (0x100000, 0x100000 + (mbi->mem_upper << 10),
		      L4_MEMDESC_CONVENTIONAL, 0);
    }

  /* The VGA memory is usually not included in the BIOS map.  */
  add_memory_map (0xa0000, 0xc0000, L4_MEMDESC_SHARED, 0);

  /* The amount of conventional memory to be reserved for the kernel.  */
#define KMEM_SIZE	(16 * 0x100000)

  /* The upper limit for the end of the kernel memory.  */
#define KMEM_MAX	(240 * 0x100000)

  if (CHECK_FLAG (mbi->flags, 6))
    {
      memory_map_t *mmap;

      for (mmap = (memory_map_t *) mbi->mmap_addr;
	   (uint32_t) mmap < mbi->mmap_addr + mbi->mmap_length;
	   mmap = (memory_map_t *) ((uint32_t) mmap
				    + mmap->size + sizeof (mmap->size)))
	{
	  if (mmap->type != 1)
	    continue;

	  if (((uint32_t) mmap->length) >= KMEM_SIZE
	      && ((uint32_t) mmap->base_addr) <= KMEM_MAX - KMEM_SIZE)
	    {
	      uint32_t high = ((uint32_t) mmap->base_addr)
			       + ((uint32_t) mmap->length);
	      uint32_t low;

	      if (high > KMEM_MAX)
		high = KMEM_MAX;
	      low = high - KMEM_SIZE;
	      /* Round up to the next super page (4 MB).  */
	      low = (low + 0x3fffff) & ~0x3fffff;

	      add_memory_map (low, high, L4_MEMDESC_RESERVED, 0);
	    }
	}
    }
  else if (CHECK_FLAG (mbi->flags, 0))
    {
      if ((mbi->mem_upper << 10) >= KMEM_SIZE)
	{
	  uint32_t high = (mbi->mem_upper << 10) + 0x100000;
	  uint32_t low;

	  if (high > KMEM_MAX)
	    high = KMEM_MAX;

	  low = high - KMEM_SIZE;
	  /* Round up to the next super page (4 MB).  */
	  low = (low + 0x3fffff) & ~0x3fffff;

	  add_memory_map (low, high, L4_MEMDESC_RESERVED, 0);
	}
    }
}
