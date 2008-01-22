#include <l4/thread.h>
#include <hurd/slab.h>
#include <hurd/storage.h>

#include <sys/reent.h>

static error_t
slab_alloc (void *hook, size_t size, void **ptr)
{
  struct storage storage = storage_alloc (ADDR_VOID, cap_page,
					  STORAGE_LONG_LIVED, ADDR_VOID);
  if (ADDR_IS_VOID (storage.addr))
    panic ("Out of space.");
  *ptr = ADDR_TO_PTR (addr_extend (storage.addr, 0, PAGESIZE_LOG2));

  return 0;
}

static error_t
slab_dealloc (void *hook, void *buffer, size_t size)
{
  assert (size == PAGESIZE);

  addr_t addr = addr_chop (PTR_TO_ADDR (buffer), PAGESIZE_LOG2);
  storage_free (addr, false);

  return 0;
}

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
  struct _reent *reent = data;

  /* XXX: This may be wrong: if hurd_slab_dealloc needs to use reent,
     we lose.  */
  _reclaim_reent (reent);
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
  _L4_word_t *utcb = _L4_utcb ();

  if (unlikely (! utcb[_L4_UTCB_THREAD_WORD0]))
    /* Thread doesn't have a reent structure.  Allocate one.  */
    {
      pthread_once (&reent_key_init, reent_key_alloc);

      void *buffer;
      error_t err = hurd_slab_alloc (&reent_slab, &buffer);
      if (err)
	panic ("Out of memory!");

      pthread_setspecific (reent_key, buffer);

      utcb[_L4_UTCB_THREAD_WORD0] = (uintptr_t) buffer;
    }

  return (void *) utcb[_L4_UTCB_THREAD_WORD0];
}

