/* wortel-intern.h - Generic definitions.
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <hurd/types.h>

#include <l4.h>

#include "output.h"
#include "shutdown.h"
#include "loader.h"


#define BUG_ADDRESS	"<bug-hurd@gnu.org>"

/* The program name.  */
extern const char program_name[];

/* The region of wortel itself.  */
extern l4_word_t wortel_start;
extern l4_word_t wortel_end;


/* Unused memory.  These fpages mark memory which we needed at some
   time, but don't need anymore.  It can be granted to the physical
   memory server at startup.  This includes architecture dependent
   boot data as well as the physical memory server module.  */
#define MAX_UNUSED_FPAGES 32
extern l4_fpage_t wortel_unused_fpages[MAX_UNUSED_FPAGES];
extern unsigned int wortel_unused_fpages_count;


/* Room for the arguments.  1 KB is a cramped half-screen full, which
   should be more than enough.  Arguments need to be copied here by
   the architecture dependent find_components, so all precious data is
   gathered in the wortel binary region.  */
extern char mods_args[1024];

/* The number of bytes in mods_args already consumed.  */
extern unsigned mods_args_len;

struct wortel_module
{
  const char *name;

  /* Low and high address of the module.  Initialized by
     find_components.  END is the address of the byte following the
     last byte in the image.  */
  l4_word_t start;
  l4_word_t end;

  /* The command line, in raw, uninterpreted form.  This points into
     mods_args.  Initialized by find_components.  */
  char *args;

  /* The task ID to which this module belongs.  */
  hurd_task_id_t task_id;

  /* The container capability in the physical memory server for this
     module.  Not valid for the physical memory server.  Initialized
     after physmem starts up.  */
  hurd_cap_handle_t mem_cont;


  /* The following information is only valid if a task will be created
     from the module.  */

  /* The FPAGE that contains the startup code.  Not valid for the
     physical memory server.  */
  l4_fpage_t startup;

  /* The container capability that contains the startup code.  Only
     valid if startup is valid.  Initialized after physmem starts
     up.  */
  l4_fpage_t startup_cont;

  /* The entry point of the executable.  Initialized just before the
     task is started.  */
  l4_word_t ip;

  /* The task control capability for this module.  Initialized after
     the task server starts up.  */
  hurd_cap_handle_t task_ctrl;

  /* The deva capability for this module (a console device).  */
  hurd_cap_handle_t deva;

  /* Server thread and the initial main thread of the task made from
     this module.  */
  l4_thread_id_t server_thread;

  /* Number of helper threads in the task made from this module.  They
     all follow the SERVER_THREAD numerically in their thread number,
     while they have the same version ID (which is equal to task_id).
     Initialized just before the task is started.  */
  unsigned int nr_extra_threads;
};


enum wortel_module_type
  {
    MOD_PHYSMEM = 0,
    MOD_TASK,
    MOD_DEVA,
    MOD_DEVA_STORE,
    MOD_ROOT_FS,
    MOD_NUMBER
  };


/* Return true if module is associated with its own task.  */
#define MOD_IS_TASK(i) (i != MOD_DEVA_STORE)

/* Printable names of the boot modules.  */
extern const char *mod_names[MOD_NUMBER];

/* The boot modules.  */
extern struct wortel_module mods[MOD_NUMBER];

/* The number of modules present.  Currently, this is enforced to be
   MOD_NUMBER.  */
extern unsigned int mods_count;

/* Initialize up to MOD_NUMBER modules in MODS.  Set MODS_COUNT to the
   number of modules initialized.  */
void find_components (void);

int main (int argc, char *argv[]);
