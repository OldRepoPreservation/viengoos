/* laden.h - Generic definitions.
   Copyright (C) 2003, 2008 Free Software Foundation, Inc.
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

#include <string.h>
#include <stdint.h>
#include <assert.h>

#include <l4.h>

#include "output.h"
#include "shutdown.h"
#include "loader.h"


/* The program name.  */
extern char *program_name;

#define BUG_ADDRESS	"<bug-hurd@gnu.org>"


/* Find the kernel, the initial servers and the other information
   required for booting.  */
void find_components (void);

/* Start kernel.  IP is the entry point.  */
void start_kernel (l4_word_t ip);


/* For the rootserver components, find_components() must fill in the
   start and end address of the ELF images in memory.  The end address
   is one more than the address of the last byte in the image.  */
extern l4_rootserver_t kernel;
extern l4_rootserver_t sigma0;
extern l4_rootserver_t sigma1;
extern l4_rootserver_t rootserver;

/* The boot info to be inserted into the L4 KIP.  find_components()
   must provide this information.  */
extern l4_word_t boot_info;

/* Total memory in bytes.  To be filled in by the architecture
   specific code (find_components).  */
extern uint64_t total_memory;


/* The memory map to be provided to the kernel.  */
#define MEMORY_MAP_MAX 200
extern l4_memory_desc_t memory_map[MEMORY_MAP_MAX];
extern l4_word_t memory_map_size;

/* START identifies the start of a region and must be 1k aligned.  END
   is the last byte of the region.  END + 1 must be 1k aligned.  */
#define add_memory_map(start, end, mtype, msubtype)			\
  ({									\
    if (memory_map_size == MEMORY_MAP_MAX)				\
      panic ("No more memory descriptor slots available.\n");		\
    /* Make sure START and END are aligned.  */				\
    assert (((start) & ((1 << 10) - 1)) == 0);				\
    assert (((end) & ((1 << 10) - 1)) == (1 << 10) - 1);		\
    memory_map[memory_map_size].low = (start) >> 10;			\
    memory_map[memory_map_size].high = (end) >> 10;			\
    memory_map[memory_map_size].virtual = 0;				\
    memory_map[memory_map_size].type = (mtype);				\
    memory_map[memory_map_size].subtype = (msubtype);			\
    memory_map_size++;							\
  })


/* Every architecture must provide the following functions.  */

/* Return a help text for this architecture.  */
const char *help_arch (void);

/* Load the system's memory descriptors into MEMDESC and return the
   number of memory descriptors loaded.  NR is the maximum number of
   descriptors to be loaded.  */
int load_mem_info (l4_memory_desc_t memdesc, int nr);


/* The generic code defines these functions.  */

void kip_fixup (void);

int main (int argc, char *argv[]);
