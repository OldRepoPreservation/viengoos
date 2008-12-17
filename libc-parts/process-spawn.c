/* process-spawn.c - Process spawn implementation.
   Copyright (C) 2008 Free Software Foundation, Inc.
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

#include <viengoos/folio.h>
#include <viengoos/thread.h>
#include <viengoos/misc.h>
#include <hurd/as.h>
#include <loader.h>

#include <string.h>

/* This file is compiled for both the kernel and for user-land.  Here
   we abstract some of the functionality that we require.
   Nevertheless, there are a few conditionals in the main code.
   Despite the ugliness, this is better than maintaining two copies of
   the code.  */

#ifdef RM_INTERN
/* For struct thread.  */
#include "../viengoos/thread.h"
#endif

#ifndef RM_INTERN
#include <hurd/ihash.h>
#include <hurd/capalloc.h>
#include <hurd/storage.h>
#endif

#ifdef RM_INTERN
#include "../viengoos/zalloc.h"
#define ALLOC(size) ((void *) zalloc (size))
#define FREE(p, size) zfree ((uintptr_t) p, size)
#else
#include <stdlib.h>
#define ALLOC(size) calloc (size, 1)
#define FREE(p, size) free (p)
#endif

#ifdef RM_INTERN
#include "../viengoos/activity.h"
#else
#define root_activity VG_ADDR_VOID
#endif

#ifdef RM_INTERN
# define AS_DUMP_ as_dump_from (root_activity, as_root_cap, __func__)
#else
# define AS_DUMP_ rm_as_dump (VG_ADDR_VOID, as_root)
#endif
#define AS_DUMP						\
  do							\
    {							\
      debug (0, "Dumping address space");		\
      AS_DUMP_;						\
    }							\
  while (0)						\

#ifdef RM_INTERN
# define rt_to_object(rt) vg_cap_to_object (root_activity, &(rt).cap)
#else
# define rt_to_object(rt)					\
  VG_ADDR_TO_PTR (vg_addr_extend ((rt).storage, 0, PAGESIZE_LOG2))
#endif 

vg_thread_t
process_spawn (vg_addr_t activity,
	       void *start, void *end,
	       const char *const argv[], const char *const env[],
	       bool make_runnable)
{
  debug (1, "Loading %s", argv[0]);

#ifdef RM_INTERN
  /* We don't need shadow objects.  */
#define add_shadow(a) NULL
  /* Or a special indexing operation.  */
#define as_insert_custom(activity_, as_root_, as_root_cap_, taddr_,	\
			 sroot_, scap_, saddr_,				\
			 alloc_, index_)				\
  ({									\
    debug (5, "Copying " VG_ADDR_FMT " to " VG_ADDR_FMT ,		\
	   VG_ADDR_PRINTF (saddr_), VG_ADDR_PRINTF (taddr_));		\
    as_insert_full (root_activity,					\
		    VG_ADDR_VOID, as_root_cap_, taddr_,			\
		    VG_ADDR_VOID, VG_ADDR_VOID, scap_, alloc_);		\
  })

#else
  struct shadow
  {
    vg_addr_t addr;
    struct vg_cap cap;
    struct shadow *next;
  };
  struct shadow *shadow_list = NULL;

  struct hurd_ihash as;
  hurd_ihash_init (&as, true, HURD_IHASH_NO_LOCP);

  struct vg_cap *add_shadow (vg_addr_t addr)
  {
    debug (5, VG_ADDR_FMT, VG_ADDR_PRINTF (addr));
    struct shadow *s = calloc (sizeof (struct shadow), 1);

    s->next = shadow_list;
    shadow_list = s;

    s->addr = addr;

    bool had_value;
    hurd_ihash_value_t old_value;
    error_t err = hurd_ihash_replace (&as, addr.raw, s, &had_value, &old_value);
    if (err)
      panic ("Failed to insert object into address hash.");
    assert (! had_value);

    return &s->cap;
  }

  /* as_insert_custom callback function.  We assume that once a page
     table is added to the tree, it never moves.  This is kosher
     because we know the implementation of as_insert.  This allows us
     to allocate the shadow capabilities for the slots lazily and not
     bind them to the page table.  That is, if there is a page table
     at X, and we index it, we don't refer to X but simply extend its
     address and return the shadow pte at that address.  */
  struct vg_cap *do_index (activity_t activity,
			struct vg_cap *pt, vg_addr_t pt_addr, int idx,
			struct vg_cap *fake_slot)
  {
    assert (pt->type == vg_cap_cappage || pt->type == vg_cap_rcappage
	    || pt->type == vg_cap_folio);

    debug (5, "-> " VG_ADDR_FMT "[%d/%d], %s",
	   VG_ADDR_PRINTF (pt_addr), idx, VG_CAPPAGE_SLOTS / VG_CAP_SUBPAGES (pt),
	   vg_cap_type_string (pt->type));

    vg_addr_t pte_addr;
    switch (pt->type)
      {
      case vg_cap_cappage:
      case vg_cap_rcappage:
	pte_addr = vg_addr_extend (pt_addr, idx, VG_CAP_SUBPAGE_SIZE_LOG2 (pt));
	break;
      case vg_cap_folio:
	pte_addr = vg_addr_extend (pt_addr, idx, VG_FOLIO_OBJECTS_LOG2);
	break;
      default:
	panic ("Expected cappage or folio but got a %s",
	       vg_cap_type_string (pt->type));
      }

    struct shadow *s = hurd_ihash_find (&as, pte_addr.raw);
    struct vg_cap *cap;
    if (s)
      {
	assert (VG_ADDR_EQ (s->addr, pte_addr));
	cap = &s->cap;
      }
    else
      /* We haven't allocated this shadow object yet.  */
      cap = add_shadow (pte_addr);

    assert (cap);

    debug (5, "<- " VG_ADDR_FMT "[%d], %s",
	   VG_ADDR_PRINTF (pte_addr), VG_CAPPAGE_SLOTS / VG_CAP_SUBPAGES (cap),
	   vg_cap_type_string (cap->type));

    if (pt->type == vg_cap_folio)
      {
	*fake_slot = *cap;
	return fake_slot;
      }

    return cap;
  }
#endif

  /* The address space is empty.  This is as good a place to put it as
     any in particular as it is unlikely that the binary will try to
     load something here.  */
#define STARTUP_DATA_ADDR 0x1000
  /* This should be enough for an ~64 MB binary.  The memory has to be
     continuous.  We start putting folios at 512kb so if we put this
     at 4k, then we can use a maximum of 127 pages for the memory.  */
#define STARTUP_DATA_MAX_SIZE (100 * PAGESIZE)
  struct hurd_startup_data *startup_data = ALLOC (STARTUP_DATA_MAX_SIZE);

  {
    /* Add the argument vector.  If it would overflow the space, we
       truncate it (which is useless but we'll error out soon).  */

    int n = 0;
    int i;
    for (i = 0; argv[i]; i ++)
      n += strlen (argv[i]) + 1;

    startup_data->argz_len = n;

    int offset = sizeof (struct hurd_startup_data);
    startup_data->argz = (void *) STARTUP_DATA_ADDR + offset;

    int space = STARTUP_DATA_MAX_SIZE - offset;
    if (space < startup_data->argz_len)
      panic ("Argument list too long.");

    char *p = (void *) startup_data + offset;
    for (i = 0; argv[i]; i ++)
      {
	int len = strlen (argv[i]) + 1;
	memcpy (p, argv[i], len);
	p += len;
      }

    offset += n;

    n = 0;
    if (env)
      for (i = 0; env[i]; i ++)
	n += strlen (env[i]) + 1;

    startup_data->envz_len = n;
    startup_data->envz = (void *) STARTUP_DATA_ADDR + offset;

    space = STARTUP_DATA_MAX_SIZE - offset;
    if (space < startup_data->envz_len)
      panic ("Environment too long.");

    p = (void *) startup_data + offset;
    if (env)
      for (i = 0; env[i]; i ++)
	{
	  int len = strlen (env[i]) + 1;
	  memcpy (p, env[i], len);
	  p += len;
	}
  }

  /* Point the descriptors after the argument string at the next
     properly aligned address.  */
  int descs_offset = (sizeof (struct hurd_startup_data)
		      + startup_data->argz_len
		      + startup_data->envz_len
		      + __alignof__ (uintptr_t) - 1)
    & ~(__alignof__ (uintptr_t) - 1);

  struct hurd_object_desc *descs = (void *) startup_data + descs_offset;
  int desc_max = ((STARTUP_DATA_MAX_SIZE - descs_offset)
		  / sizeof (struct hurd_object_desc));

  startup_data->version_major = HURD_STARTUP_VERSION_MAJOR;
  startup_data->version_minor = HURD_STARTUP_VERSION_MINOR;
#ifdef RM_INTERN
  startup_data->utcb_area = UTCB_AREA_BASE;
  startup_data->rm = l4_myself ();
#else
  {
    extern struct hurd_startup_data *__hurd_startup_data;

    startup_data->utcb_area = __hurd_startup_data->utcb_area;
    startup_data->rm = __hurd_startup_data->rm;
  }
#endif
  startup_data->descs = (void *) STARTUP_DATA_ADDR + descs_offset;


  /* Root of new address space.  */
#ifdef RM_INTERN
# define as_root VG_ADDR_VOID
  struct vg_cap as_root_cap;
  memset (&as_root_cap, 0, sizeof (as_root_cap));
# define as_root_cap (&as_root_cap)
#else
  vg_addr_t as_root = capalloc ();
  struct vg_cap *as_root_cap = add_shadow (VG_ADDR (0, 0));

  /* This is sort of a hack.  To copy a capability, we need to invoke
     the source object that contains the capability.  A capability
     slot is not an object.  Finding the object corresponding to
     AS_ROOT is possible, but iterposing a thread is just easier.  */
  struct storage thread_root
    = storage_alloc (VG_ADDR_VOID, vg_cap_thread, STORAGE_EPHEMERAL,
		     VG_OBJECT_POLICY_DEFAULT, as_root);
#endif

  /* Allocation support.  */

  /* Address of first folio in new task.  */
#define FOLIO_START (1ULL << (VG_FOLIO_OBJECTS_LOG2 + PAGESIZE_LOG2))

  bool have_folio = false;
  /* Local address.  */
  vg_folio_t folio_local_addr;
  /* Address in task.  */
  vg_addr_t folio_task_addr;
  /* Next unallocated object in folio.  */
  int folio_index;

#ifndef RM_INTERN
  struct as_region
  {
    struct as_region *next;
    vg_addr_t addr;
  };
  struct as_region *as_regions = NULL;
#endif

  struct as_allocate_pt_ret allocate_object (enum vg_cap_type type, vg_addr_t addr)
    {
      debug (5, "(%s, 0x%llx/%d)",
	     vg_cap_type_string (type), vg_addr_prefix (addr), vg_addr_depth (addr));

      assert (type != vg_cap_void);
      assert (type != vg_cap_folio);

      if (! have_folio || folio_index == VG_FOLIO_OBJECTS)
	/* Allocate additional storage.  */
	{
	  int w = VG_FOLIO_OBJECTS_LOG2 + PAGESIZE_LOG2;
	  if (! have_folio)
	    {
	      folio_task_addr = VG_ADDR (FOLIO_START, VG_ADDR_BITS - w);
	      have_folio = true;
	    }
	  else
	    /* Move to the next free space.  */
	    folio_task_addr = VG_ADDR (vg_addr_prefix (folio_task_addr) + (1ULL << w),
				    VG_ADDR_BITS - w);

	  debug (5, "Allocating folio at " VG_ADDR_FMT,
		 VG_ADDR_PRINTF (folio_task_addr));

#ifdef RM_INTERN
	  folio_local_addr = folio_alloc (root_activity, VG_FOLIO_POLICY_DEFAULT);
	  if (! folio_local_addr)
	    panic ("Out of memory");
#else
	  folio_local_addr = as_alloc (w, 1, true);
	  if (VG_ADDR_IS_VOID (folio_local_addr))
	    panic ("Failed to allocate address space for folio");

	  struct as_region *as_region = malloc (sizeof (*as_region));
	  if (! as_region)
	    panic ("Out of memory.");

	  as_region->addr = folio_local_addr;
	  as_region->next = as_regions;
	  as_regions = as_region;

	  as_ensure (folio_local_addr);

	  error_t err = rm_folio_alloc (activity, activity,
					VG_FOLIO_POLICY_DEFAULT,
					&folio_local_addr);
	  if (err)
	    panic ("Failed to allocate folio");
	  assert (! VG_ADDR_IS_VOID (folio_local_addr));

	  as_slot_lookup_use (folio_local_addr,
			      ({
				slot->type = vg_cap_folio;
				VG_CAP_SET_SUBPAGE (slot, 0, 1);
			      }));
#endif

	  folio_index = 0;

	  if (startup_data->desc_count == desc_max)
	    /* XXX: Allocate more space.  */
	    panic ("Out of object descriptors (binary too big)");

	  struct hurd_object_desc *desc = &descs[startup_data->desc_count ++];

	  desc->object = folio_task_addr;
	  desc->type = vg_cap_folio;

	  /* We need to insert the folio into the task's address
	     space, however, that is not yet possible as we may be
	     called from as_insert.  Instead, we abuse desc->storage
	     to save the location of the folio and then insert the
	     folio into the task's address space and fix this up
	     later.  */
#ifdef RM_INTERN
	  desc->storage.raw = (uintptr_t) folio_local_addr;
#else
	  desc->storage = folio_local_addr;
#endif
	}

      struct as_allocate_pt_ret rt;
      memset (&rt, 0, sizeof (rt));

      int index = folio_index ++;

      debug (5, "Allocating " VG_ADDR_FMT " (%s)",
	     VG_ADDR_PRINTF (vg_addr_extend (folio_task_addr,
				       index, VG_FOLIO_OBJECTS_LOG2)),
	     vg_cap_type_string (type));

#ifdef RM_INTERN
      rt.cap = folio_object_alloc (root_activity,
				   folio_local_addr, index,
				   vg_cap_type_strengthen (type),
				   VG_OBJECT_POLICY_VOID, 0);
#else
      rm_folio_object_alloc (VG_ADDR_VOID,
			     folio_local_addr, index,
			     vg_cap_type_strengthen (type),
			     VG_OBJECT_POLICY_VOID, 0, NULL, NULL);
      rt.cap.type = vg_cap_type_strengthen (type);
      VG_CAP_PROPERTIES_SET (&rt.cap, VG_CAP_PROPERTIES_VOID);

#ifndef NDEBUG
      if (rt.cap.type == vg_cap_page)
	{
	  unsigned int *p
	    = VG_ADDR_TO_PTR (vg_addr_extend (vg_addr_extend (folio_local_addr,
						     index, VG_FOLIO_OBJECTS_LOG2),
					0, PAGESIZE_LOG2));
	  int i;
	  for (i = 0; i < PAGESIZE / sizeof (int); i ++)
	    if (p[i])
	      panic ("%p not clean! (p[%d] = %u)",
		     p, i, *p);
	}
#endif
#endif


      if (! (startup_data->desc_count < desc_max))
	panic ("Initial task too large.");
      struct hurd_object_desc *desc = &descs[startup_data->desc_count ++];

      desc->storage = vg_addr_extend (folio_task_addr, index, VG_FOLIO_OBJECTS_LOG2);
      if (VG_ADDR_IS_VOID (addr))
	desc->object = desc->storage;
      else
	desc->object = addr;
      desc->type = type;

#ifdef RM_INTERN
      /* We can directly access the storage in the task's address
	 space.  */
      rt.storage = desc->storage;
#else
      /* We need to reference the storage in our address space.  */
      rt.storage = vg_addr_extend (folio_local_addr, index, VG_FOLIO_OBJECTS_LOG2);
#endif

      debug (5, "cap: " VG_CAP_FMT, VG_CAP_PRINTF (&rt.cap));
      return rt;
    }

  struct as_allocate_pt_ret allocate_page_table (vg_addr_t addr)
  {
    debug (5, VG_ADDR_FMT, VG_ADDR_PRINTF (addr));
    return allocate_object (vg_cap_cappage, addr);	
  }

  struct as_allocate_pt_ret rt;

#ifdef RM_INTERN
  /* XXX: Boostrap problem.  To allocate a folio we need to assign it
     to a principle, however, the representation of a principle
     requires storage.  Our solution is to allow a folio to be created
     without specifying a resource principal, allocating a resource
     principal and then assigning the folio to that resource
     principal.

     This isn't really a good solution as once we really go the
     persistent route, there may be references to the data structures
     in the persistent image.  Moreover, the root activity data needs
     to be saved.

     A way around this problem would be the approach that EROS takes:
     start with a hand-created system image.  */
  rt = allocate_object (vg_cap_activity_control, VG_ADDR_VOID);
  startup_data->activity = rt.storage;

  root_activity = (struct activity *) vg_cap_to_object (root_activity, &rt.cap);
  folio_parent (root_activity, folio_local_addr);

  /* We know that we are the only one who can access the data
     structure, however, the object_claim asserts that this lock is
     held.  */
  object_claim (root_activity, (struct object *) root_activity,
		VG_OBJECT_POLICY_VOID, true);
  object_claim (root_activity, (struct object *) folio_local_addr,
		VG_OBJECT_POLICY_VOID, true);
#else
  struct hurd_object_desc *desc;
  struct vg_cap cap;
  memset (&cap, 0, sizeof (cap));
  bool r;

  /* Stash the activity two pages before the first folio.  */
  desc = &descs[startup_data->desc_count ++];
  desc->storage = VG_ADDR_VOID;
  desc->object = VG_ADDR (FOLIO_START - 2 * PAGESIZE, VG_ADDR_BITS - PAGESIZE_LOG2);
  desc->type = vg_cap_activity;
  startup_data->activity = desc->object;

  /* Insert it into the target address space.  */
  cap.type = vg_cap_activity;
  struct vg_cap *slot = as_insert_custom (VG_ADDR_VOID,
					  as_root, as_root_cap, desc->object,
					  VG_ADDR_VOID, cap, activity,
					  allocate_page_table, do_index);
  /* Weaken the capability.  */
  r = vg_cap_copy_x (root_activity, as_root, slot, desc->object,
		     as_root, *slot, desc->object,
		     VG_CAP_COPY_WEAKEN, VG_CAP_PROPERTIES_VOID);
  assert (r);
#endif


  /* Allocate the thread.  */
  rt = allocate_object (vg_cap_thread, VG_ADDR_VOID);
  assert (descs[startup_data->desc_count - 1].type == vg_cap_thread);
  startup_data->thread = descs[startup_data->desc_count - 1].object;

#ifdef RM_INTERN
  struct thread *thread = (struct thread *) vg_cap_to_object (root_activity,
							   &rt.cap);
#else
  vg_addr_t thread = capalloc ();
  cap.type = vg_cap_thread;
  as_slot_lookup_use
    (thread,
     ({
       r = vg_cap_copy (root_activity,
		     VG_ADDR_VOID, slot, thread,
		     VG_ADDR_VOID, rt.cap, rt.storage);
       assert (r);
     }));
#endif

  /* Load the binary.  */
  uintptr_t ip;
  {
#ifndef RM_INTERN
    struct hurd_ihash map;
    hurd_ihash_init (&map, false, HURD_IHASH_NO_LOCP);
#endif

    void *alloc (uintptr_t ptr, bool ro)
    {
      assert ((ptr & (PAGESIZE - 1)) == 0);

      debug (5, "%x (ro:%d)", ptr, ro);

      vg_addr_t addr = vg_addr_chop (VG_PTR_TO_ADDR (ptr), PAGESIZE_LOG2);

      struct as_allocate_pt_ret rt = allocate_object (vg_cap_page, addr);

      as_insert_custom (root_activity,
			as_root, as_root_cap, addr,
			VG_ADDR_VOID, rt.cap, rt.storage,
			allocate_page_table, do_index);
      
      if (ro)
	as_slot_lookup_rel_use (root_activity, as_root_cap, addr,
				({
				  bool r = vg_cap_copy_x (root_activity,
						       as_root, slot, addr,
						       as_root, *slot, addr,
						       VG_CAP_COPY_WEAKEN,
						       VG_CAP_PROPERTIES_VOID);
				  assert (r);
				}));

      void *local = rt_to_object (rt);

#ifndef RM_INTERN
      bool had_value;
      hurd_ihash_value_t old_value;
      error_t err = hurd_ihash_replace (&map, ptr, local,
					&had_value, &old_value);
      if (err)
	panic ("Failed to insert object into address hash.");
      assert (! had_value);
#endif

      debug (5, "0x%x -> %p", ptr, local);

      return local;
    }

    void *lookup (uintptr_t ptr)
    {
      void *local;

      debug (5, "%x", ptr);

      assert ((ptr & (PAGESIZE - 1)) == 0);

#ifdef RM_INTERN
      vg_addr_t addr = vg_addr_chop (VG_PTR_TO_ADDR (ptr), PAGESIZE_LOG2);
      struct vg_cap cap = as_object_lookup_rel (root_activity,
					     as_root_cap, addr,
					     vg_cap_rpage, NULL);
      local = vg_cap_to_object (root_activity, &cap);
#else
      local = hurd_ihash_find (&map, ptr);
#endif

      debug (5, "0x%x -> %p", ptr, local);
      return local;
    }

    if (! loader_elf_load (alloc, lookup, start, end, &ip))
      panic ("Failed to load %s", argv[0]);

#ifndef RM_INTERN
    hurd_ihash_destroy (&map);
#endif
  }

  /* Allocate some messengers.  */
  int i;
  for (i = 0; i < 2; i ++)
    {
      rt = allocate_object (vg_cap_messenger, VG_ADDR_VOID);
      assert (descs[startup_data->desc_count - 1].type == vg_cap_messenger);
      startup_data->messengers[i] = descs[startup_data->desc_count - 1].object;
      debug (5, "Messenger %d: " VG_ADDR_FMT,
	     i, VG_ADDR_PRINTF (startup_data->messengers[i]));
    }

  /* We need to 1) insert the folios in the address space, 2) fix up
     their descriptors (recall: we are abusing desc->storage to hold
     the local name for the storge), and 3) copy the startup data.  We
     have to do this carefully as inserting can cause a new folio to
     be allocated.  */
  int d = 0;
  int page = 0;
  void *pages[STARTUP_DATA_MAX_SIZE / PAGESIZE];
  memset (pages, 0, sizeof (pages));

  while (d != startup_data->desc_count)
    {
      for (; d < startup_data->desc_count; d ++)
	{
	  struct hurd_object_desc *desc = &descs[d];

	  if (desc->type == vg_cap_folio)
	    {
	      struct vg_cap cap;
#ifdef RM_INTERN
	      cap = object_to_cap ((struct object *) (uintptr_t)
				   desc->storage.raw);
	      assert (cap.type == vg_cap_folio);
#else
	      memset (&cap, 0, sizeof (cap));
	      cap.type = vg_cap_folio;
#endif

	      as_insert_custom (VG_ADDR_VOID,
				as_root, as_root_cap, desc->object,
				VG_ADDR_VOID, cap, desc->storage,
				allocate_page_table, do_index);

	      desc->storage = desc->object;
	    }
	}

      /* Allocate memory for the startup data object(s) and insert
	 them into the task's address space.  */
      for (;
	   page < (descs_offset
		   + (sizeof (struct hurd_object_desc)
		      * startup_data->desc_count)
		   + PAGESIZE - 1) / PAGESIZE;
	   page ++)
	{
	  vg_addr_t addr = VG_ADDR (STARTUP_DATA_ADDR + page * PAGESIZE,
			      VG_ADDR_BITS - PAGESIZE_LOG2);

	  struct as_allocate_pt_ret rt = allocate_object (vg_cap_page, addr);

	  pages[page] = rt_to_object (rt);

	  as_insert_custom (VG_ADDR_VOID, as_root, as_root_cap, addr,
			    VG_ADDR_VOID, rt.cap, rt.storage,
			    allocate_page_table, do_index);
	}
    }

  /* Copy the staging area in place.  */
  for (i = 0; i < page; i ++)
    memcpy (pages[i], (void *) startup_data + i * PAGESIZE, PAGESIZE);

#ifndef RM_INTERN
  /* Free the address space that we allocated to access the folios.
     The only object we still need is the thread, which we have cached
     in a cap slot and will free at the end.  */
  struct as_region *as_region;
  struct as_region *next = as_regions;

  while ((as_region = next))
    {
      next = as_region->next;

      as_free (as_region->addr, 1);
      free (as_region);
    }
#endif

  do_debug (5)
    /* Dump the descriptors.  */
    {
      debug (0, "%d descriptors", startup_data->desc_count);
      for (i = 0; i < startup_data->desc_count; i ++)
	debug (0, VG_ADDR_FMT " (" VG_ADDR_FMT "): %s",
	       VG_ADDR_PRINTF (descs[i].object), 
	       VG_ADDR_PRINTF (descs[i].storage),
	       vg_cap_type_string (descs[i].type));
    }

  /* Free the staging area.  */
  FREE (startup_data, STARTUP_DATA_MAX_SIZE);

  do_debug (5)
    AS_DUMP;
  debug (3, "Initial IP: %x", ip);

#ifdef RM_INTERN
  thread->aspace = *as_root_cap;
  thread->activity = object_to_cap ((struct object *) root_activity);

  l4_word_t sp = STARTUP_DATA_ADDR;

  error_t err;
  err = thread_exregs (root_activity, thread,
		       HURD_EXREGS_SET_SP_IP
		       | (make_runnable ? HURD_EXREGS_START : 0)
		       | HURD_EXREGS_ABORT_IPC,
		       VG_CAP_VOID, 0, VG_CAP_PROPERTIES_VOID,
		       VG_CAP_VOID, VG_CAP_VOID, VG_CAP_VOID,
		       &sp, &ip, NULL, NULL);
#else
  /* Start thread.  */
  struct hurd_thread_exregs_in in;
  /* Per the API (cf. <hurd/startup.h>).  */
  in.sp = STARTUP_DATA_ADDR;
  in.ip = ip;
  in.aspace_cap_properties = VG_CAP_PROPERTIES_VOID;
  in.aspace_cap_properties_flags = VG_CAP_COPY_COPY_SOURCE_GUARD;

  error_t err;
  struct hurd_thread_exregs_out out;
  /* XXX: Use a weakened activity.  */
  err = rm_thread_exregs (VG_ADDR_VOID, thread,
			  HURD_EXREGS_SET_SP_IP
			  | HURD_EXREGS_SET_ASPACE
			  | HURD_EXREGS_SET_ACTIVITY
			  | (make_runnable ? HURD_EXREGS_START : 0)
			  | HURD_EXREGS_ABORT_IPC,
			  in, vg_addr_extend (as_root, VG_THREAD_ASPACE_SLOT,
					   VG_THREAD_SLOTS_LOG2),
			  activity, VG_ADDR_VOID, VG_ADDR_VOID,
			  &out, NULL, NULL, NULL, NULL);
#endif
  if (err)
    panic ("Failed to start thread: %d", err);

  /* Free the remaining locally allocated resources.  */

#ifndef RM_INTERN
  storage_free (thread_root.addr, false);
  capfree (as_root);
#endif

#ifndef RM_INTERN
  struct shadow *s = shadow_list;
  shadow_list = NULL;
  while (s)
    {
      struct shadow *next = s->next;
      free (s);
      s = next;
    }

  hurd_ihash_destroy (&as);
#endif

  return thread;
}


