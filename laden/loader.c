/* loader.c - Load ELF files.
   Copyright (C) 2003, 2007 Free Software Foundation, Inc.
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

#include <string.h>
#include <l4/kip.h>

#include "laden.h"
#include "loader.h"
#include "output.h"
#include "shutdown.h"

#include "elf.h"

/* Verify that the memory region START to END (exclusive) is valid.  */
static void
mem_check (const char *name, l4_word_t start, l4_word_t end)
{
  l4_memory_desc_t *memdesc;
  int nr;
  int fits = 0;
  int conflicts = 0;

  if (!loader_get_num_memory_desc ())
    return;

  end--;

  /* FIXME: This implementation does not account for conventional
     memory overriding non-conventional memory in the descriptor
     list.  */
  for (nr = 0; nr < loader_get_num_memory_desc (); nr++)
    {
      memdesc = loader_get_memory_desc (nr);

      if (memdesc->type == L4_MEMDESC_CONVENTIONAL)
	{
	  /* Check if the region fits into conventional memory.  */
	  if (start >= l4_memory_desc_low (memdesc)
	      && start <= l4_memory_desc_high (memdesc)
	      && end >= l4_memory_desc_low (memdesc)
	      && end <= l4_memory_desc_high (memdesc))
	    fits = 1;
	}
      else
	{
	  /* Check if the region overlaps with non-conventional
	     memory.  */
	  if ((start >= l4_memory_desc_low (memdesc)
	       && start <= l4_memory_desc_high (memdesc))
	      || (end >= l4_memory_desc_low (memdesc)
		  && end <= l4_memory_desc_high (memdesc))
	      || (start < l4_memory_desc_low (memdesc)
		  && end > l4_memory_desc_high (memdesc)))
	    {
	      fits = 0;
	      conflicts = 1 + nr;
	    }
	}
    }

  if (!fits)
    {
      if (conflicts)
	{
	  memdesc = loader_get_memory_desc (conflicts - 1);
	  panic ("%s (0x%" L4_PRIxWORD " - 0x%" L4_PRIxWORD ") conflicts "
		 "with memory of type %i/%i (0x%" L4_PRIxWORD " - 0x%"
		 L4_PRIxWORD ")", name, start, end + 1,
		 memdesc->type, memdesc->subtype,
		 l4_memory_desc_low (memdesc), l4_memory_desc_high (memdesc));
	}
      else
	panic ("%s (0x%" L4_PRIxWORD " - 0x%" L4_PRIxWORD ") does not fit "
	       "into memory", name, start, end + 1);
    }
}


/* We use a table of memory regions to check for overlap.  */

#define MAX_REGIONS 16

static struct
{
  const char *name;
  l4_word_t start;
  l4_word_t end;
  /* Function pointer to relocate this region in case of conflict.  */  
  relocate_region rr;
  void *cookie;

  /* If this descriptor is valid.  */
  int used;
} used_regions[MAX_REGIONS];

static int nr_regions;


/* Add the region with the name NAME from START to END to the table of
   regions to check against.  Before doing that, check for overlaps
   with existing regions.  */
void
loader_add_region (const char *name, l4_word_t start, l4_word_t end,
		   relocate_region rr, void *cookie)
{
  debug ("Protected Region: %s (0x%x - 0x%x)\n", name, start, end);

  if (start >= end)
    panic ("Region %s has a start address following the end address", name);

  /* Make sure that the requested region corresponds to real
     memory.  */
  mem_check (name, start, end);

  /* Allocate a region descriptor.  */
  int region = nr_regions;
  if (region == MAX_REGIONS)
    {
      for (region = 0; region < MAX_REGIONS; region ++)
	if (! used_regions[region].used)
	  break;

      if (region == MAX_REGIONS)
	panic ("Out of memory region descriptors, region %s doesn't fit",
	       name);
    }
  else
    nr_regions ++;

  used_regions[region].name = name;
  used_regions[region].start = start;
  used_regions[region].end = end;
  used_regions[region].rr = rr;
  used_regions[region].cookie = cookie;
  used_regions[region].used = 1;

  /* Check if the region overlaps with any established regions.  */
  int i;
  for (i = 0; i < nr_regions; i++)
    if (i != region
	&& used_regions[i].used
	&& ((start >= used_regions[i].start && start < used_regions[i].end)
	    || (end >= used_regions[i].start && end <= used_regions[i].end)
	    || (start < used_regions[i].start && end > used_regions[i].start)))
      /* Region REGION conflict with region I.  Try to relocate region
	 I.  */
      {
	l4_word_t mstart = used_regions[i].start;
	l4_word_t mend = used_regions[i].end;
	l4_word_t msize = mend - mstart + 1;

	if (! used_regions[i].rr)
	  /* Region I is not relocatable.  */
	  panic ("%s (0x%x - 0x%x) conflicts with %s (0x%x - 0x%x)",
		 used_regions[region].name, start, end,
		 used_regions[i].name, mstart, mend);

	debug ("%s (0x%x - 0x%x) conflicts with %s (0x%x - 0x%x);"
	       " moving latter\n",
	       used_regions[region].name, start, end,
	       used_regions[i].name, mstart, mend);

	int j;
	for (j = 0; j < loader_get_num_memory_desc (); j ++)
	  {
	    l4_memory_desc_t *memdesc = loader_get_memory_desc (j);
	    l4_word_t mem_low = l4_memory_desc_low (memdesc);
	    l4_word_t mem_high = l4_memory_desc_high (memdesc);

	    printf ("memory_map: %d, %d, 0x%x-0x%x\n",
		    j, memory_map[j].type, mem_low, mem_high);
	    if (memory_map[j].type == L4_MEMDESC_CONVENTIONAL
		&& mem_high - mem_low >= msize)
	      /* This memory map is useable and large enough.  See if
		 there is a non-conflicting, contiguous space of SIZE
		 bytes.  */
	      {
		debug ("Considering memory (0x%x-0x%x)\n",
		       mem_low, mem_high);

		/* Start with the lowest address.  */
		l4_word_t ns = mem_low;

		/* See if any regions overlap with (NS, NS + SIZE].  If
		   so, advance NS just beyond them.  */
		int k;
	      restart:
		for (k = 0;
		     k < nr_regions && ns + msize - 1 <= mem_high;
		     k ++)
		  if (used_regions[k].used && k != i
		      && ((ns >= used_regions[k].start
			   && ns <= used_regions[k].end)
			  || (ns + msize - 1 >= used_regions[k].start
			      && ns + msize - 1 <= used_regions[k].end)
			  || (ns < used_regions[k].start
			      && ns + msize - 1 >= used_regions[k].start)))
		    /* There is overlap, take the end of the conflicting
		       region as our new start and restart.  */
		    {
		      debug ("Can't use 0x%x, %s(0x%x-0x%x) in way\n",
			     ns, used_regions[k].name,
			     used_regions[k].start, used_regions[k].end);
		      ns = used_regions[k].end + 1;
		      debug ("Trying address 0x%x\n", ns);
		      goto restart;
		    }

		if (k == nr_regions && ns + msize - 1 <= mem_high)
		  /* NS is a good new start!  */
		  {
		    printf ("Moving %s(%d) from 0x%x-0x%x to 0x%x-0x%x\n",
			    used_regions[i].name, i,
			    used_regions[i].start, used_regions[i].end,
			    ns, ns + msize - 1);

		    memmove ((void *) ns, (void *) used_regions[i].start,
			     msize);
		    used_regions[i].start = ns;
		    used_regions[i].end = ns + msize - 1;
		    used_regions[i].rr (used_regions[i].name,
					used_regions[i].start,
					used_regions[i].end,
					ns, used_regions[i].cookie);
		    break;
		  }
	      }
	  }

	if (j == memory_map_size)
	  /* Couldn't find a free region!  */
	  panic ("%s (0x%x - 0x%x) conflicts with %s (0x%x - 0x%x)"
		 " but nowhere to move the latter",
		 used_regions[region].name,
		 used_regions[region].start, used_regions[region].end,
		 used_regions[i].name,
		 used_regions[i].start, used_regions[i].end);
      }
}


/* Remove the region with the name NAME from the table.  */
void
loader_remove_region (const char *name)
{
  int i;

  for (i = 0; i < nr_regions; i++)
    if (!strcmp (used_regions[i].name, name))
      break;

  if (i == nr_regions)
    panic ("Assertion failure: Could not find region %s for removal", name);

  used_regions[i].used = 0;
}


/* Get the memory range to which the ELF image from START to END
   (exclusive) will be loaded.  NAME is used for panic messages.  */
void
loader_elf_dest (const char *name, l4_word_t start, l4_word_t end,
		 l4_word_t *new_start_p, l4_word_t *new_end_p)
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

  for (i = 0; i < elf->e_phnum; i++)
    {
      Elf32_Phdr *ph = (Elf32_Phdr *) (start + elf->e_phoff
				       + i * elf->e_phentsize);
      if (ph->p_type == PT_LOAD)
	{
	  if (ph->p_paddr < new_start)
	    new_start = ph->p_paddr;
	  if (ph->p_memsz + ph->p_paddr > new_end)
	    new_end = ph->p_memsz + ph->p_paddr;
	}
    }

  if (new_start_p)
    *new_start_p = new_start;
  if (new_end_p)
    *new_end_p = new_end;
}


/* Load the ELF image from START to END (exclusive) into memory under
   the name NAME (also used as the name for the region of the
   resulting ELF program).  Return the lowest and highest address used
   by the program in NEW_START_P and NEW_END_P, and the entry point in
   ENTRY.  */
void
loader_elf_load (const char *name, l4_word_t start, l4_word_t end,
		 l4_word_t *new_start_p, l4_word_t *new_end_p,
		 l4_word_t *entry)
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

  /* Determine the bounds of the extracted executable.  */  
  for (i = 0; i < elf->e_phnum; i++)
    {
      Elf32_Phdr *ph = (Elf32_Phdr *) (start + elf->e_phoff
				       + i * elf->e_phentsize);
      if (ph->p_type == PT_LOAD)
	{
	  /* FIXME: Add this as a bootloader specific memory type to L4's
	     memdesc list instead.  */
	  /* Add the region.  */
	  loader_add_region (name, ph->p_paddr, ph->p_paddr + ph->p_memsz - 1,
			     NULL, NULL);

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

  if (new_start_p)
    *new_start_p = new_start;
  if (new_end_p)
    *new_end_p = new_end;
  if (entry)
    *entry = elf->e_entry;
}
