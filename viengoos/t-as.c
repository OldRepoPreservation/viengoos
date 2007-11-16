#define _L4_TEST_MAIN
#include "t-environment.h"

#include <hurd/types.h>
#include <hurd/stddef.h>

#include "memory.h"
#include "cap.h"
#include "object.h"
#include "activity.h"
#include "as.h"

static struct activity *root_activity;

/* Current working folio.  */
static struct folio *folio;
static int object;

static struct as_insert_rt
allocate_object (enum cap_type type, addr_t addr)
{
  if (! folio || object == FOLIO_OBJECTS)
    {
      folio = folio_alloc (root_activity);
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

struct alloc
{
  addr_t addr;
  int type;
};

static void
try (struct alloc *allocs, int count, bool dump)
{
  struct cap aspace = { .type = cap_void };
  struct cap caps[count];

  void do_check (struct cap *cap, bool writable, int i, bool present)
    {
      if (present)
	{
	  assert (cap);

	  assert (cap->type == caps[i].type);

	  struct object *object = cap_to_object (root_activity, cap);
	  struct object_desc *odesc = object_to_object_desc (object);
	  if (caps[i].type != cap_void)
	    assert (odesc->oid == caps[i].oid);

	  if (cap->type == cap_page)
	    assert (* (unsigned char *) object == i);
	}
      else
	{
	  if (cap)
	    {
	      struct object *object = cap_to_object (root_activity, cap);
	      assert (! object);
	      assert (cap->type == cap_void);
	    }
	}
    }

  int i;
  for (i = 0; i < count; i ++)
    {
      switch (allocs[i].type)
	{
	case cap_folio:
	  caps[i] = object_to_cap ((struct object *)
				   folio_alloc (root_activity));
	  break;
	case cap_void:
	  caps[i].type = cap_void;
	  break;
	case cap_page:
	case cap_rpage:
	case cap_cappage:
	case cap_rcappage:
	  caps[i] = allocate_object (allocs[i].type, allocs[i].addr).cap;
	  break;
	default:
	  assert (! " Bad type");
	}

      struct object *object = cap_to_object (root_activity, &caps[i]);
      if (caps[i].type == cap_page)
	memset (object, i, PAGESIZE);

      as_insert (root_activity, &aspace, allocs[i].addr,
		 object_to_cap (object), ADDR_VOID,
		 allocate_object);

      if (dump)
	{
	  printf ("After inserting: " ADDR_FMT "\n",
		  ADDR_PRINTF (allocs[i].addr));
	  as_dump_from (root_activity, &aspace, NULL);
	}

      int j;
      for (j = 0; j < count; j ++)
	{
	  bool writable;
	  struct cap *cap = slot_lookup_rel (root_activity,
					     &aspace, allocs[j].addr, -1,
					     &writable);
	  do_check (cap, writable, j, j <= i);

	  struct cap c;
	  c = object_lookup_rel (root_activity,
				 &aspace, allocs[j].addr, -1,
				 &writable);
	  do_check (&c, writable, j, j <= i);
	}
    }

  /* Free the allocated objects.  */
  for (i = 0; i < count; i ++)
    {
      /* Make sure allocs[i].addr maps to PAGES[i].  */
      bool writable;
      struct cap *cap = slot_lookup_rel (root_activity,
					 &aspace, allocs[i].addr, -1,
					 &writable);
      do_check (cap, writable, i, true);

      struct cap c = object_lookup_rel (root_activity,
					&aspace, allocs[i].addr, -1,
					&writable);
      do_check (&c, writable, i, true);

      /* Void the capability in the returned capability slot.  */
      cap->type = cap_void;

      /* The page should no longer be found.  */
      c = object_lookup_rel (root_activity, &aspace, allocs[i].addr, -1,
			     NULL);
      assert (c.type == cap_void);

      /* Restore the capability slot.  */
      cap->type = allocs[i].type;

      /* The page should be back.  */
      cap = slot_lookup_rel (root_activity,
			     &aspace, allocs[i].addr, -1, &writable);
      do_check (cap, writable, i, true);

      c = object_lookup_rel (root_activity,
			     &aspace, allocs[i].addr, -1, &writable);
      do_check (&c, writable, i, true);

      /* Finally, free the object.  */
      switch (caps[i].type)
	{
	case cap_folio:
	  folio_free (root_activity,
		      (struct folio *) cap_to_object (root_activity,
						      &caps[i]));
	  break;
	case cap_void:
	  break;
	default:
	  object_free (root_activity, cap_to_object (root_activity, &caps[i]));
	  break;
	}

      /* Check the state of all pages.  */
      int j;
      for (j = 0; j < count; j ++)
	{
	  bool writable;
	  cap = slot_lookup_rel (root_activity,
				 &aspace, allocs[j].addr, -1, &writable);
	  /* We should always get the slot (but it won't always
	     designate an object).  */
	  assert (cap);

	  struct cap c;
	  c = object_lookup_rel (root_activity,
				 &aspace, allocs[j].addr, -1, &writable);
	  do_check (&c, writable, j, i < j);
	}
    }
}

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
  folio = folio_alloc (NULL);
  if (! folio)
    panic ("Failed to allocate storage for the initial task!");

  struct cap c = allocate_object (cap_activity, ADDR_VOID).cap;
  root_activity = (struct activity *) cap_to_object (root_activity, &c);
    
  folio_reparent (root_activity, folio, root_activity);

  {
    printf ("Checking slot_lookup_rel... ");

    /* We have an empty address space.  When we use slot_lookup_rel
       and specify that we don't care what type of capability we get,
       we should get the capability slot--if the guard is right.  */
    struct cap aspace = { type: cap_void };

    l4_word_t addr = 0xFA000;
    bool writable;
    cap = slot_lookup_rel (root_activity, &aspace, ADDR (addr, ADDR_BITS),
			   -1, &writable);
    assert (cap == NULL);

    /* Set the root to designate ADDR.  */
    bool r = CAP_SET_GUARD (&aspace, addr, ADDR_BITS);
    assert (r);
    
    cap = slot_lookup_rel (root_activity, &aspace, ADDR (addr, ADDR_BITS),
			   -1, &writable);
    assert (cap == &aspace);
    assert (writable);

    printf ("ok.\n");
  }

  printf ("Checking as_insert... ");
  {
    struct alloc allocs[] =
      { { ADDR (1 << (FOLIO_OBJECTS_LOG2 + PAGESIZE_LOG2),
		ADDR_BITS - FOLIO_OBJECTS_LOG2 - PAGESIZE_LOG2), cap_folio },
	{ ADDR (0x100000003, 63), cap_page },
	{ ADDR (0x100000004, 63), cap_page },
	{ ADDR (0x1000 /* 4k.  */, ADDR_BITS - PAGESIZE_LOG2), cap_page },
	{ ADDR (0x00100000 /* 1MB */, ADDR_BITS - PAGESIZE_LOG2), cap_page },
	{ ADDR (0x01000000 /* 16MB */, ADDR_BITS - PAGESIZE_LOG2), cap_page },
	{ ADDR (0x10000000 /* 256MB */, ADDR_BITS - PAGESIZE_LOG2), cap_page },
	{ ADDR (0x40000000 /* 1000MB */, ADDR_BITS - PAGESIZE_LOG2),
	  cap_page },
	{ ADDR (0x40000000 - 0x2000 /* 1000MB - 4k */,
		ADDR_BITS - PAGESIZE_LOG2),
	  cap_page },
	{ ADDR (0x40001000, ADDR_BITS - PAGESIZE_LOG2), cap_page },
	{ ADDR (0x40003000, ADDR_BITS - PAGESIZE_LOG2), cap_page },
	{ ADDR (0x40002000, ADDR_BITS - PAGESIZE_LOG2), cap_page },
	{ ADDR (0x40009000, ADDR_BITS - PAGESIZE_LOG2), cap_page },
	{ ADDR (0x40008000, ADDR_BITS - PAGESIZE_LOG2), cap_page },
	{ ADDR (0x40007000, ADDR_BITS - PAGESIZE_LOG2), cap_page },
	{ ADDR (0x40006000, ADDR_BITS - PAGESIZE_LOG2), cap_page },
	{ ADDR (0x00101000 /* 1MB + 4k.  */, ADDR_BITS - PAGESIZE_LOG2),
	  cap_page },
	{ ADDR (0x00FF0000 /* 1MB - 4k.  */, ADDR_BITS - PAGESIZE_LOG2),
	  cap_page },
      };

    try (allocs, sizeof (allocs) / sizeof (allocs[0]), false);
  }

  {
    struct alloc allocs[] =
      { { ADDR (1, ADDR_BITS), cap_page },
	{ ADDR (2, ADDR_BITS), cap_page },
	{ ADDR (3, ADDR_BITS), cap_page },
	{ ADDR (4, ADDR_BITS), cap_page },
	{ ADDR (5, ADDR_BITS), cap_page },
	{ ADDR (6, ADDR_BITS), cap_page },
	{ ADDR (7, ADDR_BITS), cap_page },
	{ ADDR (8, ADDR_BITS), cap_page }
      };

    try (allocs, sizeof (allocs) / sizeof (allocs[0]), false);
  }

  {
    /* Induce a long different guard.  */
    struct alloc allocs[] =
      { { ADDR (0x100000000, 51), cap_cappage },
	{ ADDR (0x80000, 44), cap_folio }
      };

    try (allocs, sizeof (allocs) / sizeof (allocs[0]), false);
  }

  {
    /* Induce subpage allocation.  */
    struct alloc allocs[] =
      { { ADDR (0x80000, 44), cap_folio },
	{ ADDR (0x1000, 51), cap_page },
	{ ADDR (0x10000, 51), cap_page },
	{ ADDR (0x2000, 51), cap_page }
      };

    try (allocs, sizeof (allocs) / sizeof (allocs[0]), false);
  }

  printf ("ok.\n");
}
