#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

#include <viengoos/activity.h>
#include <hurd/storage.h>
#include <hurd/startup.h>

static vg_addr_t activity;

/* Initialized by the machine-specific startup-code.  */
extern struct hurd_startup_data *__hurd_startup_data;

int
main (int argc, char *argv[])
{
  extern int output_debug;
  output_debug = 1;

  activity = __hurd_startup_data->activity;

#define PAGES 1000

  /* Allocate the buffer.  */
  void *buffer = mmap (0, PAGESIZE * PAGES, PROT_READ | PROT_WRITE,
		       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (buffer == MAP_FAILED)
    {
      printf ("Failed to allocate memory: %s\n", strerror (errno));
      return 1;
    }

#define THREADS 3
  /* And the activities.  */
  vg_addr_t activities[THREADS];

  int i;
  for (i = 0; i < THREADS; i ++)
    activities[i] = storage_alloc (activity, vg_cap_activity,
				   STORAGE_LONG_LIVED,
				   VG_OBJECT_POLICY_DEFAULT,
				   VG_ADDR_VOID).addr;

  bool terminate = false;
  vg_thread_id_t tids[THREADS];
  for (i = 0; i < THREADS; i ++)
    tids[i] = vg_niltid;

  void *worker (void *arg)
  {
    int w = (intptr_t) arg;

    tids[w] = hurd_myself ();

    pthread_setactivity_np (activities[w]);

    uintptr_t t = 0;
    int count = 0;
    while (! terminate)
      {
	int i;
	for (i = 0; i < 1; i ++)
	  {
	    uintptr_t *p = buffer + (rand () % PAGES) * PAGESIZE;
	    t += *p;

	    count ++;

	    if (count % 100000 == 0)
	      debug (0, DEBUG_BOLD ("Read %d pages so far"), count);
	  }

#if 0
	/* ~128Hz.  */
	l4_sleep (l4_time_period (1 << 13));
#elif 0
	l4_thread_switch (tids[rand () % THREADS]);
#endif
      }

    printf ("Thread %d: %d operations\n", w, count);

    /* We need to return t, otherwise, the above loop will be
       optimized away.  */
    return (void *) t;
  }

  /* Start the threads.  */
  pthread_t threads[THREADS];

  error_t err;
  for (i = 0; i < THREADS; i ++)
    {
      err = pthread_create (&threads[i], NULL, worker, (intptr_t) i);
      if (err)
	printf ("Failed to create thread: %s\n", strerror (errno));
    }

#define ITERATIONS 100
  struct vg_activity_stats stats[ITERATIONS][1 + THREADS];

  uintptr_t next_period = 0;
  for (i = 0; i < ITERATIONS; i ++)
    {
      debug (0, DEBUG_BOLD ("starting iteration %d (%x)"), i, hurd_myself ());

      struct vg_activity_info info;

      vg_activity_info (activity, activity,
			vg_activity_info_stats, next_period, &info);
      assert (info.event == vg_activity_info_stats);
      assert (info.stats.count > 0);
      if (i != 0)
	assertx (info.stats.stats[0].period != stats[i - 1][0].period,
		 "%x != %x",
		 info.stats.stats[0].period, stats[i - 1][0].period);

      stats[i][0] = info.stats.stats[0];

      int j;
      for (j = 0; j < THREADS; j ++)
	{
	  vg_activity_info (activity, activity,
			    vg_activity_info_stats, next_period, &info);
	  assert (info.event == vg_activity_info_stats);
	  assert (info.stats.count > 0);
	  stats[i][1 + j] = info.stats.stats[0];
	}

      next_period = stats[i][0].period + 1;
    }

  terminate = true;
  for (i = 0; i < THREADS; i ++)
    {
      void *status;
      pthread_join (threads[i], &status);
    }

  printf ("parent ");
  for (i = 0; i < THREADS; i ++)
    printf (VG_ADDR_FMT " ", VG_ADDR_PRINTF (activities[i]));
  printf ("\n");

  for (i = 0; i < ITERATIONS; i ++)
    {
      int j;

      printf ("%d", (int) stats[i][0].period);

      for (j = 0; j < 1 + THREADS; j ++)
	printf ("\t%d\t%d\t%d\t%d\t%d\t%d", (int) stats[i][j].clean,
		(int) stats[i][j].dirty,
		(int) stats[i][j].pending_eviction,
		(int) stats[i][j].discarded,
		(int) stats[i][j].freeloading,
		(int) stats[i][j].freeloaded);
      printf ("\n");
    }

  printf ("Done!\n");

  return 0;
}
