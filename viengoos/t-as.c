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

static struct as_allocate_pt_ret
allocate_page_table (vg_addr_t addr)
{
  return allocate_object (vg_cap_cappage, addr);
}

extern char _start;
extern char _end;

struct alloc
{
  vg_addr_t addr;
  int type;
};

static void
try (struct alloc *allocs, int count, bool dump)
{
  struct vg_cap aspace = { .type = vg_cap_void };
  struct vg_cap caps[count];

  void do_check (struct vg_cap *cap, bool writable, int i, bool present)
    {
      if (present)
	{
	  assert (cap);

	  assert (cap->type == caps[i].type);

	  struct object *object = vg_cap_to_object (root_activity, cap);
	  struct object_desc *odesc = object_to_object_desc (object);
	  if (caps[i].type != vg_cap_void)
	    assert (odesc->oid == caps[i].oid);

	  if (cap->type == vg_cap_page)
	    assert (* (unsigned char *) object == i);
	}
      else
	{
	  if (cap)
	    {
	      struct object *object = vg_cap_to_object (root_activity, cap);
	      assert (! object);
	      /* This assertion relies on the fact that the
		 implementation will clear the type field on a failed
		 lookup.  */
	      assert (cap->type == vg_cap_void);
	    }
	}
    }

  int i;
  for (i = 0; i < count; i ++)
    {
      switch (allocs[i].type)
	{
	case vg_cap_folio:
	  caps[i] = object_to_cap ((struct object *)
				   folio_alloc (root_activity,
						VG_FOLIO_POLICY_DEFAULT));
	  break;
	case vg_cap_void:
	  caps[i].type = vg_cap_void;
	  break;
	case vg_cap_page:
	case vg_cap_rpage:
	case vg_cap_cappage:
	case vg_cap_rcappage:
	  caps[i] = allocate_object (allocs[i].type, allocs[i].addr).cap;
	  break;
	default:
	  assert (! " Bad type");
	}

      struct object *object = vg_cap_to_object (root_activity, &caps[i]);
      if (caps[i].type == vg_cap_page)
	memset (object, i, PAGESIZE);

      as_insert_full (root_activity, VG_ADDR_VOID, &aspace, allocs[i].addr,
		      VG_ADDR_VOID, VG_ADDR_VOID, object_to_cap (object),
		      allocate_page_table);

      if (dump)
	{
	  printf ("After inserting: " VG_ADDR_FMT "\n",
		  VG_ADDR_PRINTF (allocs[i].addr));
	  as_dump_from (root_activity, &aspace, NULL);
	}

      int j;
      for (j = 0; j < count; j ++)
	{
	  struct vg_cap *cap = NULL;
	  bool w;

	  as_slot_lookup_rel_use
	    (root_activity, &aspace, allocs[j].addr,
	     ({
	       cap = slot;
	       w = writable;
	     }));
	  do_check (cap, w, j, j <= i);

	  struct vg_cap c;
	  c = as_object_lookup_rel (root_activity,
				    &aspace, allocs[j].addr, -1,
				    &w);
	  do_check (&c, w, j, j <= i);
	}
    }

  /* Free the allocated objects.  */
  for (i = 0; i < count; i ++)
    {
      /* Make sure allocs[i].addr maps to PAGES[i].  */
      struct vg_cap *cap = NULL;
      bool w;

      as_slot_lookup_rel_use (root_activity, &aspace, allocs[i].addr,
			      ({
				cap = slot;
				w = writable;
			      }));
      do_check (cap, w, i, true);

      struct vg_cap c;
      c = as_object_lookup_rel (root_activity,
				&aspace, allocs[i].addr, -1,
				&w);
      do_check (&c, w, i, true);

      /* Void the capability in the returned capability slot.  */
      as_slot_lookup_rel_use (root_activity, &aspace, allocs[i].addr,
			      ({
				slot->type = vg_cap_void;
			      }));

      /* The page should no longer be found.  */
      c = as_object_lookup_rel (root_activity, &aspace, allocs[i].addr, -1,
				NULL);
      assert (c.type == vg_cap_void);

      /* Restore the capability slot.  */
      as_slot_lookup_rel_use (root_activity, &aspace, allocs[i].addr,
			      ({
				slot->type = allocs[i].type;
			      }));


      /* The page should be back.  */
      cap = NULL;
      as_slot_lookup_rel_use
	(root_activity, &aspace, allocs[i].addr,
	 ({
	   cap = slot;
	   w = writable;
	 }));
      do_check (cap, w, i, true);

      c = as_object_lookup_rel (root_activity,
				&aspace, allocs[i].addr, -1, &w);
      do_check (&c, w, i, true);

      /* Finally, free the object.  */
      switch (caps[i].type)
	{
	case vg_cap_folio:
	  folio_free (root_activity,
		      (struct folio *) vg_cap_to_object (root_activity,
						      &caps[i]));
	  break;
	case vg_cap_void:
	  break;
	default:
	  object_free (root_activity, vg_cap_to_object (root_activity, &caps[i]));
	  break;
	}

      /* Check the state of all pages.  */
      int j;
      for (j = 0; j < count; j ++)
	{
	  /* We should always get the slot (but it won't always
	     designate an object).  */
	  bool ret = as_slot_lookup_rel_use
	    (root_activity, &aspace, allocs[j].addr,
	     ({
	     }));
	  assert (ret);

	  struct vg_cap c;
	  bool writable;
	  c = as_object_lookup_rel (root_activity,
				    &aspace, allocs[j].addr, -1, &writable);
	  do_check (&c, writable, j, i < j);
	}
    }
}

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

  {
    printf ("Checking slot_lookup_rel... ");

    /* We have an empty address space.  When we use slot_lookup_rel
       and specify that we don't care what type of capability we get,
       we should get the capability slot--if the guard is right.  */
    struct vg_cap aspace = { type: vg_cap_void };

    l4_word_t addr = 0xFA000;
    bool ret = as_slot_lookup_rel_use (root_activity,
				       &aspace, VG_ADDR (addr, VG_ADDR_BITS),
				       ({ }));
    assert (! ret);

    /* Set the root to designate VG_ADDR.  */
    bool r = VG_CAP_SET_GUARD (&aspace, addr, VG_ADDR_BITS);
    assert (r);
    
    ret = as_slot_lookup_rel_use (root_activity,
				  &aspace, VG_ADDR (addr, VG_ADDR_BITS),
				  ({
				    assert (slot == &aspace);
				    assert (writable);
				  }));
    assert (ret);

    printf ("ok.\n");
  }

  printf ("Checking as_insert... ");
  {
    struct alloc allocs[] =
      { { VG_ADDR (1 << (VG_FOLIO_OBJECTS_LOG2 + PAGESIZE_LOG2),
		VG_ADDR_BITS - VG_FOLIO_OBJECTS_LOG2 - PAGESIZE_LOG2), vg_cap_folio },
	{ VG_ADDR (0x100000003, 63), vg_cap_page },
	{ VG_ADDR (0x100000004, 63), vg_cap_page },
	{ VG_ADDR (0x1000 /* 4k.  */, VG_ADDR_BITS - PAGESIZE_LOG2), vg_cap_page },
	{ VG_ADDR (0x00100000 /* 1MB */, VG_ADDR_BITS - PAGESIZE_LOG2), vg_cap_page },
	{ VG_ADDR (0x01000000 /* 16MB */, VG_ADDR_BITS - PAGESIZE_LOG2), vg_cap_page },
	{ VG_ADDR (0x10000000 /* 256MB */, VG_ADDR_BITS - PAGESIZE_LOG2), vg_cap_page },
	{ VG_ADDR (0x40000000 /* 1000MB */, VG_ADDR_BITS - PAGESIZE_LOG2),
	  vg_cap_page },
	{ VG_ADDR (0x40000000 - 0x2000 /* 1000MB - 4k */,
		VG_ADDR_BITS - PAGESIZE_LOG2),
	  vg_cap_page },
	{ VG_ADDR (0x40001000, VG_ADDR_BITS - PAGESIZE_LOG2), vg_cap_page },
	{ VG_ADDR (0x40003000, VG_ADDR_BITS - PAGESIZE_LOG2), vg_cap_page },
	{ VG_ADDR (0x40002000, VG_ADDR_BITS - PAGESIZE_LOG2), vg_cap_page },
	{ VG_ADDR (0x40009000, VG_ADDR_BITS - PAGESIZE_LOG2), vg_cap_page },
	{ VG_ADDR (0x40008000, VG_ADDR_BITS - PAGESIZE_LOG2), vg_cap_page },
	{ VG_ADDR (0x40007000, VG_ADDR_BITS - PAGESIZE_LOG2), vg_cap_page },
	{ VG_ADDR (0x40006000, VG_ADDR_BITS - PAGESIZE_LOG2), vg_cap_page },
	{ VG_ADDR (0x00101000 /* 1MB + 4k.  */, VG_ADDR_BITS - PAGESIZE_LOG2),
	  vg_cap_page },
	{ VG_ADDR (0x00FF0000 /* 1MB - 4k.  */, VG_ADDR_BITS - PAGESIZE_LOG2),
	  vg_cap_page },
      };

    try (allocs, sizeof (allocs) / sizeof (allocs[0]), false);
  }

  {
    struct alloc allocs[] =
      { { VG_ADDR (1, VG_ADDR_BITS), vg_cap_page },
	{ VG_ADDR (2, VG_ADDR_BITS), vg_cap_page },
	{ VG_ADDR (3, VG_ADDR_BITS), vg_cap_page },
	{ VG_ADDR (4, VG_ADDR_BITS), vg_cap_page },
	{ VG_ADDR (5, VG_ADDR_BITS), vg_cap_page },
	{ VG_ADDR (6, VG_ADDR_BITS), vg_cap_page },
	{ VG_ADDR (7, VG_ADDR_BITS), vg_cap_page },
	{ VG_ADDR (8, VG_ADDR_BITS), vg_cap_page }
      };

    try (allocs, sizeof (allocs) / sizeof (allocs[0]), false);
  }

  {
    /* Induce a long different guard.  */
    struct alloc allocs[] =
      { { VG_ADDR (0x100000000, 51), vg_cap_cappage },
	{ VG_ADDR (0x80000, 44), vg_cap_folio }
      };

    try (allocs, sizeof (allocs) / sizeof (allocs[0]), false);
  }

  {
    /* Induce subpage allocation.  */
    struct alloc allocs[] =
      { { VG_ADDR (0x80000, 44), vg_cap_folio },
	{ VG_ADDR (0x1000, 51), vg_cap_page },
	{ VG_ADDR (0x10000, 51), vg_cap_page },
	{ VG_ADDR (0x2000, 51), vg_cap_page }
      };

    try (allocs, sizeof (allocs) / sizeof (allocs[0]), false);
  }

#warning Incorrect failure mode
#if 0
  {
    /* We do our best to not have to rearrange cappages.  However,
       consider the following scenario: we insert a number of adjacent
       cappages starting at 0.5 MB.  This requires cappage immediately
       above them.  Currently, we'd place a cappage at 0/44.  If we
       then try to insert a folio at 0/43, for which there is
       technically space, it will fail as there is no slot.

          0                    <- /43
          [ | | |...| | | ]
                     P P P     <- /51

       We can only insert at 0/44 if we first reduce the size of the
       cappage and introduce a 2 element page, the first slot of which
       would be used to point to the folio and the second to the
       smaller cappage.  */
    struct alloc allocs[] =
      { { VG_ADDR (0x80000, 51), vg_cap_page },
	{ VG_ADDR (0x81000, 51), vg_cap_page },
	{ VG_ADDR (0x82000, 51), vg_cap_page },
	{ VG_ADDR (0x83000, 51), vg_cap_page },
	{ VG_ADDR (0x84000, 51), vg_cap_page },
	{ VG_ADDR (0x85000, 51), vg_cap_page },
	{ VG_ADDR (0x0, 44), vg_cap_folio }
      };

    try (allocs, sizeof (allocs) / sizeof (allocs[0]), false);
  }
#endif

  printf ("ok.\n");
}
