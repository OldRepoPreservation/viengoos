#define _L4_TEST_MAIN
#include "t-environment.h"

#include <hurd/types.h>
#include <hurd/stddef.h>

#include "memory.h"
#include "cap.h"
#include "object.h"
#include "activity.h"
#include "as.h"

struct activity *root_activity;

/* Current working folio.  */
static struct folio *folio;
static int object;

static struct as_insert_rt
allocate_object (enum cap_type type, addr_t addr)
{
  if (! folio || object == FOLIO_OBJECTS)
    {
      folio = folio_alloc (root_activity, FOLIO_POLICY_DEFAULT);
      object = 0;
    }

  struct object *o;
  folio_object_alloc (root_activity, folio, object ++, type, &o);

  struct as_insert_rt rt;
  rt.cap = object_to_cap (o);
  /* We don't need to set RT.STORAGE as as_insert doesn't require it
     for the internal interface implementations.  */
  rt.storage = ADDR (0, 0);
  return rt;
}

extern char _start;
extern char _end;

void
test (void)
{
  struct cap *cap = NULL;

  if (! memory_reserve ((l4_word_t) &_start, (l4_word_t) &_end,
			memory_reservation_self))
    panic ("Failed to reserve memory for self.");

  memory_grab ();
  object_init ();

  /* Create the root activity.  */
  folio = folio_alloc (NULL, FOLIO_POLICY_DEFAULT);
  if (! folio)
    panic ("Failed to allocate storage for the initial task!");

  struct cap c = allocate_object (cap_activity_control, ADDR_VOID).cap;
  root_activity = (struct activity *) cap_to_object (root_activity, &c);
    
  folio_parent (root_activity, folio);

#define N 20
  void try (struct activity *activity, struct folio *folio, int depth)
  {
    int i;
    int obj = 0;

    struct
    {
      struct activity *child;
      struct folio *folio;
      struct object *page;
    } a[N];

    for (i = 0; i < N; i ++)
      {
	struct object *object;

	/* Allocate a new activity.  */
	folio_object_alloc (activity, folio, obj ++,
			    cap_activity_control, &object);
	a[i].child = (struct activity *) object;

	/* Allocate a folio against the activity and use it.  */
	a[i].folio = folio_alloc (a[i].child, FOLIO_POLICY_DEFAULT);
	assert (a[i].folio);

	folio_object_alloc (a[i].child, a[i].folio, 0, cap_page, &a[i].page);
	assert (object_type (a[i].page) == cap_page);
      }

    if (depth > 0)
      /* Create another hierarchy depth.  */
      for (i = 0; i < 2; i ++)
	try (a[i].child, a[i].folio, depth - 1);

    /* We destroy the first N / 2 activities.  The caller will
       destroy the rest.  */
    for (i = 0; i < N / 2; i ++)
      {
	struct cap cap = object_to_cap (a[i].page);
	struct object *o = cap_to_object (activity, &cap);
	assert (o == a[i].page);

	/* Destroy the activity.  */
	folio_free (activity, a[i].folio);

	o = cap_to_object (activity, &cap);
	assert (! o);
      }
  }

  int i;
  for (i = 0; i < 10; i ++)
    {
      struct folio *f = folio_alloc (root_activity, FOLIO_POLICY_DEFAULT);
      assert (f);

      try (root_activity, f, 4);

      folio_free (root_activity, f);
    }
}
