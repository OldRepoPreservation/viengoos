/* getreent.c - Reentrancy support.
   Copyright (C) 2008 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

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

#include <l4/thread.h>
#include <hurd/slab.h>
#include <hurd/storage.h>

#include <sys/reent.h>

static error_t
slab_alloc (void *hook, size_t size, void **ptr)
{
  struct storage storage = storage_alloc (VG_ADDR_VOID, vg_cap_page,
					  STORAGE_LONG_LIVED,
					  VG_OBJECT_POLICY_DEFAULT,
					  VG_ADDR_VOID);
  if (VG_ADDR_IS_VOID (storage.addr))
    panic ("Out of space.");
  *ptr = VG_ADDR_TO_PTR (vg_addr_extend (storage.addr, 0, PAGESIZE_LOG2));

  return 0;
}

static error_t
slab_dealloc (void *hook, void *buffer, size_t size)
{
  assert (size == PAGESIZE);

  vg_addr_t addr = vg_addr_chop (VG_PTR_TO_ADDR (buffer), PAGESIZE_LOG2);
  storage_free (addr, false);

  return 0;
}

/* We don't use the slab for the first thread as this wastes space and
   we don't have enough stack when we first allocate it.  */
static struct _reent reent_main;
static int reent_main_alloced;

/* Storage descriptors are alloced from a slab.  */
static struct hurd_slab_space reent_slab
  = HURD_SLAB_SPACE_INITIALIZER (struct _reent, slab_alloc, slab_dealloc,
				 NULL, NULL, NULL);

/* We also associate the reent with a pthread key thereby
   ensuring that we are notified when the thread exits allowing
   us to clean up the reent structure.  We don't use the pthread
   key exclusively as the UTCB is a lot cheaper.  */
static pthread_key_t reent_key;

static void
reent_key_destroy (void *data)
{
  _L4_word_t *utcb = _L4_utcb ();
  assert (utcb[_L4_UTCB_THREAD_WORD0] == data);

  struct _reent *reent = data;

  /* XXX: This may be wrong: we know that hurd_slab_dealloc does not
     need to use reent, however, other keys may be freed, in which
     case, we might lose.  But it will cause a new TSD to be created
     and then freed.  The number of times this can happen is bounded.
     In the very worst case, we might leak some memory.  */
  _reclaim_reent (reent);

  utcb[_L4_UTCB_THREAD_WORD0] = 0;

  hurd_slab_dealloc (&reent_slab, reent);
}

static void
reent_key_alloc (void)
{
  pthread_key_create (&reent_key, reent_key_destroy);
}

static pthread_once_t reent_key_init = PTHREAD_ONCE_INIT;

struct _reent *
__getreent (void)
{
  extern int mm_init_done;
  assert (mm_init_done);

  _L4_word_t *utcb = _L4_utcb ();

  if (unlikely (! utcb[_L4_UTCB_THREAD_WORD0]))
    /* Thread doesn't have a reent structure.  Allocate one.  */
    {
      void *buffer;
      if (! reent_main_alloced)
	{
	  buffer = &reent_main;
	  reent_main_alloced = 1;
	}
      else
	{
	  error_t err = hurd_slab_alloc (&reent_slab, &buffer);
	  if (err)
	    panic ("Out of memory!");
	}

      struct _reent *reent = buffer;
      _REENT_INIT_PTR (reent);

      utcb[_L4_UTCB_THREAD_WORD0] = (uintptr_t) buffer;

      if (buffer != &reent_main)
	/* Only call after we've setup the reent structure.  */
	{
	  pthread_once (&reent_key_init, reent_key_alloc);
	  pthread_setspecific (reent_key, buffer);
	}
    }

  return (void *) utcb[_L4_UTCB_THREAD_WORD0];
}
