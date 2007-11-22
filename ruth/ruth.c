/* ruth.c - Test server.
   Copyright (C) 2007 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU Hurd is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <hurd/startup.h>
#include <hurd/cap.h>
#include <hurd/folio.h>
#include <hurd/rm.h>
#include <hurd/stddef.h>
#include <hurd/capalloc.h>
#include <hurd/as.h>
#include <hurd/storage.h>

#include <bit-array.h>
#include <string.h>

#include <sys/mman.h>

#include "ruth.h"

int output_debug;

static addr_t activity;

/* Initialized by the machine-specific startup-code.  */
extern struct hurd_startup_data *__hurd_startup_data;


/* The program name.  */
const char program_name[] = "ruth";


/* The following functions are required by pthread.  */

void
__attribute__ ((__noreturn__))
exit (int __status)
{
  panic ("exit() called");
}


void
abort (void)
{
  panic ("abort() called");
}


/* FIXME:  Should be elsewhere.  Needed by libhurd-slab.  */
int
getpagesize()
{
  return PAGESIZE;
}


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

    int visit (addr_t addr,
	       l4_word_t type, struct cap_addr_trans cap_addr_trans,
	       bool writable,
	       void *cookie)
      {
	struct cap *slot = slot_lookup (activity, addr, -1, NULL);

	assert (slot);
	assert (type == slot->type);
	if (type == cap_cappage || type == cap_rcappage || type == cap_folio)
	  assert (slot->shadow);
	else
	  assert (! slot->shadow);

	return 0;
      }

    as_walk (visit, ~(1 << cap_void), NULL);
    printf ("ok.\n");
  }

  {
    printf ("Checking folio_object_alloc... ");


    addr_t folio = capalloc ();
    assert (! ADDR_IS_VOID (folio));
    error_t err = rm_folio_alloc (activity, folio);
    assert (! err);

    int i;
    for (i = -10; i < 129; i ++)
      {
	addr_t addr = capalloc ();
	if (ADDR_IS_VOID (addr))
	  panic ("capalloc");

	err = rm_folio_object_alloc (activity, folio, i, cap_page, addr);
	assert ((err == 0) == (0 <= i && i < FOLIO_OBJECTS));

	if (0 <= i && i < FOLIO_OBJECTS)
	  {
	    l4_word_t type;
	    struct cap_addr_trans cap_addr_trans;
	    err = rm_cap_read (activity, addr, &type, &cap_addr_trans);
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

	error_t err = rm_folio_alloc (activity, f);
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
	    struct cap_addr_trans addr_trans;

	    error_t err = rm_cap_read (activity, addr_extend (root, j, bits),
				       &type, &addr_trans);
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

      debug (2, "I'm running (%x.%x)!\n",
	     l4_thread_no (l4_myself ()),
	     l4_version (l4_myself ()));

      done = 1;
      do
	l4_yield ();
      while (1);
    }

    printf ("Checking thread creation... ");

    addr_t thread = capalloc ();
    debug (1, "thread: " ADDR_FMT, ADDR_PRINTF (thread));
    addr_t storage = storage_alloc (activity, cap_thread, STORAGE_LONG_LIVED,
				    thread).addr;

    struct cap_addr_trans addr_trans = CAP_ADDR_TRANS_VOID;
    rm_object_slot_copy_in (activity, thread, THREAD_ASPACE_SLOT,
			    ADDR (0, 0), CAP_COPY_COPY_SOURCE_GUARD,
			    addr_trans);

    l4_word_t dummy;
    rm_thread_exregs (activity, thread,
		      HURD_EXREGS_SET_ACTIVITY
		      | HURD_EXREGS_SET_SP_IP | HURD_EXREGS_START,
		      ADDR (0, 0), activity,
		      (l4_word_t) ((void *) stack + sizeof (stack)),
		      (l4_word_t) &start, 0, 0,
		      ADDR_VOID, ADDR_VOID,
		      &dummy, &dummy, &dummy, &dummy);

    debug (2, "Waiting for thread");
    while (done == 0)
      l4_yield ();
    debug (2, "Thread done!");

    storage_free (storage, true);
    capfree (thread);

    printf ("ok.\n");
  }

  {
    static volatile int done;
    char stack[0x1000];

    void start (void)
    {
      do_debug (4)
	as_dump ("thread");

      debug (2, "I'm running (%x.%x)!\n",
	     l4_thread_no (l4_myself ()),
	     l4_version (l4_myself ()));

      done = 1;
      do
	l4_yield ();
      while (1);
    }

    printf ("Checking thread creation... ");

    addr_t thread = capalloc ();
    debug (1, "thread: " ADDR_FMT, ADDR_PRINTF (thread));
    addr_t storage = storage_alloc (activity, cap_thread, STORAGE_LONG_LIVED,
				    thread).addr;

    struct cap_addr_trans addr_trans = CAP_ADDR_TRANS_VOID;
    rm_object_slot_copy_in (activity, thread, THREAD_ASPACE_SLOT,
			    ADDR (0, 0), CAP_COPY_COPY_SOURCE_GUARD,
			    addr_trans);

    l4_word_t dummy;
    rm_thread_exregs (activity, thread,
		      HURD_EXREGS_SET_ACTIVITY
		      | HURD_EXREGS_SET_SP_IP | HURD_EXREGS_START,
		      ADDR (0, 0), activity,
		      (l4_word_t) ((void *) stack + sizeof (stack)),
		      (l4_word_t) &start, 0, 0,
		      ADDR_VOID, ADDR_VOID,
		      &dummy, &dummy, &dummy, &dummy);

    debug (2, "Waiting for thread");
    while (done == 0)
      l4_yield ();
    debug (2, "Thread done!");

    storage_free (storage, true);
    capfree (thread);

    printf ("ok.\n");
  }

  {
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
