/* laden.h - Generic definitions.
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

#include <l4.h>


#define PROGRAM_NAME	"laden"
#define BUG_ADDRESS	"<bug-hurd@gnu.org>"


/* Find the kernel, the initial servers and the other information
   required for booting.  */
void find_components (void);

typedef __l4_rootserver_t rootserver_t;

/* For the rootserver components, find_components() must fill in the
   start and end address of the ELF images in memory.  The end address
   is one more than the last byte in the image.  */
extern rootserver_t kernel;
extern rootserver_t sigma0;
extern rootserver_t sigma1;
extern rootserver_t rootserver;

/* The boot info to be inserted into the L4 KIP.  find_components()
   must provide this information.  */
extern l4_word_t boot_info;

/* The memory map to be provided to the kernel.  */
#define MEMORY_MAP_MAX 200
extern struct l4_memory_desc memory_map[MEMORY_MAP_MAX];
extern int memory_map_size;

#define add_memory_map(start,end,mtype,msubtype)				\
  ({									\
    if (memory_map_size == MEMORY_MAP_MAX)				\
      panic ("Error: No more memory descriptor slots available.\n");	\
      memory_map[memory_map_size].low = (start) >> 10;			\
      memory_map[memory_map_size].high = ((end) + (1 << 10) - 1) >> 10;	\
      memory_map[memory_map_size].virtual = 0;				\
      memory_map[memory_map_size].type = (mtype);			\
      memory_map[memory_map_size].subtype = (msubtype);			\
      memory_map_size++;						\
  })


/* Every architecture must define at least one output driver, but might
   define several.  For each output driver, the name and operations on
   the driver must be provided in the following structure.  */

struct output_driver
{
  const char *name;

  /* Initialize the output device.  */
  void (*init) (void);

  /* Deinitialize the output device.  */
  void (*deinit) (void);

  /* Output a character.  */
  void (*putchar) (int chr);
};


/* Every architecture must provide a list of all output drivers,
   terminated by a driver structure which has a null pointer as its
   name.  */
extern struct output_driver *output_drivers[];


/* Every architecture must provide the following functions.  */

/* Return a help text for this architecture.  */
const char *help_arch (void);

/* Reset the machine.  */
void reset (void);

/* Halt the machine.  */
void halt (void);

/* Load the system's memory descriptors into MEMDESC and return the
   number of memory descriptors loaded.  NR is the maximum number of
   descriptors to be loaded.  */
int load_mem_info (l4_memory_desc_t memdesc, int nr);


/* From string.h.  */

int strcmp (const char *s1, const char *s2);

void *memcpy (void *dest, const void *src, int n);

void *memset (void *s, int c, int n);



/* The generic code defines these functions.  */

/* End the program with a failure.  This can halt or reset the
   system.  */
void shutdown (void);

/* Activate the output driver NAME or the default one if NAME is a
   null pointer.  Must be called once at startup, before calling
   putchar or any other output routine.  */
void output_init (char *name);

/* Deactivate the output driver.  Must be called after the last time
   putchar or any other output routine is called, and before control
   is passed on to the L4 kernel.  */
void output_deinit (void);

/* Print the single character CHR on the output device.  */
void putchar (int chr);

void printf (const char *fmt, ...);

/* Print an error message and fail.  */
#define panic(...) ({ printf (__VA_ARGS__); putchar ('\n'); shutdown (); })

/* True if debug mode is enabled.  */
extern int debug;

/* Print a debug message.  */
#define debug(...) do { if (debug) printf (__VA_ARGS__); } while (0)

/* Load the ELF images of the kernel and the initial servers into
   memory, checking for overlaps.  Update the start and end
   information with the information from the ELF program, and fill in
   the entry points.  */
void load_components (void);

void kip_fixup (void);

int main (int argc, char *argv[]);
