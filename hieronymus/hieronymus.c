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

#include <viengoos/activity.h>
#include <viengoos/folio.h>
#include <viengoos/thread.h>
#include <hurd/storage.h>
#include <hurd/capalloc.h>
#include <hurd/as.h>
#include <viengoos/misc.h>
#include <hurd/ihash.h>
#include <process-spawn.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <md5.h>

#define STRINGIFY_(id) #id
#define STRINGIFY(id) STRINGIFY_(id)

struct module
{
  const char *name;
  int priority;
  int weight;
  /* Delay in seconds.  */
  unsigned int delay;
  /* Maximum of 8 arguments.  */
  const char *args[8];
  char *start;
  char *end;

  unsigned char md5sum[16];
};

#include "modules.h"

static int module_count;
static addr_t *activities;

/* Initialized by the machine-specific startup-code.  */
extern struct hurd_startup_data *__hurd_startup_data;

#define root_activity __hurd_startup_data->activity

/* Allocate a new activity out of our storage.  */
static struct storage
activity_alloc (struct activity_policy policy)
{
  struct storage storage
    = storage_alloc (root_activity, cap_activity_control, STORAGE_LONG_LIVED,
		     OBJECT_POLICY_DEFAULT, ADDR_VOID);
  if (! storage.cap)
    panic ("Failed to allocate storage.");

  struct activity_policy out;
  error_t err = rm_activity_policy (ADDR_VOID, storage.addr,
				    ACTIVITY_POLICY_STORAGE_SET
				    | ACTIVITY_POLICY_CHILD_REL_SET
				    | ACTIVITY_POLICY_SIBLING_REL_SET,
				    policy, &out);
  if (err)
    panic ("Failed to set policy on activity");

  return storage;
}

static bool all_done;

static inline uint64_t
now (void)
{
  struct timeval t;
  struct timezone tz;

  if (gettimeofday( &t, &tz ) == -1)
    return 0;
  return (t.tv_sec * 1000000ULL + t.tv_usec);
}
static uint64_t epoch;

struct stat
{
  int available;
  int alloced;
  uint64_t time;
};
static int stats_count;
static struct stat *stats;

static void *
do_gather_stats (void *arg)
{
  int size = 0;

  int period = 0;

  while (! all_done)
    {
      struct activity_info info;

      if (size == stats_count)
	{
	  if (stats_count == 0)
	    size = 100;
	  else
	    size *= 2;

	  stats = realloc (stats,
			   sizeof (struct stat) * size * module_count);
	}

      struct stat *stat = &stats[stats_count * module_count];
      stat->time = now ();

      int n = 0;

      int i;
      for (i = 0; i < module_count; i ++, stat ++)
	{
	  error_t err;
	  err = rm_activity_info (ADDR_VOID, activities[i], activity_info_stats,
				  period, &info);
	  assert_perror (err);
	  assert (info.event == activity_info_stats);
	  assert (info.stats.count > 0);
	  if (err)
	    {
	      stat->alloced = 0;
	      stat->available = 0;
	    }
	  else
	    {
	      stat->alloced = info.stats.stats[0].clean
		+ info.stats.stats[0].dirty
		+ info.stats.stats[0].pending_eviction;
	      stat->available = info.stats.stats[0].available;


	      if (n == 0)
		n = info.stats.stats[0].period + 1;
	    }
	}

      stats_count ++;
      period = n;
    }

  return NULL;
}

int
main (int argc, char *argv[])
{
  extern int output_debug;
  output_debug = 3;

  epoch = now ();

  module_count = sizeof (modules) / sizeof (modules[0]);

  addr_t a[module_count];
  activities = &a[0];

  /* Create the activities.  */
  int i;
  for (i = 0; i < module_count; i ++)
    {
      struct activity_memory_policy sibling_policy
	= ACTIVITY_MEMORY_POLICY (modules[i].priority, modules[i].weight);
      struct activity_policy policy
	= ACTIVITY_POLICY (sibling_policy, ACTIVITY_MEMORY_POLICY_VOID, 0);
      activities[i] = activity_alloc (policy).addr;

      struct object_name name;
      strncpy (&name.name[0], modules[i].name, sizeof (name.name));
      rm_object_name (ADDR_VOID, activities[i], name);
    }

  bool gather_stats = false;
  pthread_t gather_stats_tid;

  /* Parse the arguments.  */
  for (i = 1; i < argc; i ++)
    {
      if (strcmp (argv[i], "--stats") == 0)
	{
	  if (! gather_stats)
	    {
	      error_t err;
	      err = pthread_create (&gather_stats_tid, NULL,
				    do_gather_stats, NULL);
	      assert_perror (err);
	      gather_stats = true;
	    }
	}
    }

  /* Load each program (but don't yet start it).  */
  addr_t thread[module_count];
  for (i = 0; i < module_count; i ++)
    {
      struct md5_ctx ctx;
      unsigned char result[16];

      md5_init_ctx (&ctx);
      md5_process_bytes (modules[i].start,
			 modules[i].end - modules[i].start,
			 &ctx);
      md5_finish_ctx (&ctx, result);

      if (memcmp (result, modules[i].md5sum, 16) != 0)
	{
	  int j;
	  printf ("Expected md5 hash: ");
	  for (j = 0; j < 16; j ++)
	    printf ("%x%x", modules[i].md5sum[j] & 0xf,
		    modules[i].md5sum[j] >> 4);

	  printf ("\nGot: ");
	  for (j = 0; j < 16; j ++)
	    printf ("%x%x", result[j] & 0xf, result[j] >> 4);
	  printf ("\n");

	  panic ("Binary %s corrupted!", modules[i].name);
	}

      const char *argv[1 + (sizeof (modules[i].args)
			    / sizeof (modules[i].args[0]))
		       + 1];
      argv[0] = modules[i].name;
      int j;
      for (j = 0;
	   j < (sizeof (modules[i].args) / sizeof (modules[i].args[0]))
	     && modules[i].args[j];
	   j ++)
	argv[1 + j] = modules[i].args[j];
      argv[1 + j] = NULL;

      const char *env[] = { NULL };
      thread[i] = process_spawn (activities[i],
				 modules[i].start, modules[i].end,
				 argv, env, false);
    }

  /* Free the memory used by the binaries.  XXX: Also free the folios
     that are completely unused.  */
  int j;
  for (j = 0; j < __hurd_startup_data->desc_count; j ++)
    {
      struct hurd_object_desc *desc = &__hurd_startup_data->descs[j];

      if ((desc->type == cap_page || desc->type == cap_rpage)
	  && ! ADDR_IS_VOID (desc->storage)
	  && addr_depth (desc->object) == ADDR_BITS - PAGESIZE_LOG2)
	{
	  int i;
	  for (i = 0; i < module_count; i ++)
	    if ((uintptr_t) modules[i].start <= addr_prefix (desc->object)
		&& (addr_prefix (desc->object) + PAGESIZE - 1
		    <= (uintptr_t) modules[i].end))
	      break;

	  if (i != module_count)
	    {
	      debug (5, "Freeing " ADDR_FMT "(" ADDR_FMT "), a %s",
		     ADDR_PRINTF (desc->object), ADDR_PRINTF (desc->storage),
		     cap_type_string (desc->type));
	      storage_free (desc->storage, true);
	    }
	}
    }

  /* Start the modules.  */
  int started = 0;
  while (started < module_count)
    {
      l4_uint64_t start = now ();

      uint64_t deadline = -1ULL;
      const char *next = NULL;

      for (i = 0; i < module_count; i ++)
	{
	  if (modules[i].delay != -1U)
	    {
	      debug (0, "%s: %ds delayed start, starting in %d s",
		     modules[i].name, modules[i].delay,
		     modules[i].delay - (int) ((start - epoch) / 1000000ULL));

	      if (modules[i].delay * 1000000ULL < start - epoch)
		{
		  started ++;
		  modules[i].delay = -1U;

		  debug (0, DEBUG_BOLD ("Starting %s"), modules[i].name);
		  thread_start (thread[i]);
		}
	      else if (deadline > modules[i].delay * 1000000ULL)
		{
		  deadline = modules[i].delay * 1000000ULL;
		  next = modules[i].name;
		}
	    }
	}

      if (started < module_count)
	{
	  debug (0, "Waiting %llu seconds before starting %s",
		 (deadline - (start - epoch)) / 1000000,
		 next);
	  l4_sleep (l4_time_period (deadline - (start - epoch)));
	}
    }


  /* Wait for all activities to die.  */
  for (i = 0; i < module_count; i ++)
    {
      uintptr_t rt = -1;
      rm_object_reply_on_destruction (root_activity,
				      thread[i], &rt);

      addr_t folio = addr_chop (activities[i], FOLIO_OBJECTS_LOG2);
      int index = addr_extract (activities[i], FOLIO_OBJECTS_LOG2);

      error_t err;
      err = rm_folio_object_alloc (ADDR_VOID, folio, index,
				   cap_void, OBJECT_POLICY_VOID,
				   (uintptr_t) rt,
				   NULL, NULL);
      if (err)
	debug (0, "deallocating object: %d", err);

      debug (0, "%s exited with %d", modules[i].name, (int) rt);
    }

  if (gather_stats)
    {
      uint64_t n = now ();

      all_done = true;
      void *status;
      pthread_join (gather_stats_tid, &status);

      printf ("Total time: %lld.%lld\n",
	      (n - epoch) / 1000000,
	      ((n - epoch) / 100000) % 10);

      for (i = 0; i < stats_count; i ++)
	{
	  int j;

	  struct stat *stat = &stats[i * module_count];
	  printf ("%lld.%lld",
		  stat->time / 1000000,
		  (stat->time / 100000) % 10);
		  
	  for (j = 0; j < module_count; j ++, stat ++)
	    printf ("\t%d\t%d", stat->available, stat->alloced);
	  printf ("\n");
	}
    }

  return 0;
}
