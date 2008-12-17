/* Taken from:
   http://www.hpl.hp.com/personal/Hans_Boehm/gc/gc_bench/GCBench.c.
   Last modified: 26-Apr-2000 11:30.  md5sum:
   1ddd6be7a929a8dbbf3e8b26b5e46a3d . */

#define GC
// This is adapted from a benchmark written by John Ellis and Pete Kovac
// of Post Communications.
// It was modified by Hans Boehm of Silicon Graphics.
// Translated to C++ 30 May 1997 by William D Clinger of Northeastern Univ.
// Translated to C 15 March 2000 by Hans Boehm, now at HP Labs.
//
//      This is no substitute for real applications.  No actual application
//      is likely to behave in exactly this way.  However, this benchmark was
//      designed to be more representative of real applications than other
//      Java GC benchmarks of which we are aware.
//      It attempts to model those properties of allocation requests that
//      are important to current GC techniques.
//      It is designed to be used either to obtain a single overall performance
//      number, or to give a more detailed estimate of how collector
//      performance varies with object lifetimes.  It prints the time
//      required to allocate and collect balanced binary trees of various
//      sizes.  Smaller trees result in shorter object lifetimes.  Each cycle
//      allocates roughly the same amount of memory.
//      Two data structures are kept around during the entire process, so
//      that the measured performance is representative of applications
//      that maintain some live in-memory data.  One of these is a tree
//      containing many pointers.  The other is a large array containing
//      double precision floating point numbers.  Both should be of comparable
//      size.
//
//      The results are only really meaningful together with a specification
//      of how much memory was used.  It is possible to trade memory for
//      better time performance.  This benchmark should be run in a 32 MB
//      heap, though we don't currently know how to enforce that uniformly.
//
//      Unlike the original Ellis and Kovac benchmark, we do not attempt
//      measure pause times.  This facility should eventually be added back
//      in.  There are several reasons for omitting it for now.  The original
//      implementation depended on assumptions about the thread scheduler
//      that don't hold uniformly.  The results really measure both the
//      scheduler and GC.  Pause time measurements tend to not fit well with
//      current benchmark suites.  As far as we know, none of the current
//      commercial Java implementations seriously attempt to minimize GC pause
//      times.

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#ifdef GC
#  include <gc/gc.h>
#endif

#include <stdint.h>
#include <stdbool.h>

static inline uint64_t
now (void)
{
  struct timeval t;
  struct timezone tz;

  if (gettimeofday( &t, &tz ) == -1)
    return 0;
  return (t.tv_sec * 1000000ULL + t.tv_usec);
}
uint64_t epoch;

#include <assert.h>

#define STATS 5000

struct stats
{
  int alloced[2];
  int available[2];
  uint64_t time;
  int period;
  int gcs;
  int iter;
} stats[STATS];
int stat_count;

static int done;

static int iter;

#ifdef __gnu_hurd_viengoos__
#include <hurd/storage.h>
#include <viengoos/cap.h>
#include <viengoos/activity.h>
#include <pthread.h>
#include <hurd/anonymous.h>
#include <string.h>

vg_addr_t gc_activity;
vg_addr_t hog_activity;

bool have_a_hog = false;

void *
helper (void *arg)
{
  pthread_setactivity_np (hog_activity);

  struct activity_info info;

  void wait_read_stats (void)
  {
    int count;

    /* First the main thread.  */
    error_t err;

    err = vg_activity_info (gc_activity, activity_info_stats,
			    stat_count == 0
			    ? 0 : stats[stat_count - 1].period + 1,
			    &info);
    assert_perror (err);
    assert (info.event == activity_info_stats);
    assert (info.stats.count > 0);

    stats[stat_count].alloced[0]
      = info.stats.stats[0].clean + info.stats.stats[0].dirty
      + info.stats.stats[0].pending_eviction;
    stats[stat_count].available[0] = info.stats.stats[0].available;

    stats[stat_count].time = now () - epoch;
    stats[stat_count].period = info.stats.stats[0].period;
    stats[stat_count].gcs = GC_gc_no;
    stats[stat_count].iter = iter;

    /* Then, the hog.  */
    err = vg_activity_info (hog_activity, activity_info_stats,
			    stat_count == 0
			    ? 0 : stats[stat_count - 1].period + 1,
			    &info);
    assert_perror (err);
    assert (info.event == activity_info_stats);
    assert (info.stats.count > 0);

    stats[stat_count].alloced[1]
      = info.stats.stats[0].clean + info.stats.stats[0].dirty
      + info.stats.stats[0].pending_eviction;
    stats[stat_count].available[1] = info.stats.stats[0].available;

    stat_count ++;
    if (stat_count % 10 == 0)
      debug (0, DEBUG_BOLD ("Period %d"), stat_count);
  }

  if (have_a_hog)
    {
      /* Wait a minute before starting.  */
      int i;
      for (i = 0; i < 20; i ++)
	wait_read_stats ();


      /* Now, allocate a 10MB chunk of memory every 2 seconds until we've
	 allocate half the initially available memory.  */

      int available = stats[1].available[0] * PAGESIZE;

      printf (DEBUG_BOLD ("mem hog starting (avail: %d)!") "\n", available);

      /* The chunk size.  */
      int s = 5 * 1024 * 1024;
      int total = available / 2 / s;
      struct anonymous_pager *pagers[total];
      void *buffers[total];

      int c;
      for (c = 0; c < total && ! done; c ++)
	{
	  pagers[c]
	    = anonymous_pager_alloc (hog_activity, NULL, s, MAP_ACCESS_ALL,
				     VG_OBJECT_POLICY (false, VG_OBJECT_PRIORITY_DEFAULT), 0,
				     NULL, &buffers[c]);
	  assert (pagers[c]);
	  assert (buffers[c]);

	  memset (buffers[c], 0, s);

	  wait_read_stats ();
	}

      /* Wait a minute before freeing.  */
      for (i = 0; i < 20 && ! done; i ++)
	wait_read_stats ();

      printf (DEBUG_BOLD ("mem hog releasing memory!") "\n");

      /* Release the memory.  */
      for (i = 0; i < total && ! done; i ++)
	{
	  wait_read_stats ();
      
	  printf (DEBUG_BOLD ("release: %d!") "\n", i);
	  anonymous_pager_destroy (pagers[i]);
	}
    }

  /* Finally, wait until the main thread is done.  */
  while (! done)
    wait_read_stats ();

  return 0;
}
#else
#define have_a_hog false

void *
helper (void *arg)
{
  while (! done)
    {
      sleep (2);
      stats[stat_count].alloced[0] = GC_get_heap_size ();
      stats[stat_count].available[0] = 0;
      stats[stat_count].time = now () - epoch;
      stats[stat_count].gcs = GC_gc_no;
      stats[stat_count].iter = iter;
      stat_count ++;
    }

  return 0;
}
#endif


pthread_t helper_tid;

void
helper_fork (void)
{
  int err;

#ifdef __gnu_hurd_viengoos__
  gc_activity = storage_alloc (VG_ADDR_VOID,
			       vg_cap_activity_control, STORAGE_LONG_LIVED,
			       VG_OBJECT_POLICY_DEFAULT, VG_ADDR_VOID).addr;
  if (VG_ADDR_IS_VOID (gc_activity))
    panic ("Failed to allocate main activity");

  struct object_name name;
  snprintf (&name.name[0], sizeof (name.name), "gc.%x", l4_myself ());
  vg_object_name (VG_ADDR_VOID, gc_activity, name);

  hog_activity = storage_alloc (VG_ADDR_VOID,
				vg_cap_activity_control, STORAGE_LONG_LIVED,
				VG_OBJECT_POLICY_DEFAULT, VG_ADDR_VOID).addr;
  if (VG_ADDR_IS_VOID (hog_activity))
    panic ("Failed to allocate hog activity");

  snprintf (&name.name[0], sizeof (name.name), "hog.%x", l4_myself ());
  vg_object_name (VG_ADDR_VOID, hog_activity, name);

  /* We give the main thread and the hog the same priority and
     weight.  */  
  struct activity_policy in, out;
  memset (&in, 0, sizeof (in));
  in.sibling_rel.priority = 1;
  in.sibling_rel.weight = 10;

  in.child_rel.priority = 2;
  in.child_rel.weight = 20;

  err = vg_activity_policy (VG_ADDR_VOID,
			    VG_ACTIVITY_POLICY_CHILD_REL_SET, in, &out);
  assert (err == 0);

  err = vg_activity_policy (hog_activity,
			    VG_ACTIVITY_POLICY_SIBLING_REL_SET, in, &out);
  assert (err == 0);

  err = vg_activity_policy (gc_activity,
			    VG_ACTIVITY_POLICY_SIBLING_REL_SET, in, &out);
  assert (err == 0);


  pthread_setactivity_np (gc_activity);
#endif

  pthread_create (&helper_tid, NULL, helper, NULL);
}

#if 1
#define DEBUG(fmt, ...)
#else
#define DEBUG(fmt, ...) printf (fmt, ##__VA_ARGS__)
#endif

#ifdef PROFIL
  extern void init_profiling();
  extern dump_profile();
#endif

//  These macros were a quick hack for the Macintosh.
//
//  #define currentTime() clock()
//  #define elapsedTime(x) ((1000*(x))/CLOCKS_PER_SEC)

#define currentTime() stats_rtclock()
#define elapsedTime(x) (x)

/* Get the current time in milliseconds */

unsigned
stats_rtclock( void )
{
  struct timeval t;
  struct timezone tz;

  if (gettimeofday( &t, &tz ) == -1)
    return 0;
  return (t.tv_sec * 1000 + t.tv_usec / 1000);
}

static const int kStretchTreeDepth    = 18;      // about 16Mb
static const int kLongLivedTreeDepth  = 16;  // about 4Mb
static const int kArraySize  = 500000;  // about 4Mb
static const int kMinTreeDepth = 4;
static const int kMaxTreeDepth = 16;

typedef struct Node0_struct {
        struct Node0_struct * left;
        struct Node0_struct * right;
        int i, j;
} Node0;

#ifdef HOLES
#   define HOLE() GC_NEW(Node0);
#else
#   define HOLE()
#endif

typedef Node0 *Node;

void init_Node(Node me, Node l, Node r) {
    me -> left = l;
    me -> right = r;
}

#ifndef GC
  void destroy_Node(Node me) {
    if (me -> left) {
	destroy_Node(me -> left);
    }
    if (me -> right) {
	destroy_Node(me -> right);
    }
    free(me);
  }
#endif

// Nodes used by a tree of a given size
static int TreeSize(int i) {
        return ((1 << (i + 1)) - 1);
}

// Number of iterations to use for a given tree depth
static int NumIters(int i) {
        return 2 * TreeSize(kStretchTreeDepth) / TreeSize(i);
}

// Build tree top down, assigning to older objects.
static void Populate(int iDepth, Node thisNode) {
        if (iDepth<=0) {
                return;
        } else {
                iDepth--;
#		ifdef GC
                  thisNode->left  = GC_NEW(Node0); HOLE();
                  thisNode->right = GC_NEW(Node0); HOLE();
#		else
                  thisNode->left  = calloc(1, sizeof(Node0));
                  thisNode->right = calloc(1, sizeof(Node0));
#		endif
                Populate (iDepth, thisNode->left);
                Populate (iDepth, thisNode->right);
        }
}

// Build tree bottom-up
static Node MakeTree(int iDepth) {
	Node result;
        if (iDepth<=0) {
#	    ifndef GC
		result = calloc(1, sizeof(Node0));
#	    else
		result = GC_NEW(Node0); HOLE();
#	    endif
	    assert (result);
	    /* result is implicitly initialized in both cases. */
	    return result;
        } else {
	    Node left = MakeTree(iDepth-1);
	    Node right = MakeTree(iDepth-1);
#	    ifndef GC
		result = malloc(sizeof(Node0));
#	    else
		result = GC_NEW(Node0); HOLE();
#	    endif
	    assert (result);
	    init_Node(result, left, right);
	    return result;
        }
}

static void PrintDiagnostics() {
#if 0
        long lFreeMemory = Runtime.getRuntime().freeMemory();
        long lTotalMemory = Runtime.getRuntime().totalMemory();

        System.out.print(" Total memory available="
                         + lTotalMemory + " bytes");
        System.out.println("  Free memory=" + lFreeMemory + " bytes");
#endif
}

static void TimeConstruction(int depth) {
        long    tStart, tFinish;
        int     iNumIters = NumIters(depth);
        Node    tempTree;
	int 	i;

	DEBUG("Creating %d trees of depth %d\n", iNumIters, depth);
        
        tStart = currentTime();
        for (i = 0; i < iNumIters; ++i) {
#		ifndef GC
                  tempTree = calloc(1, sizeof(Node0));
#		else
                  tempTree = GC_NEW(Node0);
#		endif
                Populate(depth, tempTree);
#		ifndef GC
                  destroy_Node(tempTree);
#		endif
                tempTree = 0;
        }
        tFinish = currentTime();
        DEBUG("\tTop down construction took %ld msec\n",
               elapsedTime(tFinish - tStart));
             
        tStart = currentTime();
        for (i = 0; i < iNumIters; ++i) {
                tempTree = MakeTree(depth);
#		ifndef GC
                  destroy_Node(tempTree);
#		endif
                tempTree = 0;
        }
        tFinish = currentTime();
        DEBUG("\tBottom up construction took %ld msec\n",
               elapsedTime(tFinish - tStart));

}

int main() {
  extern int GC_print_stats;
  // GC_print_stats = 1;
  extern int GC_viengoos_scheduler;
  GC_viengoos_scheduler = 1;

  helper_fork ();
  epoch = now ();


        Node    root;
        Node    longLivedTree;
        Node    tempTree;
        long    tStart, tFinish;
        long    tElapsed;
  	int	i, d;
	double 	*array;

#ifdef GC
 // GC_full_freq = 30;
 // GC_free_space_divisor = 16;
 // GC_enable_incremental();
#endif
	printf("Garbage Collector Test\n");
 	printf(" Live storage will peak at %d bytes.\n\n",
               2 * sizeof(Node0) * TreeSize(kLongLivedTreeDepth) +
               sizeof(double) * kArraySize);
        printf(" Stretching memory with a binary tree of depth %d\n",
               kStretchTreeDepth);
        PrintDiagnostics();
#	ifdef PROFIL
	    init_profiling();
#	endif
       
        tStart = currentTime();
        
        // Stretch the memory space quickly
        tempTree = MakeTree(kStretchTreeDepth);
#	ifndef GC
          destroy_Node(tempTree);
#	endif
        tempTree = 0;

        // Create a long lived object
        DEBUG(" Creating a long-lived binary tree of depth %d\n",
               kLongLivedTreeDepth);
#	ifndef GC
          longLivedTree = calloc(1, sizeof(Node0));
#	else 
          longLivedTree = GC_NEW(Node0);
#	endif
        Populate(kLongLivedTreeDepth, longLivedTree);

        // Create long-lived array, filling half of it
	DEBUG(" Creating a long-lived array of %d doubles\n", kArraySize);
#	ifndef GC
          array = malloc(kArraySize * sizeof(double));
#	else
#	  ifndef NO_PTRFREE
            array = GC_MALLOC_ATOMIC(sizeof(double) * kArraySize);
#	  else
            array = GC_MALLOC(sizeof(double) * kArraySize);
#	  endif
#	endif
        for (i = 0; i < kArraySize/2; ++i) {
                array[i] = 1.0/i;
        }
        PrintDiagnostics();

#define ITERATIONS 100
  for (iter = 0; iter < ITERATIONS; iter ++)
    {
      printf ("Iteration %d\n", iter);
        for (d = kMinTreeDepth; d <= kMaxTreeDepth; d += 2) {
                TimeConstruction(d);
        }
    }


        if (longLivedTree == 0 || array[1000] != 1.0/1000)
		fprintf(stderr, "Failed\n");
                                // fake reference to LongLivedTree
                                // and array
                                // to keep them from being optimized away
        tFinish = currentTime();
        tElapsed = elapsedTime(tFinish-tStart);
        PrintDiagnostics();
        printf("Completed in %ld.%03ld sec\n",
	       tElapsed / 1000, tElapsed % 1000);
#	ifdef GC
	{
	  extern void GC_dump_stats (void);
	  GC_dump_stats ();
	}
#       endif
#	ifdef PROFIL
	  dump_profile();
#	endif

	  done = 1;
	  void *status;
	  pthread_join (helper_tid, &status);

	  {
	    printf ("%s scheduler, %smemory hog\n"
		    "time\tgc alloc'd\tgc avail\thog alloc'd\thog avail\tgcs\titeration\n",
		    GC_viengoos_scheduler ? "Viengoos" : "Boehm",
		    have_a_hog ? "" : "no ");

	    int i;
	    for (i = 0; i < stat_count; i ++)
	      {
		printf ("%lld.%lld\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",
			stats[i].time / 1000000,
			(stats[i].time / 100000) % 10,
			stats[i].period,
			stats[i].alloced[0], stats[i].available[0], 
			stats[i].alloced[1], stats[i].available[1],
			stats[i].gcs,
			stats[i].iter);
	      }
	  }
}

