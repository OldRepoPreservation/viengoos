/* profile.c - Profiling support implementation.
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

#ifdef RM_INTERN
# include "../viengoos/zalloc.h"
#else
# include <stdlib.h>
#endif

#include "profile.h"
#include <hurd/ihash.h>
#include <stddef.h>
#include <string.h>

#ifdef __gnu_hurd_viengoos__
# ifdef USE_L4
#  include <l4.h>
# endif
# include <viengoos/misc.h>
# include <s-printf.h>
#else
# include <pthread.h>
# include <sys/time.h>
# ifndef s_printf
#  define s_printf printf
# endif
# ifndef PAGESIZE
/* We only use this to determine a buffer size.  It needs to be a
   constant.  */
#  define PAGESIZE 4096
# endif
# ifndef panic
#  define panic(fmt, ...)						\
  do									\
    {									\
      printf ("%s:%d: " fmt "\n", __func__, __LINE__ , ##__VA_ARGS__);	\
      abort ();								\
    }									\
  while (0)
# endif
#include <stdio.h>
# ifndef do_debug
#  define do_debug(x) if (0)
# endif
#endif

static inline uint64_t
now (void)
{
#ifdef __gnu_hurd_viengoos__
# ifdef USE_L4
  return l4_system_clock ();
# else
#  warning Not ported to this platform.
  return 0;
# endif
#else
  struct timeval t;
  struct timezone tz;

  if (gettimeofday (&t, &tz) == -1)
    return 0;
  return (t.tv_sec * 1000000ULL + t.tv_usec);
#endif
}

#define CALL_SITES 100
struct call_site
{
  const char *name;
  const char *name2;
  uint64_t time;
  uint64_t start;
  int calls;
  int nested_calls;
  int pending;
};

/* The number of threads we support.  */
#define THREADS 4
static int thread_count;

struct profile_block
{
#ifdef __gnu_hurd_viengoos__
# ifdef USE_L4
#  define MYSELF() l4_myself ()
# else
#  warning Profile code broken.
#  define MYSELF() 0
# endif
  vg_thread_id_t tid;
#else
#define MYSELF() pthread_self ()
  pthread_t tid;
#endif

  /* When we started profiling.  */
  uint64_t epoch;
  uint64_t profiled_time;

  uint64_t calls;
  /* Number of extant profiling calls.  We only update profiled_time
     if EXTANT is 0.  The result is that the time spent profiling is
     correct, and the percent of the time profile that a function has
     been is more meaningful.  */
  int extant;

  int init_done;

  struct call_site call_sites[CALL_SITES];
  int call_sites_used;

  struct hurd_ihash sites_hash;
#ifndef RM_INTERN
  char sites_hash_buffer[PAGESIZE];
#endif
};

static struct profile_block profile_blocks[THREADS];

/* Return the profile block associated with the calling thread.  If
   there is none, initialize one.  */
static struct profile_block *
profile_block (void)
{
  int i;
  for (i = 0; i < thread_count; i ++)
    if (profile_blocks[i].tid == MYSELF())
      return &profile_blocks[i];

  /* New thread.  */

  i = __sync_fetch_and_add (&thread_count, 1);
  if (i >= THREADS)
    panic ("More threads than profile space available!");

  struct profile_block *pb = &profile_blocks[i];

  pb->tid = MYSELF ();

  size_t size;
  void *buffer;
#ifdef RM_INTERN
  size = hurd_ihash_buffer_size (CALL_SITES, false, 0);
  /* Round up to a multiple of the page size.  */
  size = (size + PAGESIZE - 1) & ~(PAGESIZE - 1);

  buffer = (void *) zalloc (size);
#else
  size = sizeof (pb->sites_hash_buffer);
  buffer = pb->sites_hash_buffer;
#endif
  if (! buffer)
    panic ("Failed to allocate memory for object hash!\n");

  memset (buffer, 0, size);

  hurd_ihash_init_with_buffer (&pb->sites_hash, false,
			       HURD_IHASH_NO_LOCP,
			       buffer, size);

  pb->epoch = now ();
  pb->init_done = true;

  return pb;
}

/* XXX: This implementation assumes that we are single threaded!  In
   the case of Viengoos, this is essentially true: all relevant
   functionality is with the kernel lock held.  We also don't
   currently support nested calls.  */

#undef profile_stats_dump
void
profile_stats_dump (void)
{
  uint64_t n = now ();

  char digit_whitespace[11];
  memset (digit_whitespace, ' ', sizeof (digit_whitespace));
  digit_whitespace[sizeof (digit_whitespace) - 1] = 0;

  char *dws (uint64_t number)
  {
    int w = 0;
    if (number < 0)
      w ++;

    while (number != 0)
      {
	w ++;
	number /= 10;
      }
    if (w == 0)
      w = 1;

    if (w > sizeof (digit_whitespace))
      return "";
    else
      return &digit_whitespace[w];
  }

  int t;
  for (t = 0; t < thread_count; t ++)
    {
      struct profile_block *pb = &profile_blocks[t];
      if (! pb->init_done)
	continue;

      int width = 0;
      int count = 0;

      int i;
      for (i = 0; i < pb->call_sites_used; i ++)
	{
	  if (pb->call_sites[i].calls)
	    count ++;
	  if (pb->call_sites[i].calls)
	    {
	      int w = 0;
	      if (pb->call_sites[i].name)
		w += strlen (pb->call_sites[i].name);
	      if (pb->call_sites[i].name2)
		w += strlen (pb->call_sites[i].name2);
	      if (pb->call_sites[i].name && pb->call_sites[i].name2)
		w += 2;
	      if (w > width)
		width = w;
	    }
	}

      char spaces[width + 1];
      memset (spaces, ' ', sizeof (spaces));
      spaces[width] = 0;

      /* Do a simple bubble sort.  */
      int order[count];

      int j = 0;
      for (i = 0; i < pb->call_sites_used; i ++)
	if (pb->call_sites[i].calls)
	  order[j ++] = i;

      for (i = 0; i < count; i ++)
	for (j = 0; j < count - 1; j ++)
	  if (pb->call_sites[order[j]].time
	      < pb->call_sites[order[j + 1]].time)
	    {
	      int t = order[j];
	      order[j] = order[j + 1];
	      order[j + 1] = t;
	    }

      s_printf ("Thread: %x\n", pb->tid);
      for (j = 0; j < count; j ++)
	{
	  i = order[j];
	  if (pb->call_sites[i].calls)
	    s_printf ("%s%s%s%s%s: %s%d calls (%d nested),\t%s%lld ms,"
		      "\t%lld.%d us/call,\t%d%% total,\t%d%% profiled\n",
		      &spaces[strlen (pb->call_sites[i].name ?: "")
			      + strlen (pb->call_sites[i].name2 ?: "")
			      + (pb->call_sites[i].name
				 && pb->call_sites[i].name2 ? 2 : 0)],
		      pb->call_sites[i].name ?: "",
		      pb->call_sites[i].name && pb->call_sites[i].name2
		      ? "(" : "",
		      pb->call_sites[i].name2 ?: "",
		      pb->call_sites[i].name && pb->call_sites[i].name2
		      ? ")" : "",
		      dws (pb->call_sites[i].calls), pb->call_sites[i].calls,
		      pb->call_sites[i].nested_calls,
		      dws (pb->call_sites[i].time / 1000),
		      pb->call_sites[i].time / 1000,
		      pb->call_sites[i].time / pb->call_sites[i].calls,
		      (int) ((10 * pb->call_sites[i].time)
			     / pb->call_sites[i].calls) % 10,
		      (int) ((100 * pb->call_sites[i].time) / (n - pb->epoch)),
		      (int) ((100 * pb->call_sites[i].time)
			     / pb->profiled_time));
	}

      s_printf ("profiled time: %lld ms (%d%%), calls: %lld\n",
		pb->profiled_time / 1000,
		(int) ((100 * pb->profiled_time) / (n - pb->epoch)),
		pb->calls);
      s_printf ("total time: %lld ms\n", (n - pb->epoch) / 1000);
    }
}

#undef profile_start
void
profile_start (uintptr_t id, const char *name, const char *name2)
{
  struct profile_block *pb = profile_block ();
  if (! pb->init_done)
    return;

  struct call_site *site = hurd_ihash_find (&pb->sites_hash, id);
  if (! site)
    {
      site = &pb->call_sites[pb->call_sites_used ++];
      if (pb->call_sites_used == CALL_SITES)
	panic ("Out of profile space.");

      error_t err = hurd_ihash_add (&pb->sites_hash, id, site);
      if (err)
	panic ("Failed to add to hash: %d.", err);

      if (name)
	site->name = name;
      else
	{
#ifdef __gnu_hurd_viengoos__
	  site->name = vg_method_id_string (id);
#else
	  site->name = "unknown";
#endif
	}
      site->name2 = name2;
    }

  pb->extant ++;

  site->pending ++;
  if (site->pending == 1)
    site->start = now ();
}

#undef profile_end
void
profile_end (uintptr_t id)
{
  uint64_t n = now ();

  struct profile_block *pb = profile_block ();
  if (! pb->init_done)
    return;

  struct call_site *site = hurd_ihash_find (&pb->sites_hash, id);
  if (! site)
    panic ("profile_end called without corresponding profile_begin (%p)!",
	   (void *) id);

  if (! site->pending)
    panic ("profile_end called but no extant profile_start!");

  pb->extant --;

  site->pending --;
  if (site->pending == 0)
    {
      site->time += n - site->start;

      if (pb->extant == 0)
	pb->profiled_time += n - site->start;

      site->calls ++;
      pb->calls ++;

      do_debug (5)
	if (pb->calls % 100000 == 0)
	  profile_stats_dump ();
    }
  else
    site->nested_calls ++;
}

