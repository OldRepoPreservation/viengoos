#define _L4_TEST_MAIN
#include "t-environment.h"

#include <hurd/types.h>
#include <hurd/stddef.h>
#include <hurd/as.h>

#include "memory.h"
#include "cap.h"
#include "object.h"
#include "activity.h"

struct activity *root_activity;

/* Current working folio.  */
static struct folio *folio;
static int object;

static struct as_allocate_pt_ret
allocate_object (enum vg_cap_type type, vg_addr_t addr)
{
  if (! folio || object == VG_FOLIO_OBJECTS)
    {
      folio = folio_alloc (root_activity, VG_FOLIO_POLICY_DEFAULT);
      object = 0;
    }

  struct as_allocate_pt_ret rt;
  rt.cap = folio_object_alloc (root_activity, folio, object ++,
			       type, VG_OBJECT_POLICY_DEFAULT, 0);

  /* We don't need to set RT.STORAGE as as_insert doesn't require it
     for the internal interface implementations.  */
  rt.storage = VG_ADDR (0, 0);
  return rt;
}

extern char _start;
extern char _end;

void
test (void)
{
  if (! memory_reserve ((l4_word_t) &_start, (l4_word_t) &_end,
			memory_reservation_self))
    panic ("Failed to reserve memory for self.");

  memory_grab ();
  object_init ();

  /* Create the root activity.  */
  folio = folio_alloc (NULL, VG_FOLIO_POLICY_DEFAULT);
  if (! folio)
    panic ("Failed to allocate storage for the initial task!");

  struct vg_cap c = allocate_object (vg_cap_activity_control, VG_ADDR_VOID).cap;
  root_activity = (struct activity *) vg_cap_to_object (root_activity, &c);
    
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
	/* Allocate a new activity.  */
	struct vg_cap cap;
	cap = folio_object_alloc (activity, folio, obj ++,
				  vg_cap_activity_control,
				  VG_OBJECT_POLICY_DEFAULT, 0);
	a[i].child = (struct activity *) vg_cap_to_object (activity, &cap);

	/* Allocate a folio against the activity and use it.  */
	a[i].folio = folio_alloc (a[i].child, VG_FOLIO_POLICY_DEFAULT);
	assert (a[i].folio);

	cap = folio_object_alloc (a[i].child, a[i].folio, 0,
				  vg_cap_page, VG_OBJECT_POLICY_DEFAULT, 0);
	a[i].page = vg_cap_to_object (activity, &cap);
	assert (object_type (a[i].page) == vg_cap_page);
      }

    if (depth > 0)
      /* Create another hierarchy depth.  */
      for (i = 0; i < 2; i ++)
	try (a[i].child, a[i].folio, depth - 1);

    /* We destroy the first N / 2 activities.  The caller will
       destroy the rest.  */
    for (i = 0; i < N / 2; i ++)
      {
	struct vg_cap cap = object_to_cap (a[i].page);
	struct object *o = vg_cap_to_object (activity, &cap);
	assert (o == a[i].page);

	/* Destroy the activity.  */
	folio_free (activity, a[i].folio);

	o = vg_cap_to_object (activity, &cap);
	assert (! o);
      }
  }

  int i;
  for (i = 0; i < 10; i ++)
    {
      struct folio *f = folio_alloc (root_activity, VG_FOLIO_POLICY_DEFAULT);
      assert (f);

      try (root_activity, f, 4);

      folio_free (root_activity, f);
    }
}
