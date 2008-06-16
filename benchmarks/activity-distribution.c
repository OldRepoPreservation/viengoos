#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

#include <hurd/activity.h>
#include <hurd/storage.h>
#include <hurd/startup.h>
#include <hurd/anonymous.h>

static addr_t activity;

/* Initialized by the machine-specific startup-code.  */
extern struct hurd_startup_data *__hurd_startup_data;

int
main (int argc, char *argv[])
{
  error_t err;

  extern int output_debug;
  output_debug = 1;

  activity = __hurd_startup_data->activity;

  printf ("%s running...\n", argv[0]);

#define THREADS 4

  /* The activities.  */
  addr_t activities[THREADS];

  /* Create THREADS activities, each with an increasing weight.  */
  int i;
  for (i = 0; i < THREADS; i ++)
    {
      activities[i] = storage_alloc (activity, cap_activity,
				     STORAGE_LONG_LIVED,
				     OBJECT_POLICY_DEFAULT,
				     ADDR_VOID).addr;

      struct activity_policy in;
      in.sibling_rel.weight = i + 1;
      struct activity_policy out;
      err = rm_activity_policy (activities[i],
				ACTIVITY_POLICY_SIBLING_REL_WEIGHT_SET, in,
				&out);
      assert (err == 0);
    }

  bool terminate = false;
  l4_thread_id_t tids[THREADS];
  for (i = 0; i < THREADS; i ++)
    tids[i] = l4_nilthread;

  int available;
  {
    int count;
    struct activity_stats_buffer buffer;

    err = rm_activity_stats (activity, 1, &buffer, &count);
    assert (err == 0);
    assert (count > 0);

    available = buffer.stats[0].available * PAGESIZE;
  }
  printf ("%d kb memory available\n", available / 1024);

  bool my_fill (struct anonymous_pager *anon,
		uintptr_t offset, uintptr_t count,
		void *pages[],
		struct exception_info info)
  {
    uintptr_t *p = pages[0];
    p[0] = offset;
    p[1] = l4_myself ();
    return true;
  }

  void *worker (void *arg)
  {
    int w = (intptr_t) arg;

    tids[w] = l4_myself ();

    pthread_setactivity_np (activities[w]);

    /* Object size.  */
#define SIZE (256 * PAGESIZE)

    /* The number of objects: set so that they fill half of the
       available memory.  */
#define ITEMS (available / SIZE / 2)

    struct anonymous_pager *pagers[ITEMS];
    memset (pagers, 0, sizeof (pagers));
    void *buffers[ITEMS];
    memset (buffers, 0, sizeof (buffers));

    int t = 0;
    while (! terminate)
      {
	int i = rand () % ITEMS;

	if (! pagers[i])
	  /* Allocate a (discardable) buffer.  */
	  {
	    pagers[i]
	      = anonymous_pager_alloc (ADDR_VOID, NULL, SIZE, MAP_ACCESS_ALL,
				       OBJECT_POLICY (true,
						      OBJECT_PRIORITY_LRU),
				       0, my_fill, &buffers[i]);
	    assert (pagers[i]);
	    assert (buffers[i]);
	  }

	int j;
	for (j = 0; j < SIZE; j += PAGESIZE)
	  {
	    uintptr_t *p = buffers[i] + j;
	    assertx (p[0] == j && p[1] == l4_myself (),
		     "%x: %x =? %x, thread: %x",
		     p, p[0], j, p[1]);

	    t += * (int *) (buffers[i] + j);
	  }

	/* 100ms.  */
	l4_sleep (l4_time_period (100 * 1000));
      }

    /* We need to return t, otherwise, the above loop will be
       optimized away.  */
    return (void *) t;
  }

  /* Start the threads.  */
  pthread_t threads[THREADS];

  for (i = 0; i < THREADS; i ++)
    {
      err = pthread_create (&threads[i], NULL, worker, (void *) (intptr_t) i);
      if (err)
	printf ("Failed to create thread: %s\n", strerror (errno));
    }

#define ITERATIONS 200
  struct activity_stats stats[ITERATIONS][1 + THREADS];

  uintptr_t next_period = 0;
  for (i = 0; i < ITERATIONS; i ++)
    {
      debug (0, DEBUG_BOLD ("starting iteration %d (%x)"), i, l4_myself ());

      int count;
      struct activity_stats_buffer buffer;

      rm_activity_stats (activity, next_period, &buffer, &count);
      assert (count > 0);
      if (i != 0)
	assertx (buffer.stats[0].period != stats[i - 1][0].period,
		 "%d == %d",
		 buffer.stats[0].period, stats[i - 1][0].period);

      stats[i][0] = buffer.stats[0];

      int j;
      for (j = 0; j < THREADS; j ++)
	{
	  rm_activity_stats (activities[j], next_period, &buffer, &count);
	  assert (count > 0);
	  stats[i][1 + j] = buffer.stats[0];
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
    printf (ADDR_FMT " ", ADDR_PRINTF (activities[i]));
  printf ("\n");

  for (i = 0; i < ITERATIONS; i ++)
    {
      int j;

      printf ("%d ", (int) stats[i][0].period);

      for (j = 0; j < 1 + THREADS; j ++)
	printf ("%d ", (int) stats[i][j].clean + (int) stats[i][j].dirty);
      printf ("\n");
    }

  printf ("Done!\n");

  return 0;
}
