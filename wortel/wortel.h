/* wortel.h - Generic definitions.
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

#include <hurd/cap.h>

#include <l4.h>

#include "output.h"
#include "shutdown.h"
#include "loader.h"


/* The program name.  */
extern char *program_name;

#define BUG_ADDRESS	"<bug-hurd@gnu.org>"


struct wortel_module
{
  const char *name;

  /* Low and high address of the module.  */
  l4_word_t start;
  l4_word_t end;

  /* The command line, in raw, uninterpreted form.  */
  char *args;

  /* The container capability in the physical memory server for this
     module.  Valid for all modules except for the physical memory
     server itself.  */
  hurd_cap_scid_t mem_cont;

  /* The following informartion is only valid if a task will be
     created from the module.  */

  /* The entry point of the executable.  */
  l4_word_t ip;

  /* The task control capability for this module.  Only valid if this
     is not the task server task itself.  */
  hurd_cap_scid_t task_ctrl;

  /* Main thread of the task made from this module.  */
  l4_thread_id_t main_thread;

  /* Server thread of the task made from this module.  */
  l4_thread_id_t server_thread;
};


enum wortel_module_type
  {
    MOD_PHYSMEM = 0,
    MOD_TASK,
    MOD_ROOT_FS,
    MOD_NUMBER
  };


extern const char *mod_names[MOD_NUMBER];

/* For the boot components, find_components() must fill in the start
   and end address of the ELF images in memory.  The end address is
   one more than the last byte in the image.  */
extern struct wortel_module mods[MOD_NUMBER];

extern unsigned int mods_count;

/* Find the module information required for booting (start, end, args).  */
void find_components (void);

int main (int argc, char *argv[]);
