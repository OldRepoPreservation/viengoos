/* loader.c - Load ELF files.
   Copyright (C) 2003, 2007, 2008 Free Software Foundation, Inc.
   Written by Marcus Brinkmann and Neal H. Walfield.

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

#include <hurd/stddef.h>
#include <string.h>

#include <l4.h>

#include "loader.h"
#include "elf.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

bool
loader_elf_load (loader_allocate_object_callback_t alloc,
		 loader_lookup_object_callback_t lookup,
		 void *start, void *end, uintptr_t *entry)
{
  Elf32_Ehdr *elf = (Elf32_Ehdr *) start;

  if (elf->e_ident[EI_MAG0] != ELFMAG0
      || elf->e_ident[EI_MAG1] != ELFMAG1
      || elf->e_ident[EI_MAG2] != ELFMAG2
      || elf->e_ident[EI_MAG3] != ELFMAG3)
    {
      debug (0, "Not an ELF file");
      return false;
    }

  if (elf->e_type != ET_EXEC)
    {
      debug (0, "Not an executable file");
      return false;
    }

  if (!elf->e_phoff)
    {
      debug (0, "No valid program header offset");
      return false;
    }

  /* FIXME: Some architectures support both word sizes.  */
  if (!((elf->e_ident[EI_CLASS] == ELFCLASS32
	 && L4_WORDSIZE == 32)
	|| (elf->e_ident[EI_CLASS] == ELFCLASS64
	    && L4_WORDSIZE == 64)))
    {
      debug (0, "Invalid word size");
      return false;
    }

  if (!((elf->e_ident[EI_DATA] == ELFDATA2LSB
	 && L4_BYTE_ORDER == L4_LITTLE_ENDIAN)
	|| (elf->e_ident[EI_DATA] == ELFDATA2MSB
	    && L4_BYTE_ORDER == L4_BIG_ENDIAN)))
    {
      debug (0, "Invalid byte order");
      return false;
    }

#if i386
# define elf_machine EM_386
#elif PPC
# define elf_machine EM_PPC
#else
# error Not ported to this architecture!
#endif

  if (elf->e_machine != elf_machine)
    {
      debug (0, "Binary not for this architecture");
      return false;
    }

  /* We have an ELF file.  Load it.  */

  int i;
  for (i = 0; i < elf->e_phnum; i++)
    {
      Elf32_Phdr *ph = (Elf32_Phdr *) (start + elf->e_phoff
				       + i * elf->e_phentsize);
      if (ph->p_type != PT_LOAD)
	continue;

      /* Load this section.  */

      bool ro = ! (ph->p_flags & PF_W);

      uintptr_t addr = ph->p_paddr;

      /* Offset of PH->P_PADDR in the first page.  */
      int offset = ph->p_paddr & (PAGESIZE - 1);
      if (offset)
	/* This section does not start on a page aligned address.  It
	   may be the case that another section is on this page.  If
	   so, don't allocate a new page but use the existing one.  */
	{
	  void *page = lookup (addr - offset);
	  if (! page)
	    page = alloc (addr - offset, ro);

	  /* Copy the data that belongs on the first page.  */
	  memcpy ((void *) page + offset,
		  (void *) start + ph->p_offset,
		  MIN (PAGESIZE - offset, ph->p_filesz));

	  addr = addr - offset + PAGESIZE;
	}

      /* We know process the section a page at a time.  */
      assert ((addr & (PAGESIZE - 1)) == 0);
      for (; addr < ph->p_paddr + ph->p_memsz; addr += PAGESIZE)
	{
	  /* Allocate a page.  */
	  struct vg_object *page = NULL;

	  if (ph->p_paddr + ph->p_memsz < addr + PAGESIZE)
	    /* We have less than a page of data to process.  Another
	       section could have written data to the end of this
	       page.  See if such a page has already been
	       allocated.  */
	    page = lookup (addr);

	  if (! page)
	    page = alloc (addr, ro);

	  if (addr < ph->p_paddr + ph->p_filesz)
	    memcpy ((void *) page,
		    (void *) start + ph->p_offset + (addr - ph->p_paddr),
		    MIN (PAGESIZE, ph->p_paddr + ph->p_filesz - addr));
	}
    }

  if (entry)
    *entry = elf->e_entry;

  return true;
}

