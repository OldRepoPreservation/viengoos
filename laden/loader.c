/* loader.c - Load ELF files.
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

#include "laden.h"

#include "elf.h"



/* Verify that the memory region START to END (inclusive) is valid.  */
static void
mem_check (const char *name, unsigned long start, unsigned long end)
{
  int nr;
  int fits = 0;
  int conflicts = 0;

  /* FIXME: This implementation does not account for conventional
     memory overriding non-conventional memory in the descriptor
     list.  */

  for (nr = 0; nr < memory_map_size; nr++)
    {
      l4_memory_desc_t memdesc = &memory_map[nr];

      if (memdesc->type == L4_MEMDESC_CONVENTIONAL)
	{
	  /* Check if the region fits into conventional memory.  */
	  if (start >= (memdesc->low << 10) && start < (memdesc->high << 10)
	      && end >= (memdesc->low << 10) && end < (memdesc->high << 10))
	    fits = 1;
	}
      else
	{
	  /* Check if the region overlaps with non-conventional
	     memory.  */
	  if ((start >= (memdesc->low << 10) && start < (memdesc->high << 10))
	      || (end >= (memdesc->low << 10) && end < (memdesc->high << 10))
	      || (start < (memdesc->low << 10) && end > (memdesc->high << 10)))
	    {
	      conflicts = 1;
	      break;
	    }
	}
    }
  if (conflicts)
    panic ("%s (0x%x - 0x%x) conflicts with memory of "
	   "type %i/%i (0x%x - 0x%x)", name, start, end,
	   memory_map[nr].type, memory_map[nr].subtype,
	   memory_map[nr].low << 10, memory_map[nr].high << 10);
  if (!fits)
    panic ("%s (0x%x - 0x%x) does not fit into memory",
	   name, start, end);
}


/* We use a table of memory regions to check for overlap.  */

#define MAX_REGIONS 16

static struct
{
  char *name;
  l4_word_t start;
  l4_word_t end;
} used_regions[MAX_REGIONS];

static int nr_regions;


/* Check that the region with the name NAME from START to END does not
   overlap with an existing region.  */
static void
check_region (char *name, l4_word_t start, l4_word_t end)
{
  int i;

  mem_check (name, start, end);
  
  for (i = 0; i < nr_regions; i++)
    {
      if ((start >= used_regions[i].start && start < used_regions[i].end)
	  || (end >= used_regions[i].start && end < used_regions[i].end)
	  || (start < used_regions[i].start && end >= used_regions[i].start))
	panic ("%s (0x%x - 0x%x) conflicts with %s (0x%x - 0x%x)",
	       name, start, end, used_regions[i].name, used_regions[i].start,
	       used_regions[i].end);
    }
}


/* Add the region with the name NAME from START to END to the table of
   regions to check against.  Before doing that, check for overlaps
   with existing regions.  */
void
loader_add_region (char *name, l4_word_t start, l4_word_t end)
{
  debug ("Protected Region: %s (0x%x - 0x%x)\n", name, start, end);

  if (nr_regions == MAX_REGIONS)
    panic ("Too many memory regions, region %s doesn't fit", name);

  check_region (name, start, end);

  used_regions[nr_regions].name = name;
  used_regions[nr_regions].start = start;
  used_regions[nr_regions].end = end;
  nr_regions++;
}


/* Remove the region with the name NAME from the table.  */
static void
remove_region (const char *name)
{
  int i;

  for (i = 0; i < nr_regions; i++)
    if (!strcmp (used_regions[i].name, name))
      break;

  if (i == nr_regions)
    panic ("Assertion failure: Could not find region %s for removal");

  while (i < nr_regions - 1)
    {
      used_regions[i] = used_regions[i + 1];
      i++;
    }
  nr_regions--;
}


/* Load the ELF image from START to END into memory under the name
   NAME (also used as the name for the region of the resulting ELF
   program).  Return the lowest and highest address used by the
   program in NEW_START_P and NEW_END_P, and the entry point in
   ENTRY.  */
static void
elf_load (char *name, l4_word_t start, l4_word_t end,
	  l4_word_t *new_start_p, l4_word_t *new_end_p, l4_word_t *entry)
{
  l4_word_t new_start = -1;
  l4_word_t new_end = 0;
  int i;

  Elf32_Ehdr *elf = (Elf32_Ehdr *) start;

  if (elf->e_ident[EI_MAG0] != ELFMAG0
      || elf->e_ident[EI_MAG1] != ELFMAG1
      || elf->e_ident[EI_MAG2] != ELFMAG2
      || elf->e_ident[EI_MAG3] != ELFMAG3)
    panic ("%s is not an ELF file", name);

  if (elf->e_type != ET_EXEC)
    panic ("%s is not an executable file", name);

  if (!elf->e_phoff)
    panic ("%s has no valid program header offset", name);

#ifdef i386
  if (elf->e_ident[EI_CLASS] != ELFCLASS32
      || elf->e_ident[EI_DATA] != ELFDATA2LSB
      || elf->e_machine != EM_386)
    panic ("%s is not for this architecture", name);
#else
#error Not ported to this architecture!
#endif

  for (i = 0; i < elf->e_phnum; i++)
    {
      Elf32_Phdr *ph = (Elf32_Phdr *) (start + elf->e_phoff
				       + i * elf->e_phentsize);
      if (ph->p_type == PT_LOAD)
	{
	  check_region (name, ph->p_paddr,
			ph->p_paddr + ph->p_offset + ph->p_memsz);
	  memcpy ((char *) ph->p_paddr, (char *) start + ph->p_offset,
		  ph->p_filesz);
	  /* Initialize the rest.  */
	  if (ph->p_memsz > ph->p_filesz)
	    memset ((char *) ph->p_paddr + ph->p_filesz, 0,
		    ph->p_memsz - ph->p_filesz);
	  if (ph->p_paddr < new_start)
	    new_start = ph->p_paddr;
	  if (ph->p_memsz + ph->p_paddr > new_end)
	    new_end = ph->p_memsz + ph->p_paddr;
	}
    }

  /* FIXME: Add this as a bootloader specific memory type to L4's
     memdesc list instead.  */
  loader_add_region (name, new_start, new_end);

  if (new_start_p)
    *new_start_p = new_start;
  if (new_end_p)
    *new_end_p = new_end;
  if (entry)
    *entry = elf->e_entry;
}


/* Load the ELF images of the kernel and the initial servers into
   memory, checking for overlaps.  Update the start and end
   information with the information from the ELF program, and fill in
   the entry points.  */
void
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

  elf_load ("kernel", kernel.low, kernel.high,
	    &kernel.low, &kernel.high, &kernel.ip);
  remove_region ("kernel-mod");

  elf_load ("sigma0", sigma0.low, sigma0.high,
	    &sigma0.low, &sigma0.high, &sigma0.ip);
  remove_region ("sigma0-mod");

  if (sigma1.low)
    {
      elf_load ("sigma1", sigma1.low, sigma1.high,
		&sigma1.low, &sigma1.high, &sigma1.ip);
      remove_region ("sigma1-mod");
    }

  elf_load ("rootserver", rootserver.low, rootserver.high,
	    &rootserver.low, &rootserver.high, &rootserver.ip);
  remove_region ("rootserver-mod");
}
