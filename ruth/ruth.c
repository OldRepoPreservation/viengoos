/* ruth.c - Test server.
   Copyright (C) 2007, 2008 Free Software Foundation, Inc.
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <hurd/thread.h>
#include <hurd/startup.h>
#include <hurd/cap.h>
#include <hurd/folio.h>
#include <hurd/rm.h>
#include <hurd/stddef.h>
#include <hurd/capalloc.h>
#include <hurd/as.h>
#include <hurd/storage.h>
#include <hurd/activity.h>
#include <hurd/futex.h>

#include <bit-array.h>
#include <string.h>

#include <sys/mman.h>
#include <pthread.h>
#include <stdio.h>

extern int output_debug;

static addr_t activity;

/* Initialized by the machine-specific startup-code.  */
extern struct hurd_startup_data *__hurd_startup_data;


/* The program name.  */
const char program_name[] = "ruth";


/* The following functions are required by pthread.  */


int
main (int argc, char *argv[])
{
  output_debug = 3;

  printf ("%s " PACKAGE_VERSION "\n", program_name);
  printf ("Hello, here is Ruth, your friendly root server!\n");

  debug (2, "RM: %x.%x", l4_thread_no (__hurd_startup_data->rm),
	 l4_version (__hurd_startup_data->rm));

  activity = __hurd_startup_data->activity;

  {
    printf ("Checking shadow page tables... ");

    int processing_folio = -1;

    int visit (addr_t addr,
	       l4_word_t type, struct cap_properties properties,
	       bool writable,
	       void *cookie)
      {
	struct cap *slot = slot_lookup (activity, addr, -1, NULL);

	assert (slot);
	assert (type == slot->type);
	if (type == cap_cappage || type == cap_rcappage || type == cap_folio)
	  assertx (slot->shadow,
		   ADDR_FMT ", %s",
		   ADDR_PRINTF (addr), cap_type_string (type));
	else
	  assert (! slot->shadow);

	if (type == cap_folio)
	  {
	    processing_folio = FOLIO_OBJECTS;
	    return 0;
	  }

	if (processing_folio >= 0)
	  {
	    processing_folio --;
	    return -1;
	  }

	return 0;
      }

    as_walk (visit, ~(1 << cap_void), NULL);
    printf ("ok.\n");
  }

  {
    printf ("Checking folio_object_alloc... ");


    addr_t folio = capalloc ();
    assert (! ADDR_IS_VOID (folio));
    error_t err = rm_folio_alloc (activity, folio, FOLIO_POLICY_DEFAULT);
    assert (! err);

    int i;
    for (i = -10; i < 129; i ++)
      {
	addr_t addr = capalloc ();
	if (ADDR_IS_VOID (addr))
	  panic ("capalloc");

	err = rm_folio_object_alloc (activity, folio, i, cap_page,
				     OBJECT_POLICY_DEFAULT, 0,
				     addr, ADDR_VOID);
	assert ((err == 0) == (0 <= i && i < FOLIO_OBJECTS));

	if (0 <= i && i < FOLIO_OBJECTS)
	  {
	    l4_word_t type;
	    struct cap_properties properties;
	    err = rm_cap_read (activity, ADDR_VOID, addr, &type, &properties);
	    assert (! err);
	    assert (type == cap_page);
	  }
	capfree (addr);
      }

    err = rm_folio_free (activity, folio);
    assert (! err);
    capfree (folio);

    printf ("ok.\n");
  }

  {
    printf ("Checking folio_alloc... ");

    /* We allocate a sub-tree and fill it with folios (specifically,
       2^(bits - 1) folios).  */
    int bits = 2;
    addr_t root = as_alloc (bits + FOLIO_OBJECTS_LOG2 + PAGESIZE_LOG2,
			    1, true);
    assert (! ADDR_IS_VOID (root));

    int i;
    for (i = 0; i < (1 << bits); i ++)
      {
	addr_t f = addr_extend (root, i, bits);

	struct cap *slot = as_slot_ensure (f);
	assert (slot);
	slot->type = cap_folio;

	error_t err = rm_folio_alloc (activity, f, FOLIO_POLICY_DEFAULT);
	assert (! err);

	struct storage shadow_storage
	  = storage_alloc (activity, cap_page, STORAGE_EPHEMERAL, ADDR_VOID);
	struct object *shadow = ADDR_TO_PTR (addr_extend (shadow_storage.addr,
							  0, PAGESIZE_LOG2));
	cap_set_shadow (slot, shadow);
	memset (shadow, 0, PAGESIZE);

	int j;
	for (j = 0; j <= i; j ++)
	  {
	    l4_word_t type;
	    struct cap_properties properties;

	    error_t err = rm_cap_read (activity, ADDR_VOID,
				       addr_extend (root, j, bits),
				       &type, &properties);
	    assert (! err);
	    assert (type == cap_folio);

	    struct cap *slot = slot_lookup (activity, f, -1, NULL);
	    assert (slot);
	    assert (slot->type == cap_folio);
	  }
      }

    for (i = 0; i < (1 << bits); i ++)
      {
	addr_t f = addr_extend (root, i, bits);

	error_t err = rm_folio_free (activity, f);
	assert (! err);

	struct cap *slot = slot_lookup (activity, f, -1, NULL);
	assert (slot);
	assert (slot->type == cap_folio);
	slot->type = cap_void;

	void *shadow = cap_get_shadow (slot);
	assert (shadow);
	storage_free (addr_chop (PTR_TO_ADDR (shadow), PAGESIZE_LOG2), 1);
      }

    as_free (root, 1);

    printf ("ok.\n");
  }


  {
    printf ("Checking storage_alloc... ");

    const int n = 4 * FOLIO_OBJECTS;
    addr_t storage[n];

    int i;
    for (i = 0; i < n; i ++)
      {
	storage[i] = storage_alloc (activity, cap_page,
				    (i & 1) == 0
				    ? STORAGE_LONG_LIVED
				    : STORAGE_EPHEMERAL,
				    ADDR_VOID).addr;
	assert (! ADDR_IS_VOID (storage[i]));
	* (int *) (ADDR_TO_PTR (addr_extend (storage[i], 0, PAGESIZE_LOG2)))
	  = i;

	int j;
	for (j = 0; j <= i; j ++)
	  assert (* (int *) (ADDR_TO_PTR (addr_extend (storage[j],
						       0, PAGESIZE_LOG2)))
		  == j);
      }

    for (i = 0; i < n; i ++)
      {
	storage_free (storage[i], true);
      }

    printf ("ok.\n");
  }

  {
    printf ("Checking mmap... ");

#define SIZE (16 * PAGESIZE)
    void *buffer = mmap (0, SIZE, PROT_READ | PROT_WRITE,
			 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert (buffer != MAP_FAILED);

    char *p;
    for (p = buffer; (uintptr_t) p < (uintptr_t) buffer + SIZE; p ++)
      *p = 1;
    for (p = buffer; (uintptr_t) p < (uintptr_t) buffer + SIZE; p ++)
      assert (*p == 1);

    printf ("ok.\n");
  }

  {
    static volatile int done;
    char stack[0x1000];

    void start (void)
    {
      do_debug (4)
	as_dump ("thread");

      debug (4, "I'm running (%x.%x)!",
	     l4_thread_no (l4_myself ()),
	     l4_version (l4_myself ()));

      done = 1;
      do
	l4_yield ();
      while (1);
    }

    printf ("Checking thread creation... ");

    addr_t thread = capalloc ();
    debug (5, "thread: " ADDR_FMT, ADDR_PRINTF (thread));
    addr_t storage = storage_alloc (activity, cap_thread, STORAGE_LONG_LIVED,
				    thread).addr;

    struct hurd_thread_exregs_in in;

    in.aspace = ADDR (0, 0);
    in.aspace_cap_properties = CAP_PROPERTIES_DEFAULT;
    in.aspace_cap_properties_flags = CAP_COPY_COPY_SOURCE_GUARD;

    in.activity = activity;

    in.sp = (l4_word_t) ((void *) stack + sizeof (stack));
    in.ip = (l4_word_t) &start;

    struct hurd_thread_exregs_out out;

    rm_thread_exregs (activity, thread,
		      HURD_EXREGS_SET_ASPACE | HURD_EXREGS_SET_ACTIVITY
		      | HURD_EXREGS_SET_SP_IP | HURD_EXREGS_START
		      | HURD_EXREGS_ABORT_IPC,
		      in, &out);

    debug (5, "Waiting for thread");
    while (done == 0)
      l4_yield ();
    debug (5, "Thread done!");

    storage_free (storage, true);
    capfree (thread);

    printf ("ok.\n");
  }

  {
    printf ("Checking pthread library... ");

#undef N
#define N 4
    pthread_t threads[N];

    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
#define FACTOR 10
    static volatile int shared_resource;
    shared_resource = 0;

    void *start (void *arg)
    {
      uintptr_t i = (uintptr_t) arg;

      debug (5, "%d (%x.%x) started", (int) i,
	     l4_thread_no (l4_myself ()), l4_version (l4_myself ()));

      int c;
      for (c = 0; c < FACTOR; c ++)
	{
	  int w;
	  for (w = 0; w < 10; w ++)
	    l4_yield ();

	  pthread_mutex_lock (&mutex);

	  debug (5, "%d calling, count=%d", (int) i, shared_resource);

	  for (w = 0; w < 10; w ++)
	    l4_yield ();

	  shared_resource ++;

	  if (c == FACTOR - 1)
	    debug (5, "Exiting with shared_resource = %d", shared_resource);

	  pthread_mutex_unlock (&mutex);
	}

      return arg;
    }

    int i;
    for (i = 0; i < N; i ++)
      {
	debug (5, "Creating thread %d", i);
	error_t err = pthread_create (&threads[i], NULL, start,
				      (void *) (uintptr_t) i);
	assert (err == 0);
      }

    for (i = 0; i < N; i ++)
      {
	void *status = (void *) 1;
	debug (5, "Waiting on thread %d", i);
	error_t err = pthread_join (threads[i], &status);
	assert (err == 0);
	assert ((uintptr_t) status == (uintptr_t) i);
	debug (5, "Joined %d", i);
      }

    assert (shared_resource == N * FACTOR);

    printf ("ok.\n");
  }

  {
    printf ("Checking activity creation... ");

#undef N
#define N 10
    void test (addr_t activity, addr_t folio, int depth)
    {
      error_t err;
      int i;
      int obj = 0;

      struct
      {
	addr_t child;
	addr_t folio;
	addr_t page;
      } a[N];

      for (i = 0; i < N; i ++)
	{
	  /* Allocate a new activity.  */
	  a[i].child = capalloc ();
	  err = rm_folio_object_alloc (activity, folio, obj ++,
				       cap_activity_control,
				       OBJECT_POLICY_DEFAULT, 0,
				       a[i].child, ADDR_VOID);
	  assert (err == 0);

	  /* Allocate a folio against the activity and use it.  */
	  a[i].folio = capalloc ();
	  err = rm_folio_alloc (a[i].child, a[i].folio, FOLIO_POLICY_DEFAULT);
	  assert (err == 0);

	  a[i].page = capalloc ();
	  err = rm_folio_object_alloc (a[i].child, a[i].folio, 0, cap_page,
				       OBJECT_POLICY_DEFAULT, 0,
				       a[i].page, ADDR_VOID);
	  assert (err == 0);

	  l4_word_t type;
	  struct cap_properties properties;

	  err = rm_cap_read (a[i].child, ADDR_VOID,
			     a[i].page, &type, &properties);
	  assert (err == 0);
	  assert (type == cap_page);
	}

      if (depth > 0)
	/* Create another hierarchy depth.  */
	for (i = 0; i < 2; i ++)
	  test (a[i].child, a[i].folio, depth - 1);

      /* We destroy the first N / 2 activities.  The caller will
	 destroy the rest.  */
      for (i = 0; i < N / 2; i ++)
	{
	  /* Destroy the activity.  */
	  rm_folio_free (activity, a[i].folio);

	  /* To determine if the folio has been destroyed, we cannot simply
	     read the capability: this returns the type stored in the
	     capability, not the type of the designated object.  Destroying
	     the object does not destroy the capability.  Instead, we try to
	     use the object.  If this fails, we assume that the folio was
	     destroyed.  */
	  err = rm_folio_object_alloc (a[i].child, a[i].folio, 1, cap_page,
				       OBJECT_POLICY_DEFAULT, 0,
				       a[i].page, ADDR_VOID);
	  assert (err);

	  capfree (a[i].page);
	  capfree (a[i].folio);
	  capfree (a[i].child);
	}
    }

    error_t err;
    addr_t folio = capalloc ();
    err = rm_folio_alloc (activity, folio, FOLIO_POLICY_DEFAULT);
    assert (err == 0);

    test (activity, folio, 2);

    err = rm_folio_free (activity, folio);
    assert (err == 0);

    capfree (folio);

    printf ("ok.\n");
  }

  {
    printf ("Checking activity_policy... ");

    addr_t weak = capalloc ();
    error_t err = rm_cap_copy (activity, ADDR_VOID, weak, ADDR_VOID, activity,
			       CAP_COPY_WEAKEN, CAP_PROPERTIES_VOID);
    assert (! err);

    struct activity_policy in, out;

    in.sibling_rel.priority = 2;
    in.sibling_rel.weight = 3;
    in.child_rel = ACTIVITY_MEMORY_POLICY_VOID;
    in.folios = 10000;

    err = rm_activity_policy (activity,
			      ACTIVITY_POLICY_SIBLING_REL_SET
			      | ACTIVITY_POLICY_STORAGE_SET,
			      in,
			      &out);
    assert (err == 0);
			    
    err = rm_activity_policy (activity,
			      0, ACTIVITY_POLICY_VOID,
			      &out);
    assert (err == 0);

    assert (out.sibling_rel.priority == 2);
    assert (out.sibling_rel.weight == 3);
    assert (out.folios == 10000);

    in.sibling_rel.priority = 4;
    in.sibling_rel.weight = 5;
    in.folios = 10001;
    err = rm_activity_policy (activity,
			      ACTIVITY_POLICY_SIBLING_REL_SET
			      | ACTIVITY_POLICY_STORAGE_SET,
			      in, &out);
    assert (err == 0);

    /* We expect the old values.  */
    assert (out.sibling_rel.priority == 2);
    assert (out.sibling_rel.weight == 3);
    assert (out.folios == 10000);

    err = rm_activity_policy (weak,
			      ACTIVITY_POLICY_SIBLING_REL_SET
			      | ACTIVITY_POLICY_STORAGE_SET,
			      in, &out);
    assert (err == EPERM);

    err = rm_activity_policy (weak, 0, in, &out);
    assert (err == 0);

    assert (out.sibling_rel.priority == 4);
    assert (out.sibling_rel.weight == 5);
    assert (out.folios == 10001);

    capfree (weak);

    printf ("ok.\n");
  }

  {
    printf ("Checking futex implementation... ");

#undef N
#define N 4
#undef FACTOR
#define FACTOR 10
    pthread_t threads[N];

    int futex1 = 0;
    int futex2 = 1;

    void *start (void *arg)
    {
      int i;

      for (i = 0; i < FACTOR; i ++)
	{
	  long ret = futex_wait (&futex1, 0);
	  assert (ret == 0);

	  ret = futex_wait (&futex2, 1);
	  assert (ret == 0);
	}

      return arg;
    }

    int i;
    for (i = 0; i < N; i ++)
      {
	debug (5, "Creating thread %d", i);
	error_t err = pthread_create (&threads[i], NULL, start,
				      (void *) (uintptr_t) i);
	assert (err == 0);
      }

    for (i = 0; i < FACTOR; i ++)
      {
	int count = 0;
	while (count < N)
	  {
	    long ret = futex_wake (&futex1, 1);
	    count += ret;

	    ret = futex_wake (&futex1, N);
	    count += ret;
	  }
	assert (count == N);

	count = 0;
	while (count < N)
	  {
	    long ret = futex_wake (&futex2, 1);
	    count += ret;

	    ret = futex_wake (&futex2, N);
	    count += ret;
	  }
	assert (count == N);
      }

    for (i = 0; i < N; i ++)
      {
	void *status = (void *) 1;
	debug (5, "Waiting on thread %d", i);
	error_t err = pthread_join (threads[i], &status);
	assert (err == 0);
	assert ((uintptr_t) status == (uintptr_t) i);
	debug (5, "Joined %d", i);
      }

    printf ("ok.\n");
  }

  {
    printf ("Checking thread_wait_object_destroy... ");

    struct storage storage = storage_alloc (activity, cap_page,
					    STORAGE_MEDIUM_LIVED,
					    ADDR_VOID);
    assert (! ADDR_IS_VOID (storage.addr));

    void *start (void *arg)
    {
      uintptr_t ret = 0;
      error_t err;
      err = rm_thread_wait_object_destroyed (ADDR_VOID, storage.addr, &ret);
      debug (5, "object destroy returned: err: %d, ret: %d", err, ret);
      assert (err == 0);
      assert (ret == 10);
      return 0;
    }

    pthread_t tid;
    error_t err = pthread_create (&tid, NULL, start, 0);
    assert (err == 0);

    int i;
    for (i = 0; i < 100; i ++)
      l4_yield ();

    /* Deallocate the object.  */
    debug (5, "Destroying object");
    rm_folio_object_alloc (ADDR_VOID,
			   addr_chop (storage.addr, FOLIO_OBJECTS_LOG2),
			   addr_extract (storage.addr, FOLIO_OBJECTS_LOG2),
			   cap_void,
			   OBJECT_POLICY_VOID, 10, ADDR_VOID, ADDR_VOID);
    /* Release the memory.  */
    storage_free (storage.addr, true);

    void *status;
    err = pthread_join (tid, &status);
    assert (err == 0);
    debug (5, "Joined thread");

    printf ("ok.\n");
  }

  {
    printf ("Checking read-only pages... ");
 
    addr_t addr = as_alloc (PAGESIZE_LOG2, 1, true);
    assert (! ADDR_IS_VOID (addr));

    bool r = as_slot_ensure (addr);
    assert (r);

    addr_t storage = storage_alloc (activity, cap_page,
				    STORAGE_MEDIUM_LIVED,
				    addr).addr;
    assert (! ADDR_IS_VOID (storage));


    debug (1, "Writing before dealloc...");
    int *buffer = ADDR_TO_PTR (addr_extend (addr, 0, PAGESIZE_LOG2));
    *buffer = 0;

    storage_free (storage, true);

    debug (1, "Writing after dealloc (should sigsegv)...");
    *buffer = 0;
  }

  debug (1, "Shutting down...");
  while (1)
    l4_sleep (L4_NEVER);

  return 0;
}
