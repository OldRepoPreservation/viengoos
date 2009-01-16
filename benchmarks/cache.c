/* cache.c - A cache manager benchmark.
   Copyright (C) 2008 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

   This file is part of the GNU Hurd.

   GNU Hurd is free software: you can redistribute it and/or modify it
   under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   GNU Hurd is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with GNU Hurd.  If not, see
   <http://www.gnu.org/licenses/>.  */


/* This benchmark manages a cache of objects and queries the cache in
   a tight loop.  The queries are distributed according to a Zipf
   distribution (i.e., the object that is accessed second most often
   is accessed half as often as the object that is accessed most
   often, the object that is access third most often is accessed one
   third as often as the object that is accessed most often, etc.).
   There are a few parameters that influence the benchmark's behavior
   and, in particular, how the cache manages data.  They are described
   below.  */

#include <stdbool.h>

/* Complication parameters.  */

/* Whether to use a cache.  */
#define USE_CACHE 1
/* Number of lines in the cache.  If 0, unlimited.  Depends on
   USE_CACHE.  */
#define CACHE_LINES 320

/* Whether to use discardable memory (if so, CACHE_LINES must not be
   defined).  Depends on USE_CACHE.  Overrides CACHE_LINES.  This
   currently only works on Viengoos.  */
#define USE_DISCARDABLE 1

/* Define to the object size.  Must be at least 64 bytes.  */
#define OBJECT_SIZE (1024 * 1024)
// #define OBJECT_SIZE 128

/* Number of objects in database.  */
#define OBJECTS (1000)

/* Number of object lookups.  */
#define ACCESSES (25 * 1000)

/* Zipf distribution's alpha parameter.  */
#define ALPHA 1.01

/* Whether to enable debugging output.  */
// #define DEBUG

bool have_a_hog = false;

#ifndef __gnu_hurd_viengoos__
# define s_printf printf
#endif

#if !defined(__gnu_hurd_viengoos__) && defined(USE_DISCARDABLE)
# undef USE_DISCARDABLE
# define USE_DISCARDABLE 0
#endif

#if CACHE_LINES
# if !USE_CACHE
#  undef CACHE_LINES
# endif
#endif

#if USE_DISCARDABLE
# if !USE_CACHE
#  undef USE_DISCARDABLE
# endif
#endif

#if USE_DISCARDABLE && CACHE_LINES
# undef CACHE_LINES
#endif

#ifndef OBJECT_SIZE
# error OBJECT_SIZE not define.
#else
# if OBJECT_SIZE < 64
#  error OBJECT_SIZE too small.
# endif
#endif

#if USE_CACHE && !USE_DISCARDABLE
int cache_lines = CACHE_LINES;
#endif

#ifndef debug
# ifdef DEBUG
static int level;
#  define do_debug(lvl) if ((lvl) < level)
# else
#  define do_debug(lvl) if (0)
# endif
# define debug(lvl, fmt, ...)						\
  do_debug (lvl)							\
    printf ("%s:%d: " fmt "\n", __func__, __LINE__, ##__VA_ARGS__)
#endif

#ifndef PAGESIZE
# define PAGESIZE getpagesize ()
#endif

#ifndef DEBUG_BOLD
# define DEBUG_BOLD(text) "\033[01;31m" text "\033[00m"
#endif

#include <fcntl.h>
#include <sqlite3.h>
#include <hurd/ihash.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <malloc.h>
#include <pthread.h>
#include <profile.h>

#include "zipf.h"

#if USE_DISCARDABLE
# include <hurd/anonymous.h>
#endif

static inline uint64_t
now (void)
{
  struct timeval t;
  struct timezone tz;

  if (gettimeofday (&t, &tz) == -1)
    return 0;
  return (t.tv_sec * 1000000ULL + t.tv_usec);
}
uint64_t epoch;

#define STATS 5000

struct stats
{
  int alloced[2];
  int available[2];
  int heap;
  int hog;
  uint64_t time;
  int period;
  int iter;
  int hits;
  int misses;
} stats[STATS];
int stat_count;

static uint64_t done;

static int iter;
static int hits;
static int misses;

#if USE_CACHE
static struct hurd_ihash cache;
#endif

#ifdef __gnu_hurd_viengoos__
#include <hurd/storage.h>
#include <viengoos/cap.h>
#include <viengoos/activity.h>
#include <pthread.h>
#include <hurd/anonymous.h>
#include <hurd/as.h>
#include <string.h>

vg_addr_t main_activity;
vg_addr_t hog_activity;
#endif

void *
helper (void *arg)
{
#ifdef __gnu_hurd_viengoos__
  pthread_setactivity_np (hog_activity);

  struct vg_activity_info info;
#endif

  int hog_alloced = 0;

  printf ("Gathering stats...\n");

  void wait_read_stats (void)
  {
#ifdef __gnu_hurd_viengoos__
    /* First the main thread.  */
    error_t err;

    err = vg_activity_info (VG_ADDR_VOID, main_activity, vg_activity_info_stats,
			    stat_count == 0
			    ? 0 : stats[stat_count - 1].period + 1,
			    &info);
    assert_perror (err);
    assert (info.event == vg_activity_info_stats);
    assert (info.stats.count > 0);

    stats[stat_count].alloced[0]
      = info.stats.stats[0].clean + info.stats.stats[0].dirty
      + info.stats.stats[0].pending_eviction;
    stats[stat_count].available[0] = info.stats.stats[0].available;

    stats[stat_count].period = info.stats.stats[0].period;

    /* Then, the hog.  */
    err = vg_activity_info (VG_ADDR_VOID, hog_activity, vg_activity_info_stats,
			    stat_count == 0
			    ? 0 : stats[stat_count - 1].period + 1,
			    &info);
    assert_perror (err);
    assert (info.event == vg_activity_info_stats);
    assert (info.stats.count > 0);

    stats[stat_count].alloced[1]
      = info.stats.stats[0].clean + info.stats.stats[0].dirty
      + info.stats.stats[0].pending_eviction;
    stats[stat_count].available[1] = info.stats.stats[0].available;
#else
    if (stat_count > 0)
      sleep (2);

    stats[stat_count].period = stat_count;

    stats[stat_count].alloced[1] = 0;
    stats[stat_count].available[1] = 0;
    stats[stat_count].alloced[1] = 0;
    stats[stat_count].available[1] = 0;
#endif

    /* The amount of heap memory.  */
    struct mallinfo info = mallinfo ();
    stats[stat_count].heap = info.hblkhd;
    stats[stat_count].hog = hog_alloced;

    stats[stat_count].time = now () - epoch;
    stats[stat_count].iter = iter;
    stats[stat_count].hits = hits;
    stats[stat_count].misses = misses;

    stat_count ++;
    if (stat_count % 10 == 0)
      printf ("Period %d: %d; heap: %d; hog: %d\n",
	     stat_count, iter,
	     stats[stat_count - 1].heap, 
	     stats[stat_count - 1].hog);
  }

#define MEM_PER_SEC (5 * 1024 * 1024)
  if (have_a_hog)
    /* Now, allocate MEM_PER_SEC bytes of memory per second until
       we've allocate half the initially available memory.  */
    {
      /* Wait 60 seconds before starting.  */
      uint64_t start = now ();
      while (now () - start < 60 * 1000 * 1000 && ! done)
	wait_read_stats ();

#ifdef __gnu_hurd_viengoos__
      int available = stats[1].available[0] * PAGESIZE;
#else
      int available = 512 * 1024 * 1024;
#endif

      debug (1, DEBUG_BOLD ("mem hog starting (avail: %d)!"), available);

      /* We place the allocated buffers on a list.  */
      struct buffer
      {
	struct buffer *next;
	int size;
      };

      struct buffer *buffers = NULL;

      start = now ();
      int remaining = available / 2;
      while (remaining > 0 && ! done)
	{
	  /* Allocate MEM_PER_SEC * seconds since last alloc.  */
	  uint64_t n = now ();
	  uint64_t delta = n - start;

	  int size = (MEM_PER_SEC * delta) / 1000 / 1000;
	  /* Round down to a multiple of the page size.  */
	  if (size > remaining)
	    size = remaining;

	  size &= ~(PAGESIZE - 1);
	  if (size == 0)
	    size = PAGESIZE;

	  remaining -= size;

	  debug (0, "%lld.%lld seconds since last alloc.  Hog now allocating %d kb (so far: %d kb; %d kb remaining)",
		 delta / 1000 / 1000, (delta / 1000 / 100) % 10,
		 size / 1024, hog_alloced / 1024, remaining / 1024);

	  start = n;

	  struct buffer *buffer;
	  buffer = mmap (NULL, size, PROT_READ|PROT_WRITE,
			 MAP_PRIVATE|MAP_ANON, -1, 0);
	  if (buffer == MAP_FAILED)
	    {
	      printf ("Failed to allocate memory: %m\n");
	      abort ();
	    }

	  /* Write to it.  */
	  madvise (buffer, size, MADV_NORMAL);
	  int i;
	  for (i = 0; i < size; i += PAGESIZE)
	    *(int *) (buffer + i) = 0;

	  buffer->next = buffers;
	  buffers = buffer;

	  buffer->size = size;

	  hog_alloced += size;

#ifndef __gnu_hurd_viengoos__
	  if (mlock (buffer, size) == -1)
	    {
	      printf ("Failed to lock memory: %m; are you root?\n");
	      abort ();
	    }
#endif

	  wait_read_stats ();
	}

      /* Wait 60 seconds before freeing.  */
      start = now ();
      while (now () - start < 60 * 1000 * 1000 && ! done)
	wait_read_stats ();

      debug (1, DEBUG_BOLD ("mem hog releasing memory!"));

      /* Release the memory.  */
      while (buffers && ! done)
	{
	  wait_read_stats ();
      
	  struct buffer *buffer = buffers;
	  buffers = buffer->next;

	  hog_alloced -= buffer->size;

	  munmap (buffer, buffer->size);
	}
    }

  /* Finally, wait until the main thread is done.  */
  while (! done)
    wait_read_stats ();

  return 0;
}

pthread_t helper_tid;

void
helper_fork (void)
{
#ifdef __gnu_hurd_viengoos__
  int err;

  main_activity = storage_alloc (VG_ADDR_VOID,
				 vg_cap_activity_control, STORAGE_LONG_LIVED,
				 VG_OBJECT_POLICY_DEFAULT, VG_ADDR_VOID).addr;
  if (VG_ADDR_IS_VOID (main_activity))
    panic ("Failed to allocate main activity");

  struct vg_object_name name;
  snprintf (&name.name[0], sizeof (name.name), "main.%x", hurd_myself ());
  vg_object_name (VG_ADDR_VOID, main_activity, name);

  hog_activity = storage_alloc (VG_ADDR_VOID,
				vg_cap_activity_control, STORAGE_LONG_LIVED,
				VG_OBJECT_POLICY_DEFAULT, VG_ADDR_VOID).addr;
  if (VG_ADDR_IS_VOID (hog_activity))
    panic ("Failed to allocate hog activity");

  snprintf (&name.name[0], sizeof (name.name), "hog.%x", hurd_myself ());
  vg_object_name (VG_ADDR_VOID, hog_activity, name);

  /* We give the main thread and the hog the same priority and
     weight.  */  
  struct vg_activity_policy in, out;
  memset (&in, 0, sizeof (in));
  in.sibling_rel.priority = 1;
  in.sibling_rel.weight = 10;

  in.child_rel.priority = 2;
  in.child_rel.weight = 20;

  err = vg_activity_policy (VG_ADDR_VOID, meta_data_activity,
			    VG_ACTIVITY_POLICY_CHILD_REL_SET, in, &out);
  assert (err == 0);

  err = vg_activity_policy (VG_ADDR_VOID, hog_activity,
			    VG_ACTIVITY_POLICY_SIBLING_REL_SET, in, &out);
  assert (err == 0);

  err = vg_activity_policy (VG_ADDR_VOID, main_activity,
			    VG_ACTIVITY_POLICY_SIBLING_REL_SET, in, &out);
  assert (err == 0);


  pthread_setactivity_np (main_activity);
#endif

  pthread_create (&helper_tid, NULL, helper, NULL);
}

static sqlite3 *db;

#define ROWS 1024
// #define ROWS 1

static int
sqlite3_exec_printf (sqlite3 *sqlite, 
		     const char *fmt, 
		     int (*callback)(void*,int,char**,char**),
		     void *arg, char **errmsg,
		     ...)
{
  va_list ap;

  va_start (ap, errmsg);
  char *sql = sqlite3_vmprintf (fmt, ap);
  va_end (ap);

  int ret = sqlite3_exec (sqlite, sql, callback, arg, errmsg);

  sqlite3_free (sql);

  return ret;
}

static void
db_init ()
{
  int res = sqlite3_open (NULL, &db);
  if (res)
    {
      printf ("Error opening db: %d: %s\n", res, sqlite3_errmsg (db));
      exit (1);
    }

  char *err = NULL;
  sqlite3_exec (db,
		"create temp table db1 (key STRING, val INT);"
		"create temp table db2 (key STRING, val STRING);"
		"create temp table db3 (key STRING, val INT);",
		NULL, NULL, &err);
  if (err)
    {
      printf ("%s:%d: %s\n", __func__, __LINE__, err);
      exit (1);
    }

  int i;
  for (i = 0; i < ROWS; i ++)
    {
      sqlite3_exec_printf (db,
			   "insert into db1 (key, val) values ('%d', %d);"
			   "insert into db2 (key, val) values ('%d', '%d');"
			   "insert into db3 (key, val) values ('%d', %d);",
			   NULL, NULL, &err,
			   i, i, i, i, i, i * i);
      if (err)
	{
	  printf ("%s:%d: %s\n", __func__, __LINE__, err);
	  exit (1);
	}
    }
}

struct obj
{
  int id;
  int page;
  int id2;

#if USE_DISCARDABLE
# if OBJECT_SIZE < PAGESIZE
#  define HAVE_LIVE
  /* If 0, the data was discarded.  */
  bool live;
# endif
#endif

#if CACHE_LINES
  struct obj *next;
  struct obj *prev;
#endif

  int a;
  int b;
  char padding[];
};

#if !USE_DISCARDABLE
/* Allocating and deallocating the same sized objects is surprisingly
   expensive, in particular when the memory is released back to the OS
   and any VM data structures must be ripped down and then recreated.
   To avoid this, we maintain a list of unused objects.  (We reuse the
   first pointer of the object as a next pointer.)  */
struct freeobject
{
  union
  {
    struct freeobject *next;
    struct obj object;
  };
};
struct freeobject *objects;
#endif

static void
object_free (struct obj *object)
{
  debug (5, "Freeing %d", object->id);

#if USE_DISCARDABLE
  abort ();
#else
  /* Add to free list.  */
  ((struct freeobject *) object)->next = objects;
  objects = (struct freeobject *) object;
#endif
}

static void
object_read (struct obj *object, int id)
{
  uint64_t start = now ();

#if USE_DISCARDABLE
# if OBJECT_SIZE > PAGESIZE
  profile_region ("advise");

  madvise ((void *) ((uintptr_t) object & ~(PAGESIZE - 1)),
	   (OBJECT_SIZE + PAGESIZE - 1) & ~(PAGESIZE - 1),
	   MADV_NORMAL);

  profile_region_end ();
# endif
#endif

  int calls = 0;
  int callback (void *cookie, int argc, char **argv, char **names)
    {
      if (calls == 0)
	object->a = atoi (argv[0]);
      else
	object->b = atoi (argv[0]);
      calls ++;

      return 0;
    }

  profile_region ("query");

  char *err = NULL;
  sqlite3_exec_printf (db,
		       "select val from db1 where key = %d;"
		       "select db3.val from db2 join db3 on db2.val = db3.key"
		       " where db2.val = %d;",
		       callback, NULL, &err,
		       (id - 1) % ROWS, (id / ROWS) % ROWS);
  if (err)
    {
      printf ("%s:%d: %s\n", __func__, __LINE__, err);
      exit (1);
    }

  assert (calls == 2);
  profile_region_end ();

  profile_region ("clear");

  int *i;
  for (i = &object->id;
       (void *) i < (void *) object + OBJECT_SIZE;
       i = (void *) i + PAGESIZE)
    {
      i[0] = id;
      i[1] = ((uintptr_t) i - (uintptr_t) &object->id) / PAGESIZE;
      i[2] = id;
    }

#ifdef HAVE_LIVE
  object->live = 1;
#endif


  profile_region_end ();

  debug (5, "Reading object %d: %lld ms", id, (now () - start) / 1000);
}

#if USE_DISCARDABLE
# if OBJECT_SIZE > PAGESIZE
bool
object_fill (struct anonymous_pager *anon,
	     uintptr_t offset, uintptr_t count,
	     void *pages[],
	     struct vg_activation_fault_info info)
{
  profile_region (NULL);

  /* We noted a hit in object_lookup but it is really a miss.  Adjust
     accordingly.  */
  hits --;
  misses ++;

  int id = (int) anon->cookie;

  if (id <= 0 || id > OBJECTS)
    {
      s_printf ("id %d out of range!\n", id);
      abort ();
    }

  struct obj *object = (struct obj *) (uintptr_t) vg_addr_prefix (anon->map_area);

  // debug (0, "Filling %d at %p", id, object);

  object_read (object, id);

  profile_region_end ();

  return true;
}
# endif
#endif

static struct obj *
object_lookup_hard (int id)
{
  struct obj *object;

  assert (id > 0);
  assert (id <= OBJECT_SIZE);

  profile_region (NULL);

#if USE_DISCARDABLE
# if OBJECT_SIZE <= PAGESIZE
  /* We put multiple objects on a single page.  A single object
     never straddles multiple pages.  */

  assert (sizeof (*object) <= PAGESIZE);

  static int size = 4 * 1024 * 1024;
  static uintptr_t offset = 0;
  static void *chunk;

  /* Make sure the object does not straddle pages.  If this would
     happen, start the object at the next page.  This wastes
     OBJECT_SIZE / 2 bytes on average.  */
  if (PAGESIZE - (offset & (PAGESIZE - 1)) < OBJECT_SIZE)
    offset += PAGESIZE - (offset & (PAGESIZE - 1));

  /* Check whether there is room in the current hunk.  */
  if (! chunk || offset + OBJECT_SIZE > size)
    {
      static struct anonymous_pager *pager
	= anonymous_pager_alloc (VG_ADDR_VOID, NULL,
				 size, MAP_ACCESS_ALL,
				 VG_OBJECT_POLICY (true,
						VG_OBJECT_PRIORITY_DEFAULT - 1),
				 0, NULL, &chunk);

      assert (pager);
      assert (chunk);

      /* Allocate the memory.  */
      madvise (chunk, size, MADV_NORMAL);
    }

  object = chunk + offset;
  offset += OBJECT_SIZE;
# else
  int pages_per_object = (OBJECT_SIZE + PAGESIZE - 1) / PAGESIZE;
  size_t size = pages_per_object * PAGESIZE;

  void *chunk;
  struct anonymous_pager *pager
    = anonymous_pager_alloc (VG_ADDR_VOID, NULL,
			     size, MAP_ACCESS_ALL,
			     VG_OBJECT_POLICY (true,
					    VG_OBJECT_PRIORITY_DEFAULT - 1),
			     ANONYMOUS_NO_RECURSIVE, object_fill, &chunk);
  assert (pager);
  assert (chunk);

  pager->cookie = (void *) (uintptr_t) id;

  /* XXX: We need a write barrier to prevent the compiler/CPU from
     reordering writes to OBJECT before PAGER->COOKIE is valid.  */
  __sync_synchronize ();

  object = chunk;

  /* We could do an madvise that we need the memory, however, this
     results in object_fill being called which also (via object_fill)
     does an madvise.  It is cheaper to fault and do a single madvise
     than to do an madvise twice.  */
# endif  /* OBJECT_SIZE <= PAGESIZE.  */
#else  /* USE_DISCARDABLE.  */
  if (objects)
    {
      struct freeobject *freeobject = objects;
      objects = freeobject->next;
      object = &freeobject->object;
    }
  else
    object = malloc (OBJECT_SIZE);
#endif

  /* If we are using large objects, the first access will cause a
     fault which will cause the entire object to be populated.  */

#if USE_DISCARDABLE
# if OBJECT_SIZE < PAGESIZE
  object_read (object, id);
# endif
#endif
#if !USE_DISCARDABLE
  object_read (object, id);
#endif

  object->id = id;

  profile_region_end ();

  return object;
}

static void
cache_init (void)
{
#if USE_CACHE
  hurd_ihash_init (&cache, false, HURD_IHASH_NO_LOCP);
#endif
}

#if CACHE_LINES
static int cache_entries;
static struct obj *cache_lru_list;

static void
list_unlink (struct obj **list, struct obj *e)
{
  if (e->next)
    {
      e->prev->next = e->next;
      e->next->prev = e->prev;

      if (*list == e)
	*list = e->next;
    }
  else
    /* List is now empty.  */
    {
      assert (*list == e);
      *list = NULL;
    }
}

static void
list_enqueue (struct obj **list, struct obj *e)
{
  /* Add to the head.  */

  struct obj *next = *list;

  if (next)
    {
      assert ((*list)->prev);
      struct obj *prev = (*list)->prev;
      assert (prev);
      assert (prev->next == next);

      e->next = next;
      e->prev = prev;
      next->prev = prev->next = e;
    }
  else
    /* The list was empty.  */
    e->next = e->prev = e;

  *list = e;
}

static struct obj *
list_dequeue (struct obj **list)
{
  if (! *list)
    return NULL;

  /* Remove from the tail.  */
  struct obj *e = (*list)->prev;
  list_unlink (list, e);
  return e;
}
#endif

#if USE_CACHE
static struct obj *
object_lookup (int id)
{
  struct obj *object;
  profile_region (NULL);
  object = hurd_ihash_find (&cache, id);
  profile_region_end ();
  if (object)
    {
      if (id != object->id)
	printf ("Got %d but wanted %d\n", object->id, id);
      assert (id == object->id);

#if CACHE_LINES
      list_unlink (&cache_lru_list, object);
      list_enqueue (&cache_lru_list, object);
#endif
#ifdef HAVE_LIVE
      if (! object->live)
	{
	  misses ++;
	  object_read (object, id);
	}
      else
#endif
	{
	  hits ++;
	}


      return object;
    }

  /* It's clearly a miss.  However, in the case where we use an object
     fill handler, we only know about subsequent misses when the
     handler is entered.  To correctly count hits and misses, we
     always note a hit in this function.  Then, if the handler is
     called, it increments misses and decrements hits.  */
#if !USE_DISCARDABLE
  misses ++;
#else
# if OBJECT_SIZE < PAGESIZE
  misses ++;
# else
  /* This will be adjusted in object_fill.  */
  hits ++;
# endif
#endif

  object = object_lookup_hard (id);
#if CACHE_LINES
  debug (5, "Adding %d", id);
  assert (id == object->id);

  list_enqueue (&cache_lru_list, object);

  cache_entries ++;
  if (cache_lines && cache_entries > cache_lines)
    /* Free 20% of the cache.  */
    {
      debug (5, "%d > %d, evicting %d entries",
	     cache_entries, cache_lines, cache_lines / 5);

      int i;
      for (i = 0; i < (cache_lines + 4) / 5; i ++)
	{
	  struct obj *object = list_dequeue (&cache_lru_list);
	  assert (object);

	  hurd_ihash_remove (&cache, object->id);
	  object_free (object);

	  cache_entries --;
	}
    }
#endif

  bool had_value;
  error_t err = hurd_ihash_replace (&cache, id, object,
				    &had_value, NULL);
  assert (err == 0);
  assert (! had_value);
  assert (object == hurd_ihash_find (&cache, id));

  return object;
}
#else
#define object_lookup(id) object_lookup_hard(id)
#endif

int
main (int argc, char *argv[])
{
  int i;
  for (i = 0; i < argc; i ++)
    {
      if (strcmp (argv[i], "--hog") == 0)
	have_a_hog = true;
      if (strcmp (argv[i], "--lines") == 0)
	{
#ifdef CACHE_LINES
	  cache_lines = atoi (argv[i + 1]);
#else
	  printf ("Cache lines disabled\n");
	  abort ();
#endif
	}
    }

  printf ("# Object size: %d bytes\n", OBJECT_SIZE);

#if USE_CACHE
# if CACHE_LINES
  printf ("# %d lines\n", cache_lines);
# else
  printf ("# Unlimited cache\n");
# endif
#if USE_DISCARDABLE
  printf ("# Using discardable memory.\n");
#endif
#else
  printf ("# No cache.\n");
#endif

  printf ("# Memory hog: %s\n", have_a_hog ? "yes" : "no ");

  printf ("# Objects: %d\n", OBJECTS);
  printf ("# Accesses: %d\n", ACCESSES);



#ifndef __gnu_hurd_viengoos__
  char *filename;
  asprintf (&filename, "cache-linux-%d-bytes-"
#ifdef CACHE_LINES
	    "%d-lines"
#else
	    "no-cache"
#endif
	    "-%shog.txt",
	    OBJECT_SIZE,
#ifdef CACHE_LINES
	    cache_lines,
#endif
	    have_a_hog ? "" : "no-");
  printf ("Writing to %s\n", filename);
  close (1);
  int fd = open (filename, O_CREAT|O_EXCL|O_RDWR, 0660);
  if (fd < 0)
    {
      fprintf (stderr, "Failed to open %s: %m\n", filename);
      abort ();
    }
  assert (fd == 1);
  free (filename);
#endif

  db_init ();
  cache_init ();

  printf ("And go...\n");

  epoch = now ();

  /* Fork it *after* we initialize the db.  */
  helper_fork ();

  for (iter = 0; iter < ACCESSES; iter ++)
    {
      struct obj *object;

      if (iter % 1000 == 0)
	debug (5, "iter: %d", iter);

      int id = rand_zipf (ALPHA, OBJECTS);
      object = object_lookup (id);

      int *i;
      for (i = &object->id;
	   (void *) i < (void *) object + OBJECT_SIZE;
	   i = (void *) i + PAGESIZE)
	if (i[0] != id
	    || i[1] != ((uintptr_t) i - (uintptr_t) &object->id) / PAGESIZE
	    || i[2] != id)

	  {
	    int offset = (uintptr_t) i - (uintptr_t) object;
	    printf ("Object %d contains unexpected values %d/%d/%d at %d (%p/%p).\n",
		    id, i[0], i[1], i[2],
		    offset / PAGESIZE,
		    object, i);
	    if (i[0] > 0 && i[0] <= OBJECTS)
	      {
		id = i[0];
		object = object_lookup (id);
		i = (void *) object + offset;
		printf ("Object %d is at: %p (%d/%d)\n",
			id, object, i[0], i[1]);
	      }
#ifdef USE_L4
	    _L4_kdb ("");
#endif
	    abort ();
	  }

#if !USE_CACHE
      object_free (object);
#endif
    }

  done = now ();

  printf ("# Total time: %lld.%.02lld seconds\n",
	  ((done - epoch) / 1000000),
	  ((done - epoch) / 10000) % 100);

  printf ("# Object size: %d bytes\n", OBJECT_SIZE);

#if USE_CACHE
# if CACHE_LINES
  printf ("# %d lines\n", cache_lines);
# else
  printf ("# Unlimited cache\n");
# endif
#if USE_DISCARDABLE
  printf ("# Using discardable memory.\n");
#endif

  printf ("# %d hits, %d misses\n", hits, misses);
#else
  printf ("# No cache.\n");
#endif

  printf ("# Memory hog: %s\n", have_a_hog ? "yes" : "no ");

  printf ("# Objects: %d\n", OBJECTS);
  printf ("# Accesses: %d\n", ACCESSES);

  void *status;
  pthread_join (helper_tid, &status);

  {
    s_printf ("# Alloc / avail in pages (4k); heap in bytes\n");
    s_printf ("# Time\tPeriod\tM.Alloc\tM.Avail\tH.Alloc\tH.Avail\tHeap\tHog\tIters\tHits\tMisses\n");

    int i;
    for (i = 0; i < stat_count; i ++)
      {
	s_printf ("%lld.%lld\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",
		  stats[i].time / 1000000, (stats[i].time / 100000) % 10,
		  stats[i].period,
		  stats[i].alloced[0], stats[i].available[0], 
		  stats[i].alloced[1], stats[i].available[1],
		  stats[i].heap,
		  stats[i].hog,
		  stats[i].iter,
		  stats[i].hits,
		  stats[i].misses);
      }
  }

  s_printf ("Stats!\n");

  profile_stats_dump ();

  return 0;
}
