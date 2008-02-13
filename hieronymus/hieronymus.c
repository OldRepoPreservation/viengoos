/* hieronymus.c - initrd implementation.
   Copyright (C) 2008 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

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

#include <hurd/activity.h>
#include <hurd/folio.h>
#include <hurd/storage.h>
#include <hurd/capalloc.h>
#include <hurd/thread.h>
#include <hurd/as.h>
#include <hurd/rm.h>
#include <hurd/ihash.h>
#include <process-spawn.h>

#include <stdio.h>
#include <string.h>

#define STRINGIFY_(id) #id
#define STRINGIFY(id) STRINGIFY_(id)

struct module
{
  const char *name;
  int priority;
  int weight;
  const char *commandline;
  char *start;
  char *end;
};

#include "modules.h"

/* Initialized by the machine-specific startup-code.  */
extern struct hurd_startup_data *__hurd_startup_data;

#define root_activity __hurd_startup_data->activity

/* Allocate a new activity out of our storage.  */
static struct storage
activity_alloc (struct activity_policy policy)
{
  struct storage storage
    = storage_alloc (root_activity, cap_activity_control, STORAGE_LONG_LIVED,
		     ADDR_VOID);
  if (! storage.cap)
    panic ("Failed to allocate storage.");

  struct activity_policy out;
  error_t err = rm_activity_policy (storage.addr,
				    ACTIVITY_POLICY_STORAGE_SET
				    | ACTIVITY_POLICY_CHILD_REL_SET
				    | ACTIVITY_POLICY_SIBLING_REL_SET,
				    policy, &out);
  if (err)
    panic ("Failed to set policy on activity");

  return storage;
}

int
main (int argc, char *argv[])
{
  extern int output_debug;
  output_debug = 3;

  // extern int __pthread_lock_trace;
  // __pthread_lock_trace = 0;

  int count = sizeof (modules) / sizeof (modules[0]);

  struct storage activities[count];
  addr_t thread[count];

  /* Load modules.  */
  int i;
  for (i = 0; i < count; i ++)
    {
      struct activity_memory_policy sibling_policy
	= ACTIVITY_MEMORY_POLICY (modules[i].priority, modules[i].weight);
      struct activity_policy policy
	= ACTIVITY_POLICY (sibling_policy, ACTIVITY_MEMORY_POLICY_VOID, 0);
      activities[i] = activity_alloc (policy);

      const char *argv[] = { modules[i].name, modules[i].commandline, NULL };
      const char *env[] = { NULL };
      thread[i] = process_spawn (activities[i].addr,
				 modules[i].start, modules[i].end,
				 argv, env, false);

      debug (0, "");

      /* Free the memory used by the module's binary.  */
      /* XXX: This doesn't free folios or note pages that may be
	 partially freed.  The latter is important because a page may
	 be used by two modules and after the second module is loaded,
	 it could be freed.  */
      int count = 0;
      int j;
      for (j = 0; j < __hurd_startup_data->desc_count; j ++)
	{
	  struct hurd_object_desc *desc = &__hurd_startup_data->descs[j];

	  if ((desc->type == cap_page || desc->type == cap_rpage)
	      && ! ADDR_IS_VOID (desc->storage)
	      && addr_depth (desc->object) == ADDR_BITS - PAGESIZE_LOG2
	      && (uintptr_t) modules[i].start <= addr_prefix (desc->object)
	      && (addr_prefix (desc->object) + PAGESIZE - 1
		  <= (uintptr_t) modules[i].end))
	    {
	      debug (5, "Freeing " ADDR_FMT "(" ADDR_FMT "), a %s",
		     ADDR_PRINTF (desc->object), ADDR_PRINTF (desc->storage),
		     cap_type_string (desc->type));
	      count ++;
	      storage_free (desc->storage, true);
	    }
	}
      debug (0, "Freed %d pages", count);

      thread_start (thread[i]);
    }

  /* Wait for all activities to die.  */
  for (i = 0; i < count; i ++)
    {
      uintptr_t rt = -1;
      rm_thread_wait_object_destroyed (root_activity,
				       thread[i], &rt);

      addr_t folio = addr_chop (activities[i].addr, FOLIO_OBJECTS_LOG2);
      int index = addr_extract (activities[i].addr, FOLIO_OBJECTS_LOG2);

      error_t err;
      err = rm_folio_object_alloc (ADDR_VOID, folio, index,
				   cap_void, OBJECT_POLICY_VOID,
				   (uintptr_t) rt,
				   ADDR_VOID, ADDR_VOID);
      if (err)
	debug (0, "deallocating object: %d", err);

      debug (0, "%s exited with %d", modules[i].name, (int) rt);
    }

  return 0;
}
