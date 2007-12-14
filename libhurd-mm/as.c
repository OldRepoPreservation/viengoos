/* as.c - Address space construction utility functions.
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

#include "as.h"
#include "storage.h"

#include <hurd/folio.h>
#include <hurd/cap.h>
#include <hurd/btree.h>
#include <hurd/slab.h>
#include <hurd/mutex.h>
#include <l4/types.h>

#include <string.h>

extern struct hurd_startup_data *__hurd_startup_data;

/* The top of the data address space.  */
#if L4_WORDSIZE == 32
#define DATA_ADDR_MAX (0xC0000000ULL)
#else
#error define DATA_ADDR_MAX
#endif

/* Set to true before as_init returns.  Indicates that the shadow page
   table structures may be used, etc.  */
bool as_init_done;

/* We keep track of the regions which are unallocated.  These regions
   are kept in a btree allowing for fast allocation, fast searching
   and fast insertion and deletion.

   The maximum number of free regions is the number of allocated
   regions plus one.  As each free region requires a constant amount
   of memory, the memory required to maintain the free regions is
   O(number of allocated regions).  */
struct region
{
  l4_uint64_t start;
  l4_uint64_t end;
};
  
struct free_space
{
  hurd_btree_node_t node;
  struct region region;
};

/* Compare two regions.  Two regions are considered equal if there is
   any overlap at all.  */
static int
region_compare (const struct region *a, const struct region *b)
{
  if (a->end < b->start)
    return -1;
  if (a->start > b->end)
    return 1;
  /* Overlap.  */
  return 0;
}

BTREE_CLASS (free_space, struct free_space, struct region, region, node,
	     region_compare)

/* The list of free regions.  */
static ss_mutex_t free_spaces_lock;
static hurd_btree_free_space_t free_spaces;

static struct hurd_slab_space free_space_desc_slab;

static error_t
free_space_desc_slab_alloc (void *hook, size_t size, void **ptr)
{
  assert (size == PAGESIZE);

  struct storage storage  = storage_alloc (meta_data_activity,
					   cap_page, STORAGE_LONG_LIVED,
					   ADDR_VOID);
  *ptr = ADDR_TO_PTR (addr_extend (storage.addr, 0, PAGESIZE_LOG2));

  return 0;
}

static error_t
free_space_desc_slab_dealloc (void *hook, void *buffer, size_t size)
{
  assert (size == PAGESIZE);

  addr_t addr = addr_chop (PTR_TO_ADDR (buffer), PAGESIZE_LOG2);
  storage_free (addr, false);

  return 0;
}

static struct free_space *
free_space_desc_alloc (void)
{
  void *buffer;
  error_t err = hurd_slab_alloc (&free_space_desc_slab, &buffer);
  if (err)
    panic ("Out of memory!");
  return buffer;
}

static void
free_space_desc_free (struct free_space *free_space)
{
  hurd_slab_dealloc (&free_space_desc_slab, free_space);
}

/* The sub-region starting at start byte START and endinging at byte
   END is completely covered by the free region F.  Carve it out of
   F.  */
static void
free_space_split (struct free_space *f, l4_uint64_t start, l4_uint64_t end)
{
  assert (! ss_mutex_trylock (&free_spaces_lock));

  /* START and END must be inside F.  */
  assert (f->region.start <= start);
  assert (end <= f->region.end);

  if (start == f->region.start && end == f->region.end)
    /* We completely consume the free region.  Remove it.  */
    {
      hurd_btree_free_space_detach (&free_spaces, f);
      free_space_desc_free (f);
    }
  else if (start == f->region.start)
    /* We overlap with the start of the region, just shrink it.  */
    f->region.start = end + 1;
  else if (end == f->region.end)
    /* We overlap with the end of the region, just shrink it.  */
    f->region.end = start - 1;
  else
    /* We split the region.  */
    {
      struct free_space *new = free_space_desc_alloc ();
      new->region.start = end + 1;
      new->region.end = f->region.end;
      f->region.end = start - 1;

      struct free_space *f = hurd_btree_free_space_insert (&free_spaces, new);
      if (f)
	debug (1, "%llx-%llx overlaps with %llx-%llx",
	       start, end, f->region.start, f->region.end);
      assert (! f);
    }
}

addr_t
as_alloc (int width, l4_uint64_t count, bool data_mappable)
{
  assert (as_init_done);
  assert (count);

  int shift = l4_lsb64 (count) - 1;
  int w = width + shift;
  count >>= shift;
  if (! data_mappable)
    /* We have some latitude regarding where we can place the mapping.
       Use it to ease page table construction.  */
    {
      if (w <= 4)
	w = 4;
      else if (w <= PAGESIZE_LOG2)
	w = PAGESIZE_LOG2;
      else
	/* Make W - PAGESIZE_LOG2 a multiple of CAPPAGE_SLOTS_LOG2;
	   this greatly simplifies page table construction.  */
	w += (CAPPAGE_SLOTS_LOG2
	      - ((w - PAGESIZE_LOG2) % CAPPAGE_SLOTS_LOG2));
    }

  l4_uint64_t align = 1ULL << w;
  l4_uint64_t length = align * count;

  ss_mutex_lock (&free_spaces_lock);

  addr_t addr = ADDR_VOID;

  struct free_space *free_space;
  for (free_space = hurd_btree_free_space_first (&free_spaces);
       free_space;
       free_space = hurd_btree_free_space_next (free_space))
    {
      l4_uint64_t start;
      start = (free_space->region.start + align - 1) & ~(align - 1);

      if (start < free_space->region.end
	  && length <= (free_space->region.end - start) + 1)
	/* We found a fit!  */
	{
	  if (data_mappable && start + length - 1 >= DATA_ADDR_MAX)
	    /* But it must be mappable and it extends beyond the end
	       of the address space!  */
	    break;

	  free_space_split (free_space, start, start + length - 1);
	  addr = ADDR (start, ADDR_BITS - (w - shift));
	  break;
	}
    }

  ss_mutex_unlock (&free_spaces_lock);

  if (ADDR_IS_VOID (addr))
    debug (0, "No space for object of size 0x%x", 1 << (width - 1));

  return addr;
}

bool
as_alloc_at (addr_t addr, l4_uint64_t count)
{
  l4_uint64_t start = addr_prefix (addr);
  l4_uint64_t length = (1ULL << (ADDR_BITS - addr_depth (addr))) * count;
  l4_uint64_t end = start + length - 1;

  struct region region = { start, end };
  struct free_space *f;

  bool ret = false;
  ss_mutex_lock (&free_spaces_lock);

  f = hurd_btree_free_space_find (&free_spaces, &region);
  if (f && (f->region.start <= start && end <= f->region.end))
    {
      free_space_split (f, start, end);
      ret = true;
    }

  ss_mutex_unlock (&free_spaces_lock);

  return ret;
}

void
as_free (addr_t addr, l4_uint64_t count)
{
  l4_uint64_t start = addr_prefix (addr);
  l4_uint64_t length = (1ULL << (ADDR_BITS - addr_depth (addr))) * count;
  l4_uint64_t end = start + length - 1;

  struct free_space *space = free_space_desc_alloc ();
  /* We prefer to coalesce regions where possible.  This ensures that
     if there is overlap, we bail.  */
  space->region.start = start == 0 ? 0 : start - 1;
  space->region.end = end == -1ULL ? -1ULL : end + 1;

  ss_mutex_lock (&free_spaces_lock);

  struct free_space *f = hurd_btree_free_space_insert (&free_spaces, space);
  if (f)
    /* We failed to insert.  This mean that we can coalesce.  */
    {
      free_space_desc_free (space);

      assert (f->region.end + 1 == start || end + 1 == f->region.start);

      struct free_space *prev;
      struct free_space *next;

      if (f->region.end + 1 == start)
	{
	  prev = f;
	  next = hurd_btree_free_space_next (f);
	}
      else
	{
	  prev = hurd_btree_free_space_prev (f);
	  next = f;
	}

      if (prev && next
	  && prev->region.end + 1 == start && end + 1 == next->region.start)
	/* We exactly fill a hole and have to free one.  */
	{
	  prev->region.end = next->region.end;
	  hurd_btree_free_space_detach (&free_spaces, next);
	  free_space_desc_free (next);
	}
      else if (prev && prev->region.end + 1 == start)
	prev->region.end = end;
      else
	{
	  assert (next);
	  assert (end + 1 == next->region.start);
	  next->region.start = start;
	}
    }
  else
    /* We cannot coalesce.  Just fix the region descriptor.  */
    {
      space->region.start = start;
      space->region.end = end;
    }

  ss_mutex_unlock (&free_spaces_lock);
}

static struct as_insert_rt
allocate_object (enum cap_type type, addr_t addr)
{
  struct as_insert_rt rt;

  assert (type == cap_page || type == cap_rpage
	  || type == cap_cappage || type == cap_rcappage);

  memset (&rt, 0, sizeof (rt));
  rt.cap.type = cap_void;

  /* First allocate the real object.  */
  struct storage storage = storage_alloc (meta_data_activity, type,
					  STORAGE_LONG_LIVED, ADDR_VOID);
  if (ADDR_IS_VOID (storage.addr))
    return rt;

  debug (4, ADDR_FMT " %s -> " ADDR_FMT,
	 ADDR_PRINTF (addr), cap_type_string (type),
	 ADDR_PRINTF (storage.addr));

  if (type == cap_cappage || type == cap_rcappage)
    {
      /* Then, allocate the shadow object.  */
      struct storage shadow = storage_alloc (meta_data_activity, cap_page,
					     STORAGE_LONG_LIVED, ADDR_VOID);
      if (ADDR_IS_VOID (shadow.addr))
	{
	  storage_free (storage.addr, false);
	  return rt;
	}
      cap_set_shadow (storage.cap,
		      ADDR_TO_PTR (addr_extend (shadow.addr,
						0, PAGESIZE_LOG2)));
    }

  rt.storage = storage.addr;
  rt.cap = *storage.cap;

  return rt;
}

struct cap *
as_slot_ensure (addr_t addr)
{
  assert (as_init_done);

  /* The implementation is provided by viengoos.  */
  extern struct cap * as_slot_ensure_full (activity_t activity,
					   struct cap *root, addr_t a,
					   struct as_insert_rt
					   (*allocate_object)
					   (enum cap_type type,
					    addr_t addr));

  return as_slot_ensure_full (meta_data_activity,
			      &shadow_root, addr,
			      allocate_object);
}

#define DESC_ADDITIONAL ((PAGESIZE + sizeof (struct hurd_object_desc) - 1) \
			 / sizeof (struct hurd_object_desc))
static struct hurd_object_desc __attribute__((aligned(PAGESIZE)))
  desc_additional[DESC_ADDITIONAL];
static int desc_additional_count;

/* Find an appropriate slot for an object.  */
struct hurd_object_desc *
as_alloc_slow (int width)
{
  assert (! as_init_done);

  addr_t slot = ADDR_VOID;

  int find_free_slot (addr_t addr,
		      l4_word_t type, struct cap_addr_trans addr_trans,
		      bool writable,
		      void *cookie)
  {
    if (type == cap_folio)
      /* We avoid allocating out of folios.  */
      return -1;

    assert (type == cap_void);

    if (ADDR_BITS - addr_depth (addr) < width)
      return -1;

    if (! writable)
      return 0;

    l4_uint64_t start = addr_prefix (addr);
    l4_uint64_t end = start + (1 << width) - 1;

    if (end >= DATA_ADDR_MAX)
      return 0;

    if (! (end < (uintptr_t) l4_kip ()
	   || (uintptr_t) l4_kip () + l4_kip_area_size () <= start))
      /* Overlaps the KIP.  */
      return 0;

    if (! (end < (uintptr_t) _L4_utcb ()
	   || ((uintptr_t) _L4_utcb () + l4_utcb_size () <= start)))
      /* Overlaps the UTCB.  */
      return 0;

    /* Be sure we haven't already given this addrss out.  */
    int i;
    for (i = 0; i < desc_additional_count; i ++)
      {
	struct hurd_object_desc *desc = &desc_additional[i];
	if (ADDR_EQ (addr, addr_chop (desc->object,
				      CAP_ADDR_TRANS_GUARD_BITS (addr_trans))))
	  return 0;
      }

    slot = addr;
    return 1;
  }

  error_t err;

  if (! as_walk (find_free_slot, 1 << cap_void | 1 << cap_folio,
		 (void *) &slot))
    panic ("Failed to find a free slot!");
  assert (! ADDR_IS_VOID (slot));

  debug (3, "Using slot %llx/%d", addr_prefix (slot), addr_depth (slot));

  /* Set the guard on the slot.  */
  int gbits = ADDR_BITS - addr_depth (slot) - width;
  assert (gbits >= 0);

  struct cap_addr_trans cap_addr_trans = CAP_ADDR_TRANS_VOID;
  CAP_ADDR_TRANS_SET_GUARD (&cap_addr_trans, 0, gbits);
  err = rm_cap_copy (meta_data_activity, slot, slot,
		     CAP_COPY_COPY_ADDR_TRANS_GUARD, cap_addr_trans);
  if (err)
    panic ("failed to copy capability: %d", err);

  slot = addr_extend (slot, 0, gbits);

  /* Fill in a descriptor.  */
  assertx ((((l4_word_t) &desc_additional[0]) & (PAGESIZE - 1)) == 0,
	   "%p", &desc_additional[0]);
  struct hurd_object_desc *desc = &desc_additional[desc_additional_count ++];
  if (desc_additional_count > DESC_ADDITIONAL)
    panic ("Out of object descriptors!");
  desc->object = slot;

  return desc;
}

struct cap shadow_root;

void
as_init (void)
{
  /* First, we initialize the free region data structure.  */
  error_t err = hurd_slab_init (&free_space_desc_slab,
				sizeof (struct free_space), 0,
				free_space_desc_slab_alloc,
				free_space_desc_slab_dealloc,
				NULL, NULL, NULL);
  assert (! err);

  hurd_btree_free_space_tree_init (&free_spaces);

  /* We start with a tabula rasa and then remove the regions that are
     actually in use.  */
  as_free (ADDR (0, 0), 1);

  /* Then, we create the shadow page tables and mark the allocation
     regions appropriately.  */

  void add (struct hurd_object_desc *desc, addr_t addr)
    {
      error_t err;
      int i;
      struct object *shadow;

      debug (5, "Adding object %llx/%d",
	     addr_prefix (addr), addr_depth (addr));

      /* We know that the slot that contains the capability that
	 designates this object is already shadowed as we shadow depth
	 first.  */
      struct cap *cap = slot_lookup_rel (meta_data_activity,
					 &shadow_root, addr,
					 desc->type, NULL);
      assert (cap->type == desc->type);

      int slots_log2;

      switch (desc->type)
	{
	case cap_page:
	case cap_rpage:
	  as_alloc_at (addr, 1);
	  return;

	case cap_void:
	  assert (! "void descriptor?");
	  return;

	default:
	  /* This object's address was already reserved when its
	     parent cap page was added.  */
	  return;

	case cap_cappage:
	case cap_rcappage:;
	  /* We shadow the content of cappages.  */

	  if (ADDR_BITS - addr_depth (addr)
	      < CAP_SUBPAGE_SIZE_LOG2 (cap))
	    /* The cappage is unusable for addressing.  */
	    return;
	  else
	    /* We release the addresses used by ADDR and will fill it
	       in appropriately.  */
	    as_free (addr, 1);

	  struct storage shadow_storage
	    = storage_alloc (meta_data_activity,
			     cap_page, STORAGE_LONG_LIVED, ADDR_VOID);
	  if (ADDR_IS_VOID (shadow_storage.addr))
	    panic ("Out of space.");
	  shadow = ADDR_TO_PTR (addr_extend (shadow_storage.addr,
					     0, PAGESIZE_LOG2));
	  cap_set_shadow (cap, shadow);

	  slots_log2 = CAP_SUBPAGE_SIZE_LOG2 (cap);

	  break;

	case cap_folio:
	  if (ADDR_BITS - addr_depth (addr) < FOLIO_OBJECTS_LOG2)
	    /* The folio is unusable for addressing.  */
	    return;
 
	  storage_shadow_setup (cap, addr);
	  shadow = cap_get_shadow (cap);

	  slots_log2 = FOLIO_OBJECTS_LOG2;

	  break;
	}

      /* We expect at least one non-void capability per
	 cappage.  */
      bool have_one = false;

      /* XXX: Would be nice have syscall bundling here.  */
      for (i = 0; i < (1 << slots_log2); i ++)
	{
	  struct cap *slot = &shadow->caps[i];

	  addr_t slot_addr = addr_extend (addr, i, slots_log2);
	  l4_word_t type;
	  err = rm_cap_read (meta_data_activity, slot_addr,
			     &type, &slot->addr_trans);
	  if (err)
	    panic ("Error reading cap %d: %d", i, err);
	  slot->type = type;

	  if (type != cap_void)
	    /* Mark the slot as free--unless we are in a folio.  */
	    {
	      have_one = true;

	      if (desc->type != cap_folio
		  && (addr_depth (slot_addr) + CAP_GUARD_BITS (slot)
		      <= ADDR_BITS))
		{
		  addr_t object = addr_extend (slot_addr, CAP_GUARD (slot),
					       CAP_GUARD_BITS (slot));
		  as_alloc_at (object, 1);
		}
	    }
	}

      assert (have_one);
      return;
    }

  /* Shadow the root capability.  */
  l4_word_t type;
  err = rm_cap_read (meta_data_activity, ADDR (0, 0),
		     &type, &shadow_root.addr_trans);
  assert (err == 0);
  shadow_root.type = type;

  if (type != cap_void)
    as_alloc_at (ADDR (CAP_GUARD (&shadow_root),
		       CAP_GUARD_BITS (&shadow_root)),
		 1);

  /* We assume that the address space is well-formed and that all
     objects in the address space are described by hurd object
     descriptors.

     We shadow each object depth first.  This ensures that all
     dependencies are available when we add a shadow object to the
     shadowed AS.  */

  /* Which depths have objects.  */
  l4_uint64_t depths = 0;

  struct hurd_object_desc *desc;
  int i;
  for (i = 0, desc = &__hurd_startup_data->descs[0];
       i < __hurd_startup_data->desc_count;
       i ++, desc ++)
    {
      depths |= 1ULL << addr_depth (desc->object);
      depths |= 1ULL << addr_depth (desc->storage);
    }

  while (depths)
    {
      int depth = l4_lsb64 (depths) - 1;
      depths &= ~(1ULL << depth);

      for (i = 0, desc = &__hurd_startup_data->descs[0];
	   i < __hurd_startup_data->desc_count;
	   i ++, desc ++)
	{
	  if (addr_depth (desc->object) == depth)
	    add (desc, desc->object);
	  if (! ADDR_EQ (desc->object, desc->storage)
	      && addr_depth (desc->storage) == depth)
	    add (desc, desc->storage);
	}
    }

  /* Now we add any additional descriptors that describe memory that
     we have allocated in the mean time.  */
  for (i = 0; i < desc_additional_count; i ++)
    {
      desc = &desc_additional[i];

      if (! ADDR_EQ (desc->object, desc->storage))
	add (desc, desc->storage);
      add (desc, desc->object);
    }

  /* Reserve the kip and the utcb.  */
  as_alloc_at (ADDR ((uintptr_t) l4_kip (), ADDR_BITS), l4_kip_area_size ());
  as_alloc_at (ADDR ((uintptr_t) _L4_utcb (), ADDR_BITS), l4_utcb_size ());

  /* And the page at 0.  */
  as_alloc_at (addr_chop (PTR_TO_ADDR (0), PAGESIZE_LOG2), 1);

  /* Free DESC_ADDITIONAL.  */
  for (i = 0, desc = &__hurd_startup_data->descs[0];
       i < __hurd_startup_data->desc_count;
       i ++, desc ++)
    if (ADDR_EQ (desc->object,
		 addr_chop (PTR_TO_ADDR (desc_additional), PAGESIZE_LOG2)))
      {
	storage_free (desc->storage, false);
	as_free (addr_chop (PTR_TO_ADDR (desc_additional), PAGESIZE_LOG2), 1);
	break;
      }
  assert (i != __hurd_startup_data->desc_count);

#ifndef NDEBUG
  /* Walk the address space the hard way and make sure that we've got
     everything.  */
  int visit (addr_t addr,
	     l4_word_t type, struct cap_addr_trans addr_trans,
	     bool writable, void *cookie)
    {
      struct cap *cap = slot_lookup_rel (meta_data_activity,
					 &shadow_root, addr, -1, NULL);
      assertx (cap, "addr: " ADDR_FMT "; type: %s",
	       ADDR_PRINTF (addr), cap_type_string (type));

      assert (cap->type == type);
      assert (cap->addr_trans.raw == addr_trans.raw);

      return 0;
    }

  as_walk (visit, -1, NULL);
#endif

  as_init_done = true;

  as_alloced_dump ("");
}

void
as_alloced_dump (const char *prefix)
{
  ss_mutex_lock (&free_spaces_lock);

  struct free_space *free_space;
  for (free_space = hurd_btree_free_space_first (&free_spaces);
       free_space;
       free_space = hurd_btree_free_space_next (free_space))
    printf ("%s%s%llx-%llx\n",
	    prefix ?: "", prefix ? ": " : "",
	    free_space->region.start, free_space->region.end);

  ss_mutex_unlock (&free_spaces_lock);
}

struct cap
cap_lookup (activity_t activity,
	    addr_t address, enum cap_type type, bool *writable)
{
  return cap_lookup_rel (activity, &shadow_root, address, type, writable);
}

struct cap
object_lookup (activity_t activity,
	       addr_t address, enum cap_type type, bool *writable)
{
  return object_lookup_rel (activity, &shadow_root, address, type, writable);
}

struct cap *
slot_lookup (activity_t activity,
	     addr_t address, enum cap_type type, bool *writable)
{
  return slot_lookup_rel (activity, &shadow_root, address, type, writable);
}

/* Walk the address space, depth first.  VISIT is called for each
   *slot* for which (1 << reported capability type) & TYPES is
   non-zero.  TYPE is the reported type of the capability and
   CAP_ADDR_TRANS the value of its address translation fields.
   WRITABLE is whether the slot is writable.  If VISIT returns -1, the
   current sub-tree is exited.  For other non-zero values, the walk is
   aborted and that value is returned.  If the walk is not aborted, 0
   is returned.  */
int
as_walk (int (*visit) (addr_t addr,
		       l4_word_t type, struct cap_addr_trans cap_addr_trans,
		       bool writable,
		       void *cookie),
	 int types,
	 void *cookie)
{
  if (! as_init_done)
    /* We are running on a tiny stack.  Avoid a recursive
       function.  */
    {
      /* We keep track of the child that we should visit at a
	 particular depth.  If child[0] is 2, that means traverse the
	 root's object's child #2.  */
      unsigned short child[1 + ADDR_BITS];
      assert (CAPPAGE_SLOTS_LOG2 < sizeof (child[0]) * 8);

      /* Depth is the current level that we are visiting.  If depth is
	 1, we are visiting the root object's children.  */
      int depth = 0;
      child[0] = 0;

      error_t err;
      struct cap_addr_trans addr_trans;
      l4_word_t type;

    restart:
      assert (depth >= 0);

      int slots_log2;

      addr_t addr = ADDR (0, 0);

      bool writable = true;
      int d;
      for (d = 0; d < depth; d ++)
	{
	  err = rm_cap_read (meta_data_activity,
			     addr, &type, &addr_trans);
	  assert (err == 0);

	  addr = addr_extend (addr, CAP_ADDR_TRANS_GUARD (addr_trans),
			      CAP_ADDR_TRANS_GUARD_BITS (addr_trans));

	  switch (type)
	    {
	    case cap_rcappage:
	      writable = false;
	      /* Fall through.  */
	    case cap_cappage:
	      slots_log2 = CAP_ADDR_TRANS_SUBPAGE_SIZE_LOG2 (addr_trans);
	      break;
	    case cap_folio:
	      slots_log2 = FOLIO_OBJECTS_LOG2;
	      break;
	    default:
	      assert (0 == 1);
	      break;
	    }

	  assert (child[d] <= (1 << slots_log2));

	  if (child[d] == (1 << slots_log2))
	    /* Processed a cappage or a folio.  Proceed to the next one.  */
	    {
	      assert (d == depth - 1);

	      /* Pop.  */
	      depth --;

	      if (depth == 0)
		/* We have processed all of the root's children.  */
		return 0;

	      /* Next node.  */
	      child[depth - 1] ++;

	      goto restart;
	    }

	  addr = addr_extend (addr, child[d], slots_log2);
	  err = rm_cap_read (meta_data_activity,
			     addr, &type, &addr_trans);
	  assert (err == 0);
	}

      for (;;)
	{
	  err = rm_cap_read (meta_data_activity,
			     addr, &type, &addr_trans);
	  if (err)
	    /* Dangling pointer.  */
	    {
	      /* Pop.  */
	      depth --;
	      /* Next node.  */
	      child[depth - 1] ++;

	      goto restart;
	    }

	  do_debug (5)
	    {
	      printf ("Considering " ADDR_FMT "(%s): ",
		      ADDR_PRINTF (addr), cap_type_string (type));
	      int i;
	      for (i = 0; i < depth; i ++)
		printf ("%s%d", i == 0 ? "" : " -> ", child[i]);
	      printf (", depth: %d\n", depth);
	    }

	  if (((1 << type) & types))
	    {
	      int r = visit (addr, type, addr_trans, writable, cookie);
	      if (r == -1)
		{
		  /* Pop.  */
		  depth --;

		  if (depth == 0)
		    /* We have processed all of the root's children.  */
		    return 0;

		  /* Next node.  */
		  child[depth - 1] ++;

		  goto restart;
		}
	      if (r)
		return r;
	    }

	  if (addr_depth (addr) + CAP_ADDR_TRANS_GUARD_BITS (addr_trans)
	      > ADDR_BITS)
	    {
	      child[depth - 1] ++;
	      goto restart;
	    }

	  addr = addr_extend (addr, CAP_ADDR_TRANS_GUARD (addr_trans),
			      CAP_ADDR_TRANS_GUARD_BITS (addr_trans));

	  switch (type)
	    {
	    case cap_rcappage:
	    case cap_cappage:
	      slots_log2 = CAP_ADDR_TRANS_SUBPAGE_SIZE_LOG2 (addr_trans);
	      break;
	    case cap_folio:
	      slots_log2 = FOLIO_OBJECTS_LOG2;
	      break;
	    default:
	      if (depth == 0)
		/* Root is not a cappage or folio.  */
		return 0;

	      child[depth - 1] ++;
	      goto restart;
	    }

	  if (addr_depth (addr) + slots_log2 > ADDR_BITS)
	    {
	      child[depth - 1] ++;
	      goto restart;
	    }

	  /* Visit the first child.  */
	  addr = addr_extend (addr, 0, slots_log2);
	  child[depth] = 0;
	  depth ++;
	}
    }
  
  /* We have the shadow page tables and presumably a normal stack.  */
  int do_walk (struct cap *cap, addr_t addr, bool writable)
    {
      l4_word_t type;
      struct cap_addr_trans cap_addr_trans;

      type = cap->type;
      cap_addr_trans = cap->addr_trans;

      debug (5, ADDR_FMT " (%s)", ADDR_PRINTF (addr), cap_type_string (type));

      int r;
      if (((1 << type) & types))
	{
	  r = visit (addr, type, cap_addr_trans, writable, cookie);
	  if (r == -1)
	    /* Don't go deeper.  */
	    return 0;
	  if (r)
	    return r;
	}

      if (addr_depth (addr) + CAP_ADDR_TRANS_GUARD_BITS (cap_addr_trans)
	  > ADDR_BITS)
	return 0;

      addr = addr_extend (addr, CAP_ADDR_TRANS_GUARD (cap_addr_trans),
			  CAP_ADDR_TRANS_GUARD_BITS (cap_addr_trans));

      int slots_log2 = 0;
      switch (type)
	{
	case cap_cappage:
	case cap_rcappage:
	  if (type == cap_rcappage)
	    writable = false;

	  slots_log2 = CAP_ADDR_TRANS_SUBPAGE_SIZE_LOG2 (cap_addr_trans);
	  break;

	case cap_folio:
	  slots_log2 = FOLIO_OBJECTS_LOG2;
	  break;
	default:
	  return 0;
	}

      if (addr_depth (addr) + slots_log2 > ADDR_BITS)
	return 0;

      struct object *shadow = NULL;
      if (as_init_done)
	shadow = cap_to_object (meta_data_activity, cap);

      int i;
      for (i = 0; i < (1 << slots_log2); i ++)
	{
	  struct cap *object = NULL;
	  if (as_init_done)
	    object = &shadow->caps[i];

	  r = do_walk (object, addr_extend (addr, i, slots_log2), writable);
	  if (r)
	    return r;
	}

      return 0;
    }

  return do_walk (&shadow_root, ADDR (0, 0), true);
}

void
as_dump (const char *prefix)
{
  extern void as_dump_from (activity_t activity,
			    struct cap *root, const char *prefix);

  as_dump_from (meta_data_activity, &shadow_root, prefix);
}
